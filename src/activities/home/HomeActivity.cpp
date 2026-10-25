#include "HomeActivity.h"

#include <Bitmap.h>
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
#include "../../apps_local/reading/ReadingStatsActivity.h"
#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "HapticFeedback.h"
#include "MappedInputManager.h"
#include "OpdsServerStore.h"
#include "RecentBooksStore.h"
#include "activities/plugins/PluginCatalogActivity.h"  // anyPluginInstalled()
#include "components/UITheme.h"
#include "fontIds.h"

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
    constexpr int limit = 3;
    for (size_t n = 0; n < books.size() && recentBookCount < limit; ++n) {
      if (!RecentBooksStore::isMissing(books[n])) recentSourceIndices[recentBookCount++] = static_cast<int>(n);
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
    if (RecentBooksStore::isMissing(book)) {
      continue;
    }

    recentBooks.push_back(book);
  }
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
    GUI.drawPopup(renderer, tr(STR_LIBRARY_REBUILDING));
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
    if (!book.coverBmpPath.empty()) continue;
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
      auto xtc = makeUniqueNoThrow<Xtc>(book.path, "/.crosspoint");
      if (!xtc) {
        LOG_ERR("HOME", "OOM: XTC thumbnail path");
        continue;
      }
      book.coverBmpPath = xtc->getThumbBmpPath();
    }
  }
}

void HomeActivity::loadGridCover(RecentBook& book, int height, bool& showingLoading, Rect& popupRect) {
  const bool sizedThumb = book.coverBmpPath.find("[HEIGHT]") != std::string::npos;
  if ((sizedThumb || (!FsHelpers::hasReflowableBookExtension(book.path) && !FsHelpers::hasXtcExtension(book.path))) &&
      !book.coverBmpPath.empty() && Storage.exists(UITheme::getCoverThumbPath(book.coverBmpPath, height).c_str()))
    return;
  // Only one parser lives at a time; EPUB/XTC objects exceed the stack budget.
  if (FsHelpers::hasReflowableBookExtension(book.path)) {
    auto epub = makeUniqueNoThrow<Epub>(book.path, "/.crosspoint");
    if (!epub) {
      LOG_ERR("HOME", "OOM: cover EPUB");
      return;
    }
    book.coverBmpPath = epub->getThumbBmpPath();
    if (Storage.exists(epub->getThumbBmpPath(height).c_str())) return;
    if (!showingLoading) {
      showingLoading = true;
      popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
      GUI.fillPopupProgress(renderer, popupRect, 0);
    }
    if (epub->generateThumbBmpFromSource(height)) {
      return;
    }
  } else if (FsHelpers::hasXtcExtension(book.path)) {
    auto xtc = makeUniqueNoThrow<Xtc>(book.path, "/.crosspoint");
    if (!xtc) {
      LOG_ERR("HOME", "OOM: cover XTC");
      return;
    }
    book.coverBmpPath = xtc->getThumbBmpPath();
    if (Storage.exists(xtc->getThumbBmpPath(height).c_str())) return;
    if (!showingLoading) {
      showingLoading = true;
      popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
      GUI.fillPopupProgress(renderer, popupRect, 0);
    }
    if (xtc->load() && xtc->generateThumbBmp(height)) {
      return;
    }
  }
  book.coverBmpPath.clear();
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
    loadGridCover(book, thumbHeight, showingLoading, popupRect);
    if (!FsHelpers::hasEpubExtension(book.path) && !FsHelpers::hasXtcExtension(book.path))
      pathsChanged |= !hadPath && !book.coverBmpPath.empty();
    ++progress;
    if (showingLoading) GUI.fillPopupProgress(renderer, popupRect, progress * 100 / recentBooks.size());
  }
  const bool changed = showingLoading || pathsChanged;
  if (changed) coverBufferStored = false;
  recentsLoaded = true;
  recentsLoading = false;
  return changed;
}

