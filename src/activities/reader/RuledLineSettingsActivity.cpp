#include "RuledLineSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr uint8_t OFFSET_MIN = 0;   // -8px
constexpr uint8_t OFFSET_MAX = 20;  // +12px
enum Row { ROW_STYLE = 0, ROW_OFFSET = 1, ROW_THICKNESS = 2, ROW_DASH = 3 };
}  // namespace

void RuledLineSettingsActivity::onEnter() {
  Activity::onEnter();
  selectedRow = 0;
  requestUpdate();
}

void RuledLineSettingsActivity::onExit() { Activity::onExit(); }

void RuledLineSettingsActivity::adjustCurrentRow(const int delta) {
  switch (selectedRow) {
    case ROW_STYLE:
      SETTINGS.ruledLineStyle = static_cast<uint8_t>((SETTINGS.ruledLineStyle + 5 + delta) % 5);
      break;
    case ROW_THICKNESS:
      SETTINGS.ruledLineThickness = static_cast<uint8_t>((SETTINGS.ruledLineThickness + 3 + delta) % 3);
      break;
    case ROW_DASH:
      SETTINGS.ruledLineDash = static_cast<uint8_t>((SETTINGS.ruledLineDash + 3 + delta) % 3);
      break;
    case ROW_OFFSET: {
      // Cycle forward with wrap so a single Confirm button can walk the whole range.
      const int span = OFFSET_MAX - OFFSET_MIN + 1;
      int v = static_cast<int>(SETTINGS.ruledLineOffset) - OFFSET_MIN + delta;
      v = ((v % span) + span) % span;
      SETTINGS.ruledLineOffset = static_cast<uint8_t>(OFFSET_MIN + v);
      break;
    }
    default:
      break;
  }
  requestUpdate();
}

void RuledLineSettingsActivity::loop() {
  // Up/Down move the selected row (side buttons + front nav via ButtonNavigator).
  buttonNavigator.onNext([this] {
    selectedRow = static_cast<uint8_t>(ButtonNavigator::nextIndex(selectedRow, ROW_COUNT));
    requestUpdate();
  });
  buttonNavigator.onPrevious([this] {
    selectedRow = static_cast<uint8_t>(ButtonNavigator::previousIndex(selectedRow, ROW_COUNT));
    requestUpdate();
  });

  // Confirm cycles the current row's value forward (matches the reader menu's
  // ROTATE_SCREEN / AUTO_PAGE_TURN behavior); it does NOT exit.
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    adjustCurrentRow(+1);
    return;
  }

  // Back returns to the reader; settings are already live in SETTINGS, so persist here.
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    SETTINGS.saveToFile();
    finish();
    return;
  }
}

const char* RuledLineSettingsActivity::rowLabel(const int row) const {
  switch (row) {
    case ROW_STYLE:
      return tr(STR_RULED_LINE_STYLE);
    case ROW_OFFSET:
      return tr(STR_RULED_LINE_OFFSET);
    case ROW_THICKNESS:
      return tr(STR_RULED_LINE_THICKNESS);
    case ROW_DASH:
      return tr(STR_RULED_LINE_DASH);
    default:
      return "";
  }
}

const char* RuledLineSettingsActivity::rowValue(const int row) const {
  static char offbuf[8];
  switch (row) {
    case ROW_STYLE: {
      static const StrId s[] = {StrId::STR_RULED_DASHED, StrId::STR_RULED_SOLID, StrId::STR_RULED_DOTTED,
                                StrId::STR_RULED_DASH_DOT, StrId::STR_RULED_ZIGZAG};
      return I18N.get(s[std::min<uint8_t>(4, SETTINGS.ruledLineStyle)]);
    }
    case ROW_THICKNESS: {
      static const StrId t[] = {StrId::STR_1PX, StrId::STR_2PX, StrId::STR_3PX};
      return I18N.get(t[std::min<uint8_t>(2, SETTINGS.ruledLineThickness)]);
    }
    case ROW_DASH: {
      static const StrId d[] = {StrId::STR_DASH_SPARSE, StrId::STR_DASH_NORMAL, StrId::STR_DASH_DENSE};
      return I18N.get(d[std::min<uint8_t>(2, SETTINGS.ruledLineDash)]);
    }
    case ROW_OFFSET: {
      const int off = static_cast<int>(SETTINGS.ruledLineOffset) - 8;  // display signed
      snprintf(offbuf, sizeof(offbuf), "%+d px", off);
      return offbuf;
    }
    default:
      return "";
  }
}

void RuledLineSettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  auto metrics = UITheme::getInstance().getMetrics();
  Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);

  GUI.drawHeader(renderer, Rect{screen.x, screen.y + metrics.topPadding, screen.width, metrics.headerHeight},
                 tr(STR_RULED_LINE_SETTINGS));

  const int contentTop = screen.y + metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = screen.height - contentTop - metrics.verticalSpacing;

  GUI.drawList(
      renderer, Rect{screen.x, contentTop, screen.width, contentHeight}, ROW_COUNT, selectedRow,
      [this](int index) { return std::string(rowLabel(index)); }, nullptr, nullptr,
      [this](int index) { return std::string(rowValue(index)); }, true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_TOGGLE), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
