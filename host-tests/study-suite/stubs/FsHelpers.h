#pragma once
#include <string_view>
namespace FsHelpers {
inline bool checkFileExtension(std::string_view value, const char* ext) {
  std::string_view tail(ext);
  if (value.size() < tail.size()) return false;
  value.remove_prefix(value.size() - tail.size());
  for (size_t n = 0; n < tail.size(); ++n) {
    char c = value[n];
    if (c >= 'A' && c <= 'Z') c += 32;
    if (c != tail[n]) return false;
  }
  return true;
}
}  // namespace FsHelpers
