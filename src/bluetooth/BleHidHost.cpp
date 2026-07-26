#include "BleHidHost.h"

#ifdef ONEPAGE_C61

#include <Logging.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

#include <cstring>

#include "HalPowerManager.h"

namespace {
constexpr int64_t RECONNECT_BACKOFF_US = 3000000;  // 3s cooldown after a failed connect

// Low-power connection parameters, requested once the link is encrypted. A page
// turner is an event device (bytes only on a keypress), so a slow connection
// interval + high slave latency lets our radio sleep between events. Units:
// interval = N * 1.25 ms; timeout = N * 10 ms.
//   interval 60-75 ms, latency 4 -> the peripheral may skip up to 4 events but
//   still delivers a keypress within ~one interval. Supervision timeout must
//   exceed interval_max * (latency + 1) * 2 = 75ms*5*2 = 750ms; we use 4 s.
constexpr uint16_t LP_ITVL_MIN = 48;  // 60 ms  (48 * 1.25)
constexpr uint16_t LP_ITVL_MAX = 60;  // 75 ms  (60 * 1.25)
constexpr uint16_t LP_LATENCY = 4;    // skip up to 4 events when idle
constexpr uint16_t LP_TIMEOUT = 400;  // 4 s supervision timeout (400 * 10 ms)
}  // namespace

extern "C" {
#include <host/ble_hs.h>
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <host/ble_uuid.h>
#include <host/util/util.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <os/os_mbuf.h>
}

// Forward declaration: the C-callable GAP handler (defined below, file scope).
int bleHidHostGapEvent(struct ble_gap_event* event, void* arg);

namespace {

constexpr uint16_t HID_SVC_UUID = 0x1812;
constexpr uint16_t HID_REPORT_CHR_UUID = 0x2A4D;
constexpr uint16_t CCCD_UUID = 0x2902;
constexpr uint16_t HID_APPEARANCE_MIN = 0x03C0;
constexpr uint16_t HID_APPEARANCE_MAX = 0x03C4;

constexpr size_t MIN_INTERNAL_SRAM = 40 * 1024;
constexpr size_t QUEUE_DEPTH = 4;

uint16_t g_connHandle = BLE_HS_CONN_HANDLE_NONE;
uint16_t g_hidSvcStart = 0;
uint16_t g_hidSvcEnd = 0;

void startScan();

// --- GATT discovery ---------------------------------------------------------

int onDescDisc(uint16_t conn, const struct ble_gatt_error* error, uint16_t chrValHandle,
               const struct ble_gatt_dsc* dsc, void* arg) {
  if (error->status == 0 && dsc && ble_uuid_u16(&dsc->uuid.u) == CCCD_UUID) {
    uint8_t value[2] = {0x01, 0x00};  // enable notifications
    ble_gattc_write_flat(conn, dsc->handle, value, sizeof(value), nullptr, nullptr);
  }
  return 0;
}

int onChrDisc(uint16_t conn, const struct ble_gatt_error* error, const struct ble_gatt_chr* chr,
              void* arg) {
  if (error->status == 0 && chr) {
    const uint16_t uuid = ble_uuid_u16(&chr->uuid.u);
    if (uuid == HID_REPORT_CHR_UUID && (chr->properties & BLE_GATT_CHR_PROP_NOTIFY)) {
      ble_gattc_disc_all_dscs(conn, chr->val_handle, g_hidSvcEnd, onDescDisc, nullptr);
    }
  }
  return 0;
}

int onSvcDisc(uint16_t conn, const struct ble_gatt_error* error, const struct ble_gatt_svc* svc,
              void* arg) {
  if (error->status == 0 && svc) {
    g_hidSvcStart = svc->start_handle;
    g_hidSvcEnd = svc->end_handle;
    LOG_INF("BLEHID", "HID service handles [%u..%u]", g_hidSvcStart, g_hidSvcEnd);
    ble_gattc_disc_all_chrs(conn, g_hidSvcStart, g_hidSvcEnd, onChrDisc, nullptr);
  }
  return 0;
}

bool advIsHid(const struct ble_hs_adv_fields& fields) {
  for (int i = 0; i < fields.num_uuids16; i++) {
    if (ble_uuid_u16(&fields.uuids16[i].u) == HID_SVC_UUID) return true;
  }
  if (fields.appearance_is_present && fields.appearance >= HID_APPEARANCE_MIN &&
      fields.appearance <= HID_APPEARANCE_MAX) {
    return true;
  }
  return false;
}

void startScan() {
  struct ble_gap_disc_params params = {};
  // Low duty-cycle scan. On this single-core C61 a full-duty scan
  // (itvl=window=0) keeps the radio+CPU busy almost continuously and starves
  // the e-ink UI -> menus feel laggy while unconnected. A 30ms window every
  // 300ms (~10% duty) leaves the CPU free for rendering. Units are 0.625ms.
  // Keep ACTIVE scan (passive=0) so we still detect devices that only advertise
  // the HID service UUID in their scan response. Reconnect just takes a bit
  // longer. Once connected, scanning stops entirely (near-zero overhead).
  params.itvl = 480;    // 300 ms
  params.window = 48;   // 30 ms
  params.passive = 0;
  params.filter_duplicates = 1;
  uint8_t ownAddrType;
  if (ble_hs_id_infer_auto(0, &ownAddrType) != 0) return;
  int rc = ble_gap_disc(ownAddrType, BLE_HS_FOREVER, &params, bleHidHostGapEvent, nullptr);
  if (rc != 0) {
    LOG_ERR("BLEHID", "ble_gap_disc failed: %d", rc);
  } else {
    LOG_INF("BLEHID", "scanning for BLE HID page-turner...");
  }
}

void bleHostTask(void* param) {
  nimble_port_run();
  nimble_port_freertos_deinit();
}

}  // namespace

