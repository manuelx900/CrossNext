#include "HomeActivity.h"

#include <Bitmap.h>
#include <ContentVisibility.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalStorage.h>
#include <I18n.h>
#include <LibraryBuilder.h>
#include <LibraryIndexFile.h>
#include <Memory.h>
#include <PaintClock.h>
#include <Utf8.h>
#include <Xtc.h>

#include <algorithm>
#include <cstring>
#include <vector>

#include "../../apps_local/Shelf.h"  // fork-local seam
#include "../../apps_local/home/CoverSceneCache.h"
#include "../../apps_local/home/HomeShelfOrder.h"
#include "../../apps_local/home/ShelfCatalogCache.h"
#include "../../apps_local/library/LibraryControls.h"
#include "../../apps_local/library/LibraryLoadingNotice.h"
#include "../../apps_local/reading/ReadingStatsActivity.h"
#include "../../apps_local/ui/PixelMenuTheme.h"
#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "HapticFeedback.h"
#include "MappedInputManager.h"
#include "OpdsServerStore.h"
#include "RecentBooksStore.h"
#include "activities/plugins/PluginCatalogActivity.h"  // anyPluginInstalled()
#include "apps_local/home/CoverLoadingNotice.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
homeui::ShelfCatalogCache shelfCatalog;
int shelfSlots(const GfxRenderer& renderer) { return renderer.getScreenWidth() > renderer.getScreenHeight() ? 3 : 6; }
int requestedShelfPages() { return std::clamp(int(SETTINGS.homeRecentBookCount), 1, 10); }
bool hasShelfCatalog(int limit) {
  return shelfCatalog.matches(library::libraryContentRevision(), homecover::sceneRevision(),
                              SETTINGS.libraryUseMetadata != 0, limit);
}

bool availableRecent(const RecentBook& book) {
  if (content::hiddenContent(book.path)) return false;
  if (hasShelfCatalog(0) && shelfCatalog.contains(book.path)) return true;
  const bool present = !RecentBooksStore::isMissing(book);
  if (present && hasShelfCatalog(0))
    shelfCatalog.rememberPresent(library::clixPathHash(book.path.c_str(), book.path.size()), book.path);
  return present;
}
}  // namespace

int HomeActivity::getMenuItemCount() const {
  // The persisted registry defines the same rows for drawing and activation.
  int count = homemenu::count(hasLibrarySlot());
  if (!recentBooks.empty()) {
    count += displayBookCount();
  }
  return count;
}

void HomeActivity::loadRecentBooks(int maxBooks) {
  recentBooks.clear();
  const auto& books = RECENT_BOOKS.getBooks();
  if (!coverGridUi) {
    recentBooks.reserve(1);
    recentBookCount = 0;
    recentBookIndex = 0;
    const int limit = std::clamp(int(SETTINGS.homeRecentBookCount), 1, RecentBooksStore::MAX_RECENT_BOOKS);
    for (size_t n = 0; n < books.size() && recentBookCount < limit; ++n) {
      if (availableRecent(books[n])) recentSourceIndices[recentBookCount++] = static_cast<int>(n);
    }
    if (recentBookCount) recentBooks.push_back(books[recentSourceIndices[0]]);
    return;
  }
  recentBooks.reserve(coverGridUi ? maxBooks : std::min(static_cast<int>(books.size()), maxBooks));

  for (const RecentBook& book : books) {
    // Limit to maximum number of recent books
    if (recentBooks.size() >= maxBooks) {
      break;
    }

    // Skip if file no longer exists
    if (!availableRecent(book)) {
      continue;
    }

    recentBooks.push_back(book);
  }
}

