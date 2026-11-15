#pragma once
#include <VectorFontSupport.h>

#include <cstdint>

class TtfEpdFont {
 public:
  int clears = 0, releases = 0, builds = 0, additions = 0, retries = 0;
  bool retainsReaderCache() const { return false; }
  void clearCache() { ++clears; }
  void discardFailedGlyphs() { ++retries; }
  void releaseResidentCaches() { ++releases; }
  bool build(const char*, uint8_t = 0x01) {
    ++builds;
    return true;
  }
  bool addCoverage(const char*, uint8_t = 0x01) {
    ++additions;
    return true;
  }
};
