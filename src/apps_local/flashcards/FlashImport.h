#pragma once
#include <HalMemory.h>
#include <HalStorage.h>

#include "AnkiSqlite.h"
#include "FlashMedia.h"
namespace flashcards {
enum class ImportResult { Busy, Complete, Failed };
class Importer final : private ankisql::Source {
 public:
  ~Importer();
  void setProgressReporter(ProgressReporter value) {
    progress = value;
    media.setProgressReporter(value);
  }
  bool begin(const char* source, const char* destination, const char* title);
  ImportResult step();
  uint32_t count() const { return imported; }
  uint32_t skippedCount() const { return skipped; }

 private:
  ProgressReporter progress;
  HalFile input, index, notes, cards, output;
  FlashMedia media;
  char sourcePath[256]{};
  ankisql::Database db{*this};
  ankisql::Cursor cursor;
  uint32_t notesRoot = 0, imported = 0, skipped = 0;
  uint64_t sourceBytes = 0;
  bool sqlite = false, joining = false, finished = false;
  bool ownsFiles = false, published = false, rowReady = false;
  enum class CsvState { Field, Quoted, QuoteEnd };
  enum class CsvRead { Pending, Row, End };
  CsvState csvState = CsvState::Field;
  size_t csvAt = 0;
  int csvFields = 1;
  bool csvAny = false, csvTooLong = false;
  char separator = 0;
  char directory[112]{}, name[128]{}, path[160]{}, destination[160]{};
  static constexpr size_t kFieldsBytes = 256 * 1024, kFaceBytes = 65536;
  static constexpr size_t kEncodedBytes = 2 * kFaceBytes + 32;
  static constexpr size_t kWorkspaceBytes = kFieldsBytes + 2 * kFaceBytes + kEncodedBytes;
  // Complete HTML/cloze fields need bounded random access; allocate once in
  // PSRAM during import, never on the task stack or the internal-only heap.
  HalMemory::PsramBuffer workspace;
  char* fields = nullptr;
  char* front = nullptr;
  char* back = nullptr;
  uint8_t* encoded = nullptr;
  bool read(uint64_t, void*, size_t) override;
  uint64_t size() const override;
  bool extract(const char*);
  CsvRead readCsv();
  bool readAnki(int64_t&);
  bool writeCard(int64_t);
  bool finalize();
  bool metadata();
  void close();
  void cleanFiles();
};
}  // namespace flashcards
