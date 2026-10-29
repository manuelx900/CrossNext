#include "ToyboxFonts.h"

#include <EpdFont.h>
#include <EpdFontFamily.h>
#include <Logging.h>

#include "../../CrossPointSettings.h"
#include "../../activities/Activity.h"
#include "../../fontIds.h"
#include "../../util/MenuFontStyle.h"
#include "ToyboxTokens.h"
#include "fonts/book_bold_10.h"
#include "fonts/book_text_10.h"
#include "fonts/instrument_10.h"
#include "fonts/instrument_13.h"
#include "fonts/instrument_24.h"
#include "fonts/reading_serif_11.h"
#include "fonts/reading_serif_14.h"
#include "fonts/reading_serif_bold_11.h"
#include "fonts/reading_serif_bold_12.h"
#include "fonts/reading_serif_bold_14.h"
#include "fonts/reading_serif_bold_16.h"
#include "fonts/toybox_10.h"
#include "fonts/toybox_14.h"
#include "fonts/toybox_18.h"
#include "fonts/toybox_20.h"
#include "fonts/toybox_30.h"
#include "fonts/toybox_44.h"
#include "fonts/toybox_64.h"
#include "fonts/ui_prose_10.h"
#include "fonts/ui_prose_14.h"
#include "fonts/ui_prose_bold_14.h"

extern EpdFontFamily smallFontFamily;
extern EpdFontFamily ui10FontFamily;
extern EpdFontFamily ui12FontFamily;

