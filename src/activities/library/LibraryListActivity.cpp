#include "LibraryListActivity.h"

#include <ContentVisibility.h>
#include <Epub.h>
#include <FreeInkUIIcon.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <LibraryBuilder.h>
#include <LibraryText.h>
#include <Logging.h>
#include <Memory.h>
#include <PaintClock.h>
#include <Utf8.h>
#include <Xtc.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "activities/util/ConfirmationActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "apps_local/library/LibraryFinishedBooks.h"
#include "apps_local/ui/PageIndicator.h"
#include "apps_local/ui/PixelMenuTheme.h"
#include "components/UIScale.h"
#include "components/UITheme.h"
#include "components/icons/headerIcons.h"
#include "components/icons/listIcons.h"
#include "components/icons/search32.h"
#include "fontIds.h"
#include "util/BookCacheUtils.h"

namespace fui = freeink::ui;

namespace {
constexpr int SIDE_PADDING = 12;
constexpr unsigned long LONG_PRESS_MS = 1000;
constexpr unsigned long COVER_IDLE_MS = 250;

constexpr int RECENT_TAB = 0;
constexpr bool isDescending(library::SortOrder order) {
  return order == library::SortOrder::RecentDesc || order == library::SortOrder::TitleDesc ||
         order == library::SortOrder::AuthorDesc || order == library::SortOrder::AuthorFirstDesc ||
         order == library::SortOrder::SeriesDesc || order == library::SortOrder::GenreDesc ||
         order == library::SortOrder::DateAddedDesc;
}
constexpr bool isRecentSort(library::SortOrder order) {
  return order == library::SortOrder::RecentAsc || order == library::SortOrder::RecentDesc;
}
constexpr bool isFacetSort(library::SortOrder order) {
  return order == library::SortOrder::SeriesAsc || order == library::SortOrder::SeriesDesc ||
         order == library::SortOrder::GenreAsc || order == library::SortOrder::GenreDesc;
}
constexpr bool isAuthorSort(library::SortOrder order) {
  return order == library::SortOrder::AuthorAsc || order == library::SortOrder::AuthorDesc ||
         order == library::SortOrder::AuthorFirstAsc || order == library::SortOrder::AuthorFirstDesc;
}
constexpr bool isGroupedOrder(library::SortOrder order) {
  return isAuthorSort(order) || isFacetSort(order) || order == library::SortOrder::TitleAsc ||
         order == library::SortOrder::TitleDesc;
}

}  // namespace

LibraryListActivity::LibraryListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiTabListActivity("Library", renderer, mappedInput, true) {
  setInterfaceFontSection(menufont::Section::Library);
}

void LibraryListActivity::onEnter() {
  // One lock across the base lifecycle AND the data phase: the base onEnter
  // schedules a paint, and the render task must not read the index or the
  // filter before they are in place. The rebuild also needs the lock: the
  // render task's SD-loaded fonts read glyph data at draw time, and the walk
  // needs the card to itself.
  RenderLock lock(*this);
  activeTabIndex = 0;
  descendingTabs = SETTINGS.librarySortDirections & 1u;
  sortOrder = descendingTabs & 1 ? library::SortOrder::RecentDesc : library::SortOrder::RecentAsc;
  UiTabListActivity::onEnter();
  app.on(ACTION_SEARCH, &LibraryListActivity::searchActionTrampoline, this);
  app.on(ACTION_REBUILD, &LibraryListActivity::rebuildActionTrampoline, this);
  app.on(ACTION_BACK, &LibraryListActivity::backActionTrampoline, this);
  app.on(
      ACTION_VIEW,
      [](const fui::ActionEvent& event, void* context) {
        auto* self = static_cast<LibraryListActivity*>(context);
        if (event.value == libraryui::kPickSort) {
          self->openSortPicker();
          return;
        }
        if (event.value == libraryui::kReverseSort) {
          self->toggleSortDirection();
          return;
        }
        if (event.value == -3) {
          self->openFilterMenu();
          return;
        }
        if (event.value < 0) {
          RenderLock lock(*self);
          auto& nav = self->activeNav();
          nav.requestScroll((event.value == -1 ? -1 : 1) * nav.inputPageRows());
          self->requestUpdate();
          return;
        }
        if (event.value == 0) {
          self->toggleView();
          return;
        }
        const int target = (event.value - 1) * self->gridPageSize();
        if (target >= 0 && target < self->listCount()) self->moveRingTo(target + 1);
      },
      this);
  ensureGridBuffer();
  pendingCover.reserve(520);
  gridPath.reserve(520);
  gridFileName.reserve(256);

  // Recent is backed by the resident store. Prune before opening the index so
  // its persistence write never overlaps the long-lived index reader.
  if (RECENT_BOOKS.pruneMissing()) RECENT_BOOKS.saveToFile();

  // Rebuild when the index is missing, invalid, or was built with the other
  // metadata mode. Otherwise entering the screen stays instant.
  const bool readMetadata = SETTINGS.libraryUseMetadata != 0;
  const bool rebuildNeeded = library::isLibraryIndexDirty() || !index.open(library::libraryIndexPath()) ||
                             index.header().metadataEnabled != readMetadata ||
                             index.header().reserved[0] != content::VISIBILITY_VERSION;
  if (rebuildNeeded) {
    index.close();
    GUI.drawPopup(renderer, tr(STR_LIBRARY_REBUILDING));
    rebuildIndex();
    if (!index.open(library::libraryIndexPath())) LOG_ERR("LIB", "cannot open library index");
  }
  degraded = index.isOpen() && index.ranksDegraded();
  if (index.isOpen() && index.dedupDegraded()) {
    LOG_ERR("LIB", "index was built without duplicate detection");
  }
  resolvePinned();
  applyFilter();

  // Entered while Confirm was still held (typical when launched from the home
  // menu): ignore its release, or we would open whatever sits at row 0.
  lockNextConfirmRelease = mappedInput.isPressed(MappedInputManager::Button::Confirm);
  requestUpdate(true);
}

void LibraryListActivity::onExit() {
  index.close();
  facetChoices.reset();
  gridCoverFile.close();
  Activity::onExit();
  gridCoverBitmap.setReadAheadBuffer(nullptr, 0);
  gridCache.reset();
}

bool LibraryListActivity::rebuildIndex() {
  library::BuildStats stats;
  const bool ok = library::buildLibraryIndex("/", stats, SETTINGS.libraryUseMetadata != 0);
  if (!ok) {
    LOG_ERR("LIB", "index build failed");
    return false;
  }
  LOG_INF("LIB", "reconciled: %u unchanged, %u added, %u renamed, %u removed, %u enriched (%u dup, %u unreadable)",
          static_cast<unsigned>(stats.unchanged), static_cast<unsigned>(stats.added),
          static_cast<unsigned>(stats.renamed), static_cast<unsigned>(stats.removed),
          static_cast<unsigned>(stats.enriched), static_cast<unsigned>(stats.duplicatesDropped),
          static_cast<unsigned>(stats.unreadableSkipped));
  if (stats.dedupDegraded) LOG_ERR("LIB", "rebuild completed without duplicate detection");
  return true;
}

void LibraryListActivity::swallowHeldReleases() {
  lockNextConfirmRelease = mappedInput.isPressed(MappedInputManager::Button::Confirm);
  lockNextBackRelease = mappedInput.isPressed(MappedInputManager::Button::Back);
}

int LibraryListActivity::selectedEntry() const {
  const int entry = ringPos() - 1;
  return entry < 0 ? 0 : entry;
}

// Recently opened retains store rows even when the card index omitted them.
int LibraryListActivity::pinnedCount() const {
  if (activeTabIndex != RECENT_TAB || resultMapped()) return 0;
  return pinnedTotal;
}

int LibraryListActivity::storeIndexFor(int entry) const {
  if (resultMapped()) {
    if (entry < 0 || entry >= filteredCount || !filtered) return -1;
    return (filtered[entry] & 0x8000) ? filtered[entry] & 0x7fff : -1;
  }
  const int pinned = pinnedCount();
  if (!pinned || entry < 0) return -1;
  const int fullCount = pinned;
  const int position = sortOrder == library::SortOrder::RecentAsc ? fullCount - 1 - entry : entry;
  return position >= 0 && position < pinned ? position : -1;
}

void LibraryListActivity::resolvePinned() {
  const auto& books = RECENT_BOOKS.getBooks();
  pinnedTotal = static_cast<uint8_t>(std::min<size_t>(books.size(), RecentBooksStore::MAX_RECENT_BOOKS));
  for (int i = 0; i < pinnedTotal; i++) pinnedAscRows[i] = 0xFFFF;
  if (pinnedTotal > 0 && index.isOpen()) {
    library::BookIdentity identities[RecentBooksStore::MAX_RECENT_BOOKS];
    for (int i = 0; i < pinnedTotal; i++) {
      const std::string& path = books[static_cast<size_t>(i)].path;
      identities[i].pathHash = library::clixPathHash(path.data(), path.size());
      // Size is only a lookup prefilter; 0 (stat failed, e.g. the index handle
      // is the card's one open reader) falls back to hash-only matching.
      identities[i].fileSize = 0;
      HalFile file;
      if (Storage.openFileForRead("LIB", path.c_str(), file)) {
        identities[i].fileSize = static_cast<uint32_t>(file.fileSize());
      }
    }
    if (!index.recentRowsFor(identities, pinnedTotal, pinnedAscRows)) {
      // Without the match the overlay would duplicate every pinned book that is
      // also in the index; better to drop the pins than to show doubles.
      LOG_ERR("LIB", "recent-book lookup failed; overlay disabled");
      for (int n = 0; n < pinnedTotal; ++n) pinnedAscRows[n] = 0xffff;
    }
  }
  refreshOverlap();
}

