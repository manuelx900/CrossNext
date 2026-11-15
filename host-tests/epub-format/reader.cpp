#include <Epub/ReaderRenderSpec.h>
#include <Memory.h>
#include <Performance.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <optional>
#define LOG_ERR(...) ((void)0)
constexpr int BACKGROUND_BUILD_SLICE_MS = 8;
uint32_t millis() { return 1000; }
struct RenderLock {
  RenderLock() {}
  ~RenderLock() {}
};
struct Settings {
  ReaderRenderSpec spec;
  uint8_t screenMargin = 0;
  ReaderRenderSpec readerRenderSpec(int w, int h) {
    auto v = spec;
    v.viewportWidth = w;
    v.viewportHeight = h;
    return v;
  }
} SETTINGS;
struct Renderer {};
struct Section {
  inline static bool hit = false, partial = false, fail = false;
  int currentPage = 0, pageCount = 0, steps = 0;
  bool building = false, finished = false;
  Section(int, int, Renderer&) {}
  bool loadSectionFile(const ReaderRenderSpec&) {
    pageCount = hit ? 20 : 0;
    return hit;
  }
  bool isPartial() { return partial; }
  bool isBuilding() { return building; }
  bool layoutFinished() { return finished; }
  bool startFormatBuild(const ReaderRenderSpec&) {
    building = true;
    return !fail;
  }
  bool buildReachedVisibleTextOffset(uint32_t offset) { return uint32_t(pageCount) * 10 > offset + 10; }
  bool buildSomeMore(int n, void*, uint32_t ms) {
    assert(n == 1 && ms == 8);
    ++steps;
    ++pageCount;
    finished = pageCount == 20;
    return !fail;
  }
  std::optional<uint16_t> getPageForVisibleTextOffset(uint32_t offset) {
    if (building && !finished && !buildReachedVisibleTextOffset(offset)) return {};
    return uint16_t(std::min(int(offset / 10), std::max(0, pageCount - 1)));
  }
};
struct FontSystem {
  void ensureLoaded(Renderer&) {}
} sdFontSystem;
struct EpubReaderActivity {
  enum class FormatPreparation { Ready, Pending, Failed };
  std::unique_ptr<Section> section;
  int epub = 1, currentSpineIndex = 0, cachedSpineIndex = 0, nextPageNumber = 0;
  int buildViewportWidth = 180, buildViewportHeight = 260, cachedChapterTotalPageCount = 0;
  std::optional<uint32_t> cachedVisibleTextOffset = 155;
  std::optional<ReaderRenderSpec> presentedTextSpec;
  uint8_t presentedScreenMargin = 0;
  bool partialRebuildStartFailed = false, formatReflowPending = true, textPreferencesDirty = false,
       textSaveSuppressed = false;
  bool textFormatPending = false, finalPagePresented = true;
  uint32_t textFormatChangedAt = 0;
  Renderer renderer;
  int remembered = 0, resets = 0, discarded = 0;
  void clearDeferredReposition() { cachedVisibleTextOffset.reset(); }
  bool makeRoomForChapterBuild() { return true; }
  void releasePageReuse() {}
  void rememberReflowTarget();
  void rememberCurrentContentOffset() {
    ++remembered;
    cachedVisibleTextOffset = 155;
  }
  void resetSection() {
    ++resets;
    section.reset();
    formatReflowPending = false;
  }
  void discardOverlayPage() { ++discarded; }
  FormatPreparation prepareFormatPage(const ReaderRenderSpec&);
  void applyReaderTextSettings();
};
#include "format-reader-under-test.inc"
int main() {
  using State = EpubReaderActivity::FormatPreparation;
  ReaderRenderSpec spec;
  EpubReaderActivity reader;
  int calls = 0;
  while (reader.prepareFormatPage(spec) == State::Pending) {
    ++calls;
    assert(calls < 30);
  }
  assert(calls > 2 && reader.section->currentPage == 15 && !reader.formatReflowPending);
  // An input during a new prefix build keeps the original content anchor.
  reader.formatReflowPending = true;
  reader.cachedVisibleTextOffset = 155;
  reader.section->currentPage = 0;
  reader.applyReaderTextSettings();
  assert(reader.remembered == 0 && reader.cachedVisibleTextOffset == 155 && reader.formatReflowPending);
  assert(reader.textPreferencesDirty && !reader.textFormatPending);
  // Geometry-identical changes (including AA) retain the composed section.
  reader.formatReflowPending = false;
  reader.section = makeUniqueNoThrow<Section>(1, 0, reader.renderer);
  reader.presentedTextSpec = SETTINGS.readerRenderSpec(180, 260);
  auto* section = reader.section.get();
  reader.applyReaderTextSettings();
  assert(reader.section.get() == section && !reader.formatReflowPending);
  SETTINGS.screenMargin = 10;
  reader.applyReaderTextSettings();
  assert(!reader.section && reader.formatReflowPending && reader.remembered == 1);
  // Complete profile hits require no parser steps; a failed build propagates.
  Section::hit = true;
  EpubReaderActivity cached;
  assert(cached.prepareFormatPage(spec) == State::Ready && cached.section->steps == 0);
  Section::hit = false;
  Section::fail = true;
  EpubReaderActivity error;
  assert(error.prepareFormatPage(spec) == State::Failed);
  puts(
      "Production format wiring: cooperative target composition, original anchor, cached profile, AA/no-op reuse, "
      "margin invalidation, failure propagation");
}
