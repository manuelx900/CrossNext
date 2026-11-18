#pragma once

#include "VocabularyStorage.h"

namespace vocabulary {
// Acknowledgment metadata only. Saved word/context records are never modified.
class VocabularyNotice {
 public:
  ~VocabularyNotice() { close(); }
  void resume();
  void beginScan();
  bool observe(const char* contextId);
  bool observing() const { return looking; }
  bool stepObservation();
  bool acknowledge();
  bool busy() const { return bool(pending) && !failed; }
  void step();
  void close();

 private:
  enum class Lookup : uint8_t { None, Accepted, Current };
  HalFile current, pending, lookup;
  Lookup lookupKind = Lookup::None;
  char lookupId[kIdBytes]{};
  uint64_t lookupRemaining = 0;
  bool looking = false;
  bool currentValid = true, failed = false, volatileAccepted = false;
  static bool markerKnown(const char*);
  bool beginLookup(Lookup kind);
  bool appendNew(const char* id);
  static bool writeMarker(const char* id, const uint8_t* record);
};
}  // namespace vocabulary
