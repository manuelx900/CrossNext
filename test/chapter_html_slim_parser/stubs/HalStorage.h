#pragma once

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

namespace HalFileTest {
inline void (*onRead)() = nullptr;
inline bool failNextPayloadRead = false;
inline bool failNextWrite = false;
inline bool failNextClose = false;
}  // namespace HalFileTest

constexpr int O_WRITE = 1;
constexpr int O_CREAT = 2;
constexpr int O_TRUNC = 4;

#include <Print.h>

class HalFile : public Print {
 public:
  HalFile() = default;
  explicit HalFile(const char* path) { open(path, "wb"); }
  ~HalFile() { close(); }
  HalFile(const HalFile&) = delete;
  HalFile& operator=(const HalFile&) = delete;

  bool open(const char* path, const char* mode) {
    close();
    file_ = std::fopen(path, mode);
    return file_ != nullptr;
  }
  int available() const { return file_ ? static_cast<int>(size() - position()) : 0; }
  int read() { return file_ ? std::fgetc(file_) : -1; }
  size_t read(void* buffer, size_t count) {
    if (HalFileTest::failNextPayloadRead && count > 4) {
      HalFileTest::failNextPayloadRead = false;
      return 0;
    }
    const size_t result = file_ ? std::fread(buffer, 1, count, file_) : 0;
    if (HalFileTest::onRead) {
      const auto callback = HalFileTest::onRead;
      HalFileTest::onRead = nullptr;
      callback();
    }
    return result;
  }
  size_t write(const uint8_t* buffer, size_t count) override {
    if (HalFileTest::failNextWrite) {
      HalFileTest::failNextWrite = false;
      return 0;
    }
    const size_t allowed = count < writeBytesLeft_ ? count : writeBytesLeft_;
    const size_t written = file_ ? std::fwrite(buffer, 1, allowed, file_) : 0;
    writeBytesLeft_ -= written;
    return written;
  }
  size_t write(const void* buffer, size_t count) { return write(static_cast<const uint8_t*>(buffer), count); }
  size_t write(uint8_t byte) override { return write(&byte, 1); }
  bool flush() { return file_ && std::fflush(file_) == 0; }
  bool seek(size_t pos) { return file_ && std::fseek(file_, static_cast<long>(pos), SEEK_SET) == 0; }
  bool seekCur(size_t offset) { return file_ && std::fseek(file_, static_cast<long>(offset), SEEK_CUR) == 0; }
  bool close() {
    if (!file_) return false;
    const bool ok = std::fclose(file_) == 0;
    file_ = nullptr;
    if (HalFileTest::failNextClose) {
      HalFileTest::failNextClose = false;
      return false;
    }
    return ok;
  }
  bool isOpen() const { return file_ != nullptr; }
  uint64_t fileSize64() const { return size(); }
  uint32_t modificationTime() const { return 0; }
  explicit operator bool() const { return isOpen(); }
  size_t position() const { return file_ ? static_cast<size_t>(std::ftell(file_)) : 0; }
  size_t size() const {
    if (!file_) return 0;
    const long offset = std::ftell(file_);
    std::fseek(file_, 0, SEEK_END);
    const long end = std::ftell(file_);
    std::fseek(file_, offset, SEEK_SET);
    return end > 0 ? static_cast<size_t>(end) : 0;
  }

  void limitWritesTo(size_t count) { writeBytesLeft_ = count; }

 private:
  std::FILE* file_ = nullptr;
  size_t writeBytesLeft_ = SIZE_MAX;
};

class HalStorage {
 public:
  uint32_t content = 1, media = 1, state = 0;
  uint32_t contentRevision() const { return content ? content + state : 0; }
  uint32_t resourceRevision() const { return content; }
  uint32_t mediaRevision() const { return media; }
  static HalStorage& getInstance() {
    static HalStorage instance;
    return instance;
  }
  int readOpens = 0;
  std::string dictionaryTempPath;
  std::string resolvePath(const std::string& path) const {
    return path == "/.crosspoint/dicthtml.tmp" && !dictionaryTempPath.empty() ? dictionaryTempPath : path;
  }
  HalFile open(const char* path, int) { return HalFile(resolvePath(path).c_str()); }
  bool openFileForRead(const char*, const std::string& path, HalFile& file) {
    readOpens++;
    return file.open(resolvePath(path).c_str(), "rb");
  }
  bool openFileForWrite(const char*, const std::string& path, HalFile& file) { return file.open(path.c_str(), "wb"); }
  bool openDerivedCacheForWrite(const char* m, const char* p, HalFile& f) { return openFileForWrite(m, p, f); }
  bool removeDerivedCache(const char* p) { return remove(p); }
  bool renameDerivedCache(const char* a, const char* b) { return rename(a, b); }
  bool replaceDerivedCache(const char* a, const char* b) { return replaceFile(a, b); }
  bool exists(const char* path) const {
    std::FILE* file = std::fopen(path, "rb");
    if (!file) return false;
    std::fclose(file);
    return true;
  }
  bool remove(const std::string& path) { return std::remove(resolvePath(path).c_str()) == 0; }
  bool rename(const char* from, const char* to) { return std::rename(from, to) == 0; }
  bool mkdir(const char* path) {
    std::error_code error;
    std::filesystem::create_directories(path, error);
    return !error;
  }
  bool replaceFile(const char* from, const char* to) {
    if (exists(to)) std::remove(to);
    return rename(from, to);
  }
};

#define Storage HalStorage::getInstance()

inline uint32_t parserTestMillis = 0;
inline uint32_t millis() { return parserTestMillis; }
inline void delay(uint32_t) {}
