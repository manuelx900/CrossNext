#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/apps_local/hearts/HeartsScreens.h"
#include "../../src/apps_local/solitaire/SolitaireScreens.h"
#include "../../src/apps_local/ui/CardPanels.h"
#include "../../src/components/UiControlChrome.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../src/apps_local/ui/fonts/toybox_10.h"
#include "../../src/apps_local/ui/fonts/toybox_14.h"
#include "../../src/apps_local/ui/fonts/toybox_20.h"
#include "../../src/apps_local/ui/fonts/toybox_30.h"
#pragma GCC diagnostic pop
namespace fui = freeink::ui;
namespace {
int checks = 0, failures = 0;
#define CHECK(x)                                  \
  do {                                            \
    ++checks;                                     \
    if (!(x)) {                                   \
      ++failures;                                 \
      std::printf("FAIL %d: %s\n", __LINE__, #x); \
    }                                             \
  } while (0)
struct Font {
  std::array<fui::FontGlyph, 95> glyphs{};
  fui::BitmapFont font{};
  explicit Font(const EpdFontData& source) {
    for (size_t n = 0; n < glyphs.size(); ++n) {
      const auto& g = source.glyph[n];
      glyphs[n] = {static_cast<uint16_t>(g.dataOffset),
                   g.width,
                   g.height,
                   static_cast<uint8_t>(fp4::toPixel(g.advanceX)),
                   static_cast<int8_t>(g.left),
                   static_cast<int8_t>(-g.top)};
    }
    font = {source.bitmap, glyphs.data(), 32, 126, source.advanceY, static_cast<uint8_t>(source.ascender), 64, 64, 1};
  }
};

struct Draw {
  fui::Rect box;
  std::string text;
  fui::TextStyle style;
};
class Target : public fui::DrawTarget {
 public:
  fui::DisplayTarget real;
  std::vector<Draw> texts;
  std::vector<fui::Rect> marks;
  Target(uint8_t* pixels, int w, int h) : real(pixels, w, h, (w + 7) / 8, fui::Orientation::LandscapeCounterClockwise) {
    texts.reserve(128);
    marks.reserve(128);
  }
  fui::Size measureText(fui::FontId f, const char* s, fui::TextStyle st) const override {
    return real.measureText(f, s, st);
  }
  int16_t lineHeight(fui::FontId f) const override { return real.lineHeight(f); }
  fui::Rect inkBounds(fui::FontId f, const char* s, fui::TextStyle st) const override {
    auto size = measureText(f, s, st);
    const auto* cut = toybox::cutForLineHeight(lineHeight(f));
    return fui::makeRect(0, cut ? cut->ascender - cut->inkHeight : 0, size.width, cut ? cut->inkHeight : lineHeight(f));
  }
  void text(fui::Rect r, const char* s, fui::TextStyle st) override { texts.push_back({r, s ? s : "", st}); }
  void fill(fui::Rect r, fui::Paint, uint8_t radius, uint8_t) override {
    if (r.bottom() <= chrome::headerHeight) CHECK(radius == 0);
  }
  void stroke(fui::Rect r, fui::Paint, uint8_t, uint8_t radius, uint8_t) override {
    if (r.bottom() <= chrome::headerHeight) {
      CHECK(radius == 0);
      if (r.width < 200) marks.push_back(r);
    }
  }
  void line(fui::Point, fui::Point, uint8_t, fui::Paint) override {}
  void triangle(fui::Point, fui::Point, fui::Point, fui::Paint) override {}
  void bitmap(fui::Rect r, fui::BitmapRef, fui::BitmapMode, fui::Paint, fui::Rotation) override {
    if (r.bottom() <= chrome::headerHeight) marks.push_back(r);
  }
};
fui::Rect ink(const Target& target, const Draw& draw) {
  const auto metrics = target.inkBounds(draw.style.font, draw.text.c_str(), draw.style);
  int width = std::min(int(metrics.width), int(draw.box.width));
  int x = draw.box.x;
  if (draw.style.align == fui::TextAlign::Right) x += draw.box.width - width;
  if (draw.style.align == fui::TextAlign::Center) x += (draw.box.width - width) / 2;
  const int line = target.lineHeight(draw.style.font);
  return fui::makeRect(x, draw.box.y + std::max(0, (int(draw.box.height) - line) / 2) + metrics.y, width,
                       metrics.height);
}
bool overlaps(fui::Rect a, fui::Rect b) {
  return !a.empty() && !b.empty() && a.x < b.right() && a.right() > b.x && a.y < b.bottom() && a.bottom() > b.y;
}
void scenario(int w, int h, int family, const char* title, const char* clock, bool percent, bool actions,
              int rotation) {
  std::vector<uint8_t> pixels((w + 7) / 8 * h, 255);
  Target target(pixels.data(), w, h);
  Font tiny{toybox_10}, small{toybox_14}, body{toybox_20}, large{toybox_30};
  target.real.setFont(0, small.font);
  target.real.setFont(1, body.font);
  target.real.setFont(2, large.font);
  target.real.setFont(3, tiny.font);
  target.real.setFont(5, small.font);
  auto d = target.real.deviceContext();
  d.hasTouch = true;
  static constexpr fui::Insets insets[] = {{10, 1, 0, 1}, {1, 10, 1, 0}, {0, 1, 10, 1}, {1, 0, 1, 10}};
  d.safeArea = insets[rotation];
  fui::InputSnapshot input;
  toybox::Interactions hits;
  toybox::HeaderStatus status{100, false, percent};
  std::snprintf(status.clock, sizeof(status.clock), "%s", clock);
  toybox::Frame frame(target, d, input, hits, nullptr, status);
  toybox::Screen screen(frame);
  if (family == 0) {
    pixelui::Header model;
    model.title = title;
    model.back = true;
    if (actions) {
      model.mark = fui::bitmapFromIcon(extra_calendar_32);
      model.markAction = 2;
      model.secondaryMark = model.mark;
      model.secondaryMarkAction = 3;
    }
    pixelui::header(screen, model);
  }
  if (family == 1) {
    fui::HeaderProps props;
    props.title = title;
    props.styles = toybox::bandOutlineStyles();
    toybox::headerBand(screen, props);
  }
  if (family == 2) cardpanel::begin(screen, title);
  if (family == 4) {
    solitaire::Game game;
    solitaireui::BoardModel model;
    model.game = &game;
    solitaireui::Layout layout;
    solitaireui::buildBoard(screen, model, layout);
  }
  if (family == 5) {
    hearts::Game game;
    heartsui::BoardModel model;
    model.game = &game;
    heartsui::Layout layout;
    heartsui::buildBoard(screen, model, layout);
  }
  if (family == 3) {
    fui::HeaderProps props;
    props.title = title;
    props.titleText.font = 1;
    props.titleText.color = fui::Color::White;
    props.status.clockText = clock;
    props.status.stripHeight = 28;
    props.status.showBattery = true;
    props.status.battery.percent = 100;
    props.status.battery.label = percent ? "100%" : nullptr;
    props.status.battery.text.font = 3;
    props.titleOffsetY = 14;
    fui::header(frame, fui::makeRect(d.safeRect().x + 8, d.safeRect().y, d.safeRect().width - 16, 74), props);
  }
  int clocks = 0;
  for (const auto& draw : target.texts)
    if (!draw.text.empty() && draw.text == clock) {
      ++clocks;
      CHECK(draw.style.font == fui::HEADER_CLOCK_FONT);
      auto box = ink(target, draw);
      CHECK(box.x >= d.safeRect().x && box.right() <= d.safeRect().right());
      CHECK(box.y >= d.safeRect().y && box.bottom() <= (family == 3 ? d.safeRect().y + 74 : chrome::headerHeight));
      CHECK(target.lineHeight(draw.style.font) > target.lineHeight(3));
      for (const auto& mark : target.marks) CHECK(!overlaps(box, mark));
      for (const auto& other : target.texts)
        if (&other != &draw) CHECK(!overlaps(box, ink(target, other)));
    }
  if (!*clock) CHECK(clocks == 0);
  if (family >= 4 && w == 800 && *clock) CHECK(clocks == 1);
  if (family < 3 && std::strlen(title) > 55) CHECK(clocks == 0);
  if ((family == 2 || family == 0) && !actions && std::strlen(title) < 10 && *clock) CHECK(clocks == 1);
}
}  // namespace
int main() {
  auto styles = uiButtonStyles();
  CHECK(styles.normal.radius == 0 && styles.selected.radius == 0 && styles.focused.radius == 0 &&
        styles.active.radius == 0 && styles.disabled.radius == 0);
  for (auto language : {Language::EN, Language::ES}) {
    I18N.setLanguage(language);
    for (int rotation = 0; rotation < 4; ++rotation) {
      const int w = rotation % 2 ? 800 : 480;
      for (int family = 0; family < 6; ++family)
        for (bool percent : {false, true})
          for (const char* clock : {"", "09:41", "11:59 PM"})
            for (const char* title : {"PET", "Solitaire", "Corazones", "Reading statistics",
                                      "A VERY LONG TITLE WHICH MUST KEEP ITS SPACE AND HIDE THE OPTIONAL CLOCK"})
              scenario(w, w == 480 ? 800 : 480, family, title, clock, percent, family == 0 && std::strlen(title) > 9,
                       rotation);
    }
  }
  std::printf("header clock: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
