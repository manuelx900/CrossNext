#include <sys/time.h>

#include <cstdio>
#include <cstring>
#include <initializer_list>

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
static time_t systemEpoch = 0;
extern "C" int __wrap_settimeofday(const timeval* value, const void*) {
  systemEpoch = value->tv_sec;
  return 0;
}
extern "C" time_t __wrap_time(time_t* out) {
  if (out) *out = systemEpoch;
  return systemEpoch;
}
int main(int argc, char** argv) {
  const bool positive = argc > 1 && std::strcmp(argv[1], "positive") == 0;
  const bool punta = argc > 1 && std::strcmp(argv[1], "punta") == 0, dst = argc > 1 && std::strcmp(argv[1], "dst") == 0;
  // A boot may already have a process timezone. RTC fields are always UTC.
  Rtc::written = {2024, 12, 31, 23, 59, 59, 2};
  halClock.begin();
  std::tm bootUtc{};
  gmtime_r(&systemEpoch, &bootUtc);
  CHECK(bootUtc.tm_year == 124 && bootUtc.tm_mon == 11 && bootUtc.tm_mday == 31 && bootUtc.tm_hour == 23);
  ++systemEpoch;
  std::tm current{};
  CHECK(halClock.localTime(current));
  std::tm expected{};
  localtime_r(&systemEpoch, &expected);
  CHECK(current.tm_year == expected.tm_year && current.tm_mon == expected.tm_mon &&
        current.tm_mday == expected.tm_mday && current.tm_hour == expected.tm_hour &&
        current.tm_min == expected.tm_min);
  const auto bootEpoch = systemEpoch;
  const unsigned reads = Rtc::reads;
  for (int advance : {0, 1, 59, 61, 3600, 86401}) {
    systemEpoch = bootEpoch + advance;
    char shown[16], synchronized[16];
    localtime_r(&systemEpoch, &expected);
    std::strftime(synchronized, sizeof(synchronized), "%H:%M", &expected);
    CHECK(halClock.formatTime(shown, sizeof(shown), false));
    CHECK(!std::strcmp(shown, synchronized));
  }
  CHECK(Rtc::reads == reads);
  systemEpoch = bootEpoch;
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
    // A minute rollover must not wait for the RTC cache or another redraw source.
    systemEpoch += 60;
    CHECK(halClock.formatTime(buf, sizeof(buf), false) && std::strcmp(buf, "00:00") == 0);
    CHECK(Rtc::written.hour == 23 && Rtc::written.minute == 59);
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
