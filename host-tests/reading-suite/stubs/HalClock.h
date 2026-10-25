#pragma once
#include <ctime>
#ifdef _WIN32
inline tm* localtime_r(const time_t* t, tm* out) { return localtime_s(out, t) == 0 ? out : nullptr; }
#endif
struct TestClock {
  bool localTime(tm&) { return false; }
};
inline TestClock halClock;
