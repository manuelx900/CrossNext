#include "EpubReaderActivity.h"

#include <BlePageTurner.h>
#include <Epub/Page.h>
#include <Epub/WordWidthCache.h>
#include <Epub/blocks/TextBlock.h>
#include <Epub/parsers/ChapterEventCache.h>
#include <Epub/parsers/ParagraphInputCache.h>
#include <FontCacheManager.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalFrontlight.h>
#include <HalGPIO.h>
#include <HalMemory.h>
#include <HalStorage.h>
#include <I18n.h>
#include <KOReaderDocumentId.h>
#include <Logging.h>
#include <Memory.h>
#include <Performance.h>
#include <TrustedTime.h>
#include <WiFi.h>
#include <esp_system.h>

#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>
#include <string_view>

#include "../../util/BookmarkFile.h"
#include "BookmarkEntry.h"
#include "ChapterBuildGate.h"
#include "ClipSelectionActivity.h"
#include "ClippingStore.h"
#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "DictionaryWordSelectActivity.h"
#include "EpubReaderBookmarksActivity.h"
#include "EpubReaderChapterSelectionActivity.h"
#include "EpubReaderClippingListActivity.h"
#include "EpubReaderFootnoteSelectActivity.h"
#include "EpubReaderPercentSelectionActivity.h"
#include "EpubReaderUtils.h"
#include "KOReaderCredentialStore.h"
#include "KOReaderDocumentId.h"
#include "KOReaderSyncActivity.h"
#include "MappedInputManager.h"
#include "ProgressComparison.h"
#include "ProgressMapper.h"
#include "QrDisplayActivity.h"
#include "ReaderActivity.h"
#include "ReaderFontSizes.h"
#include "ReaderToolbarUi.h"
#include "ReaderUtils.h"
#include "RecentBooksStore.h"
#include "SdCardFontSystem.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/settings/TextSettingsActivity.h"
#include "apps_local/bookorbit/BookOrbit.h"
#include "apps_local/bookorbit/BookOrbitPending.h"
#include "apps_local/bookprep/OpenedBookPreparationActivity.h"
#include "apps_local/power/FrontlightSchedule.h"
#include "apps_local/readertext/BookSearchActivity.h"
#include "apps_local/readertext/BookTextPreferences.h"
#include "apps_local/reading/ReadingStatsActivity.h"
#include "apps_local/ui/Rotation.h"
#include "apps_local/words/Vocabulary.h"
#include "clippings/ClippingText.h"
#include "clippings/ClippingsManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/WifiPowerSaveGuard.h"
#include "util/BookmarkUtil.h"
#include "util/ButtonNavigator.h"
#include "util/DictionaryRegistry.h"
#include "util/LoopDeadline.h"
#include "util/ScreenshotUtil.h"

namespace {
// The X4 Pro and X4 Classic carry the X4's panel but sit outside isXteinkDevice()
// (that helper also gates power management). Overlay refresh choices are per-panel:
// this family runs the grayscale anti-aliasing pass, so chrome painted over a
// fresh page needs the HALF ghost-cleanup and closing re-renders the page.
bool xteinkClassPanel() {
  return gpio.isXteinkDevice() || BoardConfig::isX4Pro() || BoardConfig::isX4Classic() || BoardConfig::isEegoA4();
}

constexpr int PAGE_TURN_RATES[] = {1, 1, 3, 6, 12};
constexpr size_t initialBookmarkCacheCapacity = 16;
constexpr float bookmarkProgressEpsilon = 0.0001f;
constexpr int CLIP_SELECTION_PAGE_WINDOW = 3;
constexpr uint32_t BOOKORBIT_WAKE_READY_MS = 8000;
constexpr uint32_t BOOKORBIT_WAKE_SYNC_MS = 18000;

Rect clippingWordRect(const GfxRenderer& renderer, const int fontId, const PageLine& line, const uint16_t i,
                      const int top, const int left, const int wordWidth) {
  const auto& block = line.getBlock();
  const char* text = block->wordText(i);
  const auto geometry = block->wordGeometry(renderer, fontId, i);
  const auto style = static_cast<EpdFontFamily::Style>(block->wordStyle(i) & ~EpdFontFamily::UNDERLINE);
  const int skipX =
      clippingText::hasEmSpacePrefix(text) ? renderer.getTextAdvanceX(geometry.fontId, "\xe2\x80\x83", style) : 0;
  int width = wordWidth - skipX;
  if (block->wordXOffset(i) == 0 && i + 1 < block->wordCount() && block->wordXpos(i + 1) > block->wordXpos(i)) {
    width = std::min(width, static_cast<int>(block->wordXpos(i + 1) - block->wordXpos(i) - skipX));
  }
  return Rect{left + line.xPos + block->wordXpos(i) + block->wordXOffset(i) + skipX, top + line.yPos + geometry.y,
              width, geometry.height};
}

Rect clippingCapRect(const GfxRenderer& renderer, const int fontId, const PageLine& line, const uint16_t i,
                     const int top, const int left) {
  const auto* block = line.getBlock();
  const auto* cap = i == 0 ? block->getDropCap() : nullptr;
  if (!cap) return Rect{};
  return Rect{left + line.xPos + block->wordXpos(i) - cap->inset,
              top + line.yPos + block->wordGeometry(renderer, fontId, i).y, cap->inset, cap->height};
}

int clampPercent(int percent) {
  if (percent < 0) {
    return 0;
  }
  if (percent > 100) {
    return 100;
  }
  return percent;
}

constexpr char READ_FOLDER[] = "/read";

bool isInReadFolder(const std::string& path) {
  constexpr size_t n = sizeof(READ_FOLDER) - 1;
  return path.size() > n && path.compare(0, n, READ_FOLDER) == 0 && path[n] == '/';
}

struct ProgressRange {
  float start;
  float end;
};

ProgressRange getPageProgressRange(const std::shared_ptr<Epub>& epub, const int spineIndex, const int page,
                                   const int pageCount) {
  if (pageCount <= 1) {
    return {epub->calculateProgress(spineIndex, 0.0f), epub->calculateProgress(spineIndex, 1.0f)};
  }

  const float step = 1.0f / static_cast<float>(pageCount - 1);
  const float anchor = std::clamp(static_cast<float>(page) * step, 0.0f, 1.0f);
  const float start = std::max(0.0f, anchor - (step * 0.5f));
  const float end = std::min(1.0f, anchor + (step * 0.5f));
  return {epub->calculateProgress(spineIndex, start), epub->calculateProgress(spineIndex, end)};
}

bool bookmarkMatchesProgress(const BookmarkEntry& bookmark, const int spineIndex, const int page, const int pageCount,
                             const ProgressRange& pageRange) {
  if (bookmark.computedSpineIndex == spineIndex && bookmark.computedChapterPageCount == pageCount &&
      bookmark.computedChapterProgress == page) {
    return true;
  }

  const float bookmarkProgress = std::clamp(bookmark.percentage, 0.0f, 1.0f);
  return bookmarkProgress + bookmarkProgressEpsilon >= pageRange.start &&
         bookmarkProgress - bookmarkProgressEpsilon <= pageRange.end;
}

std::string buildReadFolderDestination(const std::string& srcPath) {
  const size_t lastSlash = srcPath.rfind('/');
  const std::string filename = (lastSlash != std::string::npos) ? srcPath.substr(lastSlash + 1) : srcPath;

  Storage.mkdir(READ_FOLDER);
  std::string dstPath = std::string(READ_FOLDER) + "/" + filename;
  if (!Storage.exists(dstPath.c_str())) {
    return dstPath;
  }

  const size_t dotPos = filename.rfind('.');
  const std::string base = (dotPos != std::string::npos) ? filename.substr(0, dotPos) : filename;
  const std::string ext = (dotPos != std::string::npos) ? filename.substr(dotPos) : "";
  int suffix = 2;
  do {
    dstPath = std::string(READ_FOLDER) + "/" + base + " (" + std::to_string(suffix) + ")" + ext;
    suffix++;
  } while (Storage.exists(dstPath.c_str()) && suffix < 100);
  return dstPath;
}

void moveFinishedBookToReadFolder(const std::string& srcPath, const std::string& dstPath,
                                  const std::string& oldCachePath) {
  LOG_INF("ERS", "Moving finished epub: %s -> %s", srcPath.c_str(), dstPath.c_str());
  if (!ClippingStore::moveBook(srcPath, dstPath)) {
    LOG_ERR("ERS", "Failed to move finished book and clippings to '/Read' folder");
    return;
  }

  // Protected-book sidecars travel with the book; a protected book separated
  // from its key no longer opens, so a failed move rolls everything back.
  static constexpr const char* SIDECARS[] = {".key", ".rights"};
  for (size_t i = 0; i < std::size(SIDECARS); i++) {
    const std::string from = srcPath + SIDECARS[i];
    if (!Storage.exists(from.c_str())) continue;
    const std::string to = dstPath + SIDECARS[i];
    if (Storage.rename(from.c_str(), to.c_str())) continue;
    LOG_ERR("ERS", "Failed to move sidecar %s -> %s", from.c_str(), to.c_str());
    for (size_t j = 0; j < i; j++) {
      Storage.rename((dstPath + SIDECARS[j]).c_str(), (srcPath + SIDECARS[j]).c_str());
    }
    if (!Storage.rename(dstPath.c_str(), srcPath.c_str())) {
      LOG_ERR("ERS", "Failed to restore epub after sidecar move failure: %s -> %s", dstPath.c_str(), srcPath.c_str());
    }
    return;
  }

  if (!bookdictionary::move(srcPath.c_str(), dstPath.c_str())) {
    LOG_ERR("ERS", "Could not move book dictionaries; restoring file");
    for (const char* suffix : SIDECARS) {
      const std::string from = dstPath + suffix;
      if (Storage.exists(from.c_str()) && !Storage.rename(from.c_str(), (srcPath + suffix).c_str()))
        LOG_ERR("ERS", "Could not restore sidecar: %s", from.c_str());
    }
    if (!ClippingStore::moveBook(dstPath, srcPath)) LOG_ERR("ERS", "Could not restore finished book");
    return;
  }

  const std::string newCachePath = "/.crosspoint/epub_" + std::to_string(std::hash<std::string>{}(dstPath));
  bool cacheMoved = false;
  if (!oldCachePath.empty() && Storage.exists(oldCachePath.c_str())) {
    cacheMoved = Storage.rename(oldCachePath.c_str(), newCachePath.c_str());
    if (!cacheMoved) {
      LOG_ERR("ERS", "Failed to rename cache dir %s -> %s (non-fatal)", oldCachePath.c_str(), newCachePath.c_str());
    }
  }

  RECENT_BOOKS.updatePath(srcPath, dstPath, oldCachePath, cacheMoved ? newCachePath : oldCachePath);
  if (APP_STATE.openEpubPath == srcPath) {
    APP_STATE.openEpubPath = dstPath;
    APP_STATE.saveToFile();
  }
}

}  // namespace

EpubReaderActivity::~EpubReaderActivity() {
  ImageBlock::setExtractor(nullptr, nullptr);
  // ActivityManager destroys activities with its RenderLock already held;
  // taking another here self-deadlocks (renderingMutex is non-recursive).
  settleOverlayRefresh();
  discardOverlayPage();  // free the overlay's page snapshot if one is held

  if (footnoteDepth > 0 && epub) saveLinkStack();

  resetSection();
  CLIPPINGS.unload();
  if (pendingReadFolderMove && epub) {
    const std::string srcPath = epub->getPath();
    const std::string oldCachePath = epub->getCachePath();
    const std::string dstPath = buildReadFolderDestination(srcPath);
    epub.reset();
    moveFinishedBookToReadFolder(srcPath, dstPath, oldCachePath);
  } else {
    epub.reset();
  }
}

bool EpubReaderActivity::loadBook() {
  pageBufferStale = true;
  performance::Scope metadata(performance::Stage::Metadata);
  ChapterEventCache::retainReaderReplay(true);
  ParagraphInputCache::retainReader(true);
  WordWidthCacheLease::retainReader(true);
  auto loadedEpub = makeUniqueNoThrow<Epub>(bookPath, "/.crosspoint");
  if (!loadedEpub) {
    LOG_ERR("ERS", "Failed to allocate EPUB object");
    return false;
  }

  const bool uncached = !Storage.exists((loadedEpub->getCachePath() + "/book.bin").c_str());
  HalMemory::PsramBuffer preparationBackdrop;
  if (uncached && SETTINGS.prepareBookOnOpen) {
    RenderLock lock;
    preparationBackdrop = HalMemory::allocatePsram(renderer.getBufferSize());
    if (preparationBackdrop)
      std::memcpy(preparationBackdrop.get(), renderer.getFrameBuffer(), renderer.getBufferSize());
    else
      LOG_ERR("PREP", "No PSRAM for opening backdrop");
  }
  if (uncached) {
    disableFastInitialRefresh();
    if (SETTINGS.prepareBookOnOpen)
      bookprep::showOpeningProgress(renderer);
    else
      GUI.drawPopup(renderer, tr(STR_INDEXING));
  }

  bool loaded;
  {
    std::optional<GfxRenderer::FrameBufferLoan> loan;
    if (uncached) loan.emplace(renderer);
    loaded = loadedEpub->load(true, SETTINGS.embeddedStyle == 0);
  }
  if (!loaded) {
    // Surfaced by handleLoadFailure() as a dialog; loadedEpub dies with this
    // scope, so carry the reason out in a member.
    loadProtectionError = loadedEpub->getProtectionError();
    LOG_ERR("ERS", "Failed to load EPUB%s%s", loadProtectionError.empty() ? "" : ": ", loadProtectionError.c_str());
    return false;
  }
  epub = std::move(loadedEpub);
  if (auto* css = epub->getCssParser()) css->retainForReader(true);

  ImageBlock::clearRenderFailures();

  ImageBlock::setExtractor(epub.get(), [](void* ctx, const char* src, const char* dest) {
    return static_cast<Epub*>(ctx)->extractItemToFile(src, dest);
  });

  epub->setupCacheDir();
  bookdictionary::load(epub->getPath().c_str(), bookDictionaries);

  HalFile f;
  if (Storage.openFileForRead("ERS", epub->getCachePath() + "/progress.bin", f)) {
    uint8_t data[10];
    int dataSize = f.read(data, sizeof(data));
    if (dataSize == 4 || dataSize == 6 || dataSize == 10) {
      currentSpineIndex = data[0] + (data[1] << 8);
      nextPageNumber = data[2] + (data[3] << 8);
      if (nextPageNumber == UINT16_MAX) {
        LOG_DBG("ERS", "Ignoring stale last-page sentinel from progress cache");
        nextPageNumber = 0;
      }
      cachedSpineIndex = currentSpineIndex;
      LOG_DBG("ERS", "Loaded cache: %d, %d", currentSpineIndex, nextPageNumber);
    }
    if (dataSize == 6) {
      cachedChapterTotalPageCount = data[4] + (data[5] << 8);
    } else if (dataSize == 10) {
      cachedChapterTotalPageCount = data[4] + (data[5] << 8);
      cachedVisibleTextOffset = static_cast<uint32_t>(data[6]) | (static_cast<uint32_t>(data[7]) << 8) |
                                (static_cast<uint32_t>(data[8]) << 16) | (static_cast<uint32_t>(data[9]) << 24);
    }
  }

  if (currentSpineIndex == 0) {
    int textSpineIndex = epub->getSpineIndexForTextReference();
    if (textSpineIndex != 0) {
      currentSpineIndex = textSpineIndex;
      cachedVisibleTextOffset.reset();
      LOG_DBG("ERS", "Opened for first time, navigating to text reference at index %d", textSpineIndex);
    }
  }

  loadLinkStack();
  if (vocabularySpine < epub->getSpineItemsCount()) {
    currentSpineIndex = vocabularySpine;
    nextPageNumber = vocabularyPage;
    cachedSpineIndex = currentSpineIndex;
    cachedVisibleTextOffset.reset();
    cachedChapterTotalPageCount = 0;
    if (vocabularyOffset != UINT32_MAX) pendingOffsetJump = vocabularyOffset;
  }
  loadCachedBookmarks();
  if (!CLIPPINGS.loadForBook(epub->getPath(), epub->getTitle(), epub->getAuthor(), "epub")) {
    LOG_ERR("CLIP", "Failed to load clippings for %s", epub->getPath().c_str());
  }
  if (SETTINGS.prepareBookOnOpen) {
    if (auto preparation = bookprep::onOpen(renderer, mappedInput, epub, std::move(preparationBackdrop)))
      startActivityForResult(std::move(preparation), [this](const ActivityResult&) { requestUpdate(); });
  }
  return true;
}

ChapterPosition EpubReaderActivity::chapterPosition() const {
  if (section && !formatReflowPending) return {section->currentPage, section->estimatedTotalPages()};
  return {nextPageNumber, cachedChapterTotalPageCount};
}

int EpubReaderActivity::bookPercentFor(const ChapterPosition& position) const {
  if (!epub || epub->getBookSize() == 0 || !position.hasTotal()) return 0;
  // The page index can run past the chapter's estimated total while it is still
  // building, so the fraction is clamped before the cast.
  const float fraction = epub->calculateProgress(currentSpineIndex, position.chapterFraction());
  return static_cast<int>(std::clamp(fraction, 0.0f, 1.0f) * 100.0f + 0.5f);
}

void EpubReaderActivity::openReaderMenu() {
  ensureCompletionStatus();
  releasePageReuse();
  pendingManualTurn = 0;
  if (usesToolbarMenu()) {
    // Reached from a child activity's result handler (footnotes, bookmarks,
    // go-to-percent... cancelled back to the menu), so the framebuffer holds
    // that screen, not the page: re-render the page and let renderBook() put
    // the toolbar on top. The in-reader fast path is openOverlay().
    overlay = Overlay::Toolbar;
    focusedTool = 0;
    panelHoldJumped = false;
    panelCursorShown = !mappedInput.hasTouch();
    if (!toolbarUi) toolbarUi = std::make_unique<ReaderToolbarUi>(renderer);
    toolbarUi->begin();
    discardOverlayPage();
    requestUpdate();
    return;
  }
  // Child screens (chapter list, text settings) release the section to free its
  // pagination buffers; chapterPosition() covers that with the cached position.
  const ChapterPosition position = chapterPosition();
  const int bookProgressPercent = bookPercentFor(position);

  startActivityForResult(std::make_unique<EpubReaderMenuActivity>(
                             renderer, mappedInput, epub->getTitle(), position.displayPage(), position.totalPages,
                             bookProgressPercent, SETTINGS.orientation, !currentPageFootnotes.empty(),
                             !cachedBookmarks.empty(), CLIPPINGS.hasClippings(), completionMenuLabelId()),
                         [this](const ActivityResult& result) {
                           const auto& menu = std::get<MenuResult>(result.data);

                           if (SETTINGS.orientation != menu.orientation) {
                             applyOrientation(menu.orientation);
                           }

                           toggleAutoPageTurn(menu.pageTurnOption);
                           if (!result.isCancelled) {
                             onReaderMenuConfirm(static_cast<EpubReaderMenuActivity::MenuAction>(menu.action));
                           }
                         });
}

bool EpubReaderActivity::buildTickHeapGate() {
  const size_t freeHeap = ESP.getFreeHeap();
  const size_t maxBlock = ESP.getMaxAllocHeap();
  buildHeapPaused = freeHeap < BACKGROUND_BUILD_MIN_FREE_HEAP || maxBlock < BACKGROUND_BUILD_MIN_MAX_ALLOC;
  return !buildHeapPaused;
}

void EpubReaderActivity::showIndexingPopup(const uint32_t pageLoans) {
  // A loan clears the page bytes; the panel itself still holds the book.
  if (pageBufferStale || !renderer.hasFrameBuffer() || renderer.frameBufferLoanCount() != pageLoans) return;
  GUI.drawPopup(renderer, tr(STR_INDEXING));
  pagesUntilFullRefresh = 1;
}

void EpubReaderActivity::openDictionaryWordSelect() {
  if (!bookDictionaries.selected(false, SETTINGS.dictionaryName)[0] &&
      !bookDictionaries.selected(true, SETTINGS.translationDictionaryName)[0]) {
    showDictionaryMessage = true;
    dictionaryMessageTime = millis();
    requestUpdate();
    return;
  }
  startClipSelection(-1, -1, ClipSelectionActivity::Mode::Lookup);
}

void EpubReaderActivity::startClipSelection(const int initialX, const int initialY,
                                            const ClipSelectionActivity::Mode mode) {
  std::optional<RenderLock> lock;
  if (!RenderLock::heldByCurrentTask()) lock.emplace();
  releasePageReuse();
  if (!section || !epub || section->currentPage < 0 || section->currentPage >= section->pageCount) return;

  const int pageNumber = section->currentPage;
  const uint16_t pageCount = section->pageCount;
  const uint16_t spineIndex = static_cast<uint16_t>(currentSpineIndex);
  std::vector<std::unique_ptr<Page>> pages;
  pages.reserve(CLIP_SELECTION_PAGE_WINDOW);
  for (int pageOffset = 0; pageOffset < CLIP_SELECTION_PAGE_WINDOW && pageNumber + pageOffset < pageCount;
       ++pageOffset) {
    auto page = section->loadPage(pageNumber + pageOffset);
    if (!page) break;
    if (pageOffset == 0 && initialX >= 0 && CLIPPINGS.hasClippings()) {
      const int clippingIndex = clippingAtPoint(*page, initialX, initialY);
      if (clippingIndex >= 0) {
        const bool removed = CLIPPINGS.removeClippingAt(static_cast<size_t>(clippingIndex));
        if (!removed) LOG_ERR("CLIP", "Failed to delete highlighted clipping %d", clippingIndex);
        clippingMessage = removed ? StrId::STR_CLIPPING_REMOVED : StrId::STR_CLIPPING_DELETE_FAILED;
        showClippingMessage = true;
        clippingMessageTime = millis();
        requestUpdate();
        return;
      }
    }
    pages.push_back(std::move(page));
  }
  if (pages.empty()) {
    LOG_ERR("CLIP", "Failed to load page %d for clipping selection", pageNumber);
    requestUpdate();
    return;
  }

  if (buildViewportWidth == 0 || buildViewportHeight == 0) {
    LOG_ERR("CLIP", "Cannot anchor clipping before the reader viewport is initialized");
    requestUpdate();
    return;
  }
  const uint32_t layoutSignature =
      readerRenderSpecSignature(SETTINGS.readerRenderSpec(buildViewportWidth, buildViewportHeight));

  int marginTop, marginRight, marginBottom, marginLeft;
  renderer.getOrientedViewableTRBL(&marginTop, &marginRight, &marginBottom, &marginLeft);
  marginTop += SETTINGS.screenMargin + UITheme::getReaderTopBarHeight();
  marginLeft += SETTINGS.screenMargin;

  std::string chapterTitle;
  const int tocIndex = epub->getTocIndexForSpineIndex(currentSpineIndex);
  if (tocIndex >= 0) chapterTitle = epub->getTocItem(tocIndex).title;
  std::string bookTitle = epub->getTitle();
  std::string author = epub->getAuthor();

  auto activity = makeUniqueNoThrow<ClipSelectionActivity>(renderer, mappedInput, std::move(pages), marginLeft,
                                                           marginTop, initialX, initialY, mode);
  if (!activity) {
    LOG_ERR("CLIP", "Failed to allocate clipping selection activity");
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(activity), [this, spineIndex, pageNumber, pageCount, layoutSignature,
                                               bookTitle = std::move(bookTitle), author = std::move(author),
                                               chapterTitle = std::move(chapterTitle)](const ActivityResult& result) {
    if (result.isCancelled) {
      requestUpdate();
      return;
    }
    const auto& clipping = std::get<ClippingResult>(result.data);
    const uint16_t startPage = static_cast<uint16_t>(pageNumber + clipping.startPageOffset);
    const uint16_t endPage = static_cast<uint16_t>(pageNumber + clipping.endPageOffset);
    if (clipping.action != ClippingResult::Action::Lookup && clipping.action != ClippingResult::Action::Translate &&
        section && currentSpineIndex == spineIndex && endPage < section->pageCount) {
      section->currentPage = endPage;
      currentPageVisibleOffset = section->getVisibleTextOffsetForPage(endPage);
    }
    if (clipping.action == ClippingResult::Action::Bookmark) {
      addBookmark();
      showBookmarkMessage = true;
      bookmarkMessageTime = millis();
      requestUpdate();
      return;
    }
    if (clipping.action == ClippingResult::Action::Lookup || clipping.action == ClippingResult::Action::Translate) {
      auto page = section && currentSpineIndex == spineIndex && startPage < section->pageCount
                      ? section->loadPage(startPage)
                      : nullptr;
      if (!page) {
        LOG_ERR("CLIP", "Failed to load lookup page");
        requestUpdate();
        return;
      }
      int top, right, bottom, left;
      renderer.getOrientedViewableTRBL(&top, &right, &bottom, &left);
      // One bounded source context travels with this explicit lookup; the
      // reader remains stacked at its current page and owns no vocabulary cache.
      auto source = makeUniqueNoThrow<vocabulary::Context>();
      if (source) {
        const bool fits = vocabulary::copy(source->path, sizeof(source->path), epub->getPath().c_str()) &&
                          vocabulary::copy(source->book, sizeof(source->book), bookTitle.c_str()) &&
                          vocabulary::copy(source->author, sizeof(source->author), author.c_str()) &&
                          vocabulary::copy(source->chapter, sizeof(source->chapter), chapterTitle.c_str());
        source->fragment = !vocabulary::copy(source->sentence, sizeof(source->sentence), clipping.sentence.c_str()) ||
                           clipping.sentenceFragment;
        source->spine = spineIndex;
        source->page = startPage;
        source->offset = clipping.startOffset;
        if (!fits) source.reset();
      } else
        LOG_ERR("WORDS", "OOM: source context %u bytes", unsigned(sizeof(vocabulary::Context)));
      auto lookup = makeUniqueNoThrow<DictionaryWordSelectActivity>(
          renderer, mappedInput, std::move(page), left + SETTINGS.screenMargin,
          top + SETTINGS.screenMargin + UITheme::getReaderTopBarHeight(), clipping.text, std::move(source),
          epub->getLanguage(), true, clipping.action == ClippingResult::Action::Translate,
          bookDictionaries.selected(clipping.action == ClippingResult::Action::Translate,
                                    clipping.action == ClippingResult::Action::Translate
                                        ? SETTINGS.translationDictionaryName
                                        : SETTINGS.dictionaryName),
          freeink::ui::makeRect(clipping.lookupX, clipping.lookupY, clipping.lookupWidth, clipping.lookupHeight));
      if (!lookup) {
        LOG_ERR("CLIP", "OOM: dictionary lookup activity");
        requestUpdate();
        return;
      }
      startActivityForResult(std::move(lookup), [this](const ActivityResult&) { requestUpdate(); });
      return;
    }
    const uint16_t paragraphIndex =
        section ? section->getParagraphIndexForPage(startPage).value_or(UINT16_MAX) : UINT16_MAX;
    const auto addResult = CLIPPINGS.addClipping(
        spineIndex, startPage, endPage, pageCount, clipping.startWordIndex, clipping.endWordIndex, clipping.wordCount,
        chapterTitle.c_str(), paragraphIndex, clipping.text, layoutSignature, clipping.startOffset, clipping.endOffset);
    bool exported = false;
    if (addResult == ClippingStore::AddResult::Added) {
      exported = ClippingsManager::saveClipping(bookTitle, author, chapterTitle, startPage + 1, clipping.text);
    }
    clippingMessage = addResult == ClippingStore::AddResult::LimitReached ? StrId::STR_CLIPPING_LIMIT_REACHED
                      : exported                                          ? StrId::STR_CLIPPING_SAVED
                      : addResult == ClippingStore::AddResult::Added      ? StrId::STR_CLIPPING_EXPORT_FAILED
                                                                          : StrId::STR_CLIPPING_FAILED;
    showClippingMessage = true;
    clippingMessageTime = millis();
    requestUpdate();
  });
}

