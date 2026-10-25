#pragma once
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "Print.h"
constexpr int O_RDWR = 3, O_CREAT = 4, O_TRUNC = 8;
struct TestStorage {
  std::map<std::string, std::vector<uint8_t>> files;
  std::set<std::string> directories{"/"};
  bool available = true;
  int writeLimit = -1;
  size_t bytesRead = 0;
  size_t readCalls = 0;
  int renameCalls = 0, failRenameAt = -1;
};
inline TestStorage testStorage;
class HalFile : public Print {
 public:
  std::vector<uint8_t>* data = nullptr;
  size_t cursor = 0;
  bool writable = false;
  std::string directory, filename;
  size_t directoryIndex = 0;
  explicit operator bool() const { return (data || !directory.empty()) && testStorage.available; }
  bool isDirectory() const { return !directory.empty(); }
  void getName(char* out, size_t n) { std::snprintf(out, n, "%s", filename.c_str()); }
  HalFile openNextFile() {
    HalFile f;
    size_t at = 0;
    for (auto& item : testStorage.files) {
      if (item.first.substr(0, directory.size() + 1) != directory + "/") continue;
      const auto name = item.first.substr(directory.size() + 1);
      if (name.find('/') != std::string::npos) continue;
      if (at++ < directoryIndex) continue;
      ++directoryIndex;
      f.data = &item.second;
      f.filename = name;
      return f;
    }
    return f;
  }
  bool isOpen() const { return bool(*this); }
  uint32_t modificationTime() { return 0; }
  void close() {
    data = nullptr;
    directory.clear();
    cursor = 0;
  }
  void flush() {}
  size_t size() { return data ? data->size() : 0; }
  uint64_t fileSize64() { return size(); }
  bool seekSet(size_t n) {
    if (!*this || n > size()) return false;
    cursor = n;
    return true;
  }
  bool seek64(uint64_t n) { return seekSet(size_t(n)); }
  bool seek(size_t n) { return seekSet(n); }
  bool seekCur(int64_t n) { return n >= -int64_t(cursor) && seekSet(size_t(int64_t(cursor) + n)); }
  int available() { return *this ? int(size() - cursor) : 0; }
  size_t position() { return cursor; }
  int read(void* out, size_t n) {
    ++testStorage.readCalls;
    if (!*this) return -1;
    n = std::min(n, size() - cursor);
    std::memcpy(out, data->data() + cursor, n);
    cursor += n;
    testStorage.bytesRead += n;
    return int(n);
  }
  int read() {
    uint8_t c;
    return read(&c, 1) == 1 ? c : -1;
  }
  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t* p, size_t n) override {
    if (!*this || !writable) return 0;
    if (testStorage.writeLimit >= 0) n = std::min(n, size_t(testStorage.writeLimit));
    if (cursor + n > size()) data->resize(cursor + n);
    std::memcpy(data->data() + cursor, p, n);
    cursor += n;
    return n;
  }
  size_t write(const void* p, size_t n) { return write(static_cast<const uint8_t*>(p), n); }
};
class HalStorage {
 public:
  bool ready() const { return testStorage.available; }
  static HalStorage& getInstance() {
    static HalStorage storage;
    return storage;
  }
  bool exists(const char* p) {
    return testStorage.available && (testStorage.files.count(p) || testStorage.directories.count(p));
  }
  bool ensureDirectoryExists(const char* p) {
    if (!testStorage.available) return false;
    std::string path(p);
    if (exists(p)) return true;
    const auto parent = path.substr(0, path.rfind('/'));
    if (!testStorage.directories.count(parent.empty() ? "/" : parent)) return false;
    testStorage.directories.insert(path);
    return true;
  }
  bool mkdir(const char* p) { return ensureDirectoryExists(p); }
  HalFile open(const char* p, int flags = 0) {
    HalFile f;
    if (!testStorage.available) return f;
    if (testStorage.directories.count(p)) {
      f.directory = p;
      return f;
    }
    auto at = testStorage.files.find(p);
    if (at == testStorage.files.end()) {
      if (!(flags & O_CREAT)) return f;
      std::string path(p);
      auto parent = path.substr(0, path.rfind('/'));
      if (!testStorage.directories.count(parent.empty() ? "/" : parent)) return f;
      at = testStorage.files.emplace(path, std::vector<uint8_t>{}).first;
    }
    if (flags & O_TRUNC) at->second.clear();
    f.data = &at->second;
    f.writable = (flags & O_RDWR) != 0;
    return f;
  }
  bool openFileForRead(const char*, const char* p, HalFile& f) {
    f = open(p);
    return bool(f);
  }
  bool openFileForUpdate(const char*, const char* p, HalFile& f) {
    f = open(p, O_RDWR);
    return bool(f);
  }
  bool openFileForWrite(const char*, const char* p, HalFile& f) {
    f = open(p, O_RDWR | O_CREAT);
    if (!f) return false;
    f.data->clear();
    return true;
  }
  bool remove(const char* p) { return testStorage.available && testStorage.files.erase(p); }
  bool openFileForRead(const char* module, const std::string& p, HalFile& f) {
    return openFileForRead(module, p.c_str(), f);
  }
  bool openFileForWrite(const char* module, const std::string& p, HalFile& f) {
    return openFileForWrite(module, p.c_str(), f);
  }
  bool freeBytes(uint64_t& n) {
    n = 100 * 1024 * 1024;
    return true;
  }
  bool rename(const char* from, const char* to) {
    if (++testStorage.renameCalls == testStorage.failRenameAt) return false;
    if (exists(to)) return false;
    return replaceFile(from, to);
  }
  bool replaceFile(const char* from, const char* to) {
    if (!testStorage.available || !testStorage.files.count(from)) return false;
    auto data = std::move(testStorage.files[from]);
    testStorage.files.erase(from);
    testStorage.files[to] = std::move(data);
    return true;
  }
};
#define Storage HalStorage::getInstance()
