#include "BlePageTurnerActivity.h"

#ifdef ONEPAGE_C61

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstring>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "bluetooth/BleHidHost.h"
#include "components/UITheme.h"
#include "fontIds.h"

void BlePageTurnerActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  shownRevision = 0;

  // Entered because the user just switched the page-turner ON. Load any saved
  // pairing into the host and start BLE.
  const bool paired = SETTINGS.bleDeviceValid && SETTINGS.bleFwdLen > 0 && SETTINGS.bleBackLen > 0;
  if (paired) {
    BleReportPattern fwd;
    fwd.attrHandle = SETTINGS.bleFwdAttrHandle;
    fwd.len = SETTINGS.bleFwdLen;
    memcpy(fwd.bytes, SETTINGS.bleFwdBytes, sizeof(fwd.bytes));
    fwd.valid = true;
    BleReportPattern back;
    back.attrHandle = SETTINGS.bleBackAttrHandle;
    back.len = SETTINGS.bleBackLen;
    memcpy(back.bytes, SETTINGS.bleBackBytes, sizeof(back.bytes));
    back.valid = true;
    BLE_HID_HOST.setPattern(BleHidAction::PageForward, fwd);
    BLE_HID_HOST.setPattern(BleHidAction::PageBack, back);
    BLE_HID_HOST.setBoundDevice(SETTINGS.bleDeviceAddr, SETTINGS.bleDeviceAddrType);
    BLE_HID_HOST.start();  // auto-reconnects the bound device
    // Already paired: go straight to Ready. The Ready view shows "connecting" until
    // the bound device reconnects — it must NOT re-run the learn flow (Connecting
    // step is only for a freshly-picked device from the discovery list).
    step_ = Step::Ready;
  } else {
    // Not paired yet: start BLE and browse nearby devices.
    BLE_HID_HOST.start();
    BLE_HID_HOST.startDiscovery();
    step_ = Step::Discovering;
  }
  requestUpdate();
}

void BlePageTurnerActivity::onExit() { Activity::onExit(); }

// --- Discovering-list row model -------------------------------------------
// Row 0 is the bound remote (if any) pinned like the connected Wi-Fi network;
// remaining rows are scan results with the bound device removed (a connected
// remote doesn't advertise, so it never appears in the scan anyway).

bool BlePageTurnerActivity::boundRowShown() const {
  // Pin the bound remote even if its name wasn't captured (bindings created
  // before names were persisted) — the title falls back to its MAC tail.
  return SETTINGS.bleDeviceValid != 0;
}

uint8_t BlePageTurnerActivity::displayCount() const {
  const bool bound = boundRowShown();
  uint8_t n = bound ? 1 : 0;
  const uint8_t scanned = BLE_HID_HOST.discoveredCount();
  for (uint8_t i = 0; i < scanned; ++i) {
    BleScanEntry e;
    if (BLE_HID_HOST.getDiscovered(i, e) &&
        !(bound && memcmp(e.addr, SETTINGS.bleDeviceAddr, 6) == 0)) {
      ++n;
    }
  }
  return n;
}

bool BlePageTurnerActivity::resolveRow(int row, bool& isBound, void* entryOut) const {
  const bool bound = boundRowShown();
  if (bound && row == 0) {
    isBound = true;
    return true;
  }
  isBound = false;
  int want = row - (bound ? 1 : 0);  // index among de-duped scan results
  const uint8_t scanned = BLE_HID_HOST.discoveredCount();
  int seen = 0;
  for (uint8_t i = 0; i < scanned; ++i) {
    BleScanEntry e;
    if (!BLE_HID_HOST.getDiscovered(i, e)) continue;
    if (bound && memcmp(e.addr, SETTINGS.bleDeviceAddr, 6) == 0) continue;  // skip bound dup
    if (seen == want) {
      *static_cast<BleScanEntry*>(entryOut) = e;
      return true;
    }
    ++seen;
  }
  return false;
}

