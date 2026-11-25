#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "HomeCoverCache.h"
#include "RecentBooksStore.h"
#include "UiAppHost.h"
#include "components/bars/tab-bar.h"
#include "components/media/book-card.h"
#include "components/media/cover-grid.h"

class CoverGridHomeUi final : public UiAppHost {
 public:
  static constexpr int THUMB_HEIGHT = 400;
  static constexpr int GRID_COLUMNS = 3;
  static constexpr int GRID_ROWS = 2;
  static constexpr int MAX_BOOKS = 1 + GRID_COLUMNS * GRID_ROWS;
  static_assert(MAX_BOOKS <= HomeCoverCache::MAX_COVERS);
  explicit CoverGridHomeUi(GfxRenderer& renderer);
  void begin(const std::vector<RecentBook>& books, bool hasOpds, bool hasContinueReading, bool plugins = false);
  void refreshCoverPaths();
  void refreshCoverPath(size_t index);
  void prepareGeometry();
  void setCoverLoading(bool value) { coverLoading = value; }
  void setLoadingShell(bool value) { loadingShell = value; }
  bool coversReady() const { return allCoversReady; }
  bool coverNoticeVisible() const { return noticeVisible; }
  freeink::ui::Rect coverArea(size_t index) const {
    return index < coverAreas.size() ? coverAreas[index] : freeink::ui::Rect{};
  }
  void setSelection(int selection) { selected = selection; }
  void showDrawer() {
    drawer = true;
    resetUi();
  }
  bool closeDrawer() {
    if (!drawer) return false;
    drawer = false;
    resetUi();
    return true;
  }
  int selectedAction(const MappedInputManager& input);
  // Exact generation height, shared by every slot and recorded during draw.
  // Thumbs must be generated at the drawn size: rescaling a dithered 1-bit
  // image aliases badly.
  int thumbHeightFor() const;
  static int preparationThumbHeight(GfxRenderer& renderer);
  static constexpr int generatedThumbHeight(int slotHeight) { return std::max(1, slotHeight + 8); }
  bool takeThumbHeightChanged();
  static constexpr int PAGE_PREVIOUS = -6, PAGE_NEXT = -7, MENU_TOGGLE = -4, MENU_IGNORE = -8;
  void setPage(int current, int total) {
    page = current;
    pages = total;
  }

 private:
  static void screenFn(UiScreen& screen, void* user);
  static void onAction(const freeink::ui::ActionEvent& event, void* user);
  void draw(UiScreen& screen);
  void drawHeaderBand();
  void drawLicorice(UiScreen& screen);
  void drawEmpty(UiScreen& screen);
  void drawCurrent(UiScreen& screen, freeink::ui::Rect rect, int coverRowHeight);
  void drawGrid(UiScreen& screen);
  freeink::ui::Rect layoutGrid(freeink::ui::Rect rect);
  void drawTabs(UiScreen& screen, freeink::ui::Rect rect);
  bool paintFramedCover(freeink::ui::DrawTarget& target, freeink::ui::Rect rect, size_t index);
  void noteThumbHeight(int slotHeight);
  bool paintCover(freeink::ui::Rect rect, size_t index, bool flat = false);

  HomeCoverCache coverCache;
  GfxRenderer& renderer;
  const std::vector<RecentBook>* books = nullptr;
  std::array<std::string, MAX_BOOKS> coverPaths;
  int thumbHeight = 0;
  bool thumbHeightChanged = false;
  int selected = 0;
  int page = 0, pages = 1;
  bool drawer = false;
  bool coverLoading = false, allCoversReady = false, loadingShell = false, noticeVisible = false;
  std::array<freeink::ui::Rect, MAX_BOOKS> coverAreas{};
  int bookProgress[MAX_BOOKS]{};
  int shelfProgress(const RecentBook& book);
  int pending = -1;
  int progress = -1;
  uint64_t readingSeconds = 0;
  bool readingTimeLoaded = false, readingTimeKnown = false;
  bool hasOpds = false;
  bool hasPlugins = false;
  bool hasContinueReading = false;
  char progressText[12]{};
  freeink::ui::HeaderProps shelfStatus{};
  // Component styles and interaction tables stay off the render task's stack.
  freeink::ui::BookCardProps card;
  freeink::ui::CoverGridProps grid;
  freeink::ui::Rect gridBounds{};
  freeink::ui::TabBarProps tabs;
  // fork-local seam: room for the shelf's folders (GAMES, APPS) after
  // upstream's five tabs. See drawTabs().
  static constexpr int SHELF_TABS = 2;
  std::array<freeink::ui::TabItem, 5 + SHELF_TABS> tabItems;
};