void EpubReaderActivity::openFootnoteSelect(const bool reopenMenuOnCancel) {
  std::optional<RenderLock> lock;
  if (!RenderLock::heldByCurrentTask()) lock.emplace();
  releasePageReuse();
  if (!section || currentPageFootnotes.empty()) return;
  if (currentPageFootnotes.size() == 1) {
    navigateToHref(currentPageFootnotes[0].href, true);
    return;
  }

  auto page = section->loadPage(section->currentPage);
  if (!page) return;

  int orientedMarginTop, orientedMarginRight, orientedMarginBottom, orientedMarginLeft;
  renderer.getOrientedViewableTRBL(&orientedMarginTop, &orientedMarginRight, &orientedMarginBottom,
                                   &orientedMarginLeft);
  orientedMarginTop += SETTINGS.screenMargin + UITheme::getReaderTopBarHeight();
  orientedMarginLeft += SETTINGS.screenMargin;
  auto selector = makeUniqueNoThrow<EpubReaderFootnoteSelectActivity>(renderer, mappedInput, std::move(page),
                                                                      orientedMarginLeft, orientedMarginTop);
  if (!selector) {
    LOG_ERR("ERA", "OOM: EpubReaderFootnoteSelectActivity");
    return;
  }
  startActivityForResult(std::move(selector), [this, reopenMenuOnCancel](const ActivityResult& result) {
    if (result.isCancelled) {
      if (reopenMenuOnCancel) {
        openReaderMenu();
      } else {
        requestUpdate();
      }
      return;
    }
    const auto& footnoteResult = std::get<FootnoteResult>(result.data);
    navigateToHref(footnoteResult.href, true);
    requestUpdate();
  });
}

void EpubReaderActivity::openSavedItemsMenu() {
  releasePageReuse();
  RenderLock lock;
  if (!toolbarUi) {
    toolbarUi = makeUniqueNoThrow<ReaderToolbarUi>(renderer);
    if (!toolbarUi) {
      LOG_ERR("ERS", "OOM: saved items menu");
      requestUpdate();
      return;
    }
  }
  mappedInput.resetHomeButtonInput();
  toolbarUi->begin();
  overlay = Overlay::Text;
  textSheetTab = 3;
  textListPage = 0;
  panelIndex = 0;
  panelHoldJumped = false;
  panelCursorShown = !mappedInput.hasTouch();
  toolbarUi->nav().reset();
  buildMoreActions();
  discardOverlayPage();
  requestUpdate();
}

void EpubReaderActivity::openClippings() {
  releasePageReuse();
  auto list = makeUniqueNoThrow<EpubReaderClippingListActivity>(renderer, mappedInput, epub);
  if (!list) {
    LOG_ERR("CLIP", "OOM: clipping list");
    return;
  }
  startActivityForResult(std::move(list), [this](const ActivityResult& result) {
    if (!result.isCancelled) {
      if (const auto* progress = std::get_if<ProgressChangeResult>(&result.data)) {
        applyProgressChange(*progress);
        return;
      }
    }
    openSavedItemsMenu();
  });
}

bool EpubReaderActivity::readerMenuTapAllowed(int x, int y) const {
  return !EpubReaderUtils::linkAtPoint(currentPageLinks, x, y, currentPageLinkMarginLeft, currentPageLinkMarginTop);
}
void EpubReaderActivity::openTextSettings() {
  ensureCompletionStatus();
  if (!epub || loadFailurePopup.isActive() || isAtEndOfBook()) return;
  {
    RenderLock lock;
    readingPace.pause();
    automaticPageTurnActive = false;
    pendingManualTurn = 0;
    pendingManualTurnTouch = false;
    overlayPopup.dismiss();
  }
  openOverlay(Overlay::Text);
}

void EpubReaderActivity::loop() {
  int inputX = 0, inputY = 0;
  using Button = MappedInputManager::Button;
  const bool readerInput = mappedInput.isScreenTouchHeld(inputX, inputY) || mappedInput.wasScreenTouchReleased() ||
                           mappedInput.hasRemoteClick() || mappedInput.isPressed(Button::PageForward) ||
                           mappedInput.isPressed(Button::PageBack) || mappedInput.isPressed(Button::Back) ||
                           mappedInput.isPressed(Button::Confirm) || mappedInput.isPressed(Button::Left) ||
                           mappedInput.isPressed(Button::Right);
  if (readerInput || mappedInput.isPressed(Button::Power)) prefetchInterrupted.store(true);
  if (loadFailurePopup.isActive()) {
    loadFailurePopup.handleInput(mappedInput, [this] { requestUpdate(); });
    return;
  }
  if (!epub) {
    finish();
    return;
  }

  rememberBookOnceRendered();
  recordEndOfBook();

  // Someone else turned the screen while this reader was stacked (the control
  // center's orientation tile). Reflow before the next render, or the page
  // would be drawn with a layout built for the previous frame size.
  if (appliedOrientation != SETTINGS.orientation) {
    applyOrientation(SETTINGS.orientation);
    requestUpdate();
    return;
  }

  if (bookOrbitWakePending.load(std::memory_order_relaxed) && runBookOrbitWakeSync(readerInput)) return;
  if (!readerInput) flushBookOrbitPending();

  {
    RenderLock lock(RenderLock::Mode::Try);
    if (lock.ownsLock() && !formatReflowPending && overlay == Overlay::None && section && !section->isBuilding() &&
        section->isPartial() && buildViewportWidth > 0 && !partialRebuildStartFailed &&
        chapterBackgroundBuildMayRun() &&
        section->currentPage + PARTIAL_REBUILD_START_MARGIN >= static_cast<int>(section->pageCount)) {
      const ReaderRenderSpec buildSpec = SETTINGS.readerRenderSpec(buildViewportWidth, buildViewportHeight);
      if (!section->startBuild(buildSpec)) {
        partialRebuildStartFailed = true;
        LOG_ERR("ERS", "Failed to start deferred partial extension build");
      } else {
        LOG_DBG("ERS", "Reader near partial watermark (%d/%d), resuming extension build", section->currentPage,
                section->pageCount);
      }
    }
  }

  {
    RenderLock lock(RenderLock::Mode::Try);
    // Not under an open toolbar or panel: those repaint straight onto the page in the framebuffer.
    if (lock.ownsLock() && overlay == Overlay::None && section && !section->isFormatBuild() &&
        backgroundBuildWanted() && buildTickHeapGate()) {
      // A build step can lend the framebuffer (image probes), which hands it back white while the
      // panel still shows the page; redraw so nothing is later painted over the blank buffer.
      const uint32_t loansBefore = renderer.frameBufferLoanCount();
      const bool built = section->buildSomeMore(BACKGROUND_BUILD_PAGES_PER_TICK, nullptr, BACKGROUND_BUILD_SLICE_MS);
      if (renderer.frameBufferLoanCount() != loansBefore) {
        pageBufferStale = true;
        requestUpdate();
      }
      if (!built) {
        LOG_ERR("ERS", "Background section build failed");
        resetSection();
        requestUpdate();
      } else if (section->isBuildComplete() && applyDeferredReposition()) {
        requestUpdate();
      }
    }
  }

  queueFormatIdle();
  const bool atEndOfBook = currentSpineIndex > 0 && currentSpineIndex >= epub->getSpineItemsCount();
  clearEndOfBookOptionsIfNeeded();

  if (SETTINGS.removeReadBooksFromRecents) {
    if (atEndOfBook && !recentsEntryRemoved) {
      recentsEntryRemoved = RECENT_BOOKS.removeByPath(epub->getPath());
    } else if (!atEndOfBook && recentsEntryRemoved) {
      RECENT_BOOKS.addBook(epub->getPath(), epub->getTitle(), epub->getAuthor(), epub->getThumbBmpPath());
      recentsEntryRemoved = false;
    }
  }

  if (atEndOfBook) {
    pendingReadFolderMove = SETTINGS.moveFinishedToReadFolder && !isInReadFolder(epub->getPath());
  } else {
    pendingReadFolderMove = false;
  }

  const auto touch =
      ReaderUtils::detectTouchPageTurn(renderer, mappedInput, ReaderUtils::isRtlBookLanguage(epub->getLanguage()));

  if (showBookmarkMessage && (millis() - bookmarkMessageTime) >= ReaderUtils::BOOKMARK_MESSAGE_DURATION_MS) {
    showBookmarkMessage = false;
    requestUpdate();
  }

  if (showDictionaryMessage && (millis() - dictionaryMessageTime) >= ReaderUtils::BOOKMARK_MESSAGE_DURATION_MS) {
    showDictionaryMessage = false;
    requestUpdate();
  }

  // The toolbar reader menu owns all input while shown, ahead of the automatic page turn
  // below: the More panel's rate popup switches automatic turning on and leaves the panel
  // open, so the timer must neither flip the page under it nor eat the panel's next
  // Confirm/Back release.
  if (overlay != Overlay::None) {
    if (usesToolbarMenu() || overlay == Overlay::Text) {
      // Hold the interval at zero elapsed so closing the panel starts a fresh one.
      lastPageTurnTime = millis();
      handleOverlayInput();
      // Consume this tick's one-shot input before an idle format commit.
      if (textFormatPending && !formatInputHeld()) {
        commitTextFormat();
        requestUpdate();
      }
      queueFormatIdle();
      return;
    }
    // The style was switched off while an overlay was up (Settings reached via
    // the More panel); fall back to the clean page.
    overlay = Overlay::None;
    discardOverlayPage();
    requestUpdate();
    return;
  }

  if (handleContentsReturn()) return;
  int selectionX = 0;
  int selectionY = 0;
  if (!atEndOfBook && SETTINGS.touchReaderControls && mappedInput.wasScreenLongPress(selectionX, selectionY)) {
    automaticPageTurnActive = false;
    pendingManualTurn = 0;
    startClipSelection(selectionX, selectionY);
    return;
  }

  switch (mappedInput.homeButtonAction()) {
    case HomeButtonAction::ReaderMenu:
    case HomeButtonAction::Bookmark:
    case HomeButtonAction::Sync:
    case HomeButtonAction::Dictionary:
    case HomeButtonAction::Footnotes:
    case HomeButtonAction::CreateClipping:
      automaticPageTurnActive = false;
      break;
    default:
      break;
  }

  if (automaticPageTurnActive) {
    const bool touchStopsAutoTurn = ReaderUtils::isTouchMenuGesture(renderer, mappedInput);
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
        mappedInput.wasReleased(MappedInputManager::Button::Back) || touchStopsAutoTurn) {
      automaticPageTurnActive = false;
      if (touchStopsAutoTurn) haptic_feedback::touchAction();
      requestUpdate();
      return;
    }

    if (!section) {
      requestUpdate();
      return;
    }

    if (RenderLock::peek()) {
      lastPageTurnTime = millis();
      return;
    }

    if ((millis() - lastPageTurnTime) >= pageTurnDuration) {
      const bool succeeded = pageTurn(true);
      notePageTurn(true, succeeded);
      requestUpdate();
      return;
    }
  }

  if (showClippingMessage && (millis() - clippingMessageTime) >= ReaderUtils::BOOKMARK_MESSAGE_DURATION_MS) {
    showClippingMessage = false;
    requestUpdate();
  }

  // While the end-of-book suggestion menu is up it owns Confirm/Back/navigation, so it
  // gets this tick's input first and the long-press shortcuts below stay inert behind it
  // -- a hold there must not drop a bookmark onto the suggestion screen or paint the
  // dictionary word picker over it. Anything the menu does not handle (long-press Back to
  // the file browser, say) still falls through to the regular handlers.
  if (handleEndOfBookMenu()) {
    return;
  }
  const bool endOfBookMenuOpen = endOfBookMenuActive();

  const unsigned long confirmHoldMs = confirmLongPressThreshold();
  // wasLongPressed() suppresses the release that follows it, so leave it unpolled while
  // the end-of-book menu owns Confirm -- otherwise the menu never sees that release.
  const bool confirmLongPressed = !endOfBookMenuOpen && confirmHoldMs != 0 &&
                                  mappedInput.wasLongPressed(MappedInputManager::Button::Confirm, confirmHoldMs);
  const bool confirmReleased = mappedInput.wasReleased(MappedInputManager::Button::Confirm);
  if (confirmLongPressed) {
    switch (SETTINGS.longPressMenuFunction) {
      case CrossPointSettings::LP_MENU_BOOKMARK:
        addBookmark();
        showBookmarkMessage = true;
        bookmarkMessageTime = millis();
        requestUpdate();
        break;
      case CrossPointSettings::LP_MENU_KOSYNC:
        if (launchKOReaderSync()) {
          return;
        }
        break;
      case CrossPointSettings::LP_MENU_DICTIONARY:
        openDictionaryWordSelect();
        return;
      case CrossPointSettings::LP_MENU_CREATE_CLIPPING:
        startClipSelection();
        return;
      case CrossPointSettings::LP_MENU_READER_MENU:
      case CrossPointSettings::LP_MENU_DISABLED:
      default:
        break;
    }
  }

  if (!endOfBookMenuOpen) {
    switch (mappedInput.homeButtonAction()) {
      case HomeButtonAction::Bookmark:
        if (!showBookmarkMessage) {
          addBookmark();
          showBookmarkMessage = true;
          bookmarkMessageTime = millis();
          requestUpdate();
        }
        return;
      case HomeButtonAction::Sync:
        launchKOReaderSync();
        return;
      case HomeButtonAction::Dictionary:
        if (!showDictionaryMessage) openDictionaryWordSelect();
        return;
      case HomeButtonAction::CreateClipping:
        startClipSelection();
        return;
      case HomeButtonAction::ReaderMenu:
        openTextSettings();
        return;
      default:
        break;
    }
  }

  // Link taps take priority over the reader-menu and page-turn zones.
  if (!atEndOfBook && !currentPageLinks.empty() && SETTINGS.touchReaderControls && mappedInput.hasTouch()) {
    int touchX = 0;
    int touchY = 0;
    if (mappedInput.wasScreenTapped(touchX, touchY)) {
      const auto* link = EpubReaderUtils::linkAtPoint(currentPageLinks, touchX, touchY, currentPageLinkMarginLeft,
                                                      currentPageLinkMarginTop);
      if (link) {
        navigateToHref(link->href, true);
        haptic_feedback::touchAction();
        return;
      }
    }
  }

  const bool touchMenu = ReaderUtils::isTouchMenuGesture(renderer, mappedInput);
  if (confirmReleased || touchMenu) {
    if (touchMenu) haptic_feedback::touchAction();
    // Toolbar style: the page is on screen and in the framebuffer, so paint the
    // toolbar over it (one refresh) instead of pushing a full-screen menu.
    if (usesToolbarMenu() && section) {
      pendingManualTurn = 0;
      openOverlay(Overlay::Toolbar);
    } else {
      openReaderMenu();
    }
  }

  if (footnoteDepth > 0 && !mappedInput.wasBackGesture() && mappedInput.wasReleased(MappedInputManager::Button::Back) &&
      mappedInput.getHeldTime() < ReaderUtils::GO_BACK_OR_HOME_MS) {
    restoreSavedPosition();
    return;
  }

  if (handleBackNavigation()) {
    return;
  }

  if (SETTINGS.shortPwrBtn == CrossPointSettings::SHORT_PWRBTN::CREATE_CLIPPING &&
      mappedInput.wasReleased(MappedInputManager::Button::Power) &&
      !mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    startClipSelection();
    return;
  }

  if ((!endOfBookMenuOpen && mappedInput.homeButtonAction() == HomeButtonAction::Footnotes) ||
      (SETTINGS.shortPwrBtn == CrossPointSettings::SHORT_PWRBTN::FOOTNOTES &&
       mappedInput.wasReleased(MappedInputManager::Button::Power) &&
       !mappedInput.wasReleased(MappedInputManager::Button::Down))) {
    if (footnoteDepth > 0) {
      restoreSavedPosition();
    } else {
      openFootnoteSelect(false);
    }
    return;
  }

  constexpr unsigned long kMinManualTurnGapMs = 200;
  const bool turnGuardActive = RenderLock::peek() || (millis() - lastPageTurnTime) < kMinManualTurnGapMs;
  if (pendingManualTurn != 0 && !turnGuardActive) {
    if (!section) {
      pendingManualTurn = 0;
      return;
    }
    const bool forward = pendingManualTurn > 0;
    pendingManualTurn = 0;
    const bool succeeded = pageTurn(forward);
    notePageTurn(forward, succeeded);
    if (succeeded && pendingManualTurnTouch) haptic_feedback::touchAction();
    pendingManualTurnTouch = false;
    requestUpdate();
    return;
  }

  auto [prevTriggered, nextTriggered, fromTilt] = ReaderUtils::detectPageTurn(mappedInput);
  prevTriggered = prevTriggered || touch.prev;
  nextTriggered = nextTriggered || touch.next;
  if (!prevTriggered && !nextTriggered) {
    idlePrefetchNextPage();
    return;
  }

  if (handleEndOfBookPageTurn(prevTriggered, nextTriggered)) {
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Power) &&
      mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    return;
  }

  const unsigned long heldMs = (touch.prev || touch.next) ? touch.heldMs : mappedInput.getHeldTime();
  const bool longPress = !fromTilt && heldMs >= ReaderUtils::SKIP_HOLD_MS;
  if (longPress && SETTINGS.longPressButtonBehavior == SETTINGS.CHAPTER_SKIP) {
    const bool succeeded = skipPages(nextTriggered ? 1 : -1);
    notePageTurn(false, succeeded);
    if (succeeded && (touch.prev || touch.next)) haptic_feedback::touchAction(true);
    requestUpdate();
    return;
  }

  if (longPress && SETTINGS.longPressButtonBehavior == SETTINGS.ORIENTATION_CHANGE) {
    const uint8_t newOrientation =
        uint8_t(rotation::readerNext(SETTINGS.orientation, !nextTriggered, SETTINGS.readerFourOrientations != 0));
    applyOrientation(newOrientation);
    if (touch.prev || touch.next) haptic_feedback::touchAction(true);
    requestUpdate();
    return;
  }

  if (!section) {
    requestUpdate();
    return;
  }

  if (turnGuardActive) {
    pendingManualTurn = prevTriggered ? -1 : 1;
    pendingManualTurnTouch = touch.prev || touch.next;
    return;
  }

  const bool succeeded = pageTurn(!prevTriggered);
  notePageTurn(!prevTriggered, succeeded);
  if (succeeded && (touch.prev || touch.next)) haptic_feedback::touchAction();
  requestUpdate();
}

void EpubReaderActivity::jumpToPercent(int percent) {
  if (!epub) return;
  const size_t bookSize = epub->getBookSize();
  if (bookSize == 0) return;

  percent = clampPercent(percent);

  size_t targetSize =
      (bookSize / 100) * static_cast<size_t>(percent) + (bookSize % 100) * static_cast<size_t>(percent) / 100;
  if (percent >= 100) targetSize = bookSize - 1;

  const int spineCount = epub->getSpineItemsCount();
  if (spineCount == 0) return;

  int targetSpineIndex = spineCount - 1;
  size_t prevCumulative = 0;

  for (int i = 0; i < spineCount; i++) {
    const size_t cumulative = epub->getCumulativeSpineItemSize(i);
    if (targetSize <= cumulative) {
      targetSpineIndex = i;
      prevCumulative = (i > 0) ? epub->getCumulativeSpineItemSize(i - 1) : 0;
      break;
    }
  }

  const size_t cumulative = epub->getCumulativeSpineItemSize(targetSpineIndex);
  const size_t spineSize = (cumulative > prevCumulative) ? (cumulative - prevCumulative) : 0;
  pendingSpineProgress =
      (spineSize == 0) ? 0.0f : static_cast<float>(targetSize - prevCumulative) / static_cast<float>(spineSize);
  pendingSpineProgress = std::clamp(pendingSpineProgress, 0.0f, 1.0f);

  {
    RenderLock lock;
    clearDeferredReposition();
    currentSpineIndex = targetSpineIndex;
    nextPageNumber = 0;
    pendingPercentJump = true;
    resetSection();
  }
  requestUpdate();
}

void EpubReaderActivity::applyProgressChange(const ProgressChangeResult& sync) {
  RenderLock lock;
  applyProgressChangeLocked(sync);
}

void EpubReaderActivity::applyProgressChangeLocked(const ProgressChangeResult& sync) {
  if (sync.hasVisibleTextOffset && sync.spineIndex >= 0 && sync.spineIndex < epub->getSpineItemsCount()) {
    clearDeferredReposition();
    const auto page = section && currentSpineIndex == sync.spineIndex
                          ? section->getPageForVisibleTextOffset(sync.visibleTextOffset)
                          : std::nullopt;
    if (page) {
      section->currentPage = *page;
    } else {
      currentSpineIndex = sync.spineIndex;
      pendingOffsetJump = sync.visibleTextOffset;
      nextPageNumber = std::max(0, sync.page);
      resetSection();
    }
    requestUpdate();
    return;
  }

  int targetSpineIndex = sync.spineIndex;
  int targetPage = sync.page;
  const int activeTotalPages = section ? section->estimatedTotalPages() : 0;
  const bool cachedPageMatchesActiveSection = section && sync.totalPages > 0 && currentSpineIndex == sync.spineIndex &&
                                              sync.page >= 0 && sync.page < sync.totalPages &&
                                              activeTotalPages == sync.totalPages;

  if (!cachedPageMatchesActiveSection && sync.hasSavedProgress) {
    const int totalPages = section ? section->estimatedTotalPages() : cachedChapterTotalPageCount;
    CrossPointPosition fallback =
        ProgressMapper::toCrossPoint(epub, {sync.xpath, sync.percentage}, renderer, currentSpineIndex, totalPages);
    targetSpineIndex = fallback.spineIndex;
    targetPage = fallback.pageNumber;
  }

  clearDeferredReposition();

  if (currentSpineIndex != targetSpineIndex) {
    currentSpineIndex = targetSpineIndex;
    nextPageNumber = targetPage;
    resetSection();
  } else if (section && section->currentPage != targetPage) {
    const int clampedTargetPage = std::max(0, targetPage);
    section->currentPage = clampedTargetPage;
  } else if (!section) {
    nextPageNumber = targetPage;
  }
  requestUpdate();
}

