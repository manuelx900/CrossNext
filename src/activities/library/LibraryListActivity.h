#pragma once

#include <Bitmap.h>
#include <HalMemory.h>
#include <HalStorage.h>
#include <LibraryFilter.h>
#include <LibraryIndexFile.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "RecentBooksStore.h"
#include "activities/UiTabListActivity.h"
#include "apps_local/library/LibraryControls.h"
#include "apps_local/library/LibraryMenu.h"
#include "components/OptionPopup.h"

// The sort strip and book rows share UiTabListActivity's navigation ring.
// Recently opened uses RecentBooksStore; other orders use the card index.
// Only a visible window of row strings is retained. Active filters add one
// fallible uint16_t result slot per candidate; paging reuses that map.
class LibraryListActivity final : public UiTabListActivity {
 public:
  LibraryListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;

 protected:
  // --- UiListActivity / UiTabListActivity contract ---------------------------
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onRowAction(const freeink::ui::ActionEvent& event) override;
  freeink::ui::ListNav& activeNav() override;
  void onBackButton() override { handleBackAction(); }
  void onRowLongPress(int index) override;
  int tabCount() const override;
  int activeTab() const override;
  const char* tabLabel(int index) const override;
  freeink::ui::TabIndicator tabIndicator(int index) const override;
  void onTabAction(int index) override;
  void stepTab(int direction) override;
  bool handleCustomInput() override;
  bool handleButtons() override;
  void navigateButtons() override;
  void moveRingTo(int ringIndex);
  void requestUpdate(bool immediate = false);
  // The FreeInkUI header owns both the title and search touch target.
  void drawChrome() override {}
  void drawFooter() override;
  // OptionPopup is a self-contained modal: it owns rendering (and the button
  // hints) whenever it is up.
  void render(RenderLock&& lock) override;

 private:
  // The screen's own actions, after the base's ACTION_ROW / ACTION_TAB.
  static constexpr freeink::ui::ActionId ACTION_SEARCH = ACTION_TAB_USER;
  static constexpr freeink::ui::ActionId ACTION_REBUILD = ACTION_SEARCH + 1;
  static constexpr freeink::ui::ActionId ACTION_BACK = ACTION_REBUILD + 1;
  static constexpr freeink::ui::ActionId ACTION_VIEW = ACTION_BACK + 1;
  enum class View : uint8_t { Books, Settings, Categories, Facets };
  View view = View::Books;
  View settingsReturn = View::Books;
  bool preferencesChanged = false;
  freeink::ui::ListNav preferencesNav;
  freeink::ui::ListItem preferenceItems[int(libraryui::Preference::Count)]{};
  libraryui::Preferences preferences() const;
  void savePreferences(libraryui::Preferences value);
  void showPreferences();
  void closePreferences();
  void activatePreference(int row);
  void buildPreferences(UiScreen& screen);
  void buildFilterRows(UiScreen& screen);
  void showCategories();
  void chooseFacet(int row);
  bool resultMapped() const { return filtering() || libraryui::resultMapRequired(preferences()); }
  void openSortPicker();
  void openFilterMenu();
  void openFacetPicker(library::Facet facet);
  bool filtering() const { return !query.empty() || facetFilter.active(); }
  uint16_t facetOrdinal(int row) const;
  library::LibraryFilter facetFilter;
  HalMemory::PsramBuffer facetChoices;
  uint16_t facetCount = 0;
  enum class PendingMenu : int8_t { None = -1, Settings, Rebuild };
  PendingMenu pendingFilterMenu = PendingMenu::None;
  char filterStatus[128]{};
  library::Facet facetKind = library::Facet::Author;
  std::string facetNames[9];

  // Walk the card and write a fresh index. Blocking, with a popup: at ~70 books
  // it is well under a second, and it only runs when the index is missing or the
  // user asks.
  bool rebuildIndex();

  // Input
  void openSelectedBook();
  void openSearch();
  // Shared tail of row activation and the options menu's Open entry.
  void openBookByPath(const std::string& path);
  void promptRebuildIndex();
  void resetAfterRebuild();
  // Recent-row long-press menu: open / remove from recents / delete / rebuild.
  void showRecentBookOptions(int entry);
  void promptRemoveRecentBook(const std::string& path, const std::string& title);
  // Long-press delete owns the gesture where grouping does not apply: the
  // Recent sort, degraded lists, and any active search result.
  bool deleteEligible() const;
  // Resolves the row's path and title, then confirms via promptDeleteBookByPath.
  void promptDeleteBook(int entry);
  void promptDeleteBookByPath(const std::string& path, const std::string& title);
  bool collapseGroups(int bookEntry);
  void expandGroup(int groupEntry);
  void restoreExpandedList();
  void selectTab(int index, bool toggleIfActive);
  void toggleSortDirection();
  // Sub-screens act on button press, so a button still held when we resume must
  // not also act here. Records what to swallow on the next release.
  void swallowHeldReleases();
  // Staged back-out shared by Button::Back and the header back arrow.
  void handleBackAction();
  static void searchActionTrampoline(const freeink::ui::ActionEvent& event, void* user);
  static void rebuildActionTrampoline(const freeink::ui::ActionEvent& event, void* user);
  static void backActionTrampoline(const freeink::ui::ActionEvent& event, void* user);

  // Data
  void applyFilter();
  int bookRowCount() const;
  int rowFor(int entry) const;
  // fileName, when asked for, is the on-card name the row's icon derives from
  // (the display title may come from metadata and carry no extension).
  bool rowTextFor(int entry, std::string& title, std::string& author, std::string* fileName = nullptr,
                  uint64_t* bytes = nullptr);
  uint32_t titleInitialFor(int entry);
  bool buildGroupStarts();
  int groupForBook(int bookEntry) const;
  bool groupable() const;

