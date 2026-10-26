#pragma once
#include <string_view>
namespace FsHelpers {
inline bool suffix(std::string_view path, std::string_view ending) {
  if (path.size() < ending.size()) return false;
  path.remove_prefix(path.size() - ending.size());
  for (size_t n = 0; n < path.size(); ++n) {
    char c = path[n];
    if (c >= 'A' && c <= 'Z') c += 32;
    if (c != ending[n]) return false;
  }
  return true;
}
inline bool hasXtcExtension(std::string_view path) { return suffix(path, ".xtc") || suffix(path, ".xtch"); }
inline bool hasReflowableBookExtension(std::string_view path) {
  return suffix(path, ".epub") || suffix(path, ".txt") || suffix(path, ".md");
}
}  // namespace FsHelpers