void EpubReaderActivity::onReaderMenuConfirm(EpubReaderMenuActivity::MenuAction action, const bool contentsSheet) {
  releasePageReuse();
  auto progressChangeResultHandler = [this](const ActivityResult& result) {
    loadCachedBookmarks();
    if (result.isCancelled) {
      openSavedItemsMenu();
    } else {
      const auto& sync = std::get<ProgressChangeResult>(result.data);

      applyProgressChange(sync);
    }
  };

  switch (action) {
    case EpubReaderMenuActivity::MenuAction::BOOK_DICTIONARIES:
      openBookDictionaries();
      break;
    case EpubReaderMenuActivity::MenuAction::FINISH_BOOK:
      requestFinishBook();
      break;
    case EpubReaderMenuActivity::MenuAction::SEARCH: {
      auto search = makeUniqueNoThrow<BookSearchActivity>(renderer, mappedInput, epub);
      if (!search) {
        LOG_ERR("ERS", "OOM: book search");
        break;
      }
      startActivityForResult(std::move(search), [this](const ActivityResult& result) {
        if (result.isCancelled) {
          requestUpdate();
          return;
        }
        const auto* chapter = std::get_if<ChapterResult>(&result.data);
        if (!chapter) {
          requestUpdate();
          return;
        }
        RenderLock lock;
        clearDeferredReposition();
        currentSpineIndex = chapter->spineIndex;
        pendingAnchor = chapter->anchor;
        nextPageNumber = 0;
        resetSection();
        requestUpdate();
      });
      break;
    }
    case EpubReaderMenuActivity::MenuAction::BOOK_STATS: {
      flushReaderSession();
      auto stats = makeUniqueNoThrow<ReadingStatsActivity>(renderer, mappedInput);
      if (!stats) {
        LOG_ERR("ERS", "OOM: book statistics");
        break;
      }
      stats->openBook(KOReaderDocumentId::calculate(bookPath).c_str(), epub->getTitle().c_str(),
                      epub->getAuthor().c_str());
      startActivityForResult(std::move(stats), [this](const ActivityResult&) {
        completionStatusPending = true;
        requestUpdate();
      });
      break;
    }
    case EpubReaderMenuActivity::MenuAction::SELECT_CHAPTER: {
      const int spineIdx = currentSpineIndex;
      ContentsPosition origin;
      // Release the section while the chapter list is up (mirrors the
      // TEXT_SETTINGS path): picking a chapter resets it anyway, and its
      // tens-of-KB footprint is the difference between the chapter list
      // holding its CJK glyph arena (RAM-only repaints) and re-reading
      // glyphs from SD on every row step. Cancel restores via the same
      // cached-position rebuild TEXT_SETTINGS uses.
      {
        RenderLock lock;
        origin = {currentSpineIndex, static_cast<uint32_t>(chapterPosition().pageIndex),
                  currentPageVisibleOffset.value_or(UINT32_MAX)};
        rememberReflowTarget();
        resetSection();
      }
      auto chapters = makeUniqueNoThrow<EpubReaderChapterSelectionActivity>(renderer, mappedInput, epub, spineIdx);
      if (!chapters) {
        LOG_ERR("ERS", "OOM: chapter list");
        requestUpdate();
        break;
      }
      startActivityForResult(std::move(chapters), [this, origin, contentsSheet](const ActivityResult& result) {
        if (result.isCancelled) {
          if (contentsSheet) {
            RenderLock lock;
            overlay = Overlay::Text;
            textSheetTab = 2;
            textSheetView = readertext::SheetView::Controls;
            toolbarUi->begin();
            requestUpdate();
          } else {
            openReaderMenu();
          }
          return;
        }
        const auto& chapterResult = std::get<ChapterResult>(result.data);
        RenderLock lock;
        rememberContentsPosition(origin);
        clearDeferredReposition();
        bookOrbitLayoutReposition = false;
        currentSpineIndex = chapterResult.spineIndex;
        pendingAnchor = chapterResult.anchor;
        nextPageNumber = 0;
        resetSection();
        requestUpdate();
      });
      break;
    }
    case EpubReaderMenuActivity::MenuAction::FOOTNOTES: {
      openFootnoteSelect(true);
      break;
    }
    case EpubReaderMenuActivity::MenuAction::STYLE_SETTINGS:
      openOverlay(Overlay::Text, true);
      break;
    case EpubReaderMenuActivity::MenuAction::TEXT_SETTINGS: {
      auto settings = makeUniqueNoThrow<TextSettingsActivity>(renderer, mappedInput, &sdFontSystem.registry(),
                                                              TextSettingsActivity::Tab::Family);
      if (!settings) {
        LOG_ERR("ERS", "OOM: text settings");
        requestUpdate();
        break;
      }
      startActivityForResult(std::move(settings), [this](const ActivityResult&) {
        {
          RenderLock lock;
          rememberReflowTarget();
          resetSection();
        }
        openReaderMenu();
      });
      break;
    }
    case EpubReaderMenuActivity::MenuAction::NIGHT_MODE:
      // Handled in-place by EpubReaderMenuActivity so its On/Off value updates
      // without closing the menu.
      break;
    case EpubReaderMenuActivity::MenuAction::FRONTLIGHT:
      // Handled in-place by EpubReaderMenuActivity using the live frontlight HAL.
      break;
    case EpubReaderMenuActivity::MenuAction::GO_TO_PAGE: {
      const int current = section ? section->currentPage : nextPageNumber;
      const int total = std::max(1, section ? section->pageCount : cachedChapterTotalPageCount);
      auto dialog = makeUniqueNoThrow<EpubReaderPercentSelectionActivity>(
          renderer, mappedInput, current + 1, EpubReaderPercentSelectionActivity::Unit::ChapterPage, total);
      if (!dialog) {
        LOG_ERR("ERS", "OOM: chapter page selector");
        break;
      }
      startActivityForResult(std::move(dialog), [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          RenderLock lock;
          clearDeferredReposition();
          const int page = std::get<PageResult>(result.data).page;
          if (section)
            section->currentPage = std::clamp(page, 0, std::max(0, section->pageCount - 1));
          else
            nextPageNumber = page;
        }
        requestUpdate();
      });
      break;
    }
    case EpubReaderMenuActivity::MenuAction::GO_TO_PERCENT: {
      const int initialPercent = bookPercentFor(chapterPosition());
      startActivityForResult(
          std::make_unique<EpubReaderPercentSelectionActivity>(renderer, mappedInput, initialPercent),
          [this](const ActivityResult& result) {
            if (result.isCancelled) {
              openReaderMenu();
            } else {
              jumpToPercent(std::get<PercentResult>(result.data).percent);
            }
          });
      break;
    }
    case EpubReaderMenuActivity::MenuAction::DICTIONARY: {
      openDictionaryWordSelect();
      break;
    }
    case EpubReaderMenuActivity::MenuAction::DISPLAY_QR: {
      if (section && section->currentPage >= 0 && section->currentPage < section->pageCount) {
        std::string fullText = section->getTextFromSectionFile();
        if (!fullText.empty()) {
          startActivityForResult(std::make_unique<QrDisplayActivity>(renderer, mappedInput, fullText),
                                 [this](const ActivityResult&) { openReaderMenu(); });
          break;
        }
      }
      requestUpdate();
      break;
    }
    case EpubReaderMenuActivity::MenuAction::GO_HOME: {
      onGoHome();
      return;
    }
    case EpubReaderMenuActivity::MenuAction::DELETE_CACHE: {
      {
        RenderLock lock;
        if (epub && section) {
          uint16_t backupSpine = currentSpineIndex;
          uint16_t backupPage = section->currentPage;
          uint16_t backupPageCount = section->pageCount;
          resetSection();
          epub->clearCache();
          epub->setupCacheDir();
          if (!saveProgress(backupSpine, backupPage, backupPageCount)) {
            LOG_ERR("ERS", "Failed to save progress before cache clear");
          }
        }
      }
      onGoHome();
      return;
    }
    case EpubReaderMenuActivity::MenuAction::SCREENSHOT: {
      {
        RenderLock lock;
        pendingScreenshot = true;
      }
      requestUpdate();
      break;
    }
    case EpubReaderMenuActivity::MenuAction::SYNC:
    case EpubReaderMenuActivity::MenuAction::BOOK_ORBIT_SYNC: {
      launchKOReaderSync(action == EpubReaderMenuActivity::MenuAction::BOOK_ORBIT_SYNC);
      break;
    }
    case EpubReaderMenuActivity::MenuAction::BOOKMARKS: {
      auto bookmarks = makeUniqueNoThrow<EpubReaderBookmarksActivity>(renderer, mappedInput, epub, epub->getPath());
      if (!bookmarks) {
        LOG_ERR("ERS", "OOM: bookmarks");
        openSavedItemsMenu();
        break;
      }
      startActivityForResult(std::move(bookmarks), progressChangeResultHandler);
      break;
    }
    case EpubReaderMenuActivity::MenuAction::SAVE_CLIPPING: {
      startClipSelection();
      break;
    }
    case EpubReaderMenuActivity::MenuAction::VIEW_CLIPPINGS: {
      openClippings();
      break;
    }
    case EpubReaderMenuActivity::MenuAction::TOGGLE_BOOKMARK: {
      addBookmark();
      break;
    }
  }
}

unsigned long EpubReaderActivity::confirmLongPressThreshold() const {
  switch (SETTINGS.longPressMenuFunction) {
    case CrossPointSettings::LP_MENU_BOOKMARK:
    case CrossPointSettings::LP_MENU_DICTIONARY:
    case CrossPointSettings::LP_MENU_CREATE_CLIPPING:
      return ReaderUtils::BOOKMARK_HOLD_MS;
    case CrossPointSettings::LP_MENU_KOSYNC:
      return KOREADER_STORE.hasCredentials() ? ReaderUtils::GO_HOME_MS : 0;
    case CrossPointSettings::LP_MENU_READER_MENU:
    case CrossPointSettings::LP_MENU_DISABLED:
    default:
      return 0;
  }
}

bool EpubReaderActivity::launchKOReaderSync(bool bookOrbit) {
  if (!(bookOrbit ? bookorbit::Store::get().authenticated() : KOREADER_STORE.hasCredentials())) return false;

  RenderLock renderLock;

  const int currentPage = section ? section->currentPage : nextPageNumber;
  const int totalPages = section ? section->estimatedTotalPages() : cachedChapterTotalPageCount;

  CrossPointPosition localPos = getCurrentPosition();
  SavedProgressPosition localKoPos;
  const int tocIdx = epub->getTocIndexForSpineIndex(currentSpineIndex);
  std::string localChapterName = (tocIdx >= 0) ? epub->getTocItem(tocIdx).title : "";
  const std::string savedEpubPath = epub->getPath();

  if (!saveProgress(currentSpineIndex, currentPage, totalPages)) {
    LOG_ERR("KOSync", "Aborting sync because current progress could not be saved");
    pendingSyncSaveError = true;
    requestUpdate();
    return true;
  }

  LOG_DBG("KOSync", "Releasing epub for sync (heap before: %u)", (unsigned)ESP.getFreeHeap());
  {
    if (section) {
      nextPageNumber = section->currentPage;
    }
    discardOverlayPage();
    ImageBlock::releaseRenderCache();
    ImageBlock::setExtractor(nullptr, nullptr);
    resetSection();
    if (auto* fcm = renderer.getFontCacheManager()) {
      fcm->releaseSdFontCaches();
    }
    // No rendering may run while the chapter mapper borrows the framebuffer.
    {
      GfxRenderer::FrameBufferLoan loan(renderer);
      localKoPos = ProgressMapper::toSavedProgress(epub, localPos);
    }
    // The destructor can no longer save the back-stack once epub is gone.
    if (footnoteDepth > 0) saveLinkStack();
  }
  auto sync = makeUniqueNoThrow<KOReaderSyncActivity>(
      renderer, mappedInput, savedEpubPath, localPos, std::move(localKoPos), std::move(localChapterName), bookOrbit,
      bookOrbit && !formatReflowPending ? presentedTextSpec : std::nullopt);
  if (!sync) {
    LOG_ERR("KOSync", "OOM: sync activity");
    requestUpdate();
    return true;
  }
  epub.reset();
  activityManager.replaceActivity(std::move(sync));
  return true;
}

void EpubReaderActivity::applyInitialOrientation() {
  ReaderActivity::applyInitialOrientation();
  appliedOrientation = SETTINGS.orientation;
}

void EpubReaderActivity::applyOrientation(const uint8_t orientation) {
  // Also runs when SETTINGS already holds the new value but this layout was
  // built for the old one â€” that is what an external change looks like here.
  if (SETTINGS.orientation == orientation && appliedOrientation == orientation) {
    return;
  }

  RenderLock lock(*this);
  rememberReflowTarget();

  if (SETTINGS.orientation != orientation) {
    SETTINGS.orientation = orientation;
    SETTINGS.saveToFile();
  }
  ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);
  appliedOrientation = orientation;
  resetSection();
}

void EpubReaderActivity::toggleAutoPageTurn(const uint8_t selectedPageTurnOption) {
  if (selectedPageTurnOption == 0 || selectedPageTurnOption >= std::size(PAGE_TURN_RATES)) {
    automaticPageTurnActive = false;
    return;
  }

  lastPageTurnTime = millis();
  pageTurnDuration = (1UL * 60 * 1000) / PAGE_TURN_RATES[selectedPageTurnOption];
  automaticPageTurnActive = true;

  const uint8_t statusBarHeight = UITheme::getInstance().getStatusBarHeight();
  if (statusBarHeight == 0 || statusBarHeight == UITheme::getInstance().getProgressBarHeight()) {
    RenderLock lock;
    rememberReflowTarget();
    resetSection();
  }
}

bool EpubReaderActivity::pageTurn(bool isForwardTurn) {
  const auto requestedAt = performance::timeMicros();
  RenderLock lock;
  if (!section || formatReflowPending) return false;
  clearDeferredReposition();
  const auto turned = [this, requestedAt] {
    bookOrbitLayoutReposition = false;
    finalPagePresented = false;
    performance::begin(performance::Operation::PageTurn, this, requestedAt);
    return true;
  };
  if (isForwardTurn) {
    if (section->currentPage < section->pageCount - 1 || (section->isBuilding() && !section->layoutFinished())) {
      section->currentPage++;
      lastPageTurnTime = millis();
      return turned();
    } else if (currentSpineIndex + 1 < epub->getSpineItemsCount()) {
      nextPageNumber = 0;
      currentSpineIndex++;
      resetSection();
      lastPageTurnTime = millis();
      return turned();
    } else {
      currentSpineIndex = epub->getSpineItemsCount();
      lastPageTurnTime = millis();
      return turned();
    }
  } else {
    if (section->currentPage > 0) {
      section->currentPage--;
      lastPageTurnTime = millis();
      return turned();
    } else if (currentSpineIndex > 0) {
      nextPageNumber = 0;
      pendingPageJump = std::numeric_limits<uint16_t>::max();
      currentSpineIndex--;
      resetSection();
      lastPageTurnTime = millis();
      return turned();
    }
  }
  return false;
}

bool EpubReaderActivity::skipPages(int amount) {
  if (!section || formatReflowPending) return false;
  if (amount > 0) {
    RenderLock lock;
    nextPageNumber = 0;
    currentSpineIndex++;
    resetSection();
    return true;
  } else {
    if (section->currentPage > 0) {
      section->currentPage = 0;
      return true;
    } else if (currentSpineIndex > 0) {
      RenderLock lock;
      nextPageNumber = 0;
      currentSpineIndex--;
      resetSection();
      return true;
    }
  }
  return false;
}

// Failed protected open: show the standard option dialog (wrapped message)
// instead of silently falling back to the previous screen. Exact error
// strings are set by openProtectedBook (ContentProtection.cpp).
bool EpubReaderActivity::handleLoadFailure() {
  if (loadProtectionError.empty()) return false;
  const std::string& perr = loadProtectionError;
  StrId msg = StrId::STR_DRM_PROTECTED_FILE;
  bool offerSync = false;
  if (perr == "access expired") {
    msg = StrId::STR_LOAN_EXPIRED;
  } else if (perr == "loan date unverified") {
    msg = StrId::STR_LOAN_TIME_UNVERIFIED;
    offerSync = true;
  }
  const char* options[2] = {I18N.get(offerSync ? StrId::STR_CLOCK_SYNC_NOW : StrId::STR_OK_BUTTON),
                            I18N.get(StrId::STR_OK_BUTTON)};
  loadFailurePopup.showMessage("", I18N.get(msg), options, offerSync ? 2 : 1, 0, [this, offerSync](const int index) {
    if (offerSync && index == 0) {
      beginLoanTimeSync();
      return;
    }
    finish();
  });
  requestUpdate();
  return true;  // stay alive; the popup's Back dismiss lands in loop()'s !epub finish
}

void EpubReaderActivity::beginLoanTimeSync() {
  auto wifi = makeUniqueNoThrow<WifiSelectionActivity>(renderer, mappedInput);
  if (!wifi) {
    LOG_ERR("ERS", "OOM: Wi-Fi selection for loan time sync");
    finish();
    return;
  }
  startActivityForResult(std::move(wifi), [this](const ActivityResult& result) {
    if (result.isCancelled || WiFi.status() != WL_CONNECTED) {
      finish();
      return;
    }
    GUI.drawPopup(renderer, tr(STR_SYNCING_TIME));
    const bool synced = trustedtime::syncNow(5000);
    WiFi.disconnect(false);
    delay(30);
    if (!synced) {
      // Reopening would hit the same unverified-time refusal: offer a retry.
      const char* options[2] = {I18N.get(StrId::STR_RETRY), I18N.get(StrId::STR_OK_BUTTON)};
      loadFailurePopup.showMessage("", I18N.get(StrId::STR_CLOCK_SYNC_FAIL), options, 2, 0, [this](const int index) {
        if (index == 0) {
          beginLoanTimeSync();
          return;
        }
        finish();
      });
      requestUpdate();
      return;
    }
    APP_STATE.openEpubPath = bookPath;
    APP_STATE.saveToFile();
    // Reboot straight back into this book with a clean heap (no-op on touch
    // boards, which fall through to the in-place relaunch below).
    silentRestartToReader();
    activityManager.goToReader(bookPath);
  });
}

void EpubReaderActivity::render(RenderLock&& lock) {
  finalPagePresented = false;
  formatIdleQueued = false;
  renderedOperation = performance::current();
  if (loadFailurePopup.isActive()) {
    renderer.clearScreen();
    loadFailurePopup.processRender(renderer, mappedInput);
    return;
  }
  ReaderActivity::render(std::move(lock));
  if (isAtEndOfBook()) finalPagePresented = true;
}

bool EpubReaderActivity::makeRoomForChapterBuild() {
  const auto ready = [] {
    if (chapterBuildMayRun(bleturner::status())) return true;
    bleturner::beforeChapterBuild();
    return chapterBuildMayRun(bleturner::status());
  };
  const auto now = [] { return static_cast<uint32_t>(millis()); };
  const auto nap = [](uint32_t ms) { delay(ms); };
  if (waitForBuildRoom(ready, now, nap, 3000, 20)) return true;
  LOG_ERR("ERS", "Bluetooth still owns memory; chapter build deferred");
  return false;
}

bool EpubReaderActivity::remoteShortcut(const bool readerMenu) {
  if (!section) return false;
  automaticPageTurnActive = false;
  if (!readerMenu) {
    startClipSelection();
  } else {
    openTextSettings();
  }
  return true;
}

bool EpubReaderActivity::isAtEndOfBook() const { return epub && currentSpineIndex >= epub->getSpineItemsCount(); }

void EpubReaderActivity::onReturnFromEndOfBook() {
  if (epub && epub->getSpineItemsCount() > 0) {
    currentSpineIndex = epub->getSpineItemsCount() - 1;
    nextPageNumber = 0;
    pendingPageJump = std::numeric_limits<uint16_t>::max();
  }
}

bool EpubReaderActivity::backgroundBuildWanted() const {
  return section && section->isBuilding() && chapterBackgroundBuildMayRun() &&
         (section->isPartial() || static_cast<int>(section->pageCount) < section->currentPage + BUILD_WINDOW_AHEAD);
}

bool EpubReaderActivity::skipLoopDelay() {
  // The main loop holds the render lock while querying this hint.
  return formatReflowPending || (overlay == Overlay::None && !buildHeapPaused && backgroundBuildWanted());
}

std::optional<uint32_t> EpubReaderActivity::nextLoopWakeDelayMs(const uint32_t now) const {
  if (overlay != Overlay::None || !section || backgroundBuildWanted()) return loopdeadline::LEGACY_POLL_MS;
  loopdeadline::Deadline next(now);
  if (automaticPageTurnActive) next.after(lastPageTurnTime, pageTurnDuration);
  if (showBookmarkMessage) next.after(bookmarkMessageTime, ReaderUtils::BOOKMARK_MESSAGE_DURATION_MS);
  if (showDictionaryMessage) next.after(dictionaryMessageTime, ReaderUtils::BOOKMARK_MESSAGE_DURATION_MS);
  if (showClippingMessage) next.after(clippingMessageTime, ReaderUtils::BOOKMARK_MESSAGE_DURATION_MS);
  if (pendingManualTurn != 0) next.after(lastPageTurnTime, 200);
  if (canQueueNextPagePrefetch()) next.after(pageReadyAt, PREFETCH_IDLE_MS);
  return next.delayMs();
}

void EpubReaderActivity::clearPrefetchedPage() {
  prefetchedPage.reset();
  prefetchedPageNumber = -1;
  prefetchedSpineIndex = -1;
  prefetchedLayoutSignature = 0;
  prefetchedResource = prefetchedMedia = 0;
}

void EpubReaderActivity::releasePageReuse() {
  if (RenderLock::heldByCurrentTask()) {
    residentPage.clear();
    clearPrefetchedPage();
    return;
  }
  RenderLock lock;
  residentPage.clear();
  clearPrefetchedPage();
}

void EpubReaderActivity::resetSection() {
  formatReflowPending = false;
  formatIdleQueued = false;
  residentPage.clear();
  clearPrefetchedPage();
  prefetchAttemptedFromPage = -1;
  lastRenderedPage = -1;
  pageReadyAt = 0;
  section.reset();
}

void EpubReaderActivity::prepareForChild() {
  pageBufferStale = true;
  residentPage.clear();
  clearPrefetchedPage();
  ImageBlock::releaseRenderCache();
}

void EpubReaderActivity::onExit() {
  residentPage.clear();
  ImageBlock::releaseRenderCache();
  ChapterEventCache::retainReaderReplay(false);
  ParagraphInputCache::retainReader(false);
  WordWidthCacheLease::retainReader(false);
  if (textFormatPending) {
    textFormatPending = false;
    SETTINGS.saveToFile();
  }
  clearPrefetchedPage();
  flushFormatProgress();
  flushBookOrbitPending(true);
  ReaderActivity::onExit();
  if (epub)
    if (auto* css = epub->getCssParser()) css->retainForReader(false);
}

void EpubReaderActivity::rememberReflowTarget() {
  if (!section || formatReflowPending) return;
  bookOrbitLayoutReposition = true;
  rememberCurrentContentOffset();
  cachedSpineIndex = currentSpineIndex;
  cachedChapterTotalPageCount = section->pageCount;
  nextPageNumber = section->currentPage;
}

bool EpubReaderActivity::flushFormatProgress() {
  if (!formatProgress || !epub) return true;
  const auto& p = *formatProgress;
  if (!EpubReaderUtils::saveProgress(*epub, p.spine, p.page, p.count, p.offset)) {
    LOG_ERR("ERS", "Format progress remains unsaved");
    return false;
  }
  noteBookOrbitPositionSaved(p.localChange);
  lastSavedSpineIndex = p.spine;
  lastSavedPage = p.page;
  lastSavedPageCount = p.count;
  formatProgress.reset();
  return true;
}

void EpubReaderActivity::prepareForSleep() {
  if (textPreferencesDirty || textFormatPending) {
    if (booktext::save())
      textPreferencesDirty = false;
    else
      LOG_ERR("ERS", "Text preferences remain unsaved before sleep");
  }
  flushFormatProgress();
  flushBookOrbitPending(true);
  ReaderActivity::prepareForSleep();
}

bool EpubReaderActivity::runBookOrbitWakeSync(const bool readerInput) {
  using Button = MappedInputManager::Button;
  if (readerInput || millis() - bookOrbitWakeArmedAt >= BOOKORBIT_WAKE_READY_MS) {
    bookOrbitWakePending.store(false, std::memory_order_relaxed);
    return false;
  }
  if (mappedInput.isPressed(Button::Power)) return false;
  {
    RenderLock lock(RenderLock::Mode::Try);
    if (!lock.ownsLock()) return false;
    if (isAtEndOfBook()) {
      bookOrbitWakePending.store(false, std::memory_order_relaxed);
      return false;
    }
    if (!section || !presentationComplete()) return false;
    if (section->isBuilding() &&
        (bookOrbitWakeWaitForLayout || section->currentPage < 0 || section->currentPage >= section->pageCount ||
         !section->getVisibleTextOffsetForPage(static_cast<uint16_t>(section->currentPage)))) {
      bookOrbitWakeWaitForLayout = true;
      return false;
    }
  }

  if (bleturner::holdsHeap()) {
    bookOrbitWakePending.store(false, std::memory_order_relaxed);
    return false;
  }
  struct WakeProgress {
    EpubReaderActivity& reader;
    uint32_t startedAt;
    bool cancelled = false;
    bool shown = false;
  };
  WakeProgress progress{*this, static_cast<uint32_t>(millis())};
  const bookorbit::SyncCallbacks callbacks{
      [&progress] {
        auto& input = progress.reader.mappedInput;
        // The normal activity loop handles this action after the network unwinds.
        input.update(/*deferHomeButtonAction=*/true);
        const auto now = millis();
        const bool back = input.wasPressed(Button::Back);
        if (back) input.suppressNextRelease(Button::Back);
        progress.cancelled = progress.cancelled || back || input.wasPressed(Button::Power) ||
                             input.nextActionDelayMs(now, 1) == 0 || now - progress.startedAt >= BOOKORBIT_WAKE_SYNC_MS;
        return progress.cancelled;
      },
      [](void* context, const Epub&) {
        auto& progress = *static_cast<WakeProgress*>(context);
        RenderLock lock(progress.reader);
        progress.reader.pageBufferStale.store(true);
        GUI.drawPopup(progress.reader.renderer, tr(STR_BOOKORBIT_SYNCING_ON_SLEEP));
        progress.shown = true;
      },
      &progress};
  syncBookOrbit(callbacks, bookorbit::SyncTrigger::Wake);
  bookOrbitWakePending.store(false, std::memory_order_relaxed);
  if (progress.shown) requestUpdate();
  return true;
}

bool EpubReaderActivity::isNewBookOrbitPosition(int spine, int page) const {
  if (bookOrbitApplyingRemote || bookOrbitLayoutReposition || formatReflowPending) return false;
  const int previousSpine = formatProgress ? formatProgress->spine : lastSavedSpineIndex;
  const int previousPage = formatProgress ? formatProgress->page : lastSavedPage;
  return previousSpine >= 0 && (previousSpine != spine || previousPage != page);
}

void EpubReaderActivity::noteBookOrbitPositionSaved(bool localChange) {
  if (localChange) bookOrbitPositionRevision.fetch_add(1, std::memory_order_relaxed);
}

void EpubReaderActivity::flushBookOrbitPending(bool retry) {
  const auto revision = bookOrbitPositionRevision.load(std::memory_order_relaxed);
  if (bookOrbitRecordedRevision == revision && !(retry && bookOrbitPendingRetry)) return;
  bookOrbitRecordedRevision = revision;
  if (bookOrbitPendingStored || !bookorbit::Store::get().authenticated()) return;
  bookOrbitPendingStored = bookorbit::markPending(bookPath);
  bookOrbitPendingRetry = !bookOrbitPendingStored;
}

