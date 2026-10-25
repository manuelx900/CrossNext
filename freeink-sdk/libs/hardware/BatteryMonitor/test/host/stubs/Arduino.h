#pragma once
#include <cstdint>
constexpr int HIGH = 1, LOW = 0, INPUT = 1, OUTPUT = 3, INPUT_PULLUP = 5;
inline unsigned long hostMillis = 0;  // the test's clock; delay() advances it
inline unsigned long millis() { return hostMillis; }
inline void delay(unsigned long ms) { hostMillis += ms; }
inline void delayMicroseconds(unsigned int) {}
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return 0; }
inline uint32_t analogReadMilliVolts(int) { return 0; }
struct SerialStub {
  explicit operator bool() const { return false; }
  template <typename... Args>
  void printf(const char*, Args...) {}
};
inline SerialStub Serial;
