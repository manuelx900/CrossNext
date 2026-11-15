#include "LanguageKeyboard.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Utf8.h>

#include <cstdio>
#include <cstring>
#include <memory>

#include "PinyinInput.h"

namespace fui = freeink::ui;
namespace {
constexpr size_t HEADER_BYTES = 16, MAX_PAYLOAD = 4096;
constexpr uint8_t MAX_ROWS = 6, MAX_KEYS = 80;
uint32_t read32(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
uint32_t checksum(const uint8_t* p, size_t bytes) {
  uint32_t h = 2166136261u;
  while (bytes--) h = (h ^ *p++) * 16777619u;
  return h;
}
bool validText(const uint8_t* bytes, unsigned count) {
  const auto* cursor = bytes;
  const auto* end = bytes + count;
  while (cursor < end) {
    const auto cp = utf8NextCodepoint(&cursor);
    if (!cp || cp == REPLACEMENT_GLYPH || cp < 0x20 || cursor > end) return false;
  }
  return cursor == end;
}
constexpr fui::KeyboardKey CONTROLS[] = {
    {"?123", nullptr, fui::KeyKind::Mode, fui::StateNormal, fui::QWERTY_KEY_MODE, 2},
    {"ABC", nullptr, fui::KeyKind::Lang, fui::StateNormal, fui::QWERTY_KEY_LANG, 1},
    {"", " ", fui::KeyKind::Space, fui::StateNormal, fui::QWERTY_KEY_SPACE, 5},
    {"OK", nullptr, fui::KeyKind::Ok, fui::StateNormal, fui::QWERTY_KEY_ENTER, 3}};
}  // namespace

struct LanguageKeyboard::Data {
  struct Layer {
    fui::KeyboardKey keys[MAX_KEYS];
    char markLabels[MAX_KEYS][7]{};
    fui::KeyboardRow rows[MAX_ROWS];
    fui::KeyboardLayout layout;
  } layers[2];
  fui::KeyboardKey candidates[9];
  fui::KeyboardRow candidateRows[2][MAX_ROWS];
  fui::KeyboardLayout candidateLayouts[2];
  bool composing = false;
};

void LanguageKeyboard::clear() {
  if (data) std::destroy_at(data);
  data = nullptr;
  storage.reset();
}

bool LanguageKeyboard::load(Language language) {
  clear();
  if (unsigned(language) >= unsigned(Language::_COUNT)) return false;
  char path[80];
  std::snprintf(path, sizeof(path), "/languages/keyboards/%s.cpkbd", LANGUAGE_CODES[unsigned(language)]);
  HalFile file;
  if (!Storage.openFileForRead("KEYBOARD", path, file)) return false;
  uint8_t header[HEADER_BYTES];
  if (file.read(header, sizeof(header)) != sizeof(header) || std::memcmp(header, "CPKB", 4) || header[4] != 1 ||
      (header[5] & ~1) != 0 || header[6] < 1 || header[6] > 4 || header[7] < 1 || header[7] > 4) {
    LOG_ERR("KEYBOARD", "Invalid layout header: %s", path);
    return false;
  }
  const size_t length = read32(header + 8);
  if (length > MAX_PAYLOAD || file.size() != HEADER_BYTES + length) {
    LOG_ERR("KEYBOARD", "Invalid layout size: %s", path);
    return false;
  }
  // Bounded layout descriptors and payload share one activity-owned block. This
  // avoids ~8 KiB on the stack/internal heap and releases it on keyboard exit.
  storage = HalMemory::allocatePsram(sizeof(Data) + length);
  if (!storage) {
    LOG_ERR("KEYBOARD", "OOM: layout %u", unsigned(sizeof(Data) + length));
    return false;
  }
  const auto reject = [this]() {
    LOG_ERR("KEYBOARD", "Invalid layout payload");
    clear();
    return false;
  };
  data = std::construct_at(reinterpret_cast<Data*>(storage.get()));
  auto* payload = storage.get() + sizeof(Data);
  if (file.read(payload, length) != int(length) || checksum(payload, length) != read32(header + 12)) {
    return reject();
  }
  const uint8_t* cursor = payload;
  const uint8_t* end = payload + length;
  for (uint8_t layerIndex = 0; layerIndex < 2; ++layerIndex) {
    auto& layer = data->layers[layerIndex];
    unsigned used = 0;
    const unsigned rows = header[6 + layerIndex];
    for (unsigned row = 0; row < rows; ++row) {
      if (cursor == end) {
        return reject();
      }
      const unsigned count = *cursor++;
      if (!count || count > 12 || used + count + 2 > MAX_KEYS) {
        return reject();
      }
      layer.rows[row] = {layer.keys + used, uint8_t(count), 0};
      for (unsigned col = 0; col < count; ++col) {
        if (cursor == end) {
          return reject();
        }
        const unsigned bytes = *cursor++;
        if (!bytes || bytes > 15 || size_t(end - cursor) <= bytes || cursor[bytes] != 0 ||
            std::memchr(cursor, 0, bytes) || !validText(cursor, bytes)) {
          return reject();
        }
        auto& key = layer.keys[used];
        key.label = key.output = reinterpret_cast<const char*>(cursor);
        const auto* cpCursor = cursor;
        const auto cp = utf8NextCodepoint(&cpCursor);
        if (bytes == 3 && (cp == 0x0e31 || (cp >= 0x0e34 && cp <= 0x0e3a) || (cp >= 0x0e47 && cp <= 0x0e4e))) {
          auto* label = layer.markLabels[used];
          std::memcpy(label, "\xe2\x97\x8c", 3);
          std::memcpy(label + 3, cursor, 3);
          key.label = label;
        }
        key.value = int16_t(1000 + used++);
        cursor += bytes + 1;
        if (header[5] & 1) {
          if (cursor == end) {
            return reject();
          }
          const unsigned altBytes = *cursor++;
          if (altBytes > 15 || size_t(end - cursor) <= altBytes || cursor[altBytes] != 0 ||
              std::memchr(cursor, 0, altBytes) || (altBytes && !validText(cursor, altBytes))) {
            return reject();
          }
          if (altBytes) key.alt = reinterpret_cast<const char*>(cursor);
          cursor += altBytes + 1;
        }
        if (!std::strcmp(key.output, "\xe3\x82\x99")) key.label = "\xe3\x82\x9b";
        if (!std::strcmp(key.output, "\xe3\x82\x9a")) key.label = "\xe3\x82\x9c";
      }
    }
    auto& last = layer.rows[rows - 1];
    const unsigned first = unsigned(last.keys - layer.keys);
    std::memmove(layer.keys + first + 1, layer.keys + first, last.count * sizeof(fui::KeyboardKey));
    layer.keys[first] = {nullptr, nullptr, fui::KeyKind::Shift, fui::StateNormal, fui::QWERTY_KEY_SHIFT};
    layer.keys[used + 1] = {nullptr, nullptr, fui::KeyKind::Delete, fui::StateNormal, fui::QWERTY_KEY_BACKSPACE};
    last.count += 2;
    layer.rows[rows] = {CONTROLS, 4, 0};
    layer.layout = {layer.rows, uint8_t(rows + 1)};
  }
  if (cursor != end) {
    return reject();
  }
  return true;
}

const fui::KeyboardLayout& LanguageKeyboard::layout(bool shifted) const {
  if (data->composing) return data->candidateLayouts[shifted ? 1 : 0];
  return data->layers[shifted ? 1 : 0].layout;
}

void LanguageKeyboard::candidates(const PinyinInput& input) {
  if (!data || !input.available()) return;
  if (data->layers[0].layout.rowCount >= MAX_ROWS || data->layers[1].layout.rowCount >= MAX_ROWS) return;
  auto& keys = data->candidates;
  keys[0] = {input.spelling()[0] ? input.spelling() : tr(STR_KEY_PINYIN),
             nullptr,
             fui::KeyKind::Disabled,
             fui::StateNormal,
             0,
             3,
             false};
  keys[1] = {input.canPage(-1) ? "<" : "", nullptr, fui::KeyKind::Normal, fui::StateNormal, -101, 1, input.canPage(-1)};
  for (unsigned n = 0; n < PinyinInput::CANDIDATES_PER_PAGE; ++n) {
    const char* value = input.candidate(n);
    keys[n + 2] = {value, value, fui::KeyKind::Normal, fui::StateNormal, int16_t(2000 + n), 1, *value != 0};
  }
  keys[8] = {input.canPage(1) ? ">" : "", nullptr, fui::KeyKind::Normal, fui::StateNormal, -102, 1, input.canPage(1)};
  for (unsigned layer = 0; layer < 2; ++layer) {
    const auto& base = data->layers[layer].layout;
    auto* rows = data->candidateRows[layer];
    rows[0] = {keys, 9, 0, true};
    for (unsigned row = 0; row < base.rowCount; ++row) rows[row + 1] = base.rows[row];
    data->candidateLayouts[layer] = {rows, uint8_t(base.rowCount + 1)};
  }
  data->composing = true;
}
