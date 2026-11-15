#include "KeyboardLayoutSet.h"

#include <HalStorage.h>

#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"

namespace keyboard_layouts {

namespace {

uint8_t indexOf(const freeink::ui::KeyboardLayoutId id) {
  for (uint8_t i = 0; i < COUNT; ++i) {
    if (ALL[i].id == id) return i;
  }
  return COUNT;
}

freeink::ui::KeyboardLayoutId forLanguage(const Language language) {
  for (uint8_t i = 0; i < COUNT; ++i) {
    if (ALL[i].language == language) return ALL[i].id;
  }
  return freeink::ui::KeyboardLayoutId::QwertyEn;
}

constexpr uint16_t ALL_BITS = static_cast<uint16_t>((uint32_t{1} << COUNT) - 1);

uint16_t layoutBit(const freeink::ui::KeyboardLayoutId id) {
  const uint8_t i = indexOf(id);
  return i < COUNT ? bitAt(i) : 0;
}

}  // namespace

uint16_t enabled() {
  // Drop bits naming no layout: the mask comes from a hand-editable file and
  // survives downgrades, so it can carry bits this build does not have. Left
  // in, such a mask would read as "configured" while enabling nothing.
  uint16_t configured = static_cast<uint16_t>(SETTINGS.keyboardLayouts & ALL_BITS);
  if (configured) configured |= layoutBit(forLanguage(I18N.getLanguage()));
  if (configured != 0) {
    if (configured & LATIN_BITS) return configured;
    return static_cast<uint16_t>(configured | layoutBit(freeink::ui::KeyboardLayoutId::QwertyEn));
  }
  // Unconfigured: the UI language's layout plus English. An English UI collapses
  // to one layout and the language key disappears -- there is nowhere to go.
  return static_cast<uint16_t>(layoutBit(forLanguage(I18N.getLanguage())) |
                               layoutBit(freeink::ui::KeyboardLayoutId::QwertyEn));
}

freeink::ui::KeyboardLayoutId startingLayout() {
  const freeink::ui::KeyboardLayoutId preferred = forLanguage(I18N.getLanguage());
  if (enabled() & layoutBit(preferred)) return preferred;
  return next(preferred);
}

freeink::ui::KeyboardLayoutId next(const freeink::ui::KeyboardLayoutId current) {
  const uint16_t mask = enabled();
  const uint8_t from = indexOf(current);
  // A current layout the table does not list still has to lead somewhere.
  const uint8_t start = from < COUNT ? from : 0;
  for (uint8_t step = 1; step <= COUNT; ++step) {
    const uint8_t i = static_cast<uint8_t>((start + step) % COUNT);
    if (mask & bitAt(i)) return ALL[i].id;
  }
  return current;
}

bool builtinLanguage(Language language) {
  for (const auto& layout : ALL)
    if (layout.language == language) return true;
  return false;
}

uint64_t scanCardLayouts() {
  uint64_t mask = 0;
  char path[80];
  for (uint8_t n = 0; n < getLanguageCount(); ++n) {
    if (builtinLanguage(static_cast<Language>(n))) continue;
    std::snprintf(path, sizeof(path), "/languages/keyboards/%s.cpkbd", LANGUAGE_CODES[n]);
    if (!Storage.exists(path)) continue;
    HalFile file;
    uint8_t header[16];
    if (Storage.openFileForRead("KEYBOARD", path, file) && file.read(header, sizeof(header)) == sizeof(header) &&
        std::memcmp(header, "CPKB", 4) == 0 && header[4] == 1 && file.size() <= 4112)
      mask |= languageBit(static_cast<Language>(n));
  }
  return mask;
}

Selection startingSelection(uint64_t cardLayouts) {
  const auto language = I18N.getLanguage();
  return {forLanguage(language), language, !builtinLanguage(language) && (cardLayouts & languageBit(language)) != 0};
}

Selection nextSelection(Selection current, uint64_t cardLayouts) {
  const uint16_t builtins = enabled();
  const uint64_t optional = (SETTINGS.keyboardLanguageLayouts | languageBit(I18N.getLanguage())) & cardLayouts;
  const int total = COUNT + getLanguageCount();
  int from = -1;
  for (int n = 0; n < total; ++n) {
    const auto language = n < COUNT ? ALL[n].language : static_cast<Language>(n - COUNT);
    if (language == current.language && (n < COUNT || !builtinLanguage(language))) {
      from = n;
      break;
    }
  }
  for (int step = 1; step <= total; ++step) {
    const int n = (from + step + total) % total;
    const auto language = n < COUNT ? ALL[n].language : static_cast<Language>(n - COUNT);
    const bool included = n < COUNT ? (builtins & bitAt(uint8_t(n))) != 0
                                    : !builtinLanguage(language) && (optional & languageBit(language)) != 0;
    if (included)
      return {forLanguage(language), language,
              !builtinLanguage(language) && (cardLayouts & languageBit(language)) != 0};
  }
  return current;
}

}  // namespace keyboard_layouts
