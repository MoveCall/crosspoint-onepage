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
    step_ = BLE_HID_HOST.isConnected() ? Step::Ready : Step::Connecting;
  } else {
    // Not paired yet: start BLE and browse nearby devices.
    BLE_HID_HOST.start();
    BLE_HID_HOST.startDiscovery();
    step_ = Step::Discovering;
  }
  requestUpdate();
}

void BlePageTurnerActivity::onExit() { Activity::onExit(); }

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
      const uint8_t count = BLE_HID_HOST.discoveredCount();
      // Redraw when the list changes: new device OR a late-arriving name.
      if (BLE_HID_HOST.scanRevision() != shownRevision) {
        requestUpdate();
      }
      if (mappedInput.wasPressed(MappedInputManager::Button::Confirm) && count > 0) {
        BleScanEntry e;
        if (BLE_HID_HOST.getDiscovered(selectedIndex, e)) {
          BLE_HID_HOST.connectTo(e.addr, e.addrType);
          step_ = Step::Connecting;
          requestUpdate();
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

    case Step::Ready:
      if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
        BLE_HID_HOST.startLearn(BleHidAction::PageForward);
        step_ = Step::LearnForward;
        requestUpdate();
      } else if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
        disableAndForget();
      }
      break;
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
    const uint8_t count = BLE_HID_HOST.discoveredCount();
    shownRevision = BLE_HID_HOST.scanRevision();
    const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
    const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
    if (count == 0) {
      renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2, tr(STR_BLE_SCANNING));
    } else {
      GUI.drawList(
          renderer, Rect{0, contentTop, pageWidth, contentHeight}, count, selectedIndex,
          [](int index) -> const char* {
            static char row[24];
            BleScanEntry e;
            if (BLE_HID_HOST.getDiscovered(static_cast<uint8_t>(index), e)) {
              snprintf(row, sizeof(row), "%s", e.name[0] ? e.name : "(unnamed)");
            } else {
              row[0] = '\0';
            }
            return row;
          },
          nullptr, nullptr, nullptr, true);
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
      line1 = tr(STR_BLE_SCANNING);
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
      line2 = BLE_HID_HOST.isConnected() ? tr(STR_BLE_CONNECTED) : tr(STR_BLE_DISCONNECTED);
      break;
    case Step::Discovering:
      break;  // handled above
  }
  renderer.drawCenteredText(UI_10_FONT_ID, midY - 10, line1, true, EpdFontFamily::BOLD);
  if (line2) {
    renderer.drawCenteredText(SMALL_FONT_ID, midY + 15, line2);
  }

  if (step_ == Step::Ready) {
    GUI.drawHelpText(renderer, Rect{0, pageHeight - metrics.buttonHintsHeight - 40, pageWidth, 20},
                     tr(STR_BLE_RELEARN_HINT));
    GUI.drawHelpText(renderer, Rect{0, pageHeight - metrics.buttonHintsHeight - 20, pageWidth, 20},
                     tr(STR_BLE_FORGET_HINT));
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

#endif  // ONEPAGE_C61
