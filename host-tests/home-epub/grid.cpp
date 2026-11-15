#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
namespace fui {
struct Rect {
  int x = 0, y = 0, width = 0, height = 0;
  int right() const { return x + width; }
  int bottom() const { return y + height; }
};
Rect makeRect(int x, int y, int w, int h) { return {x, y, w, h}; }
enum class Color { Black, White, DarkGray };
struct Paint {
  bool black;
  static Paint solid(Color c) { return {c == Color::Black}; }
  static Paint dither(Color) { return {true}; }
};
enum class TextAlign { Left, Center };
struct TextStyle {
  int font = 0;
  Color color = Color::Black;
};
struct DeviceContext {
  int width = 480, height = 800;
  Rect safeRect() const { return {0, 0, width, height}; }
};
struct DrawTarget {
  int width = 480, height = 800;
  std::vector<uint8_t> pixels = std::vector<uint8_t>(800 * 800);
  void fill(Rect r, Paint p) {
    for (int y = std::max(0, r.y); y < std::min(height, r.bottom()); ++y)
      for (int x = std::max(0, r.x); x < std::min(width, r.right()); ++x) pixels[y * width + x] = p.black;
  }
  Rect measureText(int, const char* s, TextStyle) { return {0, 0, int(strlen(s)) * 7, 16}; }
  int lineHeight(int) { return 16; }
};
constexpr int InputTouch = 1;
}  // namespace fui
using Rect = fui::Rect;
struct Renderer : fui::DrawTarget {
  int orientation = 0;
  int getOrientation() const { return orientation; }
};
struct Frame {
  void hit(Rect, int, int, int) {}
};
struct Screen {
  fui::DeviceContext device_;
  Frame frame_;
  auto& device() { return device_; }
  auto& frame() { return frame_; }
};
namespace homeui {
#include "shelf-layout.inc"
}
namespace toybox {
int interfaceFontStyle() { return 1; }
}  // namespace toybox
namespace pixelui {
void copyCaps(char* out, size_t n, const char* s) { snprintf(out, n, "%s", s); }
}  // namespace pixelui
struct {
  bool screenInverted = false;
} SETTINGS;
constexpr int STR_HOME_NEW_BOOK = 1, SELECT = 1;
const char* tr(int) { return "NUEVO"; }
namespace homecover {
std::vector<uint8_t> scene;
uint64_t key_ = 0;
bool valid = false;
unsigned hits = 0;
unsigned attempts = 0;
bool restoreScene(Renderer& r, Rect, uint64_t key) {
  ++attempts;
  if (valid && key == key_) {
    r.pixels = scene;
    ++hits;
    return true;
  }
  key_ = key;
  return false;
}
void storeScene(Renderer& r) {
  scene = r.pixels;
  valid = true;
}
}  // namespace homecover
#include "grid-helpers.inc"
struct Book {
  std::string path, title;
};
struct Covers {
  Renderer& renderer;
  unsigned reads = 0;
  bool present = true;
  void paint(Rect a, int index, const std::string&, bool, bool) {
    ++reads;
    renderer.fill(a, fui::Paint::solid(fui::Color::Black));
    for (int y = a.y; y < a.bottom(); y += 7)
      renderer.fill({a.x + index, y, std::max(1, a.width - index * 2), 2}, fui::Paint::solid(fui::Color::White));
  }
  bool hasImage(int) const { return present; }
};
struct Gui {
  std::array<Rect, 7> areas{};
  unsigned placeholders = 0;
  void drawCoverPlaceholder(Renderer& r, Rect a) {
    assert(placeholders < areas.size());
    areas[placeholders++] = a;
    r.fill(a, fui::Paint::solid(fui::Color::White));
  }
} GUI;
class Grid {
 public:
  Renderer renderer;
  Screen screen;
  Covers coverCache{renderer};
  std::vector<Book> data;
  const std::vector<Book>* books = &data;
  std::array<std::string, 7> coverPaths{};
  std::array<Rect, 7> coverAreas{};
  int bookProgress[7]{};
  bool allCoversReady = true;
  bool loadingShell = false;
  void noteThumbHeight(int) {}
  void label(Rect a, const char* s, int = 0, bool inverse = false, fui::TextAlign = fui::TextAlign::Left) {
    // Deterministic glyph strokes make overlap and erase-order differences observable.
    for (int n = 0; s[n] && n * 7 < a.width; ++n)
      for (int y = 0; y < 12; ++y)
        for (int x = 0; x < 5; ++x)
          if ((unsigned(s[n]) + x + y) % 3)
            renderer.fill({a.x + n * 7 + x, a.y + 6 + y, 1, 1},
                          fui::Paint::solid(inverse ? fui::Color::White : fui::Color::Black));
  }
  void baseline() {
    auto& target = renderer;
#include "grid-baseline.inc"
  }
  void current() {
    auto& target = renderer;
#include "grid-current.inc"
  }
};
int main() {
  unsigned cases = 0;
  for (bool wide : {false, true})
    for (int orientation = 0; orientation < 4; ++orientation)
      for (bool inverted : {false, true}) {
        Grid grid;
        grid.screen.device_ = {wide ? 800 : 480, wide ? 480 : 800};
        grid.renderer.width = grid.screen.device_.width;
        grid.renderer.height = grid.screen.device_.height;
        grid.renderer.orientation = orientation;
        SETTINGS.screenInverted = inverted;
        for (int n = 0; n < 7; ++n) {
          grid.data.push_back({"book" + std::to_string(n), "Titulo" + std::to_string(n)});
          grid.coverPaths[n] = "cover" + std::to_string(n);
        }
        for (int percent : {-1, 0, 9, 50, 100}) {
          std::fill_n(grid.bookProgress, 7, percent);
          homecover::valid = false;
          std::fill(grid.renderer.pixels.begin(), grid.renderer.pixels.end(), 0);
          grid.baseline();
          const auto expected = grid.renderer.pixels;
          homecover::valid = false;
          std::fill(grid.renderer.pixels.begin(), grid.renderer.pixels.end(), 0);
          grid.current();
          assert(expected == grid.renderer.pixels);
          const auto reads = grid.coverCache.reads;
          std::fill(grid.renderer.pixels.begin(), grid.renderer.pixels.end(), 0);
          grid.current();
          assert(expected == grid.renderer.pixels);
          assert(reads == grid.coverCache.reads);
          std::fill_n(grid.bookProgress, 7, percent + 1);
          std::fill(grid.renderer.pixels.begin(), grid.renderer.pixels.end(), 0);
          grid.current();
          const auto changed = grid.renderer.pixels;
          assert(reads == grid.coverCache.reads);
          homecover::valid = false;
          std::fill(grid.renderer.pixels.begin(), grid.renderer.pixels.end(), 0);
          grid.baseline();
          assert(changed == grid.renderer.pixels);
          const auto attempts = homecover::attempts;
          const auto shellReads = grid.coverCache.reads;
          GUI.placeholders = 0;
          grid.data.clear();
          grid.loadingShell = true;
          std::fill(grid.renderer.pixels.begin(), grid.renderer.pixels.end(), 0);
          grid.current();
          assert(grid.coverCache.reads == shellReads && homecover::attempts == attempts);
          const auto layout = homeui::shelfLayout(grid.screen.device_);
          assert(GUI.placeholders == unsigned(layout.rows * 3 + 1) && !grid.allCoversReady);
          for (unsigned n = 0; n < GUI.placeholders; ++n) {
            const auto area = GUI.areas[n];
            assert(area.width > 0 && area.height > 0 && area.x >= 0 && area.y >= 0);
            assert(area.right() <= grid.renderer.width && area.bottom() <= grid.renderer.height);
            if (n < GUI.placeholders - 1) {
              const auto actual = grid.coverAreas[n + 1];
              assert(actual.x == area.x && actual.y == area.y && actual.width == area.width &&
                     actual.height == area.height);
            }
          }
          grid.loadingShell = false;
          grid.current();
          assert(GUI.placeholders == unsigned(layout.rows * 3 + 1));
          for (int n = 0; n < 7; ++n) grid.data.push_back({"book" + std::to_string(n), "Titulo" + std::to_string(n)});
          ++cases;
        }
      }
  std::printf(
      "Shelf production grid: %u layout/progress/orientation/inversion cases identical; zero art reads after badge "
      "changes\n",
      cases);
}
