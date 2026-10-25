#pragma once
#include <VectorFontSupport.h>

class TtfEpdFont {
 public:
  int clears = 0, releases = 0, builds = 0, additions = 0, retries = 0;
  void clearCache() { ++clears; }
  void discardFailedGlyphs() { ++retries; }
  void releaseResidentCaches() { ++releases; }
  bool build(const char*) {
    ++builds;
    return true;
  }
  bool addCoverage(const char*) {
    ++additions;
    return true;
  }
};