  // Screen building
  void buildHeader(UiScreen& screen);
  // Materializes ListItems and their strings for the visible window only.
  void buildRows(UiScreen& screen);
  void buildGrid(UiScreen& screen);
  static constexpr freeink::ui::ActionId ACTION_GRID_PAGE = ACTION_VIEW;
  bool gridView() const;
  uint8_t& recentViewSetting() const;
  uint8_t& expandedListSetting() const;
  int gridPageSize() const;
  void toggleView();
  bool loadPendingCover();
  void ensureGridBuffer();
  void invalidateGrid();
  static constexpr size_t GRID_CACHE_BYTES = 40 * 1024;
  HalMemory::PsramBuffer gridCache;
  freeink::ui::Rect gridArea{};
  int gridFirst = -1, gridTotal = -1;
  uint16_t gridMissingCovers = 0;
  uint8_t gridOrientation = 0;
  bool gridCacheValid = false;
  bool gridInverted = false;
  std::atomic<bool> coverWorkRequested{false};
  std::atomic<bool> presentationPending{true};
  uint32_t coverWorkPaint = 0;
  unsigned long lastNavigationAt = 0;
  std::atomic<bool> buttonCursorVisible{false};
  HalFile gridCoverFile;
  Bitmap gridCoverBitmap{gridCoverFile};
  std::string pendingCover, gridFileName, gridPath;
  library::ClixRecord gridRecord{};
  int pendingCoverHeight = 0;
  uint32_t coverAttempts = 0;
  static void formatInitialHeading(uint32_t initial, std::string& out);
  void formatAuthorHeading(const std::string& author, std::string& out) const;
  void drawPositionReadout() const;
  void drawHoldHelp() const;
  const char* headerTitle() const override;

  // Ring 0 is the strip; the selected BOOK is ring - 1, with the strip keeping
  // row 0 as the working selection exactly as the pre-ring code did.
  int selectedEntry() const;
  bool tabsFocused() const { return ringPos() == 0; }

  // --- pinned recently-opened overlay ---------------------------------------
  // On the unfiltered Recent shelf the RecentBooksStore entries sit on top, in
  // read order; the modification-time list follows with those books skipped.
  // Entries below pinnedCount() are store rows; the rest go through rowFor().
  int pinnedCount() const;
  int storeIndexFor(int entry) const;
  // Re-match the store against the index (chunked scan). Call whenever the
  // index or the store changes.
  void resolvePinned();
  // Direction-space translation of the matched rows. Call on sort toggles.
  void refreshOverlap();

  library::LibraryIndexFile index;
  int activeTabIndex = 0;
  library::SortOrder sortOrder = library::SortOrder::RecentDesc;
  // One bit per tab; Recent starts descending (newest first).
  uint8_t descendingTabs = 1u << 0;
  // Set when the walk finished but the sort did not, so the screen can say the
  // order is discovery order rather than silently showing a wrong one.
  bool degraded = false;

  // Rows surviving the current query, as positions in the active sort order.
  // Empty query means no filtering and this owns no allocation, so the ordinary
  // shelf pays nothing proportional to the library for the feature.
  std::string query;
  // The active query, pre-quoted for the header: headerTitle() returns a
  // stable c_str the render task can hold across a build.
  std::string headerSearchTitle;
  std::unique_ptr<uint16_t[]> filtered;
  uint16_t filteredCount = 0;
  bool filterFailed = false;

  // One start row per group. Grouping is only offered for the sorted <=512-book
  // index, so this fallible allocation is at most 1 KiB and is reused after its
  // first successful allocation.
  std::unique_ptr<uint16_t[]> groupStarts;
  uint16_t groupCapacity = 0;
  uint16_t groupCount = 0;
  bool groupsCollapsed = false;
  freeink::ui::ListNav expandedNav;

  // Visible-window row storage, reused across renders (buildRows). Bounded by
  // the densest page, never by the library. Headings get their own storage:
  // the surname-first inversion must not overwrite the author slot, whose raw
  // value the next row's group comparison reads.
  int windowTop = -1, windowTotal = -1;
  size_t windowCapacity = 0;
  bool windowReadFailed = false;
  std::vector<freeink::ui::ListItem> winItems;
  static constexpr size_t MAX_VISIBLE_WINDOW = 24;
  bool windowLight = false;
  std::vector<std::string> winTitles;
  std::vector<std::string> winAuthors;
  std::vector<std::string> winHeaders;

  // Pinned overlay state: per store entry its RecentAsc row (0xFFFF when the
  // book is not in the index), and the current-direction rows to skip, sorted
  // ascending, so unpinned entries map to sort rows with a <=10-step walk.
  uint16_t pinnedAscRows[RecentBooksStore::MAX_RECENT_BOOKS] = {};
  uint16_t overlapRows[RecentBooksStore::MAX_RECENT_BOOKS] = {};
  uint8_t pinnedTotal = 0;
  uint8_t overlapCount = 0;

  bool lockNextConfirmRelease = false;
  bool lockNextBackRelease = false;

  // Row options modal (Recent long-press menu); owned here so it outlives the
  // touch event that opened it.
  OptionPopup optionPopup;
  // Fixed chrome buffers; the existing window vectors still own row strings.
  libraryui::HeaderWork pixelHeader;
  freeink::ui::TabItem pixelTabs[3]{};
  char pixelTabLabels[3][32]{};
  char pixelTitle[192]{};
};
