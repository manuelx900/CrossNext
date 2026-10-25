#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

class Print {
 public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t value) = 0;
  virtual size_t write(const uint8_t* data, size_t size) = 0;
};

class HalFile final : public Print {
 public:
  std::vector<uint8_t> bytes;
  size_t calls = 0;
  size_t allowance = std::numeric_limits<size_t>::max();
  HalFile() { bytes.reserve(32768); }
  size_t write(uint8_t value) override { return write(&value, 1); }
  size_t write(const uint8_t* data, size_t size) override {
    ++calls;
    const size_t count = std::min(size, allowance);
    bytes.insert(bytes.end(), data, data + count);
    allowance -= count;
    return count;
  }
};
