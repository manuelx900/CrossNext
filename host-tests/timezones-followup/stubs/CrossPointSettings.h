#pragma once
#include <cstdint>
struct CrossPointSettings {
  uint8_t clockTimezone = 255, clockUtcOffsetQ = 48, clockDst = 0;
  enum { CLOCK_DST_AUTO, CLOCK_DST_ON, CLOCK_DST_OFF };
};
inline CrossPointSettings settings;
#define SETTINGS settings