void LibraryListActivity::refreshOverlap() {
  windowTop = -1;
  invalidateGrid();
  overlapCount = 0;
  const int total = static_cast<int>(index.bookCount());
  for (int i = 0; i < pinnedTotal; i++) {
    if (pinnedAscRows[i] == 0xFFFF || pinnedAscRows[i] >= total) continue;
    const uint16_t row =
        isDescending(sortOrder) ? static_cast<uint16_t>(total - 1 - pinnedAscRows[i]) : pinnedAscRows[i];
    overlapRows[overlapCount++] = row;
  }
  std::sort(overlapRows, overlapRows + overlapCount);
}

void LibraryListActivity::openSelectedBook() {
  RenderLock lock;
  std::string path;
  const int store = storeIndexFor(selectedEntry());
  if (store >= 0) {
    const auto& books = RECENT_BOOKS.getBooks();
    if (store >= static_cast<int>(books.size())) return;
    path = books[static_cast<size_t>(store)].path;
  } else {
    if (!index.isOpen()) return;
    const uint16_t ordinal = index.ordinalForRow(sortOrder, static_cast<uint16_t>(rowFor(selectedEntry())));
    if (ordinal == 0xFFFF) return;

    library::ClixRecord record{};
    if (!index.readRecord(ordinal, record) || !index.readPath(record, path)) {
      LOG_ERR("LIB", "cannot resolve path for row %d", selectedEntry());
      return;
    }
  }
  lock.unlock();
  openBookByPath(path);
}

// Shared by row activation and the options menu: the reader screen this opens
// has its own surfaces; a lingering tap flash would gray an unrelated element
// there. The index handle is released first — on hardware only one reader can
// hold a file open at a time, and the reader is about to open files of its own.
void LibraryListActivity::openBookByPath(const std::string& path) {
  RenderLock lock;
  app.clearTapFlash();
  index.close();
  onSelectBook(path);
}

void LibraryListActivity::activateIndex(const int index) {
  if (view == View::Categories) {
    static constexpr library::Facet facets[] = {library::Facet::Genre, library::Facet::Series,
                                                library::Facet::AuthorFirst, library::Facet::Author,
                                                library::Facet::TitleInitial};
    if (index >= 0 && index < 5) openFacetPicker(facets[descendingTabs & 2 ? 4 - index : index]);
    return;
  }
  if (view == View::Facets) {
    chooseFacet(index);
    return;
  }
  if (view == View::Settings) {
    activatePreference(index);
    return;
  }
  if (groupsCollapsed) {
    expandGroup(index);
  } else {
    openSelectedBook();
  }
}

// Row long-press prompts delete wherever grouping does not own the gesture:
// an active search is already a flat list the reader narrowed down on purpose
// ("find it, hold it, delete it"). Unfiltered Title/Author lists keep
// collapse-to-groups. The Recent shelf always opens the row options menu.
bool LibraryListActivity::deleteEligible() const { return !groupsCollapsed && (filtering() || !groupable()); }

void LibraryListActivity::onRowLongPress(const int index) {
  if (view != View::Books) return;
  if (isRecentSort(sortOrder)) {
    showRecentBookOptions(index);
  } else if (deleteEligible()) {
    promptDeleteBook(index);
  } else if (!groupsCollapsed && groupable()) {
    collapseGroups(index);
  } else {
    activateIndex(index);
  }
}

// Recently opened retains its Open, Remove from recents, Delete and Refresh actions.
void LibraryListActivity::showRecentBookOptions(const int entry) {
  RenderLock lock;
  if (entry < 0 || entry >= listCount()) return;

  std::string path;
  std::string title;
  const int store = storeIndexFor(entry);
  const bool isStoreRow = store >= 0;
  if (isStoreRow) {
    const auto& books = RECENT_BOOKS.getBooks();
    if (store >= static_cast<int>(books.size())) return;
    path = books[static_cast<size_t>(store)].path;
    title = books[static_cast<size_t>(store)].title;
  } else {
    if (!index.isOpen()) return;
    const uint16_t ordinal = index.ordinalForRow(sortOrder, static_cast<uint16_t>(rowFor(entry)));
    library::ClixRecord record{};
    std::string author;
    if (ordinal == 0xFFFF || !index.readRecord(ordinal, record) || !index.readPath(record, path) ||
        !rowTextFor(entry, title, author)) {
      LOG_ERR("LIB", "cannot resolve Recent row %d", entry);
      return;
    }
  }

  const char* STORE_OPTIONS[] = {tr(STR_OPEN), tr(STR_REMOVE_FROM_RECENTS), tr(STR_DELETE), tr(STR_LIBRARY_REBUILD)};
  const char* INDEX_OPTIONS[] = {tr(STR_OPEN), tr(STR_DELETE), tr(STR_LIBRARY_REBUILD)};
  app.clearTapFlash();
  optionPopup.show(tr(STR_LIBRARY), title.c_str(), isStoreRow ? STORE_OPTIONS : INDEX_OPTIONS, isStoreRow ? 4 : 3, 0,
                   [this, path, title, isStoreRow](const int choice) {
                     swallowHeldReleases();
                     switch (choice) {
                       case 0:
                         openBookByPath(path);
                         break;
                       case 1:
                         if (isStoreRow) {
                           promptRemoveRecentBook(path, title);
                         } else {
                           promptDeleteBookByPath(path, title);
                         }
                         break;
                       case 2:
                         if (isStoreRow)
                           promptDeleteBookByPath(path, title);
                         else
                           promptRebuildIndex();
                         break;
                       case 3:
                         if (isStoreRow) promptRebuildIndex();
                         break;
                       default:
                         break;
                     }
                   });
  requestUpdate();
}

// Manual index refresh, same card discipline as the onEnter rebuild: the walk
// wants the card to itself, and the render task must not read the index (or
// the filter) around it.
void LibraryListActivity::promptRebuildIndex() {
  RenderLock lock(*this);
  GUI.drawPopup(renderer, tr(STR_LIBRARY_REBUILDING));
  index.close();
  rebuildIndex();
  if (!index.open(library::libraryIndexPath())) LOG_ERR("LIB", "cannot open library index");
  resetAfterRebuild();
  requestUpdate();
}

void LibraryListActivity::resetAfterRebuild() {
  degraded = index.isOpen() && index.ranksDegraded();
  // Sort positions, group starts, and pinned rows all point into the old order.
  resolvePinned();
  if (view == View::Facets) {
    view = View::Categories;
    facetChoices.reset();
    preferencesNav.reset();
  }
  applyFilter();
  auto& nav = activeNav();
  const int count = listCount();
  if (count == 0) {
    nav.selected = 0;
  } else if (nav.selected > count) {
    nav.selected = count;
  }
  nav.followOnBuild = true;
}

void LibraryListActivity::promptRemoveRecentBook(const std::string& path, const std::string& title) {
  RenderLock lock;
  const bool reopenIndex = index.isOpen();
  index.close();
  auto confirmation =
      makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, tr(STR_REMOVE_FROM_RECENTS), title);
  if (!confirmation) {
    LOG_ERR("LIB", "OOM: recent removal confirmation");
    if (reopenIndex && !index.open(library::libraryIndexPath())) LOG_ERR("LIB", "cannot reopen library index");
    return;
  }

  startActivityForResult(std::move(confirmation), [this, path, reopenIndex](const ActivityResult& result) {
    swallowHeldReleases();
    RenderLock resultLock;
    if (reopenIndex && !index.open(library::libraryIndexPath())) LOG_ERR("LIB", "cannot reopen library index");
    if (!result.isCancelled && RECENT_BOOKS.removeByPath(path)) {
      resolvePinned();
      applyFilter();
      closeRouting();
      auto& nav = activeNav();
      const int count = listCount();
      if (count == 0) {
        nav.selected = 0;
      } else if (nav.selected > count) {
        nav.selected = count;
      }
      nav.followOnBuild = true;
    }
  });
}

void LibraryListActivity::promptDeleteBook(const int entry) {
  RenderLock lock;
  if (!index.isOpen() || entry < 0 || entry >= bookRowCount()) return;
  const uint16_t ordinal = index.ordinalForRow(sortOrder, static_cast<uint16_t>(rowFor(entry)));
  if (ordinal == 0xFFFF) return;

  std::string path;
  library::ClixRecord record{};
  if (!index.readRecord(ordinal, record) || !index.readPath(record, path)) {
    LOG_ERR("LIB", "cannot resolve path for row %d", entry);
    return;
  }
  std::string title;
  std::string author;
  rowTextFor(entry, title, author);
  lock.unlock();
  promptDeleteBookByPath(path, title);
}

