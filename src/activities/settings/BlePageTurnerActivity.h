#pragma once

#ifdef ONEPAGE_C61

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Bluetooth page-turner pairing + learning UI.
// Flow: enable -> discover nearby BLE HID devices -> user picks one -> connect
// -> learn "next page" key -> learn "prev page" key -> ready. The chosen device
// is remembered (bound) so only it auto-reconnects at boot. Learned report
// patterns are stored in CrossPointSettings.
class BlePageTurnerActivity final : public Activity {
 public:
  explicit BlePageTurnerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("BlePageTurner", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Step {
    Discovering,    // scanning + showing the device list to pick from
    Connecting,     // connecting to the chosen device
    LearnForward,   // connected, waiting for the "next page" key
    LearnBack,      // waiting for the "prev page" key
    Ready,          // both learned, connected
  };

  Step step_ = Step::Discovering;
  uint8_t selectedIndex = 0;   // highlighted row in the device list
  uint16_t shownRevision = 0;  // scan revision reflected in the last render
  ButtonNavigator buttonNavigator;

  void persist();
  void disableAndForget();
};

#endif  // ONEPAGE_C61
