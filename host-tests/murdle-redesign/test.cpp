#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../../src/apps_local/checkers/DraughtsScreens.h"
#include "../../src/apps_local/chess/ChessLosses.h"
#include "../../src/apps_local/chess/ChessScreens.h"
#include "../../src/apps_local/chess/ChessStyle.h"
#include "../../src/apps_local/chess/puzzles/ChessPuzzleScreens.h"
#include "../../src/apps_local/go/GoScreens.h"
#include "../../src/apps_local/home/HomeMenuOrderUi.h"
#include "../../src/apps_local/library/LibraryMenu.h"
#include "../../src/apps_local/murdle/MurdleArt.h"
#include "../../src/apps_local/murdle/MurdleCast.h"
#include "../../src/apps_local/murdle/MurdlePeople.h"
#include "../../src/apps_local/murdle/MurdleSave.h"
#include "../../src/apps_local/murdle/MurdleScreens.h"
#include "../../src/apps_local/murdle/MurdleSession.h"
#include "../../src/apps_local/study/StudyScreens.h"
#include "../../src/apps_local/ui/PageIndicator.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../lib/EpdFont/builtinFonts/ubuntu_10_regular.h"
#include "../../src/apps_local/ui/fonts/reading_serif_14.h"
#include "../../src/apps_local/ui/fonts/toybox_10.h"
#include "../../src/apps_local/ui/fonts/toybox_14.h"
#include "../../src/apps_local/ui/fonts/toybox_20.h"
#include "../../src/apps_local/ui/fonts/toybox_30.h"
#include "../../src/apps_local/ui/fonts/ui_prose_10.h"
#include "../../src/apps_local/ui/fonts/ui_prose_14.h"
#pragma GCC diagnostic pop