void HomeActivity::loadShelfPage() {
  const auto started = millis();
  const int pageSize = shelfSlots(renderer);
  shelfPageSize = pageSize;
  const int requestedPages = requestedShelfPages();
  const int shelfLimit = requestedPages * pageSize;
  struct Reader {
    library::LibraryIndexFile index;
    library::ClixRecord record;
    homeui::ShelfOrder order;
  };
  auto reader = makeUniqueNoThrow<Reader>();
  if (!reader) {
    LOG_ERR("HOME", "OOM: shelf index reader");
    return;
  }
  const bool dirty = library::isLibraryIndexDirty();
  bool catalogReady = !dirty && hasShelfCatalog(shelfLimit);
  const bool catalogHit = catalogReady;
  bool opened = false;
  if (!catalogReady) {
    opened = reader->index.open(library::libraryIndexPath());
    bool rebuild = !opened || library::isLibraryIndexDirty() ||
                   reader->index.header().metadataEnabled != (SETTINGS.libraryUseMetadata != 0) ||
                   reader->index.header().reserved[0] != content::VISIBILITY_VERSION;
    static bool cardChecked = false;
    if (opened && !rebuild && !cardChecked) {
      const auto result = library::checkLibraryCard(reader->index.header());
      rebuild = result == library::CardCheck::Changed;
      cardChecked = result == library::CardCheck::Unchanged;
    }
    if (rebuild) {
      reader->index.close();
      GUI.drawLoadingScreen(renderer, tr(STR_LIBRARY_INITIAL_INDEX));
      library::BuildStats stats;
      if (library::buildLibraryIndex("/", stats, SETTINGS.libraryUseMetadata != 0)) cardChecked = true;
      opened = reader->index.open(library::libraryIndexPath());
    }
    if (opened && shelfCatalog.begin(library::libraryContentRevision(), homecover::sceneRevision(),
                                     SETTINGS.libraryUseMetadata != 0, shelfLimit)) {
      bool complete = true;
      for (uint16_t row = 0; row < reader->index.bookCount() && row < shelfLimit; ++row) {
        const auto ordinal = reader->index.ordinalForRow(library::SortOrder::DateAddedDesc, row);
        uint64_t hash = 0;
        if (!reader->index.readRecord(ordinal, reader->record) || !reader->index.readPathHash(reader->record, hash) ||
            !shelfCatalog.append(ordinal, hash)) {
          complete = false;
          break;
        }
        if ((row & 7) == 0) delay(0);
      }
      if (complete) {
        shelfCatalog.finish();
        catalogReady = true;
      } else
        shelfCatalog.invalidate();
    } else if (opened) {
      LOG_ERR("HOME", "PSRAM shelf catalog unavailable; using the index directly");
    }
  }
  const auto& recents = RECENT_BOOKS.getBooks();
  for (size_t n = 0; n < recents.size() && reader->order.count < shelfLimit; ++n) {
    const auto& book = recents[n];
    if (availableRecent(book))
      reader->order.add(homeui::ShelfSource::Recent, uint16_t(n),
                        library::clixPathHash(book.path.c_str(), book.path.size()));
  }
  if (catalogReady) {
    for (int row = 0; row < shelfCatalog.count() && reader->order.count < shelfLimit; ++row)
      reader->order.add(homeui::ShelfSource::Index, uint16_t(row), shelfCatalog.hash(row));
  } else if (opened) {
    for (uint16_t row = 0; row < reader->index.bookCount() && reader->order.count < shelfLimit; ++row) {
      const auto ordinal = reader->index.ordinalForRow(library::SortOrder::DateAddedDesc, row);
      uint64_t hash = 0;
      if (!reader->index.readRecord(ordinal, reader->record) || !reader->index.readPathHash(reader->record, hash))
        break;
      reader->order.add(homeui::ShelfSource::Index, ordinal, hash);
    }
  }
  const int count = reader->order.count;
  shelfPages = std::clamp((count + pageSize - 1) / pageSize, 1, requestedPages);
  shelfPage = std::clamp(shelfPage, 0, shelfPages - 1);
  if (recentBooks.size() > 1) recentBooks.resize(1);
  recentBooks.reserve(CoverGridHomeUi::MAX_BOOKS);
  for (int row = shelfPage * pageSize; row < std::min(count, (shelfPage + 1) * pageSize); ++row) {
    const auto& choice = reader->order.entries[row];
    RecentBook book;
    switch (choice.source) {
      case homeui::ShelfSource::Recent:
        book = recents[choice.index];
        break;
      case homeui::ShelfSource::Index:
        if (catalogReady && shelfCatalog.read(choice.index, book.path, book.title, book.author)) break;
        if (!opened) opened = reader->index.open(library::libraryIndexPath());
        if (!opened ||
            !reader->index.readRecord(catalogReady ? shelfCatalog.ordinal(choice.index) : choice.index,
                                      reader->record) ||
            !reader->index.readPath(reader->record, book.path))
          continue;
        if (!reader->index.readTitle(reader->record, book.title)) reader->index.readName(reader->record, book.title);
        reader->index.readAuthor(reader->record, book.author);
        if (reader->index.ioFailed()) continue;
        if (catalogReady) shelfCatalog.store(choice.index, book.path, book.title, book.author);
        break;
    }
    if (recentBooks.empty()) recentBooks.push_back(book);
    recentBooks.push_back(std::move(book));
  }
  LOG_DBG("HOME", "shelf catalog %s, preparation=%lu ms", catalogHit ? "hit" : "miss", millis() - started);
}

void HomeActivity::fillCoverGridFromLibrary() {
  if (recentBooks.size() >= CoverGridHomeUi::MAX_BOOKS) return;
  // Keep the index and record together off the task stack; reuse for every row.
  struct LibraryReader {
    library::LibraryIndexFile index;
    library::ClixRecord record;
  };
  auto reader = makeUniqueNoThrow<LibraryReader>();
  if (!reader) {
    LOG_ERR("HOME", "OOM: library index");
    return;
  }
  auto& index = reader->index;
  auto& record = reader->record;
  if (!index.open(library::libraryIndexPath())) {
    index.close();
    GUI.drawLoadingScreen(renderer, tr(STR_LIBRARY_REBUILDING));
    library::BuildStats stats;
    if (!library::buildLibraryIndex("/", stats, SETTINGS.libraryUseMetadata != 0) ||
        !index.open(library::libraryIndexPath())) {
      LOG_ERR("HOME", "Cannot populate cover grid from library");
      return;
    }
  }
  for (uint16_t row = 0; row < index.bookCount() && recentBooks.size() < CoverGridHomeUi::MAX_BOOKS; ++row) {
    RecentBook book;
    if (!index.readRecord(index.ordinalForRow(library::SortOrder::RecentDesc, row), record) ||
        !index.readPath(record, book.path))
      continue;
    if (std::any_of(recentBooks.begin(), recentBooks.end(),
                    [&](const RecentBook& existing) { return existing.path == book.path; }) ||
        RecentBooksStore::isMissing(book))
      continue;
    if (!index.readTitle(record, book.title) && !index.readName(record, book.title)) continue;
    index.readAuthor(record, book.author);
    if (index.ioFailed()) break;
    recentBooks.push_back(std::move(book));
  }
}

