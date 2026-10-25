#pragma once

// FreeInk — small shared helpers (header-only).

#include <cstdlib>
#include <stdint.h>
#include <string.h>

#include <string>

namespace freeink {
namespace content {

// FNV-1a 64-bit. Entry paths are tracked as hashes (8 bytes each instead of a
// heap string apiece); at 64 bits a collision within one container's few
// hundred names is negligible.
inline uint64_t fnv1a64(const char* s, size_t n) {
  uint64_t h = 1469598103934665603ULL;
  for (size_t i = 0; i < n; i++) {
    h ^= static_cast<uint8_t>(s[i]);
    h *= 1099511628211ULL;
  }
  return h;
}

inline int b64Val(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

// Decodes base64 (ignoring whitespace) into out. Returns decoded length, or
// negative on malformed input / insufficient capacity.
inline int32_t base64Decode(const char* in, size_t inLen, uint8_t* out, size_t outCap) {
  uint32_t acc = 0;
  int bits = 0;
  size_t n = 0;
  for (size_t i = 0; i < inLen; i++) {
    const char c = in[i];
    if (c == '=' || c == '\n' || c == '\r' || c == ' ' || c == '\t') continue;
    const int v = b64Val(c);
    if (v < 0) return -1;
    acc = (acc << 6) | static_cast<uint32_t>(v);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      if (n >= outCap) return -1;
      out[n++] = static_cast<uint8_t>((acc >> bits) & 0xFF);
    }
  }
  return static_cast<int32_t>(n);
}

// True when a contiguous block of `bytes` is currently allocatable. Used to
// gate string/vector growth on this path: growth is a throwing allocation,
// and with -fno-exceptions a failed grow abort()s the firmware.
inline bool heapProbe(size_t bytes) {
  void* p = malloc(bytes);
  if (!p) return false;
  free(p);
  return true;
}

inline std::string base64Decode(const std::string& in) {
  std::string out;
  const size_t need = (in.size() * 3) / 4 + 3;
  // Empty on OOM: every caller treats an empty decode as a failed field.
  if (!heapProbe(need + 64)) return out;
  out.resize(need);
  const int32_t n = base64Decode(in.data(), in.size(), reinterpret_cast<uint8_t*>(out.data()), out.size());
  if (n < 0) return std::string();
  out.resize(static_cast<size_t>(n));
  return out;
}

}  // namespace content
}  // namespace freeink
