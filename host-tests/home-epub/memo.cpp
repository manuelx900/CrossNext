#include <HalMemory.h>
#include <HalStorage.h>

#include <cassert>
#include <cstdio>

#include "util/ReadingTimeMemo.h"
int main(int argc, char**) {
  failPsram = argc > 1;
  testStateRevision = 1;
  const auto resource = Storage.resourceRevision(), media = Storage.mediaRevision();
  uint64_t seconds = 0;
  bool known = false;
  readingtime::remember("/unproven.epub", "", 0, true, resource, media, testStateRevision);
  assert(!readingtime::lookup("/unproven.epub", seconds, known));
  readingtime::remember("/unknown.epub", "doc", 0, false, resource, media, testStateRevision);
  assert(!readingtime::lookup("/unknown.epub", seconds, known));
  readingtime::remember("/book.epub", "doc", 60, true, resource, media, testStateRevision);
  if (failPsram) {
    assert(!readingtime::lookup("/book.epub", seconds, known));
    return 0;
  }
  assert(readingtime::lookup("/book.epub", seconds, known) && seconds == 60 && known);
  ++testStateRevision;
  assert(!readingtime::lookup("/book.epub", seconds, known));
  readingtime::publish("other", 100);
  assert(!readingtime::lookup("/book.epub", seconds, known));
  readingtime::publish("doc", 120);
  assert(readingtime::lookup("/book.epub", seconds, known) && seconds == 120);
  testContentRevision.bump();
  readingtime::publish("doc", 300);
  assert(!readingtime::lookup("/book.epub", seconds, known));
  const auto changed = Storage.resourceRevision();
  readingtime::remember("/book.epub", "doc", 120, true, changed, media, testStateRevision);
  testMediaRevision.bump();
  assert(!readingtime::lookup("/book.epub", seconds, known));
  assert(!readingtime::lookup("/unknown.epub", seconds, known));
  std::puts("Reading memo: warm hit, publish, wrong document, source/media/journal invalidation and OOM passed");
}