void BlePageTurnerActivity::persist() {
  const auto& fwd = BLE_HID_HOST.pattern(BleHidAction::PageForward);
  const auto& back = BLE_HID_HOST.pattern(BleHidAction::PageBack);
  SETTINGS.bleFwdAttrHandle = fwd.attrHandle;
  SETTINGS.bleFwdLen = fwd.len;
  memcpy(SETTINGS.bleFwdBytes, fwd.bytes, sizeof(SETTINGS.bleFwdBytes));
  SETTINGS.bleBackAttrHandle = back.attrHandle;
  SETTINGS.bleBackLen = back.len;
  memcpy(SETTINGS.bleBackBytes, back.bytes, sizeof(SETTINGS.bleBackBytes));
  // Persist the bound device too.
  uint8_t addr[6];
  uint8_t addrType;
  BLE_HID_HOST.getBoundDevice(addr, addrType);
  memcpy(SETTINGS.bleDeviceAddr, addr, 6);
  SETTINGS.bleDeviceAddrType = addrType;
  SETTINGS.bleDeviceValid = BLE_HID_HOST.hasBoundDevice() ? 1 : 0;
  // Remember the display name of the just-picked remote so the device list can
  // show it (pinned, WiFi-style) even while it's connected and not advertising.
  if (pickedName_[0]) {
    strncpy(SETTINGS.bleDeviceName, pickedName_, sizeof(SETTINGS.bleDeviceName) - 1);
    SETTINGS.bleDeviceName[sizeof(SETTINGS.bleDeviceName) - 1] = '\0';
  }
  SETTINGS.saveToFile();
}

void BlePageTurnerActivity::disableAndForget() {
  BLE_HID_HOST.clearPatterns();
  BLE_HID_HOST.clearBoundDevice();
  BLE_HID_HOST.stop();
  // Forgetting also switches the feature off; re-enable to pair a new remote.
  SETTINGS.blePageTurnerOn = 0;
  SETTINGS.bleFwdLen = 0;
  SETTINGS.bleBackLen = 0;
  SETTINGS.bleDeviceValid = 0;
  SETTINGS.bleDeviceName[0] = '\0';
  SETTINGS.saveToFile();
  finish();
}

void BlePageTurnerActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  switch (step_) {
    case Step::Discovering: {
      const uint8_t count = displayCount();
      // Redraw when the list changes: new device OR a late-arriving name. Also
      // redraw when the bound remote's cached battery changes (its pinned row).
      if (BLE_HID_HOST.scanRevision() != shownRevision ||
          batteryShown_ != BLE_HID_HOST.boundBatteryLevel()) {
        requestUpdate();
      }
      if (mappedInput.wasPressed(MappedInputManager::Button::Confirm) && count > 0) {
        bool isBound = false;
        BleScanEntry e;
        if (resolveRow(selectedIndex, isBound, &e)) {
          if (isBound) {
            // The pinned bound remote is already "yours" — selecting it just
            // returns to the Ready view (no re-pair, no disconnect).
            step_ = Step::Ready;
            requestUpdate();
          } else {
            // Remember the picked name so it persists as the bound-device name.
            strncpy(pickedName_, e.name, sizeof(pickedName_) - 1);
            pickedName_[sizeof(pickedName_) - 1] = '\0';
            BLE_HID_HOST.connectTo(e.addr, e.addrType);
            step_ = Step::Connecting;
            requestUpdate();
          }
        }
        break;
      }
      if (count > 0) {
        // Use the shared navigator so side up/down AND front left/right both
        // move the selection (same as the language/settings lists), with
        // long-press continuous scroll.
        buttonNavigator.onNextRelease([this, count] {
          selectedIndex = static_cast<uint8_t>(ButtonNavigator::nextIndex(selectedIndex, count));
          requestUpdate();
        });
        buttonNavigator.onPreviousRelease([this, count] {
          selectedIndex = static_cast<uint8_t>(ButtonNavigator::previousIndex(selectedIndex, count));
          requestUpdate();
        });
        buttonNavigator.onNextContinuous([this, count] {
          selectedIndex = static_cast<uint8_t>(ButtonNavigator::nextIndex(selectedIndex, count));
          requestUpdate();
        });
        buttonNavigator.onPreviousContinuous([this, count] {
          selectedIndex = static_cast<uint8_t>(ButtonNavigator::previousIndex(selectedIndex, count));
          requestUpdate();
        });
      }
      break;
    }

    case Step::Connecting:
      if (BLE_HID_HOST.isConnected()) {
        BLE_HID_HOST.startLearn(BleHidAction::PageForward);
        step_ = Step::LearnForward;
        requestUpdate();
      }
      break;

    case Step::LearnForward:
      if (BLE_HID_HOST.consumeLearnComplete()) {
        // 2nd key: require releasing the first key so one press can't fill both.
        BLE_HID_HOST.startLearn(BleHidAction::PageBack, /*requireRelease=*/true);
        step_ = Step::LearnBack;
        requestUpdate();
      }
      break;

    case Step::LearnBack:
      if (BLE_HID_HOST.consumeLearnComplete()) {
        persist();
        step_ = Step::Ready;
        requestUpdate();
      }
      break;

    case Step::Ready: {
      const bool connected = BLE_HID_HOST.isConnected();
      if (connected && mappedInput.wasPressed(MappedInputManager::Button::Left)) {
        // Re-learn buttons — only meaningful while the remote is connected.
        BLE_HID_HOST.startLearn(BleHidAction::PageForward);
        step_ = Step::LearnForward;
        requestUpdate();
      } else if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
        // Pair a NEW remote without forgetting the old one first: jump straight
        // to the device list. Picking + learning a new device persists over the
        // old binding; backing out (Back) keeps the old binding intact. BLE stays on.
        BLE_HID_HOST.startDiscovery();
        selectedIndex = 0;
        step_ = Step::Discovering;
        requestUpdate();
      } else if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
        disableAndForget();
      } else if (connectedShown_ != connected || batteryShown_ != BLE_HID_HOST.batteryLevel()) {
        // Connection state or battery changed since the screen was drawn (e.g. the
        // bound remote just reconnected, or its battery level arrived) — refresh.
        requestUpdate();
      }
      break;
    }
  }
}

void BlePageTurnerActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BLE_PAGE_TURNER));

  // Device-list view has its own layout; other steps show centered messages.
  if (step_ == Step::Discovering) {
    const uint8_t count = displayCount();
    shownRevision = BLE_HID_HOST.scanRevision();
    batteryShown_ = BLE_HID_HOST.boundBatteryLevel();
    const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
    const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
    if (count == 0) {
      renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2, tr(STR_BLE_SCANNING));
    } else {
      GUI.drawList(
          renderer, Rect{0, contentTop, pageWidth, contentHeight}, count, selectedIndex,
          [this](int index) -> std::string {
            bool isBound = false;
            BleScanEntry e;
            if (!resolveRow(index, isBound, &e)) return std::string();
            if (isBound) {
              if (SETTINGS.bleDeviceName[0]) return std::string(SETTINGS.bleDeviceName);
              // No stored name (legacy binding): show the MAC tail so the row
              // isn't blank. BLE addr is little-endian; bytes [2][1][0] are the tail.
              char tail[24];
              snprintf(tail, sizeof(tail), "%02X:%02X:%02X", SETTINGS.bleDeviceAddr[2],
                       SETTINGS.bleDeviceAddr[1], SETTINGS.bleDeviceAddr[0]);
              return std::string(tail);
            }
            return e.name[0] ? std::string(e.name) : std::string(tr(STR_UNNAMED));
          },
          nullptr, nullptr,
          [this](int index) -> std::string {
            // Value column: for the pinned bound remote, show its status +
            // last-known battery (cached; live battery needs a live connection),
            // WiFi-list style. Scan results have no value.
            bool isBound = false;
            BleScanEntry e;
            if (!resolveRow(index, isBound, &e) || !isBound) return std::string();
            std::string s = BLE_HID_HOST.isConnected() ? tr(STR_BLE_CONNECTED) : tr(STR_BLE_PAIRED);
            const int8_t bat = BLE_HID_HOST.boundBatteryLevel();
            if (bat >= 0) {
              char suffix[12];
              snprintf(suffix, sizeof(suffix), " %d%%", bat);
              s += suffix;
            }
            return s;
          },
          true);
    }
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  const int midY = pageHeight / 2;
  const char* line1 = "";
  const char* line2 = nullptr;
  switch (step_) {
    case Step::Connecting:
      line1 = tr(STR_BLE_CONNECTING);
      break;
    case Step::LearnForward:
      line1 = tr(STR_BLE_CONNECTED);
      line2 = tr(STR_BLE_LEARN_FORWARD);
      break;
    case Step::LearnBack:
      line1 = tr(STR_BLE_CONNECTED);
      line2 = tr(STR_BLE_LEARN_BACK);
      break;
    case Step::Ready:
      line1 = tr(STR_BLE_LEARN_DONE);
      // Paired remote auto-reconnects; show "connecting" until it's back, not
      // "disconnected" (which implies a problem).
      line2 = BLE_HID_HOST.isConnected() ? tr(STR_BLE_CONNECTED) : tr(STR_BLE_CONNECTING);
      break;
    case Step::Discovering:
      break;  // handled above
  }
  renderer.drawCenteredText(UI_10_FONT_ID, midY - 10, line1, true, EpdFontFamily::BOLD);
  if (line2) {
    renderer.drawCenteredText(SMALL_FONT_ID, midY + 15, line2);
  }

  if (step_ == Step::Ready) {
    const bool connected = BLE_HID_HOST.isConnected();
    connectedShown_ = connected;
    // Battery of the connected remote (standard Battery Service); -1 = unknown.
    const int8_t bat = BLE_HID_HOST.batteryLevel();
    batteryShown_ = bat;
    if (bat >= 0) {
      // Simple battery icon (outline + proportional fill + nub) with % beside it,
      // centered. Self-drawn (not the status-bar icon, which overlays a USB
      // charging bolt that would be misleading for the remote).
      char pct[8];
      snprintf(pct, sizeof(pct), "%d%%", bat);
      constexpr int iconW = 26, iconH = 13, nub = 2, gap = 6;
      const int pctW = renderer.getTextWidth(SMALL_FONT_ID, pct);
      const int totalW = iconW + nub + gap + pctW;
      const int x0 = (pageWidth - totalW) / 2;
      const int y0 = midY + 34;
      renderer.drawRect(x0, y0, iconW, iconH);                       // body outline
      renderer.fillRect(x0 + iconW, y0 + iconH / 2 - 3, nub, 6);     // positive nub
      const int fillMax = iconW - 4;
      int fillW = bat * fillMax / 100;
      if (fillW < 1 && bat > 0) fillW = 1;
      if (fillW > 0) renderer.fillRect(x0 + 2, y0 + 2, fillW, iconH - 4);
      renderer.drawText(SMALL_FONT_ID, x0 + iconW + nub + gap, y0 - 2, pct);
    }
  }

  // Bottom button hints (front keys) + side hint (the Down side key forgets).
  if (step_ == Step::Ready) {
    const bool connected = BLE_HID_HOST.isConnected();
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", connected ? tr(STR_BLE_RELEARN) : "",
                                              tr(STR_BLE_PAIR));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    GUI.drawSideButtonHints(renderer, "", tr(STR_FORGET_BUTTON));
  } else {
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  renderer.displayBuffer();
}

#endif  // ONEPAGE_C61
