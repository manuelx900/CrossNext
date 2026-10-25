#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

struct TestStorage {
  std::map<std::string, std::vector<uint8_t>> files;
  bool available = true;
  int writeLimit = -1;
  bool refuseReads = false;
  bool corruptReadChecksum = false;
  int writes = 0;
  bool ensureDirectoryExists(const char*) const { return available; }
  bool exists(const char* path) const { return files.count(path) != 0; }
};
inline TestStorage testStorage;

class HalFile {
 public:
  std::vector<uint8_t>* data = nullptr;
  size_t size() const { return data ? data->size() : 0; }
  int read(void* out, size_t count) {
    if (!data || testStorage.refuseReads) return -1;
    const size_t length = std::min(count, data->size());
    std::memcpy(out, data->data(), length);
    if (testStorage.corruptReadChecksum && length > 0) static_cast<uint8_t*>(out)[length - 1] ^= 1;
    return static_cast<int>(length);
  }
  size_t write(const void* input, size_t count) {
    if (!data) return 0;
    const size_t length =
        testStorage.writeLimit < 0 ? count : std::min(count, static_cast<size_t>(testStorage.writeLimit));
    const auto* bytes = static_cast<const uint8_t*>(input);
    data->assign(bytes, bytes + length);
    ++testStorage.writes;
    return length;
  }
};

class HalStorage {
 public:
  static HalStorage& getInstance() {
    static HalStorage value;
    return value;
  }
  bool ensureDirectoryExists(const char* path) { return testStorage.ensureDirectoryExists(path); }
  bool exists(const char* path) { return testStorage.exists(path); }
  bool openFileForRead(const char*, const char* path, HalFile& file) {
    if (!testStorage.available || !exists(path)) return false;
    file.data = &testStorage.files[path];
    return true;
  }
  bool openFileForWrite(const char*, const char* path, HalFile& file) {
    if (!testStorage.available) return false;
    file.data = &testStorage.files[path];
    file.data->clear();
    return true;
  }
};
#define Storage HalStorage::getInstance()
