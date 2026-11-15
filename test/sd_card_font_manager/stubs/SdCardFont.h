#pragma once
#include <cstdint>
class SdCardFont {
 public:
  bool load(const char*) { return true; }
  uint32_t contentHash() const { return 17; }
  unsigned styleCount() const { return 4; }
  const void* getEpdFont(unsigned) const { return this; }
};
