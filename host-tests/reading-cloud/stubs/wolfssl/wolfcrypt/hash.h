#pragma once
inline int wc_Sha256Hash(const unsigned char*, unsigned int, unsigned char* out) {
  for (int n = 0; n < 32; ++n) out[n] = n + 1;
  return 0;
}
