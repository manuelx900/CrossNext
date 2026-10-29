#pragma once
#include <functional>
#include <vector>

#include "../../apps_local/home/HomeDashboardUi.h"
#include "../../apps_local/home/HomeMenuInput.h"
#include "../../apps_local/home/HomeMenuIntegration.h"
#include "./FileBrowserActivity.h"
#include "RecentBooksStore.h"
#include "activities/Activity.h"
// Not a forward declaration: the std::vector<RecentBook> member below needs
// the complete type wherever this header's implicit destructor instantiates,
// and the host simulator build (libc++) instantiates it in every including TU.
#include "RecentBooksStore.h"
#include "components/CoverGridHomeUi.h"
#include "util/ButtonNavigator.h"

struct Rect;

class HomeActivity final : public Activity {
  std::unique_ptr<CoverGridHomeUi> coverGridUi;
  ButtonNavigator buttonNavigator{140, 400};
  homeui::MenuInput menuInput;
  HomeDashboardUi dashboard;
  int selectorIndex = 0;
  int recentBookIndex = 0;
  int recentBookCount = 0;
  int recentSourceIndices[RecentBooksStore::MAX_RECENT_BOOKS]{};
  int displayBookCount() const {
    return coverGridUi ? static_cast<int>(recentBooks.size()) : recentBooks.empty() ? 0 : 1;
  }
  bool recentsLoading = false;
  bool recentsLoaded = false;
  bool firstRenderDone = false;
  bool dashboardDirty = true;
  uint32_t lastHomePaint = 0;
  bool hasOpdsServers = false;
  bool hasPlugins = false;
  // The optional services row opens installed Plugins, otherwise configured OPDS.
  bool hasLibrarySlot() const { return hasPlugins || hasOpdsServers; }
  bool hasContinueReading = false;
  bool coverBufferStored = false;          // Track if cover buffer is stored
  std::unique_ptr<uint8_t[]> coverBuffer;  // HomeActivity's own buffer for cover image
  size_t coverBufferSize = 0;              // Bytes allocated to coverBuffer
  // Cover art is reused on full redraws; focus updates repaint only menu rows.
  int coverRectX = 0;
  int coverRectY = 0;
  int coverRectW = 0;
  int coverRectH = 0;
  std::vector<RecentBook> recentBooks;
  const HomeMenuItem initialMenuItem;
  const bool cleanInitialRefresh;

  // Convert HomeMenuItem to menu index (used in onEnter)
  static int menuItemToIndex(HomeMenuItem item, bool hasOpdsUrl) {
    const int index = homemenu::indexOf(homemenu::config(), homemenu::idFor(item), hasOpdsUrl);
    return index < 0 ? 0 : index;
  }

  // Convert menu index to HomeMenuItem (used in loop)
  static HomeMenuItem indexToMenuItem(int idx, bool hasOpdsUrl) {
    return homemenu::item(homemenu::at(homemenu::config(), idx, hasOpdsUrl));
  }
  void onSelectBook(const std::string& path);
  void onFileBrowserOpen();
  void onLibraryOpen();
  void onSettingsOpen();
  void onFileTransferOpen();
  void onOpdsBrowserOpen();
  void onPluginsOpen();

  int getMenuItemCount() const;
  bool storeCoverBuffer();    // Store frame buffer for cover image
  bool restoreCoverBuffer();  // Restore frame buffer from stored cover
  void freeCoverBuffer();     // Free the stored cover buffer
  void loadRecentBooks(int maxBooks);
  bool loadRecentCovers(int coverHeight);
  void fillCoverGridFromLibrary();
  void resolveGridCoverPaths();
  void loadGridCover(RecentBook& book, int height, bool& showingLoading, Rect& popupRect);

 public:
  explicit HomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                        HomeMenuItem initialMenuItemValue = HomeMenuItem::NONE, bool cleanInitialRefresh = false)
      : Activity("Home", renderer, mappedInput),
        dashboard(renderer),
        initialMenuItem(initialMenuItemValue),
        cleanInitialRefresh(cleanInitialRefresh) {
    setInterfaceFontSection(menufont::Section::Home);
  }
  void onEnter() override;
  void onExit() override;
  void loop() override;
  std::optional<uint32_t> nextLoopWakeDelayMs(uint32_t) const override { return 1000; }
  void render(RenderLock&&) override;
  bool isHomeActivity() const override { return true; }
};
