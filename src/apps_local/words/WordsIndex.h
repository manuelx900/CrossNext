#pragma once

#include "VocabularyStorage.h"

namespace vocabulary {
// Disposable, activity-owned navigation data. Original records remain the only
// authority for definitions and writes; no index survives an app session.
class WordsIndex {
 public:
  static constexpr uint32_t kBatch = 4;
  struct Entry {
    char context[kIdBytes]{}, word[kIdBytes]{}, label[192]{};
    study::CardState card;
    uint16_t failures = 0;
    uint8_t rating = 0;
    bool known = false;
    uint32_t checksum = 0;
  };
  bool begin();
  bool append(const Word&, const Context&);
  bool finish();
  void cancel();
  bool available() const { return ready_; }
  uint32_t count(Purpose purpose) const { return counts_[static_cast<unsigned>(purpose)]; }
  bool read(Purpose, uint32_t first, uint32_t count);
  const Entry& entry(unsigned i) const { return batch_[i]; }
  void unpack(unsigned, Word&, Context&) const;
  bool update(const Word&);
  bool updating() const { return updating_; }
  void stepUpdate();

 private:
  HalFile files_[2];
  // Replaces the unused definition previews in the UI rows. Fixed activity
  // storage, no collection-sized RAM allocation or background worker.
  Entry batch_[kBatch]{};
  uint32_t counts_[2]{};
  uint32_t updateAt_ = 0;
  unsigned updateCollection_ = 0;
  bool ready_ = false;
  bool updating_ = false;
};
}  // namespace vocabulary
