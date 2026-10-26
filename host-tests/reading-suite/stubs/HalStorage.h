#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

struct TestStorage {
  std::map<std::string, std::vector<uint8_t>> files;
  bool available = true;
  int writeLimit = -1;
  bool refuseReads = false;
  bool corruptReadChecksum = false;
  int writes = 0;
  bool refuseRename = false;
  bool enforceParents = false;
  int readLimit = -1;
  bool refuseSeeks = false;
  std::string failDirectory;
  std::set<std::string> directories{"/"};
  bool ensureDirectoryExists(const char* name) {
    if (!available) return false;
    directories.insert(name);
    return true;
  }
  bool exists(const char* path) const { return files.count(path) != 0; }
};
inline TestStorage testStorage;

class HalFile {
 public:
  std::vector<uint8_t>* data = nullptr;
  size_t cursor = 0;
  void flush() {}
  std::string path;
  bool directory = false;
  std::string lastEntry;
  void close() {
    data = nullptr;
    cursor = 0;
    directory = false;
    path.clear();
    lastEntry.clear();
  }
  bool seek(size_t at) {
    if (!data || testStorage.refuseSeeks || at > data->size()) return false;
    cursor = at;
    return true;
  }
  bool seek64(uint64_t at) { return seek(static_cast<size_t>(at)); }
  bool seekSet(size_t at) { return seek(at); }
  bool truncate(uint64_t at) {
    if (!data || at > data->size()) return false;
    data->resize(at);
    return true;
  }
  size_t size() const { return data ? data->size() : 0; }
  size_t position() const { return cursor; }
  uint64_t fileSize64() const { return size(); }
  bool isOpen() const { return data != nullptr || directory; }
  explicit operator bool() const { return isOpen(); }
  bool isDirectory() const { return directory; }
  size_t fileSize() const { return size(); }
  size_t getName(char* out, size_t size) const {
    const auto name = path.substr(path.rfind('/') + 1);
    const size_t count = std::min(size - 1, name.size());
    std::memcpy(out, name.data(), count);
    out[count] = 0;
    return count;
  }
  HalFile openNextFile();
  HalFile openNextFile(bool& ioError) {
    ioError = !testStorage.available || path == testStorage.failDirectory;
    return ioError ? HalFile{} : openNextFile();
  }
  int read(void* out, size_t count) {
    if (!data || testStorage.refuseReads) return -1;
    const size_t length =
        std::min({count, data->size() - cursor, testStorage.readLimit < 0 ? count : size_t(testStorage.readLimit)});
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
  HalFile open(const char* path) {
    HalFile file;
    if (!testStorage.available) return file;
    file.path = path;
    const auto found = testStorage.files.find(path);
    if (found != testStorage.files.end())
      file.data = &found->second;
    else
      file.directory = true;
    return file;
  }
  bool rename(const char* from, const char* to) {
    if (testStorage.refuseRename || !exists(from) || exists(to)) return false;
    auto node = testStorage.files.extract(from);
    node.key() = to;
    testStorage.files.insert(std::move(node));
    return true;
  }
  bool readFileToString(const char*, const std::string& path, size_t cap, std::string& out) {
    const auto it = testStorage.files.find(path);
    if (it == testStorage.files.end() || it->second.size() > cap) return false;
    out.assign(it->second.begin(), it->second.end());
    return true;
  }
  bool openFileForRead(const char*, const char* path, HalFile& file) {
    if (!testStorage.available || !exists(path)) return false;
    file.data = &testStorage.files[path];
    file.cursor = 0;
    return true;
  }
  bool openFileForRead(const char* tag, const std::string& path, HalFile& file) {
    return openFileForRead(tag, path.c_str(), file);
  }
  bool openFileForUpdate(const char* tag, const char* path, HalFile& file) { return openFileForRead(tag, path, file); }
  bool openFileForWrite(const char*, const char* path, HalFile& file) {
    if (!testStorage.available) return false;
    const auto parent = std::string(path).substr(0, std::string(path).rfind('/'));
    if (testStorage.enforceParents && testStorage.directories.count(parent) == 0) return false;
    file.data = &testStorage.files[path];
    file.cursor = 0;
    file.data->clear();
    return true;
  }
  bool openFileForWrite(const char* tag, const std::string& path, HalFile& file) {
    return openFileForWrite(tag, path.c_str(), file);
  }
};
#define Storage HalStorage::getInstance()
inline HalFile HalFile::openNextFile() {
  HalFile next;
  if (!directory || !testStorage.available) return next;
  const auto prefix = path == "/" ? path : path + "/";
  std::string chosen;
  for (const auto& [name, contents] : testStorage.files) {
    (void)contents;
    if (name.compare(0, prefix.size(), prefix) != 0) continue;
    const auto child = name.substr(prefix.size(), name.find('/', prefix.size()) - prefix.size());
    if (child.empty() || child <= lastEntry || (!chosen.empty() && child >= chosen)) continue;
    chosen = child;
  }
  if (!chosen.empty()) {
    lastEntry = chosen;
    next = Storage.open((prefix + chosen).c_str());
  }
  return next;
}
