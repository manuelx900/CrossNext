#include <cstdio>
#include <cstring>

#include "../../src/util/Timezones.h"
#include "CrossPointSettings.h"
#include "HalClock.h"
int main() {
  int failures = 0, checks = 0;
  auto check = [&](bool pass) {
    ++checks;
    if (!pass) ++failures;
  };
  bool seen[255]{};
  for (unsigned row = 0; row < timezones::count(); ++row) {
    const auto zone = timezones::pickerZoneAt(row);
    check(zone < timezones::count() && !seen[zone]);
    seen[zone] = true;
    check(timezones::pickerRowForZone(zone) == row);
    settings.clockTimezone = zone;
    check(timezones::activeIndex() == zone);
  }
  check(std::strcmp(timezones::table()[timezones::pickerZoneAt(12)].name, "Santiago") == 0);
  check(std::strcmp(timezones::table()[timezones::pickerZoneAt(13)].name, "Punta Arenas") == 0);
  check(std::strcmp(timezones::table()[timezones::pickerZoneAt(14)].name, "St. John's") == 0);
  settings.clockTimezone = 255;
  check(timezones::activeIndex() == 104);
  timezones::applyToClock();
  check(halClock.zone == "UTC3");
  settings.clockTimezone = 12;
  timezones::applyToClock();
  check(halClock.zone == "CLT4CLST,M9.1.6/24,M4.1.6/24");
  std::printf("Timezone picker: %d checks, %d failures\n", checks, failures);
  return failures != 0;
}