void LibraryListActivity::promptDeleteBookByPath(const std::string& path, const std::string& title) {
  RenderLock lock;
  // The dialog and the delete both want the card; reopen when we resume.
  index.close();
  auto confirmation =
      makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, tr(STR_DELETE) + std::string("? "), title);
  if (!confirmation) {
    LOG_ERR("LIB", "OOM: delete confirmation");
    if (!index.open(library::libraryIndexPath())) LOG_ERR("LIB", "cannot reopen library index");
    return;
  }

  startActivityForResult(std::move(confirmation), [this, path](const ActivityResult& result) {
    swallowHeldReleases();
    {
      // Same lock rationale as onEnter: the walk wants the card to itself, and
      // the render task must not read the index (or the filter) around the
      // rebuild.
      RenderLock lock(*this);
      if (!result.isCancelled) {
        LOG_DBG("LIB", "deleting %s", path.c_str());
        if (!removeBookFile(path)) LOG_ERR("LIB", "cannot delete %s", path.c_str());
        if (RECENT_BOOKS.removeByPath(path)) RECENT_BOOKS.saveToFile();
        GUI.drawPopup(renderer, tr(STR_LIBRARY_REBUILDING));
        rebuildIndex();
      }
      if (!index.open(library::libraryIndexPath())) LOG_ERR("LIB", "cannot reopen library index");
      if (!result.isCancelled) {
        resetAfterRebuild();
      }
    }
    if (!result.isCancelled) {
      closeRouting();
      requestUpdate();
    }
  });
}

void LibraryListActivity::openSearch() {
  app.clearTapFlash();
  // No key filtering here on purpose. Greying out the letters that lead nowhere
  // was built, tested on device and removed: a letter you can see but cannot
  // reach reads as a broken keyboard, and the eye keeps returning to it.
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_LIBRARY_SEARCH), query, 48,
                                                           InputType::Text);
  if (!keyboard) {
    LOG_ERR("LIB", "OOM: search keyboard");
    return;
  }
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    swallowHeldReleases();
    if (result.isCancelled) return;
    RenderLock lock;
    query = std::get<KeyboardResult>(result.data).text;
    applyFilter();
    auto& nav = activeNav();
    if (filtering() && filteredCount == 0 && !degraded) {
      // Up from the tab bar reopens Search even with no results.
      nav.selected = 0;
    } else {
      // A non-empty result belongs to the list: land on
      // its first surviving row, not on the strip.
      nav.selected = 1;
    }
    nav.top = 0;
    requestUpdate();
  });
}

libraryui::Preferences LibraryListActivity::preferences() const {
  return {SETTINGS.libraryUseMetadata != 0, expandedListSetting() != 0, recentViewSetting() == 1,
          SETTINGS.libraryDisplay, SETTINGS.libraryFileTypes};
}
fui::ListNav& LibraryListActivity::activeNav() {
  return view != View::Books ? preferencesNav : UiTabListActivity::activeNav();
}
void LibraryListActivity::onRowAction(const fui::ActionEvent& event) {
  if (view != View::Books)
    UiListActivity::onRowAction(event);
  else
    UiTabListActivity::onRowAction(event);
}
void LibraryListActivity::savePreferences(libraryui::Preferences value) {
  RenderLock lock;
  const auto before = preferences();
  if (before == value) return;
  auto assign = [this](libraryui::Preferences p) {
    SETTINGS.libraryUseMetadata = p.metadata;
    expandedListSetting() = p.expanded;
    recentViewSetting() = p.grid;
    SETTINGS.libraryDisplay = p.display;
    SETTINGS.libraryFileTypes = p.formats;
  };
  assign(value);
  if (!SETTINGS.saveToFile()) {
    assign(before);
    const char* options[] = {tr(STR_OK_BUTTON)};
    optionPopup.showMessage(tr(STR_LIBRARY_SETTINGS), tr(STR_LIBRARY_SAVE_FAILED), options, 1, 0,
                            [this](int) { requestUpdate(); });
  } else
    preferencesChanged = true;
  requestUpdate();
}
void LibraryListActivity::showPreferences() {
  RenderLock lock;
  closeRouting();
  settingsReturn = view == View::Facets ? View::Categories : view;
  view = View::Settings;
  preferencesNav.reset();
  resetUi();
  requestUpdate();
}
void LibraryListActivity::closePreferences() {
  RenderLock lock;
  closeRouting();
  view = settingsReturn;
  if (preferencesChanged) {
    GUI.drawPopup(renderer, tr(STR_LOADING));
    if (!index.isOpen() || index.header().metadataEnabled != (SETTINGS.libraryUseMetadata != 0)) {
      index.close();
      const bool rebuilt = rebuildIndex();
      const bool opened = index.open(library::libraryIndexPath());
      if (!rebuilt || !opened) {
        LOG_ERR("LIB", "Cannot update library after metadata change");
        view = View::Settings;
        const char* options[] = {tr(STR_OK_BUTTON)};
        optionPopup.showMessage(tr(STR_LIBRARY_SETTINGS), tr(STR_LIBRARY_REFRESH_FAILED), options, 1, 0,
                                [this](int) { requestUpdate(); });
        requestUpdate();
        return;
      }
      degraded = index.isOpen() && index.ranksDegraded();
      resolvePinned();
    }
    applyFilter();
    activeNav().reset();
    ensureGridBuffer();
    windowTop = -1;
    preferencesChanged = false;
  }
  resetUi();
  requestUpdate();
}
void LibraryListActivity::activatePreference(int row) {
  if (row < 0 || row >= int(libraryui::Preference::Count)) return;
  app.clearTapFlash();
  auto value = preferences();
  const auto preference = static_cast<libraryui::Preference>(row);
  switch (preference) {
    case libraryui::Preference::Metadata:
      value.metadata = !value.metadata;
      break;
    case libraryui::Preference::ListView:
    case libraryui::Preference::RecentView: {
      const bool list = preference == libraryui::Preference::ListView;
      const char* names[] = {list ? tr(STR_LIBRARY_COMPACT) : tr(STR_LIBRARY_LIST),
                             list ? tr(STR_LIBRARY_EXPANDED) : tr(STR_LIBRARY_GRID_VIEW)};
      optionPopup.show(list ? tr(STR_LIBRARY_LIST_VIEW) : tr(STR_LIBRARY_RECENT_VIEW), names, 2,
                       list ? value.expanded : value.grid, [this, list](int choice) {
                         auto next = preferences();
                         if (list)
                           next.expanded = choice == 1;
                         else
                           next.grid = choice == 1;
                         savePreferences(next);
                       });
      requestUpdate();
      return;
    }
    case libraryui::Preference::Finished:
      value.display ^= libraryui::kFinished;
      break;
    case libraryui::Preference::Series:
      value.display ^= libraryui::kSeries;
      break;
    case libraryui::Preference::Genre:
      value.display ^= libraryui::kGenre;
      break;
    case libraryui::Preference::Epub:
      value.formats ^= 1;
      break;
    case libraryui::Preference::Xtc:
      value.formats ^= 2;
      break;
    case libraryui::Preference::Text:
      value.formats ^= 4;
      break;
    case libraryui::Preference::Markdown:
      value.formats ^= 8;
      break;
    case libraryui::Preference::Count:
      return;
  }
  savePreferences(value);
}
void LibraryListActivity::buildPreferences(UiScreen& screen) {
  pixelui::FontScope fonts(uiTarget, renderer, I18N.getLanguage() != Language::EN);
  pixelui::Header heading;
  heading.title = tr(STR_LIBRARY_SETTINGS);
  heading.back = mappedInput.hasTouch();
  pixelui::header(screen.target(), screen.device(), heading);
  backui::publish(heading.back, pixelui::geometry(screen.device()).back);
  pixelui::begin(screen, mappedInput.hasTouch() ? 0 : UITheme::getInstance().getMetrics().buttonHintsHeight);
  fui::ListProps props;
  libraryui::preferenceRows(props, preferenceItems, preferences());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  fui::list(screen.frame(), screen.body(), props);
}

void LibraryListActivity::stepTab(const int direction) {
  selectTab((activeTabIndex + (direction > 0 ? 1 : -1) + 2) % 2, false);
}

void LibraryListActivity::onTabAction(const int index) {
  app.clearTapFlash();
  selectTab(index, true);
}

void LibraryListActivity::selectTab(const int index, const bool) {
  if (index < 0 || index > 1) return;
  if (index == 1) {
    showCategories();
    return;
  }
  RenderLock lock;
  closeRouting();
  view = View::Books;
  activeTabIndex = 0;
  sortOrder = descendingTabs & 1 ? library::SortOrder::RecentDesc : library::SortOrder::RecentAsc;
  facetChoices.reset();
  facetFilter = {};
  query.clear();
  headerSearchTitle.clear();
  applyFilter();
  activeNav().reset();
  invalidateGrid();
  resetUi();
  requestUpdate();
}

void LibraryListActivity::openSortPicker() { selectTab(activeTab(), true); }

void LibraryListActivity::openFilterMenu() {
  app.clearTapFlash();
  const char* names[] = {tr(STR_LIBRARY_SETTINGS), tr(STR_LIBRARY_REBUILD)};
  optionPopup.show(tr(STR_LIBRARY_MENU), names, 2, -1, [this](int choice) {
    if (choice == 0)
      pendingFilterMenu = PendingMenu::Settings;
    else if (choice == 1)
      pendingFilterMenu = PendingMenu::Rebuild;
  });
  requestUpdate();
}

uint16_t LibraryListActivity::facetOrdinal(int row) const {
  uint16_t value = 0xFFFF;
  if (facetChoices && row >= 0 && row < facetCount) {
    const int position = descendingTabs & 2 ? facetCount - 1 - row : row;
    std::memcpy(&value, facetChoices.get() + position * sizeof(value), sizeof(value));
  }
  return value;
}

