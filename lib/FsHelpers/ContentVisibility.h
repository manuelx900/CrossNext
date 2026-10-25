#pragma once
#include <string_view>

namespace content {
inline constexpr unsigned char VISIBILITY_VERSION = 1;
inline bool asciiEqual(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t n = 0; n < a.size(); ++n) {
    const char c = a[n] >= 'A' && a[n] <= 'Z' ? a[n] + 32 : a[n];
    if (c != b[n]) return false;
  }
  return true;
}
inline bool appFolder(std::string_view name) {
  return asciiEqual(name, "flashcards") || asciiEqual(name, "sleep") || asciiEqual(name, "wallpaper") ||
         asciiEqual(name, "wallpapers") || asciiEqual(name, "fonts") || asciiEqual(name, "licenses") ||
         asciiEqual(name, "apps") || asciiEqual(name, "dictionaries");
}
inline bool hiddenContent(std::string_view path) {
  if (!path.empty() && path.front() == '/') path.remove_prefix(1);
  const auto slash = path.find('/');
  return appFolder(path.substr(0, slash));
}
}  // namespace content
