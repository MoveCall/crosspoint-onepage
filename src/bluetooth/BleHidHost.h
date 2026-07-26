#pragma once

// BLE HID Host for the page-turner feature (ONEPAGE_C61 only).
//
// Acts as a BLE Central + HID host: scans for a BLE HID page-turner, connects,
// discovers the HID service (0x1812) and its Report characteristics (0x2A4D),
// subscribes to input-report notifications. Instead of a fixed keycode table it
// uses a LEARNING model: the pairing UI arms a learn for "page forward" / "page
// back"; the next key the user presses is captured as a full report pattern
// (characteristic handle + payload bytes). At runtime incoming reports are
// matched byte-for-byte against the two learned patterns and the matching page
// action is posted to a queue drained by the main loop (HalGPIO::update()).
//
// Threading: NimBLE callbacks run on the BLE host task. The only cross-task
// paths are (a) the action queue (BLE task xQueueSend -> main loop
// xQueueReceive) and (b) volatile learn flags. No renderer/activity calls from
// the BLE task.
//
// Single-core coexistence: start()/stop() toggle HalPowerManager::setBleActive
// so the CPU stays at full speed while connected (10 MHz low-power mode breaks
// the BLE controller's radio timing -> interrupt WDT reboot).

#ifdef ONEPAGE_C61

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <cstdint>

enum class BleHidAction : uint8_t {
  PageForward = 0,
  PageBack = 1,
};

// A captured HID report pattern: which characteristic it arrived on plus the
// exact payload bytes. Matched byte-for-byte at runtime.
struct BleReportPattern {
  uint16_t attrHandle = 0;
  uint8_t len = 0;
  uint8_t bytes[8] = {0};
  bool valid = false;
};

enum class BleHidState : uint8_t {
  Stopped,
  Scanning,
  Connecting,
  Connected,
  Disconnected,
};

// A discovered BLE HID device, shown in the pairing list.
struct BleScanEntry {
  char name[24] = {0};
  uint8_t addr[6] = {0};
  uint8_t addrType = 0;
};

class BleHidHost {
 public:
  static BleHidHost& getInstance();

  // Start NimBLE. Idempotent. Returns false on init failure or low SRAM. Sets
  // HalPowerManager::setBleActive(true). After start, call startDiscovery() to
  // browse devices, or (if a bound device is set) it auto-reconnects to it.
  bool start();

  // Tear down NimBLE (call before deep sleep). Sets setBleActive(false).
  bool stop();
  bool isStarted() const { return started_; }
  BleHidState getState() const { return state_; }
  bool isConnected() const { return state_ == BleHidState::Connected; }

  // --- Discovery / pairing (driven by the pairing UI) ---
  // Enter discovery mode: scan and collect nearby BLE HID devices into a list
  // WITHOUT auto-connecting. Clears any previous scan results.
  void startDiscovery();
  uint8_t discoveredCount() const { return scanCount_; }
  // Bumped whenever the device list changes (new device OR a name backfilled),
  // so the pairing UI can redraw when a late-arriving name fills in.
  uint16_t scanRevision() const { return scanRevision_; }
  bool getDiscovered(uint8_t index, BleScanEntry& out) const;
  // Connect to a specific discovered device and remember it as the bound device.
  void connectTo(const uint8_t addr[6], uint8_t addrType);

  // --- Bound device (the one paired remote; auto-reconnected at boot) ---
  void setBoundDevice(const uint8_t addr[6], uint8_t addrType);
  bool hasBoundDevice() const { return boundValid_; }
  void getBoundDevice(uint8_t addr[6], uint8_t& addrType) const;
  void clearBoundDevice();

  // --- Learning (driven by the pairing UI) ---
  // Arm capture: the next non-zero input report becomes `action`'s pattern.
  // requireRelease: if true, first wait for a key-release before capturing (used
  // when chaining forward->back so one held/repeating press can't fill both).
  // For the first/fresh learn pass leave it false so a single press is captured.
  void startLearn(BleHidAction action, bool requireRelease = false);
  void cancelLearn();
  bool isLearning() const { return learnArmed_; }
  // True once a learn armed by startLearn() has captured a report. Cleared by
  // consumeLearnComplete().
  bool learnComplete() const { return learnComplete_; }
  bool consumeLearnComplete();

  // --- Learned patterns (for persistence via JsonSettingsIO) ---
  const BleReportPattern& pattern(BleHidAction action) const;
  void setPattern(BleHidAction action, const BleReportPattern& p);
  void clearPatterns();
  bool hasBothPatterns() const;

  // --- Runtime: drained by HalGPIO::update() on the main loop ---
  // Returns a bitmask of (1<<BleHidAction) for actions that fired this frame.
  uint8_t drainPendingActions();

  BleHidHost(const BleHidHost&) = delete;
  BleHidHost& operator=(const BleHidHost&) = delete;

 private:
  BleHidHost() = default;

  bool started_ = false;
  volatile BleHidState state_ = BleHidState::Stopped;

  // Scan mode: Discovery = collect list, don't connect; Bound = connect only to
  // the bound device when its advertisement is seen.
  enum class Mode : uint8_t { Bound, Discovery };
  volatile Mode mode_ = Mode::Bound;

  // Discovered device list (written by BLE task, read by activity task; count is
  // published last as the barrier).
  static constexpr uint8_t MAX_SCAN = 8;
  BleScanEntry scan_[MAX_SCAN];
  volatile uint8_t scanCount_ = 0;
  volatile uint16_t scanRevision_ = 0;

  // Bound (paired) device — only this address is auto-connected.
  uint8_t boundAddr_[6] = {0};
  uint8_t boundAddrType_ = 0;
  bool boundValid_ = false;
  // Reconnect backoff: after a failed/dropped connection, don't retry until this
  // time (esp_timer microseconds). Prevents a tight connect->fail->connect loop
  // from saturating the single-core radio when the device won't handshake.
  volatile int64_t nextConnectAllowedUs_ = 0;

  BleReportPattern fwd_;
  BleReportPattern back_;

  volatile bool learnArmed_ = false;
  volatile uint8_t learnTarget_ = 0;  // BleHidAction being learned
  volatile bool learnComplete_ = false;
  // After a learn captures, require a key-release (all-zero report) before the
  // next capture, so one held/auto-repeating keypress can't fill both actions.
  volatile bool awaitRelease_ = false;

  QueueHandle_t actionQueue_ = nullptr;

  // Called from the BLE host task when an input report arrives.
  void onReport(uint16_t attrHandle, const uint8_t* data, uint16_t len);
  // Called from the BLE host task for each advertising report seen.
  void onDiscovered(const uint8_t addr[6], uint8_t addrType, const char* name);

  // NimBLE needs a C-callable notify hook; it forwards to the singleton.
  friend int bleHidHostGapEvent(struct ble_gap_event* event, void* arg);
};

#define BLE_HID_HOST BleHidHost::getInstance()

#endif  // ONEPAGE_C61
