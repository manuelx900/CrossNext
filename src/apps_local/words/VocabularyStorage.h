#pragma once

#include <HalStorage.h>

#include "Vocabulary.h"

namespace vocabulary {
inline constexpr char kRoot[] = "/.crosspoint/vocabulary";
inline constexpr char kWords[] = "/.crosspoint/vocabulary/words";
inline constexpr char kContexts[] = "/.crosspoint/vocabulary/contexts";
// name must provide at least 96 bytes. Backup records enumerate only if no .dat exists.
bool recordId(HalFile&, const char* directory, char* name);
class StorageRecords final : public Records {
 public:
  Read word(const char*, Word&) override;
  Read context(const char*, Context&) override;
  bool put(const Word&) override;
  bool put(const Context&) override;
  bool removeWord(const char*) override;
  bool removeContext(const char*) override;
  static bool ready();

 private:
  // One reusable wire buffer, allocated with its owning lookup/app. A record
  // exceeds the task-stack budget; no second framebuffer or resident cache.
  uint8_t bytes[kWireBytes]{};
  template <typename T>
  Read read(const char* dir, const char* id, T&);
  template <typename T>
  bool replace(const char* dir, const T&);
  bool remove(const char* dir, const char* id);
};
struct CaptureNotice {
  Undo token;
  Capture status = Capture::Rejected;
  bool attempted = false;
};
// The reader passes an owned context only on explicit dictionary lookup. No
// lookup failure or a standalone dictionary search can create a blank note.
CaptureNotice saveLookup(Context&, const char* surface, const char* headword, const std::string& definition, bool html,
                         const char* dictionary, const char* language);
bool undoLookup(Undo&);
}  // namespace vocabulary
