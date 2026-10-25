#pragma once

#include <Bitmap.h>
#include <HalStorage.h>

#include "../../components/UiAppHost.h"
#include "BookCoverPainter.h"
#include "HomeScreen.h"

class GfxRenderer;
class MappedInputManager;
struct RecentBook;

class HomeDashboardUi : public UiAppHost {
 public:
  explicit HomeDashboardUi(GfxRenderer& renderer)
      : UiAppHost(renderer), renderer(renderer), covers(renderer, uiTarget) {}
  void begin();
  bool pollDate();
  bool draw(const RecentBook* book, bool librarySlot, bool plugins, int selected, int recentIndex = 0,
            int recentCount = 1, const RecentBook* const* neighbors = nullptr);
  void paintCover(const RecentBook& book);
  void finishCover();
  bool canRepaintSelection(int selected);
  bool repaintSelection(int selected);
  int selectedAction(const MappedInputManager& input);
  const homeui::Layout& geometry() const { return layout; }

 private:
  GfxRenderer& renderer;
  homeui::Model model;
  homeui::Layout layout;
  freeink::ui::HeaderProps status;
  freeink::ui::ButtonProps hit;
  BookCoverPainter covers;
  int pending = -1;
  const RecentBook* neighbors[4]{};
  std::string progressPath;

  unsigned long lastDatePoll = 0;
  int orientation = -1;
  void readDate(char* out, size_t size, char* clock, size_t clockSize) const;
  void addRow(const char* label, homeui::Icon icon);
  static void screenFn(UiScreen& screen, void* user);
  static void onAction(const freeink::ui::ActionEvent& event, void* user);
};
