#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../../src/apps_local/home/HomeScreen.h"
#include "../../src/apps_local/notes/NotesScreens.h"
#include "../../src/apps_local/notes/NotesTimestamp.h"
#include "../../src/apps_local/quotes/ClippingsLine.h"
#include "../../src/apps_local/quotes/QuotesCore.h"
#include "../../src/apps_local/quotes/QuotesScreens.h"
#include "../../src/apps_local/ui/ReadingToolsUi.h"
#include "../../src/apps_local/ui/ToyboxIcons.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../lib/EpdFont/builtinFonts/ubuntu_10_regular.h"
#include "../../src/apps_local/ui/fonts/instrument_24.h"
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
  void line(fui::Point a, fui::Point b, uint8_t w, fui::Paint p) override { backend.line(a, b, w, p); }
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
  bool previews = false, home = false;
  void text(fui::Rect box, const char* value, fui::TextStyle style) override {
    fui::layoutText(backend, box, value, style, [&](const char* run, fui::Rect line) {
      if ((!home && line.bottom() > box.bottom()) ||
          (!home && !previews && (std::strstr(run, "…") || std::strstr(run, "..."))))
        std::printf("TEXT [%d,%d %dx%d font%d] %s\n", box.x, box.y, box.width, box.height, int(style.font), value);
      CHECK(home || line.bottom() <= box.bottom());
      CHECK(previews || home || (!std::strstr(run, "…") && !std::strstr(run, "...")));
    });
    labels.push_back({value ? value : "", style.font, box});
    backend.text(box, value, style);
  }
};
struct Canvas {
  int width, height;
  std::vector<uint8_t> pixels;
  FontBridge regular{ui_prose_10}, tiny{toybox_10}, small{toybox_14}, body{toybox_20}, title{toybox_30},
      library{ubuntu_10_regular}, prose{ui_prose_14}, reading{instrument_24};
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
    target.setFont(readingui::kReadingFont, prose.font);
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
        if (!(r.x >= a.right() || r.right() <= a.x || r.y >= a.bottom() || r.bottom() <= a.y))
          std::printf("OVERLAP action%d %d,%d %dx%d and action%d %d,%d %dx%d\n", slot.action, r.x, r.y, r.width,
                      r.height, interactions.data()[j].action, a.x, a.y, a.width, a.height);
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
  for (auto lang : {Language::EN, Language::ES}) {
    I18N.setLanguage(lang);
    for (auto dims : {std::array<int, 2>{480, 800}, std::array<int, 2>{800, 480}}) {
      const int w = dims[0], h = dims[1];
      auto shot = [&](Canvas& c, const std::string& name) {
        c.checkHits();
        if (argc > 1)
          c.write(std::string(argv[1]) + "/" + (lang == Language::ES ? "es" : "en") + "-" + std::to_string(w) + "-" +
                  name + ".pbm");
      };
      notesui::DeckItem items[12]{};
      for (auto& i : items) {
        i.title = "Pendientes de la semana";
        i.preview = "Comprar café y terminar el capítulo.";
      }
      items[3].asleep = true;
      items[3].title = "Recordatorio personal";
      for (int pg = 0; pg < 3; ++pg) {
        Canvas c(w, h);
        notesui::DeckModel m;
        m.items = items;
        m.count = 12;
        m.firstVisible = pg * notesui::deckCapacity(c.device, false);
        c.target.previews = true;
        notesui::buildDeck(c.screen, m);
        for (const auto& label : c.target.labels)
          if (label.value == "Recordatorio personal")
            CHECK(label.box.right() <= pixelui::geometry(c.device).body.right() - 88);
        shot(c, "notes" + std::to_string(pg));
      }
      notesui::Task tasks[] = {{"Comprar café y preparar lo necesario para mañana.", false},
                               {"Terminar el capítulo y guardar las ideas más importantes.", false},
                               {"Revisar el diseño de Notas y probarlo en el lector.", false}};
      {
        Canvas measure(w, h);
        notesui::NoteModel m;
        m.page = true;
        m.title = "Pendientes de la semana";
        m.updated = "18 OCT 2026 · 21:30";
        m.tasks = tasks;
        m.count = 3;
        m.menuIcon = &icon_go_settings_32;
        const auto starts = notesui::notePageStarts(measure.target, measure.device, m);
        for (size_t pg = 0; pg < starts.size(); ++pg) {
          Canvas c(w, h);
          m.firstVisible = starts[pg];
          notesui::buildNote(c.screen, m);
          shot(c, "note" + std::to_string(pg));
        }
      }
      {
        Canvas c(w, h);
        notesui::MenuModel m;
        m.title = "Pendientes de la semana";
        notesui::buildMenu(c.screen, m);
        shot(c, "note-options");
      }
      {
        Canvas c(w, h);
        notesui::ConfirmModel m;
        m.title = "Pendientes";
        m.prose = tr(STR_UI_DELETE_THIS_NOTE_AND_EVERYTHING_WRITTEN_D1E983);
        m.reference = true;
        notesui::buildConfirm(c.screen, m);
        shot(c, "note-confirm");
      }
      {
        Canvas menu(w, h), confirm(w, h);
        notesui::MenuModel m;
        m.title = "Pendientes";
        notesui::buildMenu(menu.screen, m);
        notesui::ConfirmModel q;
        q.reference = true;
        q.prose = tr(STR_UI_DELETE_THIS_NOTE_AND_EVERYTHING_WRITTEN_D1E983);
        notesui::buildConfirm(confirm.screen, q);
        for (size_t n = 0; n < menu.interactions.count(); ++n)
          if (menu.interactions.data()[n].action == notesui::ActionDelete) {
            const auto r = menu.interactions.data()[n].rect;
            fui::InputSnapshot tap{};
            tap.touchReleased = true;
            tap.touchX = r.x + r.width / 2;
            tap.touchY = r.y + r.height / 2;
            paintclock::notePainted();
            CHECK(confirm.interactions.route(tap).action != notesui::ActionDelete);
          }
      }
      quotesui::Row rows[5]{};
      for (int n = 0; n < 5; ++n) {
        auto& r = rows[n];
        readingui::copyText(r.title, sizeof(r.title), "Ciudad de papel");
        readingui::copyText(r.author, sizeof(r.author), "Lucía Prado");
        readingui::copyText(r.preview, sizeof(r.preview), "Las historias encuentran espacio incluso en los márgenes.");
        r.page = 5 + n;
        r.index = n;
        r.count = 8;
        r.favorite = n % 2;
      }
      for (int mode = 0; mode < 4; ++mode) {
        Canvas c(w, h);
        quotesui::ListModel m;
        m.rows = rows;
        m.visible = quotesui::capacity(c.device);
        m.total = 24;
        m.books = 6;
        m.favorites = 5;
        m.tab = mode % 3;
        m.inBook = mode == 3;
        m.title = "Ciudad de papel";
        m.author = "Lucía Prado";
        m.bookTotal = 8;
        m.bookFavorites = 2;
        m.pages = 5;
        c.target.previews = true;
        quotesui::list(c.screen, m);
        shot(c, "quotes" + std::to_string(mode));
      }
      {
        Canvas c(w, h);
        c.target.setFont(readingui::kReadingFont, c.reading.font);
        quotesui::DetailModel m;
        m.title = "Ciudad de papel";
        m.author = "Lucía Prado";
        m.body = "Las historias encuentran espacio incluso en los márgenes.";
        m.page = 5;
        m.total = 24;
        m.favorite = true;
        pagedtext::Work work;
        quotesui::detail(c.screen, m, work);
        shot(c, "quote-detail");
      }
      {
        Canvas c(w, h);
        quotesui::message(c.screen, tr(STR_RT_DELETE_CONFIRM), true);
        shot(c, "quote-confirm");
      }
      {
        Canvas detail(w, h), confirm(w, h);
        quotesui::DetailModel m;
        m.title = "Ciudad de papel";
        m.author = "Lucía Prado";
        m.body = "Texto";
        m.total = 1;
        pagedtext::Work work;
        quotesui::detail(detail.screen, m, work);
        quotesui::message(confirm.screen, tr(STR_RT_DELETE_CONFIRM), true);
        for (size_t n = 0; n < detail.interactions.count(); ++n)
          if (detail.interactions.data()[n].action == quotesui::Remove) {
            const auto r = detail.interactions.data()[n].rect;
            fui::InputSnapshot tap{};
            tap.touchReleased = true;
            tap.touchX = r.x + r.width / 2;
            tap.touchY = r.y + r.height / 2;
            paintclock::notePainted();
            CHECK(confirm.interactions.route(tap).action != quotesui::ConfirmRemove);
          }
      }
      {
        Canvas c(w, h);
        c.target.setFont(readingui::kReadingFont, c.reading.font);
        quotesui::DetailModel m;
        m.title = "Libro";
        m.author = "Autor";
        std::string longText;
        for (int n = 0; n < 100; ++n) longText += "Un texto extenso para paginar todas las palabras. ";
        m.body = longText.c_str();
        m.total = 1;
        pagedtext::Work work;
        const int pages = quotesui::detail(c.screen, m, work);
        CHECK(pages > 1);
        CHECK(c.has(quotesui::Page, 1));
        c.checkHits();
        Canvas next(w, h);
        next.target.setFont(readingui::kReadingFont, next.reading.font);
        m.bodyPage = 1;
        quotesui::detail(next.screen, m, work);
        CHECK(next.has(quotesui::Page, -1));
        next.checkHits();
        Canvas clamped(w, h);
        clamped.target.setFont(readingui::kReadingFont, clamped.reading.font);
        m.bodyPage = 999;
        quotesui::detail(clamped.screen, m, work);
        CHECK(clamped.has(quotesui::Page, -1));
        CHECK(!clamped.has(quotesui::Page, 1));
        clamped.checkHits();
      }
      for (auto style : {homeui::Style::Framed, homeui::Style::Carousel}) {
        Canvas c(w, h);
        homeui::Model m;
        m.style = style;
        m.hasBook = true;
        m.progress = 22;
        m.recentCount = 5;
        std::strcpy(m.continueLabel, "SEGUIR LEYENDO");
        std::strcpy(m.title, "ALICE'S ADVENTURES IN WONDERLAND");
        std::strcpy(m.author, "LEWIS CARROLL");
        auto g = homeui::layout(c.device, 6, style);
        CHECK(g.author.bottom() + 8 <= g.progress.y || style == homeui::Style::Framed);
        if (style == homeui::Style::Carousel) {
          CHECK(g.bookProgress.x >= g.cover.x && g.bookProgress.right() <= g.cover.right());
          CHECK(g.bookProgress.y >= g.cover.y && g.bookProgress.bottom() <= g.cover.bottom());
        }
        c.target.setFont(toybox::kDisplayFont, c.title.font);
        c.target.home = true;
        homeui::draw(c.target, m, g);
        homeui::drawCardSelection(c.target, m, g);
        shot(c, style == homeui::Style::Carousel ? "carousel" : "frame");
      }
    }
  }
  char raw[] =
      "Ciudad de papel (Lucía Prado)\n- Your Highlight on Page 5 | Added on Sunday, October 18, 2026, 08:40 AM\n\nLas "
      "historias encuentran espacio.\n==========\n";
  quotes::Quote q;
  quotes::parse(raw, q);
  CHECK(!std::strcmp(q.title, "Ciudad de papel"));
  CHECK(!std::strcmp(q.author, "Lucía Prado"));
  CHECK(q.page == 5);
  CHECK(!std::strcmp(q.body, "Las historias encuentran espacio."));
  notes::ModifiedDate date;
  const auto packed = [](int y, int m, int d, int h, int min) {
    return uint32_t(((y - 1980) << 9) | (m << 5) | d) << 16 | uint32_t((h << 11) | (min << 5));
  };
  CHECK(notes::decodeModified(packed(2026, 10, 18, 21, 30), date));
  CHECK(date.year == 2026 && date.month == 10 && date.day == 18 && date.hour == 21 && date.minute == 30);
  CHECK(!notes::decodeModified(0, date));
  CHECK(!notes::decodeModified(packed(2026, 2, 30, 12, 0), date));
  CHECK(notes::decodeModified(packed(2024, 2, 29, 0, 0), date));
  for (const char* text : {"x\n==========\n", "x\r\n==========\r\n", "x\n==========", "x\n========="}) {
    quotes::ClippingsLine scan;
    int records = 0;
    for (const char* p = text; *p; ++p)
      if (scan.consume(*p)) ++records;
    if (scan.consume('\n')) ++records;
    CHECK(records == (std::strcmp(text, "x\n=========") == 0 ? 0 : 1));
  }
  std::printf("Reading tools: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
