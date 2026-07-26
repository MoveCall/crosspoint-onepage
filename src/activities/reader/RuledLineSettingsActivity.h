#pragma once

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// In-reader panel to configure the "ruled lines" reading aid: line style,
// vertical offset, thickness, dash density. Full-screen list; Up/Down select a
// row, Confirm cycles the selected row's value (like the reader menu's
// orientation / auto-turn rows). Back returns to the reader and persists.
// Values apply live in SETTINGS; on return the reader re-renders with the new
// style (ruled lines are a pure render-time overlay — no re-layout / cache change).
class RuledLineSettingsActivity final : public Activity {
 public:
  explicit RuledLineSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("RuledLineSettings", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr int ROW_COUNT = 4;  // style, offset, thickness, dash density
  uint8_t selectedRow = 0;
  ButtonNavigator buttonNavigator;

  void adjustCurrentRow(int delta);
  const char* rowLabel(int row) const;
  const char* rowValue(int row) const;
};