// C-callable GAP event handler that forwards to the singleton.
int bleHidHostGapEvent(struct ble_gap_event* event, void* arg) {
  BleHidHost& self = BleHidHost::getInstance();
  switch (event->type) {
    case BLE_GAP_EVENT_DISC: {
      struct ble_hs_adv_fields fields;
      if (ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data) != 0) return 0;
      if (!advIsHid(fields)) return 0;

      char name[24] = {0};
      if (fields.name_len > 0) {
        uint8_t n = fields.name_len < sizeof(name) - 1 ? fields.name_len : sizeof(name) - 1;
        memcpy(name, fields.name, n);
        name[n] = '\0';
      }

      if (self.mode_ == BleHidHost::Mode::Discovery) {
        // Browsing: collect into the list, do NOT connect.
        self.onDiscovered(event->disc.addr.val, event->disc.addr.type, name);
        return 0;
      }

      // Bound mode: only connect to the remembered device.
      if (!self.boundValid_) return 0;
      if (memcmp(event->disc.addr.val, self.boundAddr_, 6) != 0) return 0;

      // Reconnect backoff: after a failed handshake, don't hammer the radio —
      // ignore the bound device's ads until the cooldown elapses.
      if (esp_timer_get_time() < self.nextConnectAllowedUs_) return 0;

      LOG_INF("BLEHID", "bound device seen (rssi=%d) -> connecting", event->disc.rssi);
      ble_gap_disc_cancel();
      self.state_ = BleHidState::Connecting;
      uint8_t ownAddrType;
      ble_hs_id_infer_auto(0, &ownAddrType);
      if (ble_gap_connect(ownAddrType, &event->disc.addr, 30000, nullptr, bleHidHostGapEvent,
                          nullptr) != 0) {
        self.nextConnectAllowedUs_ = esp_timer_get_time() + RECONNECT_BACKOFF_US;
        startScan();
        self.state_ = BleHidState::Scanning;
      }
      return 0;
    }

    case BLE_GAP_EVENT_CONNECT:
      if (event->connect.status == 0) {
        g_connHandle = event->connect.conn_handle;
        self.state_ = BleHidState::Connected;
        LOG_INF("BLEHID", "connected (handle=%u)", g_connHandle);
        ble_gap_security_initiate(g_connHandle);
        static const ble_uuid16_t hidSvcUuid = BLE_UUID16_INIT(HID_SVC_UUID);
        ble_gattc_disc_svc_by_uuid(g_connHandle, &hidSvcUuid.u, onSvcDisc, nullptr);
      } else {
        // Connection attempt failed: back off before retrying.
        self.nextConnectAllowedUs_ = esp_timer_get_time() + RECONNECT_BACKOFF_US;
        self.state_ = BleHidState::Scanning;
        startScan();
      }
      return 0;

    case BLE_GAP_EVENT_DISCONNECT:
      LOG_INF("BLEHID", "disconnected (reason=%d)", event->disconnect.reason);
      g_connHandle = BLE_HS_CONN_HANDLE_NONE;
      self.state_ = BleHidState::Disconnected;
      // Auto-reconnect to the bound device (bound mode), after a backoff so a
      // device that won't complete the handshake can't spin the radio.
      if (self.boundValid_) {
        self.nextConnectAllowedUs_ = esp_timer_get_time() + RECONNECT_BACKOFF_US;
        self.mode_ = BleHidHost::Mode::Bound;
        startScan();
        self.state_ = BleHidState::Scanning;
      } else {
        self.state_ = BleHidState::Stopped;
      }
      return 0;

    case BLE_GAP_EVENT_ENC_CHANGE: {
      // Link is now encrypted: request slow-poll connection params so our radio
      // can sleep between keypresses. Best-effort — if the peripheral rejects
      // the update we simply keep its parameters (functionally fine, just less
      // power saving). A CONN_UPDATE event reports the negotiated result.
      if (g_connHandle != BLE_HS_CONN_HANDLE_NONE) {
        const ble_gap_upd_params params = {
            .itvl_min = LP_ITVL_MIN,
            .itvl_max = LP_ITVL_MAX,
            .latency = LP_LATENCY,
            .supervision_timeout = LP_TIMEOUT,
            .min_ce_len = 0,
            .max_ce_len = 0,
        };
        const int rc = ble_gap_update_params(g_connHandle, &params);
        LOG_INF("BLEHID", "requested low-power conn params (rc=%d)", rc);
      }
      return 0;
    }

    case BLE_GAP_EVENT_CONN_UPDATE:
      LOG_INF("BLEHID", "conn params updated (status=%d)", event->conn_update.status);
      return 0;

    case BLE_GAP_EVENT_NOTIFY_RX: {
      uint16_t len = OS_MBUF_PKTLEN(event->notify_rx.om);
      uint8_t buf[64];
      if (len > sizeof(buf)) len = sizeof(buf);
      if (ble_hs_mbuf_to_flat(event->notify_rx.om, buf, sizeof(buf), &len) != 0) return 0;
      self.onReport(event->notify_rx.attr_handle, buf, len);
      return 0;
    }

    default:
      return 0;
  }
}