void LibraryListActivity::openFacetPicker(library::Facet facet) {
  RenderLock lock;
  facetChoices.reset();
  facetCount = 0;
  facetKind = facet;
  const int total = index.bookCount();
  // One ordinal per book in PSRAM; names are loaded only for the visible cards.
  if (total) facetChoices = HalMemory::allocatePsram(size_t(total) * sizeof(uint16_t));
  if (total && !facetChoices) {
    index.dropBrowseCache();
    facetChoices = HalMemory::allocatePsram(size_t(total) * sizeof(uint16_t));
  }
  if (total && !facetChoices) {
    LOG_ERR("LIB", "OOM: %u-byte filter choices", unsigned(total * sizeof(uint16_t)));
    GUI.drawPopup(renderer, tr(STR_LIBRARY_SEARCH_UNAVAILABLE));
    return;
  }
  index.cacheForBrowsing([] { delay(1); });
  std::string prior, value;
  prior.reserve(256);
  value.reserve(256);
  bool first = true;
  for (int row = 0; row < total; ++row) {
    if ((row & 63) == 0) delay(1);
    const uint16_t ordinal = index.ordinalForRow(library::facetOrder(facet), row);
    library::ClixRecord record{};
    if (ordinal == 0xFFFF || !index.readRecord(ordinal, record) || !library::readFacet(index, record, facet, value)) {
      facetChoices.reset();
      facetCount = 0;
      LOG_ERR("LIB", "cannot read filter choices");
      GUI.drawPopup(renderer, tr(STR_LIBRARY_FILTER_UNAVAILABLE));
      return;
    }
    const auto key = library::fold(value);
    if (first || key != prior) {
      std::memcpy(facetChoices.get() + facetCount * sizeof(ordinal), &ordinal, sizeof(ordinal));
      ++facetCount;
    }
    first = false;
    prior = key;
  }
  closeRouting();
  view = View::Facets;
  preferencesNav.reset();
  resetUi();
  requestUpdate();
}

void LibraryListActivity::showCategories() {
  RenderLock lock;
  closeRouting();
  activeTabIndex = 1;
  headerSearchTitle.clear();
  view = View::Categories;
  facetChoices.reset();
  preferencesNav.reset();
  resetUi();
  requestUpdate();
}

void LibraryListActivity::chooseFacet(int row) {
  RenderLock lock;
  library::ClixRecord record{};
  std::string value;
  if (!index.readRecord(facetOrdinal(row), record) || !library::readFacet(index, record, facetKind, value)) return;
  facetFilter = {};
  auto& selected = facetFilter.forFacet(facetKind);
  selected.value = std::move(value);
  selected.enabled = true;
  query.clear();
  headerSearchTitle.clear();
  sortOrder = library::facetOrder(facetKind);
  if (descendingTabs & 2) sortOrder = static_cast<library::SortOrder>(static_cast<uint8_t>(sortOrder) ^ 1);
  view = View::Books;
  closeRouting();
  applyFilter();
  activeNav().reset();
  windowTop = -1;
  resetUi();
  requestUpdate();
}

void LibraryListActivity::buildFilterRows(UiScreen& screen) {
  static constexpr StrId categories[] = {StrId::STR_LIBRARY_GENRE, StrId::STR_LIBRARY_SERIES,
                                         StrId::STR_LIBRARY_AUTHOR_FIRST, StrId::STR_LIBRARY_AUTHOR_LAST,
                                         StrId::STR_LIBRARY_FIRST_LETTER};
  if (view == View::Facets) {
    const int category = facetKind == library::Facet::Genre         ? 0
                         : facetKind == library::Facet::Series      ? 1
                         : facetKind == library::Facet::AuthorFirst ? 2
                         : facetKind == library::Facet::Author      ? 3
                                                                    : 4;
    fui::TextStyle title;
    title.font = 0;
    title.bold = true;
    screen.target().text(screen.takeTop(36, 8), I18N.get(categories[category]), title);
  }
  fui::ListProps props;
  libraryui::filterCards(props, screen.target());
  props.count = listCount();
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.selectedIndex = -1;
  syncListViewport(screen, props);
  props.selectedIndex = -1;
  const int first = props.topIndex;
  const int shown = std::min(9, std::max(0, listCount() - first));
  for (int n = 0; n < shown; ++n) {
    auto& item = preferenceItems[n];
    item = {};
    item.actionValue = first + n;
    item.value = ">";
    if (view == View::Categories)
      item.label = I18N.get(categories[descendingTabs & 2 ? 4 - first - n : first + n]);
    else {
      library::ClixRecord record{};
      auto& name = facetNames[n];
      if (!index.readRecord(facetOrdinal(first + n), record) || !library::readFacet(index, record, facetKind, name))
        name.clear();
      if (facetKind == library::Facet::Author && !name.empty()) {
        const auto space = name.find_last_of(' ');
        if (space != std::string::npos) name = name.substr(space + 1) + ", " + name.substr(0, space);
      }
      if (name.empty())
        name = facetKind == library::Facet::Series  ? tr(STR_LIBRARY_NO_SERIES)
               : facetKind == library::Facet::Genre ? tr(STR_LIBRARY_NO_GENRE)
                                                    : tr(STR_LIBRARY_UNKNOWN_AUTHOR);
      item.label = name.c_str();
    }
  }
  props.items = preferenceItems;
  props.itemsWindowFirst = first;
  props.itemsWindowCount = shown;
  libraryui::filterList(screen, props);
  if (!props.count) screen.centeredText(tr(STR_LIBRARY_EMPTY));
}

void LibraryListActivity::toggleSortDirection() {
  RenderLock lock;
  descendingTabs ^= 1u << activeTabIndex;
  if (view == View::Books) {
    sortOrder = static_cast<library::SortOrder>(static_cast<uint8_t>(sortOrder) ^ 1);
    closeRouting();
    applyFilter();
    activeNav().reset();
    windowTop = -1;
    invalidateGrid();
  } else {
    preferencesNav.reset();
  }
  resetUi();
  requestUpdate();
}

int LibraryListActivity::tabCount() const { return 2; }

int LibraryListActivity::activeTab() const { return activeTabIndex; }

const char* LibraryListActivity::tabLabel(const int index) const {
  return index == 0 ? tr(STR_LIBRARY_TAB_RECENT) : tr(STR_LIBRARY_FILTER);
}

fui::TabIndicator LibraryListActivity::tabIndicator(const int) const { return fui::TabIndicator::None; }

int LibraryListActivity::bookRowCount() const {
  if (resultMapped()) return int(filteredCount);
  if (isRecentSort(sortOrder)) return pinnedCount();
  return static_cast<int>(index.bookCount());
}

int LibraryListActivity::listCount() const {
  return view == View::Categories ? 5
         : view == View::Facets   ? facetCount
         : view == View::Settings ? int(libraryui::Preference::Count)
         : groupsCollapsed        ? static_cast<int>(groupCount)
                                  : bookRowCount();
}

// Store rows resolve through storeIndexFor(); other entries name index positions.
int LibraryListActivity::rowFor(const int entry) const {
  if (resultMapped()) {
    if (entry < 0 || entry >= static_cast<int>(filteredCount) || !filtered) return 0;
    return filtered[entry] & 0x8000 ? 0 : filtered[entry];
  }
  return entry;
}

bool LibraryListActivity::groupable() const { return !degraded && isGroupedOrder(sortOrder) && bookRowCount() > 0; }

uint32_t LibraryListActivity::titleInitialFor(const int entry) {
  const uint16_t ordinal = index.ordinalForRow(sortOrder, static_cast<uint16_t>(rowFor(entry)));
  library::ClixRecord record{};
  if (ordinal == 0xFFFF || !index.readRecord(ordinal, record)) return 0;
  return library::foldedGroupInitial(std::string_view(record.fold, record.foldLen));
}

bool LibraryListActivity::buildGroupStarts() {
  const int count = bookRowCount();
  if (count <= 0) return false;
  if (groupCapacity < count) {
    auto starts = makeUniqueNoThrow<uint16_t[]>(static_cast<size_t>(count));
    if (!starts) {
      LOG_ERR("LIB", "cannot allocate %u-byte group map", static_cast<unsigned>(count * sizeof(uint16_t)));
      return false;
    }
    groupStarts = std::move(starts);
    groupCapacity = static_cast<uint16_t>(count);
  }

  groupCount = 0;
  uint32_t previousInitial = 0;
  std::string previousAuthor;
  std::string title;
  std::string author;
  previousAuthor.reserve(128);
  title.reserve(128);
  author.reserve(128);
  for (int entry = 0; entry < count; entry++) {
    bool startsGroup = entry == 0;
    if (isAuthorSort(sortOrder) || isFacetSort(sortOrder)) {
      rowTextFor(entry, title, author);
      startsGroup = startsGroup || author != previousAuthor;
      previousAuthor = author;
    } else {
      const uint32_t initial = titleInitialFor(entry);
      startsGroup = startsGroup || initial != previousInitial;
      previousInitial = initial;
    }
    if (startsGroup) groupStarts[groupCount++] = static_cast<uint16_t>(entry);
  }
  LOG_DBG("LIB", "group map: %u groups, %u bytes", static_cast<unsigned>(groupCount),
          static_cast<unsigned>(groupCapacity * sizeof(uint16_t)));
  return groupCount > 0;
}

int LibraryListActivity::groupForBook(const int bookEntry) const {
  int group = 0;
  while (group + 1 < groupCount && groupStarts[group + 1] <= bookEntry) group++;
  return group;
}

bool LibraryListActivity::collapseGroups(const int bookEntry) {
  RenderLock lock;
  windowTop = -1;
  invalidateGrid();
  if (!groupable() || !buildGroupStarts()) return false;
  expandedNav = activeNav();
  groupsCollapsed = true;
  auto& nav = activeNav();
  nav.reset(groupForBook(bookEntry) + 1);
  requestUpdate();
  return true;
}