void HomeActivity::resolveGridCoverPaths() {
  for (auto& book : recentBooks) {
    if (!book.coverBmpPath.empty() && !FsHelpers::hasXtcExtension(book.path)) continue;
    // Constructors only derive cache paths; no metadata parsing or image generation.
    // Keep these large objects off the task stack and release each before the next book.
    if (FsHelpers::hasReflowableBookExtension(book.path)) {
      auto epub = makeUniqueNoThrow<Epub>(book.path, "/.crosspoint");
      if (!epub) {
        LOG_ERR("HOME", "OOM: EPUB thumbnail path");
        continue;
      }
      book.coverBmpPath = epub->getThumbBmpPath();
    } else if (FsHelpers::hasXtcExtension(book.path)) {
      auto xtc = makeUniqueNoThrow<Xtc>(book.path, "/.crosspoint", SETTINGS.xtcCoverRotation != 0);
      if (!xtc) {
        LOG_ERR("HOME", "OOM: XTC thumbnail path");
        continue;
      }
      book.coverBmpPath = xtc->getThumbBmpPath();
    }
  }
}

bool HomeActivity::loadGridCover(RecentBook& book, int height, bool& showingLoading, Rect& popupRect) {
  const bool sizedThumb = book.coverBmpPath.find("[HEIGHT]") != std::string::npos;
  // XTC paths must first be resolved against the current rotation preference.
  if (!FsHelpers::hasXtcExtension(book.path) && (sizedThumb || !FsHelpers::hasReflowableBookExtension(book.path)) &&
      !book.coverBmpPath.empty() && Storage.exists(UITheme::getCoverThumbPath(book.coverBmpPath, height).c_str()))
    return false;
  // Only one parser lives at a time; EPUB/XTC objects exceed the stack budget.
  if (FsHelpers::hasReflowableBookExtension(book.path)) {
    auto epub = makeUniqueNoThrow<Epub>(book.path, "/.crosspoint");
    if (!epub) {
      LOG_ERR("HOME", "OOM: cover EPUB");
      return false;
    }
    book.coverBmpPath = epub->getThumbBmpPath();
    if (Storage.exists(epub->getThumbBmpPath(height).c_str())) return false;
    if (epub->knownMissingCover()) return false;
    if (!showingLoading && SETTINGS.homeCoverNotice(SETTINGS.homeLayout)) {
      showingLoading = true;
      if (!coverGridUi) dashboard.finishCover();
      popupRect = GUI.drawLoadingScreen(renderer, tr(STR_LOADING_POPUP));
      GUI.fillPopupProgress(renderer, popupRect, 0);
    }
    bool generated = false;
    if (epub->generateThumbBmpFromSource(height, &generated)) return generated;
  } else if (FsHelpers::hasXtcExtension(book.path)) {
    auto xtc = makeUniqueNoThrow<Xtc>(book.path, "/.crosspoint", SETTINGS.xtcCoverRotation != 0);
    if (!xtc) {
      LOG_ERR("HOME", "OOM: cover XTC");
      return false;
    }
    book.coverBmpPath = xtc->getThumbBmpPath();
    if (Storage.exists(xtc->getThumbBmpPath(height).c_str())) return false;
    if (!showingLoading && SETTINGS.homeCoverNotice(SETTINGS.homeLayout)) {
      showingLoading = true;
      if (!coverGridUi) dashboard.finishCover();
      popupRect = GUI.drawLoadingScreen(renderer, tr(STR_LOADING_POPUP));
      GUI.fillPopupProgress(renderer, popupRect, 0);
    }
    if (xtc->load() && xtc->generateThumbBmp(height)) return true;
  }
  book.coverBmpPath.clear();
  return false;
}

bool HomeActivity::loadRecentCovers(int coverHeight) {
  bool pathsChanged = false;
  recentsLoading = true;
  bool showingLoading = false;
  Rect popupRect;

  int progress = 0;
  for (RecentBook& book : recentBooks) {
    // The cover grid shares one slot size; generating at any other height
    // would rescale the dithered thumb at draw time and alias badly.
    const int thumbHeight = coverGridUi ? coverGridUi->thumbHeightFor() : coverHeight;
    const bool hadPath = !book.coverBmpPath.empty();
    pathsChanged |= loadGridCover(book, thumbHeight, showingLoading, popupRect);
    if (!FsHelpers::hasEpubExtension(book.path) && !FsHelpers::hasXtcExtension(book.path))
      pathsChanged |= !hadPath && !book.coverBmpPath.empty();
    ++progress;
    if (showingLoading && SETTINGS.homeCoverNotice(SETTINGS.homeLayout))
      GUI.fillPopupProgress(renderer, popupRect, progress * 100 / recentBooks.size());
  }
  const bool changed = showingLoading || pathsChanged;
  if (changed) coverBufferStored = false;
  recentsLoaded = true;
  recentsLoading = false;
  return changed;
}

