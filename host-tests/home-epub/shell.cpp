#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <utility>

struct RenderLock {};
struct Rect {
  int x = 0, y = 0, width = 480, height = 800;
};
#include "home-notice-area.inc"
struct CrossPointSettings {
  static constexpr int HOME_FRAMED = 0, HOME_LICORICE = 3, HOME_LAYOUT_COUNT = 4;
  int homeLayout = 0;
  uint8_t homeCoverLoadingMask = 1u << HOME_LICORICE;
  bool homeCoverNotice(uint8_t theme) const {
    return theme < HOME_LAYOUT_COUNT && (homeCoverLoadingMask & (1u << theme));
  }
} SETTINGS;
struct Renderer {
  unsigned clears = 0, presents = 0;
  void clearScreen() { ++clears; }
  void displayBuffer(int) { ++presents; }
  auto& deviceContext() { return *this; }
  Rect safeRect() { return {}; }
};
namespace HalDisplay {
constexpr int HALF_REFRESH = 1, FAST_REFRESH = 2;
}
namespace toybox {
Renderer& makeTarget(Renderer& renderer) { return renderer; }
}  // namespace toybox
struct Grid {
  bool loading = false;
  Rect coverArea(int) { return {20, 100, 120, 180}; }
  void setLoadingShell(bool value) { loading = value; }
};
struct UITheme {
  unsigned shellPaints = 0;
  static UITheme& getInstance() {
    static UITheme value;
    return value;
  }
  void drawCoverGridHome(Grid& grid) {
    assert(grid.loading);
    ++shellPaints;
  }
};
constexpr int STR_LIBRARY_LOADING_COVERS = 1;
const char* tr(int) { return "Loading covers"; }
namespace libraryui {
unsigned notices = 0;
void loadingNotice(Renderer& r, Rect area, const char*) {
  if (SETTINGS.homeLayout == 0) assert(area.height <= 96);
  assert(r.presents == 0);
  ++notices;
}
}  // namespace libraryui
class HomeActivity {
 public:
  enum class EntryPhase { Blocking, Shell, Preparing, Painting, Ready };
  EntryPhase entryPhase = EntryPhase::Blocking;
  std::unique_ptr<Grid> coverGridUi;
  Renderer renderer;
  bool hasPlugins = false, cleanInitialRefresh = false, firstRenderDone = false;
  bool hasLibrarySlot() { return true; }
  struct Dashboard {
    unsigned paints = 0, placeholders = 0;
    struct Layout {
      Rect card{20, 100, 440, 280};
    } layout;
    const Layout& geometry() const { return layout; }
    void draw(void*, bool, bool, int, int, int, void*, bool shell) {
      assert(shell);
      ++paints;
    }
    void paintPlaceholders() { ++placeholders; }
  } dashboard;
  unsigned initializations = 0, updates = 0;
  void initializeContent() {
    assert(renderer.presents == 0);
    assert(entryPhase == EntryPhase::Blocking);
    ++initializations;
  }
  void requestUpdate() { ++updates; }
  void enter() {
#include "home-init.inc"
  }
  void render(RenderLock&&);
};
#include "home-shell.inc"
int main() {
  for (Rect safe : {Rect{1, 10, 478, 790}, Rect{10, 1, 790, 478}}) {
    const Rect cover{20, 100, 120, 180};
    const auto top = homeui::coverLoadingNoticeArea(safe, cover, true);
    assert(top.x == safe.x && top.width == safe.width && top.y == cover.y && top.height == 96);
    const auto centered = homeui::coverLoadingNoticeArea(safe, cover, false);
    assert(centered.x == safe.x && centered.y == safe.y && centered.height == safe.height);
  }

  for (int theme = 0; theme < 4; ++theme)
    for (bool enabled : {false, true}) {
      SETTINGS.homeLayout = theme;
      SETTINGS.homeCoverLoadingMask = enabled ? uint8_t(1u << theme) : 0;
      HomeActivity h;
      h.enter();
      assert(h.updates == 1);
      assert(h.initializations == unsigned(!enabled));
      assert(h.entryPhase == (enabled ? HomeActivity::EntryPhase::Shell : HomeActivity::EntryPhase::Painting));
    }
  HomeActivity blocked;
  blocked.render(RenderLock{});
  assert(!blocked.renderer.clears && !blocked.renderer.presents);
  for (int theme = 0; theme < 4; ++theme) {
    SETTINGS.homeLayout = theme;
    for (bool grid : {false, true})
      for (auto phase : {HomeActivity::EntryPhase::Shell, HomeActivity::EntryPhase::Preparing}) {
        HomeActivity home;
        home.entryPhase = phase;
        if (grid) home.coverGridUi = std::make_unique<Grid>();
        const auto before = libraryui::notices;
        home.render(RenderLock{});
        assert(home.renderer.clears == 1 && home.renderer.presents == 1);
        assert(libraryui::notices == before + 1);
        assert(home.firstRenderDone && home.entryPhase == HomeActivity::EntryPhase::Preparing);
        if (grid)
          assert(!home.coverGridUi->loading);
        else
          assert(home.dashboard.paints == 1 && home.dashboard.placeholders == 1);
      }
  }
  std::puts("Home loading shell: existing first presentation retained; loading mode cleared before preparation");
}