void LibraryListActivity::expandGroup(const int groupEntry) {
  RenderLock lock;
  windowTop = -1;
  invalidateGrid();
  if (!groupsCollapsed || groupEntry < 0 || groupEntry >= groupCount) return;
  const int bookEntry = groupStarts[groupEntry];
  groupsCollapsed = false;
  activeNav() = expandedNav;
  auto& nav = activeNav();
  nav.selected = bookEntry + 1;
  nav.top = bookEntry;
  nav.followOnBuild = true;
  requestUpdate();
}

void LibraryListActivity::restoreExpandedList() {
  RenderLock lock;
  windowTop = -1;
  invalidateGrid();
  if (!groupsCollapsed) return;
  groupsCollapsed = false;
  activeNav() = expandedNav;
  requestUpdate();
}

// One checked result slot per candidate, reused while paging. Hiding finished
// books additionally builds a temporary completion lookup from the journal.
void LibraryListActivity::applyFilter() {
  windowTop = -1;
  invalidateGrid();
  groupsCollapsed = false;
  groupCount = 0;
  filtered.reset();
  filteredCount = 0;
  filterFailed = false;
  // The header shows the active query in place of the screen title, so the
  // reader can see what narrowed the list without reopening the keyboard.
  headerSearchTitle = query.empty() ? std::string() : "“" + query + "”";
  if (!resultMapped()) return;
  if (!SETTINGS.libraryFileTypes) return;

  const std::string needle = library::fold(query);
  const int total = static_cast<int>(index.bookCount());
  const int capacity = isRecentSort(sortOrder) ? pinnedTotal : total;
  if (capacity <= 0) return;

  auto matches = makeUniqueNoThrow<uint16_t[]>(static_cast<size_t>(capacity));
  if (!matches) {
    index.dropBrowseCache();
    matches = makeUniqueNoThrow<uint16_t[]>(static_cast<size_t>(capacity));
  }
  if (!matches) {
    LOG_ERR("LIB", "cannot allocate %u-byte search result buffer", static_cast<unsigned>(capacity * sizeof(uint16_t)));
    filterFailed = true;
    return;
  }

  uint16_t matchCount = 0;
  std::string author;
  author.reserve(256);
  std::string path;
  path.reserve(520);
  std::unique_ptr<libraryui::FinishedBooks> finished;
  if (!(SETTINGS.libraryDisplay & libraryui::kFinished)) {
    // The completion lookup takes priority over the optional metadata snapshot.
    index.dropBrowseCache();
    finished = makeUniqueNoThrow<libraryui::FinishedBooks>();
    if (!finished || !finished->begin()) {
      LOG_ERR("LIB", "Finished-book filter unavailable");
      filterFailed = true;
      return;
    }
    if (finished->empty()) finished.reset();
  }
  HalMemory::PsramBuffer folders;
  const size_t folderCount = index.header().folderCount;
  if (finished && !isRecentSort(sortOrder)) {
    if (folderCount > library::CLIX_MAX_RECORDS ||
        !(folders = HalMemory::allocatePsram(folderCount * sizeof(uint32_t))) ||
        !index.readFolderOffsets(reinterpret_cast<uint32_t*>(folders.get()), folderCount, [] { delay(1); })) {
      LOG_ERR("LIB", "Finished-book folder lookup unavailable");
      filterFailed = true;
      return;
    }
  }
  const auto accept = [&](int row) {
    const uint16_t ordinal = index.ordinalForRow(sortOrder, static_cast<uint16_t>(row));
    library::ClixRecord record{};
    if (ordinal == 0xFFFF || !index.readRecord(ordinal, record)) {
      filterFailed = true;
      return;
    }
    if (!index.readName(record, author)) {
      filterFailed = true;
      return;
    }
    if (!libraryui::acceptsFormat(author, SETTINGS.libraryFileTypes)) return;
    if (finished) {
      bool complete = false;
      if (!index.readFolder(record.folderId, reinterpret_cast<const uint32_t*>(folders.get()), folderCount, path)) {
        filterFailed = true;
        return;
      }
      if (path.empty() || path.back() != '/') path.push_back('/');
      path += author;
      if (!finished->contains(path, complete)) {
        filterFailed = true;
        return;
      }
      if (complete) return;
    }
    if (!facetFilter.matches(index, record, author)) {
      filterFailed |= index.ioFailed();
      return;
    }
    if (library::matchesQuery(std::string_view(record.fold, record.foldLen), needle)) {
      matches[matchCount++] = static_cast<uint16_t>(row);
      return;
    }
    author.clear();
    if (index.readAuthor(record, author) && library::matchesQuery(library::fold(author), needle))
      matches[matchCount++] = static_cast<uint16_t>(row);
    filterFailed |= index.ioFailed();
  };
  if (isRecentSort(sortOrder)) {
    const auto& books = RECENT_BOOKS.getBooks();
    for (int position = 0; position < pinnedTotal && !filterFailed; ++position) {
      const int at = isDescending(sortOrder) ? position : pinnedTotal - 1 - position;
      if (at >= int(books.size())) {
        filterFailed = true;
        break;
      }
      const auto& book = books[at];
      if (!libraryui::acceptsFormat(book.path, SETTINGS.libraryFileTypes)) continue;
      if (finished) {
        bool complete = false;
        if (!finished->contains(book.path, complete)) {
          filterFailed = true;
          break;
        }
        if (complete) continue;
      }
      if (facetFilter.active()) {
        const auto ascending = pinnedAscRows[at];
        library::ClixRecord record{};
        if (ascending == 0xffff) continue;
        const auto ordinal = index.ordinalForRow(library::SortOrder::RecentAsc, ascending);
        if (ordinal == 0xffff || !index.readRecord(ordinal, record)) {
          filterFailed = true;
          break;
        }
        if (!facetFilter.matches(index, record, author)) {
          filterFailed |= index.ioFailed();
          continue;
        }
      }
      if (!library::matchesQuery(library::fold(book.title), needle) &&
          !library::matchesQuery(library::fold(book.author), needle))
        continue;
      matches[matchCount++] = uint16_t(0x8000 | at);
    }
  } else
    for (int position = 0; position < total && !filterFailed; ++position) {
      if ((position & 63) == 0) delay(1);
      accept(position);
    }
  if (filterFailed) {
    LOG_ERR("LIB", "cannot read filter result");
    return;
  }
  filtered = std::move(matches);
  filteredCount = matchCount;
}

// Staged back-out, shared by the Back button and the header's back arrow:
// clear the search, expand collapsed groups, return focus to the tabs, then
// leave for home.
void LibraryListActivity::handleBackAction() {
  if (view == View::Settings) {
    closePreferences();
    return;
  }
  if (view == View::Facets) {
    showCategories();
    return;
  }
  if (view == View::Categories) {
    selectTab(0, false);
    return;
  }
  if (facetFilter.active()) {
    openFacetPicker(facetKind);
    return;
  }
  auto& nav = activeNav();
  if (filtering()) {
    RenderLock lock;
    query.clear();
    facetFilter = {};
    applyFilter();
    nav.selected = 0;
    nav.top = 0;
    requestUpdate();
  } else if (groupsCollapsed) {
    restoreExpandedList();
  } else if (!tabsFocused() && !degraded) {
    // Keep the current list and viewport while returning focus to the tabs.
    nav.selected = 0;
    requestUpdate();
  } else {
    onGoHome();
  }
}

void LibraryListActivity::searchActionTrampoline(const fui::ActionEvent&, void* user) {
  static_cast<LibraryListActivity*>(user)->openSearch();
}

void LibraryListActivity::backActionTrampoline(const fui::ActionEvent&, void* user) {
  static_cast<LibraryListActivity*>(user)->handleBackAction();
}

void LibraryListActivity::rebuildActionTrampoline(const fui::ActionEvent&, void* user) {
  static_cast<LibraryListActivity*>(user)->promptRebuildIndex();
}

// Title and author for one entry, read straight from the index. Only ever
// called for rows about to be drawn, so at most a screenful of strings exists
// at once.
bool LibraryListActivity::rowTextFor(const int entry, std::string& title, std::string& author, std::string* fileName,
                                     uint64_t* bytes) {
  title.clear();
  author.clear();
  if (fileName) fileName->clear();
  if (bytes) *bytes = UINT64_MAX;
  const int store = storeIndexFor(entry);
  if (store >= 0) {
    const auto& books = RECENT_BOOKS.getBooks();
    if (store >= static_cast<int>(books.size())) return false;
    const auto& book = books[static_cast<size_t>(store)];
    title = book.title;
    author = book.author;
    if (fileName) *fileName = book.path;
    if (bytes) {
      auto file = Storage.open(book.path.c_str());
      if (file && !file.isDirectory()) *bytes = file.fileSize64();
    }
    return true;
  }
  const uint16_t ordinal = index.ordinalForRow(sortOrder, static_cast<uint16_t>(rowFor(entry)));
  library::ClixRecord record{};
  if (ordinal != 0xFFFF && index.readRecord(ordinal, record)) {
    if (bytes) *bytes = record.fileSize;
    // The build already decided both fields — from the book's own metadata when
    // it has any, and with one spelling chosen per author across the library.
    // Re-parsing the name here would throw that away, and only works while the
    // name still looks like "Title - Author".
    if (!index.readAuthor(record, author)) author.clear();
    if (sortOrder == library::SortOrder::SeriesAsc || sortOrder == library::SortOrder::SeriesDesc) {
      if (!index.readSeries(record, author) || author.empty()) author = tr(STR_LIBRARY_NO_SERIES);
    } else if (sortOrder == library::SortOrder::GenreAsc || sortOrder == library::SortOrder::GenreDesc) {
      if (!index.readGenre(record, author) || author.empty()) author = tr(STR_LIBRARY_NO_GENRE);
    }

    // The stored title when the book gave one, the filename otherwise.
    if (!index.readTitle(record, title)) title.clear();
    if (title.empty() && !index.readName(record, title)) windowReadFailed = true;
    if (fileName && !index.readName(record, *fileName)) windowReadFailed = true;
    windowReadFailed |= index.ioFailed();
  } else {
    windowReadFailed = true;
  }
  if (title.empty()) title = tr(STR_LIBRARY_UNKNOWN_TITLE);
  return true;
}

