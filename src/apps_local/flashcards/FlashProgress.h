#pragma once
#include <cstdint>

namespace flashcards {
enum class ImportPhase { Checking, Unpacking, Decompressing, Cards, Images, Saving, SavingImages };
struct ImportProgress {
  ImportPhase phase = ImportPhase::Checking;
  uint64_t done = 0, total = 0;
};
// Synchronous work notifications. The importer never owns UI or callback data.
struct ProgressReporter {
  void* context = nullptr;
  void (*notify)(void*, const ImportProgress&) = nullptr;
  void report(ImportPhase phase, uint64_t done = 0, uint64_t total = 0) const {
    if (notify) notify(context, {phase, done, total});
  }
};
}  // namespace flashcards