namespace {
void onSync() {
  LOG_INF("BLEHID", "NimBLE synced");
  BleHidHost& self = BleHidHost::getInstance();
  // Only auto-scan (bound mode) if a device is already bound. Otherwise wait for
  // the pairing UI to call startDiscovery().
  if (self.hasBoundDevice()) {
    startScan();
  }
}
void onReset(int reason) { LOG_ERR("BLEHID", "NimBLE reset reason=%d", reason); }
}  // namespace

BleHidHost& BleHidHost::getInstance() {
  static BleHidHost instance;
  return instance;
}

bool BleHidHost::start() {
  if (started_) return true;

  const size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  if (freeInternal < MIN_INTERNAL_SRAM) {
    LOG_ERR("BLEHID", "insufficient internal SRAM (%u), not starting", (unsigned)freeInternal);
    return false;
  }

  if (!actionQueue_) {
    actionQueue_ = xQueueCreate(QUEUE_DEPTH, sizeof(uint8_t));
    if (!actionQueue_) {
      LOG_ERR("BLEHID", "action queue alloc failed");
      return false;
    }
  }

  if (nimble_port_init() != ESP_OK) {
    LOG_ERR("BLEHID", "nimble_port_init failed");
    return false;
  }
  ble_hs_cfg.sync_cb = onSync;
  ble_hs_cfg.reset_cb = onReset;
  ble_hs_cfg.sm_bonding = 1;
  ble_hs_cfg.sm_mitm = 0;
  ble_hs_cfg.sm_sc = 1;
  ble_hs_cfg.sm_our_key_dist = 3;
  ble_hs_cfg.sm_their_key_dist = 3;

  nimble_port_freertos_init(bleHostTask);
  started_ = true;
  // onSync decides whether to auto-scan (only when a device is bound).
  state_ = hasBoundDevice() ? BleHidState::Scanning : BleHidState::Stopped;
  // Hold full CPU speed: 10 MHz low-power mode breaks controller radio timing.
  HalPowerManager::setBleActive(true);
  LOG_INF("BLEHID", "BLE HID host started");
  return true;
}

