#pragma once
#include <cstdint>
#include <cstdio>
#ifdef FCM_TEST_VECTOR
#define CROSSPOINT_VECTOR_FONTS 1
class TtfEpdFont {
 public:
  uint8_t mask = 0;
  bool retained = false;
  bool readerCached = false;
  unsigned clears = 0, additions = 0;
  bool retainsReaderCache() const { return readerCached; }
  char prepared[32] = {};
  void build(const char* text, uint8_t styles) {
    std::snprintf(prepared, sizeof(prepared), "%s", text);
    mask |= styles;
    retained = false;
  }
  void addCoverage(const char* text, uint8_t styles) {
    ++additions;
    std::snprintf(prepared, sizeof(prepared), "%s", text);
    mask |= styles;
    retained = true;
  }
  void clearCache() { ++clears; }
  void discardFailedGlyphs() {}
  void releaseResidentCaches() {}
};
#else
#define CROSSPOINT_VECTOR_FONTS 0
class TtfEpdFont;
#endif
