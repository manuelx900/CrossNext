#include "EmbeddedKeyboard.h"

#include <Logging.h>
#include <MappedInputManager.h>
#include <Utf8.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string_view>

#include "../../apps_local/ui/ExtraIcons.h"
#include "../../apps_local/ui/KeyboardAlternates.h"
#include "../../apps_local/ui/KeyboardStyle.h"
#include "../../apps_local/ui/KeyboardSymbols.h"
#include "../../apps_local/ui/PixelMenu.h"
#include "../../apps_local/ui/ToyboxScreen.h"
#include "../../apps_local/ui/UnicodeName.h"
#include "../../apps_local/ui/WordleKeyPaint.h"
#include "KeyboardLayoutSet.h"
#include "LanguageKeyboard.h"
#include "PinyinInput.h"

namespace fui = freeink::ui;
namespace {
constexpr fui::ActionId KEY = 1700;
bool continuesUnit(uint32_t cp) {
  return utf8IsCombiningMark(cp) || (cp >= 0x591 && cp <= 0x5bd) || (cp >= 0x610 && cp <= 0x61a) ||
         (cp >= 0x64b && cp <= 0x65f) || cp == 0x670 || cp == 0xe31 || cp == 0xe33 || (cp >= 0xe34 && cp <= 0xe3a) ||
         (cp >= 0xe47 && cp <= 0xe4e);
}
}  // namespace
struct EmbeddedKeyboard::Data {
  LanguageKeyboard optional;
  PinyinInput pinyin;
  paintclock::RevealedInteractions<80> interactions;
  fui::TouchHoldRouter router;
  keyboardview::Alternates alternates;
  fui::KeyboardLayoutId builtin = fui::KeyboardLayoutId::QwertyEn;
  Language language = Language::EN;
  uint64_t cardLayouts = 0;
  unsigned maxBytes = 96, maxCharacters = 0;
  bool name = false, shifted = false, symbols = false, external = false;
  bool game = false;
  unsigned gameUnits = 6;
  fui::KeyboardKey gameKeys[80];
  fui::KeyboardRow gameRows[6];
  fui::KeyboardLayout gameLayout;
  struct Pending {
    uint32_t revision = 0, candidates = 0;
    int16_t x = 0, y = 0;
    bool tap = false, back = false, next = false, previous = false, confirm = false;
  } pending[8];
  uint8_t first = 0, count = 0;
  uint32_t revision = 0, candidateRevision = 0;
  std::atomic<uint32_t> visibleRevision{0}, visibleCandidates{0};
  std::atomic<bool> painted{false};
  fui::Rect back{}, candidateBox{};
  char text[97]{};
};