void EpubReaderActivity::acknowledgeBookOrbitPosition(uint32_t revision) {
  // Input is paused while syncing. Keep a newer save queued if rendering
  // still managed to commit a different position before confirmation.
  RenderLock lock;
  if (revision != bookOrbitPositionRevision.load(std::memory_order_relaxed)) return;
  bookOrbitRecordedRevision = revision;
  bookOrbitPendingRetry = false;
  bookOrbitPendingStored = !bookorbit::clearPending(bookPath);
}

bool EpubReaderActivity::syncBookOrbitBeforeSleep(const bookorbit::SyncCallbacks& callbacks) {
  syncBookOrbit(callbacks, bookorbit::SyncTrigger::Sleep);
  return true;
}

void EpubReaderActivity::syncBookOrbit(const bookorbit::SyncCallbacks& callbacks,
                                       const bookorbit::SyncTrigger trigger) {
  const bool waking = trigger == bookorbit::SyncTrigger::Wake;
  const auto& cancelled = callbacks.cancelled;
  auto& account = bookorbit::Store::get();
  if (!(waking ? account.syncOnWake : account.syncOnSleep) || !account.configured() || cancelled()) return;

  std::string document;
  bookorbit::Baseline baseline;
  bookorbit::Position local;
  CrossPointPosition readingPosition{};
  bool hasBaseline = false;
  bool atLandingPage = false;
  bool unfinishedLayout = false;
  uint32_t positionRevision;
  {
    RenderLock lock;
    if (!epub || !section || (!waking && section->isBuilding()) || currentSpineIndex < 0 ||
        currentSpineIndex >= epub->getSpineItemsCount() || !flushFormatProgress())
      return;
    unfinishedLayout = section->isBuilding();
    readingPosition = getCurrentPosition();
    positionRevision = bookOrbitPositionRevision.load(std::memory_order_relaxed);
    // A visible page can have an exact locator before the rest of its chapter has been laid out.
    if (unfinishedLayout && !readingPosition.hasVisibleTextOffset) return;
    document = KOReaderDocumentId::calculate(epub->getPath());
    if (document.empty()) return;
    hasBaseline = bookorbit::readBaseline(document, baseline);
    if (hasBaseline && readingPosition.spineIndex == baseline.landing.spine) {
      const auto page =
          baseline.landing.offset ? section->getPageForVisibleTextOffset(*baseline.landing.offset) : std::nullopt;
      atLandingPage = readingPosition.pageNumber == (page ? *page : baseline.landing.page);
    }
    ImageBlock::releaseRenderCache();
    clearPrefetchedPage();
    if (auto* cache = renderer.getFontCacheManager()) cache->releaseSdFontCaches();
    auto position = ProgressMapper::toSavedProgress(epub, readingPosition);
    local = {std::move(position.xpath), position.percentage};
  }

  if (!(local.fraction >= 0 && local.fraction <= 1) || cancelled()) return;
  bookorbit::SyncConnection wifi;
  if (!wifi.connect(callbacks, *epub)) return;
  WifiPowerSaveGuard powerSave;
  const auto identity = bookorbit::readServerIdentity(account, cancelled);
  if (cancelled()) return;
  if (identity.status == bookorbit::IdentityStatus::Unavailable) {
    LOG_INF("ORBIT", "Could not verify server identity");
    return;
  }
  const bool matches = hasBaseline && bookorbit::matchesBaseline(baseline, identity, account.url);
  if (hasBaseline && !baseline.instanceId.empty() && !matches) {
    LOG_INF("ORBIT", "Skipped sync: saved progress belongs to a different server");
    return;
  }

  const auto endpoint = account.endpoint(cancelled);
  KOReaderProgress remoteProgress{};
  const auto fetched = KOReaderSyncClient::getProgress(document, remoteProgress, &endpoint);
  if (cancelled()) return;
  const bool serverHasNoProgress = fetched == KOReaderSyncClient::NO_STORED_PROGRESS;
  const bool serverHasProgress =
      fetched == KOReaderSyncClient::OK && remoteProgress.percentage >= 0 && remoteProgress.percentage <= 1;
  if (!serverHasProgress && !serverHasNoProgress) {
    LOG_INF("ORBIT", "Could not read remote progress (result %d)", static_cast<int>(fetched));
    return;
  }
  if (identity.status == bookorbit::IdentityStatus::Verified && account.setAuthenticated(true))
    account.saveToFile(true);

  const std::string_view readerId(endpoint.deviceId);
  // The fallback hostname has no per-device suffix.
  const bool latestWriteFromReader = identity.status == bookorbit::IdentityStatus::Verified &&
                                     readerId.find('-') != std::string_view::npos &&
                                     remoteProgress.deviceId == readerId;
  const bookorbit::Position remote{std::move(remoteProgress.progress), remoteProgress.percentage};
  const auto remoteDevice = std::move(remoteProgress.deviceId);
  const auto remoteTimestamp = remoteProgress.timestamp;
  const auto upload = [&] {
    // The API has no conditional writes. Check for movement from another device before sending.
    const auto result = KOReaderSyncClient::getProgress(document, remoteProgress, &endpoint);
    if (cancelled()) return;
    if (serverHasNoProgress
            ? result != KOReaderSyncClient::NO_STORED_PROGRESS
            : result != KOReaderSyncClient::OK || remoteProgress.progress != remote.anchor ||
                  remoteProgress.percentage != remote.fraction || remoteProgress.deviceId != remoteDevice ||
                  remoteProgress.timestamp != remoteTimestamp) {
      LOG_INF("ORBIT", "Sleep sync skipped: remote progress could not be reconfirmed");
      return;
    }
    KOReaderProgress progress{};
    progress.document = document;
    progress.progress = local.anchor;
    progress.percentage = local.fraction;
    bookorbit::Position confirmed;
    const auto uploaded = bookorbit::uploadAndConfirm(progress, endpoint, confirmed);
    if (uploaded == KOReaderSyncClient::OK) {
      bookorbit::writeBaseline(document, identity, local, confirmed);
      acknowledgeBookOrbitPosition(positionRevision);
      LOG_INF("ORBIT", "Sleep sync uploaded and confirmed progress");
    } else if (!cancelled()) {
      LOG_INF("ORBIT", "Sleep sync could not confirm upload (result %d)", static_cast<int>(uploaded));
    }
  };
  if (serverHasNoProgress) {
    if (!waking && bookorbit::canUploadFirstPosition(hasBaseline, local)) upload();
    return;
  }

  const auto saveMatchingPositions = [&] {
    if (!matches || baseline.serverUrl != account.url || baseline.instanceId != identity.instanceId ||
        !bookorbit::samePosition(baseline.local, local) || !bookorbit::samePosition(baseline.remote, remote)) {
      bookorbit::writeBaseline(document, identity, local, remote,
                               matches && atLandingPage ? baseline.landing : bookorbit::LandingPage{});
    }
    acknowledgeBookOrbitPosition(positionRevision);
  };
  if (matches && atLandingPage) baseline.local = local;
  const auto* reference = matches ? &baseline : nullptr;
  const auto decision = waking ? bookorbit::decideOnWake(reference, local, remote)
                               : bookorbit::decideOnSleep(reference, local, remote, latestWriteFromReader);
  if (decision == bookorbit::Decision::Equal) {
    saveMatchingPositions();
    return;
  }
  if (decision == bookorbit::Decision::Skip) {
    LOG_INF("ORBIT", "Skipped sync: no eligible progress change");
    return;
  }

  CrossPointPosition mapped{};
  {
    RenderLock lock;
    mapped = ProgressMapper::toCrossPoint(epub, {remote.anchor, remote.fraction}, renderer, readingPosition.spineIndex,
                                          readingPosition.totalPages);
  }
  if (cancelled()) return;
  // Estimated page counts cannot order percentage-only progress in an unfinished chapter.
  if (unfinishedLayout && (!mapped.hasResolvedSpineIndex ||
                           (mapped.spineIndex == readingPosition.spineIndex && !mapped.hasVisibleTextOffset)))
    return;
  const auto direction = compareProgress(readingPosition, local.fraction, mapped, remote.fraction);
  if (direction == ProgressComparison::Synchronized) {
    saveMatchingPositions();
    return;
  }
  if (decision == bookorbit::Decision::Upload) {
    if (direction == ProgressComparison::LocalAhead) upload();
    return;
  }

  if (direction != ProgressComparison::RemoteAhead || mapped.spineIndex < 0 ||
      mapped.spineIndex >= epub->getSpineItemsCount())
    return;
  const auto offset = mapped.hasVisibleTextOffset ? std::optional<uint32_t>{mapped.visibleTextOffset} : std::nullopt;
  ProgressChangeResult change;
  change.spineIndex = mapped.spineIndex;
  change.page = mapped.pageNumber;
  change.totalPages = mapped.totalPages;
  change.hasVisibleTextOffset = mapped.hasVisibleTextOffset;
  change.visibleTextOffset = mapped.visibleTextOffset;
  {
    RenderLock lock;
    if (cancelled() ||
        !EpubReaderUtils::saveProgress(*epub, mapped.spineIndex, mapped.pageNumber, mapped.totalPages, offset))
      return;
    bookOrbitApplyingRemote = true;
    applyProgressChangeLocked(change);
  }
  bookorbit::writeBaseline(document, identity, remote, remote, {mapped.spineIndex, mapped.pageNumber, offset});
  acknowledgeBookOrbitPosition(positionRevision);
}

std::unique_ptr<Page> EpubReaderActivity::takePrefetchedPage(const int pageNumber) {
  const uint32_t signature =
      readerRenderSpecSignature(SETTINGS.readerRenderSpec(buildViewportWidth, buildViewportHeight));
  std::unique_ptr<Page> result;
  if (prefetchedPage && prefetchedPageNumber == pageNumber && prefetchedSpineIndex == currentSpineIndex &&
      prefetchedLayoutSignature == signature && prefetchedResource && prefetchedMedia &&
      prefetchedResource == Storage.resourceRevision() && prefetchedMedia == Storage.mediaRevision()) {
    result = std::move(prefetchedPage);
  }
  // A miss releases speculation before the foreground page allocates anything.
  clearPrefetchedPage();
  return result;
}

bool EpubReaderActivity::prefetchHeapAvailable(const bool includeBudget) const {
  const auto heap = HalMemory::getDefaultHeap();
  const auto internal = HalMemory::getInternalHeap();
  const size_t budget = includeBudget ? PREFETCH_MEMORY_BUDGET : 0;
  return heap.freeBytes >= PREFETCH_FREE_RESERVE + budget && heap.largestBlockBytes >= PREFETCH_MIN_BLOCK &&
         internal.freeBytes >= PREFETCH_INTERNAL_RESERVE + budget && internal.largestBlockBytes >= PREFETCH_MIN_BLOCK;
}

bool EpubReaderActivity::canQueueNextPagePrefetch() const {
  if (bookOrbitWakePending.load(std::memory_order_relaxed)) return false;
  int touchX = 0, touchY = 0;
  if (mappedInput.isScreenTouchHeld(touchX, touchY) || mappedInput.isPressed(MappedInputManager::Button::PageForward) ||
      mappedInput.isPressed(MappedInputManager::Button::PageBack))
    return false;
  return section && !section->isBuilding() && !section->isPartial() && overlay == Overlay::None &&
         !pageBufferStale.load() && !automaticPageTurnActive && pendingManualTurn == 0 && lastRenderedPage >= 0 &&
         lastRenderedPage == section->currentPage && prefetchAttemptedFromPage != section->currentPage &&
         section->currentPage + 1 < section->pageCount && prefetchHeapAvailable(true);
}

void EpubReaderActivity::idlePrefetchNextPage() {
  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return;
  if (!prefetchHeapAvailable(false)) {
    residentPage.clear();
    clearPrefetchedPage();
    return;
  }
  if (!canQueueNextPagePrefetch() || uint32_t(millis() - pageReadyAt) < PREFETCH_IDLE_MS) return;
  prefetchAttemptedFromPage = section->currentPage;
  prefetchInterrupted.store(false);
  activityManager.requestIdleWork(*this);
}

bool EpubReaderActivity::continuePrefetch(void* context) {
  auto& self = *static_cast<EpubReaderActivity*>(context);
  return !self.prefetchInterrupted.load() && activityManager.idleWorkIsCurrent(self) &&
         uint32_t(millis() - self.prefetchStartedAt) < PREFETCH_WORK_MS;
}

bool EpubReaderActivity::formatInputHeld() const {
  int x = 0, y = 0;
  using Button = MappedInputManager::Button;
  return mappedInput.isScreenTouchHeld(x, y) || mappedInput.isPressed(Button::Confirm) ||
         mappedInput.isPressed(Button::Left) || mappedInput.isPressed(Button::Right);
}

EpubReaderActivity::FormatPreparation EpubReaderActivity::prepareFormatPage(const ReaderRenderSpec& spec) {
  const auto offset = currentSpineIndex == cachedSpineIndex ? cachedVisibleTextOffset : std::nullopt;
  const int target = std::max(0, nextPageNumber);
  if (!section) {
    section = makeUniqueNoThrow<Section>(epub, currentSpineIndex, renderer);
    if (!section) {
      LOG_ERR("ERS", "OOM: format section");
      return FormatPreparation::Failed;
    }
    partialRebuildStartFailed = false;
    const bool loaded = section->loadSectionFile(spec);
    performance::count(loaded ? performance::Counter::LayoutHit : performance::Counter::LayoutMiss);
    const bool covered =
        loaded && (!section->isPartial() || (offset ? section->getPageForVisibleTextOffset(*offset).has_value()
                                                    : target < static_cast<int>(section->pageCount)));
    if (!covered && (!makeRoomForChapterBuild() || !section->startFormatBuild(spec))) return FormatPreparation::Failed;
  }
  if (section->isBuilding() && !section->layoutFinished() &&
      (offset ? !section->buildReachedVisibleTextOffset(*offset) : target >= static_cast<int>(section->pageCount))) {
    if (!section->buildSomeMore(1, nullptr, BACKGROUND_BUILD_SLICE_MS)) return FormatPreparation::Failed;
    if (!section->layoutFinished() &&
        (offset ? !section->buildReachedVisibleTextOffset(*offset) : target >= static_cast<int>(section->pageCount)))
      return FormatPreparation::Pending;
  }
  section->currentPage = offset ? section->getPageForVisibleTextOffset(*offset).value_or(target) : target;
  section->currentPage = std::clamp(section->currentPage, 0, std::max(0, int(section->pageCount) - 1));
  nextPageNumber = section->currentPage;
  clearDeferredReposition();
  formatReflowPending = false;
  return FormatPreparation::Ready;
}

void EpubReaderActivity::queueFormatIdle() {
  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock() || formatIdleQueued || textFormatPending || formatReflowPending || !presentationComplete() ||
      uint32_t(millis() - textFormatChangedAt) < 200 || formatInputHeld())
    return;
  if ((!textPreferencesDirty || textSaveSuppressed) && (!formatProgress || progressSaveSuppressed) &&
      !(section && section->isFormatBuild()))
    return;
  formatIdleQueued = true;
  prefetchInterrupted.store(false);
  activityManager.requestIdleWork(*this);
}

void EpubReaderActivity::runIdleWork() {
  // ActivityManager holds RenderLock; input continues polling on the main task.
  if (formatIdleQueued) {
    formatIdleQueued = false;
    if (!activityManager.idleWorkIsCurrent(*this) || prefetchInterrupted.load() || textFormatPending ||
        formatReflowPending || !presentationComplete())
      return;
    if (textPreferencesDirty && !textSaveSuppressed) {
      if (booktext::save())
        textPreferencesDirty = false;
      else {
        textSaveSuppressed = true;
        LOG_ERR("ERS", "Text preferences remain unsaved");
      }
      return;
    }
    if (formatProgress && !progressSaveSuppressed) {
      progressSaveSuppressed = !flushFormatProgress();
      return;
    }
    if (section && section->isFormatBuild()) {
      section->allowFormatPersistence();
      const uint32_t loans = renderer.frameBufferLoanCount();
      if (!section->buildSomeMore(1, nullptr, BACKGROUND_BUILD_SLICE_MS)) {
        resetSection();
        requestUpdate();
      }
      if (renderer.frameBufferLoanCount() != loans) {
        pageBufferStale = true;
        requestUpdate();
      }
    }
    return;
  }
  if (!activityManager.idleWorkIsCurrent(*this) || !section || section->isBuilding() || section->isPartial() ||
      overlay != Overlay::None || pageBufferStale.load() || lastRenderedPage != section->currentPage ||
      prefetchAttemptedFromPage != section->currentPage || section->currentPage + 1 >= section->pageCount ||
      !prefetchHeapAvailable(true))
    return;
  const unsigned long start = millis();
  prefetchStartedAt = start;
  // The ordinary Page graph is variable-sized and cannot live on the stack.
  // Its speculative decoder checks the 24 KiB budget before each allocation.
  const auto sourceResource = Storage.resourceRevision(), sourceMedia = Storage.mediaRevision();
  auto page = section->loadPageForPrefetch(section->currentPage + 1, PREFETCH_MEMORY_BUDGET);
  if (!page) {
    LOG_DBG("ERS", "Prefetch skipped: spine=%d page=%d", currentSpineIndex, section->currentPage + 1);
    return;
  }
  if (!activityManager.idleWorkIsCurrent(*this) || !prefetchHeapAvailable(false) || !sourceResource || !sourceMedia ||
      sourceResource != Storage.resourceRevision() || sourceMedia != Storage.mediaRevision() ||
      page->retainedBytes() > PREFETCH_MEMORY_BUDGET ||
      residentPage.retainedBytes() > readerpage::Reuse::Budget - PREFETCH_MEMORY_BUDGET)
    return;
  const auto retained = HalMemory::getInternalHeap();
  prefetchedPage = std::move(page);
  prefetchedResource = sourceResource;
  prefetchedMedia = sourceMedia;
  prefetchedPageNumber = section->currentPage + 1;
  prefetchedSpineIndex = currentSpineIndex;
  prefetchedLayoutSignature =
      readerRenderSpecSignature(SETTINGS.readerRenderSpec(buildViewportWidth, buildViewportHeight));
  if (continuePrefetch(this)) {
    auto scope = renderer.getFontCacheManager()->createPrewarmScope(FontCacheManager::ScopeCache::Reader);
    prefetchedPage->render(renderer, SETTINGS.getReaderFontId(), 0, 0);
    scope.endScanAndPrewarm(&EpubReaderActivity::continuePrefetch, this);
  }
  const auto extract = [](void* context, const char* source, const char* destination) {
    auto& self = *static_cast<EpubReaderActivity*>(context);
    return self.epub->extractItemToFile(source, destination, &EpubReaderActivity::continuePrefetch, context);
  };
  int top, right, bottom, left;
  renderer.getOrientedViewableTRBL(&top, &right, &bottom, &left);
  top += SETTINGS.screenMargin + UITheme::getReaderTopBarHeight();
  left += SETTINGS.screenMargin;
  for (const auto& element : prefetchedPage->elements) {
    if (!continuePrefetch(this)) break;
    if (element->getTag() != TAG_PageImage) continue;
    const auto& image = static_cast<const PageImage&>(*element).getImageBlock();
    if (image.prefetchPixels(renderer, &EpubReaderActivity::continuePrefetch, this)) continue;
    if (!continuePrefetch(this) || image.hasValidCache()) continue;
    const auto internal = HalMemory::getInternalHeap();
    if (internal.freeBytes < 128 * 1024 || internal.largestBlockBytes < 64 * 1024 ||
        HalMemory::getPsramHeap().freeBytes < 512 * 1024)
      break;
    if (image.prepareCache(renderer, this, extract, element->xPos + left, element->yPos + top,
                           &EpubReaderActivity::continuePrefetch, this) &&
        continuePrefetch(this))
      image.prefetchPixels(renderer, &EpubReaderActivity::continuePrefetch, this);
  }
  LOG_DBG("ERS", "Prefetch ready: spine=%d page=%d time=%lums internal=%u max=%u", currentSpineIndex,
          prefetchedPageNumber, millis() - start, unsigned(retained.freeBytes), unsigned(retained.largestBlockBytes));
}

readerpage::Key EpubReaderActivity::pageReuseKey() const {
  return {section.get(),
          currentSpineIndex,
          section ? section->currentPage : -1,
          SETTINGS.readerRenderSpec(buildViewportWidth, buildViewportHeight),
          Storage.resourceRevision(),
          Storage.mediaRevision()};
}