namespace fui = freeink::ui;
namespace {
int checks = 0;
int failures = 0;
#define CHECK(expr)                                  \
  do {                                               \
    ++checks;                                        \
    if (!(expr)) {                                   \
      ++failures;                                    \
      std::printf("FAIL %d: %s\n", __LINE__, #expr); \
    }                                                \
  } while (0)

// The same glyph bitmaps and metrics as the firmware, adapted to the SDK's
// freestanding framebuffer target. This is a host preview, not a device shot.
struct FontBridge {
  std::array<fui::FontGlyph, 224> glyphs{};
  fui::BitmapFont font{};
  explicit FontBridge(const EpdFontData& source) {
    for (uint32_t cp = 32; cp <= 255; ++cp) {
      for (uint32_t i = 0; i < source.intervalCount; ++i) {
        const auto& interval = source.intervals[i];
        if (cp < interval.first || cp > interval.last) continue;
        const auto& g = source.glyph[interval.offset + cp - interval.first];
        glyphs[cp - 32] = {static_cast<uint16_t>(g.dataOffset),
                           g.width,
                           g.height,
                           static_cast<uint8_t>(fp4::toPixel(g.advanceX)),
                           static_cast<int8_t>(g.left),
                           static_cast<int8_t>(-g.top)};
        break;
      }
    }
    font = {source.bitmap, glyphs.data(), 32, 255, source.advanceY, static_cast<uint8_t>(source.ascender), 64, 64, 1};
  }
};

fui::DeviceContext makeDevice(fui::DisplayTarget& target) {
  auto d = target.deviceContext();
  d.hasTouch = true;
  d.safeArea = {9, 7, 3, 7};
  return d;
}
struct RecordingTarget : fui::DrawTarget {
  fui::DisplayTarget backend;
  RecordingTarget(uint8_t* data, int w, int h, int stride, fui::Orientation orientation)
      : backend(data, w, h, stride, orientation) {
    bitmaps.reserve(64);
  }
  void setFont(fui::FontId id, const fui::BitmapFont& font) { backend.setFont(id, font); }
  fui::Size measureText(fui::FontId id, const char* text, fui::TextStyle style) const override {
    return backend.measureText(id, text, style);
  }
  int16_t lineHeight(fui::FontId id) const override { return backend.lineHeight(id); }
  fui::Rect inkBounds(fui::FontId id, const char* text, fui::TextStyle style) const override {
    return backend.inkBounds(id, text, style);
  }
  fui::Rect clipRect() const override { return backend.clipRect(); }
  bool setClipRect(fui::Rect rect) override { return backend.setClipRect(rect); }
  void fill(fui::Rect r, fui::Paint p, uint8_t radius = 0, uint8_t corners = fui::CornersAll) override {
    if (p.color == fui::Color::Black) blackFills.push_back(r);
    backend.fill(r, p, radius, corners);
  }
  void stroke(fui::Rect r, fui::Paint p, uint8_t width, uint8_t radius = 0,
              uint8_t corners = fui::CornersAll) override {
    backend.stroke(r, p, width, radius, corners);
  }
  struct Segment {
    fui::Point a, b;
  };
  std::vector<Segment> segments;
  void line(fui::Point a, fui::Point b, uint8_t w, fui::Paint p) override {
    segments.push_back({a, b});
    backend.line(a, b, w, p);
  }
  void triangle(fui::Point a, fui::Point b, fui::Point c, fui::Paint p) override { backend.triangle(a, b, c, p); }
  void bitmap(fui::Rect r, fui::BitmapRef ref, fui::BitmapMode mode,
              fui::Paint p = fui::Paint::solid(fui::Color::Black),
              fui::Rotation rotation = fui::Rotation::None) override {
    CHECK(r.width > 0 && r.height > 0);
    CHECK(r.x >= 0 && r.y >= 0 && r.right() <= clipRect().right() && r.bottom() <= clipRect().bottom());
    bitmaps.push_back(ref.data);
    backend.bitmap(r, ref, mode, p, rotation);
  }
  struct Label {
    std::string value;
    fui::FontId font;
    fui::Rect box;
  };
  std::vector<const uint8_t*> bitmaps;
  std::vector<Label> labels;
  std::vector<fui::Rect> blackFills;
  void text(fui::Rect box, const char* value, fui::TextStyle style) override {
    fui::layoutText(backend, box, value, style, [&](const char* run, fui::Rect line) {
      if (line.bottom() > box.bottom() || std::strstr(run, "\xE2\x80\xA6") || std::strstr(run, "..."))
        std::printf("TEXT [%d,%d %dx%d font%d] %s\n", box.x, box.y, box.width, box.height, int(style.font), value);
      CHECK(line.bottom() <= box.bottom());
      CHECK(!std::strstr(run, "\xE2\x80\xA6") && !std::strstr(run, "..."));
    });
    labels.push_back({value ? value : "", style.font, box});
    backend.text(box, value, style);
  }
};
struct Canvas {
  int width, height;
  std::vector<uint8_t> pixels;
  FontBridge regular{ui_prose_10}, tiny{toybox_10}, small{toybox_14}, body{toybox_20}, title{toybox_30},
      library{ubuntu_10_regular}, prose{ui_prose_14}, reading{reading_serif_14};
  RecordingTarget target;
  fui::DeviceContext device;
  fui::InputSnapshot input{};
  toybox::Interactions interactions;
  toybox::Frame frame;
  toybox::Screen screen;

  Canvas(int w, int h)
      : width(w),
        height(h),
        pixels((w + 7) / 8 * h, 255),
        target(pixels.data(), w, h, (w + 7) / 8, fui::Orientation::LandscapeCounterClockwise),
        device(makeDevice(target.backend)),
        frame(target, device, input, interactions),
        screen(frame) {
    target.setFont(3, tiny.font);
    target.setFont(4, regular.font);
    target.setFont(toybox::kSmallFont, small.font);
    target.setFont(toybox::kUiFont, body.font);
    target.setFont(toybox::kDisplayFont, title.font);
  }

  void checkHits() {
    CHECK(!interactions.overflowed());
    paintclock::notePainted();
    for (size_t n = 0; n < interactions.count(); ++n) {
      const auto& slot = interactions.data()[n];
      const auto& r = slot.rect;
      CHECK(r.x >= 0 && r.y >= 0 && r.right() <= width && r.bottom() <= height);
      CHECK(r.width >= 44 && r.height >= 44);
      for (size_t j = 0; j < n; ++j) {
        const auto& a = interactions.data()[j].rect;
        CHECK(r.x >= a.right() || r.right() <= a.x || r.y >= a.bottom() || r.bottom() <= a.y);
      }
      fui::InputSnapshot tap;
      tap.touchReleased = true;
      tap.touchX = static_cast<int16_t>(r.x + r.width / 2);
      tap.touchY = static_cast<int16_t>(r.y + r.height / 2);
      const auto event = interactions.route(tap);
      if (event.action != slot.action || event.value != slot.value)
        std::printf("HIT expected %d:%d at %d,%d -> %d:%d\n", slot.action, slot.value, tap.touchX, tap.touchY,
                    event.action, event.value);
      CHECK(event.action == slot.action && event.value == slot.value);
    }
  }

  bool has(fui::ActionId action, int value = 0) const {
    for (size_t n = 0; n < interactions.count(); ++n)
      if (interactions.data()[n].action == action && interactions.data()[n].value == value) return true;
    return false;
  }

  int hits(fui::ActionId action) const {
    int count = 0;
    for (size_t n = 0; n < interactions.count(); ++n)
      if (interactions.data()[n].action == action) ++count;
    return count;
  }
  bool label(const char* value) const {
    char caps[192];
    pixelui::copyCaps(caps, sizeof(caps), value);
    for (const auto& l : target.labels)
      if (l.value == value || l.value == caps) return true;
    return false;
  }
  void write(const std::string& path) const {
    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) {
      std::perror(path.c_str());
      std::exit(2);
    }
    std::fprintf(file, "P4\n%d %d\n", width, height);
    for (const auto byte : pixels) std::fputc(byte ^ 255, file);
    std::fclose(file);
  }
};

}  // namespace

