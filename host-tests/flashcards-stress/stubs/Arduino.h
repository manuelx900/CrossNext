#pragma once

#include <chrono>
#include <cstdint>

inline void delay(unsigned) {}

inline uint32_t millis() {
  static const auto start = std::chrono::steady_clock::now();
  return static_cast<uint32_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
}
