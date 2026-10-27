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
constexpr int TITLE_TAB = 1;
constexpr int AUTHOR_TAB = 2;
constexpr int TAB_SLOTS = AUTHOR_TAB + 1;

constexpr bool isDescending(const library::SortOrder order) {
  return order == library::SortOrder::RecentDesc || order == library::SortOrder::TitleDesc ||
         order == library::SortOrder::AuthorDesc;
}

constexpr bool isRecentSort(const library::SortOrder order) {
  return order == library::SortOrder::RecentAsc || order == library::SortOrder::RecentDesc;
}

constexpr bool isAuthorSort(const library::SortOrder order) {
  return order == library::SortOrder::AuthorAsc || order == library::SortOrder::AuthorDesc;
}

constexpr library::SortOrder orderForTab(const int tab, const uint8_t descendingTabs) {
  const bool descending = (descendingTabs & (1u << tab)) != 0;
  if (tab == TITLE_TAB) return descending ? library::SortOrder::TitleDesc : library::SortOrder::TitleAsc;
  if (tab == AUTHOR_TAB) return descending ? library::SortOrder::AuthorDesc : library::SortOrder::AuthorAsc;
  return descending ? library::SortOrder::RecentDesc : library::SortOrder::RecentAsc;
}

const char* tabLabelFor(const int tab) {
  if (tab == TITLE_TAB) return tr(STR_LIBRARY_TAB_TITLE);
  if (tab == AUTHOR_TAB) return tr(STR_LIBRARY_TAB_AUTHOR);
  return tr(STR_LIBRARY_TAB_RECENT);
}

}  // namespace

LibraryListActivity::LibraryListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiTabListActivity("Library", renderer, mappedInput, true) {}