void HomeActivity::onEnter() {
  Activity::onEnter();
  libraryWarmInputAt = millis();
  libraryWarmMediaRevision = Storage.mediaRevision();
  libraryWarmRevision = library::libraryContentRevision();
  libraryWarmArtRevision = homecover::sceneRevision();
  libraryWarmOrientation = static_cast<uint8_t>(renderer.getOrientation());
  configureLibraryWarmup();
  setEdgeBackEnabled(SETTINGS.homeLayout == CrossPointSettings::HOME_FRAMED);
  homemenu::Store::instance().load();

  hasOpdsServers = OPDS_STORE.hasServers();
  hasPlugins = SETTINGS.pluginsEnabled;

  if ((SETTINGS.homeLayout == CrossPointSettings::HOME_FRAMED ||
       SETTINGS.homeLayout == CrossPointSettings::HOME_LICORICE) &&
      UITheme::getInstance().hasCoverGridHome()) {
    // Screen-lifetime interaction tables and component properties exceed the stack budget.
    coverGridUi = makeUniqueNoThrow<CoverGridHomeUi>(renderer);
    if (!coverGridUi) LOG_ERR("HOME", "OOM: cover grid UI; using standard home");
  }
  dashboard.begin();
  shelf::autostartFromEnv(renderer, mappedInput);
  const bool warmShelf = coverGridUi && SETTINGS.homeLayout == CrossPointSettings::HOME_LICORICE &&
                         hasShelfCatalog(shelfSlots(renderer) * requestedShelfPages()) &&
                         !library::isLibraryIndexDirty();
  if (SETTINGS.homeCoverNotice(SETTINGS.homeLayout) && !warmShelf) {
    if (coverGridUi) coverGridUi->begin(recentBooks, hasLibrarySlot(), false, hasPlugins);
    entryPhase = EntryPhase::Shell;
  } else {
    initializeContent();
    entryPhase = EntryPhase::Painting;
  }
  requestUpdate();
}

void HomeActivity::initializeContent() {
  loadRecentBooks(coverGridUi && SETTINGS.homeLayout != CrossPointSettings::HOME_LICORICE ? CoverGridHomeUi::MAX_BOOKS
                                                                                          : 1);
  hasContinueReading = !recentBooks.empty();
  if (coverGridUi) {
    if (SETTINGS.homeLayout == CrossPointSettings::HOME_LICORICE)
      loadShelfPage();
    else
      fillCoverGridFromLibrary();
    resolveGridCoverPaths();
    coverGridUi->begin(recentBooks, hasLibrarySlot(), hasContinueReading, hasPlugins);
    coverGridUi->setPage(shelfPage, shelfPages);
  }

  const auto base = displayBookCount();
  selectorIndex =
      initialMenuItem == HomeMenuItem::NONE ? base : base + menuItemToIndex(initialMenuItem, hasLibrarySlot());

  // fork-local seam: goHome() restores the selection by matching the departing
  // activity's name against HomeMenuItem, which cannot know about shelf rows,
  // so leaving GAMES would otherwise drop the cursor on Browse Files. Only when
  // the shelf was what we came back from: see shelf::takeHomeReturn().
  if (shelf::takeHomeReturn()) {
    if (const int shelfRow = shelf::lastFolderOnHome(); shelfRow >= 0) {
      selectorIndex =
          base + menuItemToIndex(shelfRow == 0 ? HomeMenuItem::GAMES : HomeMenuItem::APPS, hasLibrarySlot());
    }
  }

  // fork-local seam: boot straight into a named app when the environment asks
  // for one (the site's installer preview, CROSSPLAY_AUTOSTART=chess ./bin/sim).
  // Fires once per process; on hardware getenv finds nothing and this is free.
  // Only main-loop entry may route the simulator autostart.

  // Trigger first update
  requestUpdate();
}

void HomeActivity::onExit() {
  libraryWarmInterrupted = true;
  libraryWarmFile.close();
  libraryWarmup.release();
  Activity::onExit();

  coverGridUi.reset();

  // Free the stored cover buffer if any
  freeCoverBuffer();
}

bool HomeActivity::storeCoverBuffer() {
  // render() must have already set the cover rect; without it we'd be back to
  // cloning the whole framebuffer.
  if (coverRectW <= 0 || coverRectH <= 0) return false;
  freeCoverBuffer();
  const size_t needed = renderer.getRegionByteSize(coverRectX, coverRectY, coverRectW, coverRectH);
  if (needed == 0) return false;
  coverBuffer = makeUniqueNoThrow<uint8_t[]>(needed);
  if (!coverBuffer) {
    LOG_ERR("HOME", "OOM: cover buffer (%u bytes)", (unsigned)needed);
    return false;
  }
  coverBufferSize = needed;
  if (!renderer.copyRegionToBuffer(coverRectX, coverRectY, coverRectW, coverRectH, coverBuffer.get(),
                                   coverBufferSize)) {
    coverBuffer.reset();
    coverBufferSize = 0;
    return false;
  }
  return true;
}

bool HomeActivity::restoreCoverBuffer() {
  if (!coverBuffer || coverRectW <= 0 || coverRectH <= 0) return false;
  return renderer.copyBufferToRegion(coverRectX, coverRectY, coverRectW, coverRectH, coverBuffer.get(),
                                     coverBufferSize);
}

void HomeActivity::freeCoverBuffer() {
  if (coverBuffer) {
    coverBuffer.reset();
  }
  coverBufferSize = 0;
  coverBufferStored = false;
}

