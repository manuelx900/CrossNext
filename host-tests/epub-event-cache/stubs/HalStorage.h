#pragma once
#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
struct Entry {
  std::vector<uint8_t> bytes;
  uint32_t modified = 123;
};
class HalFile {
  std::shared_ptr<Entry> entry;
  size_t cursor = 0;

 public:
  inline static bool failWrite = false, failClose = false;
  HalFile() = default;
  explicit HalFile(std::shared_ptr<Entry> e) : entry(std::move(e)) {}
  explicit operator bool() const { return bool(entry); }
  size_t size() const { return entry->bytes.size(); }
  size_t position() const { return cursor; }
  uint32_t modificationTime() const { return entry->modified; }
  bool close() {
    entry.reset();
    return !failClose;
  }
  size_t read(void* out, size_t n) {
    n = std::min(n, size() - std::min(cursor, size()));
    memcpy(out, entry->bytes.data() + cursor, n);
    cursor += n;
    return n;
  }
  size_t write(const void* in, size_t n) {
    if (failWrite) return 0;
    entry->bytes.resize(cursor + n);
    memcpy(entry->bytes.data() + cursor, in, n);
    cursor += n;
    return n;
  }
};
class TestStorage {
 public:
  std::map<std::string, std::shared_ptr<Entry>> entries;
  bool failReplace = false;
  bool exists(const char* p) const { return entries.count(p); }
  bool mkdir(const char* p) {
    if (exists(p)) return false;
    entries[p] = std::make_shared<Entry>();
    return true;
  }
  bool remove(const char* p) { return entries.erase(p); }
  bool openFileForRead(const char*, const std::string& p, HalFile& f) {
    if (!exists(p.c_str())) return false;
    f = HalFile(entries.at(p));
    return true;
  }
  bool openFileForWrite(const char*, const std::string& p, HalFile& f) {
    entries[p] = std::make_shared<Entry>();
    f = HalFile(entries[p]);
    return true;
  }
  bool replaceFile(const char* from, const char* to) {
    if (failReplace) return false;
    entries[to] = entries.at(from);
    entries.erase(from);
    return true;
  }
};
inline TestStorage Storage;
