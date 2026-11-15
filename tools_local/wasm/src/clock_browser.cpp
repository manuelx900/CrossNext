// Simulated time is shared by the RTC and consumers of the C system clock.
#include <HalClock.h>

#include <cstdlib>
#include <ctime>
extern "C" std::time_t __real_time(std::time_t*);
extern "C" std::time_t __wrap_time(std::time_t* out) {
  static const auto demonstrationOffset = [] {
    const char* value = std::getenv("CROSSNEXT_SIM_EPOCH");
    const auto epoch = value ? std::strtoll(value, nullptr, 10) : 0;
    return epoch >= 1577836800 && epoch <= 7258118399LL ? epoch - __real_time(nullptr) : 0;
  }();
  const auto now = __real_time(nullptr) + demonstrationOffset + simulatorClockOffset().load();
  if (out) *out = now;
  return now;
}