void EpubReaderActivity::renderBook() {
  if (!epub) {
    currentPageLinks.clear();
    return;
  }
  if (textFormatPending) {
    if (overlayPageStored) renderer.copyStoredBwBuffer();
    return;
  }
  if (!residentPage.matches(pageReuseKey())) currentPageLinks.clear();
  struct LinkGuard {
    std::vector<PageLink>& links;
    bool ready = false;
    ~LinkGuard() {
      if (!ready) links.clear();
    }
  } linkGuard{currentPageLinks};
  // Runs under the render task's RenderLock; catches every requestUpdate()
  // exit from the overlay while its deferred chrome refresh is still pending.
  settleOverlayRefresh();
  const uint32_t pageLoans = renderer.frameBufferLoanCount();

  const auto showPendingSyncSaveError = [this]() {
    if (!pendingSyncSaveError) return;
    pendingSyncSaveError = false;
    GUI.drawPopup(renderer, tr(STR_SAVE_PROGRESS_FAILED));
  };

  const auto showBuildError = [this]() {
    readingPace.pause();
    pageBufferStale = true;
    renderer.clearScreen();
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    GUI.drawPopup(renderer, tr(STR_INDEX_FAILED));
    automaticPageTurnActive = false;
  };

  if (currentSpineIndex < 0) currentSpineIndex = 0;
  if (currentSpineIndex > epub->getSpineItemsCount()) currentSpineIndex = epub->getSpineItemsCount();

  if (currentSpineIndex == epub->getSpineItemsCount()) {
    return;
  }

  int orientedMarginTop, orientedMarginRight, orientedMarginBottom, orientedMarginLeft;
  renderer.getOrientedViewableTRBL(&orientedMarginTop, &orientedMarginRight, &orientedMarginBottom,
                                   &orientedMarginLeft);
  orientedMarginTop += SETTINGS.screenMargin + UITheme::getReaderTopBarHeight();
  orientedMarginLeft += SETTINGS.screenMargin;
  orientedMarginRight += SETTINGS.screenMargin;

  const uint8_t statusBarHeight = UITheme::getInstance().getStatusBarHeight();

  if (automaticPageTurnActive && SETTINGS.readerBars.customized && !SETTINGS.readerBars.bottom.hasText()) {
    auto config = SETTINGS.readerBars.bottom;
    config.visible = true;
    config.slots[3] = readerbars::Item::Chapter;
    orientedMarginBottom += std::max<int>(SETTINGS.screenMargin, readerbars::barHeight(config) + 4);
  } else if (automaticPageTurnActive &&
             (statusBarHeight == 0 || statusBarHeight == UITheme::getInstance().getProgressBarHeight())) {
    orientedMarginBottom +=
        std::max(SETTINGS.screenMargin,
                 static_cast<uint8_t>(statusBarHeight + UITheme::getInstance().getMetrics().statusBarVerticalMargin));
  } else {
    orientedMarginBottom += std::max(SETTINGS.screenMargin, statusBarHeight);
  }

  const uint16_t viewportWidth = renderer.getScreenWidth() - orientedMarginLeft - orientedMarginRight;
  const uint16_t viewportHeight = renderer.getScreenHeight() - orientedMarginTop - orientedMarginBottom;
  buildViewportWidth = viewportWidth;
  buildViewportHeight = viewportHeight;

  const ReaderRenderSpec renderSpec = SETTINGS.readerRenderSpec(viewportWidth, viewportHeight);
  if (formatReflowPending) {
    switch (prepareFormatPage(renderSpec)) {
      case FormatPreparation::Pending:
        if (overlayPageStored) renderer.copyStoredBwBuffer();
        requestUpdate();
        return;
      case FormatPreparation::Failed:
        resetSection();
        showBuildError();
        return;
      case FormatPreparation::Ready:
        discardOverlayPage();
        break;
    }
  }

  if (!section) {
    const auto filepath = epub->getSpineItem(currentSpineIndex).href;
    LOG_DBG("ERS", "Loading file: %s, index: %d", filepath.c_str(), currentSpineIndex);
    section = std::unique_ptr<Section>(new Section(epub, currentSpineIndex, renderer));
    partialRebuildStartFailed = false;

    const bool cacheLoaded = section->loadSectionFile(renderSpec);
    performance::count(cacheLoaded ? performance::Counter::LayoutHit : performance::Counter::LayoutMiss);
    if (cacheLoaded) {
      cachedChapterTotalPageCount = 0;
    }
    const bool cacheComplete = cacheLoaded && !section->isPartial();
    const bool explicitOffsetJump = pendingOffsetJump.has_value();
    const std::optional<uint32_t> offsetJump =
        explicitOffsetJump ? pendingOffsetJump
        : (pendingPageJump.has_value() || !pendingAnchor.empty() || currentSpineIndex != cachedSpineIndex)
            ? std::nullopt
            : cachedVisibleTextOffset;
    if (!cacheComplete) {
      if (section->isPartial()) {
        LOG_DBG("ERS", "Partial cache found (%d pages), resuming build...", section->pageCount);
      } else {
        LOG_DBG("ERS", "Cache not found, building...");
      }

      const bool needsFullBuild = pendingPercentJump;
      if (needsFullBuild) {
        if (!makeRoomForChapterBuild()) {
          showBuildError();
          return;
        }
        showIndexingPopup(pageLoans);
        GfxRenderer::FrameBufferLoan loan(renderer);
        if (!section->createSectionFile(renderSpec)) {
          LOG_ERR("ERS", "Failed to persist page data to SD");
          resetSection();
          loan.end();
          showBuildError();
          return;
        }
        loan.end();
      } else {
        const int target = pendingPageJump.has_value() ? *pendingPageJump : (nextPageNumber < 0 ? 0 : nextPageNumber);
        const bool anchorJump = !pendingAnchor.empty();

        const bool cachedTargetAvailable =
            anchorJump               ? section->getPageForAnchor(pendingAnchor).has_value()
            : offsetJump.has_value() ? section->getPageForVisibleTextOffset(*offsetJump).has_value()
                                     : target + PARTIAL_REBUILD_START_MARGIN < static_cast<int>(section->pageCount);
        if (section->isPartial() && cachedTargetAvailable) {
          LOG_DBG("ERS", "Partial covers target %d of %d; deferring extension build", target, section->pageCount);
        } else {
          LOG_DBG("ERS", "Heap before section build: %u (max block %u)", (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getMaxAllocHeap());
          if (!makeRoomForChapterBuild()) {
            showBuildError();
            return;
          }
          showIndexingPopup(pageLoans);
          bool started;
          {
            GfxRenderer::FrameBufferLoan loan(renderer);
            started = section->startBuild(renderSpec);
          }
          if (!started) {
            LOG_ERR("ERS", "Failed to start section build");
            resetSection();
            showBuildError();
            return;
          }
          while (!section->isBuildComplete() &&
                 (anchorJump               ? !section->findAnchor(pendingAnchor)
                  : offsetJump.has_value() ? !section->buildReachedVisibleTextOffset(*offsetJump)
                                           : static_cast<int>(section->pageCount) <= target)) {
            if (!section->buildSomeMore(offsetJump.has_value() ? 1 : BUILD_PAGES_PER_CHUNK)) {
              LOG_ERR("ERS", "Failed during incremental section build");
              resetSection();
              showBuildError();
              return;
            }
          }
        }
      }
    } else {
      LOG_DBG("ERS", "Cache found, skipping build...");
    }

    if (pendingPageJump.has_value()) {
      section->currentPage = *pendingPageJump;
      pendingPageJump.reset();
    } else {
      section->currentPage = nextPageNumber;
      if (section->currentPage < 0) section->currentPage = 0;
    }

    if (offsetJump.has_value()) {
      if (const auto offsetPage = section->getPageForVisibleTextOffset(*offsetJump)) {
        section->currentPage = *offsetPage;
        clearDeferredReposition();
      }
    }
    if (explicitOffsetJump) {
      clearDeferredReposition();
    }
    pendingOffsetJump.reset();

    if (!pendingAnchor.empty()) {
      const auto page = section->findAnchor(pendingAnchor);
      if (page) {
        section->currentPage = *page;
        LOG_DBG("ERS", "Resolved anchor '%s' to page %d", pendingAnchor.c_str(), *page);
      }
      pendingAnchor.clear();
    }

    if (pendingPercentJump && section->pageCount > 0) {
      int newPage = static_cast<int>(pendingSpineProgress * static_cast<float>(section->pageCount));
      if (newPage >= section->pageCount) newPage = section->pageCount - 1;
      section->currentPage = newPage;
      pendingPercentJump = false;
    }
  }

  if (section->isPartial() && section->currentPage >= static_cast<int>(section->pageCount)) {
    showIndexingPopup(pageLoans);
  }
  while (section->isPartial() && !section->layoutFinished() &&
         section->currentPage >= static_cast<int>(section->pageCount)) {
    if (!section->isBuilding() && (!makeRoomForChapterBuild() || !section->startBuild(renderSpec))) {
      LOG_ERR("ERS", "Failed to start partial extension build");
      resetSection();
      showBuildError();
      return;
    }
    while (!section->layoutFinished() && section->currentPage >= static_cast<int>(section->pageCount)) {
      if (!section->buildSomeMore(BUILD_PAGES_PER_CHUNK)) {
        LOG_ERR("ERS", "Failed during incremental section build");
        resetSection();
        showBuildError();
        return;
      }
    }
  }
  if (section->isBuilding()) {
    while (!section->layoutFinished() && section->currentPage >= static_cast<int>(section->pageCount)) {
      if (!section->buildSomeMore(BUILD_PAGES_PER_CHUNK)) {
        LOG_ERR("ERS", "Failed during incremental section build");
        resetSection();
        showBuildError();
        return;
      }
    }
  }

  if ((!section->isBuilding() || section->layoutFinished()) && section->pageCount > 0 &&
      section->currentPage >= static_cast<int>(section->pageCount)) {
    section->currentPage = section->pageCount - 1;
  }

  applyDeferredReposition();

  pageBufferStale = true;
  renderer.clearScreen();

  if (section->pageCount == 0) {
    LOG_DBG("ERS", "No pages to render");
    renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_EMPTY_CHAPTER), true, EpdFontFamily::BOLD);
    renderStatusBar();
    renderer.displayBuffer();
    pageBufferStale = false;
    automaticPageTurnActive = false;
    showPendingSyncSaveError();
    return;
  }

  if (section->currentPage < 0 || section->currentPage >= section->pageCount) {
    LOG_DBG("ERS", "Page out of bounds: %d (max %d)", section->currentPage, section->pageCount);
    renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_OUT_OF_BOUNDS), true, EpdFontFamily::BOLD);
    renderStatusBar();
    renderer.displayBuffer();
    automaticPageTurnActive = false;
    showPendingSyncSaveError();
    return;
  }

  updateBookmarkFlag();

  {
    const unsigned long loadStart = millis();
    const auto pageKey = pageReuseKey();
    std::unique_ptr<Page> p;
    bool residentHit = false;
    bool prefetchHit = false;
    {
      performance::Scope loading(performance::Stage::PageLoad);
      p = residentPage.take(pageKey);
      residentHit = p != nullptr;
      performance::count(residentHit ? performance::Counter::PageReuseHit : performance::Counter::PageReuseMiss);
      if (!residentHit) currentPageLinks.clear();
      if (!p) {
        p = takePrefetchedPage(section->currentPage);
        prefetchHit = p != nullptr;
      }
      if (!residentHit)
        performance::count(prefetchHit ? performance::Counter::PagePrefetchHit
                                       : performance::Counter::PagePrefetchMiss);
      if (!p) p = section->loadPage(section->currentPage);
    }
    LOG_DBG("ERS", "Page load: spine=%d page=%d prefetch=%d time=%lums", currentSpineIndex, section->currentPage,
            prefetchHit, millis() - loadStart);
    if (!p) {
      LOG_ERR("ERS", "Failed to load page from SD - clearing section cache");
      automaticPageTurnActive = false;
      const bool giveUp = ++pageLoadRetryCount > MAX_PAGE_LOAD_RETRIES;
      section->abandonBuild();
      section->clearCache();
      resetSection();
      if (giveUp) {
        LOG_ERR("ERS", "Page load retry limit reached, aborting");
        pageLoadRetryCount = 0;
        renderer.clearScreen();
        renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_PAGE_LOAD_ERROR), true, EpdFontFamily::BOLD);
        renderer.displayBuffer();
        showPendingSyncSaveError();
        return;
      }
      requestUpdate();
      showPendingSyncSaveError();
      return;
    }
    pageLoadRetryCount = 0;

    currentPageVisibleOffset = p->visibleTextOffset;
    if (!residentHit) {
      currentPageFootnotes = std::move(p->footnotes);
      currentPageLinks = std::move(p->links);
    }
    currentPageLinkMarginLeft = orientedMarginLeft;
    currentPageLinkMarginTop = orientedMarginTop;
    linkGuard.ready = true;

    // The overlay and non-tiled grayscale renderer share the renderer's single
    // stored-BW slot. Release the old page snapshot before renderContents()
    // needs that slot, then snapshot the newly rendered page below.
    discardOverlayPage();

    const auto start = millis();
    renderContents(*p, orientedMarginTop, orientedMarginRight, orientedMarginBottom, orientedMarginLeft);
    presentedTextSpec = renderSpec;
    presentedScreenMargin = SETTINGS.screenMargin;
    if (prefetchHeapAvailable(prefetchedPage == nullptr) && pageKey.resource == Storage.resourceRevision() &&
        pageKey.media == Storage.mediaRevision())
      residentPage.keep(p, pageKey, prefetchedPage ? prefetchedPage->retainedBytes() : 0);
    LOG_DBG("ERS", "Rendered page in %dms", millis() - start);
    markPageRendered();
  }

  if (currentSpineIndex != lastSavedSpineIndex || section->currentPage != lastSavedPage ||
      section->pageCount != lastSavedPageCount) {
    if (textPreferencesDirty || (section && section->isFormatBuild())) {
      formatProgress = FormatProgress{currentSpineIndex, section->currentPage, section->estimatedTotalPages(),
                                      currentPageVisibleOffset,
                                      isNewBookOrbitPosition(currentSpineIndex, section->currentPage) ||
                                          (formatProgress && formatProgress->localChange)};
      progressSaveSuppressed = false;
    } else if (saveProgress(currentSpineIndex, section->currentPage, section->estimatedTotalPages())) {
      lastSavedSpineIndex = currentSpineIndex;
      lastSavedPage = section->currentPage;
      lastSavedPageCount = section->estimatedTotalPages();
    }
  }
  bookOrbitApplyingRemote = false;
  bookOrbitLayoutReposition = false;

  showPendingSyncSaveError();

  if (pendingScreenshot) {
    pendingScreenshot = false;
    ScreenshotUtil::takeScreenshot(renderer);
  }

  if (showBookmarkMessage) {
    GUI.drawPopup(renderer, bookmarkRemoved ? tr(STR_BOOKMARK_REMOVED) : tr(STR_BOOKMARK_ADDED));
  }

  if (showDictionaryMessage) {
    GUI.drawPopup(renderer, tr(STR_DICT_NO_DICT_SET));
  }

  if (showClippingMessage) {
    GUI.drawPopup(renderer, I18N.get(clippingMessage));
  }

  pageBufferStale = false;  // the page is back in the framebuffer
  lastRenderedPage = section->currentPage;
  pageReadyAt = millis();

  // Toolbar menu: overlay the toolbar / panel on top of the freshly rendered page.
  if (overlay != Overlay::None && (usesToolbarMenu() || overlay == Overlay::Text)) {
    // The page just re-rendered under the overlay: refresh the snapshot that
    // backs panel->toolbar restores (any previous copy is stale).
    overlayPageStored = renderer.storeBwBuffer();
    renderOverlay();
    // An open option picker rides on top of the freshly drawn panel.
    if (overlayPopup.isActive()) overlayPopup.render(renderer);
    // FAST, same as openOverlay: HALF's inverting pass flashes the sheet
    // (white, in night mode) on every repaint under an open panel. Any AA
    // residue a FAST differential leaves under the chrome has not shown in
    // practice; restore a HALF cleanup here if text ever visibly ghosts
    // through the sheet (see #2190 for the mechanism).
    pushOverlayRefresh();
  }
  finalPagePresented = true;
}

void EpubReaderActivity::onEndOfBookRendered() {
  automaticPageTurnActive = false;
  if (pendingSyncSaveError) {
    pendingSyncSaveError = false;
    GUI.drawPopup(renderer, tr(STR_SAVE_PROGRESS_FAILED));
  }
}

bool EpubReaderActivity::saveFinishedPosition() {
  if (!epub || epub->getSpineItemsCount() <= 0 || !saveProgress(epub->getSpineItemsCount(), 0, 0)) return false;
  clearDeferredReposition();
  currentSpineIndex = epub->getSpineItemsCount();
  nextPageNumber = 0;
  cachedSpineIndex = currentSpineIndex;
  cachedChapterTotalPageCount = 0;
  cachedVisibleTextOffset.reset();
  footnoteDepth = 0;
  pendingPageJump.reset();
  pendingOffsetJump.reset();
  pendingAnchor.clear();
  pendingPercentJump = false;
  pendingManualTurn = 0;
  automaticPageTurnActive = false;
  overlay = Overlay::None;
  discardOverlayPage();
  resetSection();
  return true;
}

bool EpubReaderActivity::applyDeferredReposition() {
  if ((!cachedVisibleTextOffset.has_value() && cachedChapterTotalPageCount == 0) || !section || section->isBuilding()) {
    return false;
  }
  bool changed = false;
  if (currentSpineIndex == cachedSpineIndex) {
    int newPage = section->currentPage;
    bool mappedOffset = false;
    if (cachedVisibleTextOffset.has_value()) {
      if (const auto offsetPage = section->getPageForVisibleTextOffset(*cachedVisibleTextOffset)) {
        newPage = *offsetPage;
        mappedOffset = true;
      }
    }
    if (!mappedOffset && cachedChapterTotalPageCount > 0 && section->pageCount != cachedChapterTotalPageCount) {
      const float progress = static_cast<float>(section->currentPage) / static_cast<float>(cachedChapterTotalPageCount);
      newPage = static_cast<int>(progress * static_cast<float>(section->pageCount));
    }
    if (newPage < 0) newPage = 0;
    if (section->pageCount > 0 && newPage >= static_cast<int>(section->pageCount)) {
      newPage = section->pageCount - 1;
    }
    if (newPage != section->currentPage) {
      section->currentPage = newPage;
      changed = true;
    }
  }
  clearDeferredReposition();
  return changed;
}

void EpubReaderActivity::clearDeferredReposition() {
  cachedChapterTotalPageCount = 0;
  cachedVisibleTextOffset.reset();
}

bool EpubReaderActivity::saveProgress(int spineIndex, int currentPage, int pageCount) {
  std::optional<uint32_t> offset;
  if (formatReflowPending && spineIndex == cachedSpineIndex) {
    currentPage = nextPageNumber;
    pageCount = cachedChapterTotalPageCount;
    offset = cachedVisibleTextOffset;
  } else if (section && spineIndex == currentSpineIndex && currentPage >= 0 && currentPage < section->pageCount) {
    offset = (currentPage == section->currentPage && currentPageVisibleOffset.has_value())
                 ? currentPageVisibleOffset
                 : section->getVisibleTextOffsetForPage(static_cast<uint16_t>(currentPage));
  }
  if (!EpubReaderUtils::saveProgress(*epub, spineIndex, currentPage, pageCount, offset)) return false;
  noteBookOrbitPositionSaved(isNewBookOrbitPosition(spineIndex, currentPage) ||
                             (formatProgress && formatProgress->localChange));
  formatProgress.reset();
  progressSaveSuppressed = false;
  return true;
}

void EpubReaderActivity::rememberCurrentContentOffset() {
  cachedVisibleTextOffset.reset();
  if (section && section->currentPage >= 0 && section->currentPage < section->pageCount) {
    cachedVisibleTextOffset = lastRenderedPage == section->currentPage && currentPageVisibleOffset.has_value()
                                  ? currentPageVisibleOffset
                                  : section->getVisibleTextOffsetForPage(static_cast<uint16_t>(section->currentPage));
  }
}