uint8_t& LibraryListActivity::recentViewSetting() const {
  return libraryui::layoutFromStored(SETTINGS.libraryLayout) == libraryui::Layout::Classic ? SETTINGS.libraryClassicGrid
                                                                                           : SETTINGS.libraryView;
}
uint8_t& LibraryListActivity::expandedListSetting() const {
  return libraryui::layoutFromStored(SETTINGS.libraryLayout) == libraryui::Layout::Classic
             ? SETTINGS.libraryClassicExpanded
             : SETTINGS.libraryExpandedList;
}
bool LibraryListActivity::gridView() const {
  return view == View::Books && recentViewSetting() == 1 && isRecentSort(sortOrder) && !groupsCollapsed;
}
void LibraryListActivity::toggleView() {
  RenderLock lock;
  closeRouting();
  auto& setting = recentViewSetting();
  const auto previous = setting;
  setting = setting == 1 ? 0 : 1;
  if (!SETTINGS.saveToFile()) {
    setting = previous;
    const char* options[] = {tr(STR_OK_BUTTON)};
    optionPopup.showMessage(tr(STR_LIBRARY_RECENT_VIEW), tr(STR_LIBRARY_SAVE_FAILED), options, 1, 0,
                            [this](int) { requestUpdate(); });
    requestUpdate();
    return;
  }
  ensureGridBuffer();
  invalidateGrid();
  activeNav().reset();
  pendingCover.clear();
  coverAttempts = 0;
  resetUi();
  requestUpdate();
}
void LibraryListActivity::ensureGridBuffer() {
  if (recentViewSetting() != 1 || gridCache) return;
  // A page-sized, bounded PSRAM cache survives selection changes; no internal-RAM fallback.
  gridCache = HalMemory::allocatePsram(GRID_CACHE_BYTES);
  if (!gridCache) LOG_ERR("LIB", "Cannot allocate %u-byte grid cache; drawing directly", unsigned(GRID_CACHE_BYTES));
}
void LibraryListActivity::invalidateGrid() {
  gridFirst = -1;
  gridCacheValid = false;
  gridMissingCovers = 0;
  coverWorkRequested = false;
  coverAttempts = 0;
  pendingCover.clear();
}
void LibraryListActivity::requestUpdate(const bool immediate) {
  presentationPending = true;
  Activity::requestUpdate(immediate);
}
void LibraryListActivity::moveRingTo(const int ringIndex) {
  presentationPending = true;
  UiTabListActivity::moveRingTo(ringIndex);
}
bool LibraryListActivity::loadPendingCover() {
  const int pageSize = gridPageSize();
  if (!gridView() || gridFirst != std::max(0, selectedEntry()) / pageSize * pageSize ||
      gridOrientation != static_cast<uint8_t>(renderer.getOrientation()) || !gridMissingCovers)
    return false;
  int slot = 0;
  while (!(gridMissingCovers & (1u << slot))) ++slot;
  gridMissingCovers &= ~(1u << slot);
  coverAttempts |= 1u << slot;
  const int entry = gridFirst + slot;
  pendingCover.clear();
  const int store = storeIndexFor(entry);
  if (store >= 0) {
    pendingCover = RECENT_BOOKS.getBooks()[store].path;
  } else {
    const auto ordinal = index.ordinalForRow(sortOrder, static_cast<uint16_t>(rowFor(entry)));
    if (ordinal != 0xffff && index.readRecord(ordinal, gridRecord)) index.readPath(gridRecord, pendingCover);
  }
  if (FsHelpers::hasEpubExtension(pendingCover)) {
    // The render worker owns the parser; the input loop keeps sampling buttons.
    auto epub = makeUniqueNoThrow<Epub>(pendingCover, "/.crosspoint");
    if (epub)
      epub->generateThumbBmpFromSource(pendingCoverHeight);
    else
      LOG_ERR("LIB", "OOM: cover EPUB");
  } else if (FsHelpers::hasXtcExtension(pendingCover)) {
    auto xtc = makeUniqueNoThrow<Xtc>(pendingCover, "/.crosspoint", SETTINGS.xtcCoverRotation != 0);
    if (xtc && xtc->load())
      xtc->generateThumbBmp(pendingCoverHeight);
    else if (!xtc)
      LOG_ERR("LIB", "OOM: cover XTC");
  }
  pendingCover.clear();
  if (!gridMissingCovers) {
    gridCacheValid = false;
    requestUpdate();
  }
  return true;
}
bool LibraryListActivity::handleCustomInput() {
  if (pendingFilterMenu != PendingMenu::None) {
    const auto action = pendingFilterMenu;
    pendingFilterMenu = PendingMenu::None;
    switch (action) {
      case PendingMenu::Settings:
        showPreferences();
        break;
      case PendingMenu::Rebuild:
        promptRebuildIndex();
        break;
      case PendingMenu::None:
        break;
    }
    return true;
  }
  if (!optionPopup.isActive() && view != View::Facets) facetChoices.reset();
  int tapX = 0, tapY = 0;
  const bool touchNavigation =
      mappedInput.wasSwipe() != MappedInputManager::SwipeDir::None || mappedInput.wasScreenTapped(tapX, tapY);
  bool navigation = touchNavigation || mappedInput.wasScreenTouchPressed(tapX, tapY) ||
                    mappedInput.isScreenTouchHeld(tapX, tapY) || mappedInput.wasScreenTouchReleased();
  for (auto key :
       {MappedInputManager::Button::Back, MappedInputManager::Button::Confirm, MappedInputManager::Button::NavNext,
        MappedInputManager::Button::NavPrevious, MappedInputManager::Button::Left, MappedInputManager::Button::Right})
    navigation |= mappedInput.isPressed(key) || mappedInput.wasPressed(key) || mappedInput.wasReleased(key);
  if (navigation) {
    lastNavigationAt = millis();
    coverWorkRequested = false;
  }
  if (optionPopup.handleInput(mappedInput, [this] { requestUpdate(); })) return true;
  if (touchNavigation && buttonCursorVisible) {
    buttonCursorVisible = false;
    requestUpdate();
  }
  if (!navigation && millis() - lastNavigationAt >= COVER_IDLE_MS) {
    // Never wait for an e-ink refresh or thumbnail decode before reading input.
    RenderLock lock(RenderLock::Mode::Try);
    if (lock.ownsLock() && gridView() && gridMissingCovers && !coverWorkRequested && !presentationPending) {
      coverWorkPaint = paintclock::counter();
      coverWorkRequested = true;
      Activity::requestUpdate();
    }
  }
  if (gridView()) {
    const auto swipe = mappedInput.wasSwipe();
    if (!mappedInput.wasReleased(MappedInputManager::Button::Back) &&
        (swipe == MappedInputManager::SwipeDir::Left || swipe == MappedInputManager::SwipeDir::Right)) {
      const int pageSize = gridPageSize();
      const int page = std::max(0, selectedEntry()) / pageSize;
      const int next = std::clamp(page + (swipe == MappedInputManager::SwipeDir::Left ? 1 : -1), 0,
                                  std::max(0, (listCount() - 1) / pageSize));
      moveRingTo(next * pageSize + 1);
      return true;
    }
  }

  if (lockNextConfirmRelease && mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    lockNextConfirmRelease = false;
    return true;
  }
  if (lockNextBackRelease && mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    lockNextBackRelease = false;
    return true;
  }

  return false;
}

bool LibraryListActivity::handleButtons() {
  if (view != View::Books) return UiListActivity::handleButtons();
  const int count = listCount();

  // Every hold action fires at the threshold, mid-hold, including the ones
  // that open a dialog (remove-recent, delete). The release that follows is
  // armed as suppressed by wasLongPressed() and consumed globally by
  // ActivityManager::loop() before any activity runs, so it cannot land in
  // the freshly opened confirmation and select its default.
  if (mappedInput.wasLongPressed(MappedInputManager::Button::Confirm, LONG_PRESS_MS)) {
    if (tabsFocused()) {
      if (!degraded) toggleSortDirection();
    } else if (isRecentSort(sortOrder)) {
      showRecentBookOptions(selectedEntry());
    } else if (deleteEligible()) {
      if (count > 0) promptDeleteBook(selectedEntry());
    } else if (!groupsCollapsed && groupable()) {
      collapseGroups(selectedEntry());
    } else {
      activateIndex(selectedEntry());
    }
    return true;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    handleBackAction();
    return true;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (tabsFocused()) {
      openSortPicker();
      return true;
    }
    if (count > 0) activateIndex(selectedEntry());
    return true;
  }

  return false;
}

