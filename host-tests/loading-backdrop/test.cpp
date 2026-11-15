#include <FreeInkUIDisplayTarget.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "../../lib/GfxRenderer/ChromeRegion.h"
#include "../../src/components/PopupLayout.h"
namespace fui = freeink::ui;
struct Rect {
  int x, y, width, height;
};
class GfxRenderer {
 public:
  int width, height;
  fui::Orientation orientation;
  bool inverted;
  mutable const char* message = nullptr;
  mutable std::vector<uint8_t> pixels, shown;
  mutable unsigned paints = 0;
  GfxRenderer(fui::Orientation o, bool inv)
      : width(o == fui::Orientation::Portrait || o == fui::Orientation::PortraitInverted ? 480 : 800),
        height(800 * 480 / width),
        orientation(o),
        inverted(inv),
        pixels(800 * 480 / 8, 255) {}
  void clearScreen() const {
    std::fill(pixels.begin(), pixels.end(), 255);
    chrome::beginFrame();
  }
  int getScreenWidth() const { return width; }
  int getScreenHeight() const { return height; }
  void displayBuffer() const {
    ++paints;
    shown = pixels;
    if (chrome::invertHeaderForOutput(inverted)) {
      std::vector<uint8_t> band(pixels.size(), 255);
      fui::DisplayTarget target(band.data(), 800, 480, 100, orientation);
      target.fill(fui::makeRect(0, 0, width, chrome::headerHeight), fui::Paint::solid(fui::Color::Black));
      for (size_t n = 0; n < band.size(); ++n) shown[n] ^= uint8_t(~band[n]);
    }
  }
};
namespace freeink::ui {
class GfxRendererTarget {
  DisplayTarget target;

 public:
  explicit GfxRendererTarget(const GfxRenderer& r) : target(r.pixels.data(), 800, 480, 100, r.orientation) {}
  void fill(Rect r, Paint p, uint8_t radius = 0) { target.fill(r, p, radius, CornersAll); }
  void stroke(Rect r, Paint p, uint8_t width, uint8_t radius = 0) { target.stroke(r, p, width, radius, CornersAll); }
};
}  // namespace freeink::ui
class BaseTheme {
 public:
  Rect drawLoadingScreen(const GfxRenderer&, const char*) const;
  Rect drawPopup(const GfxRenderer& r, const char* message) const {
    r.message = message;
    fui::DisplayTarget target(r.pixels.data(), 800, 480, 100, r.orientation);
    const auto popup = popupui::layout(target, fui::makeRect(0, 0, r.width, r.height), message, {}, 12, 12, 2,
                                       r.height * 75 / 1000, chrome::headerHeight);
    popupui::paintChrome(target, popup, 2, 0);
    const auto box = popup.body;
    r.displayBuffer();
    return {box.x, box.y, box.width, box.height};
  }
};
#include "loading-under-test.inc"
int main() {
  unsigned checks = 0, failed = 0;
  for (auto orientation : {fui::Orientation::Portrait, fui::Orientation::LandscapeClockwise,
                           fui::Orientation::PortraitInverted, fui::Orientation::LandscapeCounterClockwise})
    for (bool light : {false, true})
      for (bool incomingLight : {false, true})
        for (bool inverted : {false, true})
          for (int initial = 0; initial < 5; ++initial) {
            GfxRenderer renderer(orientation, inverted);
            chrome::lightHeader = light;
            chrome::beginFrame();
            chrome::headerHeight = initial == 4 ? 0 : 84;
            if (initial == 1) std::fill(renderer.pixels.begin(), renderer.pixels.end(), 0x55);
            if (initial == 2) {
              for (size_t n = 0; n < renderer.pixels.size(); ++n) renderer.pixels[n] = uint8_t(n * 37 + 13);
            }
            if (initial == 3) std::fill(renderer.pixels.begin(), renderer.pixels.end(), 0);
            renderer.displayBuffer();
            const auto before = renderer.pixels, shownBefore = renderer.shown;
            const int header = chrome::headerHeight;
            chrome::lightHeader = incomingLight;
            const auto popup = BaseTheme{}.drawLoadingScreen(renderer, "Cargando");
            fui::GfxRendererTarget progress(renderer);
            progress.fill(fui::makeRect(popup.x + 4, popup.y + popup.height - 8, popup.width / 2, 2),
                          fui::Paint::solid(fui::Color::Black));
            renderer.displayBuffer();
            std::vector<uint8_t> outside(renderer.pixels.size(), 255);
            fui::DisplayTarget maskTarget(outside.data(), 800, 480, 100, orientation);
            maskTarget.fill(fui::makeRect(popup.x - 2, popup.y - 2, popup.width + 4, popup.height + 4),
                            fui::Paint::solid(fui::Color::Black));
            bool unchanged = true, outputUnchanged = true;
            for (size_t offset = 0; offset < outside.size(); ++offset) {
              unchanged &= ((before[offset] ^ renderer.pixels[offset]) & outside[offset]) == 0;
              outputUnchanged &= ((shownBefore[offset] ^ renderer.shown[offset]) & outside[offset]) == 0;
            }
            const bool pass[] = {unchanged,
                                 outputUnchanged,
                                 chrome::headerHeight == header,
                                 renderer.paints == 3,
                                 chrome::framebufferLightHeader == light,
                                 chrome::lightHeader == incomingLight,
                                 popup.y - 2 >= header,
                                 renderer.message && std::strcmp(renderer.message, "Cargando") == 0,
                                 popup.x >= 0 && popup.y >= 0 && popup.x + popup.width <= renderer.width &&
                                     popup.y + popup.height <= renderer.height};
            for (bool ok : pass) {
              ++checks;
              if (!ok) ++failed;
            }
            if (!unchanged || !outputUnchanged)
              std::printf("FAIL: orientation=%d light=%d inverted=%d background=%d changed outside message\n",
                          int(orientation), light, inverted, initial);
            chrome::beginFrame();
            ++checks;
            if (chrome::headerHeight != 0 || chrome::framebufferLightHeader != incomingLight) ++failed;
          }
  std::printf("loading overlay: %u checks, %u failures\n", checks, failed);
  return failed ? 1 : 0;
}