void EpubReaderActivity::renderContents(Page& content, const int orientedMarginTop, const int orientedMarginRight,
                                        const int orientedMarginBottom, const int orientedMarginLeft) {
  auto* page = &content;
  performance::Scope drawing(performance::Stage::Draw);
  const auto t0 = millis();
  const int fontId = SETTINGS.getReaderFontId();
  ImageBlock::clearRenderFailures();
  const bool ditherImages = SETTINGS.imageRendering == CrossPointSettings::IMAGES_DISPLAY_BW || BoardConfig::isEegoA4();
  struct ImageModeGuard {
    GfxRenderer& renderer;
    bool previous;
    ~ImageModeGuard() { renderer.setDitherImagesInBw(previous); }
  } imageModeGuard{renderer, renderer.imagesDitheredInBw()};
  renderer.setDitherImagesInBw(ditherImages);

  struct PxcSlotGuard {
    PxcSlotGuard() { ImageBlock::beginRenderCache(); }
    ~PxcSlotGuard() { ImageBlock::endRenderCache(); }
  } pxcSlotGuard;

  auto* fcm = renderer.getFontCacheManager();
  auto scope = fcm->createPrewarmScope(FontCacheManager::ScopeCache::Reader);
  page->render(renderer, fontId, orientedMarginLeft, orientedMarginTop);
  // Scan the status bar too: a CJK book/chapter title redirected to the SD
  // fallback font joins the page's single batch prewarm instead of triggering
  // its own SD pass after the scope ends.
  renderStatusBar();
  scope.endScanAndPrewarm();
  const auto tPrewarm = millis();

  const bool pageHasImages = page->hasImages();
  const bool pageHasImagesNeedingDecode = pageHasImages && page->hasImagesNeedingDecode();
  const bool manualRefreshPending = forcedRefreshPending;
  forcedRefreshPending = false;
  const bool cleanImageBasePending = manualRefreshPending || pagesUntilFullRefresh <= 1;
  const bool needsTextGrayscale = SETTINGS.textAntiAliasing;
  const bool needsAnyGrayscale = needsTextGrayscale || (pageHasImages && !ditherImages);
  const bool absoluteImageGrayscale = pageHasImages && !ditherImages && !gpio.deviceIsX3() &&
                                      display.getController() == HalDisplay::Controller::UC8279 &&
                                      renderer.grayscaleCapabilities(HalDisplay::GrayscaleMode::Absolute).supported();
  const auto grayscale = renderer.grayscaleCapabilities(absoluteImageGrayscale ? HalDisplay::GrayscaleMode::Absolute
                                                                               : HalDisplay::GrayscaleMode::Overlay);
  const bool tiledGrayscale = needsAnyGrayscale && grayscale.stripUploads;
  // Paper Mono only (no other panel combines): defer the B/W base activation so
  // the gray planes join it in a single waveform. Displaying the base
  // separately makes the gray pass re-drive the whole text body â€” a visible
  // flash on every AA page.
  const bool combinedGrayscaleBase =
      tiledGrayscale && !pageHasImages && grayscale.base == HalDisplay::GrayscaleBase::Combined;
  const bool overlapRefresh = tiledGrayscale && grayscale.asyncBase && !pageHasImages;
  auto renderGrayscalePass = [&]() {
    // A4 gray planes replace the full page, including its clipping highlights.
    if (BoardConfig::isEegoA4()) drawClippingHighlights(*page, fontId, orientedMarginTop, orientedMarginLeft);
    if (absoluteImageGrayscale || needsTextGrayscale) {
      page->render(renderer, fontId, orientedMarginLeft, orientedMarginTop);
    } else {
      page->renderImages(renderer, fontId, orientedMarginLeft, orientedMarginTop);
    }
    // A4 replaces the whole frame with its gray planes, including the status bar.
    if (absoluteImageGrayscale || BoardConfig::isEegoA4())
      renderStatusBar();
    else if (overlay == Overlay::None)
      renderContentsReturn(true);
  };

  if (pageHasImagesNeedingDecode) {
    page->renderWithImagePlaceholders(renderer, fontId, orientedMarginLeft, orientedMarginTop);
    renderStatusBar();
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    renderer.clearScreen();
  }

  drawClippingHighlights(*page, fontId, orientedMarginTop, orientedMarginLeft);
  page->render(renderer, fontId, orientedMarginLeft, orientedMarginTop);
  renderStatusBar();
  const auto tBwRender = millis();

  if (absoluteImageGrayscale) {
    const auto baseMode = cleanImageBasePending ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH;
    if (!renderer.displayGrayscaleBase(HalDisplay::GrayscaleMode::Absolute, baseMode)) {
      LOG_ERR("ERS", "Could not start absolute image page; displaying B/W");
      ReaderUtils::displayWithRefreshCycle(renderer, pagesUntilFullRefresh);
      return;
    }
    LOG_DBG("ERS", "UC8279 image page: absolute quality waveform");
    pagesUntilFullRefresh = 1;
  } else if (pageHasImages && !ditherImages) {
    // Image pages use one base refresh before the grayscale pass. FAST leaves
    // the panel receptive to the gray waveform; pending cleanup still honors
    // the scheduled/manual HALF refresh.
    renderer.displayBuffer(cleanImageBasePending ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
    pagesUntilFullRefresh = 1;
  } else if (combinedGrayscaleBase) {
    // Stash the base without activating; displayGrayBuffer() below commits
    // base + grays as one waveform.
    ReaderUtils::displayBaseWithRefreshCycle(renderer, pagesUntilFullRefresh);
  } else if (needsAnyGrayscale) {
    if (pagesUntilFullRefresh <= 1) {
      // A cleanup refresh settles X3 correctly only when its grayscale
      // preconditioning waveform runs before the gray planes are written.
      renderer.displayBuffer(HalDisplay::HALF_REFRESH);
      renderer.preconditionGrayscale();
      pagesUntilFullRefresh = SETTINGS.getRefreshFrequency();
    } else if (overlapRefresh) {
      ReaderUtils::displayWithRefreshCycle(renderer, pagesUntilFullRefresh, /*async=*/true);
    } else {
      renderer.displayGrayscaleBase(HalDisplay::FAST_REFRESH);
      pagesUntilFullRefresh--;
    }
  } else {
    ReaderUtils::displayWithRefreshCycle(renderer, pagesUntilFullRefresh);
  }
  const auto tDisplay = millis();

  if (tiledGrayscale) {
    constexpr int STRIP_ROWS = 80;
    const int gh = renderer.getDisplayHeight();
    const int gwBytes = renderer.getDisplayWidthBytes();
    const size_t planeBytes = static_cast<size_t>(gwBytes) * gh;

    auto renderPlaneToBuffer = [&](const bool lsbPlane, uint8_t* buf) {
      renderer.setRenderMode(lsbPlane ? GfxRenderer::GRAYSCALE_LSB : GfxRenderer::GRAYSCALE_MSB);
      // A full-plane buffer needs one traversal; only the scratch fallback is tiled.
      renderer.beginStripTarget(buf, 0, gh);
      renderer.clearScreen(0x00);
      renderGrayscalePass();
      renderer.endStripTarget();
    };

    constexpr size_t PLANE_BUF_HEADROOM = 60000;
    constexpr size_t PLANE_BUF_MAX_ALLOC_RESERVE = 16 * 1024;
    const auto planeBufFits = [planeBytes] {
      return ESP.getFreeHeap() >= planeBytes + PLANE_BUF_HEADROOM &&
             ESP.getMaxAllocHeap() >= planeBytes + PLANE_BUF_MAX_ALLOC_RESERVE;
    };
    auto lsbPlaneBuf = (overlapRefresh && planeBufFits()) ? makeUniqueNoThrow<uint8_t[]>(planeBytes) : nullptr;
    auto msbPlaneBuf = (lsbPlaneBuf && planeBufFits()) ? makeUniqueNoThrow<uint8_t[]>(planeBytes) : nullptr;
    // Non-overlapping panels can reuse one gray plane in PSRAM (48 KB on X4 Pro).
    // Keep their internal-RAM scratch fallback when PSRAM is absent or exhausted.
    const bool usePsramPlane = !overlapRefresh && HalMemory::getPsramHeap().totalBytes > 0;
    auto psramPlaneBuf = usePsramPlane ? HalMemory::allocatePsram(planeBytes) : HalMemory::PsramBuffer{};
    if (usePsramPlane && !psramPlaneBuf) {
      LOG_ERR("ERS", "OOM: grayscale PSRAM plane (%u bytes); using strip scratch", static_cast<unsigned>(planeBytes));
    }
    uint8_t* const planeBuf = lsbPlaneBuf ? lsbPlaneBuf.get() : psramPlaneBuf.get();

    if (planeBuf) {
      renderPlaneToBuffer(true, planeBuf);
      if (msbPlaneBuf) renderPlaneToBuffer(false, msbPlaneBuf.get());
      const auto tGrayRender = millis();

      renderer.waitRefreshComplete();
      const auto tWait = millis();

      renderer.writeGrayscalePlaneStrip(true, planeBuf, 0, gh);
      if (msbPlaneBuf) {
        renderer.writeGrayscalePlaneStrip(false, msbPlaneBuf.get(), 0, gh);
      } else {
        renderPlaneToBuffer(false, planeBuf);
        renderer.writeGrayscalePlaneStrip(false, planeBuf, 0, gh);
      }
      const auto tGrayWrite = millis();

      renderer.setRenderMode(GfxRenderer::BW);
      renderer.displayGrayBuffer();
      const auto tGrayDisplay = millis();

      renderer.cleanupGrayscaleWithFrameBuffer();
      const auto tEnd = millis();

      LOG_DBG("ERS",
              "Page render (buffered): prewarm=%lums bw_render=%lums display=%lums gray_render=%lums "
              "wait=%lums gray_write=%lums gray_display=%lums cleanup=%lums total=%lums (planes buffered: %d)",
              tPrewarm - t0, tBwRender - tPrewarm, tDisplay - tBwRender, tGrayRender - tDisplay, tWait - tGrayRender,
              tGrayWrite - tWait, tGrayDisplay - tGrayWrite, tEnd - tGrayDisplay, tEnd - t0, msbPlaneBuf ? 2 : 1);
    } else {
      auto scratch = makeUniqueNoThrow<uint8_t[]>(static_cast<size_t>(gwBytes) * STRIP_ROWS);
      renderer.waitRefreshComplete();
      if (!scratch) {
        LOG_ERR("ERS", "OOM: grayscale strip scratch (%d bytes); skipping AA this page", gwBytes * STRIP_ROWS);
        if (overlapRefresh || combinedGrayscaleBase) {
          // The BW refresh ran the shadow-free async path, so controller RAM's
          // differential baseline was never rebuilt. Even with AA skipped it must
          // be re-synced from the intact BW framebuffer, or the next differential
          // update diffs against stale contents. On the combined-base path the
          // base activation is still deferred; this cleanup commits it so the
          // page reaches the panel even without its grays.
          renderer.cleanupGrayscaleWithFrameBuffer();
        }
      } else {
        renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
        for (int y = 0; y < gh; y += STRIP_ROWS) {
          const int rows = (gh - y < STRIP_ROWS) ? (gh - y) : STRIP_ROWS;
          renderer.beginStripTarget(scratch.get(), y, rows);
          renderer.clearScreen(0x00);
          renderGrayscalePass();
          renderer.endStripTarget();
          renderer.writeGrayscalePlaneStrip(true, scratch.get(), y, rows);
        }
        const auto tGrayLsb = millis();

        renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
        for (int y = 0; y < gh; y += STRIP_ROWS) {
          const int rows = (gh - y < STRIP_ROWS) ? (gh - y) : STRIP_ROWS;
          renderer.beginStripTarget(scratch.get(), y, rows);
          renderer.clearScreen(0x00);
          renderGrayscalePass();
          renderer.endStripTarget();
          renderer.writeGrayscalePlaneStrip(false, scratch.get(), y, rows);
        }
        const auto tGrayMsb = millis();

        renderer.setRenderMode(GfxRenderer::BW);
        renderer.displayGrayBuffer();
        const auto tGrayDisplay = millis();

        renderer.cleanupGrayscaleWithFrameBuffer();
        const auto tCleanup = millis();

        const auto tEnd = millis();
        LOG_DBG("ERS",
                "Page render (tiled): prewarm=%lums bw_render=%lums display=%lums gray_lsb=%lums "
                "gray_msb=%lums gray_display=%lums cleanup=%lums total=%lums",
                tPrewarm - t0, tBwRender - tPrewarm, tDisplay - tBwRender, tGrayLsb - tDisplay, tGrayMsb - tGrayLsb,
                tGrayDisplay - tGrayMsb, tCleanup - tGrayDisplay, tEnd - t0);
      }
    }
  } else {
    if (needsAnyGrayscale) {
      if (!renderer.storeBwBuffer()) {
        LOG_ERR("ERS", "Failed to store BW buffer for grayscale render; skipping grayscale this page");
        if (absoluteImageGrayscale) renderer.setRenderMode(GfxRenderer::BW);
        return;
      }
      const auto tBwStore = millis();

      renderer.clearScreen(absoluteImageGrayscale ? 0xFF : 0x00);
      renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
      renderGrayscalePass();
      renderer.copyGrayscaleLsbBuffers();
      const auto tGrayLsb = millis();

      renderer.clearScreen(absoluteImageGrayscale ? 0xFF : 0x00);
      renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
      renderGrayscalePass();
      renderer.copyGrayscaleMsbBuffers();
      const auto tGrayMsb = millis();

      renderer.displayGrayBuffer();
      const auto tGrayDisplay = millis();
      renderer.setRenderMode(GfxRenderer::BW);
      renderer.restoreBwBuffer();
      const auto tBwRestore = millis();

      const auto tEnd = millis();
      LOG_DBG("ERS",
              "Page render: prewarm=%lums bw_render=%lums display=%lums bw_store=%lums "
              "gray_lsb=%lums gray_msb=%lums gray_display=%lums bw_restore=%lums total=%lums",
              tPrewarm - t0, tBwRender - tPrewarm, tDisplay - tBwRender, tBwStore - tDisplay, tGrayLsb - tBwStore,
              tGrayMsb - tGrayLsb, tGrayDisplay - tGrayMsb, tBwRestore - tGrayDisplay, tEnd - t0);
    } else {
      const auto tEnd = millis();
      LOG_DBG("ERS", "Page render: prewarm=%lums bw_render=%lums display=%lums total=%lums", tPrewarm - t0,
              tBwRender - tPrewarm, tDisplay - tBwRender, tEnd - t0);
    }
  }
}

int EpubReaderActivity::clippingAtPoint(const Page& page, const int x, const int y) const {
  int top, right, bottom, left;
  renderer.getOrientedViewableTRBL(&top, &right, &bottom, &left);
  const int fontId = SETTINGS.getReaderFontId();
  const uint32_t signature =
      readerRenderSpecSignature(SETTINGS.readerRenderSpec(buildViewportWidth, buildViewportHeight));
  uint16_t wordIndex = 0;
  for (const auto& element : page.elements) {
    if (element->getTag() != TAG_PageLine) continue;
    const auto& line = static_cast<const PageLine&>(*element);
    const auto& block = line.getBlock();
    if (!block || !block->valid()) continue;
    for (uint16_t i = 0; i < block->wordCount(); ++i) {
      const char* text = block->wordText(i);
      if (!clippingText::hasVisibleText(text)) continue;
      const int width = block->wordGeometry(renderer, fontId, i).width;
      if (width <= 0) continue;
      const uint16_t index = wordIndex++;
      const Rect rect =
          clippingWordRect(renderer, fontId, line, i, top + SETTINGS.screenMargin + UITheme::getReaderTopBarHeight(),
                           left + SETTINGS.screenMargin, width);
      const auto cap =
          clippingCapRect(renderer, fontId, line, i, top + SETTINGS.screenMargin + UITheme::getReaderTopBarHeight(),
                          left + SETTINGS.screenMargin);
      const bool inWord = x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.height;
      const bool inCap = x >= cap.x && x < cap.x + cap.width && y >= cap.y && y < cap.y + cap.height;
      if (!inWord && !inCap) continue;
      const auto range = block->wordSourceRange(i);
      for (size_t j = 0; j < CLIPPINGS.clippingCount(); ++j) {
        if (clippingContainsWord(*CLIPPINGS.clippingAt(j), currentSpineIndex, section->currentPage, section->pageCount,
                                 signature, index, range.start, range.end))
          return static_cast<int>(j);
      }
    }
  }
  return -1;
}

void EpubReaderActivity::drawClippingHighlights(const Page& page, const int fontId, const int orientedMarginTop,
                                                const int orientedMarginLeft) const {
  if (!section || !CLIPPINGS.hasClippings() || section->currentPage < 0 || section->currentPage >= section->pageCount) {
    return;
  }

  size_t chapterClippings = 0;
  for (size_t i = 0; i < CLIPPINGS.clippingCount(); ++i) {
    if (CLIPPINGS.clippingAt(i)->spineIndex == currentSpineIndex) ++chapterClippings;
  }
  if (!chapterClippings) return;

  const auto started = millis();
  size_t highlightedWords = 0;
  const uint16_t currentPage = static_cast<uint16_t>(section->currentPage);
  const uint32_t layoutSignature =
      readerRenderSpecSignature(SETTINGS.readerRenderSpec(buildViewportWidth, buildViewportHeight));
  const auto isHighlighted = [&](const uint16_t wordIndex, const TextBlock::SourceRange range) {
    // ponytail: scan at most 256 C3 / 1024 S3 clippings per word; index by chapter if this becomes slow.
    for (size_t index = 0; index < CLIPPINGS.clippingCount(); ++index) {
      const Clipping& clipping = *CLIPPINGS.clippingAt(index);
      if (clippingContainsWord(clipping, static_cast<uint16_t>(currentSpineIndex), currentPage, section->pageCount,
                               layoutSignature, wordIndex, range.start, range.end))
        return true;
    }
    return false;
  };

  const auto fillHighlight = [&](const int x, const int y, const int width, const int height) {
    if (BoardConfig::isEegoA4() && renderer.getRenderMode() != GfxRenderer::BW) {
      // Gray planes start at zero and use set bits for ink, opposite to B/W.
      for (int py = y + (y & 1); py < y + height; py += 2) {
        for (int px = x + (x & 1); px < x + width; px += 2) renderer.drawPixel(px, py, false);
      }
    } else {
      renderer.fillRectDither(x, y, width, height, Color::LightGray);
    }
  };

  uint16_t pageWordIndex = 0;
  for (const auto& element : page.elements) {
    if (element->getTag() != TAG_PageLine) continue;
    const auto& line = static_cast<const PageLine&>(*element);
    const auto& block = line.getBlock();
    if (!block || !block->valid()) continue;
    bool hasPreviousHighlight = false;
    int previousHighlightX = 0;
    int previousHighlightWidth = 0;
    for (uint16_t i = 0; i < block->wordCount(); ++i) {
      const char* text = block->wordText(i);
      if (!clippingText::hasVisibleText(text)) continue;
      int width = block->wordGeometry(renderer, fontId, i).width;
      if (width <= 0) continue;
      const uint16_t wordIndex = pageWordIndex++;
      if (!isHighlighted(wordIndex, block->wordSourceRange(i))) {
        hasPreviousHighlight = false;
        continue;
      }

      const Rect rect = clippingWordRect(renderer, fontId, line, i, orientedMarginTop, orientedMarginLeft, width);
      const auto cap = clippingCapRect(renderer, fontId, line, i, orientedMarginTop, orientedMarginLeft);
      if (cap.width > 0) fillHighlight(cap.x, cap.y, cap.width, cap.height);
      const int x = rect.x;
      const int y = rect.y;
      width = rect.width;
      if (width > 0) {
        const int highlightRight = x + width;
        const int previousHighlightRight = previousHighlightX + previousHighlightWidth;
        if (hasPreviousHighlight && previousHighlightRight < x) {
          fillHighlight(previousHighlightRight, y, x - previousHighlightRight, rect.height);
        } else if (hasPreviousHighlight && highlightRight < previousHighlightX) {
          fillHighlight(highlightRight, y, previousHighlightX - highlightRight, rect.height);
        }
        fillHighlight(x, y, width, rect.height);
        previousHighlightX = x;
        previousHighlightWidth = width;
        hasPreviousHighlight = true;
        ++highlightedWords;
      } else {
        hasPreviousHighlight = false;
      }
    }
  }
  LOG_DBG("CLIP", "Highlight pass: chapter_clips=%u highlighted_words=%u time=%lums",
          static_cast<unsigned>(chapterClippings), static_cast<unsigned>(highlightedWords), millis() - started);
}

void EpubReaderActivity::renderStatusBar() {
  const int currentPage = section ? section->currentPage + 1 : 1;
  const float pageCount = section ? section->estimatedTotalPages() : 1;
  const float sectionChapterProg = (pageCount > 0) ? (static_cast<float>(currentPage) / pageCount) : 0;
  const float bookProgress = epub ? (epub->calculateProgress(currentSpineIndex, sectionChapterProg) * 100) : 0;

  if (SETTINGS.readerBars.customized) {
    readerbars::Data data;
    data.book = epub ? epub->getTitle().c_str() : "";

    const bool chapterNeeded = SETTINGS.readerBars.top.contains(readerbars::Item::Chapter) ||
                               SETTINGS.readerBars.bottom.contains(readerbars::Item::Chapter);
    if (chapterNeeded && epub && barChapterSpine != currentSpineIndex) {
      const int toc = epub->getTocIndexForSpineIndex(currentSpineIndex);
      const auto item = toc >= 0 ? epub->getTocItem(toc) : BookMetadataCache::TocEntry{};
      readerbars::copyLabel(barChapterTitle, toc >= 0 ? item.title.c_str() : tr(STR_UNNAMED));
      barChapterSpine = currentSpineIndex;
    }
    char autoTurn[48]{};
    if (automaticPageTurnActive) {
      snprintf(autoTurn, sizeof(autoTurn), "%s%lu", tr(STR_AUTO_TURN_ENABLED), 60000UL / pageTurnDuration);
    }
    data.chapter = automaticPageTurnActive ? autoTurn : barChapterTitle;
    data.percent = bookProgress;
    data.page = currentPage;
    data.pages = static_cast<int>(pageCount);
    data.estimatedPages = section && section->isBuilding();
    data.bookmarked = currentPageBookmarked;
    if (SETTINGS.readerBars.needsPace() && epub) {
      pacePosition = static_cast<uint32_t>(
          epub->calculateProgress(currentSpineIndex, pageCount > 0 ? (currentPage - 1) / pageCount : 0) *
          readerbars::Pace::UNITS);
      const auto end =
          static_cast<uint32_t>(epub->calculateProgress(currentSpineIndex, 1.0f) * readerbars::Pace::UNITS);
      data.timeBook = readingPace.remaining(pacePosition);
      data.timeChapter = readingPace.remaining(pacePosition, end);
    }
    if (automaticPageTurnActive) {
      readerbars::draw(renderer, SETTINGS.readerBars.top, readerbars::Position::Top, data, barWorkspace);
      auto config = SETTINGS.readerBars.bottom;
      config.visible = true;
      config.slots[3] = readerbars::Item::Chapter;
      readerbars::draw(renderer, config, readerbars::Position::Bottom, data, barWorkspace);
    } else {
      drawConfiguredBars(data);
    }
    if (overlay == Overlay::None) renderContentsReturn();
    return;
  }

  std::string title;
  int textYOffset = 0;
  const auto sb = SETTINGS.statusBarSpec();

  if (automaticPageTurnActive) {
    title = tr(STR_AUTO_TURN_ENABLED) + std::to_string(60 * 1000 / pageTurnDuration);
    const uint8_t statusBarHeight = UITheme::getInstance().getStatusBarHeight();
    if (statusBarHeight == 0 || statusBarHeight == UITheme::getInstance().getProgressBarHeight()) {
      textYOffset += UITheme::getInstance().getMetrics().statusBarVerticalMargin;
    }
  } else if (sb.titleMode == CrossPointSettings::STATUS_BAR_TITLE::CHAPTER_TITLE) {
    title = tr(STR_UNNAMED);
    if (epub) {
      const int tocIndex = epub->getTocIndexForSpineIndex(currentSpineIndex);
      if (tocIndex != -1) {
        const auto tocItem = epub->getTocItem(tocIndex);
        title = tocItem.title;
      }
    }
  } else if (sb.titleMode == CrossPointSettings::STATUS_BAR_TITLE::BOOK_TITLE) {
    title = epub ? epub->getTitle() : "";
  }

  GUI.drawStatusBar(renderer, bookProgress, currentPage, pageCount, title, 0, textYOffset, true, currentPageBookmarked,
                    section ? section->isBuilding() : false);
  if (overlay == Overlay::None) renderContentsReturn();
}
void EpubReaderActivity::restoreContentsPosition(ContentsPosition position) {
  clearDeferredReposition();
  pendingAnchor.clear();
  pendingPageJump.reset();
  pendingPercentJump = false;
  pendingOffsetJump.reset();
  currentSpineIndex = position.spine;
  nextPageNumber = position.page;
  if (position.offset != UINT32_MAX) pendingOffsetJump = position.offset;
  resetSection();
}
ReaderActivity::ContentsPosition EpubReaderActivity::contentsPagePosition() const {
  return {currentSpineIndex, static_cast<uint32_t>(section ? section->currentPage : nextPageNumber)};
}

// ---------------------------------------------------------------------------
// Toolbar reader menu
// ---------------------------------------------------------------------------

namespace {
constexpr StrId kSpacingIds[] = {StrId::STR_LINE_HEIGHT_100, StrId::STR_LINE_HEIGHT_110, StrId::STR_LINE_HEIGHT_120,
                                 StrId::STR_LINE_HEIGHT_130, StrId::STR_LINE_HEIGHT_145, StrId::STR_LINE_HEIGHT_160,
                                 StrId::STR_LINE_HEIGHT_175};
constexpr StrId kAlignIds[] = {StrId::STR_JUSTIFY, StrId::STR_ALIGN_LEFT, StrId::STR_CENTER, StrId::STR_ALIGN_RIGHT,
                               StrId::STR_BOOK_S_STYLE};
static_assert(std::size(kSpacingIds) == CrossPointSettings::LINE_COMPRESSION_COUNT, "line spacing labels");
static_assert(std::size(kAlignIds) == CrossPointSettings::PARAGRAPH_ALIGNMENT_COUNT, "alignment labels");
}  // namespace

bool EpubReaderActivity::usesToolbarMenu() const {
  // Both board classes drive the same chrome: touch through the FreeInkUI tap
  // targets, buttons through the focused-tool pill and the panel cursor.
  return SETTINGS.readerMenuStyle == CrossPointSettings::READER_MENU_TOOLBAR;
}

std::string EpubReaderActivity::currentChapterTitle() const {
  if (!epub) return "";
  const int tocIndex = epub->getTocIndexForSpineIndex(currentSpineIndex);
  if (tocIndex != -1) {
    return epub->getTocItem(tocIndex).title;
  }
  return tr(STR_UNNAMED);
}

// Paint the controls immediately; coalesce rapid adjustments before the page preview reflows.
void EpubReaderActivity::applyTextSettingLive() {
  const auto requestedAt = performance::timeMicros();
  RenderLock lock;
  performance::begin(performance::Operation::Format, this, requestedAt);
  textFormatPending = true;
  textFormatChangedAt = millis();
  settleOverlayRefresh();
  if (overlayPageStored) renderer.copyStoredBwBuffer();
  toolbarUi->begin();
  renderOverlay();
  pushOverlayRefresh();
}

void EpubReaderActivity::commitTextFormat() {
  if (!textFormatPending) return;
  applyReaderTextSettings();
}

void EpubReaderActivity::discardOverlayPage() {
  if (!overlayPageStored) return;
  renderer.discardStoredBwBuffer();
  overlayPageStored = false;
}

// Push freshly painted overlay chrome. Where the panel supports it the refresh
// is fired deferred: the loop keeps polling input while the waveform runs, so
// the chrome answers taps and buttons the moment it is visible instead of only
// after a blocking displayBuffer() returns. Caller must hold the RenderLock.
void EpubReaderActivity::pushOverlayRefresh() {
  if (renderer.supportsAsyncRefresh()) {
    renderer.displayBufferAsync(HalDisplay::FAST_REFRESH);
    overlayRefreshPending = true;
    // Counted on the next input tick; see handleOverlayInput().
    overlayPaintPending = true;
  } else {
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  }
}

// Wait out a pending deferred overlay refresh and reseed the panel's
// differential baseline from the framebuffer: the shadow-free async path skips
// the post-refresh resync, so without this the next FAST diff would run
// against the frame from before the chrome and leave stale pixels on the
// glass. Caller must hold the RenderLock.
void EpubReaderActivity::settleOverlayRefresh() {
  if (!overlayRefreshPending) return;
  overlayRefreshPending = false;
  renderer.cleanupGrayscaleWithFrameBuffer();  // waits, then reseeds the baseline
  if (presentationComplete()) performance::finish(renderedOperation, this);
}

void EpubReaderActivity::openOverlay(Overlay target, const bool showStyle) {
  RenderLock lock;
  readingPace.pause();
  clearPrefetchedPage();
  const Overlay previous = overlay;
  {
    mappedInput.resetHomeButtonInput();
    if (!toolbarUi) {
      toolbarUi = makeUniqueNoThrow<ReaderToolbarUi>(renderer);
      if (!toolbarUi) {
        LOG_ERR("ERS", "OOM: reader sheet");
        return;
      }
    }
    overlay = target;
    if (previous == Overlay::None || target == Overlay::Text) toolbarUi->begin();
    // Buttons show a cursor from the start; touch boards only once a button moves it.
    panelCursorShown = !mappedInput.hasTouch();
    switch (target) {
      case Overlay::Toolbar:
        focusedTool = 0;
        break;
      case Overlay::Contents:
        panelIndex = std::max(0, epub->getTocIndexForSpineIndex(currentSpineIndex));
        // Fresh viewport opening on the current chapter, cursor shown or not.
        toolbarUi->nav().reset(panelIndex);
        toolbarUi->nav().top = panelIndex;
        break;
      case Overlay::Text:
        textSheetView = showStyle ? readertext::SheetView::Style : readertext::SheetView::Controls;
        textSheetTab = showStyle ? 3 : 2;
        // Choose the initial page when the Contents view reads the current chapter.
        textListPage = showStyle ? 0 : -1;
        if (showStyle) buildMoreActions();
        panelIndex = 0;
        toolbarUi->nav().reset();
        break;
      case Overlay::More:
        panelIndex = 0;
        buildMoreActions();
        toolbarUi->nav().reset();
        break;
      default:
        break;
    }
    panelHoldJumped = false;
  }

  // The page is already on screen and still in the framebuffer, so paint the
  // chrome straight onto it and push one refresh. requestUpdate() would
  // re-render the whole page first: slow, and visibly wrong, since that repaint
  // lands before the overlay does.
  //
  // Refresh mode: FAST for every overlay paint, first open included. The AA
  // pass only grays glyph edges, and residue a FAST differential leaves under
  // the sheet has not shown in practice; it also self-heals on the
  // Xteink-class panels, whose close path re-renders the page. If text or
  // images ever visibly ghost through the chrome, restore a HALF cleanup on
  // the first open (see #2190 for the mechanism).
  if (section && !pageBufferStale) {
    // Serialize against the render task: renderBook may be mid-page (status
    // bar included) in the shared framebuffer, and painting the chrome from
    // the loop task at the same time interleaves the two frames.
    settleOverlayRefresh();
    if (previous == Overlay::None) {
      // Snapshot the clean page so stepping back from a panel to the toolbar
      // (and closing, where supported) can restore it without a re-render.
      overlayPageStored = renderer.storeBwBuffer();
    } else if (overlayPageStored) {
      // Overlay -> overlay: wipe the previous chrome (toolbar header, sheet,
      // progress row) while retaining the clean snapshot. The differential
      // refresh still needs the panel's baseline for the old chrome.
      if (!renderer.copyStoredBwBuffer()) {
        discardOverlayPage();
        requestUpdate();
        return;
      }
    }
    renderOverlay();
    pushOverlayRefresh();
  } else {
    requestUpdate();  // no page in the framebuffer: renderBook() draws the overlay once it is
  }
}

// Grayscale panels re-render to restore AA; other boards restore the B/W snapshot.
void EpubReaderActivity::closeOverlayToPage() {
  commitTextFormat();
  RenderLock lock;
  settleOverlayRefresh();
  mappedInput.resetHomeButtonInput();
  overlay = Overlay::None;
  textSheetView = readertext::SheetView::Controls;
  overlayPopup.dismiss();  // an option picker cannot outlive its panel
  toolbarUi.reset();       // only needed while open
  if (BoardConfig::isEegoA4()) pagesUntilFullRefresh = 1;
  if (!xteinkClassPanel() && overlayPageStored) {
    // No baseline resync: the glass shows the chrome, and erasing it needs
    // the differential to keep diffing against the last pushed frame.
    renderer.restoreBwBuffer(/*resyncPanelBaseline=*/false);
    overlayPageStored = false;
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    return;
  }
  discardOverlayPage();
  requestUpdate();  // redraw the clean page
}

void EpubReaderActivity::renderOverlay() {
  if (!epub || !section || !toolbarUi) return;

  auto& model = overlayModel;
  model = {};
  if (overlay == Overlay::Text) {
    model.textSheet = true;
    auto& text = model.text;
    text.tab = textSheetTab;
    text.contentsAvailable = true;
    if (textSheetTab == 2) {
      text.currentChapter = epub->getTocIndexForSpineIndex(currentSpineIndex);
      if (textListPage < 0)
        textListPage =
            std::max(0, text.currentChapter) / readertext::sheetRowsPerPage(toolbarUi->uiTarget.deviceContext());
    }
    text.listPage = textListPage;
    text.listContext = this;
    text.alignmentIndex = SETTINGS.paragraphAlignment;
    text.listCount = textSheetTab == 2 ? epub->getTocItemsCount() : static_cast<int>(moreItems.size());
    text.listLabel = [](const void* context, int n, char* out, size_t size) {
      const auto* self = static_cast<const EpubReaderActivity*>(context);
      if (self->textSheetTab == 2) {
        self->epub->copyTocTitle(n, out, size);
      } else if (n < int(self->moreItems.size())) {
        std::snprintf(out, size, "%s", I18N.get(self->moreItems[n].labelId));
      }
    };
    text.view = textSheetView;
    text.fontPage = textFontPage;
    if (textSheetView == readertext::SheetView::Families) {
      text.fontCount = readerFontFamilyCount(&sdFontSystem.registry());
      text.fontSelected = readerFontFamilyIndex(&sdFontSystem.registry(), SETTINGS.sdFontFamilyName);
    }
    text.fontContext = &sdFontSystem.registry();
    text.fontLabel = [](const void* context, int index) -> const char* {
      if (index < CrossPointSettings::BUILTIN_FONT_COUNT) return tr(STR_MENU_FONT_CLASSIC);
      const auto* family = readerFontFamilyAt(static_cast<const SdCardFontRegistry*>(context), index);
      return family ? family->name.c_str() : "";
    };
    text.family = SETTINGS.sdFontFamilyName[0] ? SETTINGS.sdFontFamilyName : tr(STR_MENU_FONT_CLASSIC);
    text.size = SETTINGS.fontPointSize;
    text.bold = SETTINGS.fontWeight == CrossPointSettings::FONT_WEIGHT_BOLD;
    text.sizeCount = static_cast<int>(textSheetSizeCount);
    for (size_t n = 0; n < textSheetSizeCount; ++n)
      if (textSheetSizes[n] <= SETTINGS.fontPointSize) text.sizeIndex = static_cast<int>(n);
    text.spacing = SETTINGS.lineSpacing % CrossPointSettings::LINE_COMPRESSION_COUNT;
    text.spacingCount = CrossPointSettings::LINE_COMPRESSION_COUNT;
    text.spacingLabel = I18N.get(kSpacingIds[text.spacing]);
    text.alignment = I18N.get(kAlignIds[SETTINGS.paragraphAlignment % CrossPointSettings::PARAGRAPH_ALIGNMENT_COUNT]);
    text.margin = SETTINGS.screenMargin;
    text.marginIndex =
        (SETTINGS.screenMargin - CrossPointSettings::SCREEN_MARGIN_MIN) / CrossPointSettings::SCREEN_MARGIN_STEP;
    text.marginCount = (CrossPointSettings::SCREEN_MARGIN_MAX - CrossPointSettings::SCREEN_MARGIN_MIN) /
                           CrossPointSettings::SCREEN_MARGIN_STEP +
                       1;
    text.layoutPage = textLayoutPage;
    text.indent = SETTINGS.paragraphIndentSpaces;
    text.wordSpacing = SETTINGS.wordSpacing;
    text.characterSpacing = SETTINGS.characterSpacing;
    text.paragraphs = SETTINGS.extraParagraphSpacing;
    text.focus = SETTINGS.focusReadingEnabled;
    text.hyphenation = SETTINGS.hyphenationEnabled;
    text.embedded = SETTINGS.embeddedStyle;
    text.antialiasing = SETTINGS.textAntiAliasing;
    toolbarUi->setModel(model);
    toolbarUi->render();
    return;
  }

  // The toolbar's tool pill is the button-navigation cursor: tap-first (same
  // convention as the panel lists), it only shows once a button has moved it.
  // Panels override below: there the pill marks the open panel on every board.
  model.activeTool = (overlay == Overlay::Toolbar && !panelCursorShown) ? -1 : focusedTool;
  // Strings the model points at live here until render() returns.
  std::string chapterTitle, pageInfo;

  if (overlay == Overlay::Toolbar) {
    chapterTitle = currentChapterTitle();
    const int pageCount = section->estimatedTotalPages();
    const float chapterProgress =
        pageCount > 0 ? static_cast<float>(section->currentPage + 1) / static_cast<float>(pageCount) : 0.0f;
    const float bookProgress = epub->calculateProgress(currentSpineIndex, chapterProgress);
    pageInfo = std::to_string(section->currentPage + 1) + "/" + std::to_string(pageCount) + "   " +
               std::to_string(clampPercent(static_cast<int>(bookProgress * 100.0f + 0.5f))) + "%";
    model.chapterTitle = chapterTitle.c_str();
    model.pageInfo = pageInfo.c_str();
    model.progressPermille = static_cast<int>(bookProgress * 1000.0f + 0.5f);
    toolbarUi->setModel(model);
    toolbarUi->render();
    return;
  }

  // Panels (Contents / Text / More): a bottom sheet over the page.
  model.panel = true;
  if (!mappedInput.hasTouch()) {
    model.denseRows = true;
  }
  // Tap-first: the cursor is only drawn once a button has moved it, so a
  // tapped row does not stay inverted after its action.
  model.selectedIndex = panelCursorShown ? panelIndex : -1;
  if (overlay == Overlay::Contents) {
    model.panelTitle = tr(STR_TOOL_CONTENTS);
    model.itemCount = epub->getTocItemsCount();
    model.rowText = [this](int i) {
      const auto item = epub->getTocItem(i);
      const int depth = item.level > 1 ? (item.level - 1) * 2 : 0;
      return std::string(depth, ' ') + item.title;
    };
  } else {
    model.panelTitle = tr(STR_TOOL_MORE);
    model.itemCount = static_cast<int>(moreItems.size());
    model.rowText = [this](int i) { return moreRowName(i); };
    model.rowValue = [this](int i) { return moreRowValue(i); };
    model.rowCheckboxContext = this;
    model.rowCheckbox = [](void* ctx, int i, freeink::ui::ListItem& item) {
      const auto* self = static_cast<EpubReaderActivity*>(ctx);
      using MA = EpubReaderMenuActivity::MenuAction;
      const auto action = self->moreItems[i].action;
      if (action == MA::NIGHT_MODE) GUI.setCheckboxRow(item, SETTINGS.screenInverted);
      if (action == MA::FRONTLIGHT) GUI.setCheckboxRow(item, Frontlight.isOn());
    };
  }
  toolbarUi->setModel(model);
  toolbarUi->render();
}

void EpubReaderActivity::loadTextSheetSizes() {
  textSheetSizeCount = 0;
  for (uint8_t size : readerFontPointSizes(&sdFontSystem.registry(), SETTINGS.sdFontFamilyName)) {
    if (textSheetSizeCount == std::size(textSheetSizes)) break;
    textSheetSizes[textSheetSizeCount++] = size;
  }
}

void EpubReaderActivity::handleTextSheetInput(const ReaderToolbarUi::Routed& routed) {
  using Event = ReaderToolbarUi::Event;
  if (textFormatPending &&
      (routed.event != Event::Row || routed.value < readertext::SizeDown || routed.value > readertext::MarginUp)) {
    // A navigation action must not carry an old page snapshot into another view.
    if (routed.event != Event::None || mappedInput.wasReleased(MappedInputManager::Button::Back) ||
        mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
        mappedInput.wasSwipe() != MappedInputManager::SwipeDir::None) {
      commitTextFormat();
      requestUpdate();
    }
  }
  const auto swipe = mappedInput.wasSwipe();
  const int direction = swipe == MappedInputManager::SwipeDir::Left    ? 1
                        : swipe == MappedInputManager::SwipeDir::Right ? -1
                                                                       : 0;
  const auto pageControl = readertext::pageSwipeControl(textSheetTab, textSheetView, direction);
  if (pageControl == readertext::LayoutNext || pageControl == readertext::LayoutPrevious) {
    if (!toolbarUi->routingReady()) return;
    RenderLock lock;
    const int page =
        std::clamp(textLayoutPage + direction, 0, readertext::layoutPageCount(toolbarUi->uiTarget.deviceContext()) - 1);
    if (page != textLayoutPage) {
      settleOverlayRefresh();
      textLayoutPage = page;
      toolbarUi->begin();
      renderOverlay();
      pushOverlayRefresh();
    }
    return;
  }

  int swipeStep = 0;
  if (textSheetView == readertext::SheetView::Families ||
      ((textSheetTab == 2 || textSheetTab == 3) && textSheetView == readertext::SheetView::Controls)) {
    switch (swipe) {
      case MappedInputManager::SwipeDir::Left:
        swipeStep = 1;
        break;
      case MappedInputManager::SwipeDir::Right:
        swipeStep = -1;
        break;
      case MappedInputManager::SwipeDir::None:
      case MappedInputManager::SwipeDir::Up:
      case MappedInputManager::SwipeDir::Down:
        break;
    }
  }
  if ((textSheetView == readertext::SheetView::Families &&
       (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
        mappedInput.wasReleased(MappedInputManager::Button::Confirm))) ||
      (textSheetView == readertext::SheetView::Style &&
       (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
        (routed.event == Event::Row && routed.value == readertext::StyleBack)))) {
    RenderLock lock;
    settleOverlayRefresh();
    textSheetView = readertext::SheetView::Controls;
    toolbarUi->begin();
    renderOverlay();
    pushOverlayRefresh();
    return;
  }
  if (routed.event == Event::Dismiss || (!swipeStep && mappedInput.wasReleased(MappedInputManager::Button::Back)) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
      mappedInput.wasSwipe() == MappedInputManager::SwipeDir::Down) {
    closeOverlayToPage();
    return;
  }
  int nextTab = textSheetTab;
  if (routed.event == Event::Tool)
    nextTab = std::clamp(routed.value, 0, 3);
  else if (mappedInput.wasReleased(MappedInputManager::Button::Up))
    nextTab = textSheetTab == 2 ? 3 : textSheetTab == 0 ? 2 : textSheetTab == 1 ? 0 : 1;
  else if (mappedInput.wasReleased(MappedInputManager::Button::Down))
    nextTab = textSheetTab == 2 ? 0 : textSheetTab == 0 ? 1 : textSheetTab == 1 ? 3 : 2;
  if (textSheetView != readertext::SheetView::Families && nextTab != textSheetTab) {
    RenderLock lock;
    settleOverlayRefresh();
    textSheetTab = nextTab;
    if (nextTab == 0) loadTextSheetSizes();
    if (nextTab == 3) buildMoreActions();
    textSheetView = readertext::SheetView::Controls;
    textListPage = 0;
    toolbarUi->begin();
    renderOverlay();
    pushOverlayRefresh();
    return;
  }
  if (routed.event != Event::Row && !swipeStep) return;
  if (routed.value == readertext::AllChapters && textSheetTab == 2) {
    {
      RenderLock lock;
      settleOverlayRefresh();
      overlay = Overlay::None;
      discardOverlayPage();
    }
    onReaderMenuConfirm(EpubReaderMenuActivity::MenuAction::SELECT_CHAPTER, true);
    return;
  }
  const bool fontSwipe = swipeStep && textSheetView == readertext::SheetView::Families;
  if ((!fontSwipe && swipeStep) || routed.value == readertext::SheetPrevious || routed.value == readertext::SheetNext) {
    RenderLock lock;
    const int count = textSheetTab == 2 ? epub->getTocItemsCount() : int(moreItems.size());
    const int capacity = readertext::sheetRowsPerPage(toolbarUi->uiTarget.deviceContext(), false, textSheetTab);
    const int step = swipeStep ? swipeStep : (routed.value == readertext::SheetNext ? 1 : -1);
    const int page = std::clamp(textListPage + step, 0, std::max(0, (count - 1) / capacity));
    if (page == textListPage) return;
    settleOverlayRefresh();
    textListPage = page;
    toolbarUi->begin();
    renderOverlay();
    pushOverlayRefresh();
    return;
  }
  if (textSheetTab == 2 && routed.value >= readertext::ChapterPick) {
    const int n = routed.value - readertext::ChapterPick;
    if (n >= epub->getTocItemsCount()) return;
    {
      RenderLock lock;
      settleOverlayRefresh();
      const auto item = epub->getTocItem(n);
      if (item.spineIndex < 0) return;
      rememberContentsPosition({currentSpineIndex, static_cast<uint32_t>(chapterPosition().pageIndex),
                                currentPageVisibleOffset.value_or(UINT32_MAX)});
      clearDeferredReposition();
      currentSpineIndex = item.spineIndex;
      pendingAnchor = item.anchor;
      nextPageNumber = 0;
      resetSection();
      overlay = Overlay::None;
      discardOverlayPage();
    }
    requestUpdate();
    return;
  }
  if (textSheetTab == 3 && routed.value >= readertext::MorePick) {
    const int n = routed.value - readertext::MorePick;
    if (n >= 0 && n < int(moreItems.size())) activateMoreRow(n);
    return;
  }
  if (fontSwipe || routed.value == readertext::Family || routed.value == readertext::FontBack ||
      routed.value == readertext::FontPrevious || routed.value == readertext::FontNext) {
    RenderLock lock;
    if (fontSwipe || routed.value == readertext::FontPrevious || routed.value == readertext::FontNext) {
      const int count = readerFontFamilyCount(&sdFontSystem.registry());
      const int capacity = readertext::fontRowsPerPage(toolbarUi->uiTarget.deviceContext());
      const int last = std::max(0, (count - 1) / capacity);
      const int step = fontSwipe ? swipeStep : (routed.value == readertext::FontNext ? 1 : -1);
      const int page = std::clamp(textFontPage + step, 0, last);
      if (page == textFontPage) return;
      textFontPage = page;
    }
    settleOverlayRefresh();
    if (!fontSwipe && routed.value == readertext::Family) {
      textSheetView = readertext::SheetView::Families;
      const int selected = readerFontFamilyIndex(&sdFontSystem.registry(), SETTINGS.sdFontFamilyName);
      textFontPage = selected / readertext::fontRowsPerPage(toolbarUi->uiTarget.deviceContext());
    } else if (routed.value == readertext::FontBack) {
      textSheetView = readertext::SheetView::Controls;
    }
    toolbarUi->begin();
    renderOverlay();
    pushOverlayRefresh();
    return;
  }
  if (routed.value >= readertext::FontPick && textSheetView == readertext::SheetView::Families) {
    const int index = routed.value - readertext::FontPick;
    {
      RenderLock lock;
      if (index < CrossPointSettings::BUILTIN_FONT_COUNT) {
        if (!SETTINGS.sdFontFamilyName[0] && SETTINGS.fontFamily == index) return;
        SETTINGS.fontFamily = static_cast<uint8_t>(index);
        SETTINGS.sdFontFamilyName[0] = 0;
      } else {
        const auto* family = readerFontFamilyAt(&sdFontSystem.registry(), index);
        if (!family) return;
        if (family->name == SETTINGS.sdFontFamilyName) return;
        std::snprintf(SETTINGS.sdFontFamilyName, sizeof(SETTINGS.sdFontFamilyName), "%s", family->name.c_str());
      }
    }
    applyReaderTextSettings();
    {
      RenderLock lock;
      loadTextSheetSizes();
      toolbarUi->begin();
    }
    requestUpdate();
    return;
  }
  if (routed.value == readertext::LayoutPrevious || routed.value == readertext::LayoutNext) {
    textLayoutPage = std::clamp(textLayoutPage + (routed.value == readertext::LayoutNext ? 1 : -1), 0,
                                readertext::layoutPageCount(toolbarUi->uiTarget.deviceContext()) - 1);
    RenderLock lock;
    toolbarUi->begin();
    renderOverlay();
    pushOverlayRefresh();
    return;
  }
  bool changed = false;
  {
    RenderLock lock;
    pendingManualTurn = 0;
    pendingManualTurnTouch = false;
    switch (routed.value) {
      case readertext::SizeDown:
      case readertext::SizeUp: {
        if (textSheetSizeCount == 0) break;
        int index = 0;
        for (size_t n = 0; n < textSheetSizeCount; ++n)
          if (textSheetSizes[n] <= SETTINGS.fontPointSize) index = static_cast<int>(n);
        index = std::clamp(index + (routed.value == readertext::SizeUp ? 1 : -1), 0,
                           static_cast<int>(textSheetSizeCount) - 1);
        changed = SETTINGS.fontPointSize != textSheetSizes[index];
        SETTINGS.fontPointSize = textSheetSizes[index];
        break;
      }
      case readertext::WeightDown:
      case readertext::WeightUp: {
        const auto weight = routed.value == readertext::WeightUp ? CrossPointSettings::FONT_WEIGHT_BOLD
                                                                 : CrossPointSettings::FONT_WEIGHT_REGULAR;
        changed = SETTINGS.fontWeight != weight;
        SETTINGS.fontWeight = weight;
        break;
      }
      case readertext::SpacingDown:
      case readertext::SpacingUp: {
        const int value = std::clamp(int(SETTINGS.lineSpacing) + (routed.value == readertext::SpacingUp ? 1 : -1), 0,
                                     int(CrossPointSettings::LINE_COMPRESSION_COUNT) - 1);
        changed = SETTINGS.lineSpacing != value;
        SETTINGS.lineSpacing = value;
        break;
      }
      case readertext::MarginDown:
      case readertext::MarginUp: {
        const int value =
            std::clamp(int(SETTINGS.screenMargin) + (routed.value == readertext::MarginUp
                                                         ? int(CrossPointSettings::SCREEN_MARGIN_STEP)
                                                         : -int(CrossPointSettings::SCREEN_MARGIN_STEP)),
                       int(CrossPointSettings::SCREEN_MARGIN_MIN), int(CrossPointSettings::SCREEN_MARGIN_MAX));
        changed = SETTINGS.screenMargin != value;
        SETTINGS.screenMargin = value;
        break;
      }
      case readertext::AlignmentLeft:
      case readertext::AlignmentCenter:
      case readertext::AlignmentJustify:
      case readertext::AlignmentRight:
      case readertext::AlignmentBook: {
        const auto value = routed.value == readertext::AlignmentLeft     ? CrossPointSettings::LEFT_ALIGN
                           : routed.value == readertext::AlignmentCenter ? CrossPointSettings::CENTER_ALIGN
                           : routed.value == readertext::AlignmentRight  ? CrossPointSettings::RIGHT_ALIGN
                           : routed.value == readertext::AlignmentBook   ? CrossPointSettings::BOOK_STYLE
                                                                         : CrossPointSettings::JUSTIFIED;
        changed = value != SETTINGS.paragraphAlignment;
        SETTINGS.paragraphAlignment = value;
        break;
      }
      case readertext::Alignment:
        SETTINGS.paragraphAlignment = (SETTINGS.paragraphAlignment + 1) % CrossPointSettings::PARAGRAPH_ALIGNMENT_COUNT;
        changed = true;
        break;
      case readertext::IndentDown:
      case readertext::IndentUp: {
        const int value =
            std::clamp(int(SETTINGS.paragraphIndentSpaces) + (routed.value == readertext::IndentUp ? 1 : -1), 0, 5);
        changed = value != SETTINGS.paragraphIndentSpaces;
        SETTINGS.paragraphIndentSpaces = value;
      } break;
      case readertext::WordDown:
      case readertext::WordUp: {
        const int value =
            std::clamp(int(SETTINGS.wordSpacing) + (routed.value == readertext::WordUp ? 25 : -25), 50, 200);
        changed = value != SETTINGS.wordSpacing;
        SETTINGS.wordSpacing = value;
      } break;
      case readertext::CharacterDown:
      case readertext::CharacterUp: {
        const int value =
            std::clamp(int(SETTINGS.characterSpacing) + (routed.value == readertext::CharacterUp ? 1 : -1), 0, 4);
        changed = value != SETTINGS.characterSpacing;
        SETTINGS.characterSpacing = value;
      } break;
      case readertext::AlignmentDown:
      case readertext::AlignmentUp: {
        const int value =
            std::clamp(int(SETTINGS.paragraphAlignment) + (routed.value == readertext::AlignmentUp ? 1 : -1), 0,
                       int(CrossPointSettings::PARAGRAPH_ALIGNMENT_COUNT) - 1);
        changed = SETTINGS.paragraphAlignment != value;
        SETTINGS.paragraphAlignment = value;
        break;
      }
      case readertext::Paragraphs:
        SETTINGS.extraParagraphSpacing = !SETTINGS.extraParagraphSpacing;
        changed = true;
        break;
      case readertext::Focus:
        SETTINGS.focusReadingEnabled = !SETTINGS.focusReadingEnabled;
        changed = true;
        break;
      case readertext::Hyphenation:
        SETTINGS.hyphenationEnabled = !SETTINGS.hyphenationEnabled;
        changed = true;
        break;
      case readertext::Embedded:
        SETTINGS.embeddedStyle = !SETTINGS.embeddedStyle;
        changed = true;
        break;
      case readertext::Antialiasing:
        SETTINGS.textAntiAliasing = !SETTINGS.textAntiAliasing;
        changed = true;
        break;
      default:
        break;
    }
  }
  if (changed) applyTextSettingLive();
}

void EpubReaderActivity::handleOverlayInput() {
  if (!toolbarUi) return;

  // THE MENU'S TAPS DIE WITHOUT THIS. UiAppHost::routeTouch() refuses to route
  // anything until UiAppHost::revealed() is true, and that turns true only when
  // a paint is COUNTED (paintclock::notePainted()) after the screen's last
  // build. pushOverlayRefresh() takes the deferred path on any board that can
  // defer, and GfxRenderer::displayBufferAsync() deliberately does not count
  // the paint: waitRefreshComplete() does, and nothing here was waiting.
  // settleOverlayRefresh() only reseeds the grayscale baseline, so the counter
  // never moved and every tap on the sheet was dropped before it reached a
  // target -- silently, with no log line, which is what "it stops responding"
  // looked like on the glass. Boards that cannot defer take the blocking
  // displayBuffer(), which counts the paint, which is why the upstream PR that
  // added this menu shipped green on button-only devices. Card #546.
  //
  // The wait costs nothing that was not already spent: the waveform is running
  // whether or not anyone waits, and by the first input tick most of it is
  // behind us.
  if (overlayPaintPending) {
    overlayPaintPending = false;
    RenderLock lock;  // the render task shares the panel
    renderer.waitRefreshComplete();
    // The waveform is done, so there is nothing left for settleOverlayRefresh()
    // to wait out. Leaving this set sends the destructor into
    // cleanupGrayscaleWithFrameBuffer() for a refresh that has already landed,
    // which is a wait on an idle panel taken while the activity is being torn
    // down -- the shape the device hung in on 2026-09-20 with "Exiting
    // activity: EpubReader" as its last word.
    overlayRefreshPending = false;
    if (presentationComplete()) performance::finish(renderedOperation, this);
  }

  // A modal option picker over the panel owns all input while open.
  if (overlayPopup.isActive()) {
    overlayPopup.handleInput(mappedInput, [this] {
      if (overlayPopup.isActive()) {
        paintOverlayPopup();  // highlight moved
        return;
      }
      // Dismissed or selected: erase the dialog -- clean page back, then the
      // panel over it (the dialog can overhang the sheet onto the page).
      RenderLock lock;
      settleOverlayRefresh();
      if (overlayPageStored) {
        if (!renderer.copyStoredBwBuffer()) {
          discardOverlayPage();
          requestUpdate();
          return;
        }
        renderOverlay();
        pushOverlayRefresh();
      } else {
        requestUpdate();
      }
    });
    return;
  }
  const auto fastRedraw = [this] {
    RenderLock lock;  // the render task shares the framebuffer
    settleOverlayRefresh();
    renderOverlay();
    pushOverlayRefresh();
  };

  // Jump to another spine item (chapter scrub). The overlay stays up and is
  // re-drawn over the new page by renderBook().
  const auto gotoSpine = [this](int target) {
    const int spineCount = epub->getSpineItemsCount();
    target = std::clamp(target, 0, spineCount - 1);
    if (target != currentSpineIndex) {
      RenderLock lock;
      clearDeferredReposition();
      nextPageNumber = 0;
      currentSpineIndex = target;
      resetSection();
    }
    requestUpdate();
  };
  const auto toolOverlay = [](int tool) {
    return tool == 0 ? Overlay::Contents : (tool == 1 ? Overlay::Text : Overlay::More);
  };

  // Touch first: FreeInkUI routes the frame against the tap targets the last
  // render registered and hands back the action it mapped to.
  const auto routed = toolbarUi->route(mappedInput);
  if (overlay == Overlay::Text) {
    handleTextSheetInput(routed);
    return;
  }

  // --- Toolbar ---
  if (overlay == Overlay::Toolbar) {
    switch (routed.event) {
      case ReaderToolbarUi::Event::Dismiss:
        closeOverlayToPage();
        return;
      case ReaderToolbarUi::Event::Tool:
        focusedTool = routed.value;
        openOverlay(toolOverlay(focusedTool));
        return;
      case ReaderToolbarUi::Event::PrevChapter:
        gotoSpine(currentSpineIndex - 1);
        return;
      case ReaderToolbarUi::Event::NextChapter:
        gotoSpine(currentSpineIndex + 1);
        return;
      case ReaderToolbarUi::Event::Scrub:
        gotoSpine(static_cast<int>((static_cast<float>(routed.permille) / 1000.0f) *
                                       static_cast<float>(epub->getSpineItemsCount() - 1) +
                                   0.5f));
        return;
      default:
        break;
    }
    if (routed.routed) return;  // a touch frame the chrome consumed (or dead space)

    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      closeOverlayToPage();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      focusedTool = (focusedTool + 2) % 3;
      panelCursorShown = true;
      fastRedraw();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      focusedTool = (focusedTool + 1) % 3;
      panelCursorShown = true;
      fastRedraw();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      openOverlay(toolOverlay(focusedTool));
      return;
    }
    const bool prev = mappedInput.wasReleased(MappedInputManager::Button::Up);
    const bool next = mappedInput.wasReleased(MappedInputManager::Button::Down);
    if (prev || next) {
      gotoSpine(currentSpineIndex + (next ? 1 : -1));
    }
    return;
  }

  // --- Panels (Contents / Text / More) ---
  const int count = overlay == Overlay::Contents ? epub->getTocItemsCount() : static_cast<int>(moreItems.size());
  const int pageRows = std::max(1, toolbarUi->visibleRows());

  // Activate the highlighted row: change a value / jump to a chapter / run an
  // action. Shared by the Confirm button and a row tap.
  const auto activateRow = [this, count] {
    if (panelIndex < 0 || panelIndex >= count) return;
    if (overlay == Overlay::Contents) {
      const auto item = epub->getTocItem(panelIndex);
      if (item.spineIndex != -1) {
        RenderLock lock;
        rememberContentsPosition({currentSpineIndex, static_cast<uint32_t>(chapterPosition().pageIndex),
                                  currentPageVisibleOffset.value_or(UINT32_MAX)});
        clearDeferredReposition();
        currentSpineIndex = item.spineIndex;
        pendingAnchor = item.anchor;
        nextPageNumber = 0;
        resetSection();
      }
      overlay = Overlay::None;
      discardOverlayPage();
      requestUpdate();
    } else if (overlay == Overlay::More) {
      activateMoreRow(panelIndex);
    }
  };

  // Steps up to the toolbar -- the Back button and a tap on the page above
  // the sheet.
  const auto dismissPanel = [this, &fastRedraw] {
    overlay = Overlay::Toolbar;
    // Restore the snapshotted page under the toolbar instead of re-rendering
    // it (2+ refreshes -> one FAST). Re-store right away so another panel
    // round-trip can restore again.
    if (overlayPageStored) {
      {
        RenderLock lock;  // the render task shares the framebuffer
        settleOverlayRefresh();
        // No baseline resync: the glass shows the panel, and erasing it needs
        // the differential to keep diffing against the last pushed frame.
        renderer.restoreBwBuffer(/*resyncPanelBaseline=*/false);
        overlayPageStored = renderer.storeBwBuffer();
      }
      fastRedraw();  // takes its own RenderLock
      return;
    }
    requestUpdate();
  };

  // Pages the list by one screen of rows through the nav (measured page size,
  // no-op at the ends). A shown cursor rides along so the buttons continue
  // from what is visible; on touch boards only the viewport moves.
  const auto pageList = [this, count, pageRows, &fastRedraw](int direction) {
    if (count <= 0) return;
    const bool moved = toolbarUi->nav().scrollBy(direction * pageRows, count);
    if (panelCursorShown) {
      panelIndex = std::clamp(panelIndex + direction * pageRows, 0, count - 1);
      fastRedraw();
      return;
    }
    if (moved) fastRedraw();
  };

  switch (routed.event) {
    case ReaderToolbarUi::Event::Dismiss:
      dismissPanel();
      return;
    case ReaderToolbarUi::Event::Tool: {
      // Sheet-bottom tool switcher: hop straight to another panel.
      const Overlay target = toolOverlay(routed.value);
      if (target != overlay) {
        focusedTool = routed.value;
        openOverlay(target);
      }
      return;
    }
    case ReaderToolbarUi::Event::Row:
      // A tap on the right-edge strip pages the sheet instead (upper half =
      // previous page, lower half = next): swipes are unreliable on etched
      // glass, and a long contents list needs a fast way through.
      if (routed.x >= renderer.getScreenWidth() - 44) {
        pageList(routed.y >= renderer.getScreenHeight() - (renderer.getScreenHeight() * 62) / 200 ? 1 : -1);
        return;
      }
      panelIndex = routed.value;
      panelCursorShown = false;
      activateRow();
      return;
    default:
      break;
  }
  // Swipe up/down pages the list. Checked before the routed-frame return:
  // FUI routes every touch frame over the sheet, so a swipe's frames count as
  // routed (without dispatching -- too much travel for a tap) and the gesture
  // would otherwise never be seen.
  const auto swipe = mappedInput.wasSwipe();
  if (overlay == Overlay::Contents &&
      (swipe == MappedInputManager::SwipeDir::Left || swipe == MappedInputManager::SwipeDir::Right)) {
    pageList(swipe == MappedInputManager::SwipeDir::Left ? 1 : -1);
    return;
  }
  if (swipe == MappedInputManager::SwipeDir::Up || swipe == MappedInputManager::SwipeDir::Down) {
    pageList(swipe == MappedInputManager::SwipeDir::Up ? 1 : -1);
    return;
  }
  if (routed.routed) return;  // consumed by the chrome (title band, dead space)

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    dismissPanel();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activateRow();
    return;
  }

  // Up/Down (side) and Left/Right (front) move the cursor: a tap steps one
  // row, holding past PANEL_HOLD_MS jumps PANEL_HOLD_STEP rows in one go, which
  // is how you cross a hundreds-of-chapters contents list without a press per
  // row. The jump fires once on the hold and swallows the release that ends it,
  // so it never doubles up with the tap step.
  if (count > 0) {
    const bool up = mappedInput.isPressed(MappedInputManager::Button::Up) ||
                    mappedInput.isPressed(MappedInputManager::Button::Left);
    const bool down = mappedInput.isPressed(MappedInputManager::Button::Down) ||
                      mappedInput.isPressed(MappedInputManager::Button::Right);
    if (!panelHoldJumped && (up || down) && mappedInput.getHeldTime() >= PANEL_HOLD_MS) {
      const int step = down ? PANEL_HOLD_STEP : -PANEL_HOLD_STEP;
      panelIndex = std::clamp(panelIndex + step, 0, count - 1);
      panelHoldJumped = true;
      panelCursorShown = true;
      fastRedraw();
      return;
    }

    const bool releasedUp = mappedInput.wasReleased(MappedInputManager::Button::Up) ||
                            mappedInput.wasReleased(MappedInputManager::Button::Left);
    const bool releasedDown = mappedInput.wasReleased(MappedInputManager::Button::Down) ||
                              mappedInput.wasReleased(MappedInputManager::Button::Right);
    if (releasedUp || releasedDown) {
      if (!panelHoldJumped) {
        panelIndex = releasedUp ? ButtonNavigator::previousIndex(panelIndex, count)
                                : ButtonNavigator::nextIndex(panelIndex, count);
        panelCursorShown = true;
        fastRedraw();
      }
      panelHoldJumped = false;
    }
  }
}

