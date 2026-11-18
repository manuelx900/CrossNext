#pragma once
#include <common/FsApiConstants.h>
#include <freertos/semphr.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <map>
#include <string>
#include <vector>
class String {
  std::string value;

 public:
  String() = default;
  String(const char* s) : value(s) {}
  const char* c_str() const { return value.c_str(); }
  size_t length() const { return value.size(); }
};
struct FakeEntry {
  std::vector<uint8_t> bytes;
  bool directory = false;
};
inline std::map<std::string, FakeEntry> fakeFiles;
inline size_t lockedOperations = 0;
inline void needStorageLock() {
  assert(storageLockDepth > 0);
  ++lockedOperations;
}
class FsFile {
  FakeEntry* entry = nullptr;
  std::string name;
  size_t cursor = 0;

 public:
  FsFile() = default;
  FsFile(const char* path, oflag_t flags) : name(path) {
    needStorageLock();
    auto it = fakeFiles.find(name);
    if (it == fakeFiles.end() && (flags & O_CREAT)) it = fakeFiles.emplace(name, FakeEntry{}).first;
    if (it != fakeFiles.end()) {
      entry = &it->second;
      if (flags & O_TRUNC) entry->bytes.clear();
      if (flags & (O_APPEND | O_AT_END)) cursor = entry->bytes.size();
    }
  }
  FsFile(FsFile&& o) noexcept : entry(o.entry), name(std::move(o.name)), cursor(o.cursor) { o.entry = nullptr; }
  FsFile& operator=(FsFile&& o) noexcept {
    if (entry) close();
    entry = o.entry;
    name = std::move(o.name);
    cursor = o.cursor;
    o.entry = nullptr;
    return *this;
  }
  FsFile(const FsFile&) = delete;
  ~FsFile() {
    if (entry) close();
  }
  bool isOpen() const { return entry != nullptr; }
  bool isDirectory() const { return entry && entry->directory; }
  explicit operator bool() const { return isOpen(); }
  size_t size() const { return entry ? entry->bytes.size() : 0; }
  size_t fileSize() const { return size(); }
  size_t position() const { return cursor; }
  int available() const { return int(size() > cursor ? size() - cursor : 0); }
  bool seekSet(uint64_t pos) {
    needStorageLock();
    cursor = size_t(pos);
    return isOpen();
  }
  bool seekCur(int64_t n) { return seekSet(uint64_t(int64_t(cursor) + n)); }
  bool truncate(uint64_t n) {
    needStorageLock();
    if (!entry) return false;
    entry->bytes.resize(size_t(n));
    return true;
  }
  int read(void* out, size_t n) {
    needStorageLock();
    if (!entry) return -1;
    n = std::min(n, size() > cursor ? size() - cursor : 0);
    std::memcpy(out, entry->bytes.data() + cursor, n);
    cursor += n;
    return int(n);
  }
  int read() {
    uint8_t byte = 0;
    return read(&byte, 1) == 1 ? byte : -1;
  }
  size_t write(const void* data, size_t n) {
    needStorageLock();
    if (!entry) return 0;
    if (size() < cursor + n) entry->bytes.resize(cursor + n);
    std::memcpy(entry->bytes.data() + cursor, data, n);
    cursor += n;
    return n;
  }
  void flush() { needStorageLock(); }
  bool close() {
    if (entry) needStorageLock();
    entry = nullptr;
    return true;
  }
  bool getModifyDateTime(uint16_t* d, uint16_t* t) {
    needStorageLock();
    *d = 1;
    *t = 1;
    return isOpen();
  }
  size_t getName(char* out, size_t n) {
    needStorageLock();
    const auto base = name.substr(name.rfind('/') + 1);
    const auto count = std::min(n - 1, base.size());
    std::memcpy(out, base.data(), count);
    out[count] = 0;
    return count;
  }
  bool rename(const char* path) {
    needStorageLock();
    if (!entry || fakeFiles.count(path)) return false;
    auto node = fakeFiles.extract(name);
    node.key() = path;
    auto it = fakeFiles.insert(std::move(node));
    name = path;
    entry = &it.position->second;
    return true;
  }
  void rewindDirectory() { needStorageLock(); }
  FsFile openNextFile() {
    needStorageLock();
    return {};
  }
  int getError() const { return 0; }
  bool openNext(FsFile* directory, oflag_t) {
    *this = directory->openNextFile();
    return isOpen();
  }
};
