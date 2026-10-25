#pragma once
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

inline bool sdartReadFailure = false;
inline size_t sdartReadCalls = 0, sdartReadBytes = 0, sdartSeekCalls = 0;
inline size_t sdartFailReadCall = 0;
class HalFile {
  std::FILE* handle = nullptr;

 public:
  ~HalFile() { close(); }
  bool open(const char* path) {
    close();
    handle = std::fopen(path, "rb");
    return handle;
  }
  bool close() {
    if (handle) std::fclose(handle);
    handle = nullptr;
    return true;
  }
  int read(void* data, size_t bytes) {
    ++sdartReadCalls;
    if (!handle || sdartReadFailure || sdartReadCalls == sdartFailReadCall) return -1;
    const auto readBytes = std::fread(data, 1, bytes, handle);
    sdartReadBytes += readBytes;
    return int(readBytes);
  }
  bool seek(size_t offset) {
    ++sdartSeekCalls;
    return handle && std::fseek(handle, long(offset), SEEK_SET) == 0;
  }
  uint64_t fileSize64() {
    auto position = std::ftell(handle);
    std::fseek(handle, 0, SEEK_END);
    auto bytes = std::ftell(handle);
    std::fseek(handle, position, SEEK_SET);
    return uint64_t(bytes);
  }
};
struct TestStorage {
  bool openFileForRead(const char*, const char* path, HalFile& file) {
    std::string resolved = path;
    if (resolved.rfind("/apps/", 0) == 0) resolved = "resources" + resolved;
    return file.open(resolved.c_str());
  }
};
inline TestStorage Storage;