// Start each popup paint from the clean page and sheet: a replacement picker
// or its last page can be smaller. Keep the snapshot allocated until dismissal.
void EpubReaderActivity::paintOverlayPopup() {
  RenderLock lock;
  settleOverlayRefresh();
  if (!overlayPageStored || !renderer.copyStoredBwBuffer()) {
    overlayPageStored = false;
    requestUpdate();  // renderBook redraws the page, sheet and active popup.
    return;
  }
  renderOverlay();
  overlayPopup.render(renderer);
  pushOverlayRefresh();
}

void EpubReaderActivity::applyReaderTextSettings() {
  RenderLock lock;
  if (!performance::current()) performance::begin(performance::Operation::Format, this);
  textPreferencesDirty = true;
  textSaveSuppressed = false;
  textFormatPending = false;
  finalPagePresented = false;
  textFormatChangedAt = millis();
  {
    performance::Scope font(performance::Stage::Font);
    sdFontSystem.ensureLoaded(renderer);
  }
  if (section && !formatReflowPending && presentedTextSpec && presentedScreenMargin == SETTINGS.screenMargin &&
      *presentedTextSpec == SETTINGS.readerRenderSpec(buildViewportWidth, buildViewportHeight)) {
    discardOverlayPage();
    return;
  }
  rememberReflowTarget();
  resetSection();
  formatReflowPending = true;
}

