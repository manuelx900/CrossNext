#include <FreeInkApp.h>
#include <FreeInkUIDisplayTarget.h>
#include <LanguagePack.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// This suite exercises built-in translations; SD language loading has its own suite.
bool languagepack::Pack::load(const char*) { return false; }
const char* languagepack::Pack::get(size_t) const { return nullptr; }

#include "../../src/apps_local/ui/QuickSettingsDirtyRegion.h"
#include "../../src/components/UiListLayoutCache.h"

namespace fui = freeink::ui;
int checks = 0, failures = 0;
#define CHECK(x)                                  \
  do {                                            \
    ++checks;                                     \
    if (!(x)) {                                   \
      ++failures;                                 \
      std::printf("FAIL %d: %s\n", __LINE__, #x); \
    }                                             \
  } while (0)

struct Target : fui::DrawTarget {
  mutable int measures = 0;
  uint64_t revision = 1;
  int glyphWidth = 9;
  uint64_t ink = 0;
  fui::Rect clip{0, 0, 32767, 32767};
  uint64_t textMetricsKey(fui::FontId font, const char*, fui::TextStyle) const override {
    return revision ? revision * 10 + font : 0;
  }
  fui::Size measureText(fui::FontId, const char* value, fui::TextStyle style) const override {
    ++measures;
    return {int16_t(std::strlen(value) * (glyphWidth + int(style.bold))), 20};
  }
  int16_t lineHeight(fui::FontId) const override { return 20; }
  fui::Rect clipRect() const override { return clip; }
  bool setClipRect(fui::Rect r) override {
    clip = r;
    return true;
  }
  void stamp(fui::Rect r, uint64_t n) {
    ink = ink * 31 + uint16_t(r.x);
    ink = ink * 31 + uint16_t(r.y);
    ink = ink * 31 + uint16_t(r.width);
    ink = ink * 31 + uint16_t(r.height);
    ink = ink * 31 + n;
  }
  void fill(fui::Rect r, fui::Paint p, uint8_t radius = 0, uint8_t corners = fui::CornersAll) override {
    stamp(r, uint64_t(p.kind) * 17 + uint64_t(p.color) + radius + corners);
  }
  void stroke(fui::Rect r, fui::Paint p, uint8_t w, uint8_t rad = 0, uint8_t corners = fui::CornersAll) override {
    stamp(r, uint64_t(p.kind) + w + rad + corners);
  }
  void line(fui::Point a, fui::Point b, uint8_t w, fui::Paint) override { stamp({a.x, a.y, b.x, b.y}, w); }
  void triangle(fui::Point a, fui::Point b, fui::Point c, fui::Paint) override {
    stamp({a.x, a.y, b.x, b.y}, c.x + c.y);
  }
  void text(fui::Rect r, const char* s, fui::TextStyle st) override {
    stamp(r, st.font);
    if (s)
      while (*s) ink = ink * 31 + uint8_t(*s++);
  }
  void bitmap(fui::Rect r, fui::BitmapRef, fui::BitmapMode, fui::Paint, fui::Rotation) override { stamp(r, 19); }
};
bool same(fui::Rect a, fui::Rect b) { return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height; }
bool same(fui::ListRowLayout a, fui::ListRowLayout b) {
  return a.height == b.height && a.labelWidth == b.labelWidth && a.labelHeight == b.labelHeight &&
         a.subtitleHeight == b.subtitleHeight && a.valueWidth == b.valueWidth && a.labelLines == b.labelLines;
}
int main() {
  Target target;
  std::array<fui::ListLayoutCache::Entry, 96> entries{};
  fui::ListLayoutCache cache;
  cache.bind(entries.data(), entries.size());
  fui::ListProps props;
  props.layoutCache = &cache;
  props.rowHeight = 44;
  props.rowPaddingY = 8;
  props.labelText.maxLines = 2;
  props.subtitleText.maxLines = 2;
  fui::ListItem item;
  item.label = "A very long title that wraps to a second line";
  item.subtitle = "An author with a long name";
  item.value = "epub";
  const auto verify = [&](int width) {
    auto expected = fui::measureListRow(target, nullptr, width, props, item);
    auto actual = fui::cachedListRow(target, nullptr, width, props, item);
    CHECK(same(expected, actual));
    int before = target.measures;
    CHECK(same(actual, fui::cachedListRow(target, nullptr, width, props, item)));
    CHECK(target.measures == before);
  };
  for (int width : {150, 300, 440, 760}) {
    verify(width);
    for (int style = 0; style < 4; ++style) {
      props.labelText.bold = style & 1;
      props.labelText.italic = style & 2;
      verify(width);
    }
    item.toggle = true;
    props.toggleHeight = 42;
    verify(width);
    item.toggle = false;
    item.subtitle = nullptr;
    verify(width);
    item.subtitle = "Autor en espa\xc3\xb1ol";
    verify(width);
    item.value = nullptr;
    verify(width);
    item.value = "A long trailing value";
    verify(width);
    props.balanceWrappedLabelWithValue = !props.balanceWrappedLabelWithValue;
    verify(width);
    ++props.layoutContext;
    verify(width);
    ++target.revision;
    target.glyphWidth += 1;
    verify(width);
  }
  // Providers reuse mutable buffers: pointers alone must never identify content.
  char title[80] = "First title";
  item.label = title;
  verify(200);
  std::strcpy(title, "Another title with very different wrapping behavior");
  verify(200);
  item.label = nullptr;
  verify(200);
  // Variable/unknown font metadata uses the ordinary path every time.
  item.label = title;
  target.revision = 0;
  fui::cachedListRow(target, nullptr, 200, props, item);
  int before = target.measures;
  fui::cachedListRow(target, nullptr, 200, props, item);
  CHECK(target.measures > before);
  target.revision = 1;
  // No arena is equivalent to an uncached measurement.
  fui::ListLayoutCache empty;
  props.layoutCache = &empty;
  CHECK(same(fui::cachedListRow(target, nullptr, 200, props, item),
             fui::measureListRow(target, nullptr, 200, props, item)));
  props.layoutCache = &cache;
  // Cache eviction is bounded and transparent across long lists.
  for (int n = 0; n < 1000; ++n) {
    std::snprintf(title, sizeof(title), "Long variable title %d with wrapping", n);
    verify(150);
  }

  // Full list painting and interactions are unchanged by memoized sizing.
  std::array<fui::ListItem, 12> rows{};
  for (auto& row : rows) {
    row.label = title;
    row.subtitle = "Second line";
  }
  for (int width : {220, 440, 760})
    for (int top : {0, 4, 8}) {
      fui::InteractionBuffer<24> interactions;
      fui::DeviceContext device;
      device.width = width;
      device.height = 400;
      props.items = rows.data();
      props.count = rows.size();
      props.topIndex = top;
      props.rowProvider = nullptr;
      props.layoutCache = nullptr;
      target.ink = 0;
      fui::InputSnapshot input;
      fui::Frame<24> first(target, device, input, interactions);
      fui::list(first, {0, 0, int16_t(width), 360}, props);
      auto ink = target.ink;
      props.layoutCache = &cache;
      target.ink = 0;
      fui::Frame<24> second(target, device, input, interactions);
      fui::list(second, {0, 0, int16_t(width), 360}, props);
      CHECK(target.ink == ink);
    }
  // The app propagates its cache/context to themed list props.
  fui::FreeInkApp<24, 6> app(target, {});
  app.setListLayoutCache(&cache, 71);
  app.setScreen(
      [](auto& screen, void* user) {
        auto p = screen.resolveListProps({});
        CHECK(p.layoutCache == user);
        CHECK(p.layoutContext == 71);
      },
      &cache);
  app.render();

  for (auto language : {Language::EN, Language::ES}) {
    I18N.setLanguage(language);
    for (bool light : {false, true})
      for (bool wide : {false, true}) {
        chrome::lightHeader = light;
        fui::DeviceContext device;
        device.width = wide ? 800 : 480;
        device.height = wide ? 480 : 800;
        const auto layout = quickui::layout(device, true, true, true);
        quickui::Model old, next;
        old.lightOn = next.lightOn = true;
        next.brightness = old.brightness + 1;
        CHECK(same(quickui::dirtyRegion(old, next, layout, device.width), layout.brightness));
        next = old;
        next.warmth = old.warmth + 1;
        CHECK(same(quickui::dirtyRegion(old, next, layout, device.width), layout.warmth));
        next = old;
        std::strcpy(next.time, "10:21");
        CHECK(same(quickui::dirtyRegion(old, next, layout, device.width), layout.header));
        next = old;
        next.battery = 49;
        CHECK(same(quickui::dirtyRegion(old, next, layout, device.width), layout.header));
        next = old;
        next.lightOn = false;
        auto dirty = quickui::dirtyRegion(old, next, layout, device.width);
        CHECK(dirty.y <= layout.tiles.y && dirty.bottom() >= layout.tiles.bottom());
        CHECK(dirty.y <= layout.brightness.y && dirty.bottom() >= layout.warmth.bottom());
        next = old;
        next.warmth = 0;
        dirty = quickui::dirtyRegion(old, next, layout, device.width);
        CHECK(dirty.y <= layout.tiles.y && dirty.bottom() >= layout.warmth.bottom());
        next = old;
        next.bluetoothOn = true;
        CHECK(same(quickui::dirtyRegion(old, next, layout, device.width), layout.tiles));
        CHECK(same(quickui::dirtyRegion(old, old, layout, device.width),
                   fui::makeRect(0, 0, device.width, layout.bottom)));

        // Compose actual SDK pixels: updating only the dirty region must equal a full paint,
        // including changed labels/icons and the untouched underlying page.
        const auto bytes = size_t(device.width / 8) * device.height;
        std::vector<uint8_t> initial(bytes, 0xAA), full(bytes), partial(bytes);
        fui::DisplayTarget pixels(initial.data(), device.width, device.height, device.width / 8,
                                  fui::Orientation::LandscapeCounterClockwise);
        fui::FreeInkApp<64, 8> panel(pixels, device);
        struct State {
          quickui::Model model;
          quickui::Work work;
        } state;
        state.model = old;
        state.model.shadeBackdrop = false;
        panel.setScreen(
            [](auto& screen, void* user) {
              auto& s = *static_cast<State*>(user);
              quickui::build(screen, s.model, s.work);
            },
            &state);
        panel.render();
        for (int change = 0; change < 8; ++change) {
          next = old;
          next.shadeBackdrop = false;
          if (change == 0) next.brightness = 100;
          if (change == 1) next.warmth = 90;
          if (change == 2) next.warmth = 0;
          if (change == 3) next.lightOn = false;
          if (change == 4) std::strcpy(next.time, "23:59");
          if (change == 5) {
            next.battery = 49;
            next.charging = true;
          }
          if (change == 6) {
            next.bluetoothOn = true;
            next.shortcuts[3] = uint8_t(quickui::Shortcut::Bluetooth);
          }
          if (change == 7) next.touchOn = false;
          state.model = next;
          full = initial;
          partial = initial;
          pixels = fui::DisplayTarget(full.data(), device.width, device.height, device.width / 8,
                                      fui::Orientation::LandscapeCounterClockwise);
          panel.render();
          pixels = fui::DisplayTarget(partial.data(), device.width, device.height, device.width / 8,
                                      fui::Orientation::LandscapeCounterClockwise);
          pixels.setClipRect(quickui::dirtyRegion(old, next, layout, device.width));
          panel.render();
          if (partial != full) {
            int left = device.width, top = device.height, right = 0, bottom = 0;
            for (int y = 0; y < device.height; ++y)
              for (int x = 0; x < device.width; ++x) {
                auto at = size_t(y) * (device.width / 8) + x / 8;
                if ((partial[at] ^ full[at]) & (0x80 >> (x % 8))) {
                  left = std::min(left, x);
                  top = std::min(top, y);
                  right = std::max(right, x);
                  bottom = std::max(bottom, y);
                }
              }
            std::printf("pixel diff light=%d wide=%d change=%d bounds=%d,%d..%d,%d\n", light, wide, change, left, top,
                        right, bottom);
          }
          CHECK(partial == full);
          pixels.setClipRect(fui::makeRect(0, 0, device.width, device.height));
        }

        // Sequential steppers cannot leave feedback behind in the other band.
        struct Step {
          fui::FreeInkApp<64, 8>* app;
          State* state;
        } step{&panel, &state};
        const auto onStep = [](const fui::ActionEvent& event, void* user) {
          auto& s = *static_cast<Step*>(user);
          s.app->clearTapFlash();
          auto& value = event.action == quickui::BrightnessStep ? s.state->model.brightness : s.state->model.warmth;
          value = std::clamp(value + event.value, 0, 100);
        };
        panel.on(quickui::BrightnessStep, onStep, &step);
        panel.on(quickui::WarmthStep, onStep, &step);
        state.model = old;
        state.model.shadeBackdrop = false;
        partial = initial;
        pixels = fui::DisplayTarget(partial.data(), device.width, device.height, device.width / 8,
                                    fui::Orientation::LandscapeCounterClockwise);
        panel.render();
        for (auto band : {layout.brightness, layout.warmth, layout.brightness}) {
          const auto previous = state.model;
          fui::InputSnapshot tap;
          tap.touchX = band.right() - 26;
          tap.touchY = band.y + (light ? 64 : 81);
          tap.touchPressed = true;
          panel.route(tap);
          tap.touchPressed = false;
          tap.touchReleased = true;
          CHECK(bool(panel.route(tap)));
          pixels.setClipRect(quickui::dirtyRegion(previous, state.model, layout, device.width));
          panel.render();
          full = initial;
          pixels = fui::DisplayTarget(full.data(), device.width, device.height, device.width / 8,
                                      fui::Orientation::LandscapeCounterClockwise);
          panel.render();
          CHECK(partial == full);
          pixels = fui::DisplayTarget(partial.data(), device.width, device.height, device.width / 8,
                                      fui::Orientation::LandscapeCounterClockwise);
        }
      }
  }
  // Optional shared storage never falls back to internal memory or retries per paint.
  UiListLayoutCache owner;
  owner.prepare();
  CHECK(owner.get() != nullptr);
  auto attempts = HalMemory::attempts;
  owner.prepare();
  CHECK(HalMemory::attempts == attempts);
  HalMemory::unavailable = true;
  UiListLayoutCache unavailable;
  unavailable.prepare();
  CHECK(!unavailable.get());
  attempts = HalMemory::attempts;
  unavailable.prepare();
  CHECK(HalMemory::attempts == attempts);
  std::printf("%d checks, %d failures; geometry arena %zu bytes\n", checks, failures, UiListLayoutCache::BYTES);
  return failures ? 1 : 0;
}
