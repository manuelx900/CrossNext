#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "../../src/apps_local/pet/PetAlbumWidgets.h"
#include "../../src/apps_local/pet/PetCareAdvice.h"
#include "../../src/apps_local/pet/PetCatalogView.h"
#include "../../src/apps_local/pet/PetHabitat.h"
#include "../../src/apps_local/pet/PetHeader.h"
#include "../../src/apps_local/pet/PetOverview.h"
#include "../../src/apps_local/pet/PetReadingResults.h"
#include "../../src/apps_local/pet/PetScreens.h"
#include "../../src/apps_local/pet/PetSleepAnimation.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../src/apps_local/ui/fonts/toybox_10.h"
#include "../../src/apps_local/ui/fonts/toybox_14.h"
#include "../../src/apps_local/ui/fonts/toybox_20.h"
#include "../../src/apps_local/ui/fonts/toybox_30.h"
#include "../../src/apps_local/ui/fonts/ui_prose_10.h"
#pragma GCC diagnostic pop

namespace fui = freeink::ui;
namespace {
int checks = 0, failures = 0;
#define CHECK(expr)                                                      \
  do {                                                                   \
    ++checks;                                                            \
    if (!(expr)) {                                                       \
      if (failures < 140) std::printf("FAIL %d: %s\n", __LINE__, #expr); \
      ++failures;                                                        \
    }                                                                    \
  } while (0)
struct FontBridge {
  std::array<fui::FontGlyph, 224> glyphs{};
  fui::BitmapFont font{};
  explicit FontBridge(const EpdFontData& source) {
    for (uint32_t cp = 32; cp <= 255; ++cp)
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
    font = {source.bitmap, glyphs.data(), 32, 255, source.advanceY, static_cast<uint8_t>(source.ascender), 64, 64, 1};
  }
};
// DisplayTarget's compact host bridge covers Latin-1; retain real glyphs above it.
struct StatusArrowSource : fui::RuntimeGlyphSource {
  std::array<uint8_t, 4096> coverage{};
  int16_t advance(uint32_t codepoint, uint16_t pixelSize) override {
    Glyph value{};
    return glyph(codepoint, pixelSize, &value) ? value.advance : 0;
  }
  bool glyph(uint32_t codepoint, uint16_t pixelSize, Glyph* out) override {
    if (codepoint != 0x2192 || !out) return false;
    for (const auto* source : {&toybox_10, &toybox_14, &toybox_20, &toybox_30, &ui_prose_10}) {
      if (source->advanceY != pixelSize) continue;
      for (uint32_t i = 0; i < source->intervalCount; ++i) {
        const auto& interval = source->intervals[i];
        if (codepoint < interval.first || codepoint > interval.last) continue;
        const auto& g = source->glyph[interval.offset + codepoint - interval.first];
        CHECK(unsigned(g.width) * g.height <= coverage.size());
        for (unsigned n = 0; n < unsigned(g.width) * g.height; ++n)
          coverage[n] = source->bitmap[g.dataOffset + n / 8] & (128 >> (n % 8)) ? 255 : 0;
        *out = {coverage.data(), g.width, g.height, g.left, int16_t(-g.top), int16_t(fp4::toPixel(g.advanceX))};
        return true;
      }
    }
    return false;
  }
};
struct Target : fui::DrawTarget {
  fui::DisplayTarget backend;
  StatusArrowSource arrowSource;
  int width, height;
  const char* view = "";
  const char* expectedRegularText = nullptr;
  const char* expectedEmphasizedText = nullptr;
  bool regularTextSeen = false, emphasizedTextSeen = false;
  bool watchStageLabels = false;
  unsigned stageLabels = 0;
  bool watchPlacedBand = false;
  fui::Rect placedBand{};
  const char* expectedFocusName = nullptr;
  bool focusedLabelWasWhite = false;
  bool eggTextSeen = false, vitalValueSeen = false, generationSeen = false, dailyGoalSeen = false;
  const char* expectedDailyTime = nullptr;
  const char* expectedDailyGoal = nullptr;
  bool dailyTimeSeen = false, traceBitmap = false, watchDarkCard = false;
  fui::Rect darkCard{};
  bool watchDashboardCards = false, watchReadingRules = false;
  fui::Rect metricFrames[4]{}, readingCard{};
  unsigned metricFrameCount = 0, readingRuleCount = 0;
  const char* expectedFullText = nullptr;
  bool expectedFullTextSeen = false;
  std::vector<fui::Color> tracedColors;
  Target(uint8_t* data, int w, int h)
      : backend(data, w, h, (w + 7) / 8, fui::Orientation::LandscapeCounterClockwise), width(w), height(h) {
    backend.setGlyphFallback(&arrowSource);
  }
  fui::Size measureText(fui::FontId f, const char* v, fui::TextStyle st) const override {
    return backend.measureText(f, v, st);
  }
  int16_t lineHeight(fui::FontId f) const override { return backend.lineHeight(f); }
  fui::Rect inkBounds(fui::FontId f, const char* v, fui::TextStyle st) const override {
    return backend.inkBounds(f, v, st);
  }
  fui::Rect clipRect() const override { return backend.clipRect(); }
  bool setClipRect(fui::Rect r) override { return backend.setClipRect(r); }
  void fill(fui::Rect r, fui::Paint p, uint8_t radius = 0, uint8_t corners = fui::CornersAll) override {
    if (!std::strcmp(view, "evolution") && r.width > 100 && r.width < 210 && r.height > 60 && r.height < 180)
      CHECK(p.color != fui::Color::Black);
    if (watchDarkCard && darkCard.empty() && p.color == fui::Color::Black && r.width > 200 && r.height > 150)
      darkCard = r;
    if (watchPlacedBand && p.color == fui::Color::Black && r.width > 150 && (r.height == 40 || r.height == 54))
      placedBand = r;
    backend.fill(r, p, radius, corners);
  }
  void stroke(fui::Rect r, fui::Paint p, uint8_t w, uint8_t radius = 0, uint8_t corners = fui::CornersAll) override {
    if (watchDashboardCards && w == 1 && r.width < 180 && r.height >= 132 && metricFrameCount < 4)
      metricFrames[metricFrameCount++] = r;
    if (watchReadingRules && r.x > width / 2 - 20 && r.height > 150) readingCard = r;
    backend.stroke(r, p, w, radius, corners);
  }
  void line(fui::Point a, fui::Point b, uint8_t w, fui::Paint p) override { backend.line(a, b, w, p); }
  void triangle(fui::Point a, fui::Point b, fui::Point c, fui::Paint p) override { backend.triangle(a, b, c, p); }
  void bitmap(fui::Rect r, fui::BitmapRef ref, fui::BitmapMode mode,
              fui::Paint p = fui::Paint::solid(fui::Color::Black),
              fui::Rotation rotation = fui::Rotation::None) override {
    if (traceBitmap) tracedColors.push_back(p.color);
    CHECK(r.width > 0 && r.height > 0);
    CHECK(r.x >= 0 && r.y >= 0 && r.right() <= width && r.bottom() <= height);
    backend.bitmap(r, ref, mode, p, rotation);
  }
  void text(fui::Rect r, const char* value, fui::TextStyle st) override {
    if (watchStageLabels) {
      for (unsigned age = 0; age < 5; ++age) {
        if (std::strcmp(value, petui::stageName(pet::Stage(age)))) continue;
        ++stageLabels;
        CHECK(st.maxLines == 1);
        CHECK(measureText(st.font, value, st).width <= r.width);
        unsigned lines = 0;
        fui::layoutText(*this, r, value, st, [&](const char* run, fui::Rect) {
          ++lines;
          CHECK(!std::strcmp(run, value));
        });
        CHECK(lines == 1);
      }
    }
    if (watchReadingRules &&
        (!std::strcmp(value, tr(STR_PET_READING_INTERVAL)) || !std::strcmp(value, tr(STR_PET_READING_REMEDY)) ||
         !std::strcmp(value, tr(STR_PET_LIFE_GOAL_HELP)))) {
      ++readingRuleCount;
      CHECK(!readingCard.empty() && r.x >= readingCard.x && r.y >= readingCard.y && r.right() <= readingCard.right() &&
            r.bottom() <= readingCard.bottom());
    }
    if (expectedFullText && !std::strcmp(value, expectedFullText)) expectedFullTextSeen = true;
    if (expectedRegularText && !std::strcmp(value, expectedRegularText)) {
      CHECK(st.font == 4);
      regularTextSeen = true;
    }
    if (expectedEmphasizedText && !std::strcmp(value, expectedEmphasizedText)) {
      CHECK(st.font == 0 || st.font == 3);
      emphasizedTextSeen = true;
    }
    if (expectedFocusName && !std::strcmp(expectedFocusName, value) && st.color == fui::Color::White)
      focusedLabelWasWhite = true;
    const bool eggText = !std::strcmp(value, tr(STR_PET_EGG_MOOD)) || !std::strcmp(value, tr(STR_PET_COMPANION_EGG));
    char newborn[64];
    pixelui::copyCaps(newborn, sizeof(newborn), tr(STR_PET_NEWBORN));
    const bool generation = std::strstr(value, "GEN ") == value;
    if (generation) generationSeen = true;
    const bool dailyBenefit = expectedDailyGoal && !std::strcmp(value, expectedDailyGoal);
    const bool dailyTime = expectedDailyTime && !std::strcmp(value, expectedDailyTime);
    dailyGoalSeen = dailyGoalSeen || dailyBenefit;
    dailyTimeSeen = dailyTimeSeen || dailyTime;
    const bool fullText = (expectedFullText && !std::strcmp(value, expectedFullText)) || eggText ||
                          !std::strcmp(value, newborn) || generation || dailyBenefit || dailyTime ||
                          !std::strcmp(value, tr(STR_PET_DIED_REASON)) || !std::strcmp(value, tr(STR_PET_DIED_KEPT));
    const char* remaining = value;
    bool complete = true;
    if (std::strstr(value, "/ 100") || std::strstr(value, "/100")) vitalValueSeen = true;
    fui::layoutText(backend, r, value, st, [&](const char* run, fui::Rect line) {
      if (fullText) {
        if (eggText) eggTextSeen = true;
        while (*remaining == ' ' || *remaining == '\n') ++remaining;
        const auto length = std::strlen(run);
        if (std::strncmp(remaining, run, length))
          complete = false;
        else
          remaining += length;
      }
      const bool fits = line.bottom() <= r.bottom() && !std::strstr(run, "…") && !std::strstr(run, "...");
      if (!fits && failures < 140)
        std::printf("TEXT %s %dx%d [%d,%d %dx%d f%d] %s => %s\n", view, width, height, r.x, r.y, r.width, r.height,
                    int(st.font), value, run);
      CHECK(fits);
    });
    if (!(r.x >= 0 && r.y >= 0 && r.right() <= width && r.bottom() <= height))
      std::printf("BOUNDS %s [%d,%d %dx%d] %s\n", view, r.x, r.y, r.width, r.height, value);
    CHECK(r.x >= 0 && r.y >= 0 && r.right() <= width && r.bottom() <= height);
    if (fullText) {
      while (*remaining == ' ' || *remaining == '\n') ++remaining;
      if ((!complete || *remaining) && failures < 140)
        std::printf("INCOMPLETE %s [%d,%d %dx%d f%d] %s\n", view, r.x, r.y, r.width, r.height, int(st.font), value);
      CHECK(complete && !*remaining);
    }
    backend.text(r, value, st);
  }
};
struct ArtFixture {
  std::vector<uint8_t> bytes;
  explicit ArtFixture(const char* path) {
    FILE* file = std::fopen(path, "rb");
    CHECK(file != nullptr);
    if (!file) return;
    std::fseek(file, 0, SEEK_END);
    const auto size = std::ftell(file);
    std::rewind(file);
    CHECK(size >= 24 && size < 8 * 1024 * 1024);
    if (size >= 24 && size < 8 * 1024 * 1024) {
      bytes.resize(size_t(size));
      CHECK(std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size());
    }
    std::fclose(file);
  }
  uint32_t number(unsigned at, unsigned count) const {
    uint32_t value = 0;
    for (unsigned i = 0; i < count; ++i) value |= uint32_t(bytes[at + i]) << (i * 8);
    return value;
  }
  static bool draw(void* context, fui::DrawTarget& target, fui::Rect area, uint32_t id, bool inverse) {
    const auto& pack = *static_cast<ArtFixture*>(context);
    if (pack.bytes.empty()) return false;
    CHECK(!std::memcmp(pack.bytes.data(), "CNART002", 8));
    CHECK(id < pack.number(8, 4));
    const unsigned at = 24 + id * 16;
    const auto offset = pack.number(at, 4);
    const auto width = pack.number(at + 4, 2), height = pack.number(at + 6, 2);
    CHECK(width > 1 && width <= 280 && height > 1 && height <= 512);
    CHECK(offset + pack.number(at + 8, 4) <= pack.bytes.size());
    const auto mode = pack.number(8, 4) == 45 && id >= 39 ? fui::BitmapMode::Stretch : fui::BitmapMode::Contain;
    target.bitmap(area, {pack.bytes.data() + offset, uint16_t(width), uint16_t(height), fui::BitmapFormat::BW1}, mode,
                  fui::Paint::solid(inverse ? fui::Color::White : fui::Color::Black));
    return true;
  }
};
struct Canvas {
  int width, height;
  std::vector<uint8_t> pixels;
  FontBridge tiny{toybox_10}, small{toybox_14}, body{toybox_20}, title{toybox_30}, prose{ui_prose_10};
  Target target;
  fui::DeviceContext device;
  fui::InputSnapshot input{};
  toybox::Interactions interactions;
  toybox::Frame frame;
  toybox::Screen screen;
  static fui::DeviceContext context(Target& target) {
    auto d = target.backend.deviceContext();
    d.hasTouch = true;
    d.safeArea = {9, 7, 3, 7};
    return d;
  }
  Canvas(int w, int h)
      : width(w),
        height(h),
        pixels((w + 7) / 8 * h, 255),
        target(pixels.data(), w, h),
        device(context(target)),
        frame(target, device, input, interactions, nullptr, {100, false, true}),
        screen(frame) {
    target.backend.setFont(0, small.font);
    target.backend.setFont(1, body.font);
    target.backend.setFont(2, title.font);
    target.backend.setFont(3, tiny.font);
    target.backend.setFont(4, prose.font);
  }
  void paint(const petui::Model& model, petui::Work& work) {
    petui::render(target, device, input, interactions, {100, false, true}, model, work);
  }
  void checkHits() {
    CHECK(!interactions.overflowed());
    paintclock::notePainted();
    for (size_t i = 0; i < interactions.count(); ++i) {
      const auto& slot = interactions.data()[i];
      const auto r = slot.rect;
      if (slot.inputMask & fui::InputCoordinates) continue;
      CHECK(r.x >= 0 && r.y >= 0 && r.right() <= width && r.bottom() <= height);
      CHECK(r.width >= 24 && r.height >= 24);
      fui::InputSnapshot tap;
      tap.touchReleased = true;
      tap.touchX = r.x + r.width / 2;
      tap.touchY = r.y + r.height / 2;
      const auto event = interactions.route(tap);
      if ((event.action != slot.action || event.value != slot.value) && failures < 40)
        std::printf("HIT %s expected %d:%d got %d:%d\n", target.view, slot.action, slot.value, event.action,
                    event.value);
      CHECK(event.action == slot.action && event.value == slot.value);
    }
  }
  void write(const std::string& path) const {
    FILE* out = std::fopen(path.c_str(), "wb");
    if (!out) {
      std::perror(path.c_str());
      std::exit(2);
    }
    std::fprintf(out, "P4\n%d %d\n", width, height);
    uint8_t block[128];
    for (size_t offset = 0; offset < pixels.size(); offset += sizeof(block)) {
      const auto count = std::min(sizeof(block), pixels.size() - offset);
      for (size_t i = 0; i < count; ++i) block[i] = pixels[offset + i] ^ 255;
      CHECK(std::fwrite(block, 1, count, out) == count);
    }
    CHECK(std::fclose(out) == 0);
  }
};
void gameScreens(const char* out) {
  for (const auto lang : {Language::ES, Language::EN}) {
    I18N.setLanguage(lang);
    for (unsigned orientation = 0; orientation < 4; ++orientation) {
      const int width = orientation % 2 ? 800 : 480, height = orientation % 2 ? 480 : 800;
      auto canvas = std::make_unique<Canvas>(width, height);
      canvas->device.orientation = fui::Orientation(orientation);
      pet::Profile p;
      p.created = true;
      p.readingSeconds = 7200;
      petui::Model m;
      m.profile = &p;
      m.tab = pet::Tab::Care;
      m.today = 20000;
      p.joy = 40;
      petui::Work w;
      static constexpr StrId names[] = {StrId::STR_PET_PAIRS, StrId::STR_PET_SEQUENCE, StrId::STR_PET_PUZZLE};
      static constexpr StrId levels[] = {StrId::STR_PET_GAME_EASY, StrId::STR_PET_GAME_MEDIUM,
                                         StrId::STR_PET_GAME_HARD};
      for (unsigned kind = 0; kind < 3; ++kind) {
        m.view = pet::View::Games;
        m.selectedGame = pet::Game(kind);
        canvas->paint(m, w);
        canvas->checkHits();
        char expected[80], caps[80];
        for (const auto key : {names[kind], levels[kind]}) {
          pixelui::copyCaps(caps, sizeof(caps), I18N.get(key));
          canvas->target.expectedFullText = caps;
          canvas->target.expectedFullTextSeen = false;
          canvas->paint(m, w);
          CHECK(canvas->target.expectedFullTextSeen);
        }
        std::snprintf(expected, sizeof(expected), tr(STR_PET_GAME_GAIN_VALUE), 10u + kind * 5);
        pixelui::copyCaps(caps, sizeof(caps), expected);
        canvas->target.expectedFullText = caps;
        canvas->target.expectedFullTextSeen = false;
        canvas->paint(m, w);
        CHECK(canvas->target.expectedFullTextSeen);
        canvas->target.expectedFullText = nullptr;
        for (unsigned state = 0; state < 7; ++state) {
          pet::GameState game;
          game.begin(pet::Game(kind), 123);
          if (kind == 0) {
            if (state == 1) game.select(0);
            if (state == 2) {
              game.select(0);
              unsigned different = 1;
              while (game.cards[different] == game.cards[0]) ++different;
              game.select(different);
            }
            if (state == 3) game.matched = 3;
          } else if (kind == 1) {
            if (state == 1) {
              game.advance();
              game.step = 3;
            }
            if (state == 2) game.mistakes = 4;
            if (state == 3) game.rounds = 1;
            if (state == 4) {
              game.rounds = 2;
              game.advance();
              game.step = 6;
            }
          } else {
            if (state == 1) {
              unsigned blank = 0;
              while (game.puzzle[blank]) ++blank;
              game.select(blank % 3 ? blank - 1 : blank + 1);
            }
            if (state >= 2 && state <= 4) game.preparePuzzle();
          }
          if (state == 5 && kind == 1) {
            game.rounds = 1;
            game.phase = pet::GamePhase::RoundComplete;
          }
          if (state == 5 && kind == 2) {
            static constexpr uint8_t almost[] = {1, 2, 3, 4, 5, 6, 7, 0, 8};
            std::memcpy(game.puzzle, almost, 9);
          }
          if (state == 6) {
            game.rounds = 3;
            game.matched = 63;
            for (unsigned i = 0; i < 9; ++i) game.puzzle[i] = uint8_t((i + 1) % 9);
            game.phase = pet::GamePhase::Finished;
          }
          m.game = &game;
          m.view = pet::View::Play;
          canvas->target.view = "pet-game-reference";
          canvas->target.expectedFullText = kind == 2 && state < 6 ? tr(STR_PET_PUZZLE_HELP) : nullptr;
          canvas->target.expectedFullTextSeen = false;
          canvas->paint(m, w);
          canvas->checkHits();
          if (canvas->target.expectedFullText) CHECK(canvas->target.expectedFullTextSeen);
          unsigned menu = 0, reset = 0, next = 0, done = 0;
          for (size_t hit = 0; hit < canvas->interactions.count(); ++hit) {
            const auto& slot = canvas->interactions.data()[hit];
            menu += slot.action == petui::Back;
            reset += slot.action == petui::GameReset;
            next += slot.action == petui::GameNext;
            done += slot.action == petui::GameDone;
            if (slot.action == petui::GamePick) {
              CHECK(slot.value >= 0 && slot.value < (kind == 2 ? 9 : kind == 0 ? 6 : 3));
              CHECK(slot.rect.width >= 44 && slot.rect.height >= 44);
            }
          }
          CHECK(menu == 1);
          CHECK(done == unsigned(game.finished()));
          if (kind == 2) CHECK(reset == unsigned(game.phase == pet::GamePhase::Playing));
          if (kind == 1 && game.showing()) CHECK(next == 1);
          if (orientation < 2 && (state == 0 || state == 1 || state == 2 || state == 4 || state == 6))
            canvas->write(std::string(out) + "/game-" + (lang == Language::ES ? "es-" : "en-") + std::to_string(width) +
                          "-" + std::to_string(kind) + "-" + std::to_string(state) + ".pbm");
          canvas->target.expectedFullText = nullptr;
        }
      }
    }
  }
}
}  // namespace
static void resultAndSnapshotScreens(const char* out) {
  ArtFixture graphics("resources/apps/pet/graphics.bin"), habitat("resources/apps/pet/habitat.bin"),
      breathing("resources/apps/pet/breathing.bin");
  for (auto language : {Language::ES, Language::EN}) {
    I18N.setLanguage(language);
    for (unsigned orientation = 0; orientation < 4; ++orientation)
      for (unsigned species = 0; species < 3; ++species)
        for (unsigned age = 1; age <= 4; ++age)
          for (auto kind : {petui::NoticeKind::Information, petui::NoticeKind::Cleaned, petui::NoticeKind::Cured})
            for (bool artwork : {false, true}) {
              auto c = std::make_unique<Canvas>(orientation % 2 ? 800 : 480, orientation % 2 ? 480 : 800);
              c->device.orientation = fui::Orientation(orientation);
              c->target.view = "illustrated-result";
              pet::Profile p;
              p.created = true;
              p.species = pet::Species(species);
              p.readingSeconds = pet::stageStart(pet::Stage(age));
              std::strcpy(p.name, "MMMMMMMMMMMM");
              p.hygiene = kind == petui::NoticeKind::Cured ? 60 : 100;
              petui::Model m;
              m.profile = &p;
              m.view = pet::View::Notice;
              m.noticeKind = kind;
              m.notice = kind == petui::NoticeKind::Cured     ? tr(STR_PET_CURED)
                         : kind == petui::NoticeKind::Cleaned ? tr(STR_PET_CLEANED)
                                                              : tr(STR_PET_SAVE_FAILED);
              c->target.expectedFullText = m.notice;
              if (artwork) {
                m.art = {&graphics, ArtFixture::draw};
                m.habitat = {&habitat, ArtFixture::draw};
                m.sleepArt = {&breathing, ArtFixture::draw};
              }
              petui::Work w;
              c->paint(m, w);
              c->checkHits();
              CHECK(c->target.expectedFullTextSeen);
              unsigned back = 0;
              for (size_t i = 0; i < c->interactions.count(); ++i) {
                const auto& hit = c->interactions.data()[i];
                if (hit.action == petui::Back) {
                  ++back;
                  CHECK(hit.rect.width >= 44 && hit.rect.height >= 44);
                }
              }
              CHECK(back == 1);
              if (language == Language::ES && orientation < 2 && species == 0 && age == 4 && artwork)
                c->write(std::string(out) + "/result-" + std::to_string(c->width) + "-" +
                         std::to_string(unsigned(kind)) + ".pbm");
            }
  }
  I18N.setLanguage(Language::ES);
  auto c = std::make_unique<Canvas>(480, 800);
  pet::Profile p, previous;
  p.created = true;
  p.readingSeconds = 72000;
  p.memories = 4;
  for (unsigned slot = 0; slot < 4; ++slot) {
    p.equipped[slot] = uint8_t(slot * 3);
    pet::own(p, slot * 3);
    pet::own(p, slot * 3 + 1);
  }
  previous = p;
  previous.memories = 0;
  pet::captureMoments(p, previous);
  for (unsigned slot = 0; slot < 4; ++slot) p.equipped[slot] = uint8_t(slot * 3 + 1);
  struct Trace {
    ArtFixture* pack;
    std::vector<unsigned> ids;
  } trace{&habitat, {}};
  trace.ids.reserve(32);
  petui::Model m;
  m.profile = &p;
  m.view = pet::View::Memory;
  m.memory = 3;
  m.art = {&graphics, ArtFixture::draw};
  m.sleepArt = {&breathing, ArtFixture::draw};
  m.habitat = {&trace, [](void* context, fui::DrawTarget& target, fui::Rect area, uint32_t id, bool inverse) {
                 auto& t = *static_cast<Trace*>(context);
                 t.ids.push_back(id);
                 return ArtFixture::draw(t.pack, target, area, id, inverse);
               }};
  petui::Work w;
  c->paint(m, w);
  c->checkHits();
  for (unsigned slot = 0; slot < 4; ++slot) {
    CHECK(std::find(trace.ids.begin(), trace.ids.end(), 2 + slot * 6) != trace.ids.end());
    CHECK(std::find(trace.ids.begin(), trace.ids.end(), 4 + slot * 6) == trace.ids.end());
  }
  m.sleepArt = {};
  for (unsigned species = 0; species < 3; ++species)
    for (unsigned age = 0; age < 5; ++age)
      for (const unsigned pose : {0u, 4u, 5u}) {
        p.species = pet::Species(species);
        p.moments[3] = (p.moments[3] & 0x00ffffffu) | (age << 24) | (pose << 27);
        trace.ids.clear();
        c->paint(m, w);
        c->checkHits();
        const unsigned scene = age ? 3 + species * 12 + (age - 1) * 3 + (pose == 4 ? 2 : pose == 5 ? 1 : 0) : species;
        CHECK(trace.ids.size() >= 12);
        CHECK(trace.ids[trace.ids.size() - 2] == 482 + scene * 2);
        CHECK(trace.ids.back() == 483 + scene * 2);
      }
  for (unsigned orientation = 0; orientation < 4; ++orientation) {
    auto head = std::make_unique<Canvas>(orientation % 2 ? 800 : 480, orientation % 2 ? 480 : 800);
    head->device.orientation = fui::Orientation(orientation);
    m.view = pet::View::Dashboard;
    head->paint(m, w);
    head->checkHits();
    fui::Rect info, settings;
    for (size_t i = 0; i < head->interactions.count(); ++i) {
      const auto& hit = head->interactions.data()[i];
      if (hit.action == petui::OpenSettings) settings = hit.rect;
      if (hit.action == petui::OpenReadingInfo && hit.rect.y < 90) info = hit.rect;
    }
    CHECK(info.width >= 44 && info.height >= 44 && info.right() <= settings.x);
  }
}

static unsigned darkPixels(const Canvas& c, fui::Rect r) {
  unsigned count = 0;
  for (int y = r.y; y < r.bottom(); ++y)
    for (int x = r.x; x < r.right(); ++x)
      count += !(c.pixels[size_t(y) * ((c.width + 7) / 8) + x / 8] & (0x80 >> (x % 8)));
  return count;
}
static void progressPixels() {
  auto c = std::make_unique<Canvas>(800, 480);
  pet::Profile p;
  p.created = true;
  petui::Model m;
  m.profile = &p;
  petui::Work w;
  const auto box = fui::makeRect(12, 20, 450, 54);
  unsigned previous = 0;
  for (unsigned percent : {0u, 50u, 99u}) {
    std::fill(c->pixels.begin(), c->pixels.end(), 255);
    p.readingSeconds = 1200 + 6000 * percent / 100;
    petui::widget::growth(c->screen, m, w, box);
    const unsigned pixels = darkPixels(*c, fui::makeRect(box.x + 9, box.bottom() - 10, box.width - 18, 9));
    if (percent) CHECK(pixels > previous);
    previous = pixels;
  }
  pet::own(p, 0);
  const auto project = fui::makeRect(12, 20, 225, 132);
  for (unsigned percent : {0u, 50u, 100u}) {
    std::fill(c->pixels.begin(), c->pixels.end(), 255);
    p.readingSeconds = 1200 + 300 * percent / 100;
    petui::widget::objective(c->screen, m, w, project);
    const unsigned pixels = darkPixels(*c, fui::makeRect(project.x + 9, project.bottom() - 11, project.width - 18, 9));
    if (percent) CHECK(pixels > previous);
    previous = pixels;
  }
  const auto game = fui::makeRect(8, 20, 764, 430);
  for (unsigned percent : {0u, 50u, 100u}) {
    std::fill(c->pixels.begin(), c->pixels.end(), 255);
    p.joy = p.energy = uint8_t(percent);
    petui::games(c->screen, m, w, game);
    const unsigned pixels = darkPixels(*c, fui::makeRect(game.x, game.y + 33, game.width, 10));
    if (percent) CHECK(pixels > previous);
    previous = pixels;
  }
}
static void polishScreens(const char* out) {
  progressPixels();
  for (auto language : {Language::ES, Language::EN}) {
    I18N.setLanguage(language);
    for (unsigned orientation = 0; orientation < 4; ++orientation) {
      const int width = orientation % 2 ? 800 : 480, height = orientation % 2 ? 480 : 800;
      for (unsigned state = 0; state < 3; ++state) {
        pet::Profile p;
        p.created = true;
        p.readingSeconds = 1200;
        for (unsigned i = 0; i < pet::OBJECT_COUNT; ++i)
          if (state == 2 || (state == 1 && i % 3 == 0)) pet::own(p, i);
        for (unsigned slot = 0; slot < pet::SLOT_COUNT; ++slot)
          for (auto filter : {petui::CatalogFilter::Owned, petui::CatalogFilter::Unowned}) {
            const auto total = petui::catalogCount(p, slot, filter);
            unsigned previous = 0;
            for (unsigned at = 0; at < total; ++at) {
              const auto id = petui::catalogItem(p, slot, filter, at);
              CHECK(id < pet::OBJECT_COUNT && pet::objectSlot(id) == slot && petui::catalogMatches(p, id, filter));
              if (at) CHECK(id > previous);
              previous = id;
            }
            CHECK(petui::catalogItem(p, slot, filter, total) == pet::OBJECT_COUNT);
            auto c = std::make_unique<Canvas>(width, height);
            c->device.orientation = fui::Orientation(orientation);
            c->target.view = "polish-catalog";
            petui::Model m;
            m.profile = &p;
            m.view = pet::View::Projects;
            m.catalogFilter = filter;
            m.projectSlot = int(slot);
            m.page = 99;
            petui::Work w;
            c->paint(m, w);
            c->checkHits();
            const unsigned count = width == 480 ? 6 : 4;
            CHECK(w.pages == std::max(1, int((total + count - 1) / count)));
            unsigned cards = 0, filters = 0;
            for (size_t i = 0; i < c->interactions.count(); ++i) {
              const auto& hit = c->interactions.data()[i];
              if (hit.action == petui::OpenProjectPreview) {
                ++cards;
                CHECK(hit.value >= 0 && petui::catalogMatches(p, unsigned(hit.value), filter));
              }
              if (hit.action == petui::SelectCatalogFilter) {
                ++filters;
                CHECK(hit.rect.width >= 44 && hit.rect.height >= 44);
              }
            }
            CHECK(filters == 2);
            CHECK(cards == (total ? total - (unsigned(w.pages) - 1) * count : 0));
            m.writable = false;
            c->paint(m, w);
            for (size_t i = 0; i < c->interactions.count(); ++i)
              CHECK(c->interactions.data()[i].action != petui::SelectProject);
          }
      }
      pet::Profile p;
      p.created = true;
      p.readingSeconds = 1200;
      {
        auto c = std::make_unique<Canvas>(width, height);
        c->device.orientation = fui::Orientation(orientation);
        petui::Model m;
        m.profile = &p;
        m.view = pet::View::Album;
        p.readingSeconds = 0;
        petui::Work w;
        c->paint(m, w);
        unsigned gallery = 0, directMemory = 0;
        for (size_t i = 0; i < c->interactions.count(); ++i) {
          const auto& hit = c->interactions.data()[i];
          gallery += hit.action == petui::OpenMemories;
          directMemory += hit.action == petui::OpenMemory;
        }
        CHECK(gallery == 1 && directMemory == 0);
        p.readingSeconds = 1200;
      }
      for (unsigned id = 0; id < petui::MEMORY_COUNT; ++id) {
        if (petui::memoryOwned(p, id)) continue;
        auto c = std::make_unique<Canvas>(width, height);
        c->device.orientation = fui::Orientation(orientation);
        c->target.view = "locked-memory-requirement";
        petui::Model m;
        m.profile = &p;
        m.view = pet::View::Memory;
        m.memory = int(id);
        petui::Work w, hint;
        char copy[160];
        std::snprintf(copy, sizeof(copy), "%s", petui::albumwidget::memoryRequirement(p, id, hint));
        c->target.expectedFullText = copy;
        c->paint(m, w);
        c->checkHits();
        CHECK(c->target.expectedFullTextSeen);
      }
      {
        auto c = std::make_unique<Canvas>(width, height);
        c->device.orientation = fui::Orientation(orientation);
        for (unsigned item = 0; item < pet::OBJECT_COUNT; ++item) pet::own(p, item);
        petui::Model m;
        m.profile = &p;
        m.view = pet::View::Memory;
        m.memory = 1;
        petui::Work w;
        c->target.expectedFullText = tr(STR_PET_MEMORY_HOME_COMPLETE);
        c->paint(m, w);
        c->checkHits();
        CHECK(c->target.expectedFullTextSeen);
        p.owned = 0;
        std::memset(p.ownedExtra, 0, sizeof(p.ownedExtra));
      }
      for (unsigned age = 1; age <= 4; ++age) {
        p.readingSeconds = pet::stageStart(pet::Stage(age)) + 59;
        auto c = std::make_unique<Canvas>(width, height);
        c->device.orientation = fui::Orientation(orientation);
        c->target.view = "growth-progress";
        petui::Model m;
        m.profile = &p;
        m.view = pet::View::Dashboard;
        m.today = 20000;
        m.todaySeconds = 900;
        char copy[128], caps[128];
        if (age == 4)
          std::snprintf(copy, sizeof(copy), "%s", tr(STR_PET_GROWTH_ADULT));
        else
          std::snprintf(
              copy, sizeof(copy), tr(STR_PET_GROWTH_LEFT),
              static_cast<unsigned long long>((pet::nextStageSeconds(pet::Stage(age)) - p.readingSeconds + 59) / 60));
        pixelui::copyCaps(caps, sizeof(caps), copy);
        c->target.expectedFullText = caps;
        petui::Work w;
        c->paint(m, w);
        c->checkHits();
        CHECK(!c->target.expectedFullTextSeen);
      }
      for (unsigned need = 0; need < 7; ++need) {
        p = {};
        p.created = true;
        p.readingSeconds = 1200;
        p.earnedFood = 3;
        p.joy = 80;
        p.energy = 80;
        p.saciety = 70;
        if (need == 0) p.illness = pet::Illness::Cold;
        if (need == 1) p.resting = true;
        if (need == 2) p.saciety = 10;
        if (need == 3) p.energy = 10;
        if (need == 4) p.hygiene = 10;
        if (need == 5) p.joy = 20;
        auto c = std::make_unique<Canvas>(width, height);
        c->device.orientation = fui::Orientation(orientation);
        c->target.view = "care-priority";
        petui::Model m;
        m.profile = &p;
        m.view = pet::View::Care;
        m.today = 20000;
        c->target.expectedFullText = p.resting ? tr(STR_PET_REST_TIME_UNKNOWN) : petui::careAdvice(p);
        petui::Work w;
        c->paint(m, w);
        c->checkHits();
        CHECK(c->target.expectedFullTextSeen);
        unsigned feeding = 0;
        for (size_t i = 0; i < c->interactions.count(); ++i) feeding += c->interactions.data()[i].action == petui::Feed;
        CHECK(feeding == 1);
        if (need == 0) CHECK(petui::advisedCare(p, m.today) == petui::OpenHealth);
        if (need == 2) CHECK(petui::advisedCare(p, m.today) == petui::Feed);
        if (need == 3) CHECK(petui::advisedCare(p, m.today) == petui::Rest);
        if (language == Language::ES && orientation < 2 && need == 0)
          c->write(std::string(out) + "/polish-care-" + std::to_string(width) + ".pbm");
      }
    }
  }
}

void communicationScreens() {
  pet::Profile instant;
  instant.created = true;
  instant.readingSeconds = 7200;
  pet::own(instant, 5);
  instant.project = 5;
  CHECK(petui::catalogCount(instant, 0, petui::CatalogFilter::Unowned) == 60);
  CHECK(pet::chooseProject(instant, 0));
  CHECK(pet::owns(instant, 0) && instant.equipped[0] == 0);
  CHECK(!petui::catalogMatches(instant, 0, petui::CatalogFilter::Unowned));
  CHECK(petui::catalogCount(instant, 0, petui::CatalogFilter::Unowned) == 59);
  CHECK(petui::catalogItem(instant, 0, petui::CatalogFilter::Unowned, 0) == 1);
  instant.readingSeconds = 10000000;
  for (unsigned item = 1; item < pet::OBJECT_COUNT; ++item)
    if (item != 1) pet::own(instant, item);
  CHECK(petui::catalogCount(instant, 0, petui::CatalogFilter::Unowned) == 1);
  CHECK(pet::chooseProject(instant, 1));
  CHECK(petui::catalogCount(instant, 0, petui::CatalogFilter::Unowned) == 0);
  for (unsigned slot = 0; slot < pet::SLOT_COUNT; ++slot) {
    pet::Profile catalog;
    for (unsigned at = 0; at < pet::OBJECTS_PER_SLOT; at += 2) pet::own(catalog, pet::objectAt(slot, at));
    const auto original = catalog;
    for (auto filter : {petui::CatalogFilter::Owned, petui::CatalogFilter::Unowned}) {
      const auto count = petui::catalogCount(catalog, slot, filter);
      CHECK(count == 30);
      for (unsigned at = 0; at < count; ++at) {
        const auto item = petui::catalogItem(catalog, slot, filter, at);
        CHECK(petui::catalogOrdinal(catalog, slot, filter, item) == at);
        CHECK(petui::catalogAdjacent(catalog, slot, filter, item, -1) ==
              (at ? petui::catalogItem(catalog, slot, filter, at - 1) : pet::OBJECT_COUNT));
        CHECK(petui::catalogAdjacent(catalog, slot, filter, item, 1) ==
              petui::catalogItem(catalog, slot, filter, at + 1));
        CHECK(petui::catalogAdjacent(catalog, slot, filter, item, 0) == pet::OBJECT_COUNT);
      }
      CHECK(petui::catalogAdjacent(catalog, 4, filter, 0, 1) == pet::OBJECT_COUNT);
      CHECK(petui::catalogAdjacent(catalog, slot, filter, pet::OBJECT_COUNT, 1) == pet::OBJECT_COUNT);
    }
    CHECK(!std::memcmp(&original, &catalog, sizeof(catalog)));
  }
  pet::Profile gains;
  gains.earnedFood = 3;
  gains.noticedFood = 2;
  CHECK(petui::readingGains(gains, false) == 1);
  CHECK(petui::readingGains(gains, true) == 0);
  gains.earnedMedicine = 1;
  CHECK(petui::readingGains(gains, true) == 1);
  gains.noticedFood = gains.earnedFood;
  gains.noticedMedicine = gains.earnedMedicine;
  CHECK(petui::readingGains(gains, false) == 0);
  CHECK(petui::readingGains(gains, true) == 0);
  for (auto language : {Language::ES, Language::EN}) {
    I18N.setLanguage(language);
    for (unsigned orientation = 0; orientation < 4; ++orientation) {
      auto c = std::make_unique<Canvas>(orientation % 2 ? 800 : 480, orientation % 2 ? 480 : 800);
      c->device.orientation = fui::Orientation(orientation);
      c->target.view = "communication";
      pet::Profile p;
      p.created = true;
      p.readingSeconds = 7200;
      p.earnedFood = 24;
      p.earnedMedicine = 8;
      p.noticedSeconds = 6000;
      petui::Model m;
      m.profile = &p;
      petui::Work w;
      auto hits = [&](fui::ActionId action) {
        unsigned count = 0;
        for (size_t i = 0; i < c->interactions.count(); ++i) count += c->interactions.data()[i].action == action;
        return count;
      };
      m.view = pet::View::Projects;
      m.catalogFilter = petui::CatalogFilter::Unowned;
      m.page = 9;
      auto copy = instant;
      m.profile = &copy;
      c->paint(m, w);
      c->checkHits();
      CHECK(w.pages == 1 && hits(petui::OpenProjectPreview) == 0);
      m.profile = &p;
      for (unsigned slot = 0; slot < pet::SLOT_COUNT; ++slot)
        for (unsigned state = 0; state < 4; ++state) {
          const unsigned id = pet::objectAt(slot, 40);
          p.ownedExtra[(id - 16) / 8] = state ? uint8_t(1u << ((id - 16) % 8)) : 0;
          p.equipped[slot] = state == 2 ? uint8_t(id) : uint8_t(pet::OBJECT_COUNT);
          m.view = pet::View::ProjectPreview;
          m.previewItem = uint8_t(id);
          m.catalogFilter = state ? petui::CatalogFilter::Owned : petui::CatalogFilter::Unowned;
          m.writable = state != 3;
          const auto before = p;
          c->target.expectedFullText = tr(STR_PET_PREVIEW_NO_CHANGE);
          c->target.expectedFullTextSeen = false;
          c->paint(m, w);
          c->checkHits();
          CHECK(c->target.expectedFullTextSeen == (state < 2));
          c->target.expectedFullText = nullptr;
          CHECK(!std::memcmp(&before, &p, sizeof(p)));
          CHECK(w.previewProfile.equipped[slot] == id);
          CHECK(hits(petui::Back) == 1);
          CHECK(hits(petui::SelectProject) == unsigned(state < 2));
        }
      m.writable = true;
      m.view = pet::View::Care;
      m.tab = pet::Tab::Care;
      char expected[120];
      for (unsigned sat : {55u, 90u, 99u, 100u}) {
        p.saciety = uint8_t(sat);
        std::snprintf(expected, sizeof(expected),
                      orientation % 2 ? tr(STR_PET_FEED_CHANGE_SHORT) : tr(STR_PET_FEED_CHANGE), sat,
                      std::min(100u, sat + 25));
        c->target.expectedFullText = expected;
        c->target.expectedFullTextSeen = false;
        c->paint(m, w);
        c->checkHits();
        CHECK(c->target.expectedFullTextSeen == (sat < 100));
        if (sat == 100) {
          CHECK(hits(petui::Feed) == 0);
          continue;
        }
        m.feedResult = {uint8_t(sat), uint8_t(std::min(100u, sat + 25)), true};
        m.view = pet::View::Notice;
        m.noticeKind = petui::NoticeKind::Fed;
        m.notice = tr(STR_PET_FED_MESSAGE);
        std::snprintf(expected, sizeof(expected), tr(STR_PET_FEED_CHANGE), sat, std::min(100u, sat + 25));
        char caps[120];
        pixelui::copyCaps(caps, sizeof(caps), expected);
        c->target.expectedFullText = caps;
        c->target.expectedFullTextSeen = false;
        c->paint(m, w);
        CHECK(c->target.expectedFullTextSeen);
        CHECK(hits(petui::Back) == 1);
        CHECK(hits(petui::Feed) == 0);
        c->target.expectedFullText = nullptr;
        m.feedResult = {};
        m.view = pet::View::Care;
        m.noticeKind = petui::NoticeKind::Information;
        m.notice = nullptr;
      }
      c->target.expectedFullText = nullptr;
      p.saciety = 100;
      p.spentFood = uint32_t(pet::food(p));
      c->target.expectedFullText = tr(STR_PET_ALREADY_FULL);
      c->target.expectedFullTextSeen = false;
      c->paint(m, w);
      CHECK(c->target.expectedFullTextSeen);
      CHECK(hits(petui::Feed) == 0);
      p.resting = true;
      p.energy = 67;
      std::snprintf(expected, sizeof(expected), "%u / 100", unsigned(p.energy));
      c->target.expectedFullText = expected;
      c->target.expectedFullTextSeen = false;
      c->paint(m, w);
      c->checkHits();
      CHECK(c->target.expectedFullTextSeen);
      p.resting = false;
      c->target.expectedFullText = tr(STR_PET_CARE_REST_EFFECT);
      c->target.expectedFullTextSeen = false;
      c->paint(m, w);
      CHECK(c->target.expectedFullTextSeen);
      for (unsigned joy : {50u, 98u, 100u}) {
        p.hygiene = 20;
        p.joy = uint8_t(joy);
        const auto before = p;
        CHECK(pet::clean(p));
        m.careResult = {before.hygiene, p.hygiene, before.joy, p.joy, 0, true};
        m.view = pet::View::Notice;
        m.noticeKind = petui::NoticeKind::Cleaned;
        m.notice = tr(STR_PET_CLEANED);
        std::snprintf(expected, sizeof(expected), tr(STR_PET_CLEAN_RESULT), joy, std::min(100u, joy + 5));
        char caps[120];
        pixelui::copyCaps(caps, sizeof(caps), expected);
        c->target.expectedFullText = caps;
        c->target.expectedFullTextSeen = false;
        c->paint(m, w);
        c->checkHits();
        CHECK(c->target.expectedFullTextSeen);
        CHECK(p.spentMedicine == before.spentMedicine);
      }
      p.hygiene = 20;
      p.illness = pet::Illness::Cold;
      auto beforeCure = p;
      CHECK(pet::cure(p));
      m.noticeKind = petui::NoticeKind::Cured;
      m.notice = tr(STR_PET_CURED);
      m.careResult = {beforeCure.hygiene,
                      p.hygiene,
                      beforeCure.joy,
                      p.joy,
                      uint8_t(p.spentMedicine - beforeCure.spentMedicine),
                      true};
      char caps[120];
      std::snprintf(expected, sizeof(expected), tr(STR_PET_CURE_COST), 1u);
      pixelui::copyCaps(caps, sizeof(caps), expected);
      c->target.expectedFullText = caps;
      c->target.expectedFullTextSeen = false;
      c->paint(m, w);
      c->checkHits();
      CHECK(c->target.expectedFullTextSeen);
      CHECK(p.illness == pet::Illness::Healthy && p.hygiene == 60);
      CHECK(!pet::cure(p));
      m.careResult = {};
      m.noticeKind = petui::NoticeKind::Information;
      c->target.expectedFullText = nullptr;
      m.view = pet::View::Dashboard;
      m.tab = pet::Tab::Pet;
      p.illness = pet::Illness::Cold;
      c->paint(m, w);
      c->checkHits();
      CHECK(hits(petui::OpenHealth) == 1);
      p.illness = pet::Illness::Healthy;
      p.saciety = 10;
      c->paint(m, w);
      CHECK(petui::statusAction(p) == petui::ChangeTab);
      CHECK(hits(petui::ChangeTab) >= 5);
      p.readingSeconds = 0;
      CHECK(petui::statusAction(p) == fui::NO_ACTION);
      p.readingSeconds = pet::stageStart(pet::Stage::Adult);
      m.view = pet::View::Reward;
      pixelui::copyCaps(expected, sizeof(expected), tr(STR_PET_GROWTH_ADULT));
      c->target.expectedFullText = expected;
      c->target.expectedFullTextSeen = false;
      c->paint(m, w);
      c->checkHits();
      CHECK(c->target.expectedFullTextSeen);
    }
  }
}
void guidanceScreens() {
  pet::Profile p;
  p.created = true;
  p.readingSeconds = 7200;
  petui::Model m;
  m.profile = &p;
  m.now = 1700007200;
  m.today = 20000;
  p.resting = true;
  p.restAt = m.now - 1800;
  CHECK(petui::restRemaining(m) == 5400);
  p.restAt = m.now - 7200;
  CHECK(petui::restRemaining(m) == 0);
  p.restAt = m.now + 1;
  CHECK(petui::restRemaining(m) == -1);
  p.restAt = 0;
  CHECK(petui::restRemaining(m) == -1);
  p.restAt = m.now - 1800;
  m.clockKnown = false;
  CHECK(petui::restRemaining(m) == -1);
  m.clockKnown = true;
  for (auto language : {Language::ES, Language::EN}) {
    I18N.setLanguage(language);
    char value[96], expected[96];
    for (uint64_t seconds : {1ull, 59ull, 60ull, 3599ull, 3600ull, 5401ull, UINT64_MAX}) {
      petui::widget::remainingDuration(value, sizeof(value), seconds);
      const auto minutes = seconds / 60 + unsigned(seconds % 60 != 0);
      if (minutes >= 60)
        std::snprintf(expected, sizeof(expected), tr(STR_PET_HOURS_MINUTES),
                      static_cast<unsigned long long>(minutes / 60), unsigned(minutes % 60));
      else
        std::snprintf(expected, sizeof(expected), tr(STR_PET_MINUTES), static_cast<unsigned long long>(minutes));
      CHECK(!std::strcmp(value, expected));
    }
    for (unsigned orientation = 0; orientation < 4; ++orientation) {
      auto c = std::make_unique<Canvas>(orientation % 2 ? 800 : 480, orientation % 2 ? 480 : 800);
      c->device.orientation = fui::Orientation(orientation);
      c->target.view = "guidance";
      petui::Work w;
      auto expect = [&](const char* copy, bool caps = false) {
        char capital[160];
        pixelui::copyCaps(capital, sizeof(capital), copy);
        c->target.expectedFullText = caps ? capital : copy;
        c->target.expectedFullTextSeen = false;
        c->paint(m, w);
        c->checkHits();
        CHECK(c->target.expectedFullTextSeen);
        c->target.expectedFullText = nullptr;
      };
      p = {};
      p.created = true;
      p.readingSeconds = 7200;
      m.view = pet::View::Health;
      m.writable = true;
      expect(tr(STR_PET_ALREADY_CLEAN));
      expect(tr(STR_PET_NO_TREATMENT));
      p.hygiene = 20;
      p.illness = pet::Illness::Cold;
      p.spentMedicine = uint32_t(pet::medicine(p));
      expect(tr(STR_PET_NO_MEDICINE));
      m.writable = false;
      expect(tr(STR_PET_CARE_NO_SAVE));
      m.writable = true;
      p.spentMedicine = 0;
      expect(tr(STR_PET_CURE_ONE));
      for (auto life : {pet::LifeStatus::Retired, pet::LifeStatus::Lost})
        for (auto age : {pet::Stage::Egg, pet::Stage::Adult}) {
          p.lifeStatus = life;
          p.readingSeconds = pet::stageStart(age);
          CHECK(!std::strcmp(petui::gameCareBlock(p), tr(STR_PET_LIFE_ENDED)));
          CHECK(!std::strcmp(petui::healthActionHint(m, false), tr(STR_PET_LIFE_ENDED)));
          CHECK(!std::strcmp(petui::healthActionHint(m, true), tr(STR_PET_LIFE_ENDED)));
        }
      p.lifeStatus = pet::LifeStatus::Active;
      p.readingSeconds = 7200;
      m.view = pet::View::Care;
      p.illness = pet::Illness::Healthy;
      p.saciety = 70;
      p.energy = 100;
      p.resting = true;
      p.restAt = m.now - 1800;
      expect("100 / 100", true);
      petui::widget::remainingDuration(value, sizeof(value), 5400);
      std::snprintf(expected, sizeof(expected), tr(STR_PET_REST_ENDS), value);
      expect(expected);
      expect(tr(STR_PET_GAME_WAKE_SHORT));
      p.resting = false;
      p.gameDay = m.today;
      p.gamesToday = 0;
      p.joy = 99;
      CHECK(petui::gameOffer(m) == petui::GameOffer::Reward);
      std::snprintf(expected, sizeof(expected), tr(STR_PET_GAME_GAIN_VALUE), 1u);
      expect(expected);
      p.gamesToday = 3;
      CHECK(petui::gameOffer(m) == petui::GameOffer::Practice);
      expect(tr(STR_PET_CARE_GAME_PRACTICE));
      p.saciety = 10;
      CHECK(petui::gameOffer(m) == petui::GameOffer::Blocked);
      expect(tr(STR_PET_GAME_FOOD_SHORT));
      p.saciety = 70;
      m.view = pet::View::Dashboard;
      m.todaySeconds = 1500;
      p.goalIndex = 1;
      std::snprintf(expected, sizeof(expected), tr(STR_PET_DAILY_FRACTION), 10u, 10u);
      expect(expected, true);
      petui::widget::duration(value, sizeof(value), 1500);
      std::snprintf(expected, sizeof(expected), tr(STR_PET_DAILY_TOTAL), value);
      expect(expected);
      m.view = pet::View::ProjectPreview;
      m.catalogFilter = petui::CatalogFilter::Unowned;
      m.previewItem = 0;
      p.owned = uint16_t(1u << 5);
      p.project = 5;
      expect(tr(STR_PET_PROJECT_CAN_GET));
    }
  }
}
void clarityScreens() {
  pet::Profile p;
  petui::Model m;
  m.profile = &p;
  m.today = 20000;
  for (auto language : {Language::ES, Language::EN}) {
    I18N.setLanguage(language);
    for (unsigned orientation = 0; orientation < 4; ++orientation) {
      auto c = std::make_unique<Canvas>(orientation % 2 ? 800 : 480, orientation % 2 ? 480 : 800);
      c->device.orientation = fui::Orientation(orientation);
      c->target.view = "clarity";
      petui::Work w;
      auto expect = [&](const char* copy, bool caps = false, bool visible = true) {
        char capital[160];
        pixelui::copyCaps(capital, sizeof(capital), copy);
        c->target.expectedFullText = caps ? capital : copy;
        c->target.expectedFullTextSeen = false;
        c->paint(m, w);
        c->checkHits();
        if (c->target.expectedFullTextSeen != visible)
          std::printf("MISSING clarity %dx%d: %s (expected visible: %u)\n", c->width, c->height, copy,
                      unsigned(visible));
        CHECK(c->target.expectedFullTextSeen == visible);
        c->target.expectedFullText = nullptr;
      };
      p = {};
      p.created = true;
      p.readingSeconds = 7200;
      p.goalIndex = 1;
      m.clockKnown = true;
      m.historyChanged = false;
      m.writable = true;
      m.view = pet::View::Reward;
      char expected[128], time[64];
      for (int seconds : {-60, 0, 300, 599, 600, 1500, 59040}) {
        m.todaySeconds = seconds;
        if (seconds >= 600)
          expect(tr(STR_PET_GOAL_REACHED), true);
        else {
          std::snprintf(expected, sizeof(expected), tr(STR_PET_DAILY_PROGRESS), std::max(0, seconds) / 60, 10u);
          expect(expected, true);
        }
        petui::widget::duration(time, sizeof(time), uint64_t(std::max(0, seconds)));
        std::snprintf(expected, sizeof(expected), tr(STR_PET_READ_TODAY), time);
        expect(expected);
      }
      for (unsigned unavailable = 0; unavailable < 4; ++unavailable) {
        m.clockKnown = unavailable != 0;
        m.historyChanged = unavailable == 1;
        m.writable = unavailable != 2;
        m.today = unavailable == 3 ? 0 : 20000;
        expect(tr(STR_PET_DAILY_WAIT), true);
        expect(expected, false, false);
      }
      m.today = 20000;
      m.clockKnown = m.writable = true;
      m.historyChanged = false;
      m.todaySeconds = 1500;
      m.view = pet::View::Album;
      std::snprintf(time, sizeof(time), tr(STR_PET_ALBUM_HOURS), 2ull);
      std::snprintf(expected, sizeof(expected), tr(STR_PET_ALBUM_READING), time);
      expect(expected, true);
      std::snprintf(expected, sizeof(expected), tr(STR_PET_ALBUM_DAYS), 0u, 0u);
      expect(expected);
      m.view = pet::View::Health;
      expect(tr(STR_PET_HEALTH_ADVICE_OK));
      p.hygiene = 60;
      expect(tr(STR_PET_HEALTH_ADVICE_CLEAN));
      p.hygiene = 20;
      expect(tr(STR_PET_HEALTH_ADVICE_DIRTY));
      p.illness = pet::Illness::Cold;
      expect(tr(STR_PET_HEALTH_ADVICE_CURE));
      p.spentMedicine = uint32_t(pet::medicine(p));
      expect(tr(STR_PET_HEALTH_ADVICE_READ));
      m.writable = false;
      expect(tr(STR_PET_CARE_NO_SAVE));
      m.writable = true;
      m.view = pet::View::HealthHelp;
      for (const auto key : {StrId::STR_PET_HELP_CLEAN, StrId::STR_PET_HELP_CURE, StrId::STR_PET_HELP_REST})
        expect(I18N.get(key));
      p.illness = pet::Illness::Healthy;
      p.energy = 60;
      m.view = pet::View::Care;
      expect(tr(STR_PET_CARE_REST_EFFECT));
      p.energy = 100;
      expect(tr(STR_PET_CARE_REST_EFFECT));
      expect(c->width == 800 ? tr(STR_PET_REST_BENEFIT_SHORT) : tr(STR_PET_REST_BENEFIT));
      for (unsigned energy : {0u, 60u, 80u, 99u}) {
        p.energy = uint8_t(energy);
        expect(c->width == 800 ? tr(STR_PET_REST_BENEFIT_SHORT) : tr(STR_PET_REST_BENEFIT));
      }
      p.resting = true;
      expect(tr(STR_PET_CARE_WAKE_EFFECT));
      p.resting = false;
      p.energy = 100;
      p.hygiene = 100;
      p.saciety = 70;
      p.earnedFood = 3;
      c->paint(m, w);
      c->checkHits();
      fui::Rect healthRow{}, feedRow{};
      for (size_t i = 0; i < c->interactions.count(); ++i) {
        const auto& hit = c->interactions.data()[i];
        if (hit.action == petui::OpenHealth) healthRow = hit.rect;
        if (hit.action == petui::Feed) feedRow = hit.rect;
      }
      CHECK(!healthRow.empty() && !feedRow.empty());
      CHECK(healthRow.bottom() + petui::widget::GAP == feedRow.y);
      std::snprintf(expected, sizeof(expected), tr(STR_PET_FOOD_STOCK), static_cast<unsigned long long>(pet::food(p)));
      expect(expected, true);
      std::snprintf(expected, sizeof(expected), tr(STR_PET_MEDICINE_STOCK),
                    static_cast<unsigned long long>(pet::medicine(p)));
      expect(expected, true);
    }
  }
}
static void consistencyScreens() {
  for (unsigned owned = 0; owned < pet::OBJECT_COUNT; ++owned) {
    pet::Profile project;
    project.project = uint8_t(owned);
    for (unsigned id = 0; id < owned; ++id) pet::own(project, id);
    for (auto seconds : {uint64_t(0), uint64_t(72000), pet::projectThreshold(owned) - 1, pet::projectThreshold(owned),
                         pet::projectThreshold(owned) + 60}) {
      project.readingSeconds = seconds;
      const auto progress = petui::widget::projectProgress(project);
      CHECK(progress.target > 0);
      CHECK(progress.target - std::min(progress.value, progress.target) == pet::projectRemaining(project));
    }
  }
  for (const auto area :
       {fui::makeRect(50, 200, 372, 400), fui::makeRect(260, 220, 200, 180), fui::makeRect(20, 80, 280, 140)}) {
    const auto badge = petmood::portraitBounds(area);
    CHECK(badge.width >= 36 && badge.width <= 64 && badge.width == badge.height);
    CHECK(badge.x >= area.x && badge.y >= area.y && badge.right() <= area.right() && badge.bottom() <= area.bottom());
    CHECK(badge.right() == area.right());
  }
  const auto room = fui::makeRect(0, 0, 432, 216), subject = fui::makeRect(190, 80, 100, 110);
  const fui::Rect window = fui::makeRect(350, 18, 62, 54);
  const auto badge = petmood::roomBounds(room, subject, &window, 1);
  CHECK(badge.width >= 36 && badge.width <= 56);
  CHECK(!petmood::overlap(badge, subject) && !petmood::overlap(badge, window));

  for (const auto language : {Language::EN, Language::ES}) {
    I18N.setLanguage(language);
    for (bool wide : {false, true}) {
      auto c = std::make_unique<Canvas>(wide ? 800 : 480, wide ? 480 : 800);
      pet::Profile p;
      p.created = true;
      p.readingSeconds = 72000;
      p.mode = pet::Mode::Extreme;
      p.lastReadAt = 1700000000;
      petui::Model m;
      m.profile = &p;
      m.now = p.lastReadAt + 23 * 3600;
      m.today = 20000;
      m.view = pet::View::Dashboard;
      petui::Work w;
      c->target.view = "urgent-dashboard";
      c->paint(m, w);
      c->checkHits();
      unsigned warnings = 0;
      bool buttons = false;
      for (size_t i = 0; i < c->interactions.count(); ++i) {
        const auto& h = c->interactions.data()[i];
        if (h.action == petui::Read && h.rect.height == 54) ++warnings;
        if (h.action == petui::OpenSettings)
          buttons =
              fui::acceptsInput(h.inputMask, fui::InputFocus) && fui::acceptsInput(h.inputMask, fui::InputConfirm);
      }
      CHECK(warnings == 1 && buttons);
      m.view = pet::View::SettingsConfirm;
      m.selectedMode = pet::Mode::Hard;
      char full[160];
      std::snprintf(full, sizeof(full), tr(STR_PET_SETTINGS_EXPIRED_HELP), petui::modeName(m.selectedMode));
      c->target.expectedFullText = full;
      c->target.view = "confirm-expired-settings";
      c->paint(m, w);
      c->checkHits();
      CHECK(c->target.expectedFullTextSeen);
      unsigned apply = 0, cancel = 0;
      for (size_t i = 0; i < c->interactions.count(); ++i) {
        apply += c->interactions.data()[i].action == petui::ConfirmSettings;
        cancel += c->interactions.data()[i].action == petui::Back;
      }
      CHECK(apply == 1 && cancel == 1);
      c->target.expectedFullText = nullptr;
      for (auto outcome :
           {petui::GameOutcome::Rewarded, petui::GameOutcome::Practice, petui::GameOutcome::CareNeeded}) {
        m.view = pet::View::Notice;
        m.noticeKind = petui::NoticeKind::Game;
        m.gameResult = {80, uint8_t(outcome == petui::GameOutcome::Rewarded ? 90 : 80), 80,
                        uint8_t(outcome == petui::GameOutcome::Rewarded ? 60 : 80), outcome};
        m.notice = outcome == petui::GameOutcome::Rewarded   ? tr(STR_PET_GAME_REWARDED)
                   : outcome == petui::GameOutcome::Practice ? tr(STR_PET_GAME_RESULT_PRACTICE)
                                                             : tr(STR_PET_GAME_RESULT_CARE);
        c->target.view = "game-receipt";
        c->target.expectedFullText = m.notice;
        c->target.expectedFullTextSeen = false;
        c->paint(m, w);
        c->checkHits();
        CHECK(c->target.expectedFullTextSeen);
      }
      c->target.expectedFullText = nullptr;
      p.lifeStatus = pet::LifeStatus::Lost;
      m.view = pet::View::Dashboard;
      c->target.view = "ended-dashboard";
      c->target.vitalValueSeen = false;
      c->paint(m, w);
      c->checkHits();
      CHECK(!c->target.vitalValueSeen);
      m.view = pet::View::Care;
      c->target.expectedFullText = tr(STR_PET_CARE_CLOSED_HELP);
      c->target.view = "ended-care";
      c->paint(m, w);
      c->checkHits();
      for (size_t i = 0; i < c->interactions.count(); ++i) {
        const auto action = c->interactions.data()[i].action;
        CHECK(action != petui::Feed && action != petui::Rest && action != petui::Stroke);
      }
      c->target.expectedFullText = nullptr;
      pet::History history;
      history.count = 1;
      history.records[0].readingSeconds = p.readingSeconds;
      history.records[0].reason = pet::LifeStatus::Lost;
      history.records[0].generation = 1;
      history.records[0].endedAt = m.now;
      m.history = &history;
      m.memory = 0;
      m.lifeEndedDay = 20000;
      m.lifeReturn = pet::View::Album;
      m.view = pet::View::LifeDetail;
      c->target.view = "archived-life";
      char reason[96];
      pixelui::copyCaps(reason, sizeof(reason), tr(STR_PET_LIFE_REASON_LOST));
      c->target.expectedFullText = reason;
      c->target.expectedFullTextSeen = false;
      c->paint(m, w);
      c->checkHits();
      CHECK(c->target.expectedFullTextSeen);
    }
  }
}

static void redesignRegressionScreens() {
  for (auto language : {Language::ES, Language::EN}) {
    I18N.setLanguage(language);
    for (unsigned orientation = 0; orientation < 4; ++orientation) {
      auto c = std::make_unique<Canvas>(orientation % 2 ? 800 : 480, orientation % 2 ? 480 : 800);
      c->device.orientation = fui::Orientation(orientation);
      pet::Profile p;
      p.created = true;
      p.readingSeconds = 72000;
      for (unsigned item = 0; item < pet::OBJECT_COUNT; ++item) pet::own(p, item);
      petui::Model m;
      m.profile = &p;
      m.view = pet::View::Home;
      m.tab = pet::Tab::Home;
      petui::Work w;
      c->paint(m, w);
      c->checkHits();
      int mainValue = -1, mainY = -1;
      for (size_t i = 0; i < c->interactions.count(); ++i) {
        const auto& hit = c->interactions.data()[i];
        if (hit.action == petui::OpenProjects && hit.rect.y > mainY) {
          mainY = hit.rect.y;
          mainValue = hit.value;
        }
      }
      CHECK(mainValue >= 0 && mainValue < int(pet::SLOT_COUNT));
      CHECK(petui::catalogCount(p, unsigned(mainValue), petui::CatalogFilter::Owned) == 60);
      const auto id = unsigned(pet::Moment::Reading1h);
      CHECK(petui::memoryUnrecorded(p, id) && !petui::memoryOwned(p, id));
      CHECK(!std::strcmp(petui::albumwidget::memoryRequirement(p, id, w), tr(STR_PET_MEMORY_UNRECORDED_HELP)));
      CHECK(!petui::memoryUnrecorded(p, unsigned(pet::Moment::Reading25h)));
      m.view = pet::View::Memory;
      m.memory = int(id);
      c->target.expectedFullText = tr(STR_PET_MEMORY_UNRECORDED_HELP);
      c->paint(m, w);
      c->checkHits();
      CHECK(c->target.expectedFullTextSeen);
    }
  }
}

static void companionContexts() {
  for (auto language : {Language::EN, Language::ES}) {
    I18N.setLanguage(language);
    pet::Profile p;
    p.created = true;
    p.saciety = 80;
    p.joy = 80;
    petui::Model m;
    m.profile = &p;
    m.today = 20000;
    CHECK(petui::companionLine(m) == tr(STR_PET_COMPANION_EGG));
    for (auto age : {pet::Stage::Newborn, pet::Stage::Baby, pet::Stage::Juvenile, pet::Stage::Adult}) {
      p.readingSeconds = pet::stageStart(age);
      CHECK(petui::companionLine(m) != nullptr);
      m.todaySeconds = 5 * 60;
      CHECK(petui::companionLine(m) == tr(STR_PET_COMPANION_READING));
      m.todaySeconds = 10 * 60;
      CHECK(petui::companionLine(m) == tr(STR_PET_COMPANION_GOAL));
      p.illness = pet::Illness::Cold;
      CHECK(petui::companionLine(m) == nullptr);
      p.illness = pet::Illness::Healthy;
      p.resting = true;
      CHECK(petui::companionLine(m) == nullptr);
      p.resting = false;
      p.hygiene = 20;
      CHECK(petui::companionLine(m) == nullptr);
      p.hygiene = 100;
      p.saciety = 20;
      CHECK(petui::companionLine(m) == nullptr);
      p.saciety = 80;
      p.energy = 10;
      CHECK(petui::companionLine(m) == nullptr);
      p.energy = 100;
      p.joy = 20;
      CHECK(petui::companionLine(m) == nullptr);
      p.joy = 80;
      m.clockKnown = false;
      CHECK(petui::companionLine(m) != tr(STR_PET_COMPANION_GOAL));
      m.clockKnown = true;
      m.historyChanged = true;
      CHECK(petui::companionLine(m) != tr(STR_PET_COMPANION_GOAL));
      m.historyChanged = false;
      m.todaySeconds = 0;
      p.lastPetDay = m.today;
      CHECK(petui::companionLine(m) == tr(STR_PET_COMPANION_PETTED));
      p.lastPetDay = 0;
    }
    p.lifeStatus = pet::LifeStatus::Lost;
    CHECK(petui::companionLine(m) == nullptr);
    p.lifeStatus = pet::LifeStatus::Retired;
    CHECK(petui::companionLine(m) == nullptr);
    p.lifeStatus = pet::LifeStatus::Active;
    m.view = pet::View::Dashboard;
    auto canvas = std::make_unique<Canvas>(480, 800);
    canvas->target.expectedRegularText = tr(STR_PET_HYGIENE);
    canvas->target.expectedEmphasizedText = "100/100";
    petui::Work work;
    canvas->paint(m, work);
    CHECK(canvas->target.regularTextSeen && canvas->target.emphasizedTextSeen);
  }
}
static void speciesBackgrounds(const char* out) {
  ArtFixture scenes("resources/apps/pet/home.bin"), habitat("resources/apps/pet/habitat.bin");
  struct Trace {
    ArtFixture* pack;
    unsigned first = 0, calls = 0;
    bool failRight = false;
    fui::Rect left{}, right{};
    static bool draw(void* ctx, fui::DrawTarget& target, fui::Rect r, uint32_t id, bool inverse) {
      auto& t = *static_cast<Trace*>(ctx);
      if (id < petui::HOME_ART_COUNT) return ArtFixture::draw(t.pack, target, r, id, inverse);
      CHECK(id == t.first + t.calls);
      CHECK(!inverse);
      if (t.calls++ == 0)
        t.left = r;
      else
        t.right = r;
      return !(t.failRight && t.calls == 2) && ArtFixture::draw(t.pack, target, r, id, inverse);
    }
  };
  I18N.setLanguage(Language::ES);
  for (unsigned orientation = 0; orientation < 4; ++orientation)
    for (unsigned species = 0; species < 3; ++species)
      for (unsigned age = 0; age <= 4; ++age) {
        const int width = orientation % 2 ? 800 : 480, height = orientation % 2 ? 480 : 800;
        auto c = std::make_unique<Canvas>(width, height);
        c->device.orientation = fui::Orientation(orientation);
        pet::Profile p;
        p.created = true;
        p.species = pet::Species(species);
        p.readingSeconds = pet::stageStart(pet::Stage(age));
        petui::Model m;
        m.profile = &p;
        m.view = pet::View::Dashboard;
        m.portraitBackgrounds = true;
        Trace trace{&scenes, petui::HOME_ART_COUNT + species * 2};
        m.scenes = {&trace, Trace::draw};
        m.habitat = {&habitat, ArtFixture::draw};
        petui::Work w;
        c->paint(m, w);
        c->checkHits();
        CHECK(trace.calls == 2);
        CHECK(trace.left.right() == trace.right.x && trace.left.y == trace.right.y);
        CHECK(trace.left.height == trace.right.height && trace.left.width == trace.right.width);
        c->write(std::string(out) + "/background-" + std::to_string(orientation) + "-" + std::to_string(species) + "-" +
                 std::to_string(age) + ".pbm");
        trace.calls = 0;
        trace.failRight = true;
        c->paint(m, w);
        c->checkHits();
        CHECK(trace.calls == 2);
        trace.calls = 0;
        m.portraitBackgrounds = false;
        c->paint(m, w);
        CHECK(trace.calls == 0);
        m.portraitBackgrounds = true;
        m.view = pet::View::Home;
        m.tab = pet::Tab::Home;
        c->paint(m, w);
        CHECK(trace.calls == 0);
        m.view = pet::View::Album;
        m.tab = pet::Tab::Album;
        c->paint(m, w);
        CHECK(trace.calls == 0);
      }
}
static void masklessPortraitFallback() {
  ArtFixture scenes("resources/apps/pet/home.bin"), habitat("resources/apps/pet/habitat.bin");
  const auto area = fui::makeRect(30, 30, 180, 160);
  for (unsigned species = 0; species < 3; ++species)
    for (unsigned age = 0; age <= 4; ++age)
      for (unsigned state = 0; state < 3; ++state)
        for (bool failedMasks : {false, true}) {
          pet::Profile p;
          p.created = true;
          p.species = pet::Species(species);
          p.readingSeconds = pet::stageStart(pet::Stage(age));
          p.resting = state == 1;
          if (state == 2) p.illness = pet::Illness::Cold;
          petui::Model m;
          m.profile = &p;
          m.scenes = {&scenes, ArtFixture::draw};
          if (failedMasks) {
            const petui::Art failure{nullptr, [](void*, fui::DrawTarget&, fui::Rect, uint32_t, bool) { return false; }};
            m.habitat = m.sleepArt = failure;
          }
          auto actual = std::make_unique<Canvas>(240, 240), expected = std::make_unique<Canvas>(240, 240);
          actual->target.fill(area, fui::Paint::solid(fui::Color::Black));
          expected->target.fill(area, fui::Paint::solid(fui::Color::Black));
          const auto scene = pet::homeScene(p);
          CHECK(ArtFixture::draw(&habitat, expected->target, area, 482 + scene * 2, true));
          CHECK(ArtFixture::draw(&scenes, expected->target, area, scene, false));
          CHECK(petui::fullCreature(m, actual->target, area, false, true));
          unsigned white = 0, leaks = 0;
          for (int y = area.y; y < area.bottom(); ++y)
            for (int x = area.x; x < area.right(); ++x) {
              const size_t at = size_t(y) * 30 + unsigned(x) / 8;
              const uint8_t bit = uint8_t(128 >> (x % 8));
              if (expected->pixels[at] & bit) {
                ++white;
                leaks += !(actual->pixels[at] & bit);
              }
            }
          CHECK(white > 0 && leaks == 0);
        }
}
static void dashboardCardDimensions() {
  for (const auto language : {Language::ES, Language::EN}) {
    I18N.setLanguage(language);
    for (unsigned orientation = 0; orientation < 4; ++orientation) {
      const bool wide = orientation % 2;
      auto c = std::make_unique<Canvas>(wide ? 800 : 480, wide ? 480 : 800);
      c->device.orientation = fui::Orientation(orientation);
      c->target.watchDashboardCards = true;
      pet::Profile p;
      p.created = true;
      petui::Model m;
      m.profile = &p;
      m.view = pet::View::Dashboard;
      m.today = 20000;
      petui::Work w;
      fui::Rect reference{};
      for (unsigned stage = 0; stage < 5; ++stage) {
        p.readingSeconds = pet::stageStart(pet::Stage(stage));
        c->target.metricFrameCount = 0;
        c->paint(m, w);
        c->checkHits();
        CHECK(c->target.metricFrameCount == 4);
        for (const auto frame : c->target.metricFrames) CHECK(frame.height == (wide ? 142 : 136));
        fui::Rect daily{};
        for (size_t i = 0; i < c->interactions.count(); ++i) {
          const auto& hit = c->interactions.data()[i];
          if (hit.action == petui::OpenReadingInfo && hit.value == 0) daily = hit.rect;
        }
        CHECK(!daily.empty() && daily.height == (wide ? 106 : 132));
        if (!stage) reference = daily;
        CHECK(daily.x == reference.x && daily.y == reference.y && daily.width == reference.width &&
              daily.height == reference.height);
      }
      if (!wide) continue;
      m.view = pet::View::ReadingInfo;
      c->target.watchReadingRules = true;
      c->target.readingRuleCount = 0;
      c->paint(m, w);
      CHECK(c->target.readingRuleCount == 3);
    }
  }
}
void referenceOverhaulChecks() {
  static_assert(petui::DISCOVERY_COUNT == 27 && petui::MEMORY_COUNT == 50);
  pet::Profile p;
  p.created = true;
  for (unsigned i = 0; i < petui::DISCOVERY_COUNT; ++i) {
    p.readingSeconds = p.gamesWon = p.sharedDays = p.streak = 0;
    p.owned = 0;
    std::memset(p.ownedExtra, 0, sizeof(p.ownedExtra));
    const auto& d = petui::DISCOVERIES[i];
    const auto set = [&](unsigned value) {
      switch (d.metric) {
        case petui::DiscoveryMetric::ReadingHours:
          p.readingSeconds = uint64_t(value) * 3600;
          break;
        case petui::DiscoveryMetric::Games:
          p.gamesWon = value;
          break;
        case petui::DiscoveryMetric::Days:
          p.sharedDays = uint16_t(value);
          break;
        case petui::DiscoveryMetric::Streak:
          p.streak = uint16_t(value);
          break;
      }
    };
    set(d.threshold - 1);
    CHECK(!petui::memoryOwned(p, pet::MOMENT_COUNT + i));
    set(d.threshold);
    CHECK(petui::memoryOwned(p, pet::MOMENT_COUNT + i));
    CHECK(!petui::memoryCaptured(p, pet::MOMENT_COUNT + i));
    CHECK(std::strcmp(petui::memoryTitle(pet::MOMENT_COUNT + i), petui::memoryTitle(2)));
    p.lifeStatus = pet::LifeStatus::Retired;
    CHECK(petui::memoryOwned(p, pet::MOMENT_COUNT + i));
    p.lifeStatus = pet::LifeStatus::Active;
  }
  p.readingSeconds = 100ULL * 3600;
  p.gamesWon = 100;
  p.sharedDays = 60;
  p.streak = 30;
  for (unsigned item = 0; item < 60; ++item) pet::own(p, item);
  p.momentMask = (1u << pet::MOMENT_COUNT) - 1;
  CHECK(petui::memoryOwnedCount(p) == 50);
  bool seen[petui::MEMORY_COUNT]{};
  const auto saved = p;
  for (unsigned i = 0; i < petui::MEMORY_COUNT; ++i) {
    const auto id = petui::albumwidget::memoryAt(p, i);
    CHECK(id < petui::MEMORY_COUNT && !seen[id] && petui::memoryOwned(p, id));
    seen[id] = true;
    CHECK(petui::memoryText(id)[0]);
    CHECK(unsigned(petui::memoryStage(p, id)) <= 4);
  }
  CHECK(!std::memcmp(&saved, &p, sizeof(p)));
  CHECK(!petui::memoryOwned(p, petui::MEMORY_COUNT));
  for (unsigned slot = 0; slot < pet::SLOT_COUNT; ++slot) {
    for (unsigned at = 0; at < pet::OBJECTS_PER_SLOT; ++at) pet::own(p, pet::objectAt(slot, at));
    p.equipped[slot] = pet::objectAt(slot, 30);
    CHECK(petui::catalogItem(p, slot, petui::CatalogFilter::Owned, 0) == p.equipped[slot]);
    CHECK(petui::catalogItem(p, slot, petui::CatalogFilter::Owned, 1) == pet::objectAt(slot, 0));
    p.equipped[slot] = pet::objectAt(slot, 40);
    CHECK(petui::catalogItem(p, slot, petui::CatalogFilter::Owned, 0) == p.equipped[slot]);
    CHECK(petui::catalogOrdinal(p, slot, petui::CatalogFilter::Owned, pet::objectAt(slot, 30)) == 31);
  }
  for (auto language : {Language::ES, Language::EN}) {
    I18N.setLanguage(language);
    for (unsigned orientation = 0; orientation < 4; ++orientation) {
      auto c = std::make_unique<Canvas>(orientation % 2 ? 800 : 480, orientation % 2 ? 480 : 800);
      c->device.orientation = fui::Orientation(orientation);
      c->target.view = "discovered-memories-only";
      petui::Model m;
      m.profile = &p;
      m.view = pet::View::Memories;
      petui::Work w;
      const unsigned perPage = orientation % 2 ? 3 : 4;
      for (unsigned page = 0; page < (petui::memoryOwnedCount(p) + perPage - 1) / perPage; ++page) {
        m.page = int(page);
        c->paint(m, w);
        c->checkHits();
        for (size_t i = 0; i < c->interactions.count(); ++i) {
          const auto& hit = c->interactions.data()[i];
          if (hit.action == petui::OpenMemory) CHECK(petui::memoryOwned(p, unsigned(hit.value)));
        }
      }
    }
  }
}
void memoryProgressionChecks(const char* out = nullptr) {
  static_assert(pet::MOMENT_COUNT == 23 && pet::STATE_BYTES == 640);
  std::unique_ptr<ArtFixture> graphics, habitat, breathing;
  if (out) {
    graphics = std::make_unique<ArtFixture>("resources/apps/pet/graphics.bin");
    habitat = std::make_unique<ArtFixture>("resources/apps/pet/habitat.bin");
    breathing = std::make_unique<ArtFixture>("resources/apps/pet/breathing.bin");
  }
  unsigned categories[4]{};
  for (unsigned i = 0; i < petui::DISCOVERY_COUNT; ++i) {
    const auto& d = petui::DISCOVERIES[i];
    CHECK(d.threshold > 0);
    ++categories[unsigned(d.metric)];
    for (unsigned j = 0; j < i; ++j)
      CHECK(d.metric != petui::DISCOVERIES[j].metric || d.threshold != petui::DISCOVERIES[j].threshold);
    if (d.metric == petui::DiscoveryMetric::ReadingHours) {
      CHECK(d.threshold < 100);
      for (unsigned saved : {1u, 5u, 10u, 25u, 50u, 100u}) CHECK(d.threshold != saved);
      pet::Profile p;
      p.created = true;
      p.readingSeconds = uint64_t(d.threshold) * 3600 - 1;
      CHECK(!petui::memoryOwned(p, pet::MOMENT_COUNT + i));
      ++p.readingSeconds;
      CHECK(petui::memoryOwned(p, pet::MOMENT_COUNT + i));
    }
  }
  CHECK(categories[0] == 13 && categories[1] == 4 && categories[2] == 6 && categories[3] == 4);
  pet::Profile p;
  p.created = true;
  p.readingSeconds = 100ULL * 3600 - 1;
  const auto before = p;
  p.readingSeconds++;
  pet::captureMoments(p, before, 20001);
  CHECK(petui::memoryCaptured(p, unsigned(pet::Moment::Reading100h)));
  CHECK(p.momentReading[unsigned(pet::Moment::Reading100h)] == 100ULL * 3600);
  CHECK(petui::memoryOwnedCount(p) < 50);
  std::array<uint8_t, pet::STATE_BYTES> bytes{}, after{};
  pet::encode(p, bytes.data());
  pet::Profile decoded;
  CHECK(pet::decode(bytes.data(), decoded));
  for (unsigned id = 0; id < petui::MEMORY_COUNT; ++id)
    CHECK(petui::memoryOwned(p, id) == petui::memoryOwned(decoded, id));
  petui::memoryOwnedCount(decoded);
  pet::encode(decoded, after.data());
  CHECK(bytes == after);

  pet::Profile inherited;
  inherited.created = true;
  inherited.generation = 8;
  inherited.heritageSeconds = 1000ULL * 3600;
  inherited.earnedFood = inherited.earnedMedicine = 1000;
  inherited.spentFood = inherited.spentMedicine = 500;
  for (unsigned item = 0; item < pet::OBJECT_COUNT; ++item) pet::own(inherited, item);
  for (unsigned slot = 0; slot < pet::SLOT_COUNT; ++slot) inherited.equipped[slot] = pet::objectAt(slot, 30);
  CHECK(petui::memoryOwnedCount(inherited) == 1);
  for (unsigned id = pet::MOMENT_COUNT; id < petui::MEMORY_COUNT; ++id) CHECK(!petui::memoryOwned(inherited, id));

  for (auto language : {Language::ES, Language::EN}) {
    I18N.setLanguage(language);
    for (unsigned orientation = 0; orientation < 4; ++orientation) {
      auto c = std::make_unique<Canvas>(orientation % 2 ? 800 : 480, orientation % 2 ? 480 : 800);
      c->device.orientation = fui::Orientation(orientation);
      c->target.view = "progressive-memory-gallery";
      for (unsigned progress = 0; progress < 4; ++progress) {
        p = {};
        p.created = progress > 0;
        if (progress >= 2) {
          p.readingSeconds = (progress == 2 ? 30ULL : 100ULL) * 3600;
          p.gamesWon = progress == 2 ? 10 : 100;
          p.sharedDays = progress == 2 ? 7 : 60;
          p.streak = progress == 2 ? 3 : 30;
          p.momentMask = progress == 2 ? 7 : (1u << pet::MOMENT_COUNT) - 1;
          for (unsigned item = 0; item < (progress == 2 ? 5u : 60u); ++item) pet::own(p, item);
        }
        const auto owned = petui::memoryOwnedCount(p);
        CHECK(progress > 1 || owned == progress);
        CHECK(progress != 3 || owned == 50);
        petui::Model m;
        m.profile = &p;
        m.view = pet::View::Memories;
        if (out) {
          m.art = {graphics.get(), ArtFixture::draw};
          m.habitat = {habitat.get(), ArtFixture::draw};
          m.sleepArt = {breathing.get(), ArtFixture::draw};
        }
        petui::Work w;
        const unsigned perPage = orientation % 2 ? 3 : 4;
        const unsigned pages = std::max(1u, (owned + perPage - 1) / perPage);
        bool seen[petui::MEMORY_COUNT]{};
        unsigned total = 0;
        const auto saved = p;
        for (unsigned page = 0; page < pages; ++page) {
          m.page = int(page);
          c->paint(m, w);
          c->checkHits();
          CHECK(unsigned(w.pages) == pages);
          unsigned cards = 0;
          for (size_t i = 0; i < c->interactions.count(); ++i) {
            const auto& hit = c->interactions.data()[i];
            if (hit.action != petui::OpenMemory) continue;
            CHECK(hit.value >= 0 && unsigned(hit.value) < petui::MEMORY_COUNT);
            CHECK(petui::memoryOwned(p, unsigned(hit.value)) && !seen[hit.value]);
            seen[hit.value] = true;
            ++cards;
            ++total;
          }
          CHECK(cards == std::min(perPage, owned - page * perPage));
          if (out && language == Language::ES && orientation < 2 && page == 0)
            c->write(std::string(out) + "/memories-" + std::to_string(progress) + "-" + std::to_string(orientation) +
                     ".pbm");
        }
        CHECK(total == owned);
        CHECK(!std::memcmp(&p, &saved, sizeof(p)));
        if (progress == 3) {
          for (unsigned item = 0; item < pet::OBJECT_COUNT; ++item) pet::own(p, item);
          p.equipped[0] = 0;
          const auto count = petui::memoryOwnedCount(p);
          CHECK(pet::unequip(p, 0));
          CHECK(petui::memoryOwnedCount(p) == count);
          p.lifeStatus = pet::LifeStatus::Retired;
          CHECK(petui::memoryOwnedCount(p) == count);
        }
      }
    }
  }
}
void ui50Checks(const char* out = nullptr) {
  ArtFixture graphics("resources/apps/pet/graphics.bin");
  for (auto language : {Language::ES, Language::EN}) {
    I18N.setLanguage(language);
    for (unsigned orientation = 0; orientation < 4; ++orientation) {
      auto c = std::make_unique<Canvas>(orientation % 2 ? 800 : 480, orientation % 2 ? 480 : 800);
      c->device.orientation = fui::Orientation(orientation);
      for (unsigned age = 0; age < 5; ++age) {
        pet::Profile p;
        p.created = true;
        p.readingSeconds = pet::stageStart(pet::Stage(age));
        p.earnedFood = 237;
        p.earnedMedicine = 81;
        petui::Model m;
        m.profile = &p;
        m.art = {&graphics, ArtFixture::draw};
        m.view = pet::View::Album;
        m.tab = pet::Tab::Album;
        petui::Work w;
        c->target.watchStageLabels = true;
        c->target.stageLabels = 0;
        c->paint(m, w);
        c->checkHits();
        CHECK(c->target.stageLabels == 5);
        c->target.watchStageLabels = false;
        if (out && orientation < 2 && (age == 1 || age == 4))
          c->write(std::string(out) + "/ui-album-" + (language == Language::ES ? "es-" : "en-") +
                   std::to_string(orientation) + "-" + std::to_string(age) + ".pbm");
        fui::Rect info{}, settings{};
        for (size_t i = 0; i < c->interactions.count(); ++i) {
          const auto& h = c->interactions.data()[i];
          if (h.action == petui::OpenSettings) settings = h.rect;
          if (h.action == petui::OpenReadingInfo) info = h.rect;
        }
        CHECK(info.width == 44 && settings.width == 44 && info.right() <= settings.x);
        // Header control positions share one paint/hit geometry.
        toybox::Interactions interactions;
        toybox::Frame frame(c->target, c->device, c->input, interactions, nullptr, {100, false, true});
        toybox::Screen screen(frame);
        pixelui::Header old;
        old.title = "PET";
        old.mark = fui::bitmapFromIcon(icon_go_settings_32);
        old.markAction = petui::OpenSettings;
        old.secondaryMark = fui::bitmapFromIcon(petui::READING_INFO_ICON);
        old.secondaryMarkAction = petui::OpenReadingInfo;
        pixelui::header(screen, old);
        fui::Rect previous{};
        for (size_t i = 0; i < interactions.count(); ++i)
          if (interactions.data()[i].action == petui::OpenSettings) previous = interactions.data()[i].rect;
        CHECK(settings.x > previous.x && settings.x - previous.x < 44);
        if (!age) continue;
        m.view = pet::View::Care;
        m.tab = pet::Tab::Care;
        c->paint(m, w);
        c->checkHits();
        unsigned readingLinks = 0;
        const auto body = pixelui::geometry(c->device).body;
        const auto stock = fui::makeRect(body.x, body.y + 54, orientation % 2 ? body.width * 34 / 100 : body.width,
                                         orientation % 2 ? 76 : 54);
        for (size_t i = 0; i < c->interactions.count(); ++i) {
          const auto& h = c->interactions.data()[i];
          if (h.action == petui::OpenReadingInfo) {
            ++readingLinks;
            CHECK(h.rect.y < 90);
          }
          CHECK(h.rect.right() <= stock.x || h.rect.x >= stock.right() || h.rect.bottom() <= stock.y ||
                h.rect.y >= stock.bottom());
        }
        CHECK(readingLinks == 1);
        if (out && orientation < 2 && age == 4)
          c->write(std::string(out) + "/ui-care-" + (language == Language::ES ? "es-" : "en-") +
                   std::to_string(orientation) + ".pbm");
      }
    }
  }
}
void placedPreviewChecks(const char* out = nullptr) {
  ArtFixture graphics("resources/apps/pet/graphics.bin"), habitat("resources/apps/pet/habitat.bin"),
      breathing("resources/apps/pet/breathing.bin");
  for (auto language : {Language::ES, Language::EN}) {
    I18N.setLanguage(language);
    for (unsigned orientation = 0; orientation < 4; ++orientation) {
      auto c = std::make_unique<Canvas>(orientation % 2 ? 800 : 480, orientation % 2 ? 480 : 800);
      c->device.orientation = fui::Orientation(orientation);
      pet::Profile p;
      p.created = true;
      p.readingSeconds = 20ULL * 3600;
      for (unsigned item = 0; item < pet::OBJECT_COUNT; ++item) pet::own(p, item);
      petui::Model m;
      m.profile = &p;
      m.art = {&graphics, ArtFixture::draw};
      m.habitat = {&habitat, ArtFixture::draw};
      m.sleepArt = {&breathing, ArtFixture::draw};
      m.catalogFilter = petui::CatalogFilter::Owned;
      petui::Work w;
      for (unsigned item = 0; item < pet::OBJECT_COUNT; ++item) {
        const auto slot = pet::objectSlot(item);
        p.equipped[slot] = uint8_t(item);
        m.projectSlot = int(slot);
        m.previewItem = uint8_t(item);
        m.view = pet::View::ProjectPreview;
        m.writable = true;
        c->target.view = "placed-preview";
        c->target.watchPlacedBand = true;
        c->target.placedBand = {};
        char label[64];
        pixelui::copyCaps(label, sizeof(label), tr(STR_PET_EQUIPPED));
        c->target.expectedFullText = label;
        c->target.expectedFullTextSeen = false;
        c->paint(m, w);
        c->checkHits();
        CHECK(c->target.expectedFullTextSeen);
        c->target.expectedFullText = nullptr;
        const auto band = c->target.placedBand;
        CHECK(!band.empty());
        CHECK(darkPixels(*c, band) > unsigned(band.width) * band.height / 2);
        unsigned remove = 0;
        for (size_t i = 0; i < c->interactions.count(); ++i) {
          const auto& hit = c->interactions.data()[i];
          CHECK(hit.action != petui::SelectProject);
          if (hit.action == petui::RemoveProject) {
            ++remove;
            CHECK(band.bottom() + 8 == hit.rect.y);
          }
          CHECK(hit.rect.right() <= band.x || hit.rect.x >= band.right() || hit.rect.bottom() <= band.y ||
                hit.rect.y >= band.bottom());
        }
        CHECK(remove == 1);
        c->target.watchPlacedBand = false;
        if (out && item == 6 && orientation < 2)
          c->write(std::string(out) + "/placed-preview-" + (language == Language::ES ? "es-" : "en-") +
                   std::to_string(orientation) + ".pbm");
      }
      m.view = pet::View::Projects;
      m.tab = pet::Tab::Home;
      m.projectSlot = 0;
      p.equipped[0] = 0;
      m.page = 0;
      c->target.view = "solid-catalog-border";
      c->paint(m, w);
      c->checkHits();
      for (size_t i = 0; i < c->interactions.count(); ++i) {
        const auto& hit = c->interactions.data()[i];
        if (hit.action != petui::OpenProjectPreview || hit.value != 0) continue;
        const auto edge = fui::makeRect(hit.rect.x + 2, hit.rect.y + 2, hit.rect.width - 4, 4);
        CHECK(darkPixels(*c, edge) == unsigned(edge.width) * edge.height);
      }
      if (out && orientation < 2)
        c->write(std::string(out) + "/solid-catalog-" + (language == Language::ES ? "es-" : "en-") +
                 std::to_string(orientation) + ".pbm");
    }
  }
}
void homeRemovalScreens() {
  for (auto language : {Language::ES, Language::EN}) {
    I18N.setLanguage(language);
    for (unsigned orientation = 0; orientation < 4; ++orientation) {
      auto c = std::make_unique<Canvas>(orientation % 2 ? 800 : 480, orientation % 2 ? 480 : 800);
      c->device.orientation = fui::Orientation(orientation);
      c->target.view = "home-removal-empty-states";
      pet::Profile p;
      p.created = true;
      p.readingSeconds = 72000;
      pet::own(p, p.project);
      petui::Model m;
      m.profile = &p;
      m.view = pet::View::Home;
      m.tab = pet::Tab::Home;
      petui::Work w;
      c->target.expectedFullText = tr(STR_PET_HOME_NO_SELECTION);
      c->paint(m, w);
      c->checkHits();
      CHECK(c->target.expectedFullTextSeen);
      for (unsigned id = 0; id < pet::OBJECT_COUNT; ++id) pet::own(p, id);
      c->target.expectedFullTextSeen = false;
      c->paint(m, w);
      c->checkHits();
      CHECK(c->target.expectedFullTextSeen);
      p.equipped[0] = 0;
      c->target.expectedFullTextSeen = false;
      c->paint(m, w);
      c->checkHits();
      CHECK(!c->target.expectedFullTextSeen);
      c->target.expectedFullText = nullptr;
      for (unsigned slot = 0; slot < pet::SLOT_COUNT; ++slot) {
        const unsigned id = pet::objectAt(slot, 40);
        p.equipped[slot] = uint8_t(id);
        m.view = pet::View::ProjectPreview;
        m.catalogFilter = petui::CatalogFilter::Owned;
        m.previewItem = uint8_t(id);
        m.projectSlot = int(slot);
        const auto saved = p;
        for (bool writable : {false, true}) {
          m.writable = writable;
          c->paint(m, w);
          c->checkHits();
          unsigned remove = 0, place = 0;
          for (size_t i = 0; i < c->interactions.count(); ++i) {
            const auto& hit = c->interactions.data()[i];
            remove += hit.action == petui::RemoveProject;
            place += hit.action == petui::SelectProject;
          }
          CHECK(remove == unsigned(writable) && !place);
          CHECK(!std::memcmp(&saved, &p, sizeof(p)));
        }
      }
    }
  }
}
static void resourcesRequiredScreens(const char* output) {
  pet::Profile profile;
  petui::Work work;
  petui::Model model;
  model.profile = &profile;
  model.resourcesReady = false;
  for (const auto language : {Language::EN, Language::ES}) {
    I18N.setLanguage(language);
    for (unsigned orientation = 0; orientation < 4; ++orientation) {
      Canvas c(orientation % 2 ? 800 : 480, orientation % 2 ? 480 : 800);
      c.device.orientation = fui::Orientation(orientation);
      for (const auto view : {pet::View::New, pet::View::Dashboard, pet::View::Play, pet::View::ProjectPreview}) {
        model.view = view;
        c.paint(model, work);
        c.checkHits();
        CHECK(c.interactions.count() == 1);
        CHECK(c.interactions.data()[0].action == petui::Back);
        CHECK(c.interactions.data()[0].rect.height >= 44);
      }
      if (output)
        c.write(std::string(output) + "/pet-resources-" + (language == Language::ES ? "es-" : "en-") +
                std::to_string(orientation) + ".pbm");
    }
  }
}
int main(int argc, char** argv) {
  if (argc > 1 && !std::strcmp(argv[1], "--memories-only")) {
    referenceOverhaulChecks();
    memoryProgressionChecks(argc > 2 ? argv[2] : nullptr);
    ui50Checks(argc > 2 ? argv[2] : nullptr);
    placedPreviewChecks(argc > 2 ? argv[2] : nullptr);
    homeRemovalScreens();
    std::printf("PET progressive memories: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 1 && !std::strcmp(argv[1], "--resources-only")) {
    resourcesRequiredScreens(argc > 2 ? argv[2] : nullptr);
    std::printf("PET required resources: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }
  resourcesRequiredScreens(nullptr);

  homeRemovalScreens();
  if (argc > 1 && !std::strcmp(argv[1], "--home-only")) {
    std::printf("PET Home removal: %d checks,%d failures\n", checks, failures);
    return failures ? 1 : 0;
  }
  referenceOverhaulChecks();
  memoryProgressionChecks();
  ui50Checks();
  placedPreviewChecks();
  redesignRegressionScreens();
  consistencyScreens();
  dashboardCardDimensions();
  companionContexts();
  communicationScreens();
  guidanceScreens();
  clarityScreens();
  if (argc > 1 && !std::strcmp(argv[1], "--communication-only")) {
    std::printf("PET communication: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }
  const char* out = argc > 1 ? argv[1] : ".";
  speciesBackgrounds(out);
  masklessPortraitFallback();
  gameScreens(out);
#ifdef PET_GAMES_ONLY
  std::printf("PET game screens: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
#endif
  resultAndSnapshotScreens(out);
  polishScreens(out);
  const bool withArt = argc > 2;
  ArtFixture graphics("resources/apps/pet/graphics.bin"), scenes("resources/apps/pet/home.bin"),
      habitat("resources/apps/pet/habitat.bin"), breathing("resources/apps/pet/breathing.bin"),
      farewell("resources/apps/pet/farewell.bin"), collection("resources/apps/pet/collection.bin");
#ifndef PET_AFFECTION_ONLY
  static constexpr pet::View views[] = {
      pet::View::New,       pet::View::Info,      pet::View::Rules,      pet::View::Dashboard, pet::View::Care,
      pet::View::Home,      pet::View::Album,     pet::View::Settings,   pet::View::Projects,  pet::View::Games,
      pet::View::Play,      pet::View::Reward,    pet::View::Evolution,  pet::View::Memory,    pet::View::Lives,
      pet::View::Notice,    pet::View::Health,    pet::View::LifeDetail, pet::View::Retire,    pet::View::Name,
      pet::View::Affection, pet::View::HealthHelp};
  static constexpr const char* names[] = {"new",       "info",     "rules",     "pet",        "care",   "home",
                                          "album",     "settings", "projects",  "games",      "play",   "reward",
                                          "evolution", "memory",   "lives",     "notice",     "health", "life-detail",
                                          "retire",    "name",     "affection", "health-help"};
  for (const auto lang : {Language::ES, Language::EN}) {
    I18N.setLanguage(lang);
    for (const int width : {480, 800}) {
      const int height = width == 480 ? 800 : 480;
      for (unsigned i = 0; i < sizeof(views) / sizeof(views[0]); ++i) {
        for (int state = 0; state < (views[i] == pet::View::Name        ? 5
                                     : views[i] == pet::View::Play      ? 5
                                     : views[i] == pet::View::New       ? 6
                                     : views[i] == pet::View::Dashboard ? 8
                                     : views[i] == pet::View::Evolution ? 5
                                     : views[i] == pet::View::Care      ? 5
                                     : views[i] == pet::View::Health    ? 6
                                     : views[i] == pet::View::Games     ? 2
                                     : views[i] == pet::View::Home || views[i] == pet::View::Health ||
                                             views[i] == pet::View::Dashboard || views[i] == pet::View::Lives
                                         ? 4
                                         : 1);
             ++state) {
          auto canvas = std::make_unique<Canvas>(width, height);
          auto& c = *canvas;
          c.target.view = names[i];
          pet::Profile p;
          p.created = true;
          std::strcpy(p.name, state ? "MMMMMMMMMMMM" : "LUNA");
          p.readingSeconds = 36900;
          p.noticedSeconds = 34800;
          p.sharedDays = 12;
          p.streak = 7;
          p.saciety = 72;
          p.joy = 84;
          p.energy = 61;
          p.spentFood = 117;
          p.owned = (1u << 0) | (1u << 1) | (1u << 3) | (1u << 6) | (1u << 9);
          p.equipped[0] = 1;
          p.equipped[1] = 3;
          p.equipped[2] = 6;
          p.equipped[3] = 9;
          p.memories = 3;
          p.noticedStage = pet::Stage::Baby;
          if (views[i] == pet::View::Home && state > 0) {
            for (unsigned object = 0; object < pet::OBJECT_COUNT; ++object) pet::own(p, object);
            for (unsigned slot = 0; slot < pet::SLOT_COUNT; ++slot) p.equipped[slot] = uint8_t(slot * 3 + state - 1);
          }
          if (views[i] == pet::View::Health || views[i] == pet::View::Home) {
            if (state == 1) {
              p.hygiene = 15;
              p.illness = pet::Illness::Malaise;
            }
            if (state == 2) {
              p.resting = true;
              p.hygiene = 60;
            }
            if (state == 3) p.readingSeconds = 0;
          }
          if (views[i] == pet::View::Dashboard && state) {
            p.lifeStatus = state == 1 ? pet::LifeStatus::Lost : pet::LifeStatus::Retired;
          }
          if ((views[i] == pet::View::Dashboard && state >= 4) ||
              ((views[i] == pet::View::Care || views[i] == pet::View::Games) && state >= 1)) {
            p.lifeStatus = pet::LifeStatus::Active;
            p.readingSeconds = state == 5 ? 1199 : 0;
            p.noticedSeconds = 0;
          }
          if (views[i] == pet::View::Care && state == 2) p.lifeStatus = pet::LifeStatus::Lost;
          if (views[i] == pet::View::Evolution) {
            p.noticedStage = pet::Stage(std::max(0, state - 1));
            p.readingSeconds = pet::stageStart(pet::Stage(std::max(1, state)));
          }
          if ((views[i] == pet::View::Care && state >= 3) || (views[i] == pet::View::Health && state >= 4)) {
            p.readingSeconds = 72000;
            p.lifeStatus = state % 2 ? pet::LifeStatus::Lost : pet::LifeStatus::Retired;
            p.hygiene = 15;
            p.illness = pet::Illness::Malaise;
          }
          if (views[i] == pet::View::Dashboard && state >= 6) {
            p.lifeStatus = pet::LifeStatus::Active;
            p.readingSeconds = 1200;
            if (state == 7) p.name[0] = 0;
          }
          pet::History history;
          if (views[i] == pet::View::LifeDetail || (views[i] == pet::View::Lives && state)) {
            history.count = 12;
            for (unsigned j = 0; j < 12; ++j) {
              history.records[j] = {j + 1, 72000, 1700000000, 24, pet::Species(j % 3), pet::LifeStatus::Retired};
              std::strcpy(history.records[j].name, j % 2 ? "ÑÑÑÑÑÑÑÑÑÑÑÑ" : "MMMMMMMMMMMM");
            }
          }
          pet::GameState game;
          game.begin(pet::Game::Pairs, 1);
          if (views[i] == pet::View::New) p.species = pet::Species(state % 3);
          if (views[i] == pet::View::Play) {
            if (state == 1) game.phase = pet::GamePhase::Finished;
            if (state >= 2) game.begin(state == 4 ? pet::Game::Puzzle : pet::Game::Sequence, 1);
            if (state == 3) game.advance();
          }
          petui::Model m;
          m.profile = &p;
          if (withArt) {
            m.art = {&graphics, ArtFixture::draw};
            m.scenes = {&scenes, ArtFixture::draw};
            m.habitat = {&habitat, ArtFixture::draw};
          }
          static constexpr const char* nameDrafts[] = {"", "LUNA", "MMMMMMMMMMMM", "ÑÑÑÑÑÑÑÑÑÑÑÑ", "LUNA SOL"};
          if (views[i] == pet::View::Name) m.nameDraft = nameDrafts[state];
          m.choice = p.species;
          m.history = &history;
          m.historyWritable = !(views[i] == pet::View::Lives && state == 3);
          m.today = 20000;
          m.game = &game;
          m.view = views[i];
          m.todaySeconds = 2100;
          m.weekDays = 3;
          if (views[i] == pet::View::Lives && state == 2) p.readingSeconds = 72000;
          m.tab = views[i] == pet::View::Health || views[i] == pet::View::Care || views[i] == pet::View::Games ||
                          views[i] == pet::View::Play
                      ? pet::Tab::Care
                  : views[i] == pet::View::Home || views[i] == pet::View::Projects ? pet::Tab::Home
                  : views[i] == pet::View::LifeDetail || views[i] == pet::View::Album ||
                          views[i] == pet::View::Memory || views[i] == pet::View::Lives
                      ? pet::Tab::Album
                      : pet::Tab::Pet;
          petui::Work work;
          c.paint(m, work);
          if (views[i] == pet::View::New && state >= 3) {
            int focus = -1;
            for (size_t hit = 0; hit < c.interactions.count(); ++hit)
              if (c.interactions.data()[hit].action == petui::SelectSpecies &&
                  c.interactions.data()[hit].value == int(p.species))
                focus = int(hit);
            CHECK(focus >= 0);
            paintclock::notePainted();
            fui::InputSnapshot nextFocus;
            nextFocus.focusNext = true;
            for (size_t step = 0; step < c.interactions.count() &&
                                  (!c.interactions.hasButtonFocus() || c.interactions.focusedIndex() != focus);
                 ++step)
              c.interactions.route(nextFocus);
            CHECK(c.interactions.hasButtonFocus() && c.interactions.focusedIndex() == focus);
            static constexpr const char* labelsEs[] = {"HUEVO DEL BOSQUE", "HUEVO LUNAR", "HUEVO VOLCÁN"};
            static constexpr const char* labelsEn[] = {"FOREST EGG", "LUNAR EGG", "VOLCANO EGG"};
            c.target.expectedFocusName = (lang == Language::ES ? labelsEs : labelsEn)[unsigned(p.species)];
            std::fill(c.pixels.begin(), c.pixels.end(), 255);
            toybox::Frame focusedFrame(c.target, c.device, c.input, c.interactions, nullptr, {100, false, true});
            toybox::Screen focusedScreen(focusedFrame);
            c.paint(m, work);
            CHECK(c.target.focusedLabelWasWhite);
          }
          if (views[i] == pet::View::Dashboard && p.lifeStatus == pet::LifeStatus::Active) {
            int careLinks = 0, homeLinks = 0;
            for (size_t hit = 0; hit < c.interactions.count(); ++hit) {
              const auto& control = c.interactions.data()[hit];
              if (control.action == petui::ChangeTab && control.value == 1) ++careLinks;
              if (control.action == petui::ChangeTab && control.value == 2) ++homeLinks;
            }
            CHECK(careLinks == 1 && homeLinks == 1);
          }
          if (views[i] == pet::View::New) {
            unsigned configuration = 0;
            for (size_t hit = 0; hit < c.interactions.count(); ++hit)
              if (c.interactions.data()[hit].action == petui::OpenSettings) ++configuration;
            CHECK(configuration == 1);
          }
          if (views[i] == pet::View::Name) {
            paintclock::notePainted();
            for (int row = 0; row < 3; ++row)
              for (int n = 0; n < wordlekeys::length(row, work.keys.spanish); ++n) {
                const auto key = wordlekeys::keyRect(work.keys, row, n);
                fui::InputSnapshot tap;
                tap.touchReleased = true;
                tap.touchX = key.x + key.width / 2;
                tap.touchY = key.y + key.height / 2;
                CHECK(c.interactions.route(tap).action == petui::NameKey);
                CHECK(wordlekeys::keyAt(work.keys, tap.touchX, tap.touchY) == wordlekeys::ROWS[row][n]);
              }
          }
          if (views[i] == pet::View::Name && state == 1) {
            fui::InputSnapshot step;
            step.focusNext = true;
            bool selected = false;
            for (unsigned n = 0; n < 60 && !selected; ++n) {
              petui::routeNaming(c.interactions, work.keys, step);
              fui::Rect key;
              selected = c.interactions.buttonFocusRect(key) &&
                         wordlekeys::keyAt(work.keys, key.x + key.width / 2, key.y + key.height / 2) == 'W';
            }
            CHECK(selected);
            const auto focused = c.interactions.focusPosition();
            fui::InputSnapshot confirm;
            confirm.confirm = true;
            for (unsigned repeat = 0; repeat < 2; ++repeat) {
              const auto picked = petui::routeNaming(c.interactions, work.keys, confirm);
              CHECK(picked.action == petui::NameKey && picked.value == 'W');
              CHECK(c.interactions.hasButtonFocus() && c.interactions.focusPosition() == focused);
              toybox::Frame rebuilt(c.target, c.device, c.input, c.interactions, nullptr, {100, false, true});
              toybox::Screen rebuiltScreen(rebuilt);
              c.paint(m, work);
              paintclock::notePainted();
            }
            petui::routeNaming(c.interactions, work.keys, step);
            const auto adjacent = petui::routeNaming(c.interactions, work.keys, confirm);
            CHECK(adjacent.action == petui::NameKey && adjacent.value == 'E');
          }
          if (pet::stage(p.readingSeconds) == pet::Stage::Egg) {
            if (views[i] == pet::View::Dashboard) CHECK(c.target.eggTextSeen);
            if (views[i] == pet::View::Dashboard || views[i] == pet::View::Care || views[i] == pet::View::Health ||
                views[i] == pet::View::Games) {
              CHECK(!c.target.vitalValueSeen);
              for (size_t hit = 0; hit < c.interactions.count(); ++hit) {
                const auto action = c.interactions.data()[hit].action;
                CHECK(action != petui::Feed && action != petui::Stroke && action != petui::Rest &&
                      action != petui::OpenHealth && action != petui::OpenGames && action != petui::BeginGame &&
                      action != petui::Clean && action != petui::Cure);
              }
            }
          }
          if (views[i] == pet::View::Health && p.lifeStatus != pet::LifeStatus::Active) {
            for (size_t hit = 0; hit < c.interactions.count(); ++hit)
              CHECK(c.interactions.data()[hit].action != petui::Clean &&
                    c.interactions.data()[hit].action != petui::Cure);
          }
          if (views[i] == pet::View::Care && p.lifeStatus != pet::LifeStatus::Active &&
              pet::stage(p.readingSeconds) != pet::Stage::Egg) {
            bool historyLink = false;
            for (size_t hit = 0; hit < c.interactions.count(); ++hit) {
              CHECK(c.interactions.data()[hit].action != petui::OpenHealth);
              if (c.interactions.data()[hit].action == petui::OpenLives) historyLink = true;
            }
            CHECK(historyLink);
          }
          c.checkHits();
          const auto path = std::string(out) + "/" + (lang == Language::ES ? "es-" : "en-") + std::to_string(width) +
                            "-" + names[i] + (state ? "-" + std::to_string(state) : "") + ".pbm";
          c.write(path);
        }
      }
    }
  }
  for (const auto lang : {Language::ES, Language::EN}) {
    I18N.setLanguage(lang);
    for (const int width : {480, 800}) {
      const int height = width == 480 ? 800 : 480;
      for (unsigned n = 0; n < pet::OBJECTS_PER_SLOT; ++n) {
        auto canvas = std::make_unique<Canvas>(width, height);
        auto& c = *canvas;
        c.target.view = "home-catalog";
        pet::Profile p;
        p.created = p.resting = true;
        p.species = pet::Species(n % 3);
        std::strcpy(p.name, "LUNA");
        p.readingSeconds = pet::stageStart(pet::Stage(1 + n % 4));
        p.project = uint8_t(pet::objectAt(n % 4, n));
        for (unsigned slot = 0; slot < pet::SLOT_COUNT; ++slot) {
          p.equipped[slot] = uint8_t(pet::objectAt(slot, n));
          pet::own(p, p.equipped[slot]);
        }
        pet::History history;
        petui::Model m;
        m.profile = &p;
        m.history = &history;
        m.view = pet::View::Home;
        m.tab = pet::Tab::Home;
        if (withArt) {
          m.art = {&graphics, ArtFixture::draw};
          m.scenes = {&scenes, ArtFixture::draw};
          m.habitat = {&habitat, ArtFixture::draw};
        }
        petui::Work w;
        c.paint(m, w);
        c.checkHits();
        if (withArt && lang == Language::ES && (n < 5 || n == 12 || n == 28 || n == 45 || n == 59))
          c.write(std::string(out) + "/catalog-home-" + std::to_string(width) + "-" + std::to_string(n) + ".pbm");
      }
      for (unsigned slot = 0; slot < pet::SLOT_COUNT; ++slot) {
        const int pages = width == 480 ? 10 : 15;
        for (int page = 0; page < pages; ++page) {
          auto canvas = std::make_unique<Canvas>(width, height);
          auto& c = *canvas;
          c.target.view = "projects-catalog";
          pet::Profile p;
          p.created = true;
          p.readingSeconds = 1200;
          petui::Model m;
          m.profile = &p;
          m.view = pet::View::Projects;
          m.tab = pet::Tab::Home;
          m.projectSlot = int(slot);
          m.page = page;
          if (withArt) m.habitat = {&habitat, ArtFixture::draw};
          petui::Work w;
          c.paint(m, w);
          CHECK(w.pages == pages);
          c.checkHits();
          unsigned cards = 0;
          for (size_t hit = 0; hit < c.interactions.count(); ++hit) {
            const auto& action = c.interactions.data()[hit];
            if (action.action == petui::OpenProjectPreview) {
              CHECK(action.value >= 0 && pet::objectSlot(unsigned(action.value)) == slot);
              ++cards;
            }
          }
          CHECK(cards == (width == 480 ? 6u : 4u));
          if (withArt && lang == Language::ES && page == pages - 1)
            c.write(std::string(out) + "/catalog-projects-" + std::to_string(width) + "-" + std::to_string(slot) +
                    ".pbm");
        }
      }
    }
  }
  for (unsigned species = 0; species < 3; ++species)
    for (unsigned age = 0; age < 5; ++age)
      for (unsigned pose = 0; pose < 3; ++pose)
        for (unsigned n = 0; n < 60; ++n) {
          pet::Profile p;
          p.species = pet::Species(species);
          p.readingSeconds = pet::stageStart(pet::Stage(age));
          p.equipped[0] = uint8_t(pet::objectAt(0, n));
          p.resting = pose == 1;
          p.illness = pose == 2 ? pet::Illness::Cold : pet::Illness::Healthy;
          const auto petBox = petui::habitatlayout::pet(p);
          CHECK(petBox.x >= 0 && petBox.y >= 0 && petBox.right() <= 432 && petBox.bottom() <= 216);
          if (pose != 1 || age == 0) CHECK(petBox.bottom() == 190);
        }
  I18N.setLanguage(Language::ES);
  uint32_t sleepHitSignatures[4][3][4]{};
  for (unsigned orientation = 0; orientation < 4; ++orientation)
    for (unsigned species = 0; species < 3; ++species)
      for (unsigned age = 1; age < 5; ++age)
        for (unsigned step = 0; step < 4; ++step) {
          const int width = orientation % 2 ? 800 : 480, height = orientation % 2 ? 480 : 800;
          auto canvas = std::make_unique<Canvas>(width, height);
          auto& c = *canvas;
          c.device.orientation = fui::Orientation(orientation);
          c.target.view = "sleep-animation";
          pet::Profile p;
          p.created = true;
          p.resting = true;
          p.species = pet::Species(species);
          p.readingSeconds = pet::stageStart(pet::Stage(age));
          std::strcpy(p.name, species == 0 ? "SILVA" : species == 1 ? "NOX" : "EMBER");
          for (unsigned slot = 0; slot < 4; ++slot) {
            p.equipped[slot] = uint8_t(pet::objectAt(slot, species));
            pet::own(p, p.equipped[slot]);
          }
          petui::Model m;
          m.profile = &p;
          m.view = pet::View::Home;
          m.tab = pet::Tab::Home;
          m.sleepStep = uint8_t(step);
          if (withArt) {
            m.art = {&graphics, ArtFixture::draw};
            m.scenes = {&scenes, ArtFixture::draw};
            m.habitat = {&habitat, ArtFixture::draw};
            m.sleepArt = {&breathing, ArtFixture::draw};
          }
          petui::Work work;
          c.paint(m, work);
          c.checkHits();
          // Animation never changes input geometry or destinations between phases.
          uint32_t signature = 2166136261u;
          const auto mix = [&](int value) { signature = (signature ^ uint32_t(value)) * 16777619u; };
          for (size_t hit = 0; hit < c.interactions.count(); ++hit) {
            const auto& action = c.interactions.data()[hit];
            mix(action.rect.x);
            mix(action.rect.y);
            mix(action.rect.width);
            mix(action.rect.height);
            mix(action.action);
            mix(action.value);
          }
          auto& initial = sleepHitSignatures[orientation][species][age - 1];
          if (step)
            CHECK(signature == initial);
          else
            initial = signature;
          if (orientation < 2 && age == 4)
            c.write(std::string(out) + "/sleep-" + std::to_string(width) + "-" + std::to_string(species) + "-" +
                    std::to_string(step) + ".pbm");
        }
  for (unsigned orientation = 0; orientation < 2; ++orientation)
    for (unsigned species = 0; species < 3; ++species)
      for (unsigned age = 1; age < 5; ++age)
        for (unsigned mood = 0; mood < 6; ++mood)
          for (auto view : {pet::View::Dashboard, pet::View::Home}) {
            uint32_t firstPixels = 0, firstHits = 0;
            bool changed = false;
            for (unsigned step = 0; step < 4; ++step) {
              const int width = orientation ? 800 : 480, height = orientation ? 480 : 800;
              auto canvas = std::make_unique<Canvas>(width, height);
              auto& c = *canvas;
              c.target.view = "all-pose-breathing";
              pet::Profile p;
              p.created = true;
              p.species = pet::Species(species);
              p.readingSeconds = pet::stageStart(pet::Stage(age));
              p.joy = mood == 1 ? 90 : mood == 5 ? 10 : 60;
              p.saciety = mood == 2 ? 20 : 70;
              p.energy = mood == 3 ? 10 : 80;
              p.illness = mood == 4 ? pet::Illness::Cold : pet::Illness::Healthy;
              std::strcpy(p.name, species == 0 ? "SILVA" : species == 1 ? "NOX" : "EMBER");
              petui::Model m;
              m.profile = &p;
              m.view = view;
              m.tab = view == pet::View::Home ? pet::Tab::Home : pet::Tab::Pet;
              m.sleepStep = uint8_t(step);
              if (withArt) {
                m.art = {&graphics, ArtFixture::draw};
                m.scenes = {&scenes, ArtFixture::draw};
                m.habitat = {&habitat, ArtFixture::draw};
                m.sleepArt = {&breathing, ArtFixture::draw};
              }
              petui::Work w;
              c.paint(m, w);
              c.checkHits();
              const auto pixels = pet::checksum(c.pixels.data(), c.pixels.size());
              uint32_t hits = 2166136261u;
              for (size_t i = 0; i < c.interactions.count(); ++i) {
                const auto& h = c.interactions.data()[i];
                for (const int value :
                     {int(h.rect.x), int(h.rect.y), int(h.rect.width), int(h.rect.height), int(h.action), int(h.value)})
                  hits = (hits ^ uint32_t(value)) * 16777619u;
              }
              if (!step) {
                firstPixels = pixels;
                firstHits = hits;
              } else {
                CHECK(hits == firstHits);
                changed = changed || pixels != firstPixels;
              }
              if (orientation == 0 && age == 4)
                c.write(std::string(out) + "/breathing-" + std::to_string(species) + "-" + std::to_string(mood) + "-" +
                        (view == pet::View::Home ? "home-" : "pet-") + std::to_string(step) + ".pbm");
            }
            CHECK(changed);
          }
  for (unsigned species = 0; species < 3; ++species)
    for (auto view : {pet::View::Dashboard, pet::View::Home}) {
      uint32_t first = 0;
      bool changed = false;
      for (unsigned step = 0; step < 4; ++step) {
        auto c = std::make_unique<Canvas>(480, 800);
        c->target.view = "egg-wobble";
        pet::Profile p;
        p.created = true;
        p.species = pet::Species(species);
        petui::Model m;
        m.profile = &p;
        m.view = view;
        m.tab = view == pet::View::Home ? pet::Tab::Home : pet::Tab::Pet;
        m.sleepStep = uint8_t(step);
        if (withArt) {
          m.art = {&graphics, ArtFixture::draw};
          m.scenes = {&scenes, ArtFixture::draw};
          m.habitat = {&habitat, ArtFixture::draw};
          m.sleepArt = {&breathing, ArtFixture::draw};
        }
        petui::Work w;
        c->paint(m, w);
        c->checkHits();
        const auto hash = pet::checksum(c->pixels.data(), c->pixels.size());
        if (!step)
          first = hash;
        else
          changed = changed || hash != first;
        c->write(std::string(out) + "/egg-wobble-" + std::to_string(species) + "-" +
                 (view == pet::View::Home ? "home-" : "pet-") + std::to_string(step) + ".pbm");
      }
      CHECK(changed == withArt);
    }
  {
    auto c = std::make_unique<Canvas>(480, 800);
    c->target.fill(fui::makeRect(0, 0, 480, 800), fui::Paint::solid(fui::Color::Black));
    const auto before = pet::checksum(c->pixels.data(), c->pixels.size());
    pet::Profile p;
    p.created = p.resting = true;
    p.readingSeconds = pet::stageStart(pet::Stage::Adult);
    petui::Model m;
    m.profile = &p;
    m.view = pet::View::Home;
    m.sleepStep = 2;
    petui::sleepMarks(c->screen, m, fui::makeRect(0, 0, 432, 216));
    CHECK(before != pet::checksum(c->pixels.data(), c->pixels.size()));
    c->write(std::string(out) + "/sleep-Z-white-outline.pbm");
    p.resting = false;
    p.joy = 10;
    CHECK(petmood::select(p) == petmood::Mood::Sad);
    p.joy = 90;
    CHECK(petmood::select(p) == petmood::Mood::Happy);
    p.illness = pet::Illness::Cold;
    CHECK(petmood::select(p) == petmood::Mood::Sick);
  }
  for (auto lang : {Language::ES, Language::EN}) {
    I18N.setLanguage(lang);
    for (unsigned orientation = 0; orientation < 4; ++orientation)
      for (unsigned species = 0; species < 3; ++species)
        for (uint32_t gen : {1u, 2u, UINT32_MAX})
          for (const char* name : {"LUNA", "MMMMMMMMMMMM", ""})
            for (auto view : {pet::View::Death, pet::View::Dashboard, pet::View::Home}) {
              const int width = orientation % 2 ? 800 : 480, height = orientation % 2 ? 480 : 800;
              auto canvas = std::make_unique<Canvas>(width, height);
              auto& c = *canvas;
              c.device.orientation = fui::Orientation(orientation);
              c.target.view = "farewell";
              pet::Profile p;
              p.created = true;
              p.species = pet::Species(species);
              p.generation = gen;
              p.lifeStatus = pet::LifeStatus::Lost;
              p.readingSeconds = pet::stageStart(pet::Stage((gen + species) % 5));
              std::strcpy(p.name, name);
              petui::Model m;
              m.profile = &p;
              m.view = view;
              m.scenes = {nullptr, [](void*, fui::DrawTarget&, fui::Rect, uint32_t, bool) {
                            CHECK(false);
                            return false;
                          }};
              m.sleepArt = m.scenes;
              m.art = {&graphics, [](void* ctx, fui::DrawTarget& t, fui::Rect r, uint32_t id, bool inverse) {
                         CHECK(id >= 51);
                         return ArtFixture::draw(ctx, t, r, id, inverse);
                       }};
              m.habitat = {&habitat, [](void* ctx, fui::DrawTarget& t, fui::Rect r, uint32_t id, bool inverse) {
                             CHECK(id < 482);
                             return ArtFixture::draw(ctx, t, r, id, inverse);
                           }};
              if (withArt) m.farewellArt = {&farewell, ArtFixture::draw};
              petui::Work work;
              c.paint(m, work);
              c.checkHits();
              CHECK(c.target.generationSeen);
              if (view == pet::View::Death) {
                unsigned choices = 0;
                for (size_t i = 0; i < c.interactions.count(); ++i) {
                  const auto& hit = c.interactions.data()[i];
                  CHECK(hit.action != petui::ChangeTab && hit.action != petui::OpenSettings);
                  if (hit.action == petui::AcknowledgeDeath) {
                    CHECK(hit.value == 0 || hit.value == 1);
                    ++choices;
                  }
                }
                CHECK(choices == 2);
              }
              if (orientation < 2 && gen == 2 && !std::strcmp(name, "LUNA"))
                c.write(std::string(out) + "/farewell-" + (lang == Language::ES ? "es-" : "en-") +
                        std::to_string(width) + "-" + std::to_string(species) + "-" + std::to_string(unsigned(view)) +
                        ".pbm");
            }
  }
  for (auto lang : {Language::ES, Language::EN}) {
    I18N.setLanguage(lang);
    for (unsigned orientation = 0; orientation < 4; ++orientation) {
      const int width = orientation % 2 ? 800 : 480, height = orientation % 2 ? 480 : 800;
      for (unsigned goal = 0; goal < 4; ++goal)
        for (unsigned state = 0; state < 5; ++state) {
          auto canvas = std::make_unique<Canvas>(width, height);
          auto& c = *canvas;
          c.device.orientation = fui::Orientation(orientation);
          c.target.view = "daily-goal";
          pet::Profile p;
          p.created = true;
          p.goalIndex = uint8_t(goal);
          p.readingSeconds = 420;
          std::strcpy(p.name, "LUNA");
          petui::Model m;
          m.profile = &p;
          m.view = pet::View::Dashboard;
          m.today = 20000;
          m.todaySeconds = state ? int(pet::GOALS[goal]) * 60 : 120;
          p.readingSeconds = std::max<uint64_t>(420, uint64_t(m.todaySeconds));
          if (state == 2) p.lastStoryDay = m.today;
          if (state == 3) m.historyChanged = true;
          if (state == 4) m.writable = false;
          if (withArt) {
            m.art = {&graphics, ArtFixture::draw};
            m.habitat = {&habitat, ArtFixture::draw};
          }
          char time[64], caps[64], goalText[64];
          if (state == 3 || state == 4)
            std::snprintf(time, sizeof(time), "-");
          else
            std::snprintf(time, sizeof(time), tr(STR_PET_DAILY_FRACTION),
                          std::min(unsigned(std::max(0, m.todaySeconds) / 60), unsigned(pet::GOALS[goal])),
                          unsigned(pet::GOALS[goal]));
          pixelui::copyCaps(caps, sizeof(caps), time);
          if (state == 3 || state == 4)
            std::snprintf(goalText, sizeof(goalText), "%s", tr(STR_PET_DAILY_WAIT));
          else {
            petui::widget::duration(goalText, sizeof(goalText), uint64_t(m.todaySeconds));
            std::snprintf(time, sizeof(time), tr(STR_PET_DAILY_TOTAL), goalText);
            std::snprintf(goalText, sizeof(goalText), "%s", time);
          }
          c.target.expectedDailyTime = caps;
          c.target.expectedDailyGoal = goalText;
          petui::Work w;
          c.paint(m, w);
          c.checkHits();
          CHECK(c.target.dailyGoalSeen && c.target.dailyTimeSeen);
          unsigned open = 0;
          for (size_t n = 0; n < c.interactions.count(); ++n) {
            const auto& action = c.interactions.data()[n];
            if (action.action == petui::OpenReadingInfo && action.value != 4) {
              ++open;
            }
          }
          CHECK(open >= 1 && open <= 2);
          if (orientation < 2 && goal == 1 && state < 3)
            c.write(std::string(out) + "/daily-" + (lang == Language::ES ? "es-" : "en-") + std::to_string(width) +
                    "-" + std::to_string(state) + ".pbm");
        }
      for (unsigned age : {0u, 1u}) {
        auto canvas = std::make_unique<Canvas>(width, height);
        auto& c = *canvas;
        c.device.orientation = fui::Orientation(orientation);
        c.target.view = "home-incubation";
        pet::Profile p;
        p.created = true;
        p.readingSeconds = pet::stageStart(pet::Stage(age));
        std::strcpy(p.name, "LUNA");
        p.owned = 0xFFF;
        p.equipped[0] = 0;
        p.equipped[1] = 3;
        p.equipped[2] = 6;
        p.equipped[3] = 9;
        petui::Model m;
        m.profile = &p;
        m.view = pet::View::Home;
        m.tab = pet::Tab::Home;
        if (withArt) {
          m.art = {&graphics, ArtFixture::draw};
          m.habitat = {&habitat, ArtFixture::draw};
        }
        petui::Work w;
        c.paint(m, w);
        c.checkHits();
        unsigned opens = 0;
        for (size_t n = 0; n < c.interactions.count(); ++n)
          if (c.interactions.data()[n].action == petui::OpenProjects) ++opens;
        CHECK(opens == (age ? 5u : 0u));
        if (orientation < 2)
          c.write(std::string(out) + "/home-lock-" + (lang == Language::ES ? "es-" : "en-") + std::to_string(width) +
                  "-" + std::to_string(age) + ".pbm");
        m.view = pet::View::Projects;
        c.paint(m, w);
        c.checkHits();
        bool back = false;
        for (size_t n = 0; n < c.interactions.count(); ++n) {
          const auto& hit = c.interactions.data()[n];
          if (!age) CHECK(hit.action != petui::OpenProjectPreview && hit.action != petui::SelectCategory);
          if (hit.action == petui::Back) back = true;
        }
        CHECK(back);
      }
      for (unsigned species = 0; species < 3; ++species) {
        auto canvas = std::make_unique<Canvas>(width, height);
        auto& c = *canvas;
        c.device.orientation = fui::Orientation(orientation);
        c.target.view = "dark-home";
        pet::Profile p;
        p.created = true;
        p.readingSeconds = 72000;
        p.lifeStatus = pet::LifeStatus::Lost;
        p.species = pet::Species(species);
        std::strcpy(p.name, "LUNA");
        petui::Model m;
        m.profile = &p;
        m.view = pet::View::Home;
        m.tab = pet::Tab::Home;
        struct Capture {
          ArtFixture* art;
          Target* target;
        } capture{&farewell, &c.target};
        if (withArt) {
          m.habitat = {&habitat, ArtFixture::draw};
          m.farewellArt = {&capture, [](void* ctx, fui::DrawTarget& target, fui::Rect r, uint32_t id, bool inverse) {
                             auto& data = *static_cast<Capture*>(ctx);
                             data.target->traceBitmap = true;
                             const bool drawn = ArtFixture::draw(data.art, target, r, id, inverse);
                             data.target->traceBitmap = false;
                             return drawn;
                           }};
        }
        petui::Work w;
        c.target.watchDarkCard = true;
        for (auto view : {pet::View::Home, pet::View::Dashboard}) {
          m.view = view;
          m.tab = view == pet::View::Home ? pet::Tab::Home : pet::Tab::Pet;
          c.target.darkCard = {};
          c.target.tracedColors.clear();
          c.paint(m, w);
          c.checkHits();
          // Only the room/hero is dark. Safe-area margins and the rest of the screen stay white.
          CHECK(c.pixels.back() == 255);
          const auto card = c.target.darkCard;
          CHECK(!card.empty() && card.width < width && card.height < height);
          if (!card.empty()) {
            const int x = card.x + 5, y = card.y + 5;
            CHECK(!(c.pixels[y * ((width + 7) / 8) + x / 8] & (0x80 >> (x % 8))));
          }
          if (withArt)
            CHECK(c.target.tracedColors.size() == 2 && c.target.tracedColors[0] == fui::Color::White &&
                  c.target.tracedColors[1] == fui::Color::Black);
          if (orientation < 2 && species == 0)
            c.write(std::string(out) + "/dark-card-" + (lang == Language::ES ? "es-" : "en-") + std::to_string(width) +
                    "-" + std::to_string(unsigned(view)) + ".pbm");
        }
        p.lifeStatus = pet::LifeStatus::Active;
        c.target.darkCard = {};
        c.paint(m, w);
        CHECK(c.pixels.back() == 255 && c.target.darkCard.empty());
      }
    }
  }
  for (auto lang : {Language::ES, Language::EN}) {
    I18N.setLanguage(lang);
    for (unsigned orientation = 0; orientation < 4; ++orientation) {
      const int width = orientation % 2 ? 800 : 480, height = orientation % 2 ? 480 : 800;
      for (unsigned count : {239u, 240u}) {
        auto c = std::make_unique<Canvas>(width, height);
        c->device.orientation = fui::Orientation(orientation);
        c->target.view = "collection";
        pet::Profile p;
        p.created = true;
        p.readingSeconds = 72000;
        for (unsigned i = 0; i < count; ++i) pet::own(p, i);
        petui::Model m;
        m.profile = &p;
        if (withArt) {
          m.art = {&graphics, ArtFixture::draw};
          m.habitat = {&habitat, ArtFixture::draw};
        }
        petui::Work w;
        for (auto view : {pet::View::Dashboard, pet::View::Home}) {
          m.view = view;
          m.tab = view == pet::View::Home ? pet::Tab::Home : pet::Tab::Pet;
          c->paint(m, w);
          c->checkHits();
          unsigned trophies = 0;
          for (size_t n = 0; n < c->interactions.count(); ++n)
            if (c->interactions.data()[n].action == petui::OpenCollection) ++trophies;
          CHECK(trophies == (count == 240 && view == pet::View::Dashboard ? 1u : 0u));
          if (count == 240 && view == pet::View::Home) {
            p.equipped[0] = uint8_t(pet::objectAt(0, 0));
            c->paint(m, w);
            c->checkHits();
            unsigned furnishedTrophies = 0;
            for (size_t n = 0; n < c->interactions.count(); ++n)
              if (c->interactions.data()[n].action == petui::OpenCollection) ++furnishedTrophies;
            CHECK(furnishedTrophies == 1);
            p.equipped[0] = 255;
          }
          if (count == 240 && orientation < 2)
            c->write(std::string(out) + "/trophy-" + (lang == Language::ES ? "es-" : "en-") + std::to_string(width) +
                     "-" + std::to_string(unsigned(view)) + ".pbm");
        }
        if (count != 240) continue;
        char copy[160];
        std::snprintf(copy, sizeof(copy), tr(STR_PET_COLLECTION_TEXT), pet::OBJECT_COUNT);
        c->target.expectedFullText = copy;
        m.view = pet::View::Collection;
        if (withArt) m.collectionArt = {&collection, ArtFixture::draw};
        c->paint(m, w);
        c->checkHits();
        unsigned backs = 0;
        for (size_t n = 0; n < c->interactions.count(); ++n)
          if (c->interactions.data()[n].action == petui::Back) ++backs;
        CHECK(backs >= 1 && pet::ownedCount(p) == 240);
        if (orientation < 2)
          c->write(std::string(out) + "/collection-" + (lang == Language::ES ? "es-" : "en-") + std::to_string(width) +
                   ".pbm");
      }
      // Every goal name must fit the compact overview at actual firmware font metrics.
      auto c = std::make_unique<Canvas>(width, height);
      c->device.orientation = fui::Orientation(orientation);
      c->target.view = "compact-objective-names";
      pet::Profile p;
      p.created = true;
      p.readingSeconds = 72000;
      petui::Model m;
      m.profile = &p;
      m.view = pet::View::Dashboard;
      petui::Work w;
      for (unsigned i = 0; i < pet::OBJECT_COUNT; ++i) {
        p.project = uint8_t(i);
        c->target.expectedFullText = petui::objectName(i);
        c->paint(m, w);
      }
    }
  }
#endif
  // The actual renderers must select the same full-body record, not merely similar poses.
  for (unsigned species = 0; species < 3; ++species)
    for (unsigned age = 1; age <= 4; ++age)
      for (unsigned state = 0; state < 7; ++state) {
        auto c = std::make_unique<Canvas>(480, 800);
        pet::Profile p;
        p.created = true;
        p.species = pet::Species(species);
        p.readingSeconds = pet::stageStart(pet::Stage(age));
        if (state == 1) p.joy = 90;
        if (state == 2) p.saciety = 10;
        if (state == 3) p.energy = 10;
        if (state == 4) p.illness = pet::Illness::Cold;
        if (state >= 5) p.resting = true;
        if (state == 6) p.equipped[0] = 2;
        unsigned record = UINT32_MAX;
        petui::Model m;
        m.profile = &p;
        m.sleepStep = 2;
        m.sleepArt = {&record, [](void* context, fui::DrawTarget&, fui::Rect, uint32_t id, bool) {
                        *static_cast<unsigned*>(context) = id;
                        return true;
                      }};
        petui::Work w;
        m.view = pet::View::Dashboard;
        c->paint(m, w);
        const unsigned full = record;
        CHECK(full == petsleep::fullBodyIndex(p, 2) + 1);
        m.view = pet::View::Home;
        m.tab = pet::Tab::Home;
        c->paint(m, w);
        CHECK(record == (state == 6 ? petsleep::headIndex(p) + 1 : full));
      }
  for (auto lang : {Language::ES, Language::EN}) {
    I18N.setLanguage(lang);
    for (unsigned orientation = 0; orientation < 4; ++orientation) {
      const int width = orientation % 2 ? 800 : 480, height = orientation % 2 ? 480 : 800;
      for (unsigned species = 0; species < 3; ++species)
        for (unsigned age = 1; age <= 4; ++age)
          for (unsigned state = 0; state < 8; ++state) {
            auto c = std::make_unique<Canvas>(width, height);
            c->device.orientation = fui::Orientation(orientation);
            c->target.view = "affection-reaction";
            pet::Profile p;
            p.created = true;
            p.species = pet::Species(species);
            p.readingSeconds = pet::stageStart(pet::Stage(age));
            p.joy = state == 0 ? 30 : state == 7 ? 100 : 60;
            if (state == 1) p.saciety = 10;
            if (state == 2) p.energy = 10;
            if (state == 3) p.resting = true;
            if (state == 4) p.illness = pet::Illness::Cold;
            if (state == 5) p.hygiene = 10;
            if (state == 6) p.lastPetDay = 20001;
            petui::Model m;
            m.profile = &p;
            m.view = pet::View::Affection;
            m.affection = pet::affection(p, 20001);
            if (withArt) {
              m.art = {&graphics, ArtFixture::draw};
              m.scenes = {&scenes, ArtFixture::draw};
              m.habitat = {&habitat, ArtFixture::draw};
              m.sleepArt = {&breathing, ArtFixture::draw};
            }
            petui::Work w;
            const uint8_t after = p.joy;
            c->paint(m, w);
            c->checkHits();
            CHECK(after == p.joy);
            char delta[64];
            std::snprintf(delta, sizeof(delta), tr(STR_PET_AFFECTION_RECEIPT), unsigned(m.affection.before),
                          unsigned(m.affection.after), unsigned(m.affection.after - m.affection.before));
            c->target.expectedFullText = delta;
            c->paint(m, w);
            CHECK(c->interactions.count() >= 1);
            if (lang == Language::ES && orientation < 2 && age == 3)
              c->write(std::string(out) + "/affection-" + std::to_string(width) + "-" + std::to_string(species) + "-" +
                       std::to_string(state) + ".pbm");
          }
    }
  }
  // ALBUM_CHECKS_BEGIN
  for (unsigned orientation = 0; orientation < 4; ++orientation)
    for (unsigned species = 0; species < 3; ++species)
      for (unsigned age = 1; age <= 4; ++age)
        for (unsigned pose = 0; pose < 6; ++pose) {
          auto c = std::make_unique<Canvas>(orientation % 2 ? 800 : 480, orientation % 2 ? 480 : 800);
          c->device.orientation = fui::Orientation(orientation);
          pet::Profile p, before;
          p.created = true;
          p.species = pet::Species(species);
          p.readingSeconds = pet::stageStart(pet::Stage(age));
          p.joy = pose == 1 ? 84 : 70;
          if (pose == 2) p.saciety = 10;
          if (pose == 3) p.energy = 10;
          if (pose == 4) p.illness = pet::Illness::Cold;
          if (pose == 5) p.resting = true;
          pet::captureMoments(p, before, 20000);
          const unsigned id = petui::albumwidget::memoryAt(p, 0);
          CHECK(unsigned(pet::momentPose(p.moments[id])) == pose);
          struct Trace {
            unsigned ids[2]{}, count = 0;
          } trace;
          petui::Model m;
          m.profile = &p;
          m.habitat = {nullptr, [](void*, fui::DrawTarget&, fui::Rect, uint32_t, bool) { return true; }};
          m.sleepArt = {&trace, [](void* context, fui::DrawTarget&, fui::Rect, uint32_t art, bool) {
                          auto& t = *static_cast<Trace*>(context);
                          CHECK(t.count < 2);
                          if (t.count < 2) t.ids[t.count++] = art;
                          return true;
                        }};
          const auto area = orientation % 2 ? fui::makeRect(28, 298, 276, 64) : fui::makeRect(28, 306, 426, 136);
          const auto saved = p;
          for (bool fullWidth : {false, true}) {
            trace = {};
            const auto clip = c->target.clipRect();
            petui::albumwidget::vignette(c->screen, m, area, id, fullWidth);
            const unsigned expected = pose == 5
                                          ? ((species * 4 + age - 1) * petsleep::FRAMES) * 2
                                          : petsleep::bodyIndex(p.species, pet::Stage(age), petsleep::Pose(pose), 0);
            CHECK(trace.count == 2 && trace.ids[0] == expected && trace.ids[1] == expected + 1);
            const auto restored = c->target.clipRect();
            CHECK(clip.x == restored.x && clip.y == restored.y && clip.width == restored.width &&
                  clip.height == restored.height);
            CHECK(!std::memcmp(&p, &saved, sizeof(p)));
          }
        }
  for (auto lang : {Language::ES, Language::EN}) {
    I18N.setLanguage(lang);
    for (unsigned orientation = 0; orientation < 4; ++orientation) {
      const int width = orientation % 2 ? 800 : 480, height = orientation % 2 ? 480 : 800;
      for (unsigned species = 0; species < 3; ++species)
        for (unsigned age = 0; age < 5; ++age) {
          auto c = std::make_unique<Canvas>(width, height);
          c->device.orientation = fui::Orientation(orientation);
          c->target.view = "album-redesign";
          pet::Profile p;
          p.created = true;
          p.species = pet::Species(species);
          p.readingSeconds = pet::stageStart(pet::Stage(age));
          p.memories = age ? 31 : 0;
          p.bornDay = readingstats::dayIndex({2026, 10, 23});
          std::strcpy(p.name, "LUNA");
          p.sharedDays = 1;
          p.streak = 1;
          petui::Model m;
          m.profile = &p;
          m.view = pet::View::Album;
          m.tab = pet::Tab::Album;
          m.today = p.bornDay;
          m.todaySeconds = 1800;
          m.weekDays = 1;
          struct ArtTrace {
            ArtFixture* pack;
            std::vector<unsigned> ids;
          } trace{&graphics, {}};
          if (withArt)
            m.art = {&trace, [](void* context, fui::DrawTarget& target, fui::Rect r, uint32_t id, bool inverse) {
                       auto& data = *static_cast<ArtTrace*>(context);
                       data.ids.push_back(id);
                       return ArtFixture::draw(data.pack, target, r, id, inverse);
                     }};
          petui::Work w;
          c->target.expectedEmphasizedText = petui::stageName(pet::Stage(age));
          c->target.expectedRegularText = petui::stageName(age ? pet::Stage::Egg : pet::Stage::Adult);
          char minutes[32], minutesCaps[32];
          if (age < 4) {
            const auto remaining = pet::stageStart(pet::Stage::Adult) - p.readingSeconds;
            std::snprintf(minutes, sizeof(minutes), tr(STR_PET_STAGE_MINUTES),
                          static_cast<unsigned long long>((remaining + 59) / 60));
            pixelui::copyCaps(minutesCaps, sizeof(minutesCaps), minutes);
            c->target.expectedFullText = minutesCaps;
            c->target.expectedFullTextSeen = false;
          }
          const auto saved = p;
          c->paint(m, w);
          c->checkHits();
          CHECK(c->target.emphasizedTextSeen && c->target.regularTextSeen);
          CHECK(!std::memcmp(&p, &saved, sizeof(p)));
          if (age < 4) CHECK(c->target.expectedFullTextSeen);
          c->target.expectedFullText = nullptr;
          c->target.expectedEmphasizedText = c->target.expectedRegularText = nullptr;
          unsigned memories = 0, lives = 0, reading = 0, accumulated = 0;
          fui::Rect memoryRow{}, livesRow{};
          for (size_t i = 0; i < c->interactions.count(); ++i) {
            const auto& hit = c->interactions.data()[i];
            memories += hit.action == petui::OpenMemories;
            lives += hit.action == petui::OpenLives;
            reading += hit.action == petui::OpenReadingInfo;
            accumulated += hit.action == petui::OpenAccumulatedHelp;
            CHECK(hit.action != petui::OpenMemory);
            if (hit.action == petui::OpenMemories) memoryRow = hit.rect;
            if (hit.action == petui::OpenLives) livesRow = hit.rect;
          }
          CHECK(memories == 1 && lives == 1 && reading == 1 && accumulated == 1);
          CHECK(memoryRow.bottom() < livesRow.y && memoryRow.x == livesRow.x && memoryRow.width == livesRow.width);
          for (auto id : trace.ids)
            if (id >= 3 && id < 51) CHECK((id - 3) % 16 / 4 + 1 <= age);
          if (orientation < 2 && species == 1 && (age == 0 || age == 1 || age == 4))
            c->write(std::string(out) + "/album-" + (lang == Language::ES ? "es-" : "en-") + std::to_string(width) +
                     "-" + std::to_string(age) + ".pbm");
        }
      auto c = std::make_unique<Canvas>(width, height);
      c->device.orientation = fui::Orientation(orientation);
      pet::Profile p;
      p.created = true;
      p.readingSeconds = 7200;
      p.memories = 31;
      p.bornDay = readingstats::dayIndex({2026, 10, 23});
      p.mode = pet::Mode::Normal;
      petui::Model m;
      m.profile = &p;
      m.today = p.bornDay;
      m.todaySeconds = 1800;
      if (withArt) m.art = {&graphics, ArtFixture::draw};
      petui::Work w;
      for (auto view : {pet::View::Memories, pet::View::Memory}) {
        m.view = view;
        m.tab = pet::Tab::Album;
        c->target.view = "album-subpages";
        const int cases = view == pet::View::Memory ? int(petui::MEMORY_COUNT) : 3;
        for (int i = 0; i < cases; ++i) {
          m.page = std::min(i % 3, std::max(0, (int(petui::memoryOwnedCount(p)) - 1) / (width > height ? 3 : 4)));
          m.memory = i;
          c->target.expectedRegularText = petui::memoryTitle(
              view == pet::View::Memory ? unsigned(m.memory)
                                        : petui::albumwidget::memoryAt(p, unsigned(m.page * (width > height ? 3 : 4))));
          c->target.expectedFullText = c->target.expectedRegularText;
          c->target.regularTextSeen = false;
          c->paint(m, w);
          c->checkHits();
          CHECK(c->target.regularTextSeen);
          c->target.expectedRegularText = c->target.expectedFullText = nullptr;
          if (orientation < 2 && i == 0)
            c->write(std::string(out) + "/album-detail-" + (lang == Language::ES ? "es-" : "en-") +
                     std::to_string(width) + "-" + std::to_string(unsigned(view)) + ".pbm");
        }
      }
      for (unsigned flags : {0u, 4u, 8u, 16u, 31u}) {
        p.memories = uint16_t(flags);
        m.view = pet::View::Memories;
        for (m.page = 0; m.page < 3; ++m.page) {
          c->paint(m, w);
          c->checkHits();
          for (size_t i = 0; i < c->interactions.count(); ++i) {
            const auto& hit = c->interactions.data()[i];
            if (hit.action == petui::OpenMemory) CHECK(hit.value >= 0 && unsigned(hit.value) < petui::MEMORY_COUNT);
          }
        }
      }
      p.memories = 31;
      for (unsigned mode = 0; mode < 4; ++mode)
        for (unsigned state = 0; state < 5; ++state) {
          p.mode = pet::Mode(mode);
          p.lastReadAt = 1800000000;
          m.now = p.lastReadAt + 3600;
          m.clockKnown = true;
          m.historyChanged = false;
          if (state == 1) m.now = p.lastReadAt + pet::absenceGrace(p.mode) - 60;
          if (state == 2) m.now = p.lastReadAt + pet::absenceGrace(p.mode);
          if (state == 3) m.clockKnown = false;
          if (state == 4) m.historyChanged = true;
          char line[160], caps[160];
          petui::widget::readingTime(m, line, sizeof(line));
          pixelui::copyCaps(caps, sizeof(caps), line);
          c->target.expectedFullText = caps;
          c->target.expectedFullTextSeen = false;
          m.view = pet::View::Dashboard;
          c->target.view = "reading-life-dashboard";
          c->paint(m, w);
          c->checkHits();
          CHECK(c->target.expectedFullTextSeen == petui::widget::readingUrgent(m));
          m.view = pet::View::Home;
          m.tab = pet::Tab::Home;
          c->target.view = "reading-life-home";
          c->target.expectedFullTextSeen = false;
          c->paint(m, w);
          c->checkHits();
          CHECK(!c->target.expectedFullTextSeen);
          c->target.expectedFullText = nullptr;
          m.view = pet::View::ReadingInfo;
          c->target.view = "reading-life-help";
          c->paint(m, w);
          c->checkHits();
          if (orientation < 2 && state == 0)
            c->write(std::string(out) + "/reading-help-" + (lang == Language::ES ? "es-" : "en-") +
                     std::to_string(width) + "-" + std::to_string(mode) + ".pbm");
        }
    }
  }
  // ALBUM_CHECKS_END
  std::printf("PET screens: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
