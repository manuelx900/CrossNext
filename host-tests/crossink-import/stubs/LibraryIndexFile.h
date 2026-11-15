#pragma once
#include <HalStorage.h>

#include <string>
#include <vector>
namespace library {
struct ClixRecord {
  uint16_t ordinal = 0;
};
inline std::vector<std::string> testBooks;
inline bool testIndexReady = true;
inline uint64_t clixPathHash(const char* p, size_t bytes) {
  uint64_t value = 14695981039346656037ULL;
  while (bytes--) value = (value ^ uint8_t(*p++)) * 1099511628211ULL;
  return value;
}
class LibraryIndexFile {
 public:
  bool open(const char*) { return testIndexReady; }
  void close() {}
  uint16_t bookCount() const { return testBooks.size(); }
  bool readRecord(uint16_t n, ClixRecord& out) {
    out.ordinal = n;
    return n < testBooks.size();
  }
  bool readPath(const ClixRecord& r, std::string& out) {
    out = testBooks[r.ordinal];
    return true;
  }
  bool readTitle(const ClixRecord& r, std::string& out) { return readPath(r, out); }
  bool readAuthorField(const ClixRecord&, std::string& out) {
    out.clear();
    return true;
  }
};
}  // namespace library
