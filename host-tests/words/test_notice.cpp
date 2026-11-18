#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "VocabularyNotice.h"

using namespace vocabulary;
static unsigned checks = 0;
#define CHECK(x)                                           \
  do {                                                     \
    ++checks;                                              \
    if (!(x)) {                                            \
      std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); \
      std::exit(1);                                        \
    }                                                      \
  } while (0)
constexpr char id1[] = "0123456789abcdef0123456789abcdef";
constexpr char id2[] = "aabbccddeeff00112233445566778899";
constexpr char id3[] = "abcdef0123456789abcdef0123456789";
constexpr char id4[] = "abcdef0123456789abcdef0123456780";
constexpr char id5[] = "abcdef0123456789abcdef0123456781";
constexpr char pending[] = "/.crosspoint/vocabulary/.notice-accepted.bin";
static void drain(VocabularyNotice& notice) {
  for (unsigned n = 0; notice.busy() || notice.observing(); ++n) {
    CHECK(n < 10000);
    const auto before = testStorage.bytesRead;
    const bool observing = notice.observing();
    if (observing)
      notice.stepObservation();
    else
      notice.step();
    CHECK(testStorage.bytesRead - before <= (observing ? 320u : 120u));
  }
}
int main() {
  CHECK(StorageRecords::ready());
  const std::string originalPath = std::string(kWords) + "/" + id1 + ".dat";
  testStorage.files[originalPath] = {1, 2, 3, 4, 5};
  const auto original = testStorage.files[originalPath];
  {
    VocabularyNotice notice;
    notice.resume();
    notice.beginScan();
    CHECK(notice.observe(id1));
    CHECK(notice.observe(id2));
    CHECK(notice.acknowledge());
    CHECK(Storage.exists(pending));
    notice.close();
    CHECK(Storage.exists(pending));
  }
  {
    // Exit/power loss before any marker: accepted IDs are still recognized.
    VocabularyNotice notice;
    notice.resume();
    CHECK(!notice.observe(id1) && !notice.observe(id2));
    drain(notice);
    CHECK(!Storage.exists(pending));
    notice.beginScan();
    CHECK(!notice.observe(id1) && !notice.observe(id2));
    CHECK(notice.observe(id3));
  }
  {
    // An unacknowledged new error is still new after restarting.
    VocabularyNotice notice;
    notice.resume();
    notice.beginScan();
    CHECK(notice.observe(id3));
    CHECK(notice.acknowledge());
    notice.step();
    CHECK(Storage.exists(pending));
  }
  {
    // Resume after marker creation but before the accepted batch was removed.
    VocabularyNotice notice;
    notice.resume();
    drain(notice);
    notice.beginScan();
    CHECK(!notice.observe(id1) && !notice.observe(id2) && !notice.observe(id3));
    CHECK(notice.observe(id4));
    CHECK(notice.acknowledge());
    testStorage.writeLimit = 0;
    drain(notice);
    CHECK(Storage.exists(pending));
    CHECK(!notice.observe(id4));
  }
  testStorage.writeLimit = -1;
  {
    VocabularyNotice notice;
    notice.resume();
    drain(notice);
    notice.beginScan();
    CHECK(!notice.observe(id4));
    CHECK(notice.observe(id5));
    testStorage.failRenameAt = testStorage.renameCalls + 1;
    CHECK(!notice.acknowledge());
    CHECK(!Storage.exists(pending));
    CHECK(!notice.observe(id5));
  }
  testStorage.failRenameAt = -1;
  {
    VocabularyNotice notice;
    notice.resume();
    notice.beginScan();
    CHECK(notice.observe(id5));
    CHECK(notice.acknowledge());
    drain(notice);
  }
  {
    VocabularyNotice notice;
    notice.resume();
    notice.beginScan();
    CHECK(!notice.observe(id1) && !notice.observe(id2) && !notice.observe(id3) && !notice.observe(id4) &&
          !notice.observe(id5));
  }
  CHECK(testStorage.files[originalPath] == original);
  {
    VocabularyNotice notice;
    notice.beginScan();
    CHECK(notice.observe("ffffffffffffffffffffffffffffffff"));
    testStorage.failClose = true;
    CHECK(!notice.acknowledge());
    CHECK(!Storage.exists(pending));
    testStorage.failClose = false;
  }
  // A large accepted batch keeps one fixed record per maintenance step.
  std::vector<uint8_t> accepted;
  {
    VocabularyNotice notice;
    notice.beginScan();
    char id[kIdBytes];
    for (unsigned n = 0; n < 1000; ++n) {
      std::snprintf(id, sizeof(id), "%032x", n);
      CHECK(notice.observe(id));
    }
    CHECK(notice.acknowledge());
    accepted = testStorage.files[pending];
    drain(notice);
    notice.close();
  }
  // A read-only card keeps the accepted batch usable without blocking a frame.
  testStorage.files[pending] = accepted;
  for (auto it = testStorage.files.begin(); it != testStorage.files.end();) {
    if (it->first.find("/.crosspoint/vocabulary/acknowledged/") == 0)
      it = testStorage.files.erase(it);
    else
      ++it;
  }
  testStorage.writeLimit = 0;
  {
    VocabularyNotice notice;
    notice.resume();
    notice.beginScan();
    drain(notice);
    CHECK(Storage.exists(pending));
    char id[kIdBytes];
    std::snprintf(id, sizeof(id), "%032x", 999);
    CHECK(!notice.observe(id) && notice.observing());
    unsigned steps = 0;
    while (notice.observing()) {
      const auto before = testStorage.bytesRead;
      CHECK(!notice.stepObservation());
      CHECK(testStorage.bytesRead - before <= 320);
      CHECK(++steps <= 125);
    }
    CHECK(steps == 125);
    // Continuing in source order reuses the read cursor rather than rescanning.
    for (unsigned n = 0; n < 999; ++n) {
      std::snprintf(id, sizeof(id), "%032x", n);
      const auto before = testStorage.bytesRead;
      CHECK(!notice.observe(id));
      CHECK(!notice.stepObservation() && !notice.observing());
      CHECK(testStorage.bytesRead - before == 40);
    }
    CHECK(!notice.observe("fffffffffffffffffffffffffffffffe"));
    bool fresh = false;
    while (notice.observing()) {
      const auto before = testStorage.bytesRead;
      fresh |= notice.stepObservation();
      CHECK(testStorage.bytesRead - before <= 320);
    }
    CHECK(fresh);
  }
  testStorage.writeLimit = -1;
  CHECK(testStorage.files[originalPath] == original);
  {
    // A trailing partial record cannot silently acknowledge an unknown ID.
    testStorage.files[pending] = accepted;
    testStorage.files[pending].push_back(0x5a);
    VocabularyNotice notice;
    notice.resume();
    notice.beginScan();
    char last[kIdBytes];
    std::snprintf(last, sizeof(last), "%032x", 999);
    CHECK(!notice.observe(last));
    while (notice.observing()) CHECK(!notice.stepObservation());
    CHECK(!notice.observe("fffffffffffffffffffffffffffffffd"));
    const auto before = testStorage.bytesRead;
    CHECK(notice.stepObservation() && !notice.observing());
    CHECK(testStorage.bytesRead - before <= 320);
    CHECK(Storage.exists(pending));
  }
  std::printf(
      "%u persistent notice checks, 0 failures: restart, interrupted maintenance, new IDs, failed writes/rename; "
      "originals preserved\n",
      checks);
}
