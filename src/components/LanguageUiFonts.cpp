#include "LanguageUiFonts.h"

#include <EpdFontFamily.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <SdCardFont.h>
#include <Utf8.h>

#include <cstdio>

#include "fontIds.h"

namespace {
constexpr uint8_t POINTS[] = {8, 10, 12};
constexpr int PRIMARY[] = {SMALL_FONT_ID, UI_10_FONT_ID, UI_12_FONT_ID};
constexpr int LANGUAGE_FONT_BASE = 0x4c470000;
}  // namespace

LanguageUiFonts::LanguageUiFonts() = default;
LanguageUiFonts::~LanguageUiFonts() = default;

void LanguageUiFonts::begin(GfxRenderer& renderer) {
  struct RegionalFace {
    Language language;
    const char* suffix;
  };
  static constexpr RegionalFace REGIONS[] = {{Language::ZH_CN, "_SC"}, {Language::ZH_TW, "_TC"}};
  const char* variant = "";
  for (const auto& region : REGIONS) {
    if (I18N.getLanguage() == region.language) variant = region.suffix;
  }
  for (size_t i = 0; i < 3; ++i) {
    const int id = LANGUAGE_FONT_BASE + POINTS[i];
    if (!fonts[i]) {
      auto font = makeUniqueNoThrow<SdCardFont>();
      if (!font) {
        LOG_ERR("I18N", "OOM: language UI font");
        continue;
      }
      char path[64];
      snprintf(path, sizeof(path), "/languages/fonts/Interface%s_%u.cpfont", variant, POINTS[i]);
      if (!font->load(path)) {
        if (!variant[0]) continue;
        snprintf(path, sizeof(path), "/languages/fonts/Interface_%u.cpfont", POINTS[i]);
        if (!font->load(path)) continue;
      }
      if (!font->getEpdFont(0)) {
        LOG_ERR("I18N", "Language UI font has no regular face: %s", path);
        continue;
      }
      fonts[i] = std::move(font);
      EpdFontFamily family(fonts[i]->getEpdFont(0), fonts[i]->getEpdFont(1));
      renderer.insertFont(id, family);
      renderer.registerSdCardFont(id, fonts[i].get());
    }
    renderer.setLanguageFallbackFont(PRIMARY[i], id);
  }
}

bool LanguageUiFonts::supportsText(const GfxRenderer& renderer, const char* text, uint8_t points) {
  if (!text || !*text) return false;
  const auto& fonts = renderer.getFontMap();
  const auto supports = [&](int id) {
    const auto font = fonts.find(id);
    if (font == fonts.end()) return false;
    const auto* cursor = reinterpret_cast<const uint8_t*>(text);
    uint32_t cp;
    while ((cp = utf8NextCodepoint(&cursor)))
      if (!font->second.hasCodepoint(cp, EpdFontFamily::REGULAR)) return false;
    return true;
  };
  for (size_t i = 0; i < sizeof(POINTS) / sizeof(POINTS[0]); ++i)
    if (POINTS[i] == points) return supports(PRIMARY[i]) || supports(LANGUAGE_FONT_BASE + points);
  return false;
}
