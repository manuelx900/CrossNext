#pragma once
#include <Print.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>
class HalFile final : public Print {
 public:
  std::vector<uint8_t> bytes;
  size_t pos = 0, backendReads = 0, backendBytes = 0, logicalReads = 0, zeroLengthReads = 0;
  HalFile() { bytes.reserve(65536); }
  int read(void* dst, size_t length) {
    ++logicalReads;
    if (length == 0) ++zeroLengthReads;
    {
      ++backendReads;
      size_t n = std::min(length, bytes.size() - pos);
      std::memcpy(dst, bytes.data() + pos, n);
      pos += n;
      backendBytes += n;
      return n;
    }
  }
  int read() {
    uint8_t b;
    return read(&b, 1) == 1 ? b : -1;
  }
  size_t write(const uint8_t* p, size_t n) override {
    bytes.insert(bytes.end(), p, p + n);
    return n;
  }
  size_t write(const void* p, size_t n) { return write(static_cast<const uint8_t*>(p), n); }
  size_t write(uint8_t b) override { return write(&b, 1); }
  bool seek(size_t p) {
    if (p > bytes.size()) return false;
    pos = p;
    return true;
  }
  size_t size() const { return bytes.size(); }
  size_t position() const { return pos; }
  int available() const { return static_cast<int>(bytes.size() - pos); }
  bool close() { return true; }
  explicit operator bool() const { return true; }
};
inline uint32_t millis() { return 0; }
inline void delay(uint32_t) {}
