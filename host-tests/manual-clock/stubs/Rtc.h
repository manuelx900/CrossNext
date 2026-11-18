#pragma once
#include <cstdint>
struct Rtc {
  struct DateTime {
    uint16_t year;
    uint8_t month, day, hour, minute, second, weekday;
  };
  inline static DateTime written{2000, 1, 1, 0, 0, 0, 0};
  inline static bool accept = true;
  inline static unsigned reads = 0;
  bool begin() { return true; }
  bool now(DateTime& out) {
    ++reads;
    out = written;
    return true;
  }
  bool set(const DateTime& value) {
    if (!accept) return false;
    written = value;
    return true;
  }
};
