#pragma once
#include <cstdio>
#include <string>

#include "LibraryIndexFile.h"
class KOReaderDocumentId {
 public:
  static std::string calculate(const std::string& path, uint8_t*, size_t) {
    char out[33];
    std::snprintf(out, sizeof(out), "%032llx",
                  static_cast<unsigned long long>(library::clixPathHash(path.data(), path.size())));
    return out;
  }
};