bool BleHidHost::stop() {
  if (!started_) return true;
  if (g_connHandle != BLE_HS_CONN_HANDLE_NONE) {
    ble_gap_terminate(g_connHandle, BLE_ERR_REM_USER_CONN_TERM);
  }
  ble_gap_disc_cancel();
  int rc = nimble_port_stop();
  if (rc == 0) {
    nimble_port_deinit();
  }
  started_ = false;
  state_ = BleHidState::Stopped;
  g_connHandle = BLE_HS_CONN_HANDLE_NONE;
  HalPowerManager::setBleActive(false);
  LOG_INF("BLEHID", "BLE HID host stopped");
  return true;
}

// --- Discovery / pairing ----------------------------------------------------

void BleHidHost::startDiscovery() {
  scanCount_ = 0;
  scanRevision_ = scanRevision_ + 1;
  mode_ = Mode::Discovery;
  state_ = BleHidState::Scanning;
  // Cancel any in-progress scan/connection, then start a fresh discovery scan.
  ble_gap_disc_cancel();
  startScan();
}

void BleHidHost::onDiscovered(const uint8_t addr[6], uint8_t addrType, const char* name) {
  // Dedup by address. The device name usually arrives in a SEPARATE advertising
  // report (the scan response) AFTER the initial connectable advert, so if we
  // already have this address but no name yet, backfill the name now instead of
  // dropping the update. Publish count last (barrier for the reader task).
  for (uint8_t i = 0; i < scanCount_; i++) {
    if (memcmp(scan_[i].addr, addr, 6) == 0) {
      if (scan_[i].name[0] == '\0' && name && name[0]) {
        strncpy(scan_[i].name, name, sizeof(scan_[i].name) - 1);
        scan_[i].name[sizeof(scan_[i].name) - 1] = '\0';
        scanRevision_ = scanRevision_ + 1;
        LOG_INF("BLEHID", "named #%u: %s", i + 1, scan_[i].name);
      }
      return;
    }
  }
  if (scanCount_ >= MAX_SCAN) return;
  BleScanEntry& e = scan_[scanCount_];
  memcpy(e.addr, addr, 6);
  e.addrType = addrType;
  e.name[0] = '\0';
  if (name && name[0]) {
    strncpy(e.name, name, sizeof(e.name) - 1);
    e.name[sizeof(e.name) - 1] = '\0';
  }
  scanCount_ = scanCount_ + 1;
  scanRevision_ = scanRevision_ + 1;
  LOG_INF("BLEHID", "discovered #%u: %s", scanCount_, e.name[0] ? e.name : "(unnamed)");
}

bool BleHidHost::getDiscovered(uint8_t index, BleScanEntry& out) const {
  if (index >= scanCount_) return false;
  out = scan_[index];
  return true;
}

void BleHidHost::connectTo(const uint8_t addr[6], uint8_t addrType) {
  setBoundDevice(addr, addrType);
  mode_ = Mode::Bound;
  state_ = BleHidState::Scanning;
  // Re-scan; the DISC handler will connect when the bound device advertises.
  ble_gap_disc_cancel();
  startScan();
}

// --- Bound device -----------------------------------------------------------

void BleHidHost::setBoundDevice(const uint8_t addr[6], uint8_t addrType) {
  memcpy(boundAddr_, addr, 6);
  boundAddrType_ = addrType;
  boundValid_ = true;
}

