#pragma once
#include <Arduino.h>
#include <HalStorage.h>

#include <string>

#include "../study/StudyDeck.h"
#include "FlashHistory.h"
#include "FlashProgress.h"
namespace flashcards {
class FileSource final : public study::ByteSource {
  HalFile& file;
  unsigned reads = 0;

 public:
  explicit FileSource(HalFile& f) : file(f) {}
  bool read(uint32_t offset, void* data, uint32_t length) override {
    if (!(++reads % 128)) delay(1);
    return file.seek(offset) && file.read(static_cast<uint8_t*>(data), length) == int(length);
  }
  uint32_t size() const override { return file.size(); }
};
struct DeckInfo {
  std::string name, source, directory;
  int total = 0, due = 0, fresh = 0;
  bool folder = false, ready = false;
};
// Cold-path workspace reused by the library/history activity, never a render allocation.
class Catalog {
  study::StudyDeck deck;
  uint8_t buffer[512]{};
  char path[640]{};

 public:
  bool resolve(const char* source, char* destination, size_t length, ProgressReporter progress = {});
  bool findImported(const char*, char*, size_t);
  bool describe(const char* directory, DeckInfo&, bool counts = true);
};
}  // namespace flashcards
