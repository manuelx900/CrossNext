#include "KeyboardEntryActivity.h"

#include <BidiUtils.h>
#include <ChromeRegion.h>
#include <HalGPIO.h>
#include <I18n.h>
#include <Utf8.h>

#include <algorithm>
#include <cstring>

#include "HapticFeedback.h"
#include "KeyboardLayoutSet.h"
#include "MappedInputManager.h"
#include "apps_local/ui/KeyboardStyle.h"
#include "apps_local/ui/KeyboardSymbols.h"
#include "apps_local/ui/KeyboardViewport.h"
#include "apps_local/ui/PixelMenuTheme.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace fui = freeink::ui;

namespace {

constexpr fui::ActionId ACTION_KEY = 1;
constexpr int KEYBOARD_MIN_GAP = 6;
constexpr int KEYBOARD_PANEL_PADDING = 4;
constexpr int BUTTON_KEY_HEIGHT = 42;
constexpr int BUTTON_KEYBOARD_HINT_GAP = 4;

int keyboardGap(const ThemeMetrics& metrics) { return std::max(metrics.keyboardKeySpacing, KEYBOARD_MIN_GAP); }

int keyboardKeysHeight(const ThemeMetrics& metrics, const int rows, const bool hasTouch, bool landscape, int width) {
  const int keyHeight = hasTouch ? keyboardview::touchKeyHeight(width, landscape) : BUTTON_KEY_HEIGHT;
  const int gap = hasTouch ? keyboardview::RowGap : keyboardGap(metrics);
  return rows * keyHeight + (rows > 1 ? (rows - 1) * gap : 0) + KEYBOARD_PANEL_PADDING * 2;
}

// ---------------------------------------------------------------------------
// URL layers. The SDK builtin layouts have no URL variant (":", "/", ".", the
// snippet panel), so these are app-defined tables over the same public
// KeyboardLayout structs. URLs are ASCII, so the letter rows are EN-arranged
// regardless of UI language.
// ---------------------------------------------------------------------------

constexpr uint8_t URL_KEY_WIDTH = 2;
constexpr uint8_t URL_WIDE_CONTROL_WIDTH = 3;

#define UK(label, output, value) \
  fui::KeyboardKey { label, output, fui::KeyKind::Normal, fui::StateNormal, value, URL_KEY_WIDTH, true, nullptr }
#define UKA(label, output, value, alt) \
  fui::KeyboardKey { label, output, fui::KeyKind::Normal, fui::StateNormal, value, URL_KEY_WIDTH, true, alt }
#define UKW(label, output, value, units) \
  fui::KeyboardKey { label, output, fui::KeyKind::Normal, fui::StateNormal, value, units *URL_KEY_WIDTH, true, nullptr }
#define UKS(label, kind, value, units) \
  fui::KeyboardKey { label, nullptr, kind, fui::StateNormal, value, units *URL_KEY_WIDTH, true, nullptr }
#define UK15(label, kind, value) \
  fui::KeyboardKey { label, nullptr, kind, fui::StateNormal, value, URL_WIDE_CONTROL_WIDTH, true, nullptr }

constexpr int16_t URL_PANEL_VALUE = -3;  // mirrors KeyboardEntryActivity::URL_PANEL_KEY

const fui::KeyboardKey URL_ROW1[] = {UK("q", "q", 'q'), UK("w", "w", 'w'), UK("e", "e", 'e'), UK("r", "r", 'r'),
                                     UK("t", "t", 't'), UK("y", "y", 'y'), UK("u", "u", 'u'), UK("i", "i", 'i'),
                                     UK("o", "o", 'o'), UK("p", "p", 'p')};
const fui::KeyboardKey URL_ROW2[] = {UK("a", "a", 'a'), UK("s", "s", 's'), UK("d", "d", 'd'),
                                     UK("f", "f", 'f'), UK("g", "g", 'g'), UK("h", "h", 'h'),
                                     UK("j", "j", 'j'), UK("k", "k", 'k'), UK("l", "l", 'l')};
const fui::KeyboardKey URL_ROW3[] = {UK15(nullptr, fui::KeyKind::Shift, fui::QWERTY_KEY_SHIFT),
                                     UK("z", "z", 'z'),
                                     UK("x", "x", 'x'),
                                     UK("c", "c", 'c'),
                                     UK("v", "v", 'v'),
                                     UK("b", "b", 'b'),
                                     UK("n", "n", 'n'),
                                     UK("m", "m", 'm'),
                                     UK15("Del", fui::KeyKind::Delete, fui::QWERTY_KEY_BACKSPACE)};
// URLs have no spaces, so the URL bottom row spends the space slot on ":",
// "/", "." and the snippet-panel toggle instead (the legacy keyboard did the
// same with its "URL" key).
const fui::KeyboardKey URL_BOTTOM[] = {UKS("?123", fui::KeyKind::Mode, fui::QWERTY_KEY_MODE, 2),
                                       UK(":", ":", ':'),
                                       UK("/", "/", '/'),
                                       UK(".", ".", '.'),
                                       UKW("URL", nullptr, URL_PANEL_VALUE, 3),
                                       UKS("OK", fui::KeyKind::Ok, fui::QWERTY_KEY_ENTER, 2)};

const fui::KeyboardKey URL_SHIFT_ROW1[] = {UK("Q", "Q", 'Q'), UK("W", "W", 'W'), UK("E", "E", 'E'), UK("R", "R", 'R'),
                                           UK("T", "T", 'T'), UK("Y", "Y", 'Y'), UK("U", "U", 'U'), UK("I", "I", 'I'),
                                           UK("O", "O", 'O'), UK("P", "P", 'P')};
const fui::KeyboardKey URL_SHIFT_ROW2[] = {UK("A", "A", 'A'), UK("S", "S", 'S'), UK("D", "D", 'D'),
                                           UK("F", "F", 'F'), UK("G", "G", 'G'), UK("H", "H", 'H'),
                                           UK("J", "J", 'J'), UK("K", "K", 'K'), UK("L", "L", 'L')};
const fui::KeyboardKey URL_SHIFT_ROW3[] = {UK15(nullptr, fui::KeyKind::Shift, fui::QWERTY_KEY_SHIFT),
                                           UK("Z", "Z", 'Z'),
                                           UK("X", "X", 'X'),
                                           UK("C", "C", 'C'),
                                           UK("V", "V", 'V'),
                                           UK("B", "B", 'B'),
                                           UK("N", "N", 'N'),
                                           UK("M", "M", 'M'),
                                           UK15("Del", fui::KeyKind::Delete, fui::QWERTY_KEY_BACKSPACE)};

// Snippet keys: multi-character outputs, stable ids above the localized-key
// range so they never collide with layout key ids.
const fui::KeyboardKey URL_SNIP_ROW1[] = {UK("https://", "https://", 2001), UK("www.", "www.", 2002),
                                          UK(".com", ".com", 2003)};
const fui::KeyboardKey URL_SNIP_ROW2[] = {UK("http://", "http://", 2004), UK("192.168.", "192.168.", 2005),
                                          UK(".org", ".org", 2006)};
const fui::KeyboardKey URL_SNIP_ROW3[] = {UK("/opds", "/opds", 2007), UK(":8080", ":8080", 2008),
                                          UK(".net", ".net", 2009)};
const fui::KeyboardKey URL_SNIP_BOTTOM[] = {UKS("abc", fui::KeyKind::Mode, fui::QWERTY_KEY_MODE, 2),
                                            UKW("URL", nullptr, URL_PANEL_VALUE, 3),
                                            UK15("Del", fui::KeyKind::Delete, fui::QWERTY_KEY_BACKSPACE),
                                            UKS("OK", fui::KeyKind::Ok, fui::QWERTY_KEY_ENTER, 2)};

#undef UK
#undef UKA
#undef UKW
#undef UKS
#undef UK15

const fui::KeyboardRow URL_ROWS[] = {{URL_ROW1, 10, 0}, {URL_ROW2, 9, 1}, {URL_ROW3, 9, 0}, {URL_BOTTOM, 6, 0}};
const fui::KeyboardRow URL_SHIFT_ROWS[] = {
    {URL_SHIFT_ROW1, 10, 0}, {URL_SHIFT_ROW2, 9, 1}, {URL_SHIFT_ROW3, 9, 0}, {URL_BOTTOM, 6, 0}};
const fui::KeyboardRow URL_SNIP_ROWS[] = {
    {URL_SNIP_ROW1, 3, 0}, {URL_SNIP_ROW2, 3, 0}, {URL_SNIP_ROW3, 3, 0}, {URL_SNIP_BOTTOM, 4, 0}};

const fui::KeyboardLayout URL_LAYOUT{URL_ROWS, 4};
const fui::KeyboardLayout URL_SHIFT_LAYOUT{URL_SHIFT_ROWS, 4};
const fui::KeyboardLayout URL_SNIPPET_LAYOUT{URL_SNIP_ROWS, 4};

}  // namespace

