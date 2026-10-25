#include "HomeMenuCore.h"

#include <cstring>

namespace homemenu {
namespace {
bool valid(Id id) { return static_cast<int>(id) < kCount; }
bool eligible(Id id, bool services) { return id != Id::Services || services; }
uint32_t read32(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
void write32(uint8_t* p, uint32_t value) {
  for (int n = 0; n < 4; ++n) p[n] = static_cast<uint8_t>(value >> (n * 8));
}
bool utf8(const char* s) {
  while (*s) {
    const auto c = static_cast<uint8_t>(*s++);
    if (c < 32 || c == 127) return false;
    if (c < 128) continue;
    int more = c >= 0xc2 && c <= 0xdf ? 1 : c >= 0xe0 && c <= 0xef ? 2 : c >= 0xf0 && c <= 0xf4 ? 3 : -1;
    if (more < 0) return false;
    const auto first = static_cast<uint8_t>(*s);
    if ((c == 0xe0 && first < 0xa0) || (c == 0xed && first >= 0xa0) || (c == 0xf0 && first < 0x90) ||
        (c == 0xf4 && first >= 0x90))
      return false;
    while (more--)
      if ((static_cast<uint8_t>(*s++) & 0xc0) != 0x80) return false;
  }
  return true;
}
}  // namespace
bool isShortcut(Id id) { return static_cast<int>(id) >= kBaseCount && static_cast<int>(id) < kCount; }
const Shortcut* shortcut(const Config& config, Id id) {
  return isShortcut(id) ? &config.shortcuts[static_cast<int>(id) - kBaseCount] : nullptr;
}
Id findShortcut(const Config& config, int folder, const char* title) {
  if (!title || !*title) return Id::Count;
  for (int n = 0; n < kShortcutCount; ++n)
    if (config.shortcuts[n].folder == folder && !std::strcmp(config.shortcuts[n].title, title))
      return static_cast<Id>(kBaseCount + n);
  return Id::Count;
}
void removeShortcut(Config& config, Id id) {
  if (!isShortcut(id)) return;
  setVisible(config, id, false);
  config.shortcuts[static_cast<int>(id) - kBaseCount] = {};
  config.labels[static_cast<int>(id)][0] = 0;
}
Id addShortcut(Config& config, int folder, const char* title) {
  if (folder < 0 || folder > 1 || !title || !*title || std::strlen(title) >= kItemBytes || !utf8(title))
    return Id::Count;
  Id id = findShortcut(config, folder, title);
  if (id != Id::Count && (config.visible & (1u << static_cast<int>(id)))) return id;
  if (id == Id::Count) {
    for (int n = 0; n < kShortcutCount; ++n)
      if (!config.shortcuts[n].title[0]) {
        id = static_cast<Id>(kBaseCount + n);
        break;
      }
  }
  if (id == Id::Count || visibleCount(config) >= kMaxVisible) return Id::Count;
  auto& target = config.shortcuts[static_cast<int>(id) - kBaseCount];
  target.folder = static_cast<uint8_t>(folder);
  std::strcpy(target.title, title);
  setVisible(config, id, true);
  return id;
}
int visibleCount(const Config& config, bool services) {
  int count = 0;
  for (int n = 0; n < kCount; ++n)
    if ((config.visible & (1u << n)) && eligible(static_cast<Id>(n), services)) ++count;
  return count;
}
Id at(const Config& config, int index, bool services) {
  for (const auto id : config.order) {
    if (valid(id) && (config.visible & (1u << static_cast<int>(id))) && eligible(id, services) && index-- == 0)
      return id;
  }
  return Id::Count;
}
int indexOf(const Config& config, Id id, bool services) {
  for (int n = 0; n < visibleCount(config, services); ++n)
    if (at(config, n, services) == id) return n;
  return -1;
}
bool move(Config& config, Id id, int direction, bool services) {
  if (direction != -1 && direction != 1) return false;
  const int index = indexOf(config, id, services);
  if (index < 0 || index + direction < 0 || index + direction >= visibleCount(config, services)) return false;
  const auto other = at(config, index + direction, services);
  int from = -1, to = -1;
  for (int n = 0; n < kCount; ++n) {
    if (config.order[n] == id) from = n;
    if (config.order[n] == other) to = n;
  }
  if (from < 0 || to < 0) return false;
  config.order[from] = other;
  config.order[to] = id;
  return true;
}
bool setVisible(Config& config, Id id, bool shown) {
  if (!valid(id) || (id == Id::Settings && !shown) || (shown && isShortcut(id) && !shortcut(config, id)->title[0]))
    return false;
  const auto bit = static_cast<uint16_t>(1u << static_cast<int>(id));
  if (shown && !(config.visible & bit) && visibleCount(config) >= kMaxVisible) return false;
  if (shown)
    config.visible |= bit;
  else
    config.visible &= ~bit;
  return true;
}
bool setLabel(Config& config, Id id, const char* label) {
  if (!valid(id) || !label || std::strlen(label) >= kLabelBytes || !utf8(label)) return false;
  while (*label == ' ') ++label;
  size_t length = std::strlen(label);
  while (length && label[length - 1] == ' ') --length;
  auto* out = config.labels[static_cast<int>(id)];
  std::memset(out, 0, kLabelBytes);
  std::memcpy(out, label, length);
  return true;
}
uint32_t checksum(const uint8_t* data, size_t length) {
  uint32_t value = 2166136261u;
  while (length--) value = (value ^ *data++) * 16777619u;
  return value;
}
void encode(const Config& config, uint32_t generation, uint8_t* bytes) {
  std::memset(bytes, 0, kSaveBytes);
  std::memcpy(bytes, "HMC3", 4);
  write32(bytes + 4, generation);
  write32(bytes + 8, config.visible);
  std::memcpy(bytes + 12, config.labels, sizeof(config.labels));
  auto* out = bytes + 12 + sizeof(config.labels);
  for (const auto& entry : config.shortcuts) {
    *out++ = entry.folder;
    std::memcpy(out, entry.title, kItemBytes);
    out += kItemBytes;
  }
  static_assert(12 + sizeof(config.labels) + kShortcutCount * (kItemBytes + 1) + sizeof(config.order) <=
                kSaveBytes - 4);
  std::memcpy(out, config.order, sizeof(config.order));
  write32(bytes + kSaveBytes - 4, checksum(bytes, kSaveBytes - 4));
}
bool decode(const uint8_t* bytes, Config& config, uint32_t& generation, size_t length) {
  const bool legacy = length == kLegacySaveBytes && !std::memcmp(bytes, "HMC1", 4);
  const bool ordered = length == kSaveBytes && !std::memcmp(bytes, "HMC3", 4);
  if ((!legacy && !ordered && (length != kSaveBytes || std::memcmp(bytes, "HMC2", 4))) ||
      read32(bytes + length - 4) != checksum(bytes, length - 4))
    return false;
  const auto mask = read32(bytes + 8);
  const int count = legacy ? kBaseCount : kCount;
  if (mask & ~((1u << count) - 1) || !(mask & (1u << static_cast<int>(Id::Settings)))) return false;
  config = {};
  config.visible = static_cast<uint16_t>(mask);
  if (visibleCount(config) > kMaxVisible) return false;
  for (int n = 0; n < count; ++n) {
    const auto* text = reinterpret_cast<const char*>(bytes + 12 + n * kLabelBytes);
    if (!std::memchr(text, 0, kLabelBytes) || !setLabel(config, static_cast<Id>(n), text)) return false;
  }
  if (!legacy) {
    const auto* in = bytes + 12 + sizeof(config.labels);
    for (int n = 0; n < kShortcutCount; ++n) {
      auto& entry = config.shortcuts[n];
      entry.folder = *in++;
      if (entry.folder > 1 || !std::memchr(in, 0, kItemBytes) || !utf8(reinterpret_cast<const char*>(in))) return false;
      std::memcpy(entry.title, in, kItemBytes);
      in += kItemBytes;
      if ((mask & (1u << (kBaseCount + n))) && !*entry.title) return false;
      if (*entry.title)
        for (int k = 0; k < n; ++k)
          if (config.shortcuts[k].folder == entry.folder && !std::strcmp(config.shortcuts[k].title, entry.title))
            return false;
    }
    if (ordered) {
      uint16_t seen = 0;
      for (auto& id : config.order) {
        const auto value = *in++;
        if (value >= kCount || (seen & (1u << value))) return false;
        seen |= 1u << value;
        id = static_cast<Id>(value);
      }
    }
  }
  // Retired targets are no longer Home entries; other custom labels and slots survive.
  for (int n = 0; n < kShortcutCount; ++n) {
    const auto& entry = config.shortcuts[n];
    if ((entry.folder == 0 && (!std::strcmp(entry.title, "JAIPUR") || !std::strcmp(entry.title, "SLITHERLINK") ||
                               !std::strcmp(entry.title, "HASHI") || !std::strcmp(entry.title, "SNAKE"))) ||
        (entry.folder == 1 && !std::strcmp(entry.title, "BESTIARY")))
      removeShortcut(config, static_cast<Id>(kBaseCount + n));
  }
  generation = read32(bytes + 4);
  return true;
}
}  // namespace homemenu
