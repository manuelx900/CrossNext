#pragma once

#include <ContentVisibility.h>

#include <cstring>
#include <string>

namespace FsHelpers {
inline bool checkFileExtension(const std::string& path, const char* extension) {
  return path.size() >= std::strlen(extension) &&
         content::asciiEqual(std::string_view(path).substr(path.size() - std::strlen(extension)), extension);
}
inline bool hasEpubExtension(const std::string& path) { return checkFileExtension(path, ".epub"); }
inline bool hasXtcExtension(const std::string& path) {
  return checkFileExtension(path, ".xtc") || checkFileExtension(path, ".xtch");
}
}  // namespace FsHelpers
