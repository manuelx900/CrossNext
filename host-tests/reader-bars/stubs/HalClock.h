#pragma once
#include <cstdio>
struct Clock {
  bool formatTime(char* out, unsigned n, bool) {
    std::snprintf(out, n, "21:35");
    return true;
  }
};
inline Clock halClock;
