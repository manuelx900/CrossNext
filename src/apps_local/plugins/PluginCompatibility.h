#pragma once

#include <string_view>

namespace plugincompat {
// These bundles require host features absent from this firmware. Keep IDs,
// rather than display titles, so translated catalogs cannot bypass the guard.
inline constexpr std::string_view BLOCKED[] = {"dropbox", "google-drive", "localsend", "reading-statistics"};
inline bool sameName(std::string_view left, std::string_view right) {
  if (left.size() != right.size()) return false;
  for (size_t n = 0; n < left.size(); ++n) {
    const char a = left[n] >= 'A' && left[n] <= 'Z' ? left[n] + ('a' - 'A') : left[n];
    const char b = right[n] >= 'A' && right[n] <= 'Z' ? right[n] + ('a' - 'A') : right[n];
    if (a != b) return false;
  }
  return true;
}
inline bool blocked(std::string_view name) {
  for (const auto id : BLOCKED)
    if (sameName(id, name)) return true;
  return false;
}
inline bool blockedDestination(std::string_view path) {
  // FAT accepts repeated separators and dot components. Inspect components
  // without allocating a normalized copy of an HTTP destination.
  const auto next = [&path]() {
    while (!path.empty()) {
      const auto start = path.find_first_not_of('/');
      if (start == path.npos) {
        path = {};
        return std::string_view{};
      }
      path.remove_prefix(start);
      const auto end = path.find('/');
      const auto part = path.substr(0, end);
      path.remove_prefix(end == path.npos ? path.size() : end);
      if (part != ".") return part;
    }
    return std::string_view{};
  };
  const auto root = next();
  if (sameName(root, ".crosspoint")) {
    if (!sameName(next(), "plugins")) return false;
  } else if (!sameName(root, "plugins") && !sameName(root, ".plugins"))
    return false;
  return blocked(next());
}
}  // namespace plugincompat
