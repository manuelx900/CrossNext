#include <FreeInkUI.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "../../lib/GfxRenderer/RevealedInteractions.h"
#include "../../src/apps_local/ui/LightOptionMenu.h"
#include "../../src/apps_local/ui/PageIndicator.h"

namespace fui = freeink::ui;
enum class Language { EN, ES };
enum StrId { STR_BACK, STR_SELECT, STR_DIR_UP, STR_DIR_DOWN, STR_TITLE };
struct TestI18n {
  const char* get(StrId) const { return "Dictionary"; }
  Language getLanguage() const { return Language::EN; }
} testI18n;
#define I18N testI18n
#define tr(id) I18N.get(id)

struct GfxRenderer {
  int width = 480, height = 800, line = 24;
  fui::Rect safe() const { return fui::makeRect(7, 9, width - 14, height - 16); }
  void displayBuffer() { paintclock::notePainted(); }
};
namespace freeink::ui {
class GfxRendererTarget : public DrawTarget {
 public:
  static constexpr int FONT_BODY = 1;
  explicit GfxRendererTarget(const GfxRenderer& renderer) : renderer(renderer) {}
  void setFont(int, int) {}
  DeviceContext deviceContext() const {
    DeviceContext d;
    d.width = renderer.width;
    d.height = renderer.height;
    d.safeArea = {9, 7, 7, 7};
    d.hasTouch = true;
    return d;
  }
  Size measureText(FontId, const char* text, TextStyle) const override {
    return {clampI16(int(std::strlen(text)) * renderer.line / 2), int16_t(renderer.line)};
  }
  int16_t lineHeight(FontId) const override { return renderer.line; }
  Rect inkBounds(FontId font, const char* text, TextStyle style) const override {
    const auto size = measureText(font, text, style);
    return {0, 0, size.width, size.height};
  }
  Rect clipRect() const override { return {0, 0, int16_t(renderer.width), int16_t(renderer.height)}; }
  bool setClipRect(Rect) override { return false; }
  void fill(Rect, Paint, uint8_t = 0, uint8_t = CornersAll) override {}
  void stroke(Rect, Paint, uint8_t = 1, uint8_t = 0, uint8_t = CornersAll) override {}
  void line(Point, Point, uint8_t, Paint) override {}
  void text(Rect, const char*, TextStyle) override {}
  void triangle(Point, Point, Point, Paint) override {}
  void bitmap(Rect, BitmapRef, BitmapMode, Paint, Rotation) override {}

 private:
  const GfxRenderer& renderer;
};
}  // namespace freeink::ui

struct MappedInputManager {
  enum class Button { NavPrevious, NavNext, Confirm, Back };
  enum class SwipeDir { None, Left, Right, Up, Down };
  fui::InputSnapshot snapshot;
  SwipeDir swipe = SwipeDir::None;
  SwipeDir wasSwipe() const { return swipe; }
  bool wasPressed(Button) const { return false; }
  bool wasReleased(Button) const { return false; }
  struct Labels {
    const char *btn1, *btn2, *btn3, *btn4;
  };
  Labels mapLabels(const char* a, const char* b, const char* c, const char* d) const { return {a, b, c, d}; }
};
fui::InputSnapshot touchSnapshotFrom(const MappedInputManager& input) { return input.snapshot; }
fui::GfxRendererTarget makeUiTarget(const GfxRenderer& renderer) { return fui::GfxRendererTarget(renderer); }
void refreshSharedUiThemeTokens(fui::GfxRendererTarget&) {}
namespace haptic_feedback {
void touchAction() {}
}  // namespace haptic_feedback
namespace pixelui {
struct FontScope {
  FontScope(fui::GfxRendererTarget&, GfxRenderer&, bool) {}
};
}  // namespace pixelui
namespace toybox {
inline constexpr int kTileFontId = 3;
}
struct UITheme {
  struct Metrics {
    int optionPopupInnerPadding = 16, optionPopupItemSpacing = 8, optionPopupDialogSideMargin = 16;
  } metrics;
  static UITheme& getInstance() {
    static UITheme theme;
    return theme;
  }
  const Metrics& getMetrics() const { return metrics; }
  void drawButtonHints(GfxRenderer&, const char*, const char*, const char*, const char*) {}
};
#define GUI UITheme::getInstance()
#define private public
#include "production.inc"
#undef private

