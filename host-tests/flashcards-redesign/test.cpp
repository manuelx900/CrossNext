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
#include "../../src/apps_local/flashcards/FlashScreens.h"
#include "../../src/apps_local/flashcards/FlashSettingsScreens.h"
#include "../../src/apps_local/go/GoScreens.h"
#include "../../src/apps_local/study/StudyScreens.h"
#include "../../src/apps_local/ui/Dashboard.h"
#include "../../src/apps_local/ui/PageIndicator.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../lib/EpdFont/builtinFonts/notosans_8_regular.h"
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
      : backend(data, w, h, stride, orientation) {}
  void setFont(fui::FontId id, const fui::BitmapFont& font) { backend.setFont(id, font); }
  mutable size_t measurements = 0;
  fui::Size measureText(fui::FontId id, const char* text, fui::TextStyle style) const override {
    ++measurements;
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
  struct Line {
    fui::Point a, b;
  };
  std::vector<Line> lines;
  void line(fui::Point a, fui::Point b, uint8_t w, fui::Paint p) override {
    lines.push_back({a, b});
    backend.line(a, b, w, p);
  }
  void triangle(fui::Point a, fui::Point b, fui::Point c, fui::Paint p) override { backend.triangle(a, b, c, p); }
  void bitmap(fui::Rect r, fui::BitmapRef ref, fui::BitmapMode mode,
              fui::Paint p = fui::Paint::solid(fui::Color::Black),
              fui::Rotation rotation = fui::Rotation::None) override {
    CHECK(r.width > 0 && r.height > 0);
    CHECK(r.x >= 0 && r.y >= 0 && r.right() <= clipRect().right() && r.bottom() <= clipRect().bottom());
    backend.bitmap(r, ref, mode, p, rotation);
  }
  struct Label {
    std::string value;
    fui::FontId font;
    fui::Rect box;
  };
  std::vector<Label> labels;
  std::vector<fui::Rect> blackFills;
  void text(fui::Rect box, const char* value, fui::TextStyle style) override {
    CHECK(!value || !std::strchr(value, '\n'));
    fui::layoutText(backend, box, value, style, [&](const char* run, fui::Rect line) {
      if (line.bottom() > box.bottom() || std::strstr(run, "…") || std::strstr(run, "..."))
        std::printf("TEXT [%d,%d %dx%d font%d] %s\n", box.x, box.y, box.width, box.height, int(style.font), value);
      CHECK(line.bottom() <= box.bottom());
      CHECK(!std::strstr(run, "…") && !std::strstr(run, "..."));
    });
    labels.push_back({value ? value : "", style.font, box});
    backend.text(box, value, style);
  }
};
struct Canvas {
  int width, height;
  std::vector<uint8_t> pixels;
  FontBridge tiny{toybox_10}, small{toybox_14}, body{toybox_20}, title{toybox_30}, library{ubuntu_10_regular},
      caption{notosans_8_regular}, prose{ui_prose_14}, reading{reading_serif_14};
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
    target.setFont(4, library.font);
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

struct Bytes : study::ByteSource {
  std::vector<uint8_t> data;
  bool fail = false;
  unsigned reads = 0;
  bool read(uint32_t p, void* out, uint32_t n) override {
    ++reads;
    if (fail || uint64_t(p) + n > data.size()) return false;
    std::memcpy(out, data.data() + p, n);
    return true;
  }
  uint32_t size() const override { return data.size(); }
  void add(int64_t id, int64_t at, int rating, int state = 2, uint8_t flags = 0, uint32_t took = 1000) {
    uint8_t r[32]{};
    std::memcpy(r, &id, 8);
    std::memcpy(r + 8, &at, 8);
    r[16] = rating;
    r[17] = state;
    r[28] = flags;
    std::memcpy(r + 24, &took, 4);
    data.insert(data.end(), r, r + 32);
  }
};
int main(int argc, char** argv) {
  const int64_t now = 1792312800000LL;
  Bytes bytes;
  flashcards::HistoryWork work;
  std::vector<flashcards::Session> sessions;
  bytes.add(1, now, 1, 0, flashcards::kSessionStart);
  bytes.add(2, now + 1000, 4, 0);
  bytes.add(1, now + 2000, 3);
  bytes.add(3, now + 3000, 2, 0, study::kRevlogVoided);
  bytes.add(9, now + 10000, 3, 2, flashcards::kSessionStart);
  CHECK(flashcards::scanHistory(
      bytes, work,
      [](void* c, const flashcards::Session& s) { static_cast<std::vector<flashcards::Session>*>(c)->push_back(s); },
      &sessions));
  CHECK(sessions.size() == 2);
  CHECK(sessions[0].cards == 1);
  CHECK(sessions[1].cards == 2);
  CHECK(sessions[1].answers == 3);
  CHECK(sessions[1].fresh == 2);
  CHECK(sessions[1].tookMs == 3000);
  CHECK(sessions[1].ratings[0] == 1 && sessions[1].ratings[2] == 1 && sessions[1].ratings[3] == 1);
  flashcards::Session summary;
  CHECK(flashcards::summarize(bytes, sessions[1].first, sessions[1].end, work, summary));
  CHECK(summary.cards == 2 && summary.answers == 3);
  CHECK(!flashcards::summarize(bytes, 1, 32, work, summary));
  bytes.data.push_back(42);
  sessions.clear();
  CHECK(flashcards::scanHistory(
      bytes, work,
      [](void* c, const flashcards::Session& s) { static_cast<std::vector<flashcards::Session>*>(c)->push_back(s); },
      &sessions));
  CHECK(sessions.size() == 2);
  bytes.fail = true;
  CHECK(!flashcards::scanHistory(bytes, work, [](void*, const flashcards::Session&) {}, nullptr));
  Bytes legacy;
  legacy.add(1, now, 3, 2, 0, 0);
  legacy.add(2, now + 3600000, 3, 2, 0, 0);
  sessions.clear();
  CHECK(flashcards::scanHistory(
      legacy, work,
      [](void* c, const flashcards::Session& s) { static_cast<std::vector<flashcards::Session>*>(c)->push_back(s); },
      &sessions));
  CHECK(sessions.size() == 2 && !sessions[0].hasTiming);
  Bytes large;
  for (int n = 1; n <= 300; ++n) large.add(n, now + n, 3, 0, n == 1 ? flashcards::kSessionStart : 0);
  large.add(1, now + 7200000, 4, 2);
  sessions.clear();
  CHECK(flashcards::scanHistory(
      large, work,
      [](void* c, const flashcards::Session& s) { static_cast<std::vector<flashcards::Session>*>(c)->push_back(s); },
      &sessions));
  CHECK(sessions.size() == 1 && sessions[0].cards == 300 && sessions[0].answers == 301 && sessions[0].fresh == 300);
  CHECK(flashcards::summarize(large, 0, large.size(), work, summary));
  int64_t lastId = 0;
  CHECK(flashcards::reviewedCard(large, summary, 299, work, lastId) && lastId == 1);
  CHECK(!flashcards::reviewedCard(large, summary, 300, work, lastId));
  Bytes voidBoundary;
  voidBoundary.add(1, now, 4, 2, flashcards::kSessionStart);
  voidBoundary.add(2, now + 10000, 3, 0, flashcards::kSessionStart | study::kRevlogVoided);
  voidBoundary.add(2, now + 11000, 4, 0);
  sessions.clear();
  CHECK(flashcards::scanHistory(
      voidBoundary, work,
      [](void* c, const flashcards::Session& s) { static_cast<std::vector<flashcards::Session>*>(c)->push_back(s); },
      &sessions));
  CHECK(sessions.size() == 2 && sessions[0].cards == 1 && sessions[1].cards == 1);
  Bytes many;
  for (int n = 1; n <= 5001; ++n) many.add(n, now + n, 3, 0);
  flashcards::BlockSource blocked(many, work.readCache, sizeof(work.readCache));
  sessions.clear();
  CHECK(flashcards::scanHistory(
      blocked, work,
      [](void* c, const flashcards::Session& s) { static_cast<std::vector<flashcards::Session>*>(c)->push_back(s); },
      &sessions));
  CHECK(sessions.size() == 1 && sessions[0].cards == 5001 && sessions[0].fresh == 5001);
  CHECK(many.reads < 4000);
  std::printf("Legacy 5001-card history: %u physical block reads\n", many.reads);
  Bytes cross;
  cross.data.resize(100);
  for (int n = 0; n < 100; ++n) cross.data[n] = n;
  uint8_t cache[16], out[12];
  flashcards::BlockSource blocks(cross, cache, sizeof(cache));
  CHECK(blocks.read(14, out, 12) && out[0] == 14 && out[11] == 25);
  CHECK(blocks.read(0, out, 12) && out[0] == 0 && out[11] == 11);
  CHECK(blocks.read(30, out, 6) && out[0] == 30);
  CHECK(blocks.read(18, out, 6) && out[0] == 18);
  CHECK(blocks.read(0, out, 12) && out[0] == 0);
  CHECK(!blocks.read(99, out, 2));
  for (auto lang : {Language::EN, Language::ES}) {
    I18N.setLanguage(lang);
    for (auto dim : {std::array<int, 2>{480, 800}, std::array<int, 2>{800, 480}}) {
      int w = dim[0], h = dim[1];
      const auto saveScreen = [&](Canvas& c, const char* n) {
        c.checkHits();
        if (argc > 1) {
          const std::string file = std::string(argv[1]) + "/" + (lang == Language::ES ? "es" : "en") + "-" +
                                   std::to_string(w) + "-" + n + ".pbm";
          c.write(file);
        }
      };
      flashui::DeckRow rows[] = {{"Ciencia general", 120, 12, 8, true, false},
                                 {"Inglés cotidiano", 85, 7, 5, true, false},
                                 {"Conceptos clave", 42, 3, 2, true, false}};
      {
        Canvas c(w, h);
        flashui::LibraryModel m;
        m.rows = rows;
        m.count = m.totalDecks = 3;
        m.today = 37;
        c.target.setFont(4, c.caption.font);
        flashui::library(c.screen, m);
        for (const auto& l : c.target.labels) CHECK(l.value.find("/flashcards") == std::string::npos);
        saveScreen(c, "library");
      }
      {
        Canvas c(w, h);
        c.target.setFont(4, c.caption.font);
        flashui::library(c.screen, {});
        saveScreen(c, "empty");
      }
      {
        Canvas c(w, h);
        flashui::LibraryModel m;
        m.rows = rows;
        m.count = 3;
        m.totalDecks = 7;
        m.pages = 3;
        m.page = 1;
        m.today = 37;
        c.target.setFont(4, c.caption.font);
        flashui::library(c.screen, m);
        saveScreen(c, "library-paged");
      }
      for (auto phase :
           {flashcards::ImportPhase::Checking, flashcards::ImportPhase::Unpacking,
            flashcards::ImportPhase::Decompressing, flashcards::ImportPhase::Cards, flashcards::ImportPhase::Images,
            flashcards::ImportPhase::Saving, flashcards::ImportPhase::SavingImages}) {
        Canvas c(w, h);
        pixelui::begin(c.screen);
        pixelui::Header header;
        header.title = tr(STR_FLASHCARDS_APP);
        header.back = true;
        pixelui::header(c.screen, header);
        flashui::ImportModel m;
        m.progress = {phase, 1234, phase == flashcards::ImportPhase::Checking ? 5000u : 0u};
        m.cards = 142;
        m.skipped = 3;
        m.elapsedMs = 65000;
        m.tick = 3;
        flashui::importing(c.screen, m);
        const auto b = c.screen.body();
        dashboard::action(c.screen, fui::makeRect(b.x, b.bottom() - 56, b.width, 56), tr(STR_FLASH_BACK_DECKS),
                          flashui::Back, &extra_dashboard_cards_32);
        for (const auto& label : c.target.labels)
          CHECK(label.box.x >= 0 && label.box.y >= 0 && label.box.right() <= w && label.box.bottom() <= h);
        CHECK(c.label(tr(STR_FLASH_IMPORT_KEEP_SD)));
        saveScreen(c, phase == flashcards::ImportPhase::Checking ? "import-checking"
                      : phase == flashcards::ImportPhase::Images ? "import-images"
                                                                 : "import-other");
      }
      for (int p = 0; p < 2; ++p) {
        Canvas c(w, h);
        flashui::help(c.screen, p);
        if (p) {
          const RecordingTarget::Label* question = nullptr;
          const RecordingTarget::Label* answer = nullptr;
          char questionText[48], answerText[48];
          pixelui::copyCaps(questionText, sizeof(questionText), tr(STR_FLASH_QUESTION));
          pixelui::copyCaps(answerText, sizeof(answerText), tr(STR_FLASH_ANSWER));
          for (const auto& label : c.target.labels) {
            if (label.value == questionText) question = &label;
            if (label.value == answerText) answer = &label;
          }
          CHECK(question && answer);
          if (question && answer) {
            CHECK(question->box.y == answer->box.y && question->box.width == answer->box.width);
            const int gapLeft = question->box.right() + 8, gapRight = answer->box.x - 8;
            CHECK(gapRight - gapLeft == 32);
            bool arrowInGap = false;
            for (const auto& line : c.target.lines)
              if (line.a.x >= gapLeft && line.b.x <= gapRight && line.a.y < question->box.y &&
                  line.b.y < question->box.y)
                arrowInGap = true;
            CHECK(arrowInGap);
          }
        }
        saveScreen(c, p ? "help2" : "help1");
      }
      {
        Canvas c(w, h);
        studyui::DeckModel m;
        m.localOnly = true;
        m.name = rows[0].name;
        m.total = 120;
        m.fresh = 8;
        m.due = 12;
        int data[14] = {2, 3, 1, 5, 7, 3, 4};
        m.history = data;
        m.weekday = 6;
        studyui::buildDeck(c.screen, m);
        saveScreen(c, "deck");
      }
      for (bool answer : {false, true}) {
        Canvas c(w, h);
        studyui::CardModel m;
        m.deck = rows[0].name;
        m.remaining = 20;
        m.fresh = 8;
        m.due = 12;
        m.answer = answer;
        m.pages = 3;
        m.canUndo = true;
        m.question = "¿Qué estrella ilumina la Tierra?";
        for (int n = 0; n < 4; ++n) std::snprintf(m.delays[n], sizeof(m.delays[n]), "%d min", n * 3 + 1);
        studyui::buildCard(c.screen, m);
        const auto layout = studyui::cardLayout(c.screen.device(), answer, true, false);
        CHECK(layout.pager.height == 32 && layout.pager.width <= 224);
        CHECK(layout.content.bottom() <= layout.pager.y);
        if (answer) {
          if (w < h) {
            CHECK(layout.ratings[0].y - layout.face.bottom() >= 8);
            CHECK(layout.ratings[0].y - layout.face.bottom() <= 16);
          }
          for (const auto& label : c.target.labels) {
            CHECK(label.value.find("RECORDABAS") == std::string::npos);
            CHECK(label.value.find("REMEMBER") == std::string::npos);
            CHECK(label.value.find("Elige una") == std::string::npos);
            CHECK(label.value.find("Choose a rating") == std::string::npos);
          }
        }
        saveScreen(c, answer ? "answer" : "question");
      }
      flashui::SessionModel sm;
      sm.deck = rows[0].name;
      sm.date = "18 OCT 2026 · 08:40";
      sm.session.cards = 20;
      sm.session.answers = 24;
      sm.session.fresh = 8;
      sm.session.hasTiming = true;
      sm.session.tookMs = 520000;
      sm.session.ratings[0] = 4;
      sm.session.ratings[1] = 3;
      sm.session.ratings[2] = 14;
      sm.session.ratings[3] = 3;
      {
        Canvas c(w, h);
        flashui::session(c.screen, sm);
        saveScreen(c, "session");
      }
      {
        Canvas c(w, h);
        flashui::completed(c.screen, sm);
        saveScreen(c, "completed");
      }
      {
        Canvas c(w, h);
        flashui::HistoryRow items[3];
        for (int n = 0; n < 3; ++n) items[n] = {rows[n].name, "Hoy · 08:40", sm.session};
        flashui::HistoryModel m;
        m.rows = items;
        m.count = 3;
        m.sessions = 3;
        m.cards = 60;
        m.month = "OCTUBRE 2026";
        m.filter = "Todos los mazos";
        flashui::history(c.screen, m);
        saveScreen(c, "history");
      }
      {
        Canvas c(w, h);
        flashui::CardsModel m;
        m.deck = rows[0].name;
        m.count = 4;
        for (auto& label : m.labels) label = "A complete study question";
        flashui::cards(c.screen, m);
        saveScreen(c, "cards");
      }
    }
  }
  {
    Canvas c(480, 800);
    std::string longText;
    for (int n = 0; n < 200; ++n) longText += "Complete question ¿sí? ";
    longText += "FINAL";
    char line[640];
    const auto box = fui::makeRect(20, 100, 440, 240);
    int pages = flashui::textPage(c.target, box, longText.c_str(), -1, line, sizeof(line));
    CHECK(pages > 1);
    flashui::textPage(c.target, box, longText.c_str(), pages - 1, line, sizeof(line));
    bool found = false;
    for (auto& l : c.target.labels) found |= l.value.find("FINAL") != std::string::npos;
    CHECK(found);
  }
  for (auto size : {fui::Size{480, 800}, fui::Size{800, 480}}) {
    Canvas c(size.width, size.height);
    studyui::CardModel m;
    m.deck = "Stress";
    m.answer = true;
    m.hasPhoto = true;
    m.pages = 5;
    studyui::buildCard(c.screen, m);
    auto l = studyui::cardLayout(c.screen.device(), true, false, true);
    CHECK(l.photo.width > 0 && l.photo.height > 0);
    CHECK(l.content.bottom() <= l.photo.y);
    CHECK(l.photo.y == l.pager.y && l.photo.height == l.pager.height);
    CHECK(l.photo.right() == l.face.right() - 12);
    CHECK(l.pager.right() + 10 <= l.photo.x);
    const auto withoutPhoto = studyui::cardLayout(c.device, m.answer, m.canUndo, false);
    CHECK(l.content.height == withoutPhoto.content.height);
    if (argc > 1) c.write(std::string(argv[1]) + "/photo-" + std::to_string(size.width) + ".pbm");
    for (int length : {1000, 5000, 20000, 50000}) {
      std::string value(length, 'W');
      value += "FINAL";
      char scratch[640];
      auto box = fui::makeRect(20, 100, size.width - 40, 200);
      int pages = flashui::textPage(c.target, box, value.c_str(), -1, scratch, sizeof(scratch));
      CHECK(pages > 1);
      c.target.labels.clear();
      flashui::textPage(c.target, box, value.c_str(), pages - 1, scratch, sizeof(scratch));
      std::string shown;
      for (const auto& label : c.target.labels) shown += label.value;
      CHECK(!shown.empty() && shown.size() <= value.size());
      CHECK(value.substr(value.size() - shown.size()) == shown);
      if (length == 50000) {
        std::string complete;
        for (int page = 0; page < pages; ++page) {
          c.target.labels.clear();
          flashui::textPage(c.target, box, value.c_str(), page, scratch, sizeof(scratch));
          for (const auto& label : c.target.labels) complete += label.value;
        }
        CHECK(complete == value);
      }
    }
  }

  for (auto language : {Language::EN, Language::ES}) {
    I18N.setLanguage(language);
    for (auto dimensions : {fui::Size{480, 800}, fui::Size{800, 480}}) {
      for (auto app : {flashcards::TextApp::Flashcards, flashcards::TextApp::Words}) {
        for (bool failed : {false, true}) {
          Canvas c(dimensions.width, dimensions.height);
          c.target.setFont(2, c.reading.font);
          c.target.setFont(4, c.reading.font);
          flashui::TextSettingsModel model;
          model.app = app;
          model.error = failed;
          flashui::textSettings(c.screen, model);
          CHECK(c.has(flashui::SmartSize) && c.has(flashui::SettingsBack));
          for (int n = 0; n < flashcards::kTextSizeCount; ++n)
            CHECK(c.has(flashui::QuestionSize, n) && c.has(flashui::AnswerSize, n));
          c.checkHits();
        }
      }
      Canvas header(dimensions.width, dimensions.height);
      flashui::header(header.screen, tr(STR_FLASHCARDS_APP));
      CHECK(header.has(flashui::Settings));
      header.checkHits();
    }
  }
  {
    Canvas c(480, 800);
    std::string source;
    for (int n = 0; n < 500; ++n) source += "Complete question ";
    source += "FINAL";
    char line[640];
    flashui::TextPageCache cache;
    const auto box = fui::makeRect(20, 100, 440, 80);
    const int pages = flashui::textPage(c.target, box, source.c_str(), -1, line, sizeof(line), &cache, 0);
    CHECK(pages > 10);
    for (int page : {0, pages / 2, pages - 1}) {
      c.target.labels.clear();
      c.target.measurements = 0;
      flashui::textPage(c.target, box, source.c_str(), page, line, sizeof(line), &cache, 0);
      const auto cached = c.target.labels;
      const auto reads = c.target.measurements;
      c.target.labels.clear();
      c.target.measurements = 0;
      flashui::textPage(c.target, box, source.c_str(), page, line, sizeof(line));
      CHECK(cached.size() == c.target.labels.size());
      for (size_t n = 0; n < cached.size(); ++n) CHECK(cached[n].value == c.target.labels[n].value);
      CHECK(reads * 5 < c.target.measurements);
      std::printf("Page %d: cached %zu measurements, uncached %zu\n", page, reads, c.target.measurements);
    }
    c.target.measurements = 0;
    CHECK(flashui::textPage(c.target, box, source.c_str(), -1, line, sizeof(line), &cache, 0) == pages);
    CHECK(c.target.measurements == 0);
    cache.invalidate();
    CHECK(!cache.pages);
  }
  for (auto dimensions : {fui::Size{480, 800}, fui::Size{800, 480}}) {
    // Check drawn arrow shafts against their hit boxes in both device layouts.
    constexpr fui::Point expected[4][4] = {
        {{-1, 0}, {0, -1}, {0, 1}, {1, 0}},
        {{0, -1}, {1, 0}, {-1, 0}, {0, 1}},
        {{1, 0}, {0, 1}, {0, -1}, {-1, 0}},
        {{0, 1}, {-1, 0}, {1, 0}, {0, -1}},
    };
    for (int turns = 0; turns < 4; ++turns) {
      Canvas arrows(dimensions.width, dimensions.height);
      studyui::buildImage(arrows.screen, 2, turns);
      int found = 0;
      for (size_t n = 0; n < arrows.interactions.count(); ++n) {
        const auto& slot = arrows.interactions.data()[n];
        if (slot.action != studyui::ImageStep) continue;
        ++found;
        CHECK(slot.value >= 0 && slot.value < 4);
        const auto direction = expected[turns][slot.value];
        const int cx = slot.rect.x + slot.rect.width / 2, cy = slot.rect.y + slot.rect.height / 2;
        bool shaft = false;
        for (const auto& line : arrows.target.lines)
          shaft |= line.a.x == cx - direction.x * 9 && line.a.y == cy - direction.y * 9 &&
                   line.b.x == cx + direction.x * 9 && line.b.y == cy + direction.y * 9;
        CHECK(shaft);
      }
      CHECK(found == 4);
      arrows.checkHits();
      if (argc > 1)
        arrows.write(std::string(argv[1]) + "/image-arrows-" + std::to_string(dimensions.width) + "-" +
                     std::to_string(turns) + ".pbm");
    }
    for (auto language : {Language::EN, Language::ES}) {
      I18N.setLanguage(language);
      Canvas c(dimensions.width, dimensions.height);
      studyui::buildImage(c.screen, 2);
      CHECK(c.has(studyui::ImageZoom, -1));
      CHECK(c.has(studyui::ImageZoom, 1));
      CHECK(c.has(studyui::ImageRotate));
      CHECK(c.has(studyui::ImageFit));
      CHECK(c.has(studyui::ImagePan));
      const auto viewport = studyui::imageViewport(c.device);
      CHECK(viewport.width > 0 && viewport.height > 0);
      c.checkHits();
      if (argc > 1)
        c.write(std::string(argv[1]) + "/anki-image-controls-" + std::to_string(dimensions.width) + "-" +
                std::to_string(int(language)) + ".pbm");
    }
  }
  {
    const pixelui::Header legacy{"Legacy", nullptr, nullptr, {}, -1, false, true, true};
    CHECK(legacy.battery == -1 && !legacy.charging);
    CHECK(legacy.showPercent && legacy.back);
    CHECK(legacy.markAction == fui::NO_ACTION);
  }
  std::printf("Flashcards: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
