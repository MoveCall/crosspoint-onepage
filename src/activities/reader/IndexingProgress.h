#pragma once

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <algorithm>

#include "components/UITheme.h"

// Keep indexing feedback inexpensive on an e-paper display.
class IndexingProgress {
  GfxRenderer& renderer;
  Rect popup;
  uint32_t lastRefresh;
  int lastWidth = 0;

 public:
  explicit IndexingProgress(GfxRenderer& renderer)
      : renderer(renderer), popup(GUI.drawPopup(renderer, tr(STR_INDEXING), false)), lastRefresh(millis()) {
    draw(0);
  }

  void update(size_t current, size_t total) {
    if (total == 0) return;
    // Reading all input does not mean final pagination/cache writes have finished.
    const int width = std::min<int>(static_cast<uint64_t>(std::min(current, total)) * (barWidth() - 4) / total,
                                    barWidth() - 5);
    if (width <= lastWidth || millis() - lastRefresh < 1000) return;
    draw(width);
  }

  void finish() {
    // The reader will paint the page next; never force an extra completion refresh.
    if (millis() - lastRefresh >= 1000) draw(barWidth() - 4);
  }

 private:
  int barWidth() const { return std::min<int>(240, renderer.getScreenWidth() - 40); }

  void draw(int width) {
    const int w = barWidth();
    const int x = (renderer.getScreenWidth() - w) / 2;
    const int y = popup.y + popup.height + 10;
    renderer.fillRect(x - 3, y - 3, w + 6, 18, false);
    renderer.drawRect(x, y, w, 12);
    if (width > 0) renderer.fillRect(x + 2, y + 2, std::min(width, w - 4), 8);
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    lastWidth = width;
    lastRefresh = millis();
  }
};