void BleHidHost::getBoundDevice(uint8_t addr[6], uint8_t& addrType) const {
  memcpy(addr, boundAddr_, 6);
  addrType = boundAddrType_;
}

void BleHidHost::clearBoundDevice() {
  boundValid_ = false;
  memset(boundAddr_, 0, 6);
  boundAddrType_ = 0;
}

// --- Learning ---------------------------------------------------------------

void BleHidHost::startLearn(BleHidAction action, bool requireRelease) {
  learnTarget_ = static_cast<uint8_t>(action);
  learnComplete_ = false;
  // Only gate on a key-release when chaining (2nd key): a fresh learn has no key
  // held, so requiring a release would force the user to press twice.
  awaitRelease_ = requireRelease;
  learnArmed_ = true;
  LOG_INF("BLEHID", "learn armed for action %u (awaitRelease=%d)", learnTarget_, (int)requireRelease);
}

void BleHidHost::cancelLearn() { learnArmed_ = false; }

bool BleHidHost::consumeLearnComplete() {
  if (!learnComplete_) return false;
  learnComplete_ = false;
  return true;
}

// --- Patterns ---------------------------------------------------------------

const BleReportPattern& BleHidHost::pattern(BleHidAction action) const {
  return action == BleHidAction::PageForward ? fwd_ : back_;
}

void BleHidHost::setPattern(BleHidAction action, const BleReportPattern& p) {
  (action == BleHidAction::PageForward ? fwd_ : back_) = p;
}

void BleHidHost::clearPatterns() {
  fwd_ = BleReportPattern{};
  back_ = BleReportPattern{};
}

bool BleHidHost::hasBothPatterns() const { return fwd_.valid && back_.valid; }

// --- Report handling (BLE host task context) --------------------------------

void BleHidHost::onReport(uint16_t attrHandle, const uint8_t* data, uint16_t len) {
  // Ignore all-zero (key-release) reports — but use them to clear the
  // post-capture release gate so the next key can be learned.
  bool allZero = true;
  for (uint16_t i = 0; i < len; i++) {
    if (data[i]) { allZero = false; break; }
  }
  if (allZero) {
    awaitRelease_ = false;
    return;
  }

  const uint8_t clampedLen = len > sizeof(BleReportPattern::bytes) ? sizeof(BleReportPattern::bytes)
                                                                   : static_cast<uint8_t>(len);

  // Learning: capture this report as the armed action's pattern. Skip while
  // waiting for the previous keypress to be released (avoids one held/repeating
  // press filling both forward and back).
  if (learnArmed_) {
    if (awaitRelease_) return;
    BleReportPattern p;
    p.attrHandle = attrHandle;
    p.len = clampedLen;
    memcpy(p.bytes, data, clampedLen);
    p.valid = true;
    if (static_cast<BleHidAction>(learnTarget_) == BleHidAction::PageForward) {
      fwd_ = p;
    } else {
      back_ = p;
    }
    learnArmed_ = false;
    learnComplete_ = true;
    awaitRelease_ = true;  // require a release before any further capture
    LOG_INF("BLEHID", "learned action %u: attr=%u len=%u b0=0x%02X", learnTarget_, attrHandle,
            clampedLen, data[0]);
    return;
  }

  // Runtime: match against learned patterns and enqueue the action.
  auto matches = [&](const BleReportPattern& p) {
    return p.valid && p.attrHandle == attrHandle && p.len == clampedLen &&
           memcmp(p.bytes, data, clampedLen) == 0;
  };
  uint8_t action;
  if (matches(fwd_)) {
    action = static_cast<uint8_t>(BleHidAction::PageForward);
  } else if (matches(back_)) {
    action = static_cast<uint8_t>(BleHidAction::PageBack);
  } else {
    return;
  }
  if (actionQueue_) {
    xQueueSend(actionQueue_, &action, 0);
  }
}

uint8_t BleHidHost::drainPendingActions() {
  uint8_t mask = 0;
  if (!actionQueue_) return 0;
  uint8_t action;
  while (xQueueReceive(actionQueue_, &action, 0) == pdTRUE) {
    mask |= (1u << action);
  }
  return mask;
}

#endif  // ONEPAGE_C61
