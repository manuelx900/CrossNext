#include <HalStorage.h>
#include <MD5Builder.h>

#include <cstdio>
#include <string>
#include <vector>

// Bypass the fixture document-id class: exercise the production helper.
#include "../../lib/KOReaderSync/KOReaderDocumentId.h"
int main() {
  int checks = 0, failed = 0;
  const auto check = [&](bool ok) {
    ++checks;
    if (!ok) {
      ++failed;
      std::printf("FAIL document ID check %d\n", checks);
    }
  };
  const std::string path = "/Books/real-id.epub";
  auto& bytes = testStorage.files[path];
  bytes.resize(5000);
  for (size_t n = 0; n < bytes.size(); ++n) bytes[n] = uint8_t(n / 100);
  check(KOReaderDocumentId::calculate(path).size() == 32);
  std::vector<uint8_t> expected;
  for (const size_t offset : {0u, 1024u, 4096u})
    expected.insert(expected.end(), bytes.begin() + offset, bytes.begin() + std::min(bytes.size(), offset + 1024));
  check(hashFinished);
  check(hashedBytes == expected);
  testStorage.refuseReads = true;
  check(KOReaderDocumentId::calculate(path).empty());
  check(!hashFinished && hashedBytes.empty());
  testStorage.refuseReads = false;
  testStorage.readLimit = 100;
  check(KOReaderDocumentId::calculate(path).empty());
  check(!hashFinished && hashedBytes.empty());
  testStorage.readLimit = -1;
  testStorage.refuseSeeks = true;
  check(KOReaderDocumentId::calculate(path).empty());
  check(!hashFinished && hashedBytes.empty());
  testStorage.refuseSeeks = false;
  check(KOReaderDocumentId::calculate(path).size() == 32);
  check(hashedBytes == expected);
  std::printf("Production document ID I/O: %d checks, %d failures\n", checks, failed);
  return failed != 0;
}