void HomeActivity::loop() {
  if (entryPhase != EntryPhase::Ready) {
    if (entryPhase == EntryPhase::Preparing) activityManager.requestIdleWork(*this);
    return;
  }
  int warmX = 0, warmY = 0;
  bool warmInput = mappedInput.wasSwipe() != MappedInputManager::SwipeDir::None ||
                   mappedInput.wasScreenTapped(warmX, warmY) || mappedInput.wasScreenTouchPressed(warmX, warmY) ||
                   mappedInput.isScreenTouchHeld(warmX, warmY) || mappedInput.wasScreenTouchReleased();
  for (const auto key :
       {MappedInputManager::Button::Back, MappedInputManager::Button::Confirm, MappedInputManager::Button::NavNext,
        MappedInputManager::Button::NavPrevious, MappedInputManager::Button::Left, MappedInputManager::Button::Right})
    warmInput |= mappedInput.isPressed(key) || mappedInput.wasPressed(key) || mappedInput.wasReleased(key);
  if (warmInput) {
    libraryWarmInterrupted = true;
    libraryWarmInputAt = millis();
  }
  const int options = homemenu::count(hasLibrarySlot());
  const auto move = [this, options](int direction) {
    if (!menuInput.move(direction, options)) LOG_ERR("HOME", "Menu input queue full");
  };
  buttonNavigator.onNextPress([&move] { move(1); });
  buttonNavigator.onPreviousPress([&move] { move(-1); });
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) &&
      !menuInput.push({homeui::MenuInput::Kind::Confirm}))
    LOG_ERR("HOME", "Menu confirm queue full");
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) && !menuInput.push({homeui::MenuInput::Kind::Resume}))
    LOG_ERR("HOME", "Menu resume queue full");

  // Keep this input frame until the cancelled idle job releases the renderer.
  RenderLock lock(warmInput ? RenderLock::Mode::Blocking : RenderLock::Mode::Try);
  if (!lock.ownsLock()) return;
  if (warmInput) libraryWarmup.cancel();
  if (!libraryWarmup.enabled()) libraryWarmIndex = 9;
  if (libraryWarmRevision != library::libraryContentRevision() ||
      libraryWarmArtRevision != homecover::sceneRevision() ||
      libraryWarmOrientation != static_cast<uint8_t>(renderer.getOrientation()) ||
      libraryWarmMediaRevision != Storage.mediaRevision()) {
    libraryWarmup.cancel();
    libraryWarmIndex = 0;
    shelfNativeIndex = 0;
    libraryWarmMediaRevision = Storage.mediaRevision();
    libraryWarmRevision = library::libraryContentRevision();
    libraryWarmArtRevision = homecover::sceneRevision();
    libraryWarmOrientation = static_cast<uint8_t>(renderer.getOrientation());
    configureLibraryWarmup();
  }
  const bool shelfWork = coverGridUi && (recentsLoading || nativeShelfPending());
  if (!warmInput && (shelfWork || (libraryWarmIndex < 9 && libraryWarmup.enabled())) &&
      millis() - libraryWarmInputAt >= 180) {
    libraryWarmInterrupted = false;
    activityManager.requestIdleWork(*this);
  }
  if (coverGridUi && SETTINGS.homeLayout == CrossPointSettings::HOME_LICORICE &&
      shelfPageSize != shelfSlots(renderer)) {
    loadShelfPage();
    resolveGridCoverPaths();
    coverGridUi->begin(recentBooks, hasLibrarySlot(), hasContinueReading, hasPlugins);
    coverGridUi->setPage(shelfPage, shelfPages);
    recentsLoaded = recentsLoading = false;
    gridPreparationIndex = shelfNativeIndex = 0;
    requestUpdate();
    return;
  }
  const int menuCount = getMenuItemCount();
  if (dashboard.pollDate()) {
    dashboardDirty = true;
    requestUpdate();
  }

  auto activateSelection = [this] {
    if (selectorIndex < displayBookCount()) {
      onSelectBook(recentBooks[selectorIndex].path);
      return;
    }
    const int menuIndex = selectorIndex - displayBookCount();
    const auto id = homemenu::at(homemenu::config(), menuIndex, hasLibrarySlot());
    if (homemenu::isShortcut(id)) {
      homemenu::launchShortcut(id, renderer, mappedInput);
      return;
    }
    switch (indexToMenuItem(menuIndex, hasLibrarySlot())) {
      case HomeMenuItem::FILE_BROWSER:
        onFileBrowserOpen();
        break;
      case HomeMenuItem::LIBRARY:
        onLibraryOpen();
        break;
      case HomeMenuItem::OPDS_BROWSER:  // the library slot
        hasPlugins ? onPluginsOpen() : onOpdsBrowserOpen();
        break;
      case HomeMenuItem::FILE_TRANSFER:
        onFileTransferOpen();
        break;
      case HomeMenuItem::SETTINGS_MENU:
        onSettingsOpen();
        break;
      case HomeMenuItem::READING_STATS: {
        auto activity = makeUniqueNoThrow<ReadingStatsActivity>(renderer, mappedInput);
        if (activity)
          activityManager.replaceActivity(std::move(activity));
        else
          LOG_ERR("HOME", "OOM: reading statistics");
        break;
      }
      case HomeMenuItem::GAMES:
        shelf::openFolder(0, renderer, mappedInput);
        break;
      case HomeMenuItem::APPS:
        shelf::openFolder(1, renderer, mappedInput);
        break;
      case HomeMenuItem::NONE: {
        break;
      }
    }
  };

  const int previousSelection = selectorIndex;
  const bool previousCursorVisible = menuCursorVisible;
  homeui::MenuInput::Event event;
  while (menuInput.pop(event)) {
    switch (event.kind) {
      case homeui::MenuInput::Kind::Move:
        selectorIndex = homeui::MenuInput::selection(
            SETTINGS.homeLayout == CrossPointSettings::HOME_BOOX && !menuCursorVisible ? displayBookCount()
                                                                                       : selectorIndex,
            displayBookCount(), options,
            SETTINGS.homeLayout == CrossPointSettings::HOME_BOOX && !menuCursorVisible
                ? (event.steps > 0 ? event.steps - 1 : event.steps)
                : event.steps);
        menuCursorVisible = true;
        if (coverGridUi && SETTINGS.homeLayout == CrossPointSettings::HOME_LICORICE) coverGridUi->showDrawer();
        break;
      case homeui::MenuInput::Kind::Confirm:
        if (SETTINGS.homeLayout == CrossPointSettings::HOME_BOOX && !menuCursorVisible) {
          menuCursorVisible = true;
          selectorIndex = displayBookCount();
          requestUpdate();
          break;
        }
        menuInput.clear();
        activateSelection();
        return;
      case homeui::MenuInput::Kind::Resume:
        if (coverGridUi && SETTINGS.homeLayout == CrossPointSettings::HOME_LICORICE && coverGridUi->closeDrawer()) {
          requestUpdate();
          return;
        }
        if (hasContinueReading && !recentBooks.empty()) {
          menuInput.clear();
          onSelectBook(recentBooks[0].path);
          return;
        }
        break;
    }
  }
  if (previousSelection != selectorIndex ||
      (SETTINGS.homeLayout == CrossPointSettings::HOME_BOOX && previousCursorVisible != menuCursorVisible))
    requestUpdate();

  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Up || swipe == MappedInputManager::SwipeDir::Down) {
    selectorIndex = homeui::MenuInput::selection(selectorIndex, displayBookCount(), options,
                                                 swipe == MappedInputManager::SwipeDir::Up ? 1 : -1);
    if (selectorIndex != previousSelection) requestUpdate();
    return;
  }

  if (coverGridUi) {
    const int touched = coverGridUi->selectedAction(mappedInput);
    if (SETTINGS.homeLayout == CrossPointSettings::HOME_LICORICE) {
      const int direction = swipe == MappedInputManager::SwipeDir::Left || touched == CoverGridHomeUi::PAGE_NEXT ? 1
                            : swipe == MappedInputManager::SwipeDir::Right || touched == CoverGridHomeUi::PAGE_PREVIOUS
                                ? -1
                                : 0;
      if (direction && shelfPage + direction >= 0 && shelfPage + direction < shelfPages) {
        shelfPage += direction;
        loadShelfPage();
        resolveGridCoverPaths();
        coverGridUi->begin(recentBooks, hasLibrarySlot(), hasContinueReading, hasPlugins);
        coverGridUi->setPage(shelfPage, shelfPages);
        recentsLoaded = recentsLoading = false;
        gridPreparationIndex = shelfNativeIndex = 0;
        requestUpdate();
        return;
      }
      if (touched == CoverGridHomeUi::MENU_TOGGLE) {
        requestUpdate();
        return;
      }
    }

    if (touched >= 0 && touched < menuCount) {
      selectorIndex = touched;
      activateSelection();
      return;
    }
    return;
  }

  const bool carouselNext = swipe == MappedInputManager::SwipeDir::Left;
  const bool carouselPrevious = swipe == MappedInputManager::SwipeDir::Right;
  const int touched = carouselNext       ? homeui::kNextBook
                      : carouselPrevious ? homeui::kPreviousBook
                                         : dashboard.selectedAction(mappedInput);
  if (touched == homeui::kPreviousBook || touched == homeui::kNextBook) {
    const int next =
        recentBookCount > 1
            ? (recentBookIndex + (touched == homeui::kNextBook ? 1 : recentBookCount - 1)) % recentBookCount
            : -1;
    if (next >= 0 && next < recentBookCount) {
      recentBookIndex = next;
      recentBooks[0] = RECENT_BOOKS.getBooks()[recentSourceIndices[next]];
      freeCoverBuffer();
      recentsLoaded = false;
      dashboardDirty = true;
      requestUpdate();
    }
    return;
  }
  if (touched >= 0 && touched < menuCount) {
    selectorIndex = touched;
    activateSelection();
    return;
  }
}

