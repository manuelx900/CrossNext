#include "FlashStorage.h"

#include <Arduino.h>
#include <FsHelpers.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "../study/StudyScheduler.h"
namespace flashcards {
bool Catalog::resolve(const char* source, char* destination, size_t length, ProgressReporter progress) {
  HalFile file;
  if (!Storage.openFileForRead("FLASH", source, file)) return false;
  const uint64_t total = file.fileSize64();
  progress.report(ImportPhase::Checking, 0, total);
  uint64_t hash = 1469598103934665603ull, pathHash = 1469598103934665603ull;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(source); *p; ++p)
    pathHash = (pathHash ^ *p) * 1099511628211ull;
  unsigned chunks = 0;
  while (file.available()) {
    int n = file.read(buffer, sizeof(buffer));
    if (n <= 0) return false;
    for (int k = 0; k < n; ++k) hash = (hash ^ buffer[k]) * 1099511628211ull;
    if (!(++chunks % 16)) {
      progress.report(ImportPhase::Checking, file.position(), total);
      delay(1);
    }
  }
  const int n = std::snprintf(destination, length, "/.crosspoint/flashcards/%016llx%016llx",
                              static_cast<unsigned long long>(pathHash), static_cast<unsigned long long>(hash));
  if (n <= 0 || size_t(n) >= length || std::strlen(source) >= sizeof(buffer)) return false;
  std::snprintf(path, sizeof(path), "%s/source.txt", destination);
  if (Storage.exists(path)) {
    const size_t bytes = Storage.readFileToBuffer(path, reinterpret_cast<char*>(buffer), sizeof(buffer));
    if (bytes != std::strlen(source) || std::memcmp(buffer, source, bytes)) return false;
  }
  return true;
}
bool Catalog::findImported(const char* source, char* destination, size_t length) {
  HalFile folder = Storage.open("/.crosspoint/flashcards");
  if (!folder) return false;
  bool found = false;
  int64_t newest = -1;
  char name[40];
  for (auto entry = folder.openNextFile(); entry; entry = folder.openNextFile()) {
    if (!entry.isDirectory()) continue;
    entry.getName(name, sizeof(name));
    if (std::strlen(name) != 32 || std::strspn(name, "0123456789abcdef") != 32) continue;
    std::snprintf(path, sizeof(path), "/.crosspoint/flashcards/%s/source.txt", name);
    const size_t bytes = Storage.readFileToBuffer(path, reinterpret_cast<char*>(buffer), sizeof(buffer));
    if (bytes != std::strlen(source) || std::memcmp(buffer, source, bytes)) continue;
    int64_t at = 0;
    std::snprintf(path, sizeof(path), "/.crosspoint/flashcards/%s/revlog.dat", name);
    HalFile log;
    if (Storage.openFileForRead("FLASH", path, log) && log.size() >= 32) {
      const uint32_t offset = log.size() / 32 * 32 - 32;
      if (log.seek(offset) && log.read(buffer, 32) == 32) std::memcpy(&at, buffer + 8, 8);
    }
    if (found && at < newest) continue;
    const int n = std::snprintf(destination, length, "/.crosspoint/flashcards/%s", name);
    if (n <= 0 || size_t(n) >= length) return false;
    found = true;
    newest = at;
  }
  return found;
}
bool Catalog::describe(const char* directory, DeckInfo& info, bool counts) {
  std::snprintf(path, sizeof(path), "%s/meta.dat", directory);
  HalFile meta;
  if (!Storage.openFileForRead("FLASH", path, meta)) return false;
  std::snprintf(path, sizeof(path), "%s/deck.dat", directory);
  HalFile notes;
  if (!Storage.openFileForRead("FLASH", path, notes)) return false;
  FileSource metadata(meta), noteData(notes);
  if (!deck.openMeta(metadata) || !deck.openDeck(noteData)) return false;
  info.name = deck.meta().name;
  for (const char* ext : {".csv", ".apkg", ".tsv", ".txt", ".colpkg", ".anki2", ".anki21"})
    if (FsHelpers::checkFileExtension(info.name, ext)) {
      info.name.resize(info.name.size() - std::strlen(ext));
      break;
    }
  info.directory = directory;
  info.total = deck.noteCount();
  info.due = info.fresh = 0;
  info.ready = false;
  if (!counts) {
    info.ready = true;
    return true;
  }
  std::snprintf(path, sizeof(path), "%s/cards.dat", directory);
  HalFile cards;
  if (!Storage.openFileForRead("FLASH", path, cards)) return false;
  FileSource cardFile(cards);
  BlockSource cardData(cardFile, buffer, sizeof(buffer));
  const time_t now = time(nullptr);
  const int today = study::dayNumber(deck.meta(), now);
  const int64_t dayStart = deck.meta().collectionCreated + int64_t(today) * 86400;
  const int minute = int(std::clamp<int64_t>((now - dayStart) / 60, 0, 1439));
  for (int i = 0; i < info.total; ++i) {
    study::CardState card;
    if (!deck.loadCard(cardData, i, card)) return false;
    if (card.state == static_cast<uint8_t>(study::State::Suspended)) continue;
    if (card.isNew())
      ++info.fresh;
    else if (study::Scheduler::isDue(card, today, minute))
      ++info.due;
    if (!(i % 128)) delay(1);
  }
  info.fresh = std::min(info.fresh, std::max<int>(0, deck.meta().newPerDay));
  info.due = std::min({info.due, std::max<int>(0, deck.meta().reviewsPerDay), 256});
  info.fresh = std::min(info.fresh, 256 - info.due);
  info.ready = true;
  return true;
}
}  // namespace flashcards
