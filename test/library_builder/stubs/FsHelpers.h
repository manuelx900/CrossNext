#pragma once

#include <string>

namespace FsHelpers {
inline bool checkFileExtension(const std::string& path, const char* extension) { return path.ends_with(extension); }
inline bool hasEpubExtension(const std::string& path) { return checkFileExtension(path, ".epub"); }
inline bool hasXtcExtension(const std::string& path) {
  return checkFileExtension(path, ".xtc") || checkFileExtension(path, ".xtch");
}
}  // namespace FsHelpers
