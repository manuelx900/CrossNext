#pragma once
#include <cstdio>
#include <string>
class KOReaderDocumentId {
 public:
  static std::string calculate(const std::string& path) { return hash("content:" + path); }
  static std::string calculateFromFilename(const std::string& path) { return hash(path.substr(path.rfind('/') + 1)); }
  static std::string hash(const std::string& path) {
    uint64_t value = 14695981039346656037ULL;
    for (unsigned char c : path) value = (value ^ c) * 1099511628211ULL;
    char out[33];
    std::snprintf(out, sizeof(out), "%032llx", static_cast<unsigned long long>(value));
    return out;
  }
};
