#pragma once

#include <ContentVisibility.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace library::inventory {
inline constexpr size_t NameBytes = 255, FolderBytes = 255;
inline bool visible(std::string_view name) { return !name.empty() && name.front() != '.'; }
inline bool appRootFolder(std::string_view name, int depth) { return depth == 0 && content::appFolder(name); }
inline bool bookName(std::string_view name) {
  const auto dot = name.find_last_of('.');
  if (dot == std::string_view::npos) return false;
  const auto ext = name.substr(dot);
  return content::asciiEqual(ext, ".epub") || content::asciiEqual(ext, ".txt") || content::asciiEqual(ext, ".md") ||
         content::asciiEqual(ext, ".xtc") || content::asciiEqual(ext, ".xtch");
}
inline bool representable(std::string_view name, size_t folderBytes) {
  return name.size() <= NameBytes && folderBytes <= FolderBytes;
}
enum class Remember { Added, Duplicate, Untracked };
inline Remember remember(uint64_t* keys, uint16_t capacity, uint16_t base, uint16_t& used, std::string_view name,
                         uint32_t size) {
  if (!keys) return Remember::Untracked;
  uint32_t hash = 2166136261u;
  for (unsigned char c : name) {
    hash ^= c;
    hash *= 16777619u;
  }
  const uint64_t key = (uint64_t(hash) << 32) | size;
  auto* end = keys + used;
  auto* slot = std::lower_bound(keys + base, end, key);
  if (slot != end && *slot == key) return Remember::Duplicate;
  if (used == capacity) return Remember::Untracked;
  std::memmove(slot + 1, slot, size_t(end - slot) * sizeof(*slot));
  *slot = key;
  ++used;
  return Remember::Added;
}
}  // namespace library::inventory
