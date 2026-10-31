#pragma once
#include <cstdint>
#include <map>
#include <string>
struct SourceFile {
  uint64_t size = 800;
  uint32_t modified = 1;
};
class HalFile {
 public:
  SourceFile* source = nullptr;
  uint64_t fileSize64() const { return source->size; }
  uint32_t modificationTime() const { return source->modified; }
};
struct TestStorage {
  std::map<std::string, SourceFile> files;
  int opens = 0;
  bool openFileForRead(const char*, const char* path, HalFile& out) {
    ++opens;
    auto it = files.find(path);
    if (it == files.end()) return false;
    out.source = &it->second;
    return true;
  }
};
inline TestStorage Storage;
