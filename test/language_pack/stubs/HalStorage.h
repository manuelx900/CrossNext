#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>
class HalFile;
class HalStorage {
 public:
  std::map<std::string, std::vector<uint8_t>> files;
  size_t reads = 0;
  size_t opens = 0;
  bool enumerateError = false;
  static HalStorage& getInstance() {
    static HalStorage instance;
    return instance;
  }
  bool openFileForRead(const char*, const char* path, HalFile& file);
  HalFile open(const char* path);
};
#define Storage HalStorage::getInstance()
class HalFile {
 public:
  const std::vector<uint8_t>* bytes = nullptr;
  std::string name;
  size_t cursor = 0;
  bool directory = false;
  size_t entryIndex = 0;
  operator bool() const { return bytes || directory; }
  bool seek(size_t pos) {
    cursor = pos;
    return bytes && pos <= bytes->size();
  }
  int read(void* dest, size_t count) {
    ++Storage.reads;
    if (!bytes || cursor > bytes->size()) return -1;
    count = std::min(count, bytes->size() - cursor);
    memcpy(dest, bytes->data() + cursor, count);
    cursor += count;
    return static_cast<int>(count);
  }
  uint64_t fileSize64() const { return bytes ? bytes->size() : 0; }
  bool isDirectory() const { return directory; }
  size_t getName(char* out, size_t length) {
    const auto basename = name.substr(name.find_last_of('/') + 1);
    if (length) {
      memcpy(out, basename.c_str(), std::min(length - 1, basename.size()));
      out[std::min(length - 1, basename.size())] = 0;
    }
    return basename.size();
  }
  void rewindDirectory() { entryIndex = 0; }
  HalFile openNextFile(bool& error) {
    error = Storage.enumerateError;
    if (error || entryIndex >= Storage.files.size()) return {};
    auto it = Storage.files.begin();
    std::advance(it, entryIndex++);
    return Storage.open(it->first.c_str());
  }
};
inline HalFile HalStorage::open(const char* path) {
  ++opens;
  HalFile file;
  if (strcmp(path, "/languages") == 0) {
    file.directory = true;
    return file;
  }
  const auto it = files.find(path);
  if (it != files.end()) {
    file.bytes = &it->second;
    file.name = path;
  }
  return file;
}
inline bool HalStorage::openFileForRead(const char*, const char* path, HalFile& file) {
  file = open(path);
  return static_cast<bool>(file);
}
