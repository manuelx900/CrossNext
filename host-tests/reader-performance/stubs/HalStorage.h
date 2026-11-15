#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

inline std::vector<uint8_t> written;
inline size_t writes = 0, failAt = 0;
inline bool exists = false, failClose = false;
class HalFile {
  bool opened = false;

 public:
  void open() {
    opened = exists = true;
    written.clear();
    writes = 0;
  }
  bool isOpen() const { return opened; }
  bool close() {
    opened = false;
    return !failClose;
  }
  size_t write(const void* data, size_t bytes) {
    if (++writes == failAt) return 0;
    const auto* first = static_cast<const uint8_t*>(data);
    written.insert(written.end(), first, first + bytes);
    return bytes;
  }
};
struct TestStorage {
  bool openFileForWrite(const char*, const std::string&, HalFile& file) {
    file.open();
    return true;
  }
  bool remove(const char*) {
    exists = false;
    return true;
  }
};
inline TestStorage Storage;
