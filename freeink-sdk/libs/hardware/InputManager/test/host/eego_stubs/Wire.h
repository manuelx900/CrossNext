#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

struct WireStub {
  std::vector<std::vector<uint8_t>> writes;
  std::vector<uint8_t> tx;
  uint8_t reg = 0;
  unsigned remaining = 0;
  int failAt = -1;
  bool magicValid = true;
  bool ended = false;

  bool begin(int, int, uint32_t) {
    ended = false;
    return true;
  }
  void end() { ended = true; }
  void setTimeOut(int) {}
  void beginTransmission(uint8_t address) {
    assert(address == 0x40);
    tx.clear();
  }
  size_t write(uint8_t value) {
    tx.push_back(value);
    return 1;
  }
  size_t write(const uint8_t* data, size_t length) {
    tx.insert(tx.end(), data, data + length);
    return length;
  }
  int endTransmission(bool = true) {
    const auto index = static_cast<int>(writes.size());
    writes.push_back(tx);
    reg = tx.at(0);
    return index == failAt ? 1 : 0;
  }
  uint8_t requestFrom(uint8_t, uint8_t length, uint8_t) {
    remaining = length;
    return length;
  }
  unsigned available() const { return remaining; }
  uint8_t read() {
    --remaining;
    return reg == 0xB0 && magicValid ? 0x5A : 0;
  }
};

inline WireStub Wire;