void LibraryListActivity::onEnter() {
  // One lock across the base lifecycle AND the data phase: the base onEnter
  // schedules a paint, and the render task must not read the index or the
  // filter before they are in place. The rebuild also needs the lock: the
  // render task's SD-loaded fonts read glyph data at draw time, and the walk
  // needs the card to itself.
  RenderLock lock(*this);
  UiTabListActivity::onEnter();
  app.on(ACTION_SEARCH, &LibraryListActivity::searchActionTrampoline, this);
  app.on(ACTION_REBUILD, &LibraryListActivity::rebuildActionTrampoline, this);
  app.on(ACTION_BACK, &LibraryListActivity::backActionTrampoline, this);
  app.on(
      ACTION_VIEW,
      [](const fui::ActionEvent& event, void* context) {
        auto* self = static_cast<LibraryListActivity*>(context);
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

  // Entered while Confirm was still held (typical when launched from the home
  // menu): ignore its release, or we would open whatever sits at row 0.
  lockNextConfirmRelease = mappedInput.isPressed(MappedInputManager::Button::Confirm);
  requestUpdate(true);
}

void LibraryListActivity::onExit() {
  index.close();
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

// The pinned overlay applies only to the shelf that reads as "what am I up
// to": the unfiltered Recent sort, newest first. A search result is a flat
// list the reader narrowed down on purpose, and the ascending toggle asks for
// oldest-first, which pinned fresh reads would contradict.
int LibraryListActivity::pinnedCount() const {
  if (activeTabIndex != RECENT_TAB || !query.empty() || !isDescending(sortOrder)) return 0;
  return pinnedTotal;
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
      pinnedTotal = 0;
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
  if (selectedEntry() < pinnedCount()) {
    const auto& books = RECENT_BOOKS.getBooks();
    if (selectedEntry() >= static_cast<int>(books.size())) return;
    path = books[static_cast<size_t>(selectedEntry())].path;
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
bool LibraryListActivity::deleteEligible() const { return !groupsCollapsed && (!query.empty() || !groupable()); }

void LibraryListActivity::onRowLongPress(const int index) {
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

// Recent-shelf long-press menu (button hold and touch long-press). The first
// rows may come from RecentBooksStore; the rest are index rows sorted by
// modification time. Only store rows can be removed from recents.
void LibraryListActivity::showRecentBookOptions(const int entry) {
  RenderLock lock;
  if (entry < 0 || entry >= listCount()) return;

  std::string path;
  std::string title;
  const bool isStoreRow = entry < pinnedCount();
  if (isStoreRow) {
    const auto& books = RECENT_BOOKS.getBooks();
    if (entry >= static_cast<int>(books.size())) return;
    path = books[static_cast<size_t>(entry)].path;
    title = books[static_cast<size_t>(entry)].title;
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
  // Sort positions, group starts, and pinned rows all point into the old order.
  applyFilter();
  resolvePinned();
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
    if (!query.empty() && filteredCount == 0 && !degraded) {
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

void LibraryListActivity::stepTab(const int direction) {
  const int next = (activeTab() + (direction > 0 ? 1 : TAB_SLOTS - 1)) % TAB_SLOTS;
  selectTab(next, false);
}

void LibraryListActivity::onTabAction(const int index) {
  app.clearTapFlash();
  selectTab(index, true);
}

void LibraryListActivity::selectTab(const int index, const bool toggleIfActive) {
  if (index < 0 || index >= TAB_SLOTS) return;
  RenderLock lock;
  closeRouting();
  if (toggleIfActive && index == activeTab()) descendingTabs ^= static_cast<uint8_t>(1u << index);
  sortOrder = orderForTab(index, descendingTabs);
  // The filter and the overlap rows hold positions in the old order, so they
  // must be rebuilt.
  applyFilter();
  activeTabIndex = index;
  coverAttempts = 0;
  pendingCover.clear();
  refreshOverlap();
  // Tab changes happen only while the bar owns focus. A tab's remembered row
  // must not pull focus back into the list after the switch.
  auto& nav = activeNav();
  nav.selected = 0;
  nav.top = 0;
  resetUi();
  requestUpdate();
}

void LibraryListActivity::toggleSortDirection() { selectTab(activeTab(), true); }

int LibraryListActivity::tabCount() const { return TAB_SLOTS; }

int LibraryListActivity::activeTab() const { return activeTabIndex; }

const char* LibraryListActivity::tabLabel(const int index) const { return tabLabelFor(index); }

fui::TabIndicator LibraryListActivity::tabIndicator(const int index) const {
  if (index != activeTab()) return fui::TabIndicator::None;
  return isDescending(sortOrder) ? fui::TabIndicator::Down : fui::TabIndicator::Up;
}

int LibraryListActivity::bookRowCount() const {
  if (!query.empty()) return isRecentSort(sortOrder) ? std::min(30, int(filteredCount)) : int(filteredCount);
  // Pinned books already in the index are skipped below the pins, not doubled;
  // pinned books the index missed still show, so the difference stays split.
  const int pinned = pinnedCount();
  const int count = static_cast<int>(index.bookCount()) + (pinned > 0 ? pinned - overlapCount : 0);
  return isRecentSort(sortOrder) ? std::min(30, count) : count;
}

int LibraryListActivity::listCount() const { return groupsCollapsed ? static_cast<int>(groupCount) : bookRowCount(); }

// Entry position on screen to row position in the sort order. Identity while
// unfiltered and unpinned, so the shelf costs nothing when nothing is typed.
// With pins active, entries below pinnedCount() belong to the store and must
// not reach this; the rest walk past the pinned books' own sort rows.
int LibraryListActivity::rowFor(const int entry) const {
  if (!query.empty()) {
    if (entry < 0 || entry >= static_cast<int>(filteredCount) || !filtered) return 0;
    return filtered[entry];
  }
  const int pinned = pinnedCount();
  if (pinned == 0) return entry;
  int row = entry - pinned;
  for (int i = 0; i < overlapCount; i++) {
    if (overlapRows[i] <= row) row++;
  }
  return row;
}

bool LibraryListActivity::groupable() const { return !degraded && !isRecentSort(sortOrder) && bookRowCount() > 0; }

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
    if (isAuthorSort(sortOrder)) {
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

// One pass over the sort order, keeping what matches. No index, no cache: at the
// 4096-book format cap this is 4096 comparisons of at most 96 bytes. The result
// array is allocated once with the exact upper bound and fails back to an
// explicit message rather than letting vector growth abort the firmware.
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
  if (query.empty()) return;

  const std::string needle = library::fold(query);
  const int total = static_cast<int>(index.bookCount());
  if (total <= 0) return;

  auto matches = makeUniqueNoThrow<uint16_t[]>(static_cast<size_t>(total));
  if (!matches) {
    LOG_ERR("LIB", "cannot allocate %u-byte search result buffer", static_cast<unsigned>(total * sizeof(uint16_t)));
    filterFailed = true;
    return;
  }

  uint16_t matchCount = 0;
  std::string author;
  for (int row = 0; row < total; row++) {
    const uint16_t ordinal = index.ordinalForRow(sortOrder, static_cast<uint16_t>(row));
    library::ClixRecord record{};
    if (ordinal == 0xFFFF || !index.readRecord(ordinal, record)) continue;
    if (library::matchesQuery(std::string_view(record.fold, record.foldLen), needle)) {
      matches[matchCount++] = static_cast<uint16_t>(row);
      continue;
    }
    // The stored fold covers the title only, so the author has to be read and
    // folded here. That is the search most worth having: the reader who knows
    // the author usually also knows where the book is, while "emily" finding
    // Alice Hunter is the case the shelf exists to answer.
    author.clear();
    if (index.readAuthor(record, author) && library::matchesQuery(library::fold(author), needle)) {
      matches[matchCount++] = static_cast<uint16_t>(row);
    }
  }
  filtered = std::move(matches);
  filteredCount = matchCount;
}

// Staged back-out, shared by the Back button and the header's back arrow:
// clear the search, expand collapsed groups, return focus to the tabs, then
// leave for home.
void LibraryListActivity::handleBackAction() {
  auto& nav = activeNav();
  if (!query.empty()) {
    RenderLock lock;
    query.clear();
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
bool LibraryListActivity::rowTextFor(const int entry, std::string& title, std::string& author, std::string* fileName) {
  title.clear();
  author.clear();
  if (fileName) fileName->clear();
  if (entry < pinnedCount()) {
    const auto& books = RECENT_BOOKS.getBooks();
    if (entry < 0 || entry >= static_cast<int>(books.size())) return false;
    const auto& book = books[static_cast<size_t>(entry)];
    title = book.title;
    author = book.author;
    if (fileName) *fileName = book.path;
    return true;
  }
  const uint16_t ordinal = index.ordinalForRow(sortOrder, static_cast<uint16_t>(rowFor(entry)));
  library::ClixRecord record{};
  if (ordinal != 0xFFFF && index.readRecord(ordinal, record)) {
    // The build already decided both fields — from the book's own metadata when
    // it has any, and with one spelling chosen per author across the library.
    // Re-parsing the name here would throw that away, and only works while the
    // name still looks like "Title - Author".
    if (!index.readAuthor(record, author)) author.clear();
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

bool LibraryListActivity::gridView() const {
  return SETTINGS.libraryView == 1 && isRecentSort(sortOrder) && !groupsCollapsed;
}
void LibraryListActivity::toggleView() {
  RenderLock lock;
  closeRouting();
  SETTINGS.libraryView = SETTINGS.libraryView == 1 ? 0 : 1;
  SETTINGS.saveToFile();
  ensureGridBuffer();
  invalidateGrid();
  activeNav().reset();
  pendingCover.clear();
  coverAttempts = 0;
  resetUi();
  requestUpdate();
}
void LibraryListActivity::ensureGridBuffer() {
  if (SETTINGS.libraryView != 1 || gridCache) return;
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
  if (entry < pinnedCount()) {
    pendingCover = RECENT_BOOKS.getBooks()[entry].path;
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
    auto xtc = makeUniqueNoThrow<Xtc>(pendingCover, "/.crosspoint");
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
      stepTab(1);
      return true;
    }
    if (count > 0) activateIndex(selectedEntry());
    return true;
  }

  return false;
}

void LibraryListActivity::navigateButtons() {
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
  const bool authorGrouped = isAuthorSort(sortOrder);
  const bool grouped = !isRecentSort(sortOrder);

  fui::ListProps props;
  props.count = static_cast<uint16_t>(count);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch | fui::InputLongPress;
  uiTarget.setFont(4, toybox::kBookListFontId);
  libraryui::styleBookRows(props, screen.target());

  props.rtl =
      I18N.getLanguage() == Language::AR || I18N.getLanguage() == Language::FA || I18N.getLanguage() == Language::HE;
  props.headerUnderline = false;
  syncTabListViewport(screen, props);
  if (!buttonCursorVisible) props.selectedIndex = -1;
  props.partialTrailingRow = false;

  // Keep one extra entry in the reusable window for a clipped trailing row.
  const size_t cap = static_cast<size_t>(nav.visibleRows > 0 ? nav.visibleRows : 1) + 1;
  const int windowStart = static_cast<int>(props.topIndex);
  if (windowTop != windowStart || windowTotal != count || windowCapacity != cap) {
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
      item.labelBold = groupsCollapsed;
      // Group headings stay bare; every book row gets its file-type icon.
      if (!groupsCollapsed && !rowFile.empty()) item.icon = listIconFor(UITheme::getFileIcon(rowFile), 32);
      item.actionValue = static_cast<int16_t>(entry);
      winItems.push_back(item);
      rows++;
    }
    windowTop = windowReadFailed || index.ioFailed() ? -1 : windowStart;
    windowTotal = count;
    windowCapacity = cap;
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
  pixelui::list(screen, props, false, grouped && !groupsCollapsed, 2);
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
    if (entry < pinnedCount())
      path = RECENT_BOOKS.getBooks()[entry].path;
    else {
      const auto ordinal = index.ordinalForRow(sortOrder, static_cast<uint16_t>(rowFor(entry)));
      if (ordinal == 0xffff || !index.readRecord(ordinal, gridRecord) || !index.readPath(gridRecord, path)) continue;
    }
    const auto art = fui::makeRect(card.x + 5, card.y + 5, card.width - 10, card.height - 10);
    screen.target().stroke(card, ink, pixelui::kBorder);
    bool drawn = false;
    if (FsHelpers::hasEpubExtension(path) || FsHelpers::hasXtcExtension(path)) {
      auto* thumb = pixelTitle;
      std::snprintf(thumb, sizeof(pixelTitle), "/.crosspoint/%s_%lu/thumb_%d.bmp",
                    FsHelpers::hasXtcExtension(path) ? "xtc" : "epub",
                    static_cast<unsigned long>(std::hash<std::string>{}(path)), art.height);
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
  if (author.empty()) return;
  const size_t lastSpace = out.find_last_of(' ');
  if (lastSpace != std::string::npos && lastSpace + 1 < out.size()) {
    out = out.substr(lastSpace + 1) + ", " + out.substr(0, lastSpace);
  }
}

void LibraryListActivity::buildHeader(UiScreen& screen) {
  pixelui::copyCaps(pixelTitle, sizeof(pixelTitle), headerTitle());
  libraryui::header(screen, pixelHeader, pixelTitle, !degraded, fui::bitmapFromIcon(icon_search_32),
                    fui::bitmapFromIcon(icon_refresh_cw_32), ACTION_SEARCH, ACTION_REBUILD,
                    isRecentSort(sortOrder) ? ACTION_VIEW : 0, gridView());
}

void LibraryListActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const char* title = headerTitle();
  bool unicode = I18N.getLanguage() != Language::EN;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(title); p && *p; ++p) unicode |= *p >= 0x80;
  pixelui::FontScope fonts(uiTarget, renderer, unicode);
  buildHeader(screen);
  pixelui::begin(screen, libraryui::kFooterHeight + (mappedInput.hasTouch() ? 0 : metrics.buttonHintsHeight));

  if (!degraded) {
    for (int n = 0; n < TAB_SLOTS; ++n) {
      pixelui::copyCaps(pixelTabLabels[n], sizeof(pixelTabLabels[n]), tabLabel(n));
      pixelTabs[n] = fui::tabItem(n, activeTab() == n, true, pixelTabLabels[n]);
      pixelTabs[n].indicator = tabIndicator(n);
    }
    pixelui::tabs(screen, pixelTabs, TAB_SLOTS, ACTION_TAB, pixelui::kSmall);
  }
  if (bookRowCount() == 0) {
    const char* message = tr(STR_LIBRARY_NO_RESULTS);
    if (filterFailed) {
      message = tr(STR_LIBRARY_SEARCH_UNAVAILABLE);
    } else if (query.empty()) {
      message = tr(STR_LIBRARY_EMPTY);
    }
    screen.centeredText(message);
    return;
  }
  if (gridView())
    buildGrid(screen);
  else
    buildRows(screen);
}

// "12/69 books" at the bottom right: which book is selected, out of how many.
//
// NOT a page count. How many rows fit varies with the view (author headings
// consume band height), so a page total grows and shrinks as you scroll. The
// book position is stable by construction, and it answers the question the
// reader actually has: how far in am I, and how much is left.
void LibraryListActivity::drawPositionReadout() const {
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
  if (!headerSearchTitle.empty()) return headerSearchTitle.c_str();
  return degraded ? tr(STR_LIBRARY_TITLE_UNSORTED) : tr(STR_LIBRARY);
}

void LibraryListActivity::drawHoldHelp() const {
  if (mappedInput.hasTouch() || groupsCollapsed) return;
  const char* help = nullptr;
  if (tabsFocused() && !degraded)
    help = tr(STR_LIBRARY_HOLD_SORT);
  else if (!tabsFocused() && isRecentSort(sortOrder) && listCount() > 0)
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
  drawPositionReadout();
  drawHoldHelp();

  const bool backGoesHome = tabsFocused() && !groupsCollapsed && query.empty();
  const char* backLabel = backGoesHome ? tr(STR_HOME) : tr(STR_BACK);
  const char* confirmLabel = groupsCollapsed ? tr(STR_SELECT) : tr(STR_OPEN);
  const bool canSearch = tabsFocused() && !degraded;
  const auto labels = mappedInput.mapLabels(backLabel, tabsFocused() ? tr(STR_TOGGLE) : confirmLabel,
                                            canSearch ? tr(STR_SEARCH) : tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
