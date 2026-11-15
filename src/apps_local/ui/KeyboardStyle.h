#pragma once

#include <FreeInkUI.h>
#include <I18n.h>
#include <Utf8.h>

#include <algorithm>

#include "UnicodeCase.h"

namespace keyboardview {
namespace fui = freeink::ui;
inline constexpr fui::BoxStyle paperKey{fui::Paint::solid(fui::Color::White), fui::Paint::solid(fui::Color::Black),
                                        fui::Paint::solid(fui::Color::Black), 2};
inline constexpr fui::BoxStyle inkKey{fui::Paint::solid(fui::Color::Black), fui::Paint::solid(fui::Color::White),
                                      fui::Paint::solid(fui::Color::Black), 2};
inline constexpr fui::StyleSet characterKeys{paperKey, inkKey, paperKey, inkKey, paperKey, true};
inline constexpr fui::StyleSet functionKeys{inkKey, inkKey, inkKey, inkKey, paperKey, true};
inline constexpr int KeyGap = 6, RowGap = 8, PanelPadding = 4;
inline uint32_t upperCodepoint(uint32_t cp, Language language) {
  return uicase::upper(cp, language == Language::TR || language == Language::AZ);
}
inline void upperLabel(const char* text, Language language, char* buffer, size_t capacity) {
  if (!capacity) return;
  char* out = buffer;
  const auto* cursor = reinterpret_cast<const unsigned char*>(text);
  while (*cursor) {
    auto cp = utf8NextCodepoint(&cursor);
    cp = upperCodepoint(cp, language);
    const size_t bytes = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
    if (size_t(out - buffer) + bytes >= capacity) break;
    out = utf8AppendCodepoint(out, cp);
  }
  *out = 0;
}
inline int touchKeyHeight(int width, bool landscape) {
  return landscape ? 40 : std::clamp((width - 9 * KeyGap - 2 * PanelPadding) * 5 / 40, 44, 56);
}
inline void style(fui::KeyboardProps& props, bool shifted, bool symbols, Language language = I18N.getLanguage()) {
  props.keyStyles = characterKeys;
  props.controlStyles = &functionKeys;
  props.shiftState = shifted || symbols ? fui::StateSelected : fui::StateNormal;
  props.showAltHints = false;
  props.spaceLabel = tr(STR_KEY_SPACE);
  props.okLabel = tr(STR_KEY_RETURN);
  props.modeLabel = symbols ? tr(STR_KEY_LETTERS) : tr(STR_KEY_NUMBERS);
  props.shiftLabel = !symbols && language == Language::JA   ? (shifted ? "あ" : "ア")
                     : !symbols && language == Language::FA ? (shifted ? "ا" : "آ")
                                                            : nullptr;
  props.keyRadius = 8;
  props.gap = KeyGap;
  props.rowGap = RowGap;
  props.padding = {PanelPadding, PanelPadding, PanelPadding, PanelPadding};

  props.digitLabelOffsetX = 0;
  props.background = fui::Paint::solid(fui::Color::White);
}
}  // namespace keyboardview
