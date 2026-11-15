#include <cassert>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "../../../src/apps_local/reading/CrossInkStatsImport.h"
#include "../../../src/apps_local/reading/ReadingStatsProjection.h"
#include "../../../src/apps_local/reading/ReadingStatsStore.h"
using namespace readingstats;
namespace readingstats {
int today() { return dayIndex({2026, 11, 7}); }
}  // namespace readingstats
namespace {
void put(std::vector<uint8_t>& out, size_t at, uint32_t n, int count = 4) {
  while (count--) {
    out[at++] = n & 255;
    n >>= 8;
  }
}
std::vector<uint8_t> global(int version) {
  const size_t sizes[] = {0, 13, 17, 159};
  std::vector<uint8_t> out(sizes[version]);
  out[0] = version;
  put(out, 1, 8);
  put(out, 5, 3600);
  put(out, 9, 100);
  if (version >= 2) put(out, 13, 2);
  if (version == 3) {
    put(out, 61, today() - crossink::kEpochDay);
    out[65] = 7;
    put(out, 157, 4, 2);
  }
  return out;
}
std::vector<uint8_t> book(int version) {
  const size_t sizes[] = {0, 11, 12, 16, 69, 73};
  std::vector<uint8_t> out(sizes[version]);
  out[0] = version;
  put(out, 1, 3, 2);
  put(out, 3, 1200);
  put(out, 7, 50);
  if (version >= 2) out[11] = 1;
  if (version >= 4) {
    put(out, 21, 2026, 2);
    out[23] = 11;
    out[24] = 5;
  }
  return out;
}
std::string cache(const std::string& path, bool legacy = false, const char* prefix = "epub") {
  return "/.crosspoint/" + std::string(prefix) + "_" +
         std::to_string(legacy ? std::hash<std::string>{}(path) : library::clixPathHash(path.data(), path.size()));
}
cloud::Result run(crossink::Importer& importer) {
  auto result = importer.begin();
  for (int n = 0; n < 20 && result == cloud::Result::Busy; ++n) result = importer.step();
  return result;
}
}  // namespace
int main() {
  crossink::Global decoded;
  Book parsed;
  for (int v = 1; v <= 3; ++v) {
    auto bytes = global(v);
    assert(crossink::decodeGlobal(bytes.data(), bytes.size(), decoded));
    assert(decoded.sessions == 8 && decoded.seconds == 3600 && decoded.pages == 100);
    assert(!crossink::decodeGlobal(bytes.data(), bytes.size() - 1, decoded));
  }
  auto bytes = global(3);
  put(bytes, 61, 999999);
  assert(!crossink::decodeGlobal(bytes.data(), bytes.size(), decoded));
  for (int v = 1; v <= 5; ++v) {
    auto bytes = book(v);
    assert(crossink::decodeBook(bytes.data(), bytes.size(), parsed));
    assert(parsed.sessions == 3 && parsed.seconds == 1200 && parsed.pages == 50);
    assert(parsed.completed == (v >= 2));
    assert(!crossink::decodeBook(bytes.data(), bytes.size() - 1, parsed));
  }
  testStorage = {};
  library::testBooks = {"/books/uno.epub", "/books/dos.xtc"};
  testStorage.files["/.crosspoint/global_stats.bin"] = global(3);
  testStorage.files[cache(library::testBooks[0]) + "/stats_v5.bin"] = book(5);
  testStorage.files[cache(library::testBooks[1], true, "xtc") + "/stats.bin"] = book(2);
  testStorage.files[kJournalPath] = std::vector<uint8_t>(std::begin(kJournalHeader), std::end(kJournalHeader));
  const auto originals = testStorage.files;
  {
    crossink::Importer importer;
    assert(run(importer) == cloud::Result::Success);
    assert(importer.matched() == 2);
  }
  cloud::Reader reader;
  assert(reader.open(cloud::SnapshotKind::CrossInk) == cloud::Result::Success);
  assert(reader.data().summary.seconds == 3600 && reader.data().books == 2 && reader.data().finished == 2);
  assert(reader.data().days == 0 && reader.data().summary.onDay(today()) == 0);
  assert(reader.activeOn(today()) && reader.activeOn(today() - 2) && !reader.knownOn(today()));
  {
    cloud::Projection projection;
    assert(projection.open(&reader, today(), 1) == cloud::Result::Success);
    assert(projection.data().summary.seconds == 3600 && projection.data().summary.completed == 2);
    assert(projection.data().summary.daysRead() == 3 && projection.data().summary.longest == 4);
    assert(projection.daySeconds(today()) == 0);
    PeriodStats year;
    setPeriod(year, Period::Year, today(), false);
    assert(!projection.annualTimeKnown(year));
    assert(projection.recordedYears().at(0) == 2026);
  }
  {
    const auto savedJournal = testStorage.files[kJournalPath];
    Record local;
    assert(reader.at(0, local.book));
    local.book.seconds = local.seconds = 600;
    local.book.sessions = 1;
    local.localDay = today();
    local.epoch = int64_t(today()) * 86400;
    auto& journal = testStorage.files[kJournalPath];
    journal.resize(16 + kRecordBytes);
    encode(local, journal.data() + 16);
    cloud::Projection projection;
    assert(projection.open(&reader, today(), 1) == cloud::Result::Success);
    assert(projection.data().summary.seconds == 4200);
    PeriodStats year;
    setPeriod(year, Period::Year, today(), false);
    bool anyKnown = false;
    assert(projection.project(year, &anyKnown) && anyKnown && year.seconds == 600);
    assert(!projection.annualTimeKnown(year));
    projection.close();
    local.localDay = dayIndex({2023, 6, 1});
    local.epoch = int64_t(local.localDay) * 86400;
    encode(local, journal.data() + 16);
    assert(projection.open(&reader, today(), 1) == cloud::Result::Success);
    setPeriod(year, Period::Year, local.localDay, false);
    assert(projection.project(year, &anyKnown) && anyKnown && year.seconds == 600);
    assert(!projection.annualTimeKnown(year));
    assert(projection.recordedYears().at(1) == 2023);
    projection.close();
    testStorage.files[kJournalPath] = savedJournal;
  }
  {
    Book imported;
    bool found = false;
    for (uint32_t n = 0; n < reader.data().books; ++n) {
      assert(reader.at(n, imported));
      if (imported.finishedDay) {
        found = true;
        break;
      }
    }
    assert(found);
    const auto originalJournal = testStorage.files[kJournalPath];
    Record local;
    local.book = imported;
    local.book.finishedDay = today();
    local.book.finishedEpoch = int64_t(today()) * 86400 + 3600;
    local.epoch = local.book.finishedEpoch;
    local.localDay = today();
    local.kind = RecordKind::Completion;
    local.firstCompletion = true;
    auto& journal = testStorage.files[kJournalPath];
    journal.resize(16 + kRecordBytes);
    encode(local, journal.data() + 16);
    cloud::Projection view;
    assert(view.open(&reader, today(), 1) == cloud::Result::Success);
    Book combined;
    assert(view.find(imported.document, combined));
    assert(combined.finishedDay == today() && combined.finishedEpoch == local.epoch);
    view.close();
    local.kind = RecordKind::Reopen;
    local.firstCompletion = false;
    local.book.manuallyUnfinished = true;
    local.book.completed = false;
    local.book.progressBp = 9999;
    local.book.finishedEpoch = 0;
    local.book.finishedDay = 0;
    encode(local, journal.data() + 16);
    assert(view.open(&reader, today(), 1) == cloud::Result::Success);
    assert(view.find(imported.document, combined));
    assert(!combined.completed && combined.finishedEpoch == 0 && combined.finishedDay == 0);
    view.close();
    testStorage.files[kJournalPath] = originalJournal;
  }
  reader.close();
  {
    crossink::Importer importer;
    assert(run(importer) == cloud::Result::Success);
  }
  assert(reader.open(cloud::SnapshotKind::CrossInk) == cloud::Result::Success);
  assert(reader.data().generation == 2 && reader.data().summary.seconds == 3600);
  reader.close();
  {
    crossink::Importer importer;
    assert(importer.begin() == cloud::Result::Busy);
    assert(importer.step() == cloud::Result::Busy);
    importer.cancel();
  }
  assert(reader.open(cloud::SnapshotKind::CrossInk) == cloud::Result::Success && reader.data().generation == 2);
  reader.close();
  testStorage.writeLimit = 1;
  {
    crossink::Importer importer;
    assert(run(importer) == cloud::Result::StorageError);
  }
  testStorage.writeLimit = -1;
  assert(reader.open(cloud::SnapshotKind::CrossInk) == cloud::Result::Success && reader.data().generation == 2);
  reader.close();
  for (const auto& [name, data] : originals) assert(testStorage.files[name] == data);
  failPsram = true;
  {
    crossink::Importer importer;
    assert(importer.begin() == cloud::Result::LowMemory);
  }
  failPsram = false;
  // A corrupt primary falls back without modifying or renaming the source.
  testStorage.files["/.crosspoint/global_stats.bin.bak"] = global(2);
  testStorage.files["/.crosspoint/global_stats.bin"] = {99};
  {
    crossink::Importer importer;
    assert(run(importer) == cloud::Result::Success);
  }
  assert(testStorage.files["/.crosspoint/global_stats.bin"] == std::vector<uint8_t>{99});
  std::puts(
      "CrossInk codec, import, idempotence, cancellation, write-failure recovery, PSRAM failure and source "
      "preservation passed.");
}
