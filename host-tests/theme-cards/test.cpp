#include <I18n.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/apps_local/game2048/Game2048Screens.h"
#include "../../src/apps_local/go/GoScreens.h"
#include "../../src/apps_local/minesweeper/MinesweeperScreens.h"
#include "../../src/apps_local/murdle/MurdleCast.h"
#include "../../src/apps_local/murdle/MurdleScreens.h"
#include "../../src/apps_local/reading/ReadingStatsScreens.h"
#include "../../src/apps_local/sudoku/SudokuScreens.h"
#include "../../src/apps_local/ui/LightOptionMenu.h"
#include "../../src/apps_local/ui/MatchSettings.h"
#include "../../src/apps_local/ui/PixelMenu.h"

namespace fui = freeink::ui;
namespace {
int checks = 0, failures = 0;
#define CHECK(v)                                  \
  do {                                            \
    ++checks;                                     \
    if (!(v)) {                                   \
      ++failures;                                 \
      std::printf("FAIL %d: %s\n", __LINE__, #v); \
    }                                             \
  } while (0)
bool same(fui::Rect a, fui::Rect b) { return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height; }
bool inside(fui::Rect inner, fui::Rect outer, int padding = 0) {
  return inner.x >= outer.x + padding && inner.y >= outer.y + padding && inner.right() <= outer.right() - padding &&
         inner.bottom() <= outer.bottom() - padding;
}
struct Shape {
  fui::Rect rect;
  int radius, width;
  fui::Paint paint;
};
struct Text {
  fui::Rect rect;
  std::string value;
};
struct Target : fui::DrawTarget {
  std::vector<Shape> fills, outlines;
  std::vector<fui::Rect> whiteIcons;
  std::vector<Text> texts;
  fui::Size measureText(fui::FontId, const char* value, fui::TextStyle) const override {
    int chars = 0;
    for (auto* p = reinterpret_cast<const unsigned char*>(value); p && *p; ++p) chars += (*p & 0xc0) != 0x80;
    return fui::Size{int16_t(chars * 8), 18};
  }
  int16_t lineHeight(fui::FontId) const override { return 18; }
  void fill(fui::Rect r, fui::Paint p, uint8_t radius, uint8_t) override { fills.push_back({r, radius, 0, p}); }
  void stroke(fui::Rect r, fui::Paint p, uint8_t width, uint8_t radius, uint8_t) override {
    outlines.push_back({r, radius, width, p});
  }
  void line(fui::Point, fui::Point, uint8_t, fui::Paint) override {}
  void triangle(fui::Point, fui::Point, fui::Point, fui::Paint) override {}
  void bitmap(fui::Rect r, fui::BitmapRef, fui::BitmapMode, fui::Paint p, fui::Rotation) override {
    if (p.color == fui::Color::White) whiteIcons.push_back(r);
  }
  void text(fui::Rect r, const char* value, fui::TextStyle) override { texts.push_back({r, value ? value : ""}); }
};
struct Canvas {
  Target target;
  fui::DeviceContext device;
  fui::InteractionBuffer<toybox::kMaxInteractions> hits;
  fui::InputSnapshot input;
  fui::Frame<toybox::kMaxInteractions> frame;
  toybox::Screen screen;
  Canvas(int w, int h) : device{}, frame(target, device, input, hits), screen(frame) {
    device.width = w;
    device.height = h;
    device.hasTouch = true;
  }
  fui::Rect hit(fui::ActionId action, int value = 0) {
    for (size_t n = 0; n < hits.count(); ++n)
      if (hits.data()[n].action == action && hits.data()[n].value == value) return hits.data()[n].rect;
    CHECK(false);
    return {};
  }
  void validHits() {
    CHECK(!hits.overflowed());
    for (size_t n = 0; n < hits.count(); ++n) {
      const auto& h = hits.data()[n];
      CHECK(h.rect.width > 0 && h.rect.height > 0);
      CHECK(inside(h.rect, fui::makeRect(0, 0, device.width, device.height)));
      fui::InputSnapshot tap;
      tap.touchReleased = true;
      tap.touchX = h.rect.x + h.rect.width / 2;
      tap.touchY = h.rect.y + h.rect.height / 2;
      const auto event = hits.route(tap);
      CHECK(event.action == h.action && event.value == h.value);
    }
  }
};
void goCards(int w, int h, bool light) {
  Canvas c(w, h);
  goui::buildSettings(c.screen, {});
  c.validHits();
  auto opponent = c.hit(goui::ActionSettingsRow, int(goui::SettingsRow::Opponent));
  auto level = c.hit(goui::ActionSettingsRow, int(goui::SettingsRow::Level));
  auto play = c.hit(goui::ActionSettingsRow, int(goui::SettingsRow::PlayAs));
  auto size = c.hit(goui::ActionSettingsRow, int(goui::SettingsRow::Board));
  auto handicap = c.hit(goui::ActionSettingsRow, int(goui::SettingsRow::Handicap));
  const auto footer = c.hit(goui::ActionDone);
  CHECK(level.y - opponent.bottom() == (light ? 8 : 0));
  CHECK(play.y - level.bottom() == (light ? 8 : 0));
  CHECK(handicap.y - size.bottom() == (light ? 8 : 0));
  CHECK(play.bottom() + 8 <= footer.y && handicap.bottom() + 8 <= footer.y);
  for (const auto& shape : c.target.outlines)
    if (same(shape.rect, level) || same(shape.rect, size)) CHECK(shape.radius == (light ? 5 : 0));
}
void murdleCards(int w, int h, bool light) {
  murdle::Scratch scratch;
  for (int tier = 0; tier < 4; ++tier) {
    murdle::Puzzle puzzle;
    uint8_t cast[4][4]{};
    const auto shape = murdle::shapeOf(murdle::Tier(tier));
    CHECK(murdle::drawCast(123 + tier, shape, cast));
    CHECK(murdle::generate(murdle::Tier(tier), 123 + tier, cast, murdle::attrMasksFor(cast, shape), scratch, puzzle));
    murdle::Marks marks;
    marks.reset(shape);
    Canvas c(w, h);
    murdleui::CaseModel m;
    m.puzzle = &puzzle;
    m.marks = &marks;
    m.face = murdleui::Face::Info;
    murdleui::buildCase(c.screen, m);
    c.validHits();
    if (light)
      for (auto action : {murdleui::ActionFace, murdleui::ActionInfoCategory}) {
        const auto first = c.hit(action), last = c.hit(action, action == murdleui::ActionFace ? 2 : shape.cats - 1);
        const auto band = fui::makeRect(first.x, first.y, last.right() - first.x, first.height);
        int borders = 0;
        for (const auto& outline : c.target.outlines)
          if (same(outline.rect, band)) {
            ++borders;
            CHECK(outline.radius == 5);
          }
        CHECK(borders == 1);
      }
    for (bool complete : {false, true}) {
      Canvas a(w, h);
      murdleui::AccuseModel accusation;
      accusation.puzzle = &puzzle;
      if (complete)
        for (int n = 0; n < shape.cats; ++n) accusation.picks[n] = 0;
      murdleui::buildAccuse(a.screen, accusation);
      a.validHits();
      if (!light) continue;
      for (const auto& text : a.target.texts) {
        CHECK(text.rect.width > 0 && text.rect.height > 0);
        if (text.value == tr(STR_MR_YOUR_ACCUSATION)) {
          bool padded = false;
          for (const auto& border : a.target.outlines) padded |= inside(text.rect, border.rect, 6);
          CHECK(padded);
        }
      }
    }
  }
}
void sudokuCards(int w, int h, bool light) {
  Canvas c(w, h);
  sudokuui::BoardModel m;
  m.game.armed = 8;
  sudokuui::buildBoard(c.screen, m);
  c.validHits();
  const auto selected = c.hit(sudokuui::ActionDigit, 8);
  bool found = false;
  for (const auto& fill : c.target.fills)
    if (same(fill.rect, selected)) {
      found = true;
      CHECK(fill.radius == (light ? 5 : 0));
    }
  CHECK(found);
}
void todayCard(int w, int h, bool light) {
  Canvas c(w, h);
  readingui::Model m;
  m.today = 20000;
  m.displayedDay = m.today;
  readingstats::PeriodStats period;
  period.sessions = 2;
  period.pages = 18;
  m.periodStats = &period;
  readingui::Work work;
  readingui::build(c.screen, m, work);
  c.validHits();
  fui::Rect metrics[3]{};
  const char* labels[] = {tr(STR_READING_SESSIONS), tr(STR_READING_PAGES), tr(STR_READING_FINISHED)};
  for (int n = 0; n < 3; ++n) {
    char label[96];
    pixelui::copyCaps(label, sizeof(label), labels[n]);
    for (const auto& text : c.target.texts)
      if (text.value == label) metrics[n] = text.rect;
    CHECK(metrics[n].width > 0);
  }
  int groups = 0;
  for (const auto& shape : c.target.outlines)
    if (inside(metrics[0], shape.rect) && inside(metrics[1], shape.rect) && inside(metrics[2], shape.rect)) {
      ++groups;
      CHECK(shape.radius == (light ? 5 : 0));
    }
  CHECK(groups == 1);
  for (const auto& shape : c.target.outlines)
    CHECK(!(inside(metrics[0], shape.rect) && !inside(metrics[1], shape.rect)));
}
}  // namespace
void statsArrow(int width, int height) {
  readingstats::Book book;
  std::snprintf(book.title, sizeof(book.title), "An example book");
  readingstats::Summary summary;
  for (bool buttons : {false, true}) {
    Canvas c(width, height);
    readingui::Model m;
    m.summary = &summary;
    m.books = &book;
    m.count = m.bookTotal = 1;
    m.tab = 3;
    c.device.hasTouch = !buttons;
    readingui::Work work;
    readingui::build(c.screen, m, work);
    const auto row = c.hit(readingui::OpenBook);
    int markers = 0;
    for (const auto& shape : c.target.fills)
      if (shape.rect.width == 22 && shape.rect.height == 30 && inside(shape.rect, row)) ++markers;
    CHECK(markers == (buttons ? 1 : 0));
  }
}
void minesSizeChecks(int width, int height, bool light) {
  for (int size : {10, 15, 20}) {
    Canvas c(width, height);
    mineui::MenuModel model;
    model.settings = true;
    model.size = size;
    mineui::buildMenu(c.screen, model);
    int filled = 0, icons = 0;
    for (const auto& shape : c.target.fills) {
      if (shape.rect.width != 28 || shape.rect.height != 28) continue;
      CHECK(shape.radius == (light ? lightui::kControlRadius : 0));
      ++filled;
      for (auto icon : c.target.whiteIcons) icons += inside(icon, shape.rect, 5);
    }
    CHECK(filled == 1 && icons == (light ? 1 : 0));
    CHECK(!c.hits.overflowed());
  }
}
void popupFocus(int w, int h) {
  const fui::DialogOption options[] = {{"One", 101, 1}, {"Two", 102, 2}, {"Three", 103, 3}};
  for (int current : {-1, 0}) {
    for (int focus = 0; focus < 3; ++focus) {
      Canvas c(w, h);
      lightui::ChoiceMenu m;
      m.options = options;
      m.count = 3;
      m.current = current;
      m.focus = focus;
      lightui::choiceMenu(c.frame, fui::makeRect(0, 0, w, h), m);
      int outlined = 0;
      for (int n = 0; n < 3; ++n) {
        const auto hit = c.hit(options[n].action, options[n].value);
        bool found = false;
        for (const auto& fill : c.target.fills)
          found |=
              same(fill.rect, hit) && fill.paint.kind == fui::PaintKind::Solid && fill.paint.color == fui::Color::Black;
        CHECK(found == (n == focus));
        outlined += found;
      }
      CHECK(outlined == 1);
    }
  }
}
int main() {
  for (auto language : {Language::EN, Language::ES}) {
    I18N.setLanguage(language);
    for (bool light : {false, true}) {
      chrome::lightHeader = light;
      for (auto size : {fui::Size{480, 800}, fui::Size{800, 480}}) {
        if (light) popupFocus(size.width, size.height);
        for (bool human : {false, true}) {
          Canvas c(size.width, size.height);
          matchsettings::Model m;
          m.local = human;
          matchsettings::build(c.screen, m);
          int levelHits = 0;
          for (size_t n = 0; n < c.hits.count(); ++n) {
            const auto& hit = c.hits.data()[n];
            levelHits += hit.action == matchsettings::Difficulty;
          }
          CHECK(levelHits == (human ? 0 : 3));
        }
        for (auto result : {game2048::Result::Won, game2048::Result::Lost}) {
          Canvas c(size.width, size.height);
          game2048::State state;
          state.cells[0] = result == game2048::Result::Won ? 11 : 1;
          game2048ui::Model m;
          m.state = &state;
          m.result = result;
          m.view = game2048ui::View::Result;
          game2048ui::Work work;
          game2048ui::build(c.screen, m, work);
          CHECK(c.hit(game2048ui::ConfirmNew).height >= 44);
          CHECK(c.hit(game2048ui::Review).height >= 44);
          CHECK(c.hit(game2048ui::CancelNew).height >= 44);
          c.validHits();
        }
        goCards(size.width, size.height, light);
        murdleCards(size.width, size.height, light);
        sudokuCards(size.width, size.height, light);
        todayCard(size.width, size.height, light);
        statsArrow(size.width, size.height);
        minesSizeChecks(size.width, size.height, light);
      }
    }
  }
  std::printf("Theme cards: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
