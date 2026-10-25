#include <sys/time.h>

#include <cstdio>
#include <cstring>

#include "../../lib/hal/HalClock.h"
int checks = 0, failed = 0;
#define CHECK(x)                                  \
  do {                                            \
    ++checks;                                     \
    if (!(x)) {                                   \
      ++failed;                                   \
      std::printf("FAIL %d: %s\n", __LINE__, #x); \
    }                                             \
  } while (0)
extern "C" int __wrap_settimeofday(const timeval*, const void*) { return 0; }
int main(int argc, char** argv) {
  const bool positive = argc > 1 && std::strcmp(argv[1], "positive") == 0;
  const bool punta = argc > 1 && std::strcmp(argv[1], "punta") == 0, dst = argc > 1 && std::strcmp(argv[1], "dst") == 0;
  halClock.begin();
  std::tm t{};
  t.tm_year = 124;
  t.tm_mon = 1;
  t.tm_mday = 29;
  t.tm_hour = 23;
  t.tm_min = 59;
  if (dst) {
    halClock.setTimezone("EST5EDT,M3.2.0/2,M11.1.0/2");
    t.tm_mon = 2;
    t.tm_mday = 10;
    t.tm_hour = 2;
    t.tm_min = 30;
    CHECK(!halClock.setLocalTime(t));
  } else if (positive) {
    halClock.setTimezone("<+14>-14");
    t = {};
    t.tm_year = 120;
    t.tm_mon = 0;
    t.tm_mday = 1;
    CHECK(!halClock.setLocalTime(t));
    t.tm_hour = 14;
    CHECK(halClock.setLocalTime(t));
    CHECK(Rtc::written.year == 2020 && Rtc::written.day == 1 && Rtc::written.hour == 0);
  } else if (punta) {
    halClock.setTimezone("<-03>3");
    t.tm_mon = 11;
    t.tm_mday = 31;
    t.tm_hour = 23;
    t.tm_min = 30;
    CHECK(halClock.setLocalTime(t));
    CHECK(Rtc::written.year == 2025 && Rtc::written.month == 1 && Rtc::written.day == 1 && Rtc::written.hour == 2);
    std::tm out{};
    CHECK(halClock.localTime(out) && out.tm_mday == 31 && out.tm_hour == 23);
  } else {
    halClock.setTimezone("UTC0");
    CHECK(halClock.setLocalTime(t));
    CHECK(Rtc::written.year == 2024 && Rtc::written.day == 29);
    char buf[16];
    CHECK(halClock.formatTime(buf, sizeof(buf), true));
    CHECK(std::strcmp(buf, "11:59 PM") == 0);
    CHECK(halClock.formatTime(buf, sizeof(buf), false));
    CHECK(std::strcmp(buf, "23:59") == 0);
    t.tm_year = 119;
    CHECK(!halClock.setLocalTime(t));
    t.tm_year = 123;
    CHECK(!halClock.setLocalTime(t));
    t.tm_year = 124;
    t.tm_mon = 3;
    t.tm_mday = 31;
    CHECK(!halClock.setLocalTime(t));
    t.tm_mon = 1;
    t.tm_mday = 29;
    t.tm_hour = 24;
    CHECK(!halClock.setLocalTime(t));
    t.tm_hour = 0;
    t.tm_min = 60;
    CHECK(!halClock.setLocalTime(t));
    Rtc::accept = false;
    t.tm_min = 31;
    CHECK(!halClock.setLocalTime(t));
    CHECK(Rtc::written.minute == 59);
  }
  std::printf("Manual clock: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