bool EmbeddedKeyboard::begin(Language language, unsigned maxBytes, unsigned maxCharacters, bool name,
                             const char* initial) {
  close();
  storage = HalMemory::allocatePsram(sizeof(Data));
  if (!storage) {
    LOG_ERR("KEYBOARD", "OOM: embedded workspace");
    return false;
  }
  data = std::construct_at(reinterpret_cast<Data*>(storage.get()));
  data->language = language;
  data->maxBytes = std::min(96u, maxBytes);
  data->maxCharacters = maxCharacters;
  data->name = name;
  data->builtin = keyboard_layouts::startingLayout();
  bool native = language == Language::OOK;
  if (native) data->builtin = fui::KeyboardLayoutId::QwertyEn;
  for (const auto& item : keyboard_layouts::ALL)
    if (item.language == language) {
      data->builtin = item.id;
      native = true;
    }
  data->cardLayouts = keyboard_layouts::scanCardLayouts();
  data->external =
      !native && (data->cardLayouts & keyboard_layouts::languageBit(language)) && data->optional.load(language);
  if (!native && !data->external) {
    LOG_ERR("KEYBOARD", "Missing language keyboard");
    close();
    return false;
  }
  if (data->external && data->pinyin.begin(language)) data->optional.candidates(data->pinyin);
  if (initial && *initial) insert(initial);
  return true;
}
void EmbeddedKeyboard::close() {
  if (data) std::destroy_at(data);
  data = nullptr;
  storage.reset();
}
const char* EmbeddedKeyboard::text() const { return data ? data->text : ""; }
void EmbeddedKeyboard::clear() {
  if (!data) return;
  data->text[0] = 0;
  data->pinyin.clear();
  ++data->candidateRevision;
  if (data->optional.available()) data->optional.candidates(data->pinyin);
}
const fui::KeyboardLayout& EmbeddedKeyboard::layout() const {
  if (data->symbols) return keyboardview::symbols(data->shifted, true);
  if (data->external) return data->optional.layout(data->shifted);
  return fui::builtinKeyboardLayout(data->builtin, data->shifted, data->symbols, false, true);
}
void EmbeddedKeyboard::wordGame(unsigned units) {
  if (data) {
    data->game = true;
    data->gameUnits = std::clamp(units, 2u, 6u);
  }
}
const fui::KeyboardLayout& EmbeddedKeyboard::gameLayout() const {
  auto& d = *data;
  const auto& source = layout();
  unsigned keys = 0, rows = 0;
  for (unsigned row = 0; row < source.rowCount; ++row) {
    const auto first = keys;
    for (unsigned col = 0; col < source.rows[row].count; ++col) {
      const auto& key = source.rows[row].keys[col];
      const bool candidates = key.value == -101 || key.value == -102 || (key.value >= 2000 && key.value < 2006) ||
                              key.kind == fui::KeyKind::Disabled;
      if (!candidates) {
        if (key.kind != fui::KeyKind::Normal || !key.output || !*key.output) continue;
        const auto* cursor = reinterpret_cast<const unsigned char*>(key.output);
        const auto cp = utf8NextCodepoint(&cursor);
        const bool letter = (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') || (cp >= 0xc0 && cp <= 0x52f) ||
                            (cp >= 0x5d0 && cp <= 0x5ea) || (cp >= 0x620 && cp <= 0x65f) ||
                            (cp >= 0x671 && cp <= 0x6d3) || (cp >= 0xe01 && cp <= 0xe3a) ||
                            (cp >= 0xe40 && cp <= 0xe4e) || (cp >= 0x3041 && cp <= 0x30ff);
        if (!letter) continue;
      }
      if (keys < 76) {
        d.gameKeys[keys] = key;
        // Enter and Delete span two letter keys in every source distribution.
        if (!candidates) d.gameKeys[keys].widthUnits = 1;
        ++keys;
      }
    }
    if (keys > first) d.gameRows[rows++] = {d.gameKeys + first, uint8_t(keys - first), 0};
  }
  if (rows) {
    const bool separate = d.language == Language::TH || d.language == Language::JA || d.language == Language::AR ||
                          d.language == Language::FA;
    if (separate && rows < std::size(d.gameRows)) {
      d.gameKeys[keys] = {nullptr, nullptr, fui::KeyKind::Ok, fui::StateNormal, fui::QWERTY_KEY_ENTER, 2};
      d.gameKeys[keys + 1] = {nullptr, nullptr, fui::KeyKind::Shift, fui::StateNormal, fui::QWERTY_KEY_SHIFT};
      d.gameKeys[keys + 2] = {nullptr, nullptr, fui::KeyKind::Delete, fui::StateNormal, fui::QWERTY_KEY_BACKSPACE, 2};
      d.gameRows[rows++] = {d.gameKeys + keys, 3, 0};
      d.gameLayout = {d.gameRows, uint8_t(rows)};
      return d.gameLayout;
    }
    unsigned controls = rows - 1;
    if (d.gameRows[controls].count > 10 && !d.pinyin.available())
      for (unsigned row = 0; row < rows; ++row)
        if (d.gameRows[row].count < d.gameRows[controls].count) controls = row;
    auto& last = d.gameRows[controls];
    const unsigned first = unsigned(last.keys - d.gameKeys);
    const bool shift = d.language == Language::TH || d.language == Language::JA || d.language == Language::AR ||
                       d.language == Language::FA;
    const unsigned extra = shift ? 2 : 1;
    const unsigned tail = first + last.count;
    std::memmove(d.gameKeys + tail + extra + 1, d.gameKeys + tail, (keys - tail) * sizeof(fui::KeyboardKey));
    std::memmove(d.gameKeys + first + extra, d.gameKeys + first, last.count * sizeof(fui::KeyboardKey));
    for (unsigned row = controls + 1; row < rows; ++row) d.gameRows[row].keys += extra + 1;
    d.gameKeys[first] = {nullptr, nullptr, fui::KeyKind::Ok, fui::StateNormal, fui::QWERTY_KEY_ENTER, 2};
    if (shift) d.gameKeys[first + 1] = {nullptr, nullptr, fui::KeyKind::Shift, fui::StateNormal, fui::QWERTY_KEY_SHIFT};
    last.count += extra;
    d.gameKeys[first + last.count++] = {
        nullptr, nullptr, fui::KeyKind::Delete, fui::StateNormal, fui::QWERTY_KEY_BACKSPACE, 2};
  }
  d.gameLayout = {d.gameRows, uint8_t(rows)};
  return d.gameLayout;
}
unsigned EmbeddedKeyboard::rows() const { return data ? (data->game ? gameLayout().rowCount : layout().rowCount) : 0; }
int EmbeddedKeyboard::preferredHeight() const {
  constexpr int keyHeight = 52;
  const int count = rows();
  if (data && data->game) return count * (count > 4 ? 42 : count > 3 ? 48 : 64) + (count - 1) * 4;
  return count ? count * keyHeight + (count - 1) * keyboardview::RowGap + 2 * keyboardview::PanelPadding : 0;
}
void EmbeddedKeyboard::unit(unsigned column, char out[16]) const {
  out[0] = 0;
  const auto* cursor = reinterpret_cast<const unsigned char*>(text());
  unsigned index = 0;
  while (*cursor) {
    const auto* start = cursor;
    utf8NextCodepoint(&cursor);
    while (*cursor) {
      const auto* next = cursor;
      const auto cp = utf8NextCodepoint(&next);
      const bool combining = continuesUnit(cp);
      if (!combining) break;
      cursor = next;
    }
    if (index++ == column) {
      const auto bytes = std::min<size_t>(15, cursor - start);
      std::memcpy(out, start, bytes);
      out[bytes] = 0;
      return;
    }
  }
}
bool EmbeddedKeyboard::insert(const char* value) {
  if (!value || !*value) return false;
  if (data->name && value[0] == ' ' && (!data->text[0] || data->text[std::strlen(data->text) - 1] == ' ')) return false;
  std::string composed(data->text);
  composed += value;
  composed = utf8ComposeNfc(composed);
  if (composed.size() > data->maxBytes) return false;
  unsigned count = 0;
  if (data->name)
    count = name_units::measure(composed.c_str(), composed.size() + 1).count;
  else
    for (const unsigned char c : composed)
      if ((c & 0xc0) != 0x80) ++count;
  if (data->maxCharacters && count > data->maxCharacters) return false;
  if (data->game) {
    const auto* cursor = reinterpret_cast<const unsigned char*>(composed.c_str());
    unsigned units = 0;
    while (*cursor) {
      const auto cp = utf8NextCodepoint(&cursor);
      if ((cp >= '0' && cp <= '9') || (cp >= 0xe50 && cp <= 0xe59) || (cp >= 0x660 && cp <= 0x669) ||
          (cp >= 0x6f0 && cp <= 0x6f9))
        return false;
      if (!continuesUnit(cp)) ++units;
    }
    if (units > data->gameUnits) return false;
  }
  if (data->name) {
    // A trailing space is useful while entering a two-part name.
    const bool space = !composed.empty() && composed.back() == ' ';
    if (space) composed.pop_back();
    if (!unicode_name::valid(composed.c_str(), 97, data->maxCharacters, false)) return false;
    if (space) composed += ' ';
  }
  std::memcpy(data->text, composed.c_str(), composed.size() + 1);
  return true;
}
EmbeddedKeyboard::Event EmbeddedKeyboard::activate(int value, bool held) {
  const bool shifted = data->shifted, symbols = data->symbols, external = data->external;
  const auto builtin = data->builtin;
  const auto language = data->language;
  const auto result = edit(value, held);
  if (result == Event::Changed) {
    if (shifted != data->shifted || symbols != data->symbols || external != data->external ||
        builtin != data->builtin || language != data->language)
      ++data->revision;
    if (data->pinyin.available()) ++data->candidateRevision;
  }
  return result;
}
EmbeddedKeyboard::Event EmbeddedKeyboard::edit(int value, bool held) {
  auto& d = *data;
  if (d.external && !d.symbols && d.pinyin.available()) {
    if (value == -101 || value == -102) {
      const bool changed = d.pinyin.page(value == -101 ? -1 : 1);
      if (changed) d.optional.candidates(d.pinyin);
      return changed ? Event::Changed : Event::Consumed;
    }
    if ((value >= 2000 && value < 2006) ||
        ((value == fui::QWERTY_KEY_ENTER || value == fui::QWERTY_KEY_SPACE) && *d.pinyin.spelling())) {
      const unsigned choice = value >= 2000 ? unsigned(value - 2000) : 0;
      if (!insert(d.pinyin.candidate(choice))) return Event::Consumed;
      d.pinyin.clear();
      d.optional.candidates(d.pinyin);
      return Event::Changed;
    }
    if (value == fui::QWERTY_KEY_BACKSPACE && *d.pinyin.spelling()) {
      if (held)
        d.pinyin.clear();
      else
        d.pinyin.erase();
      d.optional.candidates(d.pinyin);
      return Event::Changed;
    }
  }
  switch (value) {
    case fui::QWERTY_KEY_ENTER:
      return Event::Submit;
    case fui::QWERTY_KEY_SHIFT:
      d.shifted = !d.shifted;
      return Event::Changed;
    case fui::QWERTY_KEY_MODE:
      d.symbols = !d.symbols;
      d.shifted = false;
      return Event::Changed;
    case fui::QWERTY_KEY_LANG: {
      if (d.game) return Event::Consumed;
      auto next = keyboard_layouts::nextSelection({d.builtin, d.language, d.external}, d.cardLayouts);
      if (next.language == d.language && next.external == d.external) return Event::Consumed;
      if (d.pinyin.spelling()[0]) insert(d.pinyin.spelling());
      for (unsigned n = 0; n < getLanguageCount() + keyboard_layouts::COUNT; ++n) {
        d.pinyin.close();
        d.optional.clear();
        if (!next.external || d.optional.load(next.language)) {
          d.builtin = next.builtin;
          d.language = next.language;
          d.external = next.external;
          d.shifted = d.symbols = false;
          if (d.external && d.pinyin.begin(d.language)) d.optional.candidates(d.pinyin);
          return Event::Changed;
        }
        d.cardLayouts &= ~keyboard_layouts::languageBit(next.language);
        next = keyboard_layouts::nextSelection(next, d.cardLayouts);
      }
      return Event::Consumed;
    }
    case fui::QWERTY_KEY_BACKSPACE: {
      size_t end = std::strlen(d.text);
      if (!end) return Event::Consumed;
      if (held)
        end = 0;
      else
        do {
          --end;
        } while (end && (uint8_t(d.text[end]) & 0xc0) == 0x80);
      if (d.name && !held) end = name_units::measure(d.text, sizeof(d.text)).last;
      d.text[end] = 0;
      return Event::Changed;
    }
    default: {
      if (held && d.alternates.open(layout(), value, d.language,
                                    static_cast<paintclock::RevealedInteractions<80>::Buffer&>(d.interactions))) {
        d.router.reset();
        d.first = d.count = 0;
        return Event::Changed;
      }
      const char* out = fui::keyboardOutputFor(layout(), value);
      if (!out) return Event::Consumed;
      if (d.external && !d.symbols && d.pinyin.available() && out[0] && !out[1] &&
          ((out[0] >= 'a' && out[0] <= 'z') || (out[0] >= 'A' && out[0] <= 'Z'))) {
        if (!d.pinyin.type(out[0])) return Event::Consumed;
        d.optional.candidates(d.pinyin);
        return Event::Changed;
      }
      if (!insert(out)) return Event::Consumed;
      if (!d.symbols && d.language != Language::JA && d.language != Language::TH) d.shifted = false;
      return Event::Changed;
    }
  }
}
void EmbeddedKeyboard::capture(const MappedInputManager& input) {
  if (!data) return;
  if (data->alternates.active() && data->alternates.capture(input)) return;
  Data::Pending event;
  int x = 0, y = 0;
  event.tap = input.wasScreenTapped(x, y);
  event.x = x;
  event.y = y;
  event.back = input.wasReleased(MappedInputManager::Button::Back);
  event.next = input.wasReleased(MappedInputManager::Button::NavNext);
  event.previous = input.wasReleased(MappedInputManager::Button::NavPrevious);
  event.confirm = input.wasReleased(MappedInputManager::Button::Confirm);
  if (!event.back && !data->painted.load(std::memory_order_acquire)) return;
  if (event.back) data->first = data->count = 0;
  if (!event.tap && !event.back && !event.next && !event.previous && !event.confirm) return;
  event.revision = data->visibleRevision.load(std::memory_order_acquire);
  event.candidates = data->visibleCandidates.load(std::memory_order_acquire);
  if (data->count == 8) {
    LOG_ERR("KEYBOARD", "Input queue full");
    return;
  }
  data->pending[(data->first + data->count++) % 8] = event;
}
void EmbeddedKeyboard::didPaint() {
  if (!data) return;
  data->visibleRevision.store(data->revision, std::memory_order_release);
  data->visibleCandidates.store(data->candidateRevision, std::memory_order_release);
  data->painted.store(true, std::memory_order_release);
}
EmbeddedKeyboard::Event EmbeddedKeyboard::poll(const MappedInputManager& input, bool captureNow) {
  if (!data) return Event::None;
  if (captureNow) capture(input);
  if (data->alternates.active()) {
    const auto result =
        data->alternates.poll(static_cast<paintclock::RevealedInteractions<80>::Buffer&>(data->interactions));
    if (*result.output && insert(result.output)) {
      if (!data->symbols && data->language != Language::JA && data->language != Language::TH && data->shifted) {
        data->shifted = false;
        ++data->revision;
      }
    }
    return result.changed ? Event::Changed : Event::Consumed;
  }
  if (!data->interactions.publishedRoutable()) return Event::None;
  Data::Pending event;
  const bool queued = data->count != 0;
  if (queued) {
    event = data->pending[data->first];
    data->first = (data->first + 1) % 8;
    --data->count;
  }
  if (event.back || (event.tap && data->back.contains(event.x, event.y))) return Event::Cancel;
  if (queued && (event.revision != data->revision ||
                 (event.tap && data->pinyin.available() && data->candidateBox.contains(event.x, event.y) &&
                  event.candidates != data->candidateRevision)))
    return Event::Consumed;
  if (event.next || event.previous || event.confirm) {
    fui::InputSnapshot buttons;
    buttons.focusNext = event.next;
    buttons.focusPrev = event.previous;
    buttons.confirm = event.confirm;
    const auto before = data->interactions.focusPosition();
    const auto routed = data->interactions.routePublished(buttons);
    if (routed.action == KEY) return activate(routed.value, false);
    return before != data->interactions.focusPosition() ? Event::Changed : Event::Consumed;
  }
  int x = 0, y = 0, hx = 0, hy = 0;
  const bool down = input.wasScreenTouchDown(x, y), held = input.isScreenTouchHeld(hx, hy);
  const auto routed = data->router.update(
      static_cast<paintclock::RevealedInteractions<80>::Buffer&>(data->interactions), down || held,
      int16_t(down ? x : hx), int16_t(down ? y : hy), event.tap, event.x, event.y, down || held, millis());
  if (!routed.event) return event.tap ? Event::Consumed : Event::None;
  const auto value = routed.event.value;
  return activate(value, routed.event.longPress);
}
void EmbeddedKeyboard::draw(fui::DrawTarget& target, const fui::DeviceContext& device, fui::Rect box) {
  if (!data) return;
  const int width = std::min(720, int(box.width));
  box.x += (box.width - width) / 2;
  box.width = width;
  const int height = std::min(int(box.height), preferredHeight());
  box.y += box.height - height;
  box.height = height;
  drawKeys(target, device, box);
}
const fui::KeyboardLayout& EmbeddedKeyboard::nameLayout() const {
  const auto& source = layout();
  unsigned keys = 0, rows = 0;
  for (unsigned row = 0; row < source.rowCount; ++row) {
    if (rows == std::size(data->gameRows)) return source;
    const unsigned first = keys;
    for (unsigned col = 0; col < source.rows[row].count; ++col) {
      const auto& key = source.rows[row].keys[col];
      if (key.kind == fui::KeyKind::Ok) continue;
      if (keys == std::size(data->gameKeys)) return source;
      data->gameKeys[keys++] = key;
    }
    if (keys > first) {
      data->gameRows[rows] = source.rows[row];
      data->gameRows[rows].keys = data->gameKeys + first;
      data->gameRows[rows++].count = uint8_t(keys - first);
    }
  }
  data->gameLayout = {data->gameRows, uint8_t(rows)};
  return data->gameLayout;
}
void EmbeddedKeyboard::drawKeys(fui::DrawTarget& target, const fui::DeviceContext& device, fui::Rect box,
                                const NamePresentation* name, fui::Rect submit) {
  data->candidateBox =
      data->pinyin.available() ? fui::makeRect(box.x, box.y, box.width, (box.height - 12) / rows()) : fui::Rect{};
  const fui::InputSnapshot empty{};
  data->interactions.beginBuild();
  data->interactions.beginPublishCycle();
  fui::Frame<80> frame(target, device, empty, data->interactions);
  fui::KeyboardProps props;
  props.layout = name ? &nameLayout() : &layout();
  props.keyAction = KEY;
  props.inputMask = fui::InputDefault | fui::InputLongPress;
  props.labelText.font = 4;
  props.altText.font = 3;
  props.controlText.font = 3;
  const int rowHeight = (box.height - 12 - (rows() - 1) * 6) / rows();
  if (target.lineHeight(4) + target.lineHeight(3) + 5 > rowHeight) props.labelText.font = 3;
  props.altLabelGap = 1;
  props.altHintRightPadding = 2;
  props.okLabel = tr(STR_OK_BUTTON);
  props.gap = 4;
  props.rowGap = 6;
  props.padding = {6, 6, 6, 6};
  props.keyRadius = chrome::lightHeader ? 4 : 0;
  props.background = fui::Paint::solid(fui::Color::White);
  props.selectedIndex = -1;
  if (name) {
    props.labelText.font = 0;
    if (target.lineHeight(0) + target.lineHeight(3) + 5 > rowHeight) props.labelText.font = 3;
    props.controlText.font = 3;
    props.keyRadius = 0;
    props.padding = {2, 0, 2, 0};
    props.spaceLabel = tr(STR_PET_NAME_SPACE);
    props.keyStyles = fui::defaultKeyStyles();
    props.keyStyles.normal.borderWidth = props.keyStyles.selected.borderWidth = props.keyStyles.focused.borderWidth =
        props.keyStyles.active.borderWidth = props.keyStyles.disabled.borderWidth = 2;
  }
  keyboardview::style(props, data->shifted, data->symbols, data->language);
  if (name) props.okLabel = name->confirmLabel;
  fui::keyboard(frame, box, props);
  if (name) {
    fui::ButtonProps confirm;
    confirm.label = name->confirmLabel;
    confirm.action = KEY;
    confirm.value = fui::QWERTY_KEY_ENTER;
    confirm.enabled = name->enabled && (data->text[0] || *data->pinyin.spelling());
    confirm.icon = fui::bitmapFromIcon(extra_check_32);
    confirm.iconSize = 26;
    confirm.gap = 18;
    confirm.text.font = 0;
    confirm.text.bold = true;
    confirm.radius = 12;
    confirm.styles = fui::defaultKeyStyles();
    confirm.styles.normal.borderWidth = confirm.styles.selected.borderWidth = confirm.styles.focused.borderWidth =
        confirm.styles.active.borderWidth = confirm.styles.disabled.borderWidth = 2;
    target.fill(fui::makeRect(submit.x, submit.y + 3, submit.width, submit.height),
                fui::Paint::solid(fui::Color::Black), 12);
    fui::button(frame, submit, confirm);
  }
  data->alternates.draw(frame, pixelui::geometry(device).body, KEY, 4);
  data->interactions.publish();
  if (!data->alternates.active()) data->interactions.paintButtonFocus(target);
}
void EmbeddedKeyboard::drawGame(fui::DrawTarget& target, const fui::DeviceContext& device, fui::Rect box,
                                const void* context, wordlekeys::Tone (*keyTone)(const void*, const char*)) {
  if (!data) return;
  data->back = pixelui::geometry(device).back;
  const auto& keys = gameLayout();
  if (!keys.rowCount) return;
  data->interactions.beginBuild();
  data->interactions.beginPublishCycle();
  const fui::InputSnapshot empty{};
  fui::Frame<80> frame(target, device, empty, data->interactions);
  const auto ink = fui::Paint::solid(fui::Color::Black);
  constexpr int gap = 4;
  bool alternates = false;
  for (unsigned row = 0; row < keys.rowCount; ++row) {
    for (unsigned col = 0; col < keys.rows[row].count; ++col) {
      const auto& key = keys.rows[row].keys[col];
      alternates |= key.alt && *key.alt && key.kind == fui::KeyKind::Normal;
    }
  }
  const auto fitUnit = [&] {
    int unit = box.width;
    for (unsigned row = 0; row < keys.rowCount; ++row) {
      unsigned units = 0;
      for (unsigned col = 0; col < keys.rows[row].count; ++col) units += keys.rows[row].keys[col].widthUnits;
      unit = std::min(unit, (box.width - int(keys.rows[row].count - 1) * gap) / int(units));
    }
    return unit;
  };
  fui::TextStyle enterStyle;
  enterStyle.font = 3;
  const int enterWidth = target.measureText(3, tr(STR_WORDLE_OK), enterStyle).width + 4;
  int unit = 0;
  for (unsigned span = 2; span <= 4; ++span) {
    for (unsigned row = 0; row < keys.rowCount; ++row)
      for (unsigned col = 0; col < keys.rows[row].count; ++col) {
        const auto* key = keys.rows[row].keys + col;
        if (key->kind == fui::KeyKind::Ok) data->gameKeys[key - data->gameKeys].widthUnits = span;
      }
    unit = fitUnit();
    if (int(span) * unit >= enterWidth) break;
  }
  const int height = (box.height - (keys.rowCount - 1) * gap) / keys.rowCount;
  data->candidateBox = data->pinyin.available() ? fui::makeRect(box.x, box.y, box.width, height) : fui::Rect{};
  for (unsigned row = 0; row < keys.rowCount; ++row) {
    const auto& r = keys.rows[row];
    int total = (r.count - 1) * gap;
    for (unsigned col = 0; col < r.count; ++col) total += r.keys[col].widthUnits * unit;
    int x = box.x + (box.width - total) / 2;
    for (unsigned col = 0; col < r.count; ++col) {
      const auto& k = r.keys[col];
      const auto rect = fui::makeRect(x, box.y + row * (height + gap), k.widthUnits * unit, height);
      x = rect.right() + gap;
      const bool enter = k.kind == fui::KeyKind::Ok;
      const auto tone = k.output && *k.output && keyTone ? keyTone(context, k.output) : wordlekeys::Tone::Normal;
      const bool black = wordlekeys::paintKey(target, rect, tone, enter);
      fui::TextStyle style;
      style.font = pixelui::kBody;
      style.align = fui::TextAlign::Center;
      style.color = black ? fui::Color::White : fui::Color::Black;
      if (k.kind == fui::KeyKind::Delete) {
        wordlekeys::paintErase(target, rect);
      } else if (k.kind == fui::KeyKind::Shift) {
        const int cx = rect.x + rect.width / 2, cy = rect.y + rect.height / 2;
        target.line({int16_t(cx - 8), int16_t(cy)}, {int16_t(cx), int16_t(cy - 9)}, 2, ink);
        target.line({int16_t(cx), int16_t(cy - 9)}, {int16_t(cx + 8), int16_t(cy)}, 2, ink);
        target.line({int16_t(cx), int16_t(cy - 8)}, {int16_t(cx), int16_t(cy + 9)}, 2, ink);
      } else {
        char label[16]{};
        const char* text = enter ? tr(STR_WORDLE_OK) : k.label;
        if (text && k.kind == fui::KeyKind::Normal && k.value >= 0 && k.value < 2000) {
          keyboardview::upperLabel(text, data->language, label, sizeof(label));
          text = label;
        }
        if (enter) style.font = pixelui::kSmall;
        auto labelBox = rect;
        if (alternates && k.kind == fui::KeyKind::Normal) {
          labelBox.y += 14;
          labelBox.height -= 14;
        }
        if (text)
          pixelui::text(target, labelBox.inset(fui::Insets{0, 2, 0, 2}), text, style.font, style.color,
                        fui::TextAlign::Center);
        if (tone == wordlekeys::Tone::Absent) wordlekeys::fadeAbsent(target, rect);
        if (k.alt && *k.alt && k.kind == fui::KeyKind::Normal) {
          fui::TextStyle alternate;
          alternate.font = 3;
          alternate.align = fui::TextAlign::Right;
          alternate.color = style.color;
          const auto alternateBox = fui::makeRect(rect.right() - 18, rect.y + 2, 16, 16);
          char alternateLabel[16];
          keyboardview::upperLabel(k.alt, data->language, alternateLabel, sizeof(alternateLabel));
          target.text(alternateBox, alternateLabel, alternate);
          if (keyTone && keyTone(context, k.alt) == wordlekeys::Tone::Absent) {
            const auto background = black                               ? ink
                                    : tone == wordlekeys::Tone::Present ? fui::Paint::dither(fui::Color::LightGray)
                                                                        : fui::Paint::solid(fui::Color::White);
            wordlekeys::fadeAbsent(target, alternateBox, background);
          }
        }
      }
      if (k.enabled && k.kind != fui::KeyKind::Disabled)
        frame.hit(rect, KEY, k.value, fui::InputDefault | fui::InputLongPress);
    }
  }
  data->alternates.draw(frame, pixelui::geometry(device).body, KEY, pixelui::kBody, true);
  data->interactions.publish();
  if (!data->alternates.active()) data->interactions.paintButtonFocus(target);
}
void EmbeddedKeyboard::drawName(toybox::Screen& screen, const char* title, const NamePresentation& name) {
  if (!data) return;
  pixelui::begin(screen);
  pixelui::Header header;
  header.title = title;
  header.back = true;
  header.clockText = screen.headerStatus().clock;
  pixelui::header(screen.target(), screen.device(), header);
  chrome::framebufferLightHeader = false;
  data->back = pixelui::geometry(screen.device()).back;
  backui::publish(true, data->back);
  const auto body = screen.body();
  const bool wide = body.width > body.height;
  constexpr int gap = 14, fieldHeight = 90, footerHeight = 54;
  const int introHeight = screen.target().lineHeight(0) + screen.target().lineHeight(4) * 2 + 18;
  const int leftWidth = wide ? body.width * 43 / 100 : body.width;
  const auto left = fui::makeRect(body.x, body.y, leftWidth, body.height);
  const auto controls =
      wide ? fui::makeRect(left.right() + gap, body.y, body.width - leftWidth - gap, body.height) : body;
  const auto submit = fui::makeRect(controls.x, body.bottom() - footerHeight - 3, controls.width, footerHeight);
  const int count = rows();
  const int desired = count * 60 + (count - 1) * 6 + 4;
  const int available =
      wide ? submit.y - controls.y - gap : body.height - introHeight - 96 - fieldHeight - footerHeight - gap * 4;
  const int keysHeight = std::max(1, std::min(desired, available));
  const auto keyBox = fui::makeRect(controls.x, submit.y - gap - keysHeight, controls.width, keysHeight);
  const auto field =
      fui::makeRect(left.x, (wide ? body.bottom() : keyBox.y - gap) - fieldHeight, left.width, fieldHeight);
  const auto card =
      fui::makeRect(left.x, left.y + introHeight, left.width, std::max(1, field.y - gap - left.y - introHeight));
  auto& target = screen.target();
  const auto ink = fui::Paint::solid(fui::Color::Black);
  fui::TextStyle label;
  label.font = 0;
  label.bold = true;
  pixelui::text(target, fui::makeRect(left.x + 6, left.y, left.width - 12, target.lineHeight(0) + 4), name.prompt, 0,
                fui::Color::Black, fui::TextAlign::Left, true);
  const int subtitleTop = left.y + target.lineHeight(0) + 8;
  label.font = 4;
  label.bold = false;
  label.maxLines = 2;
  target.text(fui::makeRect(left.x + 6, subtitleTop, left.width - 12, target.lineHeight(4) * 2), name.subtitle, label);
  target.stroke(card, ink, 2, 12);
  const int imageWidth = std::min(128, int(card.width) / 3);
  const auto image = fui::makeRect(card.right() - imageWidth - 12, card.y + 10, imageWidth, card.height - 20);
  const auto copy = fui::makeRect(card.x + 16, card.y + 12, card.width - imageWidth - 40, card.height - 24);
  if (name.drawArt) name.drawArt(name.artContext, target, image, name.artId, false);
  label.maxLines = 1;
  label.bold = true;
  pixelui::text(target, fui::makeRect(copy.x, copy.y, copy.width, 30), name.cardTitle, 0, fui::Color::Black,
                fui::TextAlign::Left, true);
  label.bold = false;
  label.maxLines = 3;
  target.text(fui::makeRect(copy.x, copy.y + 38, copy.width, std::max(0, copy.height - 38)), name.cardText, label);
  target.stroke(field, ink, 2, 12);
  label.font = 3;
  label.maxLines = 1;
  target.text(fui::makeRect(field.x + 16, field.y + 8, field.width - 120, 24), name.fieldLabel, label);
  const unsigned length = name_units::measure(text(), sizeof(data->text)).count;
  char countLabel[24];
  std::snprintf(countLabel, sizeof(countLabel), "%u / %u", length, data->maxCharacters);
  label.align = fui::TextAlign::Right;
  target.text(fui::makeRect(field.right() - 100, field.y + 8, 84, 24), countLabel, label);
  label.font = 1;
  label.align = fui::TextAlign::Center;
  label.bold = true;
  target.text(fui::makeRect(field.x + 16, field.y + 34, field.width - 32, 46), text()[0] ? text() : name.placeholder,
              label);
  drawKeys(target, screen.device(), keyBox, &name, submit);
}
