#pragma once

#include "VocabularyStorage.h"

namespace vocabulary {
// One context traversal, with an SD-backed ID set and bounded work per loop.
// The caller owns the record workspace and filter strings until cancel/finish.
class ExportJob {
 public:
  ExportJob(Records& records, Word& word, Context& context) : records(records), word{word}, context(context) {}
  ~ExportJob() { cancel(); }
  bool begin(Purpose, const char* path, int spine, const char* dictionary, uint32_t matchingContexts);
  void step(unsigned budget = 4);
  void cancel();
  bool idle() const { return phase == Phase::Idle; }
  bool finished() const { return phase == Phase::Done || phase == Phase::Failed; }
  bool succeeded() const { return phase == Phase::Done; }

 private:
  enum class Phase : uint8_t { Idle, Prepare, Read, Probe, Append, Commit, Done, Failed };
  Records& records;
  Word& word;
  Context& context;
  HalFile directory, output, index;
  const char* filterPath = nullptr;
  const char* filterDictionary = nullptr;
  Purpose purpose = Purpose::Vocabulary;
  int filterSpine = -1;
  Phase phase = Phase::Idle;
  uint32_t slots = 0, prepared = 0, slot = 0, probes = 0;
  unsigned field = 0, offset = 0;
  char buffer[96]{};
  void fail();
  void close();
  void readNext();
  void probe();
  void append();
  void commit();
};
}  // namespace vocabulary