namespace toybox {
namespace {

// Jersey 25 (SIL OFL, Google Fonts), with Latin letters and Spanish accents, converted in 1-BIT
// mode. The bit depth is the whole point. Our stock fonts are built with
// --2bit, and GfxRenderer's BW path paints a pixel for ANY coverage above zero
// (`bmpVal < 3`), so every antialiased edge floods to solid black: stems fatten,
// counters close, and the type turns to mush. A pixel font converted at 1 bit
// has no coverage to flood. Every glyph pixel is deliberate.
//
// Jersey is also condensed, which matters on a 480px-wide screen where a normal
// grotesque runs out of room.
//
// One face, no bold: at these sizes a synthesised bold would just be the same
// flooding by another route. Weight comes from size and from inversion instead.
EpdFont prose14(&ui_prose_14);
EpdFont proseBold14(&ui_prose_bold_14);
EpdFontFamily proseFamily(&prose14, &proseBold14);
EpdFont library10(&ui_prose_10);
EpdFont libraryBold10(&book_bold_10);
EpdFontFamily libraryFamily(&library10, &libraryBold10);
EpdFont bookText10(&book_text_10);
EpdFontFamily bookListFamily(&bookText10, &libraryBold10);
EpdFont display30(&toybox_30);
EpdFont large44(&toybox_44);
EpdFont huge64(&toybox_64);
EpdFont ui20(&toybox_20);
EpdFont carousel18(&toybox_18);
EpdFont tile10(&toybox_10);
// Noto Serif, converted at 1 bit by us rather than taken from builtinFonts/.
// The builtin cuts are --2bit, and GfxRenderer's BW path paints a pixel for ANY
// coverage above zero, so they flood to mush -- which is why Toybox has its own
// face at all. Converting the same TTF at 1 bit sidesteps the flooding entirely
// and gives a real text face: Jersey is a display cut with uniform heavy stems,
// and a screenful of it reads as permanently bold.
//
// Noto Serif specifically because it is what the EPUB reader sets books in. An
// app whose surface is a page of prose should look like the rest of this
// device's reading, not like its games.
EpdFont button14(&toybox_14);
EpdFont reading11(&reading_serif_11);
EpdFont reading14(&reading_serif_14);
EpdFont readingBold12(&reading_serif_bold_12);
EpdFont readingBold16(&reading_serif_bold_16);
EpdFont instrument10(&instrument_10);
EpdFont instrument13(&instrument_13);
EpdFont instrument24(&instrument_24);
EpdFontFamily displayFamily(&display30);
EpdFontFamily largeFamily(&large44);
EpdFontFamily hugeFamily(&huge64);
EpdFontFamily uiFamily(&ui20);
EpdFontFamily carouselFamily(&carousel18);
EpdFontFamily tileFamily(&tile10);
EpdFontFamily buttonFamily(&button14);
EpdFont readingBold11(&reading_serif_bold_11);
EpdFont readingUiBold14(&reading_serif_bold_14);
EpdFontFamily readingSmallFamily(&reading11, &readingBold11);
EpdFontFamily readingFamily(&reading14, &readingUiBold14);
EpdFontFamily readingBoldSmallFamily(&readingBold12);
EpdFontFamily readingBoldFamily(&readingBold16);
EpdFontFamily serifTileFamily(&instrument13);
EpdFontFamily serifSmallFamily(&instrument10);
EpdFontFamily serifTitleFamily(&instrument24);

bool registered = false;
menufont::Style registeredStyle = menufont::Style::Pixel;

const EpdFontFamily* menuFamily(menufont::Style style, int id) {
  // The built-in serif cuts are too tall for dense captions and board labels.
  if (id == kTileFontId && style != menufont::Style::Pixel) return &smallFontFamily;
  bool small = false;
  switch (id) {
    case kTileFontId:
    case kButtonFontId:
    case kLibraryFontId:
    case kBookListFontId:
      small = true;
      break;
    case kUiFontId:
    case kDisplayFontId:
    case kCarouselTitleFontId:
    case kProseFontId:
      break;
    default:
      return nullptr;
  }
  switch (style) {
    case menufont::Style::Pixel:
      switch (id) {
        case kTileFontId:
          return &tileFamily;
        case kButtonFontId:
          return &buttonFamily;
        case kLibraryFontId:
          return &libraryFamily;
        case kBookListFontId:
          return &bookListFamily;
        case kDisplayFontId:
          return &displayFamily;
        case kCarouselTitleFontId:
          return &carouselFamily;
        case kProseFontId:
          return &proseFamily;
        default:
          return &uiFamily;
      }
    case menufont::Style::Classic:
      return id == kTileFontId ? &smallFontFamily : small ? &ui10FontFamily : &ui12FontFamily;
    case menufont::Style::NotoSans:
      return small ? &libraryFamily : &proseFamily;
    case menufont::Style::NotoSerif:
      return small ? &readingSmallFamily : &readingFamily;
    case menufont::Style::Count:
      break;
  }
  return nullptr;
}

bool bindMenuFonts(GfxRenderer& renderer, menufont::Style style) {
  static constexpr int ids[] = {kLibraryFontId, kBookListFontId, kCarouselTitleFontId, kDisplayFontId,
                                kUiFontId,      kTileFontId,     kButtonFontId,        kProseFontId};
  bool ok = true;
  for (int id : ids) ok = renderer.replaceBuiltinFont(id, *menuFamily(style, id)) && ok;
  return ok;
}

}  // namespace

FontMetrics metricsFor(const int fontId) {
  if (registeredStyle != menufont::Style::Pixel) {
    if (const auto* family = menuFamily(registeredStyle, fontId)) {
      const auto* data = family->getData();
      FontMetrics metrics;
      metrics.ascender = data->ascender;
      if (const auto* glyph = family->getGlyph('H')) {
        metrics.capTop = glyph->top;
        metrics.capHeight = glyph->height;
      }
      return metrics;
    }
  }
  const EpdFontFamily& family = fontId == kHugeFontId               ? hugeFamily
                                : fontId == kLargeFontId            ? largeFamily
                                : fontId == kDisplayFontId          ? displayFamily
                                : fontId == kTileFontId             ? tileFamily
                                : fontId == kButtonFontId           ? buttonFamily
                                : fontId == kReadingBoldFontId      ? readingBoldFamily
                                : fontId == kReadingFontId          ? readingFamily
                                : fontId == kReadingSmallFontId     ? readingSmallFamily
                                : fontId == kReadingBoldSmallFontId ? readingBoldSmallFamily
                                                                    : uiFamily;
  const EpdFontData* data = fontId == kHugeFontId               ? &toybox_64
                            : fontId == kLargeFontId            ? &toybox_44
                            : fontId == kDisplayFontId          ? &toybox_30
                            : fontId == kTileFontId             ? &toybox_10
                            : fontId == kButtonFontId           ? &toybox_14
                            : fontId == kReadingBoldFontId      ? &reading_serif_bold_16
                            : fontId == kReadingFontId          ? &reading_serif_14
                            : fontId == kReadingSmallFontId     ? &reading_serif_11
                            : fontId == kReadingBoldSmallFontId ? &reading_serif_bold_12
                                                                : &toybox_20;
  FontMetrics metrics;
  metrics.ascender = data->ascender;
  // 'H' stands in for cap height: a flat-topped capital with no overshoot, so it
  // measures the band the eye actually aligns to.
  if (const EpdGlyph* glyph = family.getGlyph('H'); glyph != nullptr) {
    metrics.capTop = glyph->top;
    metrics.capHeight = glyph->height;
  }
  return metrics;
}

int drawCapsCentered(const GfxRenderer& renderer, const int fontId, const int x, const int boxY, const int boxH,
                     const char* text, const bool black) {
  const FontMetrics metrics = metricsFor(fontId);
  // Ink top on screen is (y + ascender) - capTop. Solve for the y that puts ink
  // top at the box's centred position.
  const int y = boxY + (boxH - metrics.capHeight) / 2 - metrics.ascender + metrics.capTop;
  renderer.drawText(fontId, x, y, text, black);
  return x;
}

// The cuts, paired with the constants ToyboxTokens.h carries for them.
// Screens are freestanding and cannot see a font, so those constants are the
// only thing standing between a regenerated cut and every centred label in the
// fork drifting at once. Checked here, where the real data is in scope.
void verifyCutMetrics() {
  struct Pair {
    const char* name;
    const EpdFontData* data;
    const EpdFontFamily* family;
    const toybox::CutMetrics* cut;
  };
  const Pair pairs[] = {
      {"toybox_10", &toybox_10, &tileFamily, &toybox::kTileCut},
      {"toybox_14", &toybox_14, &buttonFamily, &toybox::kButtonCut},
      {"toybox_20", &toybox_20, &uiFamily, &toybox::kUiCut},
      {"toybox_30", &toybox_30, &displayFamily, &toybox::kDisplayCut},
      {"toybox_44", &toybox_44, &largeFamily, &toybox::kLargeCut},
      {"toybox_64", &toybox_64, &hugeFamily, &toybox::kHugeCut},
      {"instrument_10", &instrument_10, &serifSmallFamily, &toybox::kSerifSmallCut},
      {"instrument_13", &instrument_13, &serifTileFamily, &toybox::kSerifTileCut},
      {"instrument_24", &instrument_24, &serifTitleFamily, &toybox::kSerifTitleCut},
      {"reading_serif_11", &reading_serif_11, &readingSmallFamily, &toybox::kReadingSmallCut},
      {"reading_serif_14", &reading_serif_14, &readingFamily, &toybox::kReadingCut},
      {"reading_serif_bold_12", &reading_serif_bold_12, &readingBoldSmallFamily, &toybox::kReadingBoldSmallCut},
      {"reading_serif_bold_16", &reading_serif_bold_16, &readingBoldFamily, &toybox::kReadingBoldCut},
  };
  for (const Pair& pair : pairs) {
    const EpdGlyph* glyph = pair.family->getGlyph('H');
    const int ink = glyph != nullptr ? glyph->height : 0;
    if (pair.data->advanceY == pair.cut->lineHeight && pair.data->ascender == pair.cut->ascender &&
        ink == pair.cut->inkHeight) {
      continue;
    }
    LOG_ERR("TOYBOX", "%s moved: advanceY %d/%d ascender %d/%d ink %d/%d -- update CutMetrics in ToyboxTokens.h",
            pair.name, pair.data->advanceY, pair.cut->lineHeight, pair.data->ascender, pair.cut->ascender, ink,
            pair.cut->inkHeight);
  }
}

menufont::Style interfaceFontStyle() {
  const auto section = activityManager.currentInterfaceFontSection();
  const auto item = activityManager.currentInterfaceFontItem();
  return menufont::resolve(SETTINGS.menuFontStyle,
                           section == menufont::Section::General ? 0 : SETTINGS.menuFontOverrides[int(section)],
                           item < menufont::kItemCount ? SETTINGS.menuFontItemOverrides[item] : 0);
}

void syncInterfaceFonts(GfxRenderer& renderer) {
  if (!registered) {
    ensureFonts(renderer);
    return;
  }
  const auto style = interfaceFontStyle();
  if (registeredStyle != style && bindMenuFonts(renderer, style)) registeredStyle = style;
}

void ensureFonts(GfxRenderer& renderer) {
  // Font settings can clear this registry while the pixel fonts remain loaded.
  // Shared map entries are reused; no image or per-label allocation.
  renderer.setFallbackFont(kButtonFontId, UI_10_FONT_ID);
  renderer.setFallbackFont(kUiFontId, UI_12_FONT_ID);
  renderer.setFallbackFont(kDisplayFontId, UI_12_FONT_ID);
  renderer.setFallbackFont(kTileFontId, SMALL_FONT_ID);
  renderer.setFallbackFont(kLibraryFontId, UI_10_FONT_ID);
  renderer.setFallbackFont(kBookListFontId, UI_10_FONT_ID);
  renderer.setFallbackFont(kProseFontId, UI_12_FONT_ID);
  renderer.setFallbackFont(kCarouselTitleFontId, UI_12_FONT_ID);
  const auto style = interfaceFontStyle();
  if (registered) {
    syncInterfaceFonts(renderer);
    return;
  }
  verifyCutMetrics();
  // Registered from here rather than main.cpp so the design language costs zero
  // upstream surface. insertFont is idempotent per id, but the guard keeps this
  // free to call from every activity's onEnter().
  renderer.insertFont(kProseFontId, proseFamily);
  renderer.insertFont(kLibraryFontId, libraryFamily);
  renderer.insertFont(kBookListFontId, bookListFamily);
  renderer.insertFont(kCarouselTitleFontId, carouselFamily);
  renderer.insertFont(kDisplayFontId, displayFamily);
  renderer.insertFont(kLargeFontId, largeFamily);
  renderer.insertFont(kHugeFontId, hugeFamily);
  renderer.insertFont(kUiFontId, uiFamily);
  renderer.insertFont(kTileFontId, tileFamily);
  renderer.insertFont(kButtonFontId, buttonFamily);
  renderer.insertFont(kReadingFontId, readingFamily);
  renderer.insertFont(kReadingSmallFontId, readingSmallFamily);
  renderer.insertFont(kReadingBoldSmallFontId, readingBoldSmallFamily);
  renderer.insertFont(kReadingBoldFontId, readingBoldFamily);
  renderer.insertFont(kSerifTileFontId, serifTileFamily);
  renderer.insertFont(kSerifSmallFontId, serifSmallFamily);
  renderer.insertFont(kSerifTitleFontId, serifTitleFamily);
  if (bindMenuFonts(renderer, style)) registeredStyle = style;
  registered = true;
}

}  // namespace toybox