namespace {
int checks = 0;
void check(bool condition) {
  ++checks;
  assert(condition);
}
std::vector<std::string> labels(int count) {
  std::vector<std::string> result;
  result.reserve(count);
  for (int i = 0; i < count; ++i) result.push_back("Dictionary " + std::to_string(i) + " extended language name");
  return result;
}
void draw(OptionPopup& popup, GfxRenderer& gfx) {
  popup.render(gfx);
  gfx.displayBuffer();
  check(!popup.interactions.overflowed());
  int visible = 0;
  for (size_t i = 0; i < popup.interactions.publishedCount(); ++i) {
    const auto& hit = popup.interactions.publishedData()[i];
    const auto safe = gfx.safe();
    check(hit.rect.x >= safe.x && hit.rect.right() <= safe.right());
    check(hit.rect.y >= safe.y && hit.rect.bottom() <= safe.bottom());
    if (hit.action == 1) {
      ++visible;
      check(hit.value >= 0 && hit.value < int(popup.ownedStrings.size()));
    }
  }
  check(visible > 0 && visible <= 7);
}
void swipe(OptionPopup& popup, GfxRenderer& gfx, bool next, int& updates) {
  MappedInputManager input;
  input.swipe = next ? MappedInputManager::SwipeDir::Left : MappedInputManager::SwipeDir::Right;
  input.snapshot.touchReleased = true;
  input.snapshot.touchX = input.snapshot.touchY = -1;
  check(popup.handleInput(input, [&] { ++updates; }));
  if (updates) draw(popup, gfx);
}
void tap(OptionPopup& popup, GfxRenderer& gfx, fui::Rect rect) {
  MappedInputManager input;
  input.snapshot.touchX = rect.x + rect.width / 2;
  input.snapshot.touchY = rect.y + rect.height / 2;
  input.snapshot.touchPressed = true;
  int updates = 0;
  popup.handleInput(input, [&] { ++updates; });
  if (updates) draw(popup, gfx);
  input.snapshot.touchPressed = false;
  input.snapshot.touchReleased = true;
  popup.handleInput(input, [] {});
}
}  // namespace

int main() {
  for (bool light : {false, true})
    for (bool wide : {false, true})
      for (int line : {20, 28, 38})
        for (int count : {1, 2, 7, 8, 16, 17, 42, 128, 255}) {
          chrome::lightHeader = light;
          chrome::headerHeight = 80;
          GfxRenderer gfx{wide ? 800 : 480, wide ? 480 : 800, line};
          OptionPopup popup;
          int chosen = -1;
          const auto values = labels(count);
          popup.show(STR_TITLE, values, 0, [&](int value) { chosen = value; });
          draw(popup, gfx);
          const auto panel = popup.interactions.publishedData()[0].rect;
          std::vector<bool> seen(count);
          for (int page = 0; page < count; ++page) {
            for (size_t i = 0; i < popup.interactions.publishedCount(); ++i) {
              const auto& hit = popup.interactions.publishedData()[i];
              if (hit.action == 1) seen[hit.value] = true;
            }
            int updates = 0;
            swipe(popup, gfx, true, updates);
            const auto after = popup.interactions.publishedData()[0].rect;
            check(after.x == panel.x && after.y == panel.y && after.width == panel.width &&
                  after.height == panel.height);
            check(chosen == -1 && popup.isActive());
            if (!updates) break;
          }
          for (bool reachable : seen) check(reachable);
          int unchanged = 0;
          swipe(popup, gfx, true, unchanged);
          check(!unchanged);
          popup.handleButtons(false, false, true, false, [] {});
          check(chosen >= 0 && chosen < count);
          popup.show(STR_TITLE, values, count - 1, [&](int value) { chosen = value; });
          draw(popup, gfx);
          check(popup.selectedIndex == count - 1);
          fui::Rect last;
          for (size_t i = 0; i < popup.interactions.publishedCount(); ++i) {
            const auto& hit = popup.interactions.publishedData()[i];
            if (hit.action == 1 && hit.value == count - 1) last = hit.rect;
          }
          check(last.height > 0);
          tap(popup, gfx, last);
          check(chosen == count - 1);
        }
  for (bool light : {false, true}) {
    chrome::lightHeader = light;
    GfxRenderer gfx{800, 480, 38};
    OptionPopup popup;
    const char* choices[] = {"Cancel", "Accept"};
    popup.showMessage("A long translated configuration title",
                      "A long confirmation message that wraps over several lines and still leaves room for the choices "
                      "and page controls.",
                      choices, 2, 0, [](int) {});
    draw(popup, gfx);
    check(popup.isActive());
    popup.show(STR_TITLE, labels(42), 41, [](int) {});
    check(popup.ownedStrings.size() == 42);
    draw(popup, gfx);
    const int updatesBefore = popup.selectedIndex;
    int updates = 0;
    swipe(popup, gfx, false, updates);
    check(updates > 0 && popup.selectedIndex < updatesBefore);
    popup.show(STR_TITLE, labels(42), 0, [](int) {});
    draw(popup, gfx);
    fui::Rect next;
    for (size_t i = 0; i < popup.interactions.publishedCount(); ++i) {
      const auto& hit = popup.interactions.publishedData()[i];
      if (hit.action == 3 && hit.value > 0) next = hit.rect;
    }
    check(next.height > 0);
    tap(popup, gfx, next);
    check(popup.isActive() && popup.selectedIndex > 0);
  }
  std::printf("Popup pagination: %d geometry, reachability, swipe and selection checks passed\n", checks);
}
