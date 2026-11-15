#pragma once
#include <algorithm>
#include <cctype>
#include <string>
namespace FsHelpers {
inline bool suffix(std::string value, const char* suffix) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });
  return value.ends_with(suffix);
}
inline bool hasJpgExtension(const std::string& value) { return suffix(value, ".jpg") || suffix(value, ".jpeg"); }
inline bool hasPngExtension(const std::string& value) { return suffix(value, ".png"); }
}  // namespace FsHelpers
