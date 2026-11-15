#pragma once
#include <cstdint>
#include <cstdio>
#include <string>

struct SdCardFontFileInfo {
  uint8_t pointSize;
};
struct SdCardFontFamilyInfo {
  static constexpr unsigned PATH_BUFFER_SIZE = 160;
  std::string name;
  SdCardFontFileInfo files[4] = {{12}, {14}, {16}, {18}};
  const SdCardFontFileInfo* findFile(unsigned size) const {
    for (const auto& file : files)
      if (file.pointSize == size) return &file;
    return nullptr;
  }
  const SdCardFontFileInfo* findNearestSize(unsigned size) const { return findFile(size); }
  bool filePath(const SdCardFontFileInfo& file, char* out, unsigned bytes) const {
    return std::snprintf(out, bytes, "/fonts/%s/%u.cpfont", name.c_str(), file.pointSize) < int(bytes);
  }
};
