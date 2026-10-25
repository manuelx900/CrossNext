#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// The test supplies the device behind these two calls.
bool hostI2cWrite(uint8_t addr, const std::vector<uint8_t>& bytes);
bool hostI2cRead(uint8_t addr, uint8_t reg, uint8_t* out, size_t length);

class TwoWire {
  uint8_t addr_ = 0;
  std::vector<uint8_t> tx_;
  uint8_t rx_[32] = {};
  size_t rxLength_ = 0;
  size_t rxPos_ = 0;

 public:
  bool begin(int, int, uint32_t) { return true; }
  void setTimeOut(int) {}
  void beginTransmission(uint8_t addr) {
    addr_ = addr;
    tx_.clear();
  }
  size_t write(uint8_t byte) {
    tx_.push_back(byte);
    return 1;
  }
  uint8_t endTransmission(bool stop = true) { return (!stop && tx_.size() == 1) || hostI2cWrite(addr_, tx_) ? 0 : 2; }
  uint8_t requestFrom(uint8_t addr, uint8_t length, uint8_t = 1) {
    rxPos_ = 0;
    rxLength_ = !tx_.empty() && hostI2cRead(addr, tx_[0], rx_, length) ? length : 0;
    return static_cast<uint8_t>(rxLength_);
  }
  int read() { return rxPos_ < rxLength_ ? rx_[rxPos_++] : -1; }
};
inline TwoWire Wire;
