#include <Epub.h>
#include <Epub/LayoutCacheOrder.h>
#include <Epub/PreparedLayoutCopy.h>
#include <Epub/ReaderRenderSpec.h>
#include <GfxRenderer.h>
#include <HalStorage.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
namespace serialization {
template <class T>
void writePod(HalFile& f, const T& v) {
  f.write(&v, sizeof(v));
}
template <class T>
void readPod(HalFile& f, T& v) {
  f.read(&v, sizeof(v));
}
}  // namespace serialization
inline void delay(unsigned long) {}
#define LOG_DBG(...) ((void)0)
// Compile the production routing/serialization/probe methods against an in-memory
// HAL; parser and display ownership are covered by the queue suite and firmware build.
class Section {
 public:
  std::shared_ptr<Epub> epub;
  int spineIndex = 0;
  std::string filePath;
  HalFile file;
  static constexpr uint8_t LAYOUT_CACHE_SLOTS = 4;
  bool layoutCacheReady_ = true, partial_ = false;
  EpubEngine engine_ = EpubEngine::Modern;
  uint16_t pageCount = 0, partialPageCount_ = 0;
  uint32_t partialBytesConsumed_ = 0, partialTotalBytes_ = 0;
  explicit Section(const std::shared_ptr<Epub>& owner)
      : epub(owner), filePath(owner->getCachePath() + "/sections/0.bin") {}
  std::string binTmpPath() const { return filePath + ".part"; }
  ReaderRenderSpec effectiveSpec(const ReaderRenderSpec&) const;
  void writeSectionFileHeader(const ReaderRenderSpec&);
  bool loadSectionFile(const ReaderRenderSpec&);
  bool loadReaderLayout(const ReaderRenderSpec&);
  bool loadPreparedSectionFile(const ReaderRenderSpec&);
  std::string preparedLayoutPath(const ReaderRenderSpec&) const;
  std::string layoutSlotPath(uint8_t) const;
  bool loadLayoutCandidate(const ReaderRenderSpec&);
  bool clearCache() const;
  std::optional<uint16_t> getCachedPageCount() const;
};
#include "section-under-test.inc"
unsigned checks = 0;
#define CHECK(x)                                          \
  do {                                                    \
    ++checks;                                             \
    if (!(x)) {                                           \
      std::printf("FAIL section %d: %s\n", __LINE__, #x); \
      std::exit(1);                                       \
    }                                                     \
  } while (0)
void writeLayout(Section& s, const std::string& path, const ReaderRenderSpec& spec, uint16_t pages) {
  CHECK(Storage.openFileForWrite("TEST", path, s.file));
  s.pageCount = pages;
  s.writeSectionFileHeader(spec);
  auto& bytes = testStorage.files[path];
  bytes[0] = SECTION_FILE_VERSION;
  bytes.resize(12000, 0x71);
  s.file.close();
}
void writePartial(Section& s, const std::string& path, const ReaderRenderSpec& spec, uint16_t pages) {
  writeLayout(s, path, spec, pages);
  auto& bytes = testStorage.files[path];
  bytes[0] = SECTION_FILE_PARTIAL_VERSION;
  const uint32_t li = HEADER_SIZE, visible = HEADER_SIZE + 2;
  const uint32_t consumed = 4000, total = 10000, trailer = visible + uint32_t(pages) * 4;
  bytes.resize(trailer + 8);
  std::memcpy(bytes.data() + HEADER_SIZE - 8, &li, 4);
  std::memcpy(bytes.data() + HEADER_SIZE - 4, &visible, 4);
  std::memcpy(bytes.data() + trailer, &consumed, 4);
  std::memcpy(bytes.data() + trailer + 4, &total, 4);
}
int main() {
  auto epub = std::make_shared<Epub>("/book.epub", "/.crosspoint");
  Section s(epub);
  ReaderRenderSpec spec;
  spec.fontId = 100;
  spec.viewportWidth = 400;
  spec.viewportHeight = 680;
  const auto primary = s.layoutSlotPath(0), named = s.preparedLayoutPath(spec);
  writeLayout(s, primary, spec, 44);
  const auto original = testStorage.files[primary];
  CHECK(s.loadPreparedSectionFile(spec));
  CHECK(testStorage.files[primary] == original && testStorage.files[named] == original);
  Section bare(epub);
  CHECK(bare.getCachedPageCount() == 44);
  testStorage.files.erase(primary);
  CHECK(s.loadSectionFile(spec));
  CHECK(testStorage.files[named] == original && !testStorage.files.count(primary));
  CHECK(s.filePath == named);
  CHECK(!Section(epub).getCachedPageCount());
  CHECK(preparedlayout::copy(named, primary));
  const auto writes = testStorage.writes;
  CHECK(s.loadSectionFile(spec));
  CHECK(testStorage.writes == writes);
  for (int n = 1; n < 7; ++n) {
    ReaderRenderSpec next = spec;
    next.fontId += n;
    writeLayout(s, s.preparedLayoutPath(next), next, 44 + n);
    CHECK(s.loadSectionFile(next));
    CHECK(s.pageCount == 44 + n && s.filePath == s.preparedLayoutPath(next));
    CHECK(Section(epub).getCachedPageCount() == 44);
    CHECK(testStorage.files.count(named));
  }
  CHECK(s.loadSectionFile(spec));
  CHECK(Section(epub).getCachedPageCount() == 44);
  // Short reads/writes and failed publication must never replace a good primary.
  const auto current = testStorage.files[primary];
  testStorage.writeLimit = 10;
  CHECK(!preparedlayout::copy(named, primary));
  CHECK(testStorage.files[primary] == current && testStorage.files[named] == original);
  testStorage.writeLimit = -1;
  testStorage.readLimit = 10;
  CHECK(!preparedlayout::copy(named, primary));
  CHECK(testStorage.files[primary] == current);
  testStorage.readLimit = -1;
  testStorage.refuseRename = true;
  CHECK(!preparedlayout::copy(named, primary));
  CHECK(testStorage.files[primary] == current);
  testStorage.refuseRename = false;
  testStorage.failClose = true;
  CHECK(!preparedlayout::copy(named, primary));
  CHECK(testStorage.files[primary] == current && testStorage.files[named] == original);
  CHECK(!testStorage.files.count(primary + ".prep.tmp"));
  testStorage.failClose = false;
  // Reader recovery follows the same .swap convention after interrupted publication.
  CHECK(Storage.rename(primary.c_str(), (primary + ".swap").c_str()));
  CHECK(s.loadReaderLayout(spec));
  CHECK(Section(epub).getCachedPageCount() == 44);
  // N pages prepared, then M>N pages suspended by reading: keep M on reopen.
  writePartial(s, named, spec, 3);
  writePartial(s, primary, spec, 7);
  const auto newer = testStorage.files[primary];
  CHECK(s.loadSectionFile(spec));
  CHECK(s.partial_ && s.pageCount == 7);
  CHECK(testStorage.files[primary] == newer);
  CHECK(s.loadSectionFile(spec));
  CHECK(s.pageCount == 7 && testStorage.files[primary] == newer);
  auto classic = spec;
  classic.engine = EpubEngine::Classic;
  const auto modernBytes = testStorage.files[primary];
  const auto classicPrepared = s.preparedLayoutPath(classic);
  CHECK(classicPrepared != named);
  writeLayout(s, classicPrepared, classic, 17);
  CHECK(s.loadSectionFile(classic));
  const auto classicPrimary = s.filePath;
  CHECK(classicPrimary != primary && s.pageCount == 17);
  CHECK(testStorage.files[primary] == modernBytes);
  const auto classicBytes = testStorage.files[classicPrimary];
  CHECK(s.loadSectionFile(spec));
  CHECK(s.filePath == primary && s.pageCount == 7);
  CHECK(testStorage.files[classicPrimary] == classicBytes);
  CHECK(s.loadSectionFile(classic));
  CHECK(s.filePath == classicPrimary && s.pageCount == 17);
  CHECK(testStorage.files[primary] == modernBytes);
  std::printf("prepared layout integration: %u checks, 0 failed\n", checks);
}