void KeyboardEntryActivity::onEnter() {
  Activity::onEnter();
  cursorPos = text.length();
  // URL layers are EN-arranged app tables; everything else opens on the UI
  // language's layout.
  cardLayouts = inputType == InputType::Url ? 0 : keyboard_layouts::scanCardLayouts();
  const auto selected =
      inputType == InputType::Url ? keyboard_layouts::Selection{} : keyboard_layouts::startingSelection(cardLayouts);
  layoutId = selected.builtin;
  layoutLanguage = selected.language;
  useLanguageKeyboard = selected.external && languageKeyboard.load(layoutLanguage);
  if (useLanguageKeyboard && pinyin.begin(layoutLanguage)) languageKeyboard.candidates(pinyin);
  const uint16_t enabledLayouts = keyboard_layouts::enabled();
  showLangKey = useLanguageKeyboard || (enabledLayouts & (enabledLayouts - 1)) != 0 ||
                (SETTINGS.keyboardLanguageLayouts & cardLayouts) != 0;
  shifted = false;
  symbols = false;
  urlPanel = false;
  cursorMode = false;
  togglePos = false;
  selRow = 0;
  selCol = 0;
  delPressCount = 0;
  hintVisible = false;
  hintShowTime = 0;
  rightHeld = false;
  rightLongHandled = false;
  savedCursorPos = 0;
  rightStartCursorPos = 0;
  touchRouter.reset();
  touchRouter.holdMs = TOUCH_LONG_PRESS_MS;
  touchRouter.overrideHoldMs = TOUCH_DEL_LONG_PRESS_MS;
  interactionsReady = false;
  alternates.reset();
  revealGate.arm();
  requestUpdate();
}

void KeyboardEntryActivity::onExit() {
  paintedKeys = {};
  pinyin.close();
  languageKeyboard.clear();
  Activity::onExit();
}

const fui::KeyboardLayout& KeyboardEntryActivity::currentLayout() const {
  if (symbols) return keyboardview::symbols(shifted, showLangKey);
  if (useLanguageKeyboard) return languageKeyboard.layout(shifted);
  if (inputType == InputType::Url) {
    if (urlPanel) return URL_SNIPPET_LAYOUT;
    return shifted ? URL_SHIFT_LAYOUT : URL_LAYOUT;
  }
  return fui::builtinKeyboardLayout(layoutId, shifted, false, /*numberRow=*/false, showLangKey);
}

const fui::KeyboardKey* KeyboardEntryActivity::selectedKey() const {
  const fui::KeyboardLayout& layout = currentLayout();
  if (selRow < 0 || selRow >= layout.rowCount) return nullptr;
  const fui::KeyboardRow& row = layout.rows[selRow];
  if (selCol < 0 || selCol >= row.count) return nullptr;
  return &row.keys[selCol];
}

int KeyboardEntryActivity::selectedLogicalIndex() const {
  const fui::KeyboardLayout& layout = currentLayout();
  int index = 0;
  for (int r = 0; r < selRow && r < layout.rowCount; r++) {
    index += layout.rows[r].count;
  }
  return index + selCol;
}

void KeyboardEntryActivity::clampSelection() {
  const fui::KeyboardLayout& layout = currentLayout();
  if (layout.rowCount == 0) {
    selRow = 0;
    selCol = 0;
    return;
  }
  if (selRow < 0) selRow = 0;
  if (selRow >= layout.rowCount) selRow = layout.rowCount - 1;
  const int cols = layout.rows[selRow].count;
  if (selCol < 0) selCol = 0;
  if (selCol >= cols) selCol = cols > 0 ? cols - 1 : 0;
}

void KeyboardEntryActivity::moveSelectionRow(const int delta) {
  const fui::KeyboardLayout& layout = currentLayout();
  if (layout.rowCount == 0) return;
  const int oldCols = selRow < layout.rowCount ? layout.rows[selRow].count : 1;
  selRow = (selRow + delta + layout.rowCount) % layout.rowCount;
  const int newCols = layout.rows[selRow].count;
  // Proportional column mapping keeps vertical travel intuitive between rows
  // of different key counts (e.g. a 10-key letter row over a 6-key bottom row).
  if (oldCols > 0 && newCols > 0 && oldCols != newCols) {
    selCol = selCol * newCols / oldCols;
  }
  clampSelection();
}

void KeyboardEntryActivity::moveSelectionCol(const int delta) {
  const fui::KeyboardLayout& layout = currentLayout();
  if (selRow < 0 || selRow >= layout.rowCount) return;
  const int cols = layout.rows[selRow].count;
  if (cols <= 0) return;
  selCol = (selCol + delta + cols) % cols;
}

bool KeyboardEntryActivity::syncSelectionToValue(const int16_t value) {
  const fui::KeyboardLayout& layout = currentLayout();
  for (int r = 0; r < layout.rowCount; r++) {
    for (int c = 0; c < layout.rows[r].count; c++) {
      if (layout.rows[r].keys[c].value == value) {
        selRow = r;
        selCol = c;
        return true;
      }
    }
  }
  return false;
}

size_t KeyboardEntryActivity::utf8Prev(const std::string& s, size_t pos) {
  if (pos == 0) return 0;
  pos--;
  while (pos > 0 && (static_cast<uint8_t>(s[pos]) & 0xC0) == 0x80) pos--;
  return pos;
}

