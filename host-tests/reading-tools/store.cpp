#include <HalStorage.h>

#include <cassert>
#include <cstdio>

#include "../../src/apps_local/quotes/QuotesStore.h"
int main() {
  using namespace quotes;
  static Entry e[kMax];
  for (int n = 0; n < kMax; ++n) e[n].key = uint64_t(n) + 1000;
  assert(loadFlags(e, kMax) == Load::Missing);
  e[0].flags = kFavorite;
  e[1].flags = kHidden;
  assert(saveFlags(e, kMax));
  const auto original = testStorage.files["/.crosspoint/quotes.bin"];
  e[0].flags = e[1].flags = 0;
  assert(loadFlags(e, kMax) == Load::Ok);
  assert(e[0].flags == kFavorite && e[1].flags == kHidden);
  int checks = 5;
  for (size_t byte = 0; byte < original.size(); ++byte) {
    testStorage.files["/.crosspoint/quotes.bin"] = original;
    testStorage.files["/.crosspoint/quotes.bin"][byte] ^= 1;
    e[0].flags = 0;
    assert(loadFlags(e, kMax) == Load::Error);
    assert(e[0].flags == 0);
    checks += 2;
  }
  testStorage.files["/.crosspoint/quotes.bin"] = original;
  e[2].flags = kFavorite;
  testStorage.writeLimit = 3;
  assert(!saveFlags(e, kMax));
  assert(testStorage.files["/.crosspoint/quotes.bin"] == original);
  testStorage.writeLimit = -1;
  checks += 2;
  assert(saveFlags(e, kMax));
  testStorage.files["/.crosspoint/quotes.bin"][0] ^= 1;
  e[2].flags = 0;
  assert(loadFlags(e, kMax) == Load::Ok);
  assert(e[2].flags == 0);
  checks += 3;
  testStorage.files["/.crosspoint/quotes.bin"] = original;
  testStorage.files["/.crosspoint/quotes.bin"][4] = 2;
  const auto future = testStorage.files["/.crosspoint/quotes.bin"];
  assert(loadFlags(e, kMax) == Load::Error);
  assert(!saveFlags(e, kMax));
  assert(testStorage.files["/.crosspoint/quotes.bin"] == future);
  checks += 3;
  testStorage.files.erase("/.crosspoint/quotes.bin");
  testStorage.refuseRename = true;
  assert(!saveFlags(e, kMax));
  assert(loadFlags(e, kMax) == Load::Ok);
  checks += 2;
  testStorage = {};
  for (auto& entry : e) entry.flags = 0;
  assert(saveFlags(e, kMax));  // valid empty backup before adding primary-only marks
  e[0].flags = kFavorite;
  e[1].flags = kHidden;
  assert(saveFlags(e, kMax));
  e[0].flags = e[1].flags = 0;
  testStorage.failApplyPath = "/.crosspoint/quotes.bin";
  testStorage.failApplyRecord = 1;
  assert(loadFlags(e, kMax) == Load::Ok);
  assert(e[0].flags == 0 && e[1].flags == 0);
  checks += 4;
  std::printf("Quotes store: %d checks, 0 failures\n", checks);
}
