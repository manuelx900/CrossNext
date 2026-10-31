#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

class HalFile {
  friend class HalStorage;

 public:
  bool close() {
    if (!hasImpl) {
      invalidCloses++;
      return false;
    }
    open = false;
    return true;
  }

  bool isOpen() const { return hasImpl && open; }
  int read(void* dst, size_t len) {
    ++reads;
    if (!isOpen() || !data || position + len > data->size()) return 0;
    std::memcpy(dst, data->data() + position, len);
    position += len;
    return static_cast<int>(len);
  }
  uint64_t fileSize64() { return data ? data->size() : 0; }
  bool seekSet(size_t offset) {
    ++seeks;
    if (!isOpen() || !data || offset > data->size()) return false;
    position = offset;
    return true;
  }

  static void resetInvalidCloseCount() { invalidCloses = 0; }
  static int invalidCloseCount() { return invalidCloses; }
  static void resetIoCounts() { reads = seeks = 0; }
  static size_t readCount() { return reads; }
  static size_t seekCount() { return seeks; }

 private:
  bool hasImpl = false;
  bool open = false;
  const std::vector<uint8_t>* data = nullptr;
  size_t position = 0;
  static inline int invalidCloses = 0;
  static inline size_t reads = 0, seeks = 0;
};

class HalStorage {
 public:
  static HalStorage& getInstance() {
    static HalStorage instance;
    return instance;
  }

  bool openFileForRead(const char*, const char* path, HalFile& file) {
    file.hasImpl = true;
    file.open = path == filePath;
    file.data = file.open ? &fileData : nullptr;
    file.position = 0;
    return file.open;
  }

  void setFile(const std::string& path, std::vector<uint8_t> data) {
    filePath = path;
    fileData = std::move(data);
  }

  void clearFile() {
    filePath.clear();
    fileData.clear();
  }
  void truncate(size_t size) { fileData.resize(size); }

 private:
  std::string filePath;
  std::vector<uint8_t> fileData;
};

#define Storage HalStorage::getInstance()
