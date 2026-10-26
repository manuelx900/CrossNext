#include <Xtc/XtcParser.h>

#include <cstdio>
#include <cstring>
#include <limits>

#include "../../src/activities/reader/ReaderSession.h"
#include "../../src/apps_local/reading/ReadingStatsStore.h"

namespace {
int checks = 0, failures = 0;
#define CHECK(condition)                                       \
  do {                                                         \
    ++checks;                                                  \
    if (!(condition)) {                                        \
      ++failures;                                              \
      std::printf("FAIL line %d: %s\n", __LINE__, #condition); \
    }                                                          \
  } while (false)

std::vector<uint8_t> fixture(bool gray, uint64_t metadataOffset) {
  constexpr uint32_t table = 384, pages = 4, width = 24, height = 16;
  const uint32_t payload = width * height / 8 * (gray ? 2 : 1);
  const uint32_t pageSize = sizeof(xtc::XtgPageHeader) + payload;
  std::vector<uint8_t> bytes(table + pages * sizeof(xtc::PageTableEntry) + pages * pageSize, 0);
  xtc::XtcHeader header{};
  header.magic = gray ? xtc::XTCH_MAGIC : xtc::XTC_MAGIC;
  header.versionMajor = 1;
  header.pageCount = pages;
  header.hasMetadata = 1;
  header.metadataOffset = metadataOffset;
  header.pageTableOffset = table;
  header.dataOffset = table + pages * sizeof(xtc::PageTableEntry);
  std::memcpy(bytes.data(), &header, sizeof(header));
  // Zero-offset files retain the legacy fixed metadata location.
  const uint64_t at = metadataOffset ? metadataOffset : sizeof(header);
  if (at <= table && 192 <= table - at) {
    std::memcpy(bytes.data() + at, "Reading test", 13);
    std::memcpy(bytes.data() + at + 128, "Test author", 12);
  }
  for (uint32_t n = 0; n < pages; ++n) {
    xtc::PageTableEntry entry{header.dataOffset + n * pageSize, pageSize, width, height};
    std::memcpy(bytes.data() + table + n * sizeof(entry), &entry, sizeof(entry));
    xtc::XtgPageHeader page{gray ? xtc::XTH_MAGIC : xtc::XTG_MAGIC, width, height, 0, 0, payload, 0};
    std::memcpy(bytes.data() + entry.dataOffset, &page, sizeof(page));
    std::memset(bytes.data() + entry.dataOffset + sizeof(page), 0x55, payload);
  }
  return bytes;
}

void readAndRecord(bool gray, uint64_t metadataOffset) {
  testStorage = {};
  const char* path = gray ? "/book.XTCH" : "/book.XTC";
  testStorage.files[path] = fixture(gray, metadataOffset);
  xtc::XtcParser parser;
  CHECK(parser.open(path) == xtc::XtcError::OK);
  CHECK(parser.getBitDepth() == (gray ? 2 : 1));
  CHECK(parser.getTitle() == "Reading test");
  CHECK(parser.getAuthor() == "Test author");
  uint8_t bitmap[96]{};
  ReaderSession session;
  constexpr int64_t epoch = 1792018700;
  session.onRenderComplete(1000, epoch, 2500);
  CHECK(parser.loadPage(1, bitmap, sizeof(bitmap)) == (gray ? 96u : 48u));
  session.noteTurn(true, true);
  session.onRenderComplete(6000, epoch + 5, 5000);
  CHECK(session.isEmitWorthy());
  CHECK(parser.loadPage(2, bitmap, sizeof(bitmap)) == (gray ? 96u : 48u));
  session.noteTurn(true, true);
  session.onRenderComplete(11000, epoch + 10, 7500);
  CHECK(session.isEmitWorthy());
  CHECK(session.durationSeconds() == 10 && session.pagesRead() == 2);
  CHECK(readingstats::enabledFormat(path, 16));
  CHECK(!readingstats::enabledFormat(path, 7));
  const char* document = "0123456789abcdef0123456789abcdef";
  readingstats::Repository repository;
  CHECK(repository.record(document, parser.getTitle().c_str(), parser.getAuthor().c_str(), session.startTime(),
                          session.durationSeconds(), session.pagesRead(), session.endProgressBp()));
  readingstats::Book book;
  CHECK(repository.loadBook(document, book));
  CHECK(book.seconds == 10 && book.pages == 2 && book.sessions == 1 && book.progressBp == 7500);
  CHECK(!std::strcmp(book.title, "Reading test") && !std::strcmp(book.author, "Test author"));
}
}  // namespace

int main() {
  ReaderSession fast;
  fast.onRenderComplete(1000, 1792018700, 100);
  for (int n = 1; n <= 10; ++n) {
    fast.noteTurn(true, true);
    fast.onRenderComplete(1000 + n * 100, 1792018701, 100 + n);
  }
  CHECK(fast.isEmitWorthy() && fast.pagesRead() == 10 && fast.durationSeconds() == 1);
  fast.noteTurn(false, true);
  fast.onRenderComplete(3000, 1792018702, 100);
  fast.noteTurn(true, false);
  fast.onRenderComplete(4000, 1792018703, 110);
  CHECK(fast.pagesRead() == 10 && fast.durationSeconds() == 1);
  fast.reset();
  fast.onRenderComplete(1000, 0, 100);
  fast.noteTurn(true, true);
  fast.onRenderComplete(2000, 0, 200);
  CHECK(!fast.isEmitWorthy());
  fast.reset();
  fast.onRenderComplete(1000, 1792018700, 100);
  fast.noteTurn(true, true);
  fast.onRenderComplete(1000, 1792018700, 200);
  CHECK(fast.isEmitWorthy() && fast.pagesRead() == 1 && fast.durationSeconds() == 1);
  fast.reset();
  fast.onRenderComplete(0xffffff00u, 1792018700, 100);
  fast.noteTurn(true, true);
  fast.onRenderComplete(744, 1792018701, 200);
  CHECK(fast.pagesRead() == 1 && fast.durationSeconds() == 1);
  fast.reset();
  fast.onRenderComplete(0, 1792018700, 100);
  fast.noteTurn(true, true);
  fast.onRenderComplete(3600000, 1792022300, 200);
  CHECK(fast.durationSeconds() == 1800);
  CHECK(sizeof(ReaderSession) <= 48);
  for (bool gray : {false, true}) {
    for (uint64_t offset : {uint64_t(0), uint64_t(56), uint64_t(128)}) readAndRecord(gray, offset);
    for (uint64_t offset : {uint64_t(1), uint64_t(10000), std::numeric_limits<uint64_t>::max()}) {
      testStorage = {};
      testStorage.files["/broken.xtch"] = fixture(gray, offset);
      xtc::XtcParser parser;
      CHECK(parser.open("/broken.xtch") != xtc::XtcError::OK);
    }
  }
  std::printf("XTC/XTCH reading: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