bool HomeActivity::prepareGridCover(RecentBook& book, int height) {
  if (FsHelpers::hasReflowableBookExtension(book.path)) {
    auto epub = makeUniqueNoThrow<Epub>(book.path, "/.crosspoint");
    if (!epub) {
      LOG_ERR("HOME", "OOM: cover preparation");
      return false;
    }
    book.coverBmpPath = epub->getThumbBmpPath();
    bool generated = false;
    if (epub->generateThumbBmpFromSource(height, &generated)) return generated;
  } else if (FsHelpers::hasXtcExtension(book.path)) {
    auto xtc = makeUniqueNoThrow<Xtc>(book.path, "/.crosspoint", SETTINGS.xtcCoverRotation != 0);
    if (!xtc) {
      LOG_ERR("HOME", "OOM: XTC cover preparation");
      return false;
    }
    book.coverBmpPath = xtc->getThumbBmpPath();
    if (Storage.exists(xtc->getThumbBmpPath(height).c_str())) return false;
    if (xtc->load() && xtc->generateThumbBmp(height)) return true;
  } else
    return false;
  return false;
}

void HomeActivity::configureLibraryWarmup() {
  auto target = toybox::makeTarget(renderer);
  const auto grid = libraryui::preparationGrid(target.deviceContext(), false);
  libraryWarmArea =
      grid.card(0).inset(freeink::ui::Insets{pixelui::kBorder, pixelui::kBorder, pixelui::kBorder, pixelui::kBorder});
  libraryWarmFallbackHeight =
      libraryui::preparationCoverHeight(target.deviceContext(), false, libraryui::kFooterHeight);
}

bool HomeActivity::continueLibraryWarmup(void* context) {
  auto& self = *static_cast<HomeActivity*>(context);
  return !self.libraryWarmInterrupted && activityManager.idleWorkIsCurrent(self) &&
         self.libraryWarmRevision == library::libraryContentRevision() &&
         self.libraryWarmArtRevision == homecover::sceneRevision() &&
         self.libraryWarmMediaRevision == Storage.mediaRevision();
}

