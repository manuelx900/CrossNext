#include "FlashImport.h"

#include <Arduino.h>
#include <FsHelpers.h>
#include <HalMemory.h>
#include <Logging.h>
#include <ZipFile.h>
#include <strings.h>
#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "../study/StudyFsrs.h"
#include "FlashText.h"

namespace flashcards {
namespace {
constexpr uint64_t kMaxCollection = 256ull * 1024 * 1024;
void put(uint8_t* p, uint64_t v, int n) {
  for (int k = 0; k < n; ++k) p[k] = uint8_t(v >> (8 * k));
}
class BoundedSink final : public Print {
 public:
  BoundedSink(HalFile& f, ProgressReporter reporter, uint64_t maximum)
      : file(f), progress(reporter), expected(maximum) {}
  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t* p, size_t n) override {
    if (total + n > kMaxCollection) return 0;
    const size_t written = file.write(p, n);
    total += written;
    if (total - lastYield >= 8192) {
      progress.report(ImportPhase::Unpacking, total, expected);
      delay(1);
      lastYield = total;
    }
    return written;
  }

 private:
  HalFile& file;
  ProgressReporter progress;
  uint64_t expected;
  uint64_t total = 0, lastYield = 0;
};
void* allocate(void*, size_t n) {
  auto memory = HalMemory::allocatePsram(n);
  if (!memory) LOG_ERR("FLASH", "OOM: zstd %u PSRAM bytes", unsigned(n));
  return memory.release();
}
void release(void*, void* p) { HalMemory::PsramDeleter{}(static_cast<uint8_t*>(p)); }
}  // namespace
Importer::~Importer() {
  close();
  cleanFiles();
}
void Importer::cleanFiles() {
  if (!ownsFiles) return;
  for (const auto* file :
       {".index", ".notes", ".collection", ".sqlite", "cards.dat.part", "deck.dat.part", "meta.dat.part"}) {
    std::snprintf(path, sizeof(path), "%s/%s", directory, file);
    if (Storage.exists(path) && !Storage.remove(path)) LOG_ERR("FLASH", "Could not remove import scratch %s", path);
  }
  if (!published) {
    for (const auto* file : {"deck.dat", "cards.dat", "images.dat", "images.zoom.dat"}) {
      std::snprintf(path, sizeof(path), "%s/%s", directory, file);
      if (Storage.exists(path) && !Storage.remove(path)) LOG_ERR("FLASH", "Could not remove unpublished deck %s", path);
    }
  }
  ownsFiles = false;
}
void Importer::close() {
  input.close();
  index.close();
  notes.close();
  cards.close();
  output.close();
}
bool Importer::read(uint64_t offset, void* out, size_t n) {
  return input.seek64(offset) && input.read(out, n) == int(n);
}
uint64_t Importer::size() const { return sourceBytes; }
bool Importer::extract(const char* source) {
  std::snprintf(path, sizeof(path), "%s/.collection", directory);
  {
    const std::string archivePath(source);
    ZipFile zip(archivePath);
    size_t inflated = 0;
    static constexpr const char* entries[] = {"collection.anki21b", "collection.anki21", "collection.anki2"};
    const char* selected = nullptr;
    for (const auto* entry : entries)
      if (zip.getInflatedFileSize(entry, &inflated)) {
        selected = entry;
        break;
      }
    if (!selected || inflated > kMaxCollection || !Storage.openFileForWrite("FLASH", path, output)) return false;
    progress.report(ImportPhase::Unpacking, 0, inflated);
    BoundedSink sink(output, progress, inflated);
    const bool ok = zip.readFileToStream(selected, sink, 4096);
    output.flush();
    output.close();
    if (!ok) return false;
  }
  if (!Storage.openFileForRead("FLASH", path, input)) return false;
  uint8_t signature[4];
  if (input.read(signature, 4) != 4) return false;
  input.seekSet(0);
  if (std::memcmp(signature, "\x28\xb5\x2f\xfd", 4)) return true;
  std::snprintf(path, sizeof(path), "%s/.sqlite", directory);
  if (!Storage.openFileForWrite("FLASH", path, output)) return false;
  progress.report(ImportPhase::Decompressing, 0, input.fileSize64());
  // Zstd owns these PSRAM blocks until freeDStream. A large collection window
  // cannot fall back into the reader's internal heap; cap it at four MiB.
  ZSTD_DStream* stream = ZSTD_createDStream_advanced({allocate, release, nullptr});
  if (!stream) return false;
  bool ok =
      !ZSTD_isError(ZSTD_initDStream(stream)) && !ZSTD_isError(ZSTD_DCtx_setParameter(stream, ZSTD_d_windowLogMax, 22));
  size_t remaining = 1;
  uint64_t total = 0;
  const uint64_t compressedSize = input.fileSize64();
  while (ok && input.available()) {
    const int n = input.read(encoded, 4096);
    if (n <= 0) {
      ok = false;
      break;
    }
    ZSTD_inBuffer in{encoded, size_t(n), 0};
    bool drain = true;
    while (ok && (in.pos < in.size || drain)) {
      ZSTD_outBuffer out{encoded + 4096, 4096, 0};
      const size_t before = in.pos;
      remaining = ZSTD_decompressStream(stream, &out, &in);
      total += out.pos;
      if (ZSTD_isError(remaining) || total > kMaxCollection || output.write(encoded + 4096, out.pos) != out.pos ||
          (!out.pos && in.pos == before && in.pos < in.size))
        ok = false;
      drain = out.pos == out.size && remaining != 0;
      progress.report(ImportPhase::Decompressing, input.position(), compressedSize);
      delay(1);
    }
  }
  ZSTD_freeDStream(stream);
  output.flush();
  output.close();
  input.close();
  if (!ok || remaining != 0) return false;
  return Storage.openFileForRead("FLASH", path, input);
}
bool Importer::begin(const char* source, const char* destination, const char* title) {
  if (!source || !destination || std::strlen(destination) >= sizeof(directory) ||
      std::strlen(source) >= sizeof(sourcePath))
    return false;
  if (!workspace) workspace = HalMemory::allocatePsram(kWorkspaceBytes);
  if (!workspace) {
    LOG_ERR("FLASH", "OOM: import text workspace %u PSRAM bytes", unsigned(kWorkspaceBytes));
    return false;
  }
  fields = reinterpret_cast<char*>(workspace.get());
  front = fields + kFieldsBytes;
  back = front + kFaceBytes;
  encoded = reinterpret_cast<uint8_t*>(back + kFaceBytes);
  std::snprintf(sourcePath, sizeof(sourcePath), "%s", source);
  std::snprintf(directory, sizeof(directory), "%s", destination);
  std::snprintf(name, sizeof(name), "%s", title ? title : "");
  if (!Storage.ensureDirectoryExists(directory)) return false;
  std::snprintf(path, sizeof(path), "%s/meta.dat", directory);
  if (Storage.exists(path)) return false;
  ownsFiles = true;
  sqlite = FsHelpers::checkFileExtension(std::string_view(source), ".apkg") ||
           FsHelpers::checkFileExtension(std::string_view(source), ".colpkg") ||
           FsHelpers::checkFileExtension(std::string_view(source), ".anki2") ||
           FsHelpers::checkFileExtension(std::string_view(source), ".anki21");
  if (FsHelpers::checkFileExtension(std::string_view(source), ".apkg") ||
      FsHelpers::checkFileExtension(std::string_view(source), ".colpkg")) {
    if (!extract(source)) return false;
  } else if (!Storage.openFileForRead("FLASH", source, input))
    return false;
  sourceBytes = input.fileSize64();
  if (sourceBytes > kMaxCollection) return false;
  if (sqlite) {
    uint32_t cardsRoot;
    if (!db.open() || !db.table("notes", notesRoot) || !db.table("cards", cardsRoot) || !cursor.begin(db, cardsRoot))
      return false;
  } else {
    if (FsHelpers::checkFileExtension(std::string_view(source), ".tsv")) separator = '\t';
    uint8_t bom[3];
    if (input.read(bom, 3) != 3 || std::memcmp(bom, "\xef\xbb\xbf", 3)) input.seekSet(0);
  }
  std::snprintf(path, sizeof(path), "%s/.index", directory);
  if (!Storage.openFileForWrite("FLASH", path, index)) return false;
  std::snprintf(path, sizeof(path), "%s/.notes", directory);
  if (!Storage.openFileForWrite("FLASH", path, notes)) return false;
  std::snprintf(path, sizeof(path), "%s/cards.dat.part", directory);
  if (!Storage.openFileForWrite("FLASH", path, cards)) return false;
  const bool archive = FsHelpers::checkFileExtension(std::string_view(source), ".apkg") ||
                       FsHelpers::checkFileExtension(std::string_view(source), ".colpkg");
  return media.begin(archive ? sourcePath : nullptr, directory);
}
Importer::CsvRead Importer::readCsv() {
  rowReady = false;
  bool ended = false;
  for (int budget = 0; budget < 4096 && input.available(); ++budget) {
    const int value = input.read();
    if (value < 0) break;
    char c = char(value);
    csvAny = true;
    if (csvState == CsvState::QuoteEnd) {
      if (c == '"') {
        csvState = CsvState::Quoted;
      } else {
        csvState = CsvState::Field;
      }
    } else if (csvState == CsvState::Quoted) {
      if (c == '"') {
        csvState = CsvState::QuoteEnd;
        continue;
      }
    } else if (c == '"' && (csvAt == 0 || fields[csvAt - 1] == 31)) {
      csvState = CsvState::Quoted;
      continue;
    }
    if (csvState == CsvState::Field) {
      if (c == '\n') {
        ended = true;
        break;
      }
      if (c == '\r') continue;
      if (!separator && (c == '\t' || c == ',' || c == ';')) separator = c;
      if (separator && c == separator) {
        c = 31;
        ++csvFields;
      }
    }
    if (csvAt + 1 < kFieldsBytes)
      fields[csvAt++] = c;
    else
      csvTooLong = true;
  }
  delay(1);
  if (!ended && input.available()) return CsvRead::Pending;
  if (!csvAny) return CsvRead::End;
  fields[csvAt] = 0;
  const bool invalid = csvTooLong || csvState == CsvState::Quoted || csvFields < 2;
  csvAt = 0;
  csvFields = 1;
  csvAny = csvTooLong = false;
  csvState = CsvState::Field;
  front[0] = back[0] = 0;
  media.prepare(nullptr, nullptr);
  if (fields[0] == '#') {
    if (!std::strncmp(fields, "#separator:tab", 14)) separator = '\t';
    if (!std::strncmp(fields, "#separator:comma", 16)) separator = ',';
    if (!std::strncmp(fields, "#separator:semicolon", 20)) separator = ';';
    return CsvRead::Row;
  }
  char* split = std::strchr(fields, 31);
  if (invalid || !split) {
    ++skipped;
    return CsvRead::Row;
  }
  *split++ = 0;
  if (char* end = std::strchr(split, 31)) *end = 0;
  if ((!strcasecmp(fields, "front") && !strcasecmp(split, "back")) ||
      (!strcasecmp(fields, "question") && !strcasecmp(split, "answer")))
    return CsvRead::Row;
  if (!flashtext::plain(fields, front, kFaceBytes) || !flashtext::plain(split, back, kFaceBytes)) {
    front[0] = back[0] = 0;
    media.prepare(nullptr, nullptr);
    ++skipped;
  } else {
    rowReady = true;
  }
  return CsvRead::Row;
}
bool Importer::readAnki(int64_t& id) {
  rowReady = false;
  ankisql::Row card, note;
  const auto status = cursor.next(card);
  if (status == ankisql::Result::End) {
    finished = true;
    return false;
  }
  if (status == ankisql::Result::Error) return false;
  int64_t nid, ordinal;
  if (!card.integer(1, nid) || !card.integer(3, ordinal) || nid < 0 || ordinal < 0) return false;
  front[0] = back[0] = 0;
  media.prepare(nullptr, nullptr);
  id = int64_t(card.id);
  if (!db.find(notesRoot, uint64_t(nid), note) || !note.text(6, fields, kFieldsBytes)) {
    ++skipped;
    return true;
  }
  char* split = std::strchr(fields, 31);
  if (!split) {
    ++skipped;
    return true;
  }
  const char* identifierEnd = std::strchr(fields, 31);
  const bool occlusion = identifierEnd && identifierEnd - fields > 36 &&
                         std::strspn(fields, "0123456789abcdef") == 32 &&
                         (!std::strncmp(fields + 32, "-ao-", 4) || !std::strncmp(fields + 32, "-oa-", 4));
  unsigned separators = 0;
  for (const char* c = fields; *c; ++c) separators += *c == 31;
  if (occlusion && separators == 10) {
    ++skipped;
    return true;
  }
  *split++ = 0;
  if (char* end = std::strchr(split, 31)) *end = 0;
  const auto reject = [this]() {
    front[0] = back[0] = 0;
    media.prepare(nullptr, nullptr);
    ++skipped;
  };
  if (std::strstr(fields, "{{c")) {
    // Resolve holes before math removes grouping braces; keep Back Extra.
    char* resolved = reinterpret_cast<char*>(encoded);
    if (!flashtext::cloze(fields, unsigned(ordinal + 1), true, resolved, kEncodedBytes)) {
      reject();
      return true;
    }
    const size_t length = std::strlen(resolved), extra = std::strlen(split);
    if (extra) {
      if (length + 4 + extra >= kEncodedBytes) {
        reject();
        return true;
      }
      std::memcpy(resolved + length, "<br>", 4);
      std::memcpy(resolved + length + 4, split, extra + 1);
    }
    media.prepareFace(resolved, true);
    if (!flashtext::plain(resolved, back, kFaceBytes) ||
        !flashtext::cloze(fields, unsigned(ordinal + 1), false, resolved, kEncodedBytes)) {
      reject();
      return true;
    }
    media.prepareFace(resolved, false);
    if (!flashtext::plain(resolved, front, kFaceBytes)) {
      reject();
      return true;
    }
  } else {
    media.prepare(fields, split);
    if (!flashtext::plain(fields, front, kFaceBytes) || !flashtext::plain(split, back, kFaceBytes)) {
      reject();
      return true;
    }
    if (ordinal == 1) {
      media.reverse();
      const size_t questionBytes = std::strlen(front) + 1;
      std::memcpy(encoded, front, questionBytes);
      std::memcpy(front, back, std::strlen(back) + 1);
      std::memcpy(back, encoded, questionBytes);
    }
  }
  rowReady = true;
  return true;
}
bool Importer::writeCard(int64_t id) {
  const size_t question = std::strlen(front), answer = std::strlen(back);
  if ((!question && !media.question()) || (!answer && !media.answer())) {
    ++skipped;
    return true;
  }
  if (question + answer + 18 > kEncodedBytes) {
    ++skipped;
    return true;
  }
  if (imported >= 100000) return false;
  size_t at = 0;
  for (int field = 0; field < 8; ++field) {
    const char* value = field == 0 ? front : field == 2 ? back : "";
    const size_t length = std::strlen(value) + (field == 4 ? 2 : 0);
    put(encoded + at, length, 2);
    at += 2;
    std::memcpy(encoded + at, value, std::strlen(value));
    at += std::strlen(value);
    if (field == 4) {
      encoded[at++] = 0;
      encoded[at++] = 0;
    }
  }
  uint8_t offset[4];
  put(offset, notes.position(), 4);
  if (index.write(offset, 4) != 4 || notes.write(encoded, at) != at) return false;
  std::memset(encoded, 0, 32);
  put(encoded, uint64_t(id), 8);
  if (cards.write(encoded, 32) != 32 || !media.append()) return false;
  ++imported;
  return true;
}
bool Importer::metadata() {
  std::memset(encoded, 0, 164);
  std::memcpy(encoded, "XSTUDYM", 7);
  put(encoded + 8, 3, 2);
  for (int n = 0; n < study::kNumParams; ++n) std::memcpy(encoded + 12 + n * 4, &study::kDefaultParams[n], 4);
  const float retention = .9f, one = 1.f, ten = 10.f;
  std::memcpy(encoded + 88, &retention, 4);
  put(encoded + 92, 36500, 4);
  put(encoded + 96, 20, 4);
  put(encoded + 100, 200, 4);
  const auto now = std::time(nullptr);
  put(encoded + 104, uint64_t(now > 0 ? now / 86400 * 86400 : 0), 8);
  encoded[112] = 4;
  encoded[113] = 2;
  encoded[114] = 1;
  std::memcpy(encoded + 115, &one, 4);
  std::memcpy(encoded + 119, &ten, 4);
  std::memcpy(encoded + 139, &ten, 4);
  encoded[163] = uint8_t(std::strlen(name));
  std::snprintf(path, sizeof(path), "%s/meta.dat.part", directory);
  if (!Storage.openFileForWrite("FLASH", path, output) || output.write(encoded, 164) != 164 ||
      output.write(name, std::strlen(name)) != std::strlen(name))
    return false;
  output.flush();
  output.close();
  std::snprintf(destination, sizeof(destination), "%s/meta.dat", directory);
  return Storage.replaceFile(path, destination);
}
bool Importer::finalize() {
  progress.report(ImportPhase::Saving);
  if (!imported) return false;
  put(encoded, notes.position(), 4);
  if (index.write(encoded, 4) != 4) return false;
  input.close();
  index.flush();
  notes.flush();
  cards.flush();
  index.close();
  notes.close();
  cards.close();
  std::snprintf(path, sizeof(path), "%s/deck.dat.part", directory);
  if (!Storage.openFileForWrite("FLASH", path, output)) return false;
  std::memset(encoded, 0, 16);
  std::memcpy(encoded, "XSTUDYD", 7);
  put(encoded + 8, 4, 2);
  encoded[10] = 8;
  put(encoded + 12, imported, 4);
  if (output.write(encoded, 16) != 16) return false;
  std::snprintf(path, sizeof(path), "%s/.index", directory);
  if (!Storage.openFileForRead("FLASH", path, input)) return false;
  const uint64_t base = 16 + uint64_t(imported + 1) * 4;
  for (uint32_t n = 0; n <= imported; ++n) {
    uint8_t bytes[4];
    if (input.read(bytes, 4) != 4) return false;
    uint32_t off =
        uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) | (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
    if (base + off > UINT32_MAX) return false;
    put(bytes, base + off, 4);
    if (output.write(bytes, 4) != 4) return false;
    if (!(n % 512)) {
      progress.report(ImportPhase::Saving);
      delay(1);
    }
  }
  input.close();
  std::snprintf(path, sizeof(path), "%s/.notes", directory);
  if (!Storage.openFileForRead("FLASH", path, input)) return false;
  while (input.available()) {
    const int n = input.read(encoded, 8192);
    if (n <= 0 || output.write(encoded, n) != size_t(n)) return false;
    progress.report(ImportPhase::Saving, input.position(), input.fileSize64());
    delay(1);
  }
  input.close();
  output.flush();
  output.close();
  std::snprintf(path, sizeof(path), "%s/deck.dat.part", directory);
  std::snprintf(destination, sizeof(destination), "%s/deck.dat", directory);
  if (!Storage.replaceFile(path, destination)) return false;
  std::snprintf(path, sizeof(path), "%s/cards.dat.part", directory);
  std::snprintf(destination, sizeof(destination), "%s/cards.dat", directory);
  if (!Storage.replaceFile(path, destination) || !media.finish(imported) || !metadata()) return false;
  published = true;
  cleanFiles();
  return true;
}
ImportResult Importer::step() {
  progress.report(ImportPhase::Cards, imported + skipped);
  if (joining) return finalize() ? ImportResult::Complete : ImportResult::Failed;
  for (int n = 0; n < (media.hasMedia() ? 1 : 8); ++n) {
    int64_t id = imported + skipped + 1;
    if (sqlite) {
      if (!readAnki(id)) {
        if (!finished) return ImportResult::Failed;
        joining = true;
        return ImportResult::Busy;
      }
    } else {
      switch (readCsv()) {
        case CsvRead::Pending:
          return ImportResult::Busy;
        case CsvRead::End:
          joining = true;
          return ImportResult::Busy;
        case CsvRead::Row:
          break;
      }
    }
    if (rowReady && !writeCard(id)) return ImportResult::Failed;
  }
  return ImportResult::Busy;
}
}  // namespace flashcards
