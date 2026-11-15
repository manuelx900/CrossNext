#include <Epub/Page.h>
#include <GfxRenderer.h>
#include <Performance.h>

#include <cstdio>
#include <cstdlib>

#include "../../src/apps_local/reader/PageReuse.h"
unsigned checks = 0;
#define CHECK(x)                                                        \
  do {                                                                  \
    ++checks;                                                           \
    if (!(x)) {                                                         \
      std::printf("reader wiring failed %u at %d\n", checks, __LINE__); \
      std::exit(1);                                                     \
    }                                                                   \
  } while (0)
#define LOG_DBG(...) ((void)0)
#define LOG_ERR(...) ((void)0)
constexpr int UI_12_FONT_ID = 0, STR_PAGE_LOAD_ERROR = 0;
const char* tr(int) { return "error"; }
struct Bytes : Print {
  std::vector<uint8_t> data;
  size_t write(uint8_t b) override {
    data.push_back(b);
    return 1;
  }
  size_t write(const uint8_t* p, size_t n) override {
    if (n) data.insert(data.end(), p, p + n);
    return n;
  }
};
std::unique_ptr<Page> makePage() {
  auto p = std::make_unique<Page>();
  p->elements.reserve(1);
  p->footnotes.reserve(1);
  p->links.reserve(1);
  std::vector<std::string> words = {"misma", "página"};
  auto text =
      std::make_unique<TextBlock>(words, std::vector<int16_t>{0, 48},
                                  std::vector<EpdFontFamily::Style>{EpdFontFamily::REGULAR, EpdFontFamily::REGULAR},
                                  std::vector<uint8_t>{0, 0}, std::vector<uint16_t>{0, 0});
  p->elements.push_back(std::make_unique<PageLine>(std::move(text), 3, 4));
  p->addFootnote("1", "book.xhtml#note");
  CHECK(p->addLink("book.xhtml#link", 1, 2, 20, 20));
  return p;
}
struct Settings {
  ReaderRenderSpec value;
  uint8_t screenMargin = 0;
  ReaderRenderSpec readerRenderSpec(int w, int h) const {
    auto spec = value;
    spec.viewportWidth = w;
    spec.viewportHeight = h;
    return spec;
  }
};
Settings settings;
#define SETTINGS settings
struct Section {
  int currentPage = 1, pageCount = 2, loads = 0;
  bool fail = false;
  std::unique_ptr<Page> loadPage(int) {
    ++loads;
    return fail ? nullptr : makePage();
  }
  void abandonBuild() {}
  void clearCache() {}
};
struct ReuseWiringReader {
  bool epub = true, automaticPageTurnActive = false, allowRetention = true;
  bool textFormatPending = false, overlayPageStored = false;
  ReaderRenderSpec presentedTextSpec;
  uint8_t presentedScreenMargin = 0;
  std::unique_ptr<Section> section = std::make_unique<Section>();
  readerpage::Reuse residentPage;
  std::unique_ptr<Page> prefetchedPage;
  int currentSpineIndex = 0, buildViewportWidth = 480, buildViewportHeight = 800;
  int currentPageLinkMarginLeft = 0, currentPageLinkMarginTop = 0;
  uint32_t currentPageVisibleOffset = 0;
  uint8_t pageLoadRetryCount = 0;
  static constexpr int MAX_PAGE_LOAD_RETRIES = 3;
  std::vector<FootnoteEntry> currentPageFootnotes;
  std::vector<PageLink> currentPageLinks;
  std::vector<uint8_t> drawn;
  struct Renderer : GfxRenderer {
    unsigned storedCopies = 0;
    void copyStoredBwBuffer() { ++storedCopies; }
  } renderer;
  ReuseWiringReader() {
    Storage.content = Storage.media = 1;
    Storage.state = 0;
    settings.value = {};
  }
  readerpage::Key pageReuseKey() const;
  void discardOverlayPage() {}
  void settleOverlayRefresh() {}
  void renderStatusBar() {}
  void markPageRendered() {}
  void showPendingSyncSaveError() {}
  bool prefetchHeapAvailable(bool) const { return allowRetention; }
  std::unique_ptr<Page> takePrefetchedPage(int) { return std::move(prefetchedPage); }
  void resetSection() {
    residentPage.clear();
    section.reset();
  }
  void requestUpdate() {}
  void renderContents(Page& page, int, int, int, int) {
    Bytes bytes;
    CHECK(page.serialize(bytes));
    drawn = std::move(bytes.data);
  }
  void renderPage() {
#include "reader-prefix-under-test.inc"
    int orientedMarginTop = 1, orientedMarginBottom = 2, orientedMarginLeft = 3, orientedMarginRight = 4;
    const auto renderSpec = SETTINGS.readerRenderSpec(buildViewportWidth, buildViewportHeight);
#include "reader-load-under-test.inc"
  }
};
#include "reader-key-under-test.inc"
int main() {
  ReuseWiringReader reader;
  reader.renderPage();
  CHECK(reader.section->loads == 1);
  CHECK(reader.currentPageLinks.size() == 1 && reader.currentPageFootnotes.size() == 1);
  const auto original = reader.drawn;
  reader.textFormatPending = true;
  reader.overlayPageStored = true;
  reader.renderPage();
  CHECK(reader.section->loads == 1 && reader.drawn == original);
  CHECK(reader.renderer.storedCopies == 1);
  reader.textFormatPending = false;
  reader.renderPage();
  CHECK(reader.section->loads == 1);
  CHECK(reader.currentPageLinks.size() == 1 && reader.currentPageFootnotes.size() == 1);
  CHECK(reader.drawn == original);
  ++Storage.state;
  reader.renderPage();
  CHECK(reader.section->loads == 1 && reader.drawn == original);
  CHECK(reader.currentPageLinks.size() == 1);
  ++Storage.content;
  reader.renderPage();
  CHECK(reader.section->loads == 2 && reader.drawn == original);
  ++settings.value.paragraphIndentSpaces;
  reader.renderPage();
  CHECK(reader.section->loads == 3 && reader.currentPageLinks.size() == 1);
  reader.allowRetention = false;
  reader.renderPage();
  CHECK(reader.residentPage.retainedBytes() == 0);
  reader.renderPage();
  CHECK(reader.section->loads == 4 && reader.drawn == original);
  reader.allowRetention = true;
  reader.renderPage();
  reader.section->fail = true;
  ++Storage.content;
  reader.renderPage();
  CHECK(!reader.section && reader.currentPageLinks.empty());
  ReuseWiringReader prefetched;
  prefetched.prefetchedPage = makePage();
  prefetched.renderPage();
  CHECK(prefetched.section->loads == 0);
  CHECK(prefetched.currentPageLinks.size() == 1 && prefetched.currentPageFootnotes.size() == 1);
  prefetched.epub = false;
  prefetched.renderPage();
  CHECK(prefetched.currentPageLinks.empty());
  std::printf(
      "Production reader page dispatch: %u checks; zero extra loads on redraw/progress, exact bytes and live "
      "links/notes\n",
      checks);
}
