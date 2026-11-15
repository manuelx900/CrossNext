#pragma once
#include <FS.h>
#include <Print.h>
class SDCardManager {
 public:
  static SDCardManager& getInstance() {
    static SDCardManager value;
    return value;
  }
  bool begin() { return true; }
  bool ready() const { return true; }
  void shutdown() {}
  FsFile open(const char* path, oflag_t flags = O_RDONLY) { return FsFile(path, flags); }
  std::vector<String> listFiles(const char*, int) { return {}; }
  String readFile(const char*) { return String(""); }
  bool readFileToStream(const char*, Print&, size_t) { return true; }
  size_t readFileToBuffer(const char*, char*, size_t, size_t) { return 0; }
  bool writeFile(const char* path, const String& text) {
    auto file = open(path, O_WRITE | O_CREAT | O_TRUNC);
    return file && file.write(text.c_str(), text.length()) == text.length();
  }
  bool exists(const char* path) { return fakeFiles.count(path); }
  bool mkdir(const char* path, bool = true) {
    if (exists(path)) return false;
    fakeFiles[path].directory = true;
    return true;
  }
  bool ensureDirectoryExists(const char* path) { return exists(path) ? fakeFiles[path].directory : mkdir(path); }
  bool sdFreeBytes(uint64_t& out) {
    out = 1024 * 1024;
    return true;
  }
  bool remove(const char* path) { return fakeFiles.erase(path); }
  bool rmdir(const char* path) { return remove(path); }
  bool removeDir(const char* path) { return remove(path); }
  bool rename(const char* from, const char* to) {
    if (!exists(from) || exists(to)) return false;
    auto node = fakeFiles.extract(from);
    node.key() = to;
    fakeFiles.insert(std::move(node));
    return true;
  }
  bool replaceFile(const char* from, const char* to) {
    remove(to);
    return rename(from, to);
  }
  bool openFileForRead(const char*, const char* path, FsFile& file) {
    file = open(path);
    return file.isOpen();
  }
  bool openFileForWrite(const char*, const char* path, FsFile& file) {
    file = open(path, O_RDWR | O_CREAT | O_TRUNC);
    return file.isOpen();
  }
};
