#include <KOReaderDocumentId.h>

#include <cassert>
#include <iostream>

#include "src/apps_local/library/LibraryFinishedBooks.h"
int main() {
  testStorage = {};
  testStorage.files["/recent-not-indexed.epub"] = {1, 2, 3};
  testStorage.files["/indexed.xtc"] = {4, 5, 6};
  uint8_t scratch[1];
  const auto recent = KOReaderDocumentId::calculate("/recent-not-indexed.epub", scratch, 1);
  const auto indexed = KOReaderDocumentId::calculate("/indexed.xtc", scratch, 1);
  constexpr int64_t epoch = 1793059200;
  readingstats::Repository journal;
  libraryui::FinishedBooks finished;
  assert(finished.begin() && finished.empty());
  // Both automatic session completion and explicit Finish use the real journal.
  assert(journal.record(recent.c_str(), "Recent", "Author", epoch, 120, 10, 10000));
  assert(journal.complete(indexed.c_str(), "Indexed", "Author", epoch));
  assert(finished.begin() && !finished.empty());
  bool complete = false;
  assert(finished.contains("/recent-not-indexed.epub", complete) && complete);
  assert(finished.contains("/indexed.xtc", complete) && complete);
  libraryui::FinishedCacheStamp stamp, changed;
  assert(stamp.capture("/library.idx"));
  assert(!stamp.matches(1));
  assert(stamp.save(1) && stamp.matches(1) && !stamp.matches(2));
  testStorage.files["/library.idx"] = {1};
  assert(changed.capture("/library.idx") && !changed.matches(1));
  assert(changed.save(2));
  testStorage.files["/library.idx"] = {2};
  libraryui::FinishedCacheStamp sameSize;
  assert(sameSize.capture("/library.idx") && !sameSize.matches(2));
  testStorage.files.erase("/library.idx");
  assert(stamp.save(1));
  assert(changed.capture("/library.idx") && changed.matches(1));
  testStorage.files["/.crosspoint/collections/finished-cache.bin"][44] ^= 1;
  assert(!stamp.matches(1));
  assert(stamp.save(1));
  assert(changed.capture("/library.idx", 1234) && !changed.matches(1));
  // A cumulative completed snapshot can survive migration without its first-completion flag.
  auto& bytes = testStorage.files[readingstats::kJournalPath];
  readingstats::Record snapshot;
  assert(readingstats::decode(bytes.data() + 16, snapshot));
  snapshot.firstCompletion = false;
  readingstats::encode(snapshot, bytes.data() + 16);
  assert(finished.begin() && finished.contains("/recent-not-indexed.epub", complete) && complete);
  assert(!finished.contains("/deleted.epub", complete));
  assert(journal.reopen(recent.c_str(), "Recent", "Author", epoch + 86400, 9000));
  assert(changed.capture("/library.idx") && !changed.matches(1));
  assert(finished.begin());
  assert(finished.contains("/recent-not-indexed.epub", complete) && !complete);
  assert(finished.contains("/indexed.xtc", complete) && complete);
  // A reused lookup must forget old generations when history is absent.
  testStorage.files.erase(readingstats::kJournalPath);
  assert(finished.begin() && finished.empty());
  assert(finished.contains("/indexed.xtc", complete) && !complete);
  std::cout << "Finished-book journal: automatic, explicit, reopen, missing file and refresh passed\n";
}