void LibraryListActivity::navigateButtons() {
  if (view != View::Books) {
    UiListActivity::navigateButtons();
    return;
  }
  const int count = listCount();
  buttonNavigator.onNext([this, count] {
    if (count <= 0) return;
    const int next = !buttonCursorVisible || tabsFocused() ? 1 : ringPos() % count + 1;
    buttonCursorVisible = true;
    moveRingTo(next);
  });
  buttonNavigator.onPrevious([this, count] {
    if (count <= 0) return;
    const int previous = !buttonCursorVisible || tabsFocused() ? 1 : ringPos() <= 1 ? count : ringPos() - 1;
    buttonCursorVisible = true;
    moveRingTo(previous);
  });
}

void LibraryListActivity::buildRows(UiScreen& screen) {
  invalidateGrid();
  auto& nav = activeNav();
  const int count = listCount();
  const bool grouped = false;
  const bool authorGrouped = (groupsCollapsed || grouped) && (isAuthorSort(sortOrder) || isFacetSort(sortOrder));

  fui::ListProps props;
  props.count = static_cast<uint16_t>(count);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch | fui::InputLongPress;
  uiTarget.setFont(4, toybox::kBookListFontId);
  libraryui::bookRows(props, screen.target(), false);
  props.labelText.maxLines = 2;
  if (isRecentSort(sortOrder) && screen.device().width < screen.device().height) {
    props.rowHeight = screen.body().height / 9;
    props.rowPaddingY = 4;
  }

  props.rtl =
      I18N.getLanguage() == Language::AR || I18N.getLanguage() == Language::FA || I18N.getLanguage() == Language::HE;
  props.headerUnderline = false;
  syncTabListViewport(screen, props);
  if (!buttonCursorVisible) props.selectedIndex = -1;
  props.partialTrailingRow = false;

  // Keep one extra entry in the reusable window for a clipped trailing row.
  const size_t cap = std::min(MAX_VISIBLE_WINDOW, static_cast<size_t>(nav.visibleRows > 0 ? nav.visibleRows : 1) + 1);
  const int windowStart = static_cast<int>(props.topIndex);
  if (windowTop != windowStart || windowTotal != count || windowCapacity != cap || windowLight != chrome::lightHeader) {
    windowReadFailed = false;
    if (winTitles.size() < cap) winTitles.resize(cap);
    if (winAuthors.size() < cap) winAuthors.resize(cap);
    if (!groupsCollapsed && winHeaders.size() < cap) winHeaders.resize(cap);
    winItems.clear();
    if (winItems.capacity() < cap) winItems.reserve(cap);

    int rows = 0;
    int headers = 0;
    uint32_t previousInitial = 0;
    std::string rowFile;
    rowFile.reserve(128);
    for (int entry = windowStart; entry < count && rows < static_cast<int>(cap); entry++) {
      std::string& title = winTitles[static_cast<size_t>(rows)];
      std::string& author = winAuthors[static_cast<size_t>(rows)];
      fui::ListItem item;
      if (groupsCollapsed) {
        const int bookEntry = groupStarts[entry];
        if (authorGrouped) {
          rowTextFor(bookEntry, title, author);
          formatAuthorHeading(author, title);
        } else {
          formatInitialHeading(titleInitialFor(bookEntry), title);
        }
      } else {
        if (!rowTextFor(entry, title, author, &rowFile)) continue;
        uint32_t initial = 0;
        bool startsGroup = false;
        if (authorGrouped) {
          startsGroup = rows == 0 || author != winAuthors[static_cast<size_t>(rows - 1)];
        } else if (grouped) {
          initial = titleInitialFor(entry);
          startsGroup = rows == 0 || initial != previousInitial;
          previousInitial = initial;
        }
        if (startsGroup) {
          std::string& heading = winHeaders[static_cast<size_t>(headers++)];
          if (authorGrouped)
            formatAuthorHeading(author, heading);
          else
            formatInitialHeading(initial, heading);
          item.sectionHeading = heading.c_str();
        }
      }

      item.label = title.c_str();
      item.labelBold = groupsCollapsed && !chrome::lightHeader;
      // Group headings stay bare; every book row gets its file-type icon.
      if (!groupsCollapsed && !rowFile.empty()) {
        item.icon = listIconFor(UITheme::getFileIcon(rowFile), 32);
      }
      item.actionValue = static_cast<int16_t>(entry);
      winItems.push_back(item);
      rows++;
    }
    windowTop = windowReadFailed || index.ioFailed() ? -1 : windowStart;
    windowTotal = count;
    windowCapacity = cap;
    windowLight = chrome::lightHeader;
  }
  const int rows = static_cast<int>(winItems.size());

  bool unicode = I18N.getLanguage() != Language::EN;
  for (int n = 0; n < rows; ++n) {
    for (unsigned char c : winTitles[n]) unicode |= c >= 0x80;
    for (unsigned char c : winAuthors[n]) unicode |= c >= 0x80;
  }
  pixelui::bindFonts(uiTarget, renderer, unicode);
  uiTarget.setFont(4, toybox::kBookListFontId);

  props.items = winItems.data();
  props.itemsWindowFirst = static_cast<uint16_t>(windowStart);
  props.itemsWindowCount = static_cast<uint16_t>(winItems.size());
  pixelui::list(screen, props, false, grouped && !groupsCollapsed);
  const int next = nav.drawnRows;
  const auto body = screen.body();
  LOG_DBG("LIB", "page tab=%d top=%d full=%d loaded=%d body=%d..%d next=%d title=%s", activeTabIndex, windowStart,
          nav.drawnRows, rows, body.y, body.bottom(),
          next < rows ? winItems[static_cast<size_t>(next)].actionValue : -1,
          next < rows ? winItems[static_cast<size_t>(next)].label : "<none>");
  LOG_DBG("LIB", "page first=%d title=%s", rows > 0 ? winItems[0].actionValue : -1,
          rows > 0 ? winItems[0].label : "<none>");
}

int LibraryListActivity::gridPageSize() const {
  return libraryui::gridCapacity(renderer.getScreenWidth(), renderer.getScreenHeight());
}
void LibraryListActivity::buildGrid(UiScreen& screen) {
  windowTop = -1;
  auto& nav = activeNav();
  const auto grid = libraryui::gridLayout(screen.device(), screen.body());
  const int pageSize = grid.capacity();
  const int count = listCount(), first = std::max(0, selectedEntry()) / pageSize * pageSize;
  const auto area = grid.area;
  const int pages = std::max(1, (count + pageSize - 1) / pageSize);
  pageui::build(screen, grid.pager, first / pageSize + 1, pages, ACTION_GRID_PAGE, first / pageSize, ACTION_GRID_PAGE,
                first / pageSize + 2, pixelui::kSmall, 48);
  nav.top = first;
  nav.visibleRows = nav.drawnRows = pageSize;
  nav.drawnCount = count;
  nav.followOnBuild = false;
  const auto orientation = static_cast<uint8_t>(renderer.getOrientation());
  const bool inverted = SETTINGS.screenInverted != 0;
  if (gridFirst != first || gridTotal != count || gridArea.x != area.x || gridArea.y != area.y ||
      gridArea.width != area.width || gridArea.height != area.height || gridOrientation != orientation ||
      gridInverted != inverted) {
    invalidateGrid();
    gridFirst = first;
    gridTotal = count;
    gridArea = area;
    gridOrientation = orientation;
    gridInverted = inverted;
  }
  const bool cached =
      gridCacheValid && gridCache &&
      renderer.copyBufferToRegion(area.x, area.y, area.width, area.height, gridCache.get(), GRID_CACHE_BYTES);
  if (!cached) {
    gridCacheValid = false;
    gridMissingCovers = 0;
    windowReadFailed = false;
    if (winTitles.size() < 9) winTitles.resize(9);
    if (winAuthors.size() < 9) winAuthors.resize(9);
    // Borrow the invalid cache as a sequential BMP read buffer, then replace it with the finished grid.
    gridCoverBitmap.setReadAheadBuffer(gridCache.get(), gridCache ? 4096 : 0);
  }
  const auto cardFor = [&](int n) { return grid.card(n); };
  const auto ink = fui::Paint::solid(fui::Color::Black);
  for (int n = 0; n < pageSize && first + n < count; ++n) {
    const int entry = first + n;
    const auto card = cardFor(n);
    screen.frame().hit(card, ACTION_ROW, entry, fui::InputTouch | fui::InputLongPress);
    if (cached) continue;
    auto& title = winTitles[n];
    auto& author = winAuthors[n];
    auto& name = gridFileName;
    auto& path = gridPath;
    if (!rowTextFor(entry, title, author, &name)) continue;
    if (const int store = storeIndexFor(entry); store >= 0)
      path = RECENT_BOOKS.getBooks()[store].path;
    else {
      const auto ordinal = index.ordinalForRow(sortOrder, static_cast<uint16_t>(rowFor(entry)));
      if (ordinal == 0xffff || !index.readRecord(ordinal, gridRecord) || !index.readPath(gridRecord, path)) continue;
    }
    const auto art = fui::makeRect(card.x + 5, card.y + 5, card.width - 10, card.height - 10);
    screen.target().stroke(card, ink, pixelui::kBorder);
    bool drawn = false;
    if (FsHelpers::hasEpubExtension(path) || FsHelpers::hasXtcExtension(path)) {
      auto* thumb = pixelTitle;
      std::snprintf(thumb, sizeof(pixelTitle), "/.crosspoint/%s_%lu/thumb_%s%d.bmp",
                    FsHelpers::hasXtcExtension(path) ? "xtc" : "epub",
                    static_cast<unsigned long>(std::hash<std::string>{}(path)),
                    FsHelpers::hasXtcExtension(path) && SETTINGS.xtcCoverRotation ? "ccw_" : "", art.height);
      if (Storage.openFileForRead("LIB", thumb, gridCoverFile)) {
        if (gridCoverBitmap.parseHeaders() == BmpReaderError::Ok)
          drawn = GUI.drawCoverThumbFill(renderer, gridCoverBitmap, Rect{art.x, art.y, art.width, art.height});
        gridCoverFile.close();
      }
      if (!drawn && !(coverAttempts & (1u << n))) {
        gridMissingCovers |= 1u << n;
        pendingCoverHeight = art.height;
      }
    }
    if (!drawn) {
      const auto icon = listIconFor(UITheme::getFileIcon(name), 32);
      const auto symbol = fui::makeRect(art.x + art.width / 4, art.y + 12, art.width / 2, art.height / 3);
      screen.target().bitmap(symbol, icon, fui::BitmapMode::Contain, ink);
      pixelui::copyCaps(pixelTitle, sizeof(pixelTitle), name.empty() ? title.c_str() : name.c_str());
      fui::TextStyle style;
      style.font = pixelui::kSmall;
      style.maxLines = 3;
      style.align = fui::TextAlign::Center;
      pixelui::fitLabel(screen.target(), pixelTitle, art.width - 16, style);
      screen.target().text(
          fui::makeRect(art.x + 8, symbol.bottom() + 8, art.width - 16, art.bottom() - symbol.bottom() - 12),
          pixelTitle, style);
    }
  }
  if (!cached && gridCache && !windowReadFailed && !index.ioFailed())
    gridCacheValid =
        renderer.copyRegionToBuffer(area.x, area.y, area.width, area.height, gridCache.get(), GRID_CACHE_BYTES);
  const int selected = selectedEntry() - first;
  if (buttonCursorVisible && selected >= 0 && selected < std::min(pageSize, count - first)) {
    const auto card = cardFor(selected);
    screen.target().stroke(card.inset(fui::Insets{3, 3, 3, 3}), fui::Paint::solid(fui::Color::White), 8);
    screen.target().stroke(card.inset(fui::Insets{2, 2, 2, 2}), ink, 5);
  }
}

