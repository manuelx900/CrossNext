#pragma once
#include <HalStorage.h>

#include <cstdio>
#include <string>
class KOReaderDocumentId {
 public:
  static std::string calculate(const std::string& path, uint8_t*, size_t) {
    if (!Storage.exists(path.c_str())) return {};
    uint64_t hash = 5381;
    for (unsigned char c : path) hash = hash * 33 + c;
    char out[33];
    std::snprintf(out, sizeof(out), "%032llx", static_cast<unsigned long long>(hash));
    return out;
  }
};
