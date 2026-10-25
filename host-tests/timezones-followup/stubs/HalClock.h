#pragma once
#include <string>
struct Clock {
  std::string zone;
  void setTimezone(const char* z) { zone = z; }
};
inline Clock halClock;
