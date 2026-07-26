#pragma once

// CJK UI font fallback for the OnePage (ESP32-C61 / PSRAM) build.
//
// The builtin UI fonts (ubuntu_10/12, flash-resident EpdFontData) carry no CJK
// glyphs, so Chinese UI labels — including the "简体中文" / "繁體中文" entries in
// the language picker — would render as replacement boxes. This system loads a
// CJK .cpfont from the SD card and wires it as a glyph-miss fallback into the
// four UI EpdFont objects: EpdFont::getGlyph() then back-fills any codepoint the
// builtin font lacks from the SD font's on-demand overflow path. Because both
// the bitmap render (renderCharImpl) and the layout measurement (getTextBounds)
// go through the same getGlyph(), the fallback covers rendering AND metrics.
//
// ONEPAGE_C61 only: the X4 build has no PSRAM to hold the CJK glyph cache.

#ifdef ONEPAGE_C61

#include <EpdFont.h>
#include <EpdFontData.h>
#include <SdCardFont.h>

class CjkUiFontSystem {
 public:
  CjkUiFontSystem() = default;
  ~CjkUiFontSystem() = default;
  CjkUiFontSystem(const CjkUiFontSystem&) = delete;
  CjkUiFontSystem& operator=(const CjkUiFontSystem&) = delete;

  // Load the CJK UI font from the SD card (first existing candidate path).
  // Safe to call more than once (no-op once loaded). Returns true on success;
  // logs a warning and returns false if no font file is present — callers must
  // tolerate this (UI simply shows replacement boxes for CJK).
  bool loadIfAvailable();

  // Wire the CJK fallback into the UI EpdFont objects. Must be called after
  // loadIfAvailable() returned true. Copies each builtin EpdFontData into a
  // mutable DRAM copy, sets its glyphMissHandler to the CJK SdCardFont, and
  // redirects EpdFont::data to the copy. Idempotent.
  // `small` is the 8pt font used by some themes' button hints — it must get the
  // fallback too or those hints render garbled/blank in Chinese.
  void attachToUiFonts(EpdFont& ui10Reg, EpdFont& ui10Bold, EpdFont& ui12Reg, EpdFont& ui12Bold, EpdFont& small);

  // Restore the original builtin EpdFontData pointers on the UI fonts.
  void detachFromUiFonts(EpdFont& ui10Reg, EpdFont& ui10Bold, EpdFont& ui12Reg, EpdFont& ui12Bold, EpdFont& small);

  bool isLoaded() const { return loaded_; }
  bool isAttached() const { return attached_; }

 private:
  // Candidate SD paths, tried in order. The LXGW WenKai 12pt .cpfont already
  // present on OnePage cards serves both Simplified and Traditional; 10pt UI
  // reuses the same file (slightly large but legible).
  static constexpr const char* kCandidatePaths[] = {
      "/.fonts/LXGWWenKai-Regular/LXGWWenKai-Regular_12.cpfont",
      "/fonts/LXGWWenKai-Regular/LXGWWenKai-Regular_12.cpfont",
      "/.fonts/LXGWWenKai/LXGWWenKai_12.cpfont",
      "/fonts/LXGWWenKai/LXGWWenKai_12.cpfont",
  };

  SdCardFont font_;
  bool loaded_ = false;
  bool attached_ = false;

  // Mutable EpdFontData copies (BSS, ~144B x4 in DRAM). All const pointer
  // members still reference the builtin flash arrays; only glyphMissHandler /
  // glyphMissCtx are overwritten.
  EpdFontData ui10RegData_{};
  EpdFontData ui10BoldData_{};
  EpdFontData ui12RegData_{};
  EpdFontData ui12BoldData_{};
  EpdFontData smallData_{};

  const EpdFontData* ui10RegOrig_ = nullptr;
  const EpdFontData* ui10BoldOrig_ = nullptr;
  const EpdFontData* ui12RegOrig_ = nullptr;
  const EpdFontData* ui12BoldOrig_ = nullptr;
  const EpdFontData* smallOrig_ = nullptr;
};

extern CjkUiFontSystem cjkUiFontSystem;

#endif  // ONEPAGE_C61
