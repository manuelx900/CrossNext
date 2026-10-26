#pragma once
#include <cstring>
#include <string>
#include <vector>
inline std::vector<uint8_t> hashedBytes;
inline bool hashFinished = false;
struct TestHashString {
  const char* c_str() const { return "0123456789abcdef0123456789abcdef"; }
};
class MD5Builder {
 public:
  void begin() {
    hashedBytes.clear();
    hashFinished = false;
  }
  void add(const uint8_t* bytes, size_t count) {
    // Reject the pre-fix signed-to-size_t error without reading outside the fixture.
    if (count > 1024) {
      hashedBytes.clear();
      hashedBytes.push_back(255);
      return;
    }
    hashedBytes.insert(hashedBytes.end(), bytes, bytes + count);
  }
  void add(const char* text) { add(reinterpret_cast<const uint8_t*>(text), std::strlen(text)); }
  void calculate() { hashFinished = true; }
  TestHashString toString() const { return {}; }
};