void HomeActivity::onEnter() {
  Activity::onEnter();
  setEdgeBackEnabled(SETTINGS.homeLayout == CrossPointSettings::HOME_FRAMED);
  homemenu::Store::instance().load();

  hasOpdsServers = OPDS_STORE.hasServers();
  hasPlugins = anyPluginInstalled();

  if (SETTINGS.homeLayout == CrossPointSettings::HOME_FRAMED && UITheme::getInstance().hasCoverGridHome()) {
    // Screen-lifetime interaction tables and component properties exceed the stack budget.
    coverGridUi = makeUniqueNoThrow<CoverGridHomeUi>(renderer);
    if (!coverGridUi) LOG_ERR("HOME", "OOM: cover grid UI; using standard home");
  }
  loadRecentBooks(coverGridUi ? CoverGridHomeUi::MAX_BOOKS : 1);
  if (!coverGridUi) dashboard.begin();
  hasContinueReading = !recentBooks.empty();
  if (coverGridUi) {
    fillCoverGridFromLibrary();
    resolveGridCoverPaths();
    coverGridUi->begin(recentBooks, hasLibrarySlot(), hasContinueReading, hasPlugins);
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
  // Safe from onEnter because replaceActivity defers to the end of the loop.
  shelf::autostartFromEnv(renderer, mappedInput);

  // Trigger first update
  requestUpdate();
}

void HomeActivity::onExit() {
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
  const int options = homemenu::count(hasLibrarySlot());
  const auto move = [this, options](int direction) {
    if (!menuInput.move(direction, options)) LOG_ERR("HOME", "Menu input queue full");
  };
  buttonNavigator.onNext([&move] { move(1); });
  buttonNavigator.onPrevious([&move] { move(-1); });
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) &&
      !menuInput.push({homeui::MenuInput::Kind::Confirm}))
    LOG_ERR("HOME", "Menu confirm queue full");
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) && !menuInput.push({homeui::MenuInput::Kind::Resume}))
    LOG_ERR("HOME", "Menu resume queue full");

  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return;
  const int menuCount = getMenuItemCount();
  if (!coverGridUi && dashboard.pollDate()) {
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
  homeui::MenuInput::Event event;
  while (menuInput.pop(event)) {
    switch (event.kind) {
      case homeui::MenuInput::Kind::Move:
        selectorIndex = homeui::MenuInput::selection(selectorIndex, displayBookCount(), options, event.steps);
        break;
      case homeui::MenuInput::Kind::Confirm:
        menuInput.clear();
        activateSelection();
        return;
      case homeui::MenuInput::Kind::Resume:
        if (hasContinueReading && !recentBooks.empty()) {
          menuInput.clear();
          onSelectBook(recentBooks[0].path);
          return;
        }
        break;
    }
  }
  if (previousSelection != selectorIndex) requestUpdate();

  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Up || swipe == MappedInputManager::SwipeDir::Down) {
    selectorIndex = homeui::MenuInput::selection(selectorIndex, displayBookCount(), options,
                                                 swipe == MappedInputManager::SwipeDir::Up ? 1 : -1);
    if (selectorIndex != previousSelection) requestUpdate();
    return;
  }

  if (coverGridUi) {
    const int touched = coverGridUi->selectedAction(mappedInput);
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

void HomeActivity::render(RenderLock&&) {
  // Another activity/popup may have used the single framebuffer while Home was stacked.
  if (!coverGridUi && firstRenderDone && !dashboardDirty && lastHomePaint == paintclock::counter() &&
      dashboard.canRepaintSelection(selectorIndex)) {
    if (dashboard.repaintSelection(selectorIndex)) {
      renderer.displayBuffer(HalDisplay::FAST_REFRESH);
      lastHomePaint = paintclock::counter();
    }
    return;
  }
  renderer.clearScreen();
  if (coverGridUi) {
    coverGridUi->setSelection(selectorIndex);
    UITheme::getInstance().drawCoverGridHome(*coverGridUi);
    // Physical navigation walks only the menu tabs; covers remain touch targets.
    const auto labels = mappedInput.mapLabels(hasContinueReading ? tr(STR_RESUME) : "", tr(STR_SELECT),
                                              tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer(cleanInitialRefresh && !firstRenderDone ? HalDisplay::HALF_REFRESH
                                                                   : HalDisplay::FAST_REFRESH);
    // Slot heights are recorded during the draw above; a change (first layout
    // pass, orientation switch) means the paths must point at those sizes and
    // any missing thumbs must be generated. Refreshing the paths right away
    // lets the next pass draw already-cached thumbs before generation runs.
    const bool coverSpecChanged = coverGridUi->takeThumbHeightChanged();
    if (coverSpecChanged) {
      recentsLoaded = false;
    }
    firstRenderDone = true;
    if (!recentsLoaded && !recentsLoading) {
      const bool changed = loadRecentCovers(CoverGridHomeUi::THUMB_HEIGHT);
      if (changed) {
        coverGridUi->refreshCoverPaths();
        requestUpdate();
      }
    }
    return;
  }
  const RecentBook* book = recentBooks.empty() ? nullptr : &recentBooks.front();
  const RecentBook* sides[4]{};
  static constexpr int offsets[] = {-2, -1, 1, 2};
  for (int n = 0; n < 4; ++n) {
    if (recentBookCount <= 1 || n == 0 || n == 3) continue;
    const int at = (recentBookIndex + offsets[n] + recentBookCount) % recentBookCount;
    sides[n] = &RECENT_BOOKS.getBooks()[recentSourceIndices[at]];
  }
  const bool geometryChanged =
      dashboard.draw(book, hasLibrarySlot(), hasPlugins, selectorIndex, recentBookIndex, recentBookCount, sides);
  if (geometryChanged) {
    freeCoverBuffer();
    recentsLoaded = false;
  }
  const auto& cover = dashboard.geometry().cover;
  const auto& cache = dashboard.geometry().coverRegion;
  coverRectX = cache.x;
  coverRectY = cache.y;
  coverRectW = cache.width;
  coverRectH = cache.height;
  if (book && !(coverBufferStored && restoreCoverBuffer())) {
    dashboard.paintCover(*book);
    coverBufferStored = storeCoverBuffer();
  }
  dashboard.finishCover();
  renderer.displayBuffer(cleanInitialRefresh && !firstRenderDone ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  firstRenderDone = true;
  dashboardDirty = false;
  lastHomePaint = paintclock::counter();
  if (!recentsLoaded && !recentsLoading) {
    bool changed = loadRecentCovers(cover.height);
    if (SETTINGS.homeLayout != CrossPointSettings::HOME_FRAMED) {
      bool loading = false;
      Rect popup;
      for (int n = 0; n < 4; ++n)
        if (sides[n]) {
          RecentBook side = *sides[n];
          loadGridCover(side, dashboard.geometry().sides[n].height, loading, popup);
        }
      changed |= loading;
    }
    if (changed) {
      coverBufferStored = false;
      dashboardDirty = true;
      requestUpdate();
    }
  }
}

void HomeActivity::onSelectBook(const std::string& path) { activityManager.goToReader(path); }

void HomeActivity::onFileBrowserOpen() { activityManager.goToFileBrowser(); }

void HomeActivity::onLibraryOpen() { activityManager.goToLibrary(); }

void HomeActivity::onSettingsOpen() { activityManager.goToSettings(); }

void HomeActivity::onFileTransferOpen() { activityManager.goToFileTransfer(); }

void HomeActivity::onOpdsBrowserOpen() { activityManager.goToBrowser(); }

void HomeActivity::onPluginsOpen() { activityManager.goToPlugins(hasOpdsServers); }
