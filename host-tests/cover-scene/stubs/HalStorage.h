#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>
struct SourceFile {
  uint64_t size = 800;
  uint32_t modified = 1;
  std::vector<uint8_t> bytes;
};
inline uint32_t testSceneResource = 1;
inline int packWritesLeft = -1, packReadsLeft = -1, sceneOpenHandles = 0;
inline bool sceneFailClose = false;
inline size_t sceneReads = 0, sceneReadBytes = 0;
inline void (*sceneOnRead)() = nullptr;
class HalFile {
 public:
  SourceFile* source = nullptr;
  size_t cursor = 0;
  bool writable = false, derived = false;
  HalFile() = default;
  HalFile(const HalFile&) = delete;
  HalFile(HalFile&& other) noexcept { *this = std::move(other); }
  HalFile& operator=(HalFile&& other) noexcept {
    if (this != &other) {
      close();
      source = other.source;
      cursor = other.cursor;
      writable = other.writable;
      derived = other.derived;
      other.source = nullptr;
    }
    return *this;
  }
  ~HalFile() { close(); }
  uint64_t fileSize64() const { return source ? source->size : 0; }
  uint32_t modificationTime() const { return source ? source->modified : 0; }
  bool isOpen() const { return source != nullptr; }
  bool seek(size_t offset) {
    cursor = offset;
    return isOpen();
  }
  int read(void* out, size_t count) {
    ++sceneReads;
    if (!source || packReadsLeft == 0) return -1;
    if (packReadsLeft > 0) --packReadsLeft;
    count = std::min(count, source->bytes.size() > cursor ? source->bytes.size() - cursor : 0);
    if (count) std::memcpy(out, source->bytes.data() + cursor, count);
    cursor += count;
    sceneReadBytes += count;
    if (sceneOnRead) {
      auto cb = sceneOnRead;
      sceneOnRead = nullptr;
      cb();
    }
    return int(count);
  }
  size_t write(const void* data, size_t count) {
    if (!source || packWritesLeft == 0) return 0;
    if (packWritesLeft > 0) --packWritesLeft;
    if (!derived) ++testSceneResource;
    if (source->bytes.size() < cursor + count) source->bytes.resize(cursor + count);
    if (count) std::memcpy(source->bytes.data() + cursor, data, count);
    cursor += count;
    source->size = source->bytes.size();
    return count;
  }
  bool close() {
    if (!source) return false;
    if (writable && !derived) ++testSceneResource;
    source = nullptr;
    --sceneOpenHandles;
    return !sceneFailClose;
  }
};
struct TestStorage {
  std::map<std::string, SourceFile> files;
  int opens = 0;
  uint32_t media = 1;
  uint32_t mediaRevision() const { return media; }
  uint32_t resourceRevision() const { return testSceneResource; }
  bool openFileForRead(const char*, const char* path, HalFile& out) {
    ++opens;
    auto it = files.find(path);
    if (it == files.end()) return false;
    out.close();
    out.source = &it->second;
    out.cursor = 0;
    out.writable = false;
    out.derived = false;
    ++sceneOpenHandles;
    return true;
  }
  bool openDerivedCacheForWrite(const char*, const char* path, HalFile& out) {
    out.close();
    auto& file = files[path];
    file.bytes.clear();
    file.size = 0;
    out.source = &file;
    out.cursor = 0;
    out.writable = true;
    out.derived = true;
    ++sceneOpenHandles;
    return true;
  }
};
inline TestStorage Storage;
