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
  size_t cursor = 0;
  void close() {
    data = nullptr;
    cursor = 0;
  }
  bool seek(size_t at) {
    if (!data || at > data->size()) return false;
    cursor = at;
    return true;
  }
  bool seek64(uint64_t at) { return seek(static_cast<size_t>(at)); }
  bool truncate(uint64_t at) {
    if (!data || at > data->size()) return false;
    data->resize(at);
    return true;
  }
  size_t size() const { return data ? data->size() : 0; }
  uint64_t fileSize64() const { return size(); }
  bool isOpen() const { return data != nullptr; }
  int read(void* out, size_t count) {
    if (!data || testStorage.refuseReads) return -1;
    const size_t length = std::min(count, data->size() - cursor);
    std::memcpy(out, data->data() + cursor, length);
    if (testStorage.corruptReadChecksum && length > 0) static_cast<uint8_t*>(out)[length - 1] ^= 1;
    cursor += length;
    return static_cast<int>(length);
  }
  size_t write(const void* input, size_t count) {
    if (!data) return 0;
    const size_t length =
        testStorage.writeLimit < 0 ? count : std::min(count, static_cast<size_t>(testStorage.writeLimit));
    const auto* bytes = static_cast<const uint8_t*>(input);
    if (data->size() < cursor + length) data->resize(cursor + length);
    std::memcpy(data->data() + cursor, bytes, length);
    cursor += length;
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
  bool remove(const char* path) { return testStorage.available && testStorage.files.erase(path) != 0; }
  bool ensureDirectoryExists(const char* path) { return testStorage.ensureDirectoryExists(path); }
  bool exists(const char* path) { return testStorage.exists(path); }
  bool openFileForRead(const char*, const char* path, HalFile& file) {
    if (!testStorage.available || !exists(path)) return false;
    file.data = &testStorage.files[path];
    file.cursor = 0;
    return true;
  }
  bool openFileForUpdate(const char* tag, const char* path, HalFile& file) { return openFileForRead(tag, path, file); }
  bool openFileForWrite(const char*, const char* path, HalFile& file) {
    if (!testStorage.available) return false;
    file.data = &testStorage.files[path];
    file.cursor = 0;
    file.data->clear();
    return true;
  }
};
#define Storage HalStorage::getInstance()
