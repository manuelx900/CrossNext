#pragma once
#include <cstdint>
inline bool deviceSecret(uint8_t (&out)[32]) {
  for (int n = 0; n < 32; ++n) out[n] = n;
  return true;
}
