#include "CjkUiFontSystem.h"

#ifdef ONEPAGE_C61

#include <EpdFontFamily.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

CjkUiFontSystem cjkUiFontSystem;

constexpr const char* CjkUiFontSystem::kCandidatePaths[];

bool CjkUiFontSystem::loadIfAvailable() {
  if (loaded_) return true;

  for (const char* path : kCandidatePaths) {
    if (!Storage.exists(path)) continue;
    if (font_.load(path)) {
      loaded_ = true;
      LOG_INF("CJKUI", "Loaded CJK UI font: %s", path);
      return true;
    }
    LOG_ERR("CJKUI", "Found but failed to load: %s", path);
  }

  LOG_DBG("CJKUI", "No CJK UI font on SD; Chinese UI will show replacement glyphs");
  return false;
}

void CjkUiFontSystem::attachToUiFonts(EpdFont& ui10Reg, EpdFont& ui10Bold, EpdFont& ui12Reg, EpdFont& ui12Bold,
                                      EpdFont& small) {
  if (!loaded_ || attached_) return;

  // Save originals for detach.
  ui10RegOrig_ = ui10Reg.data;
  ui10BoldOrig_ = ui10Bold.data;
  ui12RegOrig_ = ui12Reg.data;
  ui12BoldOrig_ = ui12Bold.data;
  smallOrig_ = small.data;

  // Copy the flash EpdFontData (POD) into DRAM-resident mutable copies. The
  // const pointer members keep pointing at the builtin flash arrays; only the
  // glyph-miss hook changes, so the builtin Latin/Cyrillic glyphs still render
  // from flash and only missing (CJK) codepoints route to the SD font.
  memcpy(&ui10RegData_, ui10Reg.data, sizeof(EpdFontData));
  memcpy(&ui10BoldData_, ui10Bold.data, sizeof(EpdFontData));
  memcpy(&ui12RegData_, ui12Reg.data, sizeof(EpdFontData));
  memcpy(&ui12BoldData_, ui12Bold.data, sizeof(EpdFontData));
  memcpy(&smallData_, small.data, sizeof(EpdFontData));

  // LXGW WenKai is a single-style file; resolveStyle() maps bold -> the closest
  // present style (regular). CJK has no synthetic bold, so this is expected.
  const uint8_t regStyle = font_.resolveStyle(EpdFontFamily::REGULAR);
  const uint8_t boldStyle = font_.resolveStyle(EpdFontFamily::BOLD);
  font_.wireAsFallback(ui10RegData_, regStyle);
  font_.wireAsFallback(ui10BoldData_, boldStyle);
  font_.wireAsFallback(ui12RegData_, regStyle);
  font_.wireAsFallback(ui12BoldData_, boldStyle);
  font_.wireAsFallback(smallData_, regStyle);

  ui10Reg.data = &ui10RegData_;
  ui10Bold.data = &ui10BoldData_;
  ui12Reg.data = &ui12RegData_;
  ui12Bold.data = &ui12BoldData_;
  small.data = &smallData_;

  attached_ = true;
  LOG_INF("CJKUI", "CJK fallback attached to UI fonts");
}

void CjkUiFontSystem::detachFromUiFonts(EpdFont& ui10Reg, EpdFont& ui10Bold, EpdFont& ui12Reg, EpdFont& ui12Bold,
                                        EpdFont& small) {
  if (!attached_) return;
  if (ui10RegOrig_) ui10Reg.data = ui10RegOrig_;
  if (ui10BoldOrig_) ui10Bold.data = ui10BoldOrig_;
  if (ui12RegOrig_) ui12Reg.data = ui12RegOrig_;
  if (ui12BoldOrig_) ui12Bold.data = ui12BoldOrig_;
  if (smallOrig_) small.data = smallOrig_;
  attached_ = false;
}

#endif  // ONEPAGE_C61