void HomeActivity::runIdleWork() {
  if (entryPhase == EntryPhase::Preparing) {
    initializeContent();
    entryPhase = EntryPhase::Painting;
    requestUpdate();
    return;
  }
  if (!continueLibraryWarmup(this)) {
    libraryWarmup.cancel();
    return;
  }
  if (coverGridUi && recentsLoading) {
    if (gridPreparationIndex < recentBooks.size()) {
      auto& book = recentBooks[gridPreparationIndex++];
      if (prepareGridCover(book, coverGridUi->thumbHeightFor())) {
        // Bookshelf can show the current book in both the grid and the reading strip.
        for (size_t i = 0; i < recentBooks.size(); ++i) {
          auto& slot = recentBooks[i];
          if (slot.path != book.path) continue;
          if (slot.coverBmpPath != book.coverBmpPath) slot.coverBmpPath = book.coverBmpPath;
          coverGridUi->refreshCoverPath(i);
        }
        gridArtworkChanged = true;
      }
    }
    if (gridPreparationIndex >= recentBooks.size()) {
      recentsLoading = false;
      recentsLoaded = true;
      if (gridArtworkChanged || coverGridUi->coverNoticeVisible()) requestUpdate();
      gridArtworkChanged = false;
    }
    return;
  }
  if (nativeShelfPending()) {
    if (!libraryWarmup.enabled()) {
      shelfNativeIndex = recentBooks.size();
      return;
    }
    if (libraryWarmup.writing()) {
      libraryWarmup.step();
      if (!libraryWarmup.writing()) ++shelfNativeIndex;
      return;
    }
    const auto area = coverGridUi->coverArea(shelfNativeIndex);
    const auto& book = recentBooks[shelfNativeIndex];
    const auto thumb = UITheme::getCoverThumbPath(book.coverBmpPath, coverGridUi->thumbHeightFor());
    if (area.width > 0 && area.height > 0 && Storage.openFileForRead("HOME", thumb, libraryWarmFile)) {
      libraryui::PreparedCoverCache::Info info;
      const auto key = libraryui::PreparedCoverCache::key(renderer, area);
      const auto bytes = renderer.getPackedRegionByteSize(area.x, area.y, area.width, area.height);
      if (!libraryui::PreparedCoverCache::inspect(libraryWarmFile, key, bytes, info))
        libraryWarmup.prepare(renderer, libraryWarmBitmap, area, key, thumb.c_str(),
                              &HomeActivity::continueLibraryWarmup, this);
      libraryWarmFile.close();
    }
    if (!libraryWarmup.writing()) ++shelfNativeIndex;
    return;
  }
  const auto advance = [this] {
    if (++libraryWarmIndex >= 9) libraryWarmup.release();
  };
  if (libraryWarmup.writing()) {
    libraryWarmup.step();
    if (!libraryWarmup.writing()) advance();
    return;
  }
  const auto& books = RECENT_BOOKS.getBooks();
  if (libraryWarmIndex >=
      std::min<size_t>(libraryui::gridCapacity(renderer.getScreenWidth(), renderer.getScreenHeight()), books.size())) {
    libraryWarmIndex = 9;
    libraryWarmup.release();
    return;
  }
  const auto& path = books[libraryWarmIndex.load()].path;
  if (!FsHelpers::hasEpubExtension(path) && !FsHelpers::hasXtcExtension(path)) {
    advance();
    return;
  }
  const auto area = libraryWarmArea;
  const int heights[] = {area.height, libraryWarmFallbackHeight};
  for (const int height : heights) {
    std::snprintf(libraryWarmPath, sizeof(libraryWarmPath), "/.crosspoint/%s_%lu/thumb_%s%d.bmp",
                  FsHelpers::hasXtcExtension(path) ? "xtc" : "epub",
                  static_cast<unsigned long>(std::hash<std::string>{}(path)),
                  FsHelpers::hasXtcExtension(path) && SETTINGS.xtcCoverRotation ? "ccw3_" : "", height);
    if (!Storage.openFileForRead("HOME", libraryWarmPath, libraryWarmFile)) continue;
    libraryui::PreparedCoverCache::Info info;
    const auto key = libraryui::PreparedCoverCache::key(renderer, area);
    const auto bytes = renderer.getPackedRegionByteSize(area.x, area.y, area.width, area.height);
    if (!libraryui::PreparedCoverCache::inspect(libraryWarmFile, key, bytes, info))
      libraryWarmup.prepare(renderer, libraryWarmBitmap, area, key, libraryWarmPath,
                            &HomeActivity::continueLibraryWarmup, this);
    libraryWarmFile.close();
    break;
  }
  if (!libraryWarmup.writing()) advance();
}