size_t KeyboardEntryActivity::utf8Next(const std::string& s, size_t pos) {
  if (pos >= s.length()) return s.length();
  pos++;
  while (pos < s.length() && (static_cast<uint8_t>(s[pos]) & 0xC0) == 0x80) pos++;
  return pos;
}

void KeyboardEntryActivity::insertUtf8(const char* out) {
  if (!out || !*out) return;
  if (cursorPos > text.size()) cursorPos = text.size();
  // Text entry is a cold path. Normalize before checking the bounded save
  // field so the final accent/dakuten can compose at the character limit.
  std::string candidate = text;
  candidate.insert(cursorPos, out);
  const bool normalize = inputType == InputType::Text;
  size_t nextCursor = cursorPos + strlen(out);
  if (normalize) {
    const auto prefix = utf8ComposeNfc(candidate.substr(0, nextCursor));
    candidate = utf8ComposeNfc(candidate);
    nextCursor = prefix.size();
  }
  if (maxLength && candidate.size() > maxLength) return;
  text = std::move(candidate);
  cursorPos = std::min(nextCursor, text.size());
}

bool KeyboardEntryActivity::backspaceUtf8() {
  if (text.empty() || cursorPos == 0) return false;
  const size_t prev = utf8Prev(text, cursorPos);
  text.erase(prev, cursorPos - prev);
  cursorPos = prev;
  return true;
}

bool KeyboardEntryActivity::activateValue(const int16_t value, const bool longPress) {
  if (useLanguageKeyboard && !symbols && pinyin.available()) {
    if (value == -101 || value == -102) {
      const bool changed = pinyin.page(value == -101 ? -1 : 1);
      if (changed) languageKeyboard.candidates(pinyin);
      return changed;
    }
    if (value >= 2000 && value < 2000 + PinyinInput::CANDIDATES_PER_PAGE) return commitPinyin(unsigned(value - 2000));
    if ((value == fui::QWERTY_KEY_SPACE || value == fui::QWERTY_KEY_ENTER) && pinyin.spelling()[0])
      return commitPinyin();
    if (value == fui::QWERTY_KEY_BACKSPACE && pinyin.spelling()[0]) {
      if (longPress)
        pinyin.clear();
      else
        pinyin.erase();
      languageKeyboard.candidates(pinyin);
      return true;
    }
  }
  switch (value) {
    case fui::QWERTY_KEY_SHIFT:
      delPressCount = 0;
      hintVisible = false;
      // Letters: case toggle. Symbols: pages between "?123" and "#+=".
      shifted = !shifted;
      clampSelection();
      return true;
    case fui::QWERTY_KEY_MODE:
      delPressCount = 0;
      hintVisible = false;
      if (urlPanel) {
        urlPanel = false;
      } else {
        symbols = !symbols;
        shifted = false;
      }
      clampSelection();
      return true;
    case fui::QWERTY_KEY_LANG: {
      RenderLock lock;
      delPressCount = 0;
      hintVisible = false;
      auto next = keyboard_layouts::nextSelection({layoutId, layoutLanguage, useLanguageKeyboard}, cardLayouts);
      if (next.language == layoutLanguage && next.external == useLanguageKeyboard) return false;
      if (pinyin.spelling()[0]) insertUtf8(pinyin.spelling());
      for (unsigned n = 0; n < getLanguageCount() + keyboard_layouts::COUNT; ++n) {
        pinyin.close();
        languageKeyboard.clear();
        if (!next.external || languageKeyboard.load(next.language)) {
          layoutId = next.builtin;
          layoutLanguage = next.language;
          useLanguageKeyboard = next.external;
          if (useLanguageKeyboard && pinyin.begin(layoutLanguage)) languageKeyboard.candidates(pinyin);
          shifted = symbols = false;
          clampSelection();
          return true;
        }
        cardLayouts &= ~keyboard_layouts::languageBit(next.language);
        next = keyboard_layouts::nextSelection(next, cardLayouts);
      }
      return false;
    }
    case URL_PANEL_KEY:
      delPressCount = 0;
      hintVisible = false;
      urlPanel = !urlPanel;
      symbols = false;
      shifted = false;
      clampSelection();
      return true;
    case fui::QWERTY_KEY_ENTER:
      onComplete(text);
      return false;
    case fui::QWERTY_KEY_BACKSPACE:
      if (longPress) {
        text.clear();
        cursorPos = 0;
        return true;
      }
      delPressCount++;
      if (delPressCount >= 2) {
        hintVisible = true;
        hintShowTime = millis();
      }
      backspaceUtf8();
      return true;
    default: {
      delPressCount = 0;
      hintVisible = false;
      const fui::KeyboardLayout& layer = currentLayout();
      if (longPress) {
        RenderLock lock;
        if (alternates.open(layer, value, layoutLanguage, interactions, confirmHeld, inputType != InputType::Url,
                            inputType != InputType::Url)) {
          touchRouter.reset();
          return true;
        }
      }
      const char* out = fui::keyboardOutputFor(layer, value);
      if (!out) return false;
      if (useLanguageKeyboard && !symbols && pinyin.available() && out[0] && !out[1] &&
          ((out[0] >= 'a' && out[0] <= 'z') || (out[0] >= 'A' && out[0] <= 'Z'))) {
        const bool changed = pinyin.type(out[0]);
        if (changed) languageKeyboard.candidates(pinyin);
        return changed;
      }
      insertUtf8(out);
      if (shifted && !symbols &&
          (!useLanguageKeyboard || (layoutLanguage != Language::JA && layoutLanguage != Language::TH))) {
        shifted = false;  // shift auto-releases after one character
        clampSelection();
      }
      return true;
    }
  }
}

bool KeyboardEntryActivity::commitPinyin(unsigned choice) {
  const char* value = pinyin.candidate(choice);
  if (!*value) return false;
  const size_t before = text.size();
  insertUtf8(value);
  if (before == text.size()) return false;
  pinyin.clear();
  languageKeyboard.candidates(pinyin);
  return true;
}

bool KeyboardEntryActivity::clearAllOrAltOnSelected() {
  const fui::KeyboardKey* key = selectedKey();
  if (!key) return false;
  if (key->value == fui::QWERTY_KEY_BACKSPACE) {
    text.clear();
    cursorPos = 0;
    return true;
  }
  return key->kind == fui::KeyKind::Normal && activateValue(key->value, true);
}

std::string KeyboardEntryActivity::displayTextForCurrentState() const {
  std::string displayText = text;
  if (inputType != InputType::Password || passwordVisible) {
    return displayText;
  }

  size_t revealPos;
  if (cursorMode) {
    revealPos = text.length();  // no reveal in displayText; block draws actual char directly
  } else {
    revealPos = (text.length() > 0 && cursorPos > 0) ? cursorPos - 1 : std::string::npos;
  }
  for (size_t i = 0; i < displayText.length(); i++) {
    if (i != revealPos) {
      displayText[i] = '*';
    }
  }
  return displayText;
}