int main(int argc, char** argv) {
  static murdle::Scratch scratch;
  // Portrait IDs are artwork identities, independent of the name-pool order.
  const auto femalePortrait = [](int id) {
    static constexpr int female[] = {10, 15, 16, 17, 18, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29};
    for (int portrait : female)
      if (portrait == id) return true;
    return false;
  };
  bool seen[30]{}, portraits[30]{};
  for (int seed = 1; seed < 300; ++seed) {
    murdle::Puzzle p;
    p.peopleSeed = seed;
    for (int i = 0; i < 4; ++i) {
      int a = murdlepeople::person(p, i), b = murdlepeople::portrait(p, i);
      seen[a] = true;
      portraits[b] = true;
      CHECK(a >= 0 && a < 30 && b >= 0 && b < 30);
      CHECK((a >= 15) == femalePortrait(b));
      for (int j = 0; j < i; ++j) {
        CHECK(a != murdlepeople::person(p, j));
        CHECK(b != murdlepeople::portrait(p, j));
      }
    }
  }
  for (int n = 0; n < 30; ++n) {
    CHECK(seen[n]);
    CHECK(portraits[n]);
  }
  // Legacy saves keep their original names but use the corrected portrait pools.
  static constexpr bool femaleSuspect[murdle::kSuspectCount] = {true,  false, true,  false, true, false, true,  false,
                                                                false, true,  false, false, true, true,  false, false,
                                                                false, true,  true,  false, true, true,  false, true};
  for (uint32_t seed = 1; seed < 300; ++seed) {
    murdle::Puzzle p;
    p.seed = seed;
    for (int suspect = 0; suspect < murdle::kSuspectCount; ++suspect) {
      p.cast[0][0] = suspect;
      CHECK(femalePortrait(murdlepeople::portrait(p, 0)) == femaleSuspect[suspect]);
      CHECK(std::strcmp(murdlepeople::name(p, 0), murdle::kSuspects[suspect].name) == 0);
    }
  }
  CHECK(sizeof(murdleart::weapons) / sizeof(murdleart::weapons[0]) == 20);
  CHECK(sizeof(murdleart::places) / sizeof(murdleart::places[0]) == 16);
  CHECK(sizeof(murdleart::motives) / sizeof(murdleart::motives[0]) == 12);
  CHECK(murdleart::motiveById(nullptr) == nullptr);
  CHECK(murdleart::motiveById("UNKNOWN") == nullptr);
  for (int category : {1, 2, 3}) {
    const int count = category == 1 ? murdle::kWeaponCount : category == 2 ? murdle::kPlaceCount : murdle::kMotiveCount;
    const auto* images = category == 1 ? murdleart::weapons : category == 2 ? murdleart::places : murdleart::motives;
    const auto* ids = category == 1   ? murdleart::weaponsIds
                      : category == 2 ? murdleart::placesIds
                                      : murdleart::motivesIds;
    for (int item = 0; item < count; ++item) {
      const auto& bitmap = *images[item];
      CHECK(bitmap.data && bitmap.width == 96 && bitmap.height == 96);
      CHECK(std::strcmp(ids[item], murdle::castName(category, item)) == 0);
      if (category == 3) CHECK(murdleart::motiveById(murdle::kMotives[item].name) == images[item]);
      for (int prior = 0; prior < item; ++prior) CHECK(bitmap.data != images[prior]->data);
      for (auto dimensions : {std::array<int, 2>{480, 800}, std::array<int, 2>{800, 480}}) {
        Canvas c(dimensions[0], dimensions[1]);
        murdle::Puzzle p;
        p.tier = category == 3 ? murdle::Tier::HardBoiled : murdle::Tier::Elementary;
        p.shape = murdle::shapeOf(p.tier);
        p.peopleSeed = 1;
        for (int n = 0; n < p.shape.items; ++n) p.cast[category][n] = (item + n) % count;
        murdle::Marks marks;
        marks.reset(p.shape);
        murdleui::CaseModel model;
        model.puzzle = &p;
        model.marks = &marks;
        model.face = murdleui::Face::Info;
        model.page = category;
        murdleui::buildCase(c.screen, model);
        for (int n = 0; n < p.shape.items; ++n) {
          const auto* expected = images[p.cast[category][n]]->data;
          CHECK(std::find(c.target.bitmaps.begin(), c.target.bitmaps.end(), expected) != c.target.bitmaps.end());
        }
        c.checkHits();
        if (category == 3) {
          for (int page : {2, 4, 5}) {
            Canvas table(dimensions[0], dimensions[1]);
            model.face = murdleui::Face::Grid;
            model.page = page;
            murdleui::buildCase(table.screen, model);
            CHECK(std::find(table.target.bitmaps.begin(), table.target.bitmaps.end(), images[item]->data) !=
                  table.target.bitmaps.end());
            table.checkHits();
          }
          Canvas accusation(dimensions[0], dimensions[1]);
          murdleui::AccuseModel selection;
          selection.puzzle = &p;
          selection.picks[3] = 0;
          murdleui::buildAccuse(accusation.screen, selection);
          CHECK(std::count(accusation.target.bitmaps.begin(), accusation.target.bitmaps.end(), images[item]->data) ==
                2);
          accusation.checkHits();
        }
      }
    }
  }
  for (auto lang : {Language::EN, Language::ES}) {
    I18N.setLanguage(lang);
    for (auto dim : {std::array<int, 2>{480, 800}, std::array<int, 2>{800, 480}}) {
      const int w = dim[0], h = dim[1];
      const auto shot = [&](Canvas& c, const std::string& name) {
        c.checkHits();
        if (argc > 1)
          c.write(std::string(argv[1]) + "/" + (lang == Language::ES ? "es" : "en") + "-" + std::to_string(w) + "-" +
                  name + ".pbm");
      };
      {
        Canvas c(w, h);
        pixelui::begin(c.screen);
        pixelui::Header header;
        header.title = tr(STR_HOME_MENU_ORDER);
        header.back = true;
        pixelui::header(c.screen, header);
        homemenuui::OrderWork orderWork;
        homemenuui::orderControls(c.screen, orderWork, 2, 6, 90);
        fui::ListItem items[6]{};
        const char* labels[] = {tr(STR_BROWSE_FILES),        tr(STR_MENU_RECENT_BOOKS), tr(STR_HOME_GAMES),
                                tr(STR_READING_STATS_TITLE), tr(STR_HOME_APPS),         tr(STR_SETTINGS_TITLE)};
        for (int n = 0; n < 6; ++n) {
          items[n].label = labels[n];
          items[n].actionValue = n;
        }
        fui::ListProps props;
        props.items = items;
        props.count = 6;
        props.action = 91;
        props.selectedIndex = 2;
        pixelui::styleList(props);
        pixelui::RowSource rowText;
        rowText.bind(props, c.target, c.screen.body());
        pixelui::list(c.screen, props);
        CHECK(c.has(90, -1) && c.has(90, 1));
        shot(c, "home-order");
      }
      for (int tier = 0; tier < 4; ++tier) {
        murdle::Puzzle p;
        uint8_t cast[4][4]{};
        const uint32_t seed = murdlecampaign::kSeeds[tier * murdlecampaign::kCasesPerTier];
        auto shape = murdle::shapeOf(murdle::Tier(tier));
        CHECK(murdle::drawCast(seed, shape, cast));
        CHECK(murdle::generate(murdle::Tier(tier), seed, cast, murdle::attrMasksFor(cast, shape), scratch, p));
        p.peopleSeed = 1234;
        murdle::Marks marks;
        marks.reset(shape);
        int wrong = 0;
        uint8_t picks[4] = {255, 255, 255, 255};
        CHECK(murdlesession::accuse(p, picks, wrong, false) == murdlesession::Accusation::Incomplete && wrong == 0);
        for (int c = 0; c < shape.cats; ++c) picks[c] = p.assign[c][p.murderRow];
        CHECK(murdlesession::accuse(p, picks, wrong, false) == murdlesession::Accusation::Solved && wrong == 0);
        picks[0] = (picks[0] + 1) % shape.items;
        for (int n = 1; n <= 3; ++n) {
          CHECK(murdlesession::accuse(p, picks, wrong, false) == murdlesession::Accusation::Wrong);
          CHECK(wrong == n && murdlesession::lives(wrong) == 3 - n);
        }
        CHECK(murdlesession::accuse(p, picks, wrong, false) == murdlesession::Accusation::Closed && wrong == 3);
        for (int face = 0; face < 3; ++face) {
          int pages = 1;
          for (int pg = 0; pg < pages; ++pg) {
            Canvas c(w, h);
            murdleui::CaseModel m;
            m.puzzle = &p;
            m.marks = &marks;
            m.face = murdleui::Face(face);
            m.page = pg;
            m.struck = 2;
            auto report = murdleui::buildCase(c.screen, m);
            pages = report.pages;
            if (tier == 3 && face == 0) CHECK(c.label(tr(STR_MR_LIAR_RULE)));
            if (face == 0) {
              CHECK(c.hits(murdleui::ActionPage) == (pg > 0 ? 1 : 0) + (pg + 1 < report.pages ? 1 : 0));
              CHECK(!c.label("REVIEWED") && !c.label("REVISADA"));
              char evidence[64];
              std::snprintf(evidence, sizeof(evidence), tr(STR_MR_EVIDENCE), report.page + 1, report.pages);
              for (const auto& label : c.target.labels)
                if (label.value == evidence) CHECK(label.font == toybox::kUiFont);
              for (size_t index = 0; index < c.interactions.count(); ++index) {
                const auto& hit = c.interactions.data()[index];
                if (hit.action != murdleui::ActionClue) continue;
                char number[12];
                std::snprintf(number, sizeof(number), "%d", hit.value + 1);
                bool centered = false, separated = false;
                for (const auto& label : c.target.labels) {
                  if (label.value != number || !hit.rect.contains(label.box.x, label.box.y)) continue;
                  centered = std::abs((label.box.y * 2 + label.box.height) - (hit.rect.y * 2 + hit.rect.height)) <= 1;
                  for (const auto& segment : c.target.segments)
                    separated |= segment.a.x == segment.b.x && segment.a.x > hit.rect.x + 48 &&
                                 segment.a.x < label.box.x && segment.a.y > hit.rect.y &&
                                 segment.b.y < hit.rect.bottom() && segment.b.y - segment.a.y > hit.rect.height / 2;
                }
                CHECK(centered && separated);
              }
            }
            CHECK(report.page == pg);
            if (face == 1) {
              murdleui::GridCell cell;
              CHECK(murdleui::cellAt(report.grid, report.grid.originX + 1, report.grid.originY + 1, cell));
              CHECK(cell.itemA == 0 && cell.itemB == 0 && cell.catA < cell.catB);
              CHECK(report.grid.cell >= 44 && report.grid.cellWidth >= 44);
              const int a = report.grid.rowCat[0], b = report.grid.colCat[0];
              marks.enter(a, 0, b, 0, murdle::Mark::No);
              marks.enter(a, 1, b, 1, murdle::Mark::Yes);
              Canvas marked(w, h);
              const auto drawn = murdleui::buildCase(marked.screen, m);
              for (int item = 0; item < 2; ++item) {
                const auto box = fui::makeRect(drawn.grid.cellX(item), drawn.grid.cellY(item), drawn.grid.cellWidth,
                                               drawn.grid.cell);
                int x0 = box.right(), y0 = box.bottom(), x1 = box.x, y1 = box.y, lines = 0;
                for (const auto& segment : marked.target.segments) {
                  if (!box.contains(segment.a.x, segment.a.y) || !box.contains(segment.b.x, segment.b.y)) continue;
                  x0 = std::min(x0, int(std::min(segment.a.x, segment.b.x)));
                  y0 = std::min(y0, int(std::min(segment.a.y, segment.b.y)));
                  x1 = std::max(x1, int(std::max(segment.a.x, segment.b.x)));
                  y1 = std::max(y1, int(std::max(segment.a.y, segment.b.y)));
                  ++lines;
                }
                CHECK(lines == 2 && x1 > x0 && y1 > y0);
                CHECK((x1 - x0) * 2 <= (y1 - y0) * 3);
                CHECK((y1 - y0) * 2 <= (x1 - x0) * 3);
              }
              shot(marked, "t" + std::to_string(tier) + "-grid" + std::to_string(pg) + "-marks");
              marks.reset(shape);
            }
            shot(c, "t" + std::to_string(tier) + "-f" + std::to_string(face) + "-p" + std::to_string(pg));
          }
        }
        {
          Canvas c(w, h);
          murdleui::AccuseModel m;
          m.puzzle = &p;
          murdleui::buildAccuse(c.screen, m);
          int options = 0;
          for (size_t hitIndex = 0; hitIndex < c.interactions.count(); ++hitIndex) {
            const auto& hit = c.interactions.data()[hitIndex];
            CHECK(hit.action != murdleui::ActionConfirm);
            options += hit.action == murdleui::ActionPick;
          }
          CHECK(options == shape.items * shape.cats);
          shot(c, "blank" + std::to_string(tier));
        }
        {
          Canvas c(w, h);
          murdleui::AccuseModel m;
          m.puzzle = &p;
          for (int n = 0; n < shape.cats; ++n) m.picks[n] = p.assign[n][p.murderRow];
          murdleui::buildAccuse(c.screen, m);
          shot(c, "accuse" + std::to_string(tier));
        }
        for (int n = 0; n < 4; ++n) {
          Canvas c(w, h);
          murdleui::VerdictModel m;
          m.puzzle = &p;
          m.right = n == 0;
          m.nextCase = true;
          m.wrongAccusations = n;
          std::memcpy(m.picks, picks, 4);
          murdleui::buildVerdict(c.screen, m);
          if (m.right) CHECK(c.has(murdleui::ActionRetryCase));
          shot(c, "verdict" + std::to_string(n));
        }
        {
          Canvas c(w, h);
          for (int n = 0; n < 4; ++n) p.clues[n].attr = n == 3 ? murdle::kAttrTallest : static_cast<uint8_t>(n * 16);
          murdleui::CaseModel m;
          m.puzzle = &p;
          m.marks = &marks;
          m.face = murdleui::Face::Info;
          murdleui::buildCase(c.screen, m);
          shot(c, "all-traits" + std::to_string(tier));
        }
        if (!tier) {
          Canvas c(w, h);
          murdleui::MenuModel m;
          m.puzzle = &p;
          m.hasCase = true;
          murdleui::buildMenu(c.screen, m);
          shot(c, "menu");
        }
      }
      for (int tier = 0; tier < murdle::kTierCount; ++tier) {
        for (int pg = 0; pg < murdlecampaign::kPagesPerTier; ++pg) {
          Canvas c(w, h);
          murdlecampaign::Progress progress;
          const auto filter = static_cast<murdle::Tier>(tier);
          const int first = murdlecampaign::first(filter);
          for (int i = first; i < first + 4; ++i) progress.finish(i);
          murdleui::buildCampaign(c.screen, progress, pg, first + 4, first + 4, filter);
          int cases = 0, filters = 0;
          for (size_t hitIndex = 0; hitIndex < c.interactions.count(); ++hitIndex) {
            const auto& hit = c.interactions.data()[hitIndex];
            if (hit.action == murdleui::ActionOpenCampaign) {
              CHECK(hit.value == first + pg * murdlecampaign::kCasesPerPage + cases);
              ++cases;
            }
            filters += hit.action == murdleui::ActionTier;
          }
          CHECK(cases == std::min(16, murdlecampaign::kCasesPerTier - pg * 16));
          CHECK(c.has(murdleui::ActionResetProgress));
          CHECK(filters == murdle::kTierCount);
          shot(c, "cases-t" + std::to_string(tier) + "-p" + std::to_string(pg));
        }
      }
      CHECK(murdleui::howToPages(w > h) == 3);
      for (int pg = 0; pg < 3; ++pg) {
        Canvas c(w, h);
        murdleui::HowToModel m;
        m.page = pg;
        murdleui::buildHowTo(c.screen, m);
        CHECK(!c.label(tr(STR_MR_LIAR_RULE)));
        if (pg == 2) {
          CHECK(c.label(tr(STR_MR_HELP_IMPOSSIBLE_RULE)));
          CHECK(c.label(tr(STR_MR_HELP_BASIC_RULE)));
          CHECK(c.label(tr(STR_MR_HELP_NOSY_RULE)));
          CHECK(c.label(tr(STR_MR_HELP_HARD_RULE)));
          for (int tier = 0; tier < murdle::kTierCount; ++tier)
            CHECK(c.label(murdleui::tierName(static_cast<murdle::Tier>(tier))));
        }
        bool pager = false, start = false;
        for (size_t hitIndex = 0; hitIndex < c.interactions.count(); ++hitIndex) {
          const auto& hit = c.interactions.data()[hitIndex];
          if (hit.action == murdleui::ActionPage) {
            pager = true;
            CHECK(hit.value == -1 ? pg > 0 : hit.value == 1 && pg < 2);
          }
          start |= hit.action == murdleui::ActionPlay;
        }
        CHECK(pager && !start);
        CHECK(std::find(c.target.bitmaps.begin(), c.target.bitmaps.end(), murdleart::Link.data) ==
              c.target.bitmaps.end());
        shot(c, "help" + std::to_string(pg));
      }
      {
        Canvas c(w, h);
        murdleui::buildConfirmReset(c.screen, 53);
        CHECK(c.has(murdleui::ActionCancel));
        CHECK(c.has(murdleui::ActionConfirmReset));
        shot(c, "confirm-reset");
      }
      {
        Canvas c(w, h);
        murdleui::buildConfirmNew(c.screen);
        shot(c, "confirm-new");
      }
      {
        Canvas c(w, h);
        murdleui::MenuModel m;
        murdleui::buildMenu(c.screen, m);
        shot(c, "fresh-menu");
      }
      {
        Canvas c(w, h);
        murdleui::buildStorageError(c.screen, true);
        CHECK(c.label(tr(STR_MR_SAVE_ERROR)));
        shot(c, "error");
      }
    }
  }
  std::printf("Murdle redesign: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
