#pragma once
#include "HalStorage.h"
// The real ZIP reader is exercised in the firmware simulator. This adapter
// supplies archive members to test the importer's format selection and zstd.
class ZipFile {
 public:
  explicit ZipFile(const std::string& p) : path(p) {}
  bool getInflatedFileSize(const char* name, size_t* size) {
    const auto found = testStorage.files.find(path + "::" + name);
    if (found == testStorage.files.end()) return false;
    *size = found->second.size();
    return true;
  }
  bool readFileToStream(const char* name, Print& out, size_t chunk) {
    const auto found = testStorage.files.find(path + "::" + name);
    if (found == testStorage.files.end()) return false;
    for (size_t n = 0; n < found->second.size(); n += chunk) {
      const size_t size = std::min(chunk, found->second.size() - n);
      if (out.write(found->second.data() + n, size) != size) return false;
    }
    return true;
  }

 private:
  const std::string& path;
};