int KeyboardEntryActivity::measureRange(std::string& s, const int start, const int end) const {
  if (end <= start) return 0;
  // s[end] is writable even at s.length() (the terminator slot); only '\0' may
  // be written there, which is exactly what the measurement needs.
  const char saved = s[end];
  s[end] = '\0';
  const int width = renderer.getTextAdvanceX(UI_12_FONT_ID, s.c_str() + start, EpdFontFamily::REGULAR);
  s[end] = saved;
  return width;
}

bool KeyboardEntryActivity::rangeIsRtl(std::string& s, const int start, const int end) const {
  if (end <= start) return false;
  const char saved = s[end];
  s[end] = '\0';
  const bool isRtl = BidiUtils::detectParagraphLevel(s.c_str() + start, 0, end - start) != 0;
  s[end] = saved;
  return isRtl;
}

int KeyboardEntryActivity::lineBreakEnd(std::string& s, const int start, const int maxWidth) const {
  const int len = static_cast<int>(s.length());
  if (measureRange(s, start, len) <= maxWidth) return len;
  int lo = start + 1;
  int hi = len - 1;
  int best = start + 1;
  while (lo <= hi) {
    const int mid = lo + (hi - lo) / 2;
    if (measureRange(s, start, mid) <= maxWidth) {
      best = mid;
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }

  // The byte-index search can stop inside a character; snap back to a boundary,
  // keeping one whole character so the wrap loop always advances.
  const int firstCharEnd = static_cast<int>(utf8Next(s, static_cast<size_t>(start)));
  while (best > start && (static_cast<uint8_t>(s[best]) & 0xC0) == 0x80) best--;
  // Widths measured mid-character are unreliable, so the search can overshoot.
  while (best > firstCharEnd && measureRange(s, start, best) > maxWidth) {
    best = static_cast<int>(utf8Prev(s, static_cast<size_t>(best)));
  }
  return best < firstCharEnd ? firstCharEnd : best;
}

bool KeyboardEntryActivity::cursorPositionFromPoint(const int x, const int y, size_t& position) const {
  // Key taps are the overwhelmingly common case; they land on the keyboard,
  // never the text field, so skip the wrap/measure work entirely.
  if (y >= keyboardRect().y) return false;

  const auto& metrics = UITheme::getInstance().getMetrics();
  // Text and cursor hit-testing live in the same bezel-safe content box the
  // field underline (drawTextField) and keyboard use; deriving the margin from
  // getScreenWidth() instead offset taps from the glyphs by content.x on
  // inset panels (EEGO A4).
  const Rect content = UITheme::getContentArea(renderer);

  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int inputStartY = inputFieldTop();

  int availableWidth = content.width;
  // Clear the side-button hint gutters, which only render on edge-button boards without touch.
  if (gpio.hasEdgeSideButtons() && !gpio.hasTouch()) {
    availableWidth -= 2 * metrics.sideButtonHintsWidth;
  }
  const int effectiveMargin = (content.width - availableWidth * metrics.keyboardTextFieldWidthPercent / 100) / 2;
  const int textLeft = content.x + effectiveMargin;
  const int toggleGap = inputType == InputType::Password ? 4 : 0;
  const int toggleReserve = inputType == InputType::Password ? std::max(renderer.getTextWidth(UI_12_FONT_ID, "[abc]"),
                                                                        renderer.getTextWidth(UI_12_FONT_ID, "[***]")) +
                                                                   toggleGap
                                                             : 0;
  const int textAreaWidth = content.width - 2 * effectiveMargin - toggleReserve;
  const int maxLineWidth = textAreaWidth;
  const bool centerText = centerInputText || metrics.keyboardCenteredText;
  std::string displayText = displayTextForCurrentState();

  int lineStartIdx = inputWindowStart(displayText, maxLineWidth);
  int lines = 0;
  const int visibleLines = inputFieldLines();
  int lineY = inputStartY;
  int lastLineStartIdx = 0;
  int lastLineEndIdx = static_cast<int>(displayText.length());
  int lastLineStartX = textLeft;
  int lastLineWidth = 0;

  while (true) {
    const int lineEndIdx = lineBreakEnd(displayText, lineStartIdx, maxLineWidth);
    const int textWidth = measureRange(displayText, lineStartIdx, lineEndIdx);
    const int lineStartX = centerText ? textLeft + (maxLineWidth - textWidth) / 2 : textLeft;
    const bool isRtl = rangeIsRtl(displayText, lineStartIdx, lineEndIdx);
    lastLineStartIdx = lineStartIdx;
    lastLineEndIdx = lineEndIdx;
    lastLineStartX = lineStartX;
    lastLineWidth = textWidth;

    if (y >= lineY - metrics.verticalSpacing && y < lineY + lineHeight + metrics.verticalSpacing) {
      if (x <= lineStartX) {
        position = static_cast<size_t>(isRtl ? lineEndIdx : lineStartIdx);
        return true;
      }
      if (x >= lineStartX + textWidth) {
        position = static_cast<size_t>(isRtl ? lineStartIdx : lineEndIdx);
        return true;
      }

      int previousWidth = 0;
      for (int i = lineStartIdx; i < lineEndIdx;) {
        const int next = static_cast<int>(utf8Next(displayText, static_cast<size_t>(i)));
        const int nextWidth = measureRange(displayText, lineStartIdx, next);
        const int halfAdvance = (nextWidth - previousWidth) / 2;
        const int midpoint =
            isRtl ? lineStartX + textWidth - previousWidth - halfAdvance : lineStartX + previousWidth + halfAdvance;
        if ((isRtl && x >= midpoint) || (!isRtl && x < midpoint)) {
          position = static_cast<size_t>(i);
          return true;
        }
        previousWidth = nextWidth;
        i = next;
      }
      position = static_cast<size_t>(lineEndIdx);
      return true;
    }

    if (lineEndIdx == static_cast<int>(displayText.length()) || ++lines >= visibleLines) {
      break;
    }

    lineY += lineHeight;
    lineStartIdx = lineEndIdx;
  }

  const int underlineBottom = lineY + lineHeight + metrics.verticalSpacing + 8;
  if (y >= inputStartY - metrics.verticalSpacing && y < underlineBottom && x >= textLeft &&
      x < textLeft + maxLineWidth + toggleReserve) {
    const bool isRtl = rangeIsRtl(displayText, lastLineStartIdx, lastLineEndIdx);
    const bool insideText = x < lastLineStartX + lastLineWidth;
    position = static_cast<size_t>(insideText == isRtl ? lastLineEndIdx : lastLineStartIdx);
    return true;
  }

  return false;
}

fui::Rect KeyboardEntryActivity::keyboardRect() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect content = UITheme::getContentArea(renderer);  // clear the bezel on rounded panels
  const int pageHeight = renderer.getScreenHeight();
  const int rows = currentLayout().rowCount;
  const bool hasTouch = mappedInput.hasTouch();
  const int height = keyboardKeysHeight(metrics, rows, hasTouch, renderer.getScreenWidth() > pageHeight, content.width);
  const int hintGap = hasTouch ? 0 : BUTTON_KEYBOARD_HINT_GAP;
  const int bottom = hasTouch ? content.y + content.height : pageHeight - metrics.buttonHintsHeight;
  const int y = bottom - height - hintGap;
  return fui::Rect{static_cast<int16_t>(content.x), static_cast<int16_t>(y), static_cast<int16_t>(content.width),
                   static_cast<int16_t>(height)};
}
int KeyboardEntryActivity::inputFieldTop() const {
  if (renderer.getScreenWidth() > renderer.getScreenHeight())
    return pixelui::geometry(toybox::makeTarget(renderer).deviceContext()).body.y + 4;
  const auto& metrics = UITheme::getInstance().getMetrics();
  return metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 5 + metrics.keyboardVerticalOffset;
}

