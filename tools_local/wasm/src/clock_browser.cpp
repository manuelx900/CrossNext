// Simulated time is shared by the RTC and consumers of the C system clock.
#include <HalClock.h>

#include <ctime>
extern "C" std::time_t __real_time(std::time_t*);
extern "C" std::time_t __wrap_time(std::time_t* out) {
  const auto now = __real_time(nullptr) + simulatorClockOffset().load();
  if (out) *out = now;
  return now;
}