void EpubReaderActivity::openBookDictionaries() {
  releasePageReuse();
  static constexpr StrId choices[] = {StrId::STR_DICTIONARY, StrId::STR_WORD_TRANSLATION_DICT};
  overlayPopup.show(StrId::STR_BOOK_DICTIONARIES, choices, 2, 0,
                    [this](int choice) { chooseBookDictionary(choice != 0); });
  paintOverlayPopup();
}

void EpubReaderActivity::chooseBookDictionary(bool translation) {
  // Discovery and the bounded label snapshot belong to this explicit settings
  // action, never to page rendering or lookup hot paths.
  std::vector<DictionaryEntry> entries;
  entries.reserve(32);
  DictionaryRegistry::discover(entries);
  bookDictionaryFolders.clear();
  bookDictionaryFolders.reserve(std::min(size_t(128), entries.size()));
  for (const auto& entry : entries) {
    if (entry.name.size() < 32 && bookDictionaryFolders.size() < 128) bookDictionaryFolders.push_back(entry.name);
  }
  const char* name = translation ? bookDictionaries.translation : bookDictionaries.definition;
  int selected = -1;
  for (size_t i = 0; i < bookDictionaryFolders.size(); ++i)
    if (bookDictionaryFolders[i] == name) selected = int(i);
  if (*name && selected < 0 && bookDictionaryFolders.size() < 128) {
    selected = int(bookDictionaryFolders.size());
    bookDictionaryFolders.emplace_back(name);
  }
  std::vector<std::string> labels;
  labels.reserve(bookDictionaryFolders.size() + 2);
  labels.emplace_back(tr(STR_BOOK_DICTIONARY_DEFAULT));
  labels.emplace_back(tr(STR_NONE_OPT));
  labels.insert(labels.end(), bookDictionaryFolders.begin(), bookDictionaryFolders.end());
  const int current = bookDictionaries.overrides & (translation ? 2 : 1) ? (selected < 0 ? 1 : selected + 2) : 0;
  overlayPopup.show(translation ? StrId::STR_WORD_TRANSLATION_DICT : StrId::STR_DICTIONARY, std::move(labels), current,
                    [this, translation](int choice) {
                      if (choice < 0 || choice >= int(bookDictionaryFolders.size()) + 2) return;
                      auto changed = bookDictionaries;
                      const uint8_t bit = translation ? 2 : 1;
                      char* destination = translation ? changed.translation : changed.definition;
                      if (!choice) {
                        changed.overrides &= ~bit;
                        destination[0] = 0;
                      } else {
                        changed.overrides |= bit;
                        std::snprintf(destination, 32, "%s",
                                      choice == 1 ? "" : bookDictionaryFolders[choice - 2].c_str());
                      }
                      if (!std::memcmp(&changed, &bookDictionaries, sizeof(changed)) ||
                          bookdictionary::save(epub->getPath().c_str(), changed)) {
                        bookDictionaries = changed;
                        requestUpdate();
                      } else {
                        static constexpr StrId close[] = {StrId::STR_OK_BUTTON};
                        overlayPopup.show(StrId::STR_TOOLS_SAVE_FAILED, close, 1, 0, [this](int) { requestUpdate(); });
                        paintOverlayPopup();
                      }
                    });
  paintOverlayPopup();
}

// More links to the in-sheet Style controls and the existing reader actions.
void EpubReaderActivity::buildMoreActions() {
  using MA = EpubReaderMenuActivity::MenuAction;
  moreItems.clear();
  moreItems.reserve(11);
  moreItems.push_back({MA::STYLE_SETTINGS, StrId::STR_STYLE});
  moreItems.push_back({MA::SEARCH, StrId::STR_READER_SEARCH});
  moreItems.push_back({MA::TOGGLE_BOOKMARK,
                       currentPageBookmarked ? StrId::STR_READER_REMOVE_BOOKMARK : StrId::STR_READER_ADD_BOOKMARK});
  moreItems.push_back({MA::BOOKMARKS, StrId::STR_READER_BOOKMARKS_NOTES});
  moreItems.push_back({MA::GO_TO_PERCENT, StrId::STR_READER_GOTO});
  moreItems.push_back({MA::BOOK_STATS, StrId::STR_READER_BOOK_STATS});
  moreItems.push_back({MA::FINISH_BOOK, completionMenuLabelId()});
  moreItems.push_back({MA::BOOK_DICTIONARIES, StrId::STR_BOOK_DICTIONARIES});
  moreItems.push_back({MA::AUTO_PAGE_TURN, StrId::STR_AUTO_TURN_PAGES_PER_MIN});
  if (KOREADER_STORE.hasCredentials()) moreItems.push_back({MA::SYNC, StrId::STR_SYNC_PROGRESS});
  if (bookorbit::Store::get().authenticated()) moreItems.push_back({MA::BOOK_ORBIT_SYNC, StrId::STR_BOOKORBIT_SYNC});
}

std::string EpubReaderActivity::moreRowName(int row) const {
  return row >= 0 && row < static_cast<int>(moreItems.size()) ? I18N.get(moreItems[row].labelId) : "";
}

std::string EpubReaderActivity::moreRowValue(int row) const {
  char value[64]{};
  writeMoreRowValue(row, value, sizeof(value));
  return value;
}

void EpubReaderActivity::writeMoreRowValue(int row, char* out, size_t size) const {
  using MA = EpubReaderMenuActivity::MenuAction;
  static constexpr StrId kOrient[] = {StrId::STR_PORTRAIT, StrId::STR_LANDSCAPE_CW, StrId::STR_ORIENTATION_INVERTED,
                                      StrId::STR_LANDSCAPE_CCW};
  static_assert(std::size(kOrient) == CrossPointSettings::ORIENTATION_COUNT, "orientation labels");
  if (!size) return;
  out[0] = 0;
  if (row < 0 || row >= static_cast<int>(moreItems.size())) return;
  switch (moreItems[row].action) {
    case MA::ROTATE_SCREEN:
      std::snprintf(out, size, "%s", I18N.get(kOrient[SETTINGS.orientation % CrossPointSettings::ORIENTATION_COUNT]));
      return;
    case MA::AUTO_PAGE_TURN:
      if (autoTurnOption == 0 || autoTurnOption >= static_cast<int>(std::size(PAGE_TURN_RATES)))
        std::snprintf(out, size, "%s", tr(STR_STATE_OFF));
      else
        std::snprintf(out, size, "%d", PAGE_TURN_RATES[autoTurnOption]);
      return;
    default:
      return;
  }
}

void EpubReaderActivity::activateMoreRow(int row) {
  using MA = EpubReaderMenuActivity::MenuAction;
  if (row < 0 || row >= static_cast<int>(moreItems.size())) return;
  {
    RenderLock lock;  // several actions launch screens that paint the framebuffer
    settleOverlayRefresh();
  }
  const auto action = moreItems[row].action;
  if (action == MA::BOOK_DICTIONARIES) {
    openBookDictionaries();
    return;
  }
  if (action == MA::STYLE_SETTINGS && overlay == Overlay::Text) {
    RenderLock lock;
    textSheetView = readertext::SheetView::Style;
    textListPage = 0;
    toolbarUi->begin();
    renderOverlay();
    pushOverlayRefresh();
    return;
  }
  if (action == MA::GO_TO_PERCENT) {
    static constexpr StrId choices[] = {StrId::STR_READER_CHAPTER_PAGE, StrId::STR_GO_TO_PERCENT};
    overlayPopup.show(StrId::STR_READER_GOTO, choices, 2, 0, [this](int choice) {
      overlay = Overlay::None;
      discardOverlayPage();
      onReaderMenuConfirm(choice == 0 ? EpubReaderMenuActivity::MenuAction::GO_TO_PAGE
                                      : EpubReaderMenuActivity::MenuAction::GO_TO_PERCENT);
    });
    paintOverlayPopup();
    return;
  }
  if (action == MA::BOOKMARKS) {
    static constexpr StrId choices[] = {StrId::STR_BOOKMARKS, StrId::STR_VIEW_CLIPPINGS};
    overlayPopup.show(StrId::STR_READER_BOOKMARKS_NOTES, choices, 2, 0, [this](int choice) {
      overlay = Overlay::None;
      discardOverlayPage();
      onReaderMenuConfirm(choice == 0 ? EpubReaderMenuActivity::MenuAction::BOOKMARKS
                                      : EpubReaderMenuActivity::MenuAction::VIEW_CLIPPINGS);
    });
    paintOverlayPopup();
    return;
  }

  // In-place toggles keep the panel open and re-render the page beneath it.
  switch (action) {
    case MA::ROTATE_SCREEN: {
      applyOrientation(uint8_t(rotation::readerNext(SETTINGS.orientation, SETTINGS.rotationDirection != 0,
                                                    SETTINGS.readerFourOrientations != 0)));
      discardOverlayPage();
      requestUpdate();
      return;
    }
    case MA::AUTO_PAGE_TURN: {
      std::vector<std::string> labels;
      labels.reserve(std::size(PAGE_TURN_RATES));
      labels.emplace_back(tr(STR_STATE_OFF));
      for (size_t i = 1; i < std::size(PAGE_TURN_RATES); ++i) labels.push_back(std::to_string(PAGE_TURN_RATES[i]));
      overlayPopup.show(StrId::STR_AUTO_TURN_PAGES_PER_MIN, labels, autoTurnOption, [this](int idx) {
        autoTurnOption = idx;
        toggleAutoPageTurn(static_cast<uint8_t>(idx));
        RenderLock lock;
        discardOverlayPage();
        requestUpdate();
      });
      paintOverlayPopup();
      return;
    }
    case MA::NIGHT_MODE:
      SETTINGS.screenInverted = SETTINGS.screenInverted == 0 ? 1 : 0;
      SETTINGS.saveToFile();
      discardOverlayPage();
      requestUpdate();
      return;
    case MA::FRONTLIGHT: {
      const bool lightOn = !Frontlight.isOn();
      Frontlight.setOn(lightOn);
      frontlightschedule::noteManual(frontlightschedule::Power);
      SETTINGS.frontlightOn = lightOn ? 1 : 0;
      SETTINGS.saveToFile();
      {
        RenderLock lock;  // the render task shares the framebuffer
        renderOverlay();
        renderer.displayBuffer(HalDisplay::FAST_REFRESH);
      }
      return;
    }
    default:
      break;
  }
  // Leaf actions open their own screen / perform the action; close the overlay first.
  overlay = Overlay::None;
  if (action == MA::GO_TO_PERCENT && overlayPageStored) {
    // The percent dialog is a popup over the current frame: wipe the toolbar
    // chrome back to the clean page first so the dialog draws over the page,
    // not the sheet. No refresh push â€” the dialog's first frame carries it.
    RenderLock lock;
    settleOverlayRefresh();
    renderer.restoreBwBuffer(/*resyncPanelBaseline=*/false);
    overlayPageStored = false;
  } else {
    discardOverlayPage();
  }
  if (action == MA::TOGGLE_BOOKMARK) {
    // No child activity here to trigger the re-render the list menu relies on:
    // show the same confirmation popup the long-press path does.
    addBookmark();
    showBookmarkMessage = true;
    bookmarkMessageTime = millis();
    requestUpdate();
    return;
  }
  onReaderMenuConfirm(action);
  // Actions that neither open a screen nor leave the reader (a sync with no
  // credentials, say) would otherwise leave the closed panel on screen.
  if (action != MA::GO_HOME && action != MA::DELETE_CACHE) requestUpdate();
}

void EpubReaderActivity::navigateToHref(const std::string& hrefStr, const bool savePosition) {
  std::optional<RenderLock> lock;
  if (!RenderLock::heldByCurrentTask()) lock.emplace();
  if (!epub) return;

  std::string anchor;
  const auto hashPos = hrefStr.find('#');
  if (hashPos != std::string::npos && hashPos + 1 < hrefStr.size()) {
    anchor = hrefStr.substr(hashPos + 1);
  }

  bool sameFile = !hrefStr.empty() && hrefStr[0] == '#';
  int targetSpineIndex = sameFile ? currentSpineIndex : epub->resolveHrefToSpineIndex(hrefStr);

  if (targetSpineIndex < 0) {
    LOG_DBG("ERS", "Could not resolve href: %s", hrefStr.c_str());
    return;
  }

  if (savePosition) {
    // Full: drop the oldest entry so Back always returns from the newest jump.
    if (footnoteDepth == MAX_FOOTNOTE_DEPTH) {
      std::copy(savedPositions + 1, savedPositions + MAX_FOOTNOTE_DEPTH, savedPositions);
      footnoteDepth--;
    }
    // A child screen (menu, footnote list) may have released the section;
    // nextPageNumber then holds the page it was on.
    const int page = section ? section->currentPage : nextPageNumber;
    savedPositions[footnoteDepth] = {currentSpineIndex, page};
    footnoteDepth++;
    LOG_DBG("ERS", "Saved position [%d]: spine %d, page %d", footnoteDepth, currentSpineIndex, page);
  }

  clearDeferredReposition();
  pendingAnchor = std::move(anchor);
  currentSpineIndex = targetSpineIndex;
  nextPageNumber = 0;
  resetSection();
  requestUpdate();
  LOG_DBG("ERS", "Navigated to spine %d for href: %s", targetSpineIndex, hrefStr.c_str());
}

void EpubReaderActivity::saveLinkStack() const {
  // [depth] then depth x (spine u16 LE, page u16 LE)
  uint8_t data[1 + MAX_FOOTNOTE_DEPTH * 4];
  size_t size = 0;
  data[size++] = static_cast<uint8_t>(footnoteDepth);
  for (int i = 0; i < footnoteDepth; i++) {
    const SavedPosition& pos = savedPositions[i];
    data[size++] = pos.spineIndex & 0xFF;
    data[size++] = (pos.spineIndex >> 8) & 0xFF;
    data[size++] = pos.pageNumber & 0xFF;
    data[size++] = (pos.pageNumber >> 8) & 0xFF;
  }
  HalFile f;
  if (!Storage.openFileForWrite("ERS", epub->getCachePath() + "/links.bin", f)) return;
  if (f.write(data, size) != size) LOG_ERR("ERS", "Failed to write link stack");
}

void EpubReaderActivity::loadLinkStack() {
  const std::string path = epub->getCachePath() + "/links.bin";
  if (!Storage.exists(path.c_str())) return;
  {
    HalFile f;
    uint8_t data[1 + MAX_FOOTNOTE_DEPTH * 4];
    if (Storage.openFileForRead("ERS", path, f)) {
      const int size = f.read(data, sizeof(data));
      const int depth = size > 0 ? data[0] : 0;
      if (depth >= 1 && depth <= MAX_FOOTNOTE_DEPTH && size == 1 + depth * 4) {
        const int spineCount = epub->getSpineItemsCount();
        bool valid = true;
        for (int i = 0; i < depth; i++) {
          const uint8_t* p = data + 1 + i * 4;
          savedPositions[i] = {p[0] | (p[1] << 8), p[2] | (p[3] << 8)};
          valid = valid && savedPositions[i].spineIndex < spineCount;
        }
        if (valid) {
          footnoteDepth = depth;
          LOG_DBG("ERS", "Loaded link stack, depth %d", depth);
        }
      }
    }
  }
  // Consumed once: a later exit rewrites it, and an unclean shutdown must not
  // resurrect a stale stack.
  Storage.remove(path.c_str());
}

void EpubReaderActivity::restoreSavedPosition() {
  if (footnoteDepth <= 0) return;
  footnoteDepth--;
  const auto& pos = savedPositions[footnoteDepth];
  LOG_DBG("ERS", "Restoring position [%d]: spine %d, page %d", footnoteDepth, pos.spineIndex, pos.pageNumber);

  {
    RenderLock lock;
    clearDeferredReposition();
    currentSpineIndex = pos.spineIndex;
    nextPageNumber = pos.pageNumber;
    resetSection();
  }
  requestUpdate();
}

void EpubReaderActivity::loadCachedBookmarks() {
  cachedBookmarks.clear();
  if (cachedBookmarks.capacity() < initialBookmarkCacheCapacity) {
    cachedBookmarks.reserve(initialBookmarkCacheCapacity);
  }
  if (!epub) {
    currentPageBookmarked = false;
    return;
  }

  BookmarkFile::load(epub->getPath(), cachedBookmarks);
  updateBookmarkFlag();
}

void EpubReaderActivity::addBookmark() {
  if (!section || !epub) return;
  LOG_DBG("ERS", "Toggle bookmark at spine %d, page %d", currentSpineIndex, section ? section->currentPage : -1);
  int currentPage;
  int pageCount;
  {
    RenderLock lock;
    pageCount = section->estimatedTotalPages();
    currentPage = section->currentPage;
  }

  SavedProgressPosition progress = ProgressMapper::toSavedProgress(epub, getCurrentPosition());
  const ProgressRange pageRange = getPageProgressRange(epub, currentSpineIndex, currentPage, pageCount);

  const size_t bookmarkCountBeforeToggle = cachedBookmarks.size();
  cachedBookmarks.erase(std::remove_if(cachedBookmarks.begin(), cachedBookmarks.end(),
                                       [&](const BookmarkEntry& b) {
                                         return bookmarkMatchesProgress(b, currentSpineIndex, currentPage, pageCount,
                                                                        pageRange);
                                       }),
                        cachedBookmarks.end());
  if (cachedBookmarks.size() != bookmarkCountBeforeToggle) {
    bookmarkRemoved = true;
    currentPageBookmarked = false;
  } else {
    std::string pageText;
    if (currentPage >= 0 && currentPage < pageCount) {
      pageText = section->getTextFromSectionFile();
    }
    BookmarkEntry entry;
    entry.percentage = progress.percentage;
    entry.xpath = progress.xpath;
    entry.summary = BookmarkUtil::sanitizeBookmarkSummary(pageText);
    entry.computedSpineIndex = currentSpineIndex;
    entry.computedChapterPageCount = pageCount;
    entry.computedChapterProgress = currentPage;
    const std::optional<uint32_t> offset =
        currentPageVisibleOffset.has_value() ? currentPageVisibleOffset
        : (currentPage >= 0 && currentPage < section->pageCount)
            ? section->getVisibleTextOffsetForPage(static_cast<uint16_t>(currentPage))
            : std::nullopt;
    if (offset.has_value()) {
      entry.visibleTextOffset = *offset;
      entry.hasVisibleTextOffset = true;
    }
    cachedBookmarks.insert(cachedBookmarks.begin(), entry);
    bookmarkRemoved = false;
    currentPageBookmarked = true;
  }

  if (!BookmarkFile::save(epub->getPath(), cachedBookmarks)) {
    LOG_ERR("ERS", "Failed to save bookmarks");
  }
  requestUpdate();
}

void EpubReaderActivity::updateBookmarkFlag() {
  if (!section || !epub || cachedBookmarks.empty()) {
    currentPageBookmarked = false;
    return;
  }
  const int pageCount = section->estimatedTotalPages();
  const ProgressRange pageRange = getPageProgressRange(epub, currentSpineIndex, section->currentPage, pageCount);
  currentPageBookmarked = std::any_of(cachedBookmarks.begin(), cachedBookmarks.end(), [&](const BookmarkEntry& b) {
    return bookmarkMatchesProgress(b, currentSpineIndex, section->currentPage, pageCount, pageRange);
  });
}

ScreenshotInfo EpubReaderActivity::getScreenshotInfo() const {
  ScreenshotInfo info;
  info.readerType = ScreenshotInfo::ReaderType::Epub;
  if (epub) {
    snprintf(info.title, sizeof(info.title), "%s", epub->getTitle().c_str());
    info.spineIndex = currentSpineIndex;
    if (isAtEndOfBook()) info.progressPercent = 100;
  }
  const auto position = chapterPosition();
  if (position.hasTotal()) {
    info.currentPage = position.displayPage();
    info.totalPages = position.totalPages;
    if (epub && epub->getBookSize() > 0 && !isAtEndOfBook()) info.progressPercent = bookPercentFor(position);
  }
  return info;
}

int EpubReaderActivity::getProgressBasisPoints() const {
  if (isAtEndOfBook()) return 10000;
  const auto position = chapterPosition();
  if (!epub || epub->getBookSize() == 0 || !position.hasTotal()) return getProgressPercent() * 100;
  const int basisPoints =
      static_cast<int>(epub->calculateProgress(currentSpineIndex, position.chapterFraction()) * 10000.0f + 0.5f);
  return std::clamp(basisPoints, 0, 10000);
}

CrossPointPosition EpubReaderActivity::getCurrentPosition() const {
  const int currentPage = section && !formatReflowPending ? section->currentPage : nextPageNumber;
  const int totalPages = section && !formatReflowPending ? section->estimatedTotalPages() : cachedChapterTotalPageCount;
  std::optional<uint16_t> paragraphIndex;
  if (section && !formatReflowPending && currentPage >= 0 && currentPage < section->pageCount) {
    const uint16_t paragraphPage =
        currentPage > 0 ? static_cast<uint16_t>(currentPage - 1) : static_cast<uint16_t>(currentPage);
    if (const auto pIdx = section->getParagraphIndexForPage(paragraphPage)) {
      paragraphIndex = *pIdx;
    }
  }

  CrossPointPosition localPos = {currentSpineIndex, currentPage, totalPages};
  localPos.hasResolvedSpineIndex = true;
  localPos.hasMappedPage = true;
  if (section && !formatReflowPending && currentPage >= 0 && currentPage < section->pageCount) {
    if (const auto offset = section->getVisibleTextOffsetForPage(static_cast<uint16_t>(currentPage))) {
      localPos.visibleTextOffset = *offset;
      localPos.hasVisibleTextOffset = true;
    }
  }
  if (formatReflowPending && cachedVisibleTextOffset) {
    localPos.visibleTextOffset = *cachedVisibleTextOffset;
    localPos.hasVisibleTextOffset = true;
  }
  if (paragraphIndex.has_value()) {
    localPos.paragraphIndex = *paragraphIndex;
    localPos.hasParagraphIndex = true;
  }
  return localPos;
}