void HomeActivity::render(RenderLock&&) {
  if (entryPhase == EntryPhase::Blocking) return;
  if (entryPhase == EntryPhase::Shell || entryPhase == EntryPhase::Preparing) {
    renderer.clearScreen();
    if (coverGridUi) {
      coverGridUi->setLoadingShell(true);
      UITheme::getInstance().drawCoverGridHome(*coverGridUi);
      coverGridUi->setLoadingShell(false);
    } else {
      const int count = std::clamp(int(SETTINGS.homeRecentBookCount), 1, RecentBooksStore::MAX_RECENT_BOOKS);
      dashboard.draw(nullptr, hasLibrarySlot(), hasPlugins, -1, 0, count, nullptr, true);
      dashboard.paintPlaceholders();
    }
    auto target = toybox::makeTarget(renderer);
    const auto noticeArea = homeui::coverLoadingNoticeArea(
        target.deviceContext().safeRect(), coverGridUi ? coverGridUi->coverArea(0) : dashboard.geometry().cover,
        SETTINGS.homeLayout == CrossPointSettings::HOME_FRAMED);
    libraryui::loadingNotice(target, noticeArea, tr(STR_LIBRARY_LOADING_COVERS));
    renderer.displayBuffer(cleanInitialRefresh ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
    firstRenderDone = true;
    entryPhase = EntryPhase::Preparing;
    return;
  }
  libraryWarmup.cancel();
  // Another activity/popup may have used the single framebuffer while Home was stacked.
  if (!coverGridUi && firstRenderDone && !dashboardDirty && lastHomePaint == paintclock::counter() &&
      dashboard.canRepaintSelection(paintedSelection())) {
    if (dashboard.repaintSelection(paintedSelection())) {
      renderer.displayBuffer(HalDisplay::FAST_REFRESH);
      lastHomePaint = paintclock::counter();
    }
    return;
  }
  renderer.clearScreen();
  if (coverGridUi) {
    coverGridUi->prepareGeometry();
    if (coverGridUi->takeThumbHeightChanged()) {
      recentsLoaded = recentsLoading = false;
      gridPreparationIndex = shelfNativeIndex = 0;
    }
    coverGridUi->setSelection(selectorIndex);
    const bool showNotice = SETTINGS.homeCoverNotice(SETTINGS.homeLayout);
    coverGridUi->setCoverLoading(!recentsLoaded && showNotice);
    UITheme::getInstance().drawCoverGridHome(*coverGridUi);
    if (coverGridUi->coversReady()) {
      recentsLoaded = true;
      recentsLoading = false;
    } else if (!recentsLoaded && !recentsLoading) {
      recentsLoading = true;
      gridPreparationIndex = 0;
      gridArtworkChanged = false;
    }
    // Physical navigation walks only the menu tabs; covers remain touch targets.
    const auto labels = mappedInput.mapLabels(hasContinueReading ? tr(STR_RESUME) : "", tr(STR_SELECT),
                                              tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer(cleanInitialRefresh && !firstRenderDone ? HalDisplay::HALF_REFRESH
                                                                   : HalDisplay::FAST_REFRESH);
    firstRenderDone = true;
    entryPhase = EntryPhase::Ready;
    return;
  }
  const RecentBook* book = recentBooks.empty() ? nullptr : &recentBooks.front();
  const RecentBook* sides[4]{};
  static constexpr int offsets[] = {-2, -1, 1, 2};
  for (int n = 0; n < 4; ++n) {
    if (recentBookCount <= 1 || n == 0 || n == 3 || (recentBookCount == 2 && n == 1)) continue;
    const int at = (recentBookIndex + offsets[n] + recentBookCount) % recentBookCount;
    sides[n] = &RECENT_BOOKS.getBooks()[recentSourceIndices[at]];
  }
  const bool geometryChanged =
      dashboard.draw(book, hasLibrarySlot(), hasPlugins, paintedSelection(), recentBookIndex, recentBookCount, sides);
  if (geometryChanged) {
    freeCoverBuffer();
    recentsLoaded = false;
  }
  bool coverPainted = false;
  if (book && !recentsLoaded && !recentsLoading && dashboard.restoreCover(*book)) {
    coverPainted = true;
    recentsLoaded = true;
  }
  if (!recentsLoaded && !recentsLoading) {
    const bool showNotice = SETTINGS.homeCoverNotice(SETTINGS.homeLayout);
    if (book && showNotice) {
      dashboard.paintCover(*book);
      coverPainted = true;
    }
    bool changed = loadRecentCovers(dashboard.geometry().cover.height);
    if (SETTINGS.homeLayout != CrossPointSettings::HOME_FRAMED) {
      bool loading = false;
      Rect popup;
      for (int n = 0; n < 4; ++n)
        if (sides[n]) {
          RecentBook side = *sides[n];
          changed |= loadGridCover(side, dashboard.geometry().sides[n].height, loading, popup);
        }
      changed |= loading;
    }
    if (changed) {
      homecover::invalidateScenes();
      coverPainted = false;
      coverBufferStored = false;
      if (showNotice) {
        renderer.clearScreen();
        dashboard.draw(book, hasLibrarySlot(), hasPlugins, paintedSelection(), recentBookIndex, recentBookCount, sides);
      }
    }
  }
  const auto& cover = dashboard.geometry().cover;
  const auto& cache = dashboard.geometry().coverRegion;
  coverRectX = cache.x;
  coverRectY = cache.y;
  coverRectW = cache.width;
  coverRectH = cache.height;
  if (book && (coverPainted || !(coverBufferStored && restoreCoverBuffer()))) {
    if (!coverPainted) dashboard.paintCover(*book);
    coverBufferStored = storeCoverBuffer();
  }
  dashboard.finishCover();
  renderer.displayBuffer(cleanInitialRefresh && !firstRenderDone ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  firstRenderDone = true;
  dashboardDirty = false;
  lastHomePaint = paintclock::counter();
  entryPhase = EntryPhase::Ready;
}

void HomeActivity::onSelectBook(const std::string& path) { activityManager.goToReader(path); }

void HomeActivity::onFileBrowserOpen() { activityManager.goToFileBrowser(); }

void HomeActivity::onLibraryOpen() { activityManager.goToLibrary(); }

void HomeActivity::onSettingsOpen() { activityManager.goToSettings(); }

void HomeActivity::onFileTransferOpen() { activityManager.goToFileTransfer(); }

void HomeActivity::onOpdsBrowserOpen() { activityManager.goToBrowser(); }

void HomeActivity::onPluginsOpen() { activityManager.goToPlugins(hasOpdsServers); }
