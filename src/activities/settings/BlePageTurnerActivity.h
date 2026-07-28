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
  int8_t batteryShown_ = -2;   // battery % reflected in the last Ready render (-2 = never drawn)
  bool connectedShown_ = false;  // connection state reflected in the last Ready render
  char pickedName_[24] = "";   // name of a device just picked from the list (persisted as the bound name)
  ButtonNavigator buttonNavigator;

  // In the Discovering list, the currently-bound remote is pinned as row 0 (like
  // the connected Wi-Fi network sits atop the network list) even when it isn't in
  // the scan results — a connected remote doesn't advertise, so it's never scanned.
  // These helpers map a display row to either that synthetic bound row or a
  // scan-result entry (skipping the bound device to avoid showing it twice).
  bool boundRowShown() const;                       // is a synthetic bound row present?
  uint8_t displayCount() const;                     // total rows (bound + de-duped scan results)
  // Resolves display row -> content. isBound=true means the synthetic bound row.
  bool resolveRow(int row, bool& isBound, void* entryOut) const;

  void persist();
  void disableAndForget();
};

#endif  // ONEPAGE_C61
