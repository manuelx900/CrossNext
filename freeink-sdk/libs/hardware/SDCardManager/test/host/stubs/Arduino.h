#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

using std::min;
inline uint32_t nowMs = 0;
inline unsigned yields = 0;
inline unsigned watchdogResets = 0;
inline bool watchdogSubscribed = false;
inline uint32_t millis() { return nowMs; }
inline void delay(unsigned ms) { nowMs += ms; }
inline void vTaskDelay(unsigned ticks) { ++yields; nowMs += ticks; }
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
constexpr int OUTPUT = 1;
constexpr int HIGH = 1;
constexpr int LOW = 0;

class Print {
 public:
  virtual ~Print() = default;
  virtual size_t write(const uint8_t*, size_t) = 0;
};

struct FakeSerial {
  explicit operator bool() const { return false; }
  void println(const char*) {}
  template <typename... Args>
  void printf(const char*, Args...) {}
};
inline FakeSerial Serial;