int KeyboardEntryActivity::inputFieldLines() const {
  return keyboardview::visibleLines(keyboardRect().y, inputFieldTop(), renderer.getLineHeight(UI_12_FONT_ID),
                                    UITheme::getInstance().getMetrics().verticalSpacing + 8,
                                    renderer.getScreenWidth() > renderer.getScreenHeight());
}

int KeyboardEntryActivity::inputWindowStart(std::string& displayText, int width) const {
  return keyboardview::firstByte(int(displayText.length()), int(cursorPos), inputFieldLines(),
                                 [&](int start) { return lineBreakEnd(displayText, start, width); });
}

void KeyboardEntryActivity::loop() {
  if (alternates.active() && alternates.capture(mappedInput)) {
    RenderLock lock(RenderLock::Mode::Try);
    if (!lock.ownsLock()) return;
    const auto result = alternates.poll(interactions);
    if (*result.output) {
      insertUtf8(result.output);
      if (!symbols && layoutLanguage != Language::JA && layoutLanguage != Language::TH) shifted = false;
      clampSelection();
    }
    touchRouter.reset();
    confirmHeld = confirmLongHandled = upHeld = downHeld = rightHeld = false;
    if (result.changed) requestUpdate();
    return;
  }
  int tx = 0;
  int ty = 0;

  // The keyboard is published before the panel shows it; until the first paint
  // lands, a tap belongs to whatever screen is still on the glass. Ignore
  // touch, not input: the branches below this one are the physical buttons and
  // they stay live throughout. Nothing is suppressed at the InputManager level
  // either -- suppressTouchContact() would also kill isTouchTapCandidate,
  // isTouchHeldAt and wasSwipe, cancelling holds while the finger is down.
  const bool revealed = revealGate.revealed();

  size_t touchedCursorPos = 0;
  if (revealed && mappedInput.wasScreenTapped(tx, ty) && cursorPositionFromPoint(tx, ty, touchedCursorPos)) {
    haptic_feedback::touchAction();
    cursorPos = std::min(touchedCursorPos, text.length());
    // The masked text field maps taps per byte; snap back to a boundary so
    // the cursor never lands inside a multi-byte character.
    while (cursorPos > 0 && cursorPos < text.length() && (static_cast<uint8_t>(text[cursorPos]) & 0xC0) == 0x80) {
      cursorPos--;
    }
    cursorMode = false;
    togglePos = false;
    hintVisible = false;
    touchRouter.reset();
    requestUpdate();
    return;
  }

  if (!cursorMode && interactionsReady && revealed) {
    const bool pressedDown = mappedInput.wasScreenTouchDown(tx, ty);
    int tapX = 0;
    int tapY = 0;
    const bool tapped = mappedInput.wasScreenTapped(tapX, tapY);
    int hx = 0;
    int hy = 0;
    const bool inContact = mappedInput.isScreenTouchHeld(hx, hy);

    const fui::TouchHoldRouter::Result result =
        touchRouter.update(interactions, pressedDown, static_cast<int16_t>(tx), static_cast<int16_t>(ty), tapped,
                           static_cast<int16_t>(tapX), static_cast<int16_t>(tapY), inContact, millis());
    if (result.event) {
      syncSelectionToValue(result.event.value);
      if (activateValue(result.event.value, result.event.longPress)) {
        haptic_feedback::touchAction(result.event.longPress);
        requestUpdate();
      }
      return;
    }
    // Commit the key before repainting instead of refreshing the press highlight first.
    if (pressedDown || tapped) {
      return;
    }
  }

  if (!cursorMode && mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    upHeld = true;
    upLongHandled = false;
  }

  if (upHeld && !upLongHandled && mappedInput.isPressed(MappedInputManager::Button::Up) &&
      mappedInput.getHeldTime() > LONG_PRESS_MS) {
    cursorMode = true;
    upLongHandled = true;
    hintVisible = true;
    hintShowTime = millis();
    requestUpdate();
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    if (upHeld && !upLongHandled && !cursorMode) {
      moveSelectionRow(-1);
      requestUpdate();
    }
    upHeld = false;
    upLongHandled = false;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    downHeld = true;
    if (cursorMode) {
      togglePos = false;
      cursorMode = false;
      hintVisible = false;
      downLongHandled = true;
      requestUpdate();
    } else {
      downLongHandled = false;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    if (downHeld && !downLongHandled && !cursorMode) {
      moveSelectionRow(1);
      requestUpdate();
    }
    downHeld = false;
    downLongHandled = false;
  }

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Left}, [this] {
    if (cursorMode) return;
    moveSelectionCol(-1);
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    if (cursorMode) {
      if (togglePos) {
        cursorPos = savedCursorPos;
        togglePos = false;
        requestUpdate();
      } else if (cursorPos > 0) {
        cursorPos = utf8Prev(text, cursorPos);
        requestUpdate();
      }
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    if (cursorMode && inputType == InputType::Password && !togglePos) {
      rightHeld = true;
      rightLongHandled = false;
      rightStartCursorPos = cursorPos;
    }
  }

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Right}, [this] {
    if (cursorMode) return;
    moveSelectionCol(1);
    requestUpdate();
  });

  if (rightHeld && !rightLongHandled && mappedInput.isPressed(MappedInputManager::Button::Right) &&
      mappedInput.getHeldTime() > LONG_PRESS_MS) {
    if (cursorMode && inputType == InputType::Password && !togglePos) {
      savedCursorPos = rightStartCursorPos;
      togglePos = true;
      rightLongHandled = true;
      requestUpdate();
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    if (cursorMode && inputType == InputType::Password) {
      rightHeld = false;
      rightLongHandled = false;
    }
    if (cursorMode && !togglePos && cursorPos < text.length()) {
      cursorPos = utf8Next(text, cursorPos);
      requestUpdate();
    }
    if (cursorMode) return;
    rightHeld = false;
    rightLongHandled = false;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    confirmHeld = true;
    confirmLongHandled = false;
  }

  const fui::KeyboardKey* selKey = selectedKey();
  const bool selectedDel = selKey && selKey->value == fui::QWERTY_KEY_BACKSPACE;

  if (confirmHeld && !confirmLongHandled && mappedInput.isPressed(MappedInputManager::Button::Confirm) &&
      mappedInput.getHeldTime() > DEL_LONG_PRESS_MS && selectedDel) {
    clearAllOrAltOnSelected();
    confirmLongHandled = true;
    requestUpdate();
  }

  if (confirmHeld && !confirmLongHandled && mappedInput.isPressed(MappedInputManager::Button::Confirm) &&
      mappedInput.getHeldTime() > LONG_PRESS_MS) {
    if (!selectedDel && clearAllOrAltOnSelected()) {
      requestUpdate();
      confirmLongHandled = true;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (confirmHeld && !confirmLongHandled && !cursorMode) {
      if (selKey && activateValue(selKey->value, false)) {
        requestUpdate();
      }
    } else if (confirmHeld && !confirmLongHandled && cursorMode && inputType == InputType::Password && togglePos) {
      passwordVisible = !passwordVisible;
      requestUpdate();
    }
    confirmHeld = false;
    confirmLongHandled = false;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onCancel();
  }

  if (hintVisible && !cursorMode && millis() - hintShowTime > 4000) {
    hintVisible = false;
    requestUpdate();
  }
}

void KeyboardEntryActivity::render(RenderLock&&) {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const Rect content = UITheme::getContentArea(renderer);  // bezel-safe content bounds
  const auto& metrics = UITheme::getInstance().getMetrics();
  const fui::Rect kbRect = keyboardRect();
  const fui::KeyboardLayout& layout = currentLayout();
  const bool hasTouch = mappedInput.hasTouch();
  const int inputStartY = inputFieldTop();
  const bool stableKeys = hasTouch && kbRect.y > inputStartY && kbRect.y < pageHeight && !useLanguageKeyboard &&
                          !cursorMode && !alternates.active();
  const KeyLayer layer{stableKeys ? &layout : nullptr,
                       renderer.getOrientation(),
                       kbRect.x,
                       kbRect.y,
                       kbRect.width,
                       kbRect.height,
                       layoutLanguage,
                       shifted,
                       symbols,
                       urlPanel};
  const bool fieldOnly = stableKeys && paintedKeys == layer && keysPaintedAt == paintclock::painted();

  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  int inputHeight = 0;

  std::string displayText = displayTextForCurrentState();

  const bool isPassword = (inputType == InputType::Password);
  // Field text/cursor share the bezel-safe content box the underline and
  // keyboard use (matches cursorPositionFromPoint); content.x offsets them past
  // the bezel on inset panels (EEGO A4).
  int availableWidth = content.width;
  // Clear the side-button hint gutters, which only render on edge-button boards without touch.
  if (gpio.hasEdgeSideButtons() && !gpio.hasTouch()) {
    availableWidth -= 2 * metrics.sideButtonHintsWidth;
  }
  const int effectiveMargin = (content.width - availableWidth * metrics.keyboardTextFieldWidthPercent / 100) / 2;
  const int textLeft = content.x + effectiveMargin;
  const int toggleGap = isPassword ? 4 : 0;
  const int toggleReserve = isPassword ? std::max(renderer.getTextWidth(UI_12_FONT_ID, "[abc]"),
                                                  renderer.getTextWidth(UI_12_FONT_ID, "[***]")) +
                                             toggleGap
                                       : 0;
  const int textAreaWidth = content.width - 2 * effectiveMargin - toggleReserve;
  const int maxLineWidth = textAreaWidth;
  const bool centerText = centerInputText || metrics.keyboardCenteredText;
  const int fullTextWidth = measureRange(displayText, 0, static_cast<int>(displayText.size()));
  const bool singleLine = fullTextWidth <= maxLineWidth;
  const int fieldTop = inputStartY - 4;
  const int fieldBottom = inputStartY + lineHeight + metrics.verticalSpacing + 4;
  // Fallback glyphs and combining marks can extend beyond the narrow band.
  const bool narrowField = stableKeys && singleLine && fieldBottom <= kbRect.y &&
                           std::all_of(displayText.begin(), displayText.end(),
                                       [](unsigned char byte) { return byte >= 0x20 && byte <= 0x7e; });
  const bool narrowUpdate = fieldOnly && narrowField && paintedNarrowField;
  if (narrowUpdate) {
    renderer.fillRect(0, fieldTop, pageWidth, fieldBottom - fieldTop, false);
  } else {
    if (fieldOnly) {
      chrome::beginFrame();
      renderer.fillRect(0, 0, pageWidth, kbRect.y, false);
    } else {
      renderer.clearScreen();
    }
    pixelui::drawHeader(renderer, title.c_str(), true);
  }

  const auto clip = renderer.getClipRect();
  if (stableKeys) {
    const int top = narrowUpdate ? std::max(clip[1], fieldTop) : clip[1];
    const int bottom = std::min(clip[1] + clip[3], narrowUpdate ? fieldBottom : int(kbRect.y));
    renderer.setClipRect(clip[0], top, clip[2], bottom - top);
  }

  // The cursor spans a whole code point: a lone byte of it renders as a replacement glyph.
  // Masking is per byte, so displayText keeps text's length and the same span applies to both.
  const size_t cursorCharBytes = (cursorPos < text.length()) ? utf8Next(text, cursorPos) - cursorPos : 0;
  char cursorChar[8] = {};         // the character under the cursor
  char displayCursorChar[8] = {};  // same span of displayText, masked for passwords
  if (cursorCharBytes > 0) {
    const size_t n = std::min(cursorCharBytes, sizeof(cursorChar) - 1);
    memcpy(cursorChar, text.data() + cursorPos, n);
    memcpy(displayCursorChar, displayText.data() + cursorPos, n);
  }

  int cursorCharWidth = 6;
  if (cursorCharBytes > 0) {
    int w = renderer.getTextWidth(UI_12_FONT_ID, cursorChar);
    if (w > cursorCharWidth) cursorCharWidth = w;
  }

  int lineStartIdx = singleLine ? 0 : inputWindowStart(displayText, maxLineWidth);
  const int firstVisible = lineStartIdx;
  int lastVisible = lineStartIdx, drawnLines = 0;
  const int visibleLines = inputFieldLines();
  int textWidth = 0;
  int cursorPixelX = textLeft;
  int cursorLineY = inputStartY;
  bool cursorDrawn = false;

  while (true) {
    const int lineEndIdx =
        singleLine ? static_cast<int>(displayText.size()) : lineBreakEnd(displayText, lineStartIdx, maxLineWidth);
    lastVisible = lineEndIdx;
    const auto fragment = singleLine ? std::string{} : displayText.substr(lineStartIdx, lineEndIdx - lineStartIdx);
    const auto& lineText = singleLine ? displayText : fragment;
    textWidth = singleLine ? fullTextWidth : measureRange(displayText, lineStartIdx, lineEndIdx);
    {
      const bool isRtl = rangeIsRtl(displayText, lineStartIdx, lineEndIdx);
      const int lineStartX = centerText ? textLeft + (maxLineWidth - textWidth) / 2 : textLeft;
      const bool isLastLine = (lineEndIdx == static_cast<int>(displayText.length()));
      bool isCursorLine = false;
      if (!cursorDrawn && cursorPos >= lineStartIdx &&
          (isLastLine ? cursorPos <= lineEndIdx : cursorPos < lineEndIdx)) {
        int beforeWidth = textWidth;
        int throughCursorWidth = textWidth;
        int kernOffset = 0;
        if (cursorPos < static_cast<size_t>(lineEndIdx)) {
          const std::string beforeCursor = isPassword && !passwordVisible && cursorMode
                                               ? std::string(cursorPos - lineStartIdx, '*')
                                               : displayText.substr(lineStartIdx, cursorPos - lineStartIdx);
          beforeWidth = renderer.getTextAdvanceX(UI_12_FONT_ID, beforeCursor.c_str(), EpdFontFamily::REGULAR);
          std::string beforeAndCursor = beforeCursor + displayCursorChar;
          throughCursorWidth = renderer.getTextAdvanceX(UI_12_FONT_ID, beforeAndCursor.c_str(), EpdFontFamily::REGULAR);
          const int charAdvance = renderer.getTextAdvanceX(UI_12_FONT_ID, displayCursorChar, EpdFontFamily::REGULAR);
          kernOffset = throughCursorWidth - beforeWidth - charAdvance;
        }
        if (isRtl) {
          const int logicalWidth = cursorMode && cursorCharBytes > 0 ? throughCursorWidth : beforeWidth;
          cursorPixelX = lineStartX + textWidth - logicalWidth;
        } else {
          cursorPixelX = lineStartX + beforeWidth + kernOffset;
        }
        cursorLineY = inputStartY + inputHeight;
        cursorDrawn = true;
        isCursorLine = true;
      }

      if (isCursorLine && cursorMode && isPassword && !passwordVisible && !togglePos) {
        // Draw text in 3 parts to avoid block cursor overflowing onto next char.
        // displayText uses '*' for all chars; actual char may be wider than '*'.
        // Part 1: chars before cursor position
        const std::string part1 = displayText.substr(lineStartIdx, cursorPos - lineStartIdx);
        renderer.drawText(UI_12_FONT_ID, lineStartX, inputStartY + inputHeight, part1.c_str());
        // Part 2: skip cursor slot (block + actual char drawn later)
        // Part 3: chars after cursor position (skip char under cursor), starting at cursorPixelX + cursorCharWidth
        const int afterStart = static_cast<int>(cursorPos + cursorCharBytes);
        const int afterEnd = lineEndIdx;
        if (afterStart < afterEnd) {
          const std::string part3 = displayText.substr(afterStart, afterEnd - afterStart);
          renderer.drawText(UI_12_FONT_ID, cursorPixelX + cursorCharWidth, inputStartY + inputHeight, part3.c_str());
        }
      } else {
        renderer.drawText(UI_12_FONT_ID, lineStartX, inputStartY + inputHeight, lineText.c_str());
      }
      if (lineEndIdx == static_cast<int>(displayText.length()) || ++drawnLines >= visibleLines) {
        break;
      }

      inputHeight += lineHeight;
      lineStartIdx = lineEndIdx;
    }
  }

  if (renderer.getScreenWidth() > renderer.getScreenHeight()) {
    const int x = content.x + 16;
    const auto cue = [&](int y, int direction) {
      renderer.drawLine(x - 5, y - direction * 3, x, y + direction * 3, 2, true);
      renderer.drawLine(x, y + direction * 3, x + 5, y - direction * 3, 2, true);
    };
    if (firstVisible > 0) cue(inputStartY + 10, -1);
    if (lastVisible < int(displayText.length())) cue(inputStartY + inputHeight + lineHeight - 10, 1);
  }

  const int fieldWidth = (inputHeight > 0) ? maxLineWidth : textWidth;
  const int lineMargin = effectiveMargin;
  GUI.drawTextField(renderer, Rect{content.x, inputStartY, content.width, inputHeight}, fieldWidth, cursorMode,
                    lineMargin, content.width - 2 * lineMargin);

  if (cursorMode && !togglePos && cursorPos <= displayText.length()) {
    static constexpr int blockPadding = 1;
    renderer.fillRect(cursorPixelX - blockPadding, cursorLineY, cursorCharWidth + blockPadding * 2, lineHeight, true);
    if (cursorCharBytes > 0) {
      renderer.drawText(UI_12_FONT_ID, cursorPixelX, cursorLineY, cursorChar, false);
    }
  } else if (cursorPos <= displayText.length()) {
    static constexpr int serifW = 3;
    const int cX = cursorPixelX;
    const int cY = cursorLineY;
    const int cBottom = cursorLineY + lineHeight - 1;
    renderer.fillRect(cX, cY, 2, lineHeight, true);
    renderer.drawLine(cX - serifW, cY, cX - 1, cY, 2, true);
    renderer.drawLine(cX + 1, cY, cX + serifW, cY, 2, true);
    renderer.drawLine(cX - serifW, cBottom, cX - 1, cBottom, 2, true);
    renderer.drawLine(cX + 1, cBottom, cX + serifW, cBottom, 2, true);
  }

  if (isPassword) {
    const char* toggleLabel = passwordVisible ? "[***]" : "[abc]";
    const int toggleWidth = renderer.getTextWidth(UI_12_FONT_ID, toggleLabel);
    const int toggleX = content.x + content.width - effectiveMargin - toggleWidth;
    const int toggleY = inputStartY + inputHeight;
    const bool toggleSelected = cursorMode && togglePos;

    if (toggleSelected) {
      renderer.fillRect(toggleX - 2, toggleY, toggleWidth + 5, lineHeight + 3, true);
      renderer.drawText(UI_12_FONT_ID, toggleX, toggleY, toggleLabel, false);
    } else {
      renderer.drawText(UI_12_FONT_ID, toggleX, toggleY, toggleLabel, true);
    }
  }

  if (hintVisible && cursorMode && !text.empty() && renderer.getScreenWidth() <= renderer.getScreenHeight()) {
    const int hintLh = renderer.getLineHeight(SMALL_FONT_ID);
    const int underlineY = inputStartY + inputHeight + lineHeight + metrics.verticalSpacing;
    const int hintY = underlineY + 4;
    int hintLineY = hintY;
    if (inputType == InputType::Password && togglePos) {
      renderer.drawCenteredText(
          SMALL_FONT_ID, hintLineY,
          passwordVisible ? tr(STR_KB_HINT_TOGGLE_HIDE_PASSWORD) : tr(STR_KB_HINT_TOGGLE_SHOW_PASSWORD), true);
      hintLineY += hintLh;
      renderer.drawCenteredText(SMALL_FONT_ID, hintLineY, tr(STR_KB_HINT_RETURN_CURSOR), true);
    } else {
      renderer.drawCenteredText(SMALL_FONT_ID, hintLineY, tr(STR_KB_HINT_MOVE_CURSOR), true);
      hintLineY += hintLh;
      if (inputType == InputType::Password) {
        const char* passTip = passwordVisible ? tr(STR_KB_HINT_HIDE_PASSWORD) : tr(STR_KB_HINT_SHOW_PASSWORD);
        renderer.drawCenteredText(SMALL_FONT_ID, hintLineY, passTip, true);
      }
    }
  }

  paintedNarrowField = narrowField;
  if (narrowUpdate) {
    renderer.setClipRect(clip[0], clip[1], clip[2], clip[3]);
    renderer.displayWindow(0, fieldTop, pageWidth, fieldBottom - fieldTop);
    keysPaintedAt = paintclock::painted();
    return;
  }
  const int tipsLh = renderer.getLineHeight(SMALL_FONT_ID);
  const int underlineBottom = inputStartY + inputHeight + lineHeight + metrics.verticalSpacing + 4;
  auto drawTip = [&](const char* tip, int y) { renderer.drawCenteredText(SMALL_FONT_ID, y, tip, true); };

  // Reserve the Clear Text row even when the entry is empty.
  const int tipCount = cursorMode ? 1 : urlPanel ? 3 : symbols ? 2 : 3 + (inputType == InputType::Url ? 1 : 0);
  const int tipsHeight = (tipCount + 1) * tipsLh;
  // Reserve cursor instructions even for an empty field; passwords need a second row.
  const int tipsTop = underlineBottom + (cursorMode ? (isPassword ? 2 : 1) * tipsLh : 0);

  if (hasTouch) {
    if (kbRect.y - tipsTop >= 2 * tipsLh) drawTip(tr(STR_KEY_TOUCH_HINT), tipsTop + (kbRect.y - tipsTop - tipsLh) / 2);
  } else if (kbRect.y - tipsTop >= tipsHeight) {
    int y = tipsTop + (kbRect.y - tipsTop - tipsHeight) / 2;
    drawTip(tr(STR_KB_TIPS), y);
    y += tipsLh;
    if (!cursorMode) {
      drawTip(tr(STR_KB_HINT_EDIT_ENTRY), y);
      y += tipsLh;
    }
    if (cursorMode) {
      drawTip(tr(STR_KB_HINT_RETURN_KEYBOARD), y);
    } else if (urlPanel) {
      drawTip(tr(STR_KB_HINT_EXIT_URL_MODE), y);
      y += tipsLh;
      if (!text.empty()) {
        drawTip(tr(STR_KB_HINT_CLEAR_TEXT), y);
      }
    } else if (symbols) {
      if (!text.empty()) {
        drawTip(tr(STR_KB_HINT_CLEAR_TEXT), y);
      }
    } else {
      const char* altCharTip;
      if (inputType == InputType::Url) {
        altCharTip = tr(STR_KB_HINT_SECONDARY_CHAR);
      } else if (shifted) {
        altCharTip = tr(STR_KB_HINT_LOWER_SECONDARY);
      } else {
        altCharTip = tr(STR_KB_HINT_UPPER_SECONDARY);
      }
      drawTip(altCharTip, y);
      y += tipsLh;
      if (inputType == InputType::Url) {
        drawTip(tr(STR_KB_HINT_URL_SNIPPETS), y);
        y += tipsLh;
      }
      if (!text.empty()) {
        drawTip(tr(STR_KB_HINT_CLEAR_TEXT), y);
      }
    }
  }

  if (stableKeys) renderer.setClipRect(clip[0], clip[1], clip[2], clip[3]);
  if (fieldOnly) {
    // The keys and their published hit targets are still valid.
    renderer.displayWindow(0, 0, pageWidth, kbRect.y);
    keysPaintedAt = paintclock::painted();
    return;
  }

  const int keysHeight = keyboardKeysHeight(metrics, layout.rowCount, hasTouch, pageWidth > pageHeight, kbRect.width);
  const fui::Rect keysRect{kbRect.x, static_cast<int16_t>(kbRect.y + (kbRect.height - keysHeight) / 2), kbRect.width,
                           static_cast<int16_t>(keysHeight)};

  // The FreeInkUI keyboard draws the keys and registers their hit rects into
  // `interactions`; loop() (the main task) routes touch snapshots against
  // that table via TouchHoldRouter, which reads the published generation
  // (routePublished()/publishedData()) — beginPublishCycle() here makes this
  // render build into the OTHER generation, so loop() never sees a
  // half-rebuilt table no matter when it runs relative to this.
  interactions.beginPublishCycle();
  fui::GfxRendererTarget target(renderer);
  target.setFont(fui::GfxRendererTarget::FONT_SMALL, SMALL_FONT_ID);
  target.setFont(fui::GfxRendererTarget::FONT_BODY, UI_12_FONT_ID);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  fui::Frame<80> frame(target, device, noInput, interactions);

  fui::KeyboardProps props;
  props.layout = &layout;
  props.keyAction = ACTION_KEY;  // one action id; loop() dispatches on key value
  props.okLabel = tr(STR_OK_BUTTON);
  // Match the label to the layer the mode key leads back from: the symbols
  // layer and the URL snippet panel both label it "abc" in the static tables.
  props.modeLabel = (layer.symbols || (inputType == InputType::Url && layer.urlPanel)) ? tr(STR_KEY_MODE_ABC)
                                                                                       : tr(STR_KEY_MODE_SYMBOLS);
  props.inputMask = static_cast<uint16_t>(fui::InputTouch | fui::InputLongPress);
  props.selectedIndex = (cursorMode || hasTouch) ? -1 : static_cast<int16_t>(selectedLogicalIndex());
  props.labelText.font =
      pageWidth > pageHeight ? fui::GfxRendererTarget::FONT_SMALL : fui::GfxRendererTarget::FONT_BODY;
  props.altText.font = fui::GfxRendererTarget::FONT_SMALL;
  props.controlText.font = props.labelText.font;
  props.gap = props.rowGap = static_cast<int16_t>(keyboardGap(metrics));
  if (layer.urlPanel) props.uniformKeyWidth = false;  // Shortcut rows stretch independently.
  if (!hasTouch) props.background = fui::Paint::none();
  props.background = fui::Paint::solid(fui::Color::White);
  frame.target().fill(kbRect, props.background);
  const int bottomEdge = hasTouch ? pageHeight : pageHeight - metrics.buttonHintsHeight;
  props.bottomHitOverflow = static_cast<int16_t>(std::max(0, bottomEdge - keysRect.bottom()));
  keyboardview::style(props, layer.shifted, layer.symbols || layer.urlPanel, layer.language);
  fui::keyboard(frame, keysRect, props);
  const auto popupBounds = UITheme::getContentArea(renderer);
  alternates.draw(frame, fui::makeRect(popupBounds.x, popupBounds.y, popupBounds.width, popupBounds.height), ACTION_KEY,
                  fui::GfxRendererTarget::FONT_BODY);
  interactions.publish();
  interactionsReady = true;
  // Measures from THIS build rather than the first, so a render that rebuilds
  // before its single paint does not open the gate early. No-op once the first
  // paint has landed.
  revealGate.markBuilt();

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  GUI.drawSideButtonHints(renderer, ">", "<");

  renderer.displayBuffer();
  paintedKeys = layer;
  keysPaintedAt = paintclock::painted();
}

void KeyboardEntryActivity::onComplete(std::string text) {
  if (inputType == InputType::Text) text = utf8ComposeNfc(text);
  setResult(KeyboardResult{std::move(text), passwordVisible});
  finish();
}

void KeyboardEntryActivity::onCancel() {
  ActivityResult result;
  result.isCancelled = true;
  setResult(std::move(result));
  finish();
}