void LibraryListActivity::formatInitialHeading(uint32_t initial, std::string& out) {
  out.clear();
  if (initial == 0) {
    out.push_back('#');
    return;
  }
  if (initial >= 'a' && initial <= 'z') initial -= 'a' - 'A';
  utf8AppendCodepoint(initial, out);
}

void LibraryListActivity::formatAuthorHeading(const std::string& author, std::string& out) const {
  out = author.empty() ? std::string(tr(STR_LIBRARY_UNKNOWN_AUTHOR)) : author;
  if (author.empty() || isFacetSort(sortOrder) || sortOrder == library::SortOrder::AuthorFirstAsc ||
      sortOrder == library::SortOrder::AuthorFirstDesc)
    return;
  const size_t lastSpace = out.find_last_of(' ');
  if (lastSpace != std::string::npos && lastSpace + 1 < out.size()) {
    out = out.substr(lastSpace + 1) + ", " + out.substr(0, lastSpace);
  }
}

void LibraryListActivity::buildHeader(UiScreen& screen) {
  pixelui::copyNatural(pixelTitle, sizeof(pixelTitle), headerTitle());
  libraryui::header(screen, pixelHeader, pixelTitle, !degraded && view == View::Books,
                    fui::bitmapFromIcon(icon_search_32), fui::bitmapFromIcon(icon_refresh_cw_32), ACTION_SEARCH,
                    ACTION_REBUILD, view == View::Books && isRecentSort(sortOrder) ? ACTION_VIEW : 0, gridView(), 0);
}

void LibraryListActivity::buildScreen(UiScreen& screen) {
  if (view == View::Settings) {
    buildPreferences(screen);
    return;
  }
  const auto& metrics = UITheme::getInstance().getMetrics();
  const char* title = headerTitle();
  bool unicode = I18N.getLanguage() != Language::EN;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(title); p && *p; ++p) unicode |= *p >= 0x80;
  pixelui::FontScope fonts(uiTarget, renderer, unicode);
  buildHeader(screen);
  pixelui::begin(screen, libraryui::kFooterHeight + (mappedInput.hasTouch() ? 0 : metrics.buttonHintsHeight));

  libraryui::filterTabs(screen, activeTabIndex, ACTION_TAB, ACTION_VIEW,
                        activeTabIndex == 0 ? isDescending(sortOrder) : bool(descendingTabs & 2));
  if (view == View::Categories || view == View::Facets) {
    buildFilterRows(screen);
    return;
  }
  if (facetFilter.active()) {
    fui::TextStyle status;
    status.font = 3;
    status.maxLines = 1;
    const auto& chosen = facetFilter.forFacet(facetKind);
    screen.target().text(screen.takeTop(28, 4), chosen.value.empty() ? tr(STR_LIBRARY_FILTERED) : chosen.value.c_str(),
                         status);
  }
  if (bookRowCount() == 0) {
    const char* message = tr(STR_LIBRARY_NO_RESULTS);
    if (filterFailed) {
      message = tr(STR_LIBRARY_FILTER_UNAVAILABLE);
    } else if (!resultMapped()) {
      message = isRecentSort(sortOrder) ? tr(STR_LIBRARY_NO_RECENT) : tr(STR_LIBRARY_EMPTY);
    }
    screen.centeredText(message);
    if (!gridView()) {
      char count[48];
      std::snprintf(count, sizeof(count), tr(STR_COVER_BOOK_COUNT), 0U);
      libraryui::countFooter(screen, count);
    }
    return;
  }
  if (gridView())
    buildGrid(screen);
  else {
    buildRows(screen);
    if (!gridView()) {
      char count[48];
      std::snprintf(count, sizeof(count), tr(STR_COVER_BOOK_COUNT), unsigned(bookRowCount()));
      libraryui::countFooter(screen, count);
    }
  }
}

// "12/69 books" at the bottom right: which book is selected, out of how many.
//
// NOT a page count. How many rows fit varies with the view (author headings
// consume band height), so a page total grows and shrinks as you scroll. The
// book position is stable by construction, and it answers the question the
// reader actually has: how far in am I, and how much is left.
void LibraryListActivity::drawPositionReadout() const {
  if (!gridView()) return;
  const int count = listCount();
  if (count <= 0) return;

  char raw[48], buf[48];
  const char* positionFormat = groupsCollapsed ? tr(STR_LIBRARY_GROUP_POSITION) : tr(STR_LIBRARY_POSITION);
  snprintf(raw, sizeof(raw), positionFormat, selectedEntry() + 1, count);
  pixelui::copyCaps(buf, sizeof(buf), raw);
  const auto& metrics = UITheme::getInstance().getMetrics();
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer, I18N.getLanguage() != Language::EN);
  libraryui::footer(target, target.deviceContext(), buf, mappedInput.hasTouch() ? 0 : metrics.buttonHintsHeight);
}

const char* LibraryListActivity::headerTitle() const {
  if (view == View::Settings) return tr(STR_LIBRARY_SETTINGS);
  if (!headerSearchTitle.empty()) return headerSearchTitle.c_str();
  return degraded ? tr(STR_LIBRARY_TITLE_UNSORTED) : tr(STR_LIBRARY);
}

void LibraryListActivity::drawHoldHelp() const {
  if (mappedInput.hasTouch() || groupsCollapsed || view != View::Books) return;
  const char* help = nullptr;
  if (!tabsFocused() && isRecentSort(sortOrder) && listCount() > 0)
    help = tr(STR_LIBRARY_HOLD_OPTIONS);  // recent rows: hold opens the row menu
  else if (!tabsFocused() && deleteEligible() && listCount() > 0)
    help = tr(STR_HOLD_OPEN_TO_DELETE);
  else if (!tabsFocused() && groupable())
    help = tr(STR_LIBRARY_HOLD_GROUPS);
  if (!help) return;

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int lineHeight = renderer.getLineHeight(SMALL_FONT_ID);
  const int y = renderer.getScreenHeight() - metrics.buttonHintsHeight - lineHeight;
  GUI.drawHelpText(renderer, Rect{SIDE_PADDING, y, renderer.getScreenWidth() / 2 - SIDE_PADDING, lineHeight}, help);
}

// OptionPopup is a self-contained modal: it owns the whole frame (hints
// included) whenever it is up, mirroring the FileBrowser pattern.
void LibraryListActivity::render(RenderLock&& lock) {
  if (optionPopup.processRender(renderer, mappedInput)) return;
  if (coverWorkRequested.exchange(false) && !presentationPending && coverWorkPaint == paintclock::counter() &&
      gridOrientation == static_cast<uint8_t>(renderer.getOrientation()) &&
      gridInverted == (SETTINGS.screenInverted != 0)) {
    if (loadPendingCover()) return;
  }
  presentationPending = false;
  UiTabListActivity::render(std::move(lock));
}

void LibraryListActivity::drawFooter() {
  if (view != View::Books) {
    UiListActivity::drawFooter();
    return;
  }
  drawPositionReadout();
  drawHoldHelp();

  const bool backGoesHome = tabsFocused() && !groupsCollapsed && !filtering();
  const char* backLabel = backGoesHome ? tr(STR_HOME) : tr(STR_BACK);
  const char* confirmLabel = groupsCollapsed ? tr(STR_SELECT) : tr(STR_OPEN);
  const bool canSearch = tabsFocused() && !degraded;
  const auto labels = mappedInput.mapLabels(backLabel, tabsFocused() ? tr(STR_TOGGLE) : confirmLabel,
                                            canSearch ? tr(STR_SEARCH) : tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
