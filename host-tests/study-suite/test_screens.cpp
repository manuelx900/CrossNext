#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../../assets_local/quest/encoded/QuestArt.h"
#include "../../src/apps_local/chess/ChessStyle.h"
#include "../../src/apps_local/intercept/InterceptScreens.h"
#include "../../src/apps_local/quest/QuestArtMeta.h"
#include "../../src/apps_local/quest/QuestAvatar.h"
#include "../../src/apps_local/quest/QuestCampaignStory.h"
#include "../../src/apps_local/quest/QuestEnemyArt.h"
#include "../../src/apps_local/quest/QuestJourney.h"
#include "../../src/apps_local/quest/QuestJourneyData.h"
#include "../../src/apps_local/quest/QuestJourneyScreens.h"
#include "../../src/apps_local/quest/QuestMilestones.h"
#include "../../src/apps_local/quest/QuestReadingReturnArt.h"
#include "../../src/apps_local/quest/QuestRequestData.h"
#include "../../src/apps_local/quest/QuestRequestUi.h"
#include "../../src/apps_local/quest/QuestRequests.h"
#include "../../src/apps_local/quest/QuestSceneData.h"
#include "../../src/apps_local/quest/QuestScenePlan.h"
#include "../../src/apps_local/quest/QuestScreens.h"
#include "../../src/apps_local/tools/ToolsScreens.h"
#include "../../src/apps_local/ui/PixelMenu.h"
#include "../../src/util/InterfaceThemeStyle.h"
#include "../../tools_local/quest/legacy_request_fixture.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <builtinFonts/ubuntu_10_regular.h>
#include <builtinFonts/ubuntu_12_bold.h>
#include <builtinFonts/ubuntu_12_regular.h>

#include "../../src/apps_local/ui/fonts/toybox_10.h"
#include "../../src/apps_local/ui/fonts/toybox_14.h"
#include "../../src/apps_local/ui/fonts/toybox_20.h"
#include "../../src/apps_local/ui/fonts/toybox_30.h"
#include "../../src/apps_local/ui/fonts/ui_prose_10.h"
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
  d.safeArea = {10, 0, 0, 0};
  return d;
}
struct Canvas {
  int width, height;
  std::vector<uint8_t> pixels;
  FontBridge tiny{toybox_10}, small{toybox_14}, body{toybox_20}, title{toybox_30};
  fui::DisplayTarget target;
  fui::DeviceContext device;
  fui::InputSnapshot input{};
  toybox::Interactions interactions;
  toybox::Frame frame;
  toybox::Screen screen;

  Canvas(int w, int h, toybox::HeaderStatus status = {})
      : width(w),
        height(h),
        pixels((w + 7) / 8 * h, 255),
        target(pixels.data(), w, h, (w + 7) / 8, fui::Orientation::LandscapeCounterClockwise),
        device(makeDevice(target)),
        frame(target, device, input, interactions, nullptr, status),
        screen(frame) {
    target.setFont(questui::NARRATIVE_FONT, small.font);
    target.setFont(3, tiny.font);
    target.setFont(4, body.font);
    target.setFont(toybox::kSmallFont, small.font);
    target.setFont(toybox::kUiFont, body.font);
    target.setFont(toybox::kDisplayFont, title.font);
  }

  void checkHits() {
    CHECK(!interactions.overflowed());
    paintclock::notePainted();
    for (size_t n = 0; n < interactions.count(); ++n) {
      const auto& slot = interactions.data()[n];
      if (slot.inputMask & fui::InputCoordinates) continue;
      const auto& r = slot.rect;
      CHECK(r.x >= 0 && r.y >= 0 && r.right() <= width && r.bottom() <= height);

      if (r.width < 44 || r.height < 32)
        std::printf("small hit: %d:%d %d,%d %dx%d screen=%dx%d\n", int(slot.action), int(slot.value), r.x, r.y, r.width,
                    r.height, width, height);
      CHECK(r.width >= 44 && r.height >= 32);
      fui::InputSnapshot tap;
      tap.touchReleased = true;
      tap.touchX = static_cast<int16_t>(r.x + r.width / 2);
      tap.touchY = static_cast<int16_t>(r.y + r.height / 2);
      const auto event = interactions.route(tap);

      CHECK(event.action == slot.action && event.value == slot.value);
    }
  }

  bool has(fui::ActionId action, int value = 0) const {
    for (size_t n = 0; n < interactions.count(); ++n)
      if (interactions.data()[n].action == action && interactions.data()[n].value == value) return true;
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

struct NarrativeTrace final : fui::DrawTarget {
  fui::DrawTarget& inner;
  std::string prose;
  std::string chrome;
  bool battleProse = false;
  const char* watched[2]{};
  fui::Color watchedColor[2]{};
  bool watchedSeen[2]{};
  bool watchedFits[2]{};
  fui::FontId watchedFont[2]{};
  fui::Rect watchedBox[2]{};
  bool watchLast = false;
  int triangleCalls = 0;
  explicit NarrativeTrace(fui::DrawTarget& target) : inner(target) {
    prose.reserve(8192);
    chrome.reserve(2048);
  }
  fui::Rect clipRect() const override { return inner.clipRect(); }
  bool setClipRect(fui::Rect r) override { return inner.setClipRect(r); }
  fui::Size measureText(fui::FontId f, const char* s, fui::TextStyle st) const override {
    return inner.measureText(f, s, st);
  }
  int16_t lineHeight(fui::FontId f) const override { return inner.lineHeight(f); }
  void fill(fui::Rect r, fui::Paint p, uint8_t radius, uint8_t corners) override { inner.fill(r, p, radius, corners); }
  void stroke(fui::Rect r, fui::Paint p, uint8_t width, uint8_t radius, uint8_t corners) override {
    inner.stroke(r, p, width, radius, corners);
  }
  void line(fui::Point a, fui::Point b, uint8_t w, fui::Paint p) override { inner.line(a, b, w, p); }
  void triangle(fui::Point a, fui::Point b, fui::Point c, fui::Paint p) override {
    ++triangleCalls;
    inner.triangle(a, b, c, p);
  }
  void bitmap(fui::Rect r, fui::BitmapRef b, fui::BitmapMode m, fui::Paint p, fui::Rotation rot) override {
    inner.bitmap(r, b, m, p, rot);
  }
  void text(fui::Rect r, const char* value, fui::TextStyle style) override {
    for (int n = 0; n < 2; ++n)
      if ((!watchedSeen[n] || watchLast) && watched[n] && std::strcmp(watched[n], value) == 0) {
        watchedSeen[n] = true;
        watchedColor[n] = style.color;
        watchedFont[n] = style.font;
        watchedBox[n] = r;
        const auto lines = fui::textAreaWalk(inner, r.width, value, style, [](uint32_t, const fui::TextAreaLine&) {});
        watchedFits[n] = lines <= unsigned(std::max(1, int(style.maxLines))) &&
                         int(lines) * inner.lineHeight(style.font) <= r.height;
      }
    if ((style.font == questui::NARRATIVE_FONT && !style.bold && style.align == fui::TextAlign::Left &&
         r.height == inner.lineHeight(style.font)) ||
        (!battleProse && style.font == 4)) {
      prose += value;
      prose += ' ';
    } else {
      chrome += value;
      chrome += ' ';
    }
    inner.text(r, value, style);
  }
};
std::string withoutWhitespace(const std::string& value) {
  std::string result;
  result.reserve(value.size());
  for (unsigned char c : value)
    if (!std::isspace(c)) result += char(c);
  return result;
}
std::string expectedPrologue() {
  std::string expected;
  for (const auto id : queststory::PROLOGUE_TEXTS) expected += I18N.get(id);
  return expected;
}
void checkNarrativePages(const char* directory = nullptr) {
  for (auto language : {Language::EN, Language::ES}) {
    I18N.setLanguage(language);
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      for (int story : {-2, -1, 0, 1, 2, 3, 4, 5, 719}) {
        for (bool recapping : {false, true}) {
          if (story < 0 && recapping) continue;
          quest::Profile hero;
          std::snprintf(hero.name, sizeof(hero.name), "ÁÁÁÁÁÁÁÁÁÁÁÁ");
          hero.campaign.encounter = story < 0 ? 0 : story;
          hero.campaign.prologueSeen = story != -2;
          hero.campaign.introducedBooks = story == -1 ? 0 : 12;
          hero.campaign.narratedEncounters = hero.campaign.encounter + unsigned(recapping);
          readingstats::Summary summary;
          questui::Model model;
          model.profile = &hero;
          model.summary = &summary;
          model.view = story == -2 ? questui::View::Campaign : story == -1 ? questui::View::Book : questui::View::Lore;
          model.selectedEncounter = hero.campaign.encounter;
          model.recalling = recapping;
          std::string actual, expected;
          actual.reserve(16384);
          expected.reserve(16384);
          if (story < 0) {
            expected = expectedPrologue();
            if (story == -1) expected += I18N.get(questscene::SAGA[0][0]);
          } else {
            const unsigned chapter = unsigned(story) / 6;
            expected += I18N.get(questscene::APPROACH[chapter]);
          }
          int pages = 1;
          for (int page = 0; page < pages; ++page) {
            model.introductionPage = page;
            Canvas canvas(size.first, size.second);
            NarrativeTrace target(canvas.target);
            target.battleProse = story >= 0;
            toybox::Frame frame(target, canvas.device, canvas.input, canvas.interactions);
            toybox::Screen screen(frame);
            questui::Work work;
            questui::build(screen, model, work);
            pages = work.pages;
            CHECK(pages >= 1 && pages < 96 && work.page == page);
            const bool starts = story >= 0 && !recapping && unsigned(story) == quest::battleAnchor(unsigned(story) / 6);
            const auto finish = story == -2   ? questui::FinishPrologue
                                : story == -1 ? questui::CampaignSection
                                : starts      ? questui::BeginEncounter
                                              : questui::ReturnPath;
            CHECK(canvas.has(finish, starts ? story : 0) == (page + 1 == pages));
            canvas.checkHits();
            actual += target.prose;
            if (directory && language == Language::ES && !recapping)
              canvas.write(std::string(directory) + "/story-" + std::to_string(story) + "-" +
                           std::to_string(size.first) + "-" + std::to_string(page) + ".pbm");
          }
          CHECK(withoutWhitespace(actual) == withoutWhitespace(expected));
        }
      }
    }
  }
}
}  // namespace
void checkJourneyPages() {
  for (auto language : {Language::EN, Language::ES}) {
    I18N.setLanguage(language);
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      quest::Profile hero;
      readingstats::Summary summary;
      hero.campaign.initialized = true;
      hero.campaign.prologueSeen = true;
      hero.campaign.introducedBooks = 12;
      hero.campaign.narratedEncounters = 720;
      hero.campaign.encounter = 719;
      hero.campaign.phase = quest::BattlePhase::Complete;
      questui::Model model;
      model.profile = &hero;
      model.summary = &summary;
      model.today = 20000;
      for (int book = 0; book < 12; ++book) {
        model.selectedBook = book;
        model.view = questui::View::AtlasRegion;
        int pages = 1, total = 0;
        for (int page = 0; page < pages; ++page) {
          Canvas map(size.first, size.second);
          questui::Work work;
          model.page = page;
          questui::build(map.screen, model, work);
          map.checkHits();
          pages = work.pages;
          for (int n = 0; n < 10; ++n)
            if (map.has(questui::AtlasChapter, book * 10 + n)) ++total;
        }
        CHECK(total == 10);
      }
      for (int chapter = 0; chapter < 120; ++chapter)
        for (int beat = 0; beat < 4; ++beat) {
          hero.campaign.encounter = chapter * 6 + (beat == 0 ? 0 : beat == 1 ? 2 : 4);
          hero.campaign.phase = beat == 3 ? quest::BattlePhase::Complete : quest::BattlePhase::Reading;
          if (beat == 3) {
            hero.campaign.phase = quest::BattlePhase::Reading;
            hero.campaign.encounter = chapter == 119 ? 719 : (chapter + 1) * 6;
            hero.campaign.narratedEncounters = hero.campaign.encounter;
            if (chapter == 119) hero.campaign.phase = quest::BattlePhase::Complete;
          } else
            hero.campaign.narratedEncounters = hero.campaign.encounter + 1;
          model.view = questui::View::Recap;
          std::string actual;
          actual.reserve(1024);
          int pages = 1;
          for (int page = 0; page < pages; ++page) {
            model.introductionPage = page;
            Canvas canvas(size.first, size.second);
            questui::Work work;
            NarrativeTrace target(canvas.target);
            toybox::Frame frame(target, canvas.device, canvas.input, canvas.interactions);
            toybox::Screen screen(frame);
            questui::build(screen, model, work);
            pages = work.pages;
            CHECK(pages > 0 && pages < 20 && work.page == page);
            canvas.checkHits();
            actual += target.prose;
            CHECK(canvas.has(questui::RecapContinue, 1) == (page + 1 == pages));
          }
          const auto context = questjourney::context(hero);
          CHECK(withoutWhitespace(actual) == withoutWhitespace(context.beat
                                                                   ? tr(STR_QUEST_CAM_ORDER_CLEAR_PATH)
                                                                   : I18N.get(questscene::APPROACH[context.chapter])));
        }
      for (int book : {0, 6, 11})
        for (int step = 0; step < 6; ++step)
          for (auto phase : {quest::BattlePhase::Reading, quest::BattlePhase::Victory, quest::BattlePhase::Story}) {
            hero.campaign.encounter = book * 60 + step;
            hero.campaign.narratedEncounters = hero.campaign.encounter + 1;
            hero.campaign.phase = phase;
            const auto context = questjourney::context(hero);
            const unsigned chapter = context.chapter;
            model.view = questui::View::Lore;
            model.recalling = true;
            model.selectedEncounter = questjourney::recallEncounter(context);
            std::string expected, actual;
            expected = context.beat ? tr(STR_QUEST_CAM_ORDER_CLEAR_PATH) : I18N.get(questscene::APPROACH[chapter]);
            int pages = 1;
            for (int page = 0; page < pages; ++page) {
              model.introductionPage = page;
              Canvas canvas(size.first, size.second);
              questui::Work work;
              NarrativeTrace target(canvas.target);
              target.battleProse = !context.beat;
              toybox::Frame frame(target, canvas.device, canvas.input, canvas.interactions);
              toybox::Screen screen(frame);
              questui::build(screen, model, work);
              pages = work.pages;
              CHECK(pages > 0 && pages < 96);
              canvas.checkHits();
              CHECK(!canvas.has(questui::BeginEncounter));
              CHECK(!canvas.has(questui::EncounterInfo));
              CHECK(canvas.has(questui::ReturnPath) == (page + 1 == pages));
              if (context.beat == 0)
                CHECK(target.chrome.find(I18N.get(queststory::CHAPTERS[chapter])) != std::string::npos);
              actual += target.prose;
            }
            CHECK(withoutWhitespace(actual) == withoutWhitespace(expected));
          }
      model.recalling = false;
      for (int id = 0; id < quest::kRequestCount; ++id) {
        if (id != quest::huntForPath(quest::requestChapter(id))) continue;
        hero.campaign.encounter = quest::battleAnchor(119);
        hero.campaign.phase = quest::BattlePhase::Complete;
        hero.campaign.introducedBooks = 12;
        model.view = questui::View::Request;
        model.selectedRequest = id;
        model.introductionPage = 0;
        Canvas card(size.first, size.second);
        NarrativeTrace trace(card.target);
        trace.watched[0] = questui::huntTitle(id);
        toybox::Frame frame(trace, card.device, card.input, card.interactions);
        toybox::Screen screen(frame);
        questui::Work work;
        questui::build(screen, model, work);
        card.checkHits();
        CHECK(trace.watchedSeen[0] && trace.watchedFits[0]);
        CHECK(card.has(questui::AcceptRequest, id));
        hero.requestClaims[id / 8] |= 1u << (id % 8);
        Canvas history(size.first, size.second);
        questui::build(history.screen, model, work);
        history.checkHits();
        CHECK(history.has(questui::BackToRequests));
        CHECK(!history.has(questui::AcceptRequest) && !history.has(questui::CollectRequest));
      }
    }
  }
}
void checkCardFocus() {
  for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
    for (int kind = 0; kind < 4; ++kind) {
      quest::Profile hero;
      readingstats::Summary summary;
      hero.campaign.initialized = true;
      hero.campaign.prologueSeen = true;
      hero.campaign.introducedBooks = 12;
      hero.campaign.encounter = 719;
      hero.campaign.narratedEncounters = 720;
      hero.campaign.phase = quest::BattlePhase::Complete;
      if (kind == 3) hero.requestClaims[0] = 1;
      questui::Model model;
      questui::Work work;
      model.profile = &hero;
      model.summary = &summary;
      model.view = kind == 0 ? questui::View::Atlas : kind == 1 ? questui::View::AtlasRegion : questui::View::Requests;
      model.requestFilter = kind == 3 ? questui::RequestFilter::Collection : questui::RequestFilter::Available;
      const auto action = kind == 0 ? questui::AtlasBook : kind == 1 ? questui::AtlasChapter : questui::RequestInfo;
      const int first = kind >= 2 ? quest::requestAt(hero, kind == 3, 0) : 0;
      const char* title = kind == 0   ? I18N.get(queststory::BOOKS[0])
                          : kind == 1 ? questui::huntTitle(quest::huntForPath(0))
                                      : questui::huntTitle(first);
      Canvas canvas(size.first, size.second);
      questui::build(canvas.screen, model, work);
      fui::InputSnapshot input;
      input.focusNext = true;
      paintclock::notePainted();
      for (size_t n = 0; n <= canvas.interactions.count() && !canvas.frame.buttonSelected(action, first); ++n)
        canvas.interactions.route(input);
      CHECK(canvas.frame.buttonSelected(action, first));
      NarrativeTrace trace(canvas.target);
      trace.watched[0] = title;
      toybox::Frame frame(trace, canvas.device, canvas.input, canvas.interactions);
      toybox::Screen screen(frame);
      questui::build(screen, model, work);
      CHECK(trace.watchedSeen[0] && trace.watchedColor[0] == (kind >= 2 ? fui::Color::Black : fui::Color::White));
    }
  }
}
void checkMissionScreens(const char* directory = nullptr) {
  const bool oldLight = chrome::lightHeader;
  const int oldHeight = chrome::headerHeight;
  I18N.setLanguage(Language::ES);
  for (auto size : {std::pair<int, int>{480, 800}, {800, 480}})
    for (int home = 0; home < 4; ++home)
      for (int state = 0; state < 5; ++state) {
        quest::Profile hero;
        hero.campaign.initialized = true;
        hero.campaign.startedDay = 20000;
        hero.campaign.prologueSeen = state >= 2;
        hero.campaign.introducedBooks = state >= 2 ? 1 : 0;
        hero.campaign.encounter = state >= 2 ? 6 : 0;
        hero.campaign.narratedEncounters = state >= 2 ? 7 : 0;
        readingstats::Summary summary;
        summary.anchorDay = 20000;
        readingstats::Record reading;
        reading.localDay = 20000;
        reading.seconds = (state % 2 || state == 4) ? quest::WEEKLY_MINUTES * 60 : 480;
        summary.add(reading);
        questui::Model model;
        model.profile = &hero;
        model.summary = &summary;
        model.today = 20000;
        model.writable = state != 4;
        toybox::HeaderStatus status;
        std::snprintf(status.clock, sizeof(status.clock), "10:37");
        chrome::lightHeader = interfacetheme::light(interfacetheme::homeStyle(home));
        for (auto view : {questui::View::Missions, questui::View::Requests}) {
          model.view = view;
          Canvas canvas(size.first, size.second, status);
          questui::Work work;
          chrome::headerHeight = 99;
          questui::build(canvas.screen, model, work);
          canvas.checkHits();
          CHECK(chrome::headerHeight == 0);
          const auto black = [&](int x, int y) {
            return !(canvas.pixels[y * ((size.first + 7) / 8) + x / 8] & (0x80 >> (x % 8)));
          };
          CHECK(black(3, 3) == !chrome::lightHeader);
          CHECK(black(22, 44) == chrome::lightHeader);
          CHECK(!black(1, size.second / 2));
          if (view == questui::View::Missions) {
            CHECK(canvas.has(questui::OpenView, int(questui::View::Requests)));
            CHECK(!canvas.has(questui::Claim, int(quest::Mission::Story)));
            const bool ready = state % 2 && model.writable;
            CHECK(canvas.has(questui::Claim, int(quest::Mission::Daily)) == ready);
            CHECK(canvas.has(questui::Claim, int(quest::Mission::Weekly)) == ready);
            CHECK(!canvas.has(questui::RequestInfo));
          } else if (state < 2) {
            CHECK(!canvas.has(questui::RequestInfo));
            CHECK(!canvas.has(questui::Claim));
          } else {
            CHECK(canvas.has(questui::RequestInfo, quest::requestAt(hero, false, 0)));
          }
          if (directory && (home == 0 || home == 2) && state < 3)
            canvas.write(std::string(directory) + "/" + (view == questui::View::Missions ? "missions" : "requests") +
                         "-" + std::to_string(size.first) + "-" + std::to_string(home) + "-" + std::to_string(state) +
                         ".pbm");
        }
      }
  for (auto language : {Language::EN, Language::ES}) {
    I18N.setLanguage(language);
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      quest::Profile hero;
      hero.campaign.initialized = true;
      hero.campaign.prologueSeen = true;
      hero.campaign.introducedBooks = 2;
      hero.campaign.encounter = 120;
      hero.campaign.storyClaims[0] = 0xff;
      hero.campaign.storyClaims[1] = 0xff;
      hero.campaign.storyClaims[2] = 0x0f;
      CHECK(quest::missionChapter(hero) == 20);
      readingstats::Summary summary;
      questui::Model model;
      model.profile = &hero;
      model.summary = &summary;
      model.view = questui::View::Missions;
      Canvas canvas(size.first, size.second);
      NarrativeTrace words(canvas.target);
      toybox::Frame frame(words, canvas.device, canvas.input, canvas.interactions);
      toybox::Screen screen(frame);
      questui::Work work;
      questui::build(screen, model, work);
      CHECK(words.chrome.find(I18N.get(questjourney::PATRONS[2])) == std::string::npos);
      CHECK(words.chrome.find(I18N.get(questrequests::TEXTS[50].title)) == std::string::npos);
    }
  }
  chrome::lightHeader = oldLight;
  chrome::headerHeight = oldHeight;
}
void checkOrderScreens(const char* directory = nullptr) {
  I18N.setLanguage(Language::ES);
  sdart::Pack base, story, rewards, enemies;
  CHECK(base.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT));
  CHECK(story.open("resources/apps/reading-adventure/story.bin", questassets::STORY_COUNT));
  CHECK(rewards.open("resources/apps/reading-adventure/rewards.bin", questassets::REWARDS_COUNT));
  CHECK(enemies.open("resources/apps/reading-adventure/guardians.bin", questassets::GUARDIANS_COUNT));
  quest::Profile hero;
  hero.xp = quest::threshold(100);
  hero.campaign.initialized = hero.campaign.prologueSeen = true;
  hero.campaign.introducedBooks = 12;
  hero.campaign.narratedEncounters = 720;
  hero.campaign.encounter = 719;
  hero.campaign.phase = quest::BattlePhase::Complete;
  hero.campaign.state = quest::AdventureState::Peace;
  hero.campaign.observedSeconds = 10000;
  readingstats::Summary summary;
  summary.anchorDay = 20000;
  summary.seconds = 10000;
  summary.pages = 100;
  summary.completed = 2;
  questui::Model model;
  model.profile = &hero;
  model.summary = &summary;
  model.today = 20000;
  model.assets = &base;
  model.storyAssets = &story;
  model.rewardAssets = &rewards;
  model.enemyAssets = &enemies;
  for (auto language : {Language::ES, Language::EN}) {
    I18N.setLanguage(language);
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      const auto render = [&](const char* name) {
        Canvas canvas(size.first, size.second);
        questui::Work work;
        questui::build(canvas.screen, model, work);
        canvas.checkHits();
        if (directory && language == Language::ES)
          canvas.write(std::string(directory) + "/" + name + "-" + std::to_string(size.first) + ".pbm");
        return work.pages;
      };
      model.view = questui::View::Request;
      for (int id = 0; id < quest::kLegacyRequestCount; ++id) {
        model.selectedRequest = id;
        model.introductionPage = 0;
        CHECK(render("request-last") == 1);
      }
      {
        auto sessionProfile = hero;
        CHECK(questfixture::activateLegacyRequest(sessionProfile, 2, summary));
        summary.sessions++;
        model.profile = &sessionProfile;
        model.view = questui::View::Requests;
        model.page = 0;
        Canvas canvas(size.first, size.second);
        NarrativeTrace trace(canvas.target);
        char label[64];
        std::snprintf(label, sizeof(label), tr(STR_QUEST_CAM_ORDER_PROGRESS_SHORT_SESSIONS), 1u,
                      quest::requestTarget(sessionProfile, 2));
        trace.watched[0] = questui::huntTitle(2);
        trace.watched[1] = tr(STR_QUEST_CAM_REQUEST_ACTIVE);
        toybox::Frame frame(trace, canvas.device, canvas.input, canvas.interactions);
        toybox::Screen screen(frame);
        questui::Work work;
        questui::build(screen, model, work);
        canvas.checkHits();
        // Wrapped labels reach the target as individual, intact line runs.
        CHECK((trace.watchedSeen[0] && trace.watchedFits[0]) ||
              withoutWhitespace(trace.chrome).find(withoutWhitespace(trace.watched[0])) != std::string::npos);
        CHECK(trace.watchedSeen[1] && trace.watchedFits[1]);
        if (directory && language == Language::ES)
          canvas.write(std::string(directory) + "/requests-sessions-" + std::to_string(size.first) + ".pbm");
        summary.sessions--;
        model.profile = &hero;
        model.view = questui::View::Request;
      }
      model.selectedRequest = 6;
      render("request-available");
      CHECK(questfixture::activateLegacyRequest(hero, 7, summary));
      model.selectedRequest = 7;
      render("request-active");
      model.view = questui::View::Requests;
      model.page = 0;
      CHECK(quest::requestAt(hero, false, 0) == 7);
      render("requests-active-first");
      const auto before = hero;
      summary.completed++;
      auto report = questreturn::capture(before, hero);
      CHECK(questreturn::captureRequest(before, hero, summary, report) && report.requestId == 7);
      model.readingReturn = &report;
      model.returnPanel = questreturn::Panel::Request;
      model.view = questui::View::ReadingReturn;
      render("request-return");
      model.view = questui::View::Request;
      model.selectedRequest = 7;
      render("request-ready");
      hero.activeRequest = quest::kRequestCount;
      hero.requestStarted = 0;
      hero.requestBaseline = 0;
      hero.requestReported = 0;
      hero.requestGoal = quest::RequestGoal::Minutes;
      model.view = questui::View::Gear;
      model.preview = 216;
      render("item-detail");
      for (int item = 0; item < quest::kGearCount; ++item) {
        if (!quest::availableItem(item)) continue;
        model.preview = item;
        Canvas canvas(size.first, size.second);
        NarrativeTrace trace(canvas.target);
        trace.watched[0] = tr(STR_QUEST_CAM_EXP_DETAIL_READING);
        trace.watched[1] = tr(STR_QUEST_CAM_EXP_DETAIL_LATER);
        toybox::Frame frame(trace, canvas.device, canvas.input, canvas.interactions);
        toybox::Screen screen(frame);
        questui::Work work;
        questui::build(screen, model, work);
        CHECK(trace.watchedSeen[0] && trace.watchedFits[0]);
        CHECK(trace.watchedSeen[1] && trace.watchedFits[1]);
      }
      model.preview = -1;
      for (unsigned level : {1u, 10u, 50u}) {
        hero.xp = quest::threshold(level);
        model.view = questui::View::Shop;
        model.category = 0;
        render(("shop-level-" + std::to_string(level)).c_str());
      }
      {
        auto purchased = hero;
        purchased.xp = quest::threshold(10);
        for (int n = 0; n < quest::count(quest::Slot::Decoration); ++n) {
          const auto item = quest::catalogItem(quest::Slot::Decoration, n);
          if (!quest::storyExclusive(item)) quest::grantItem(purchased, item);
        }
        model.profile = &purchased;
        model.category = int(quest::Slot::Decoration);
        Canvas canvas(size.first, size.second);
        NarrativeTrace trace(canvas.target);
        trace.watched[0] = tr(STR_QUEST_SHOP_EMPTY);
        toybox::Frame frame(trace, canvas.device, canvas.input, canvas.interactions);
        toybox::Screen screen(frame);
        questui::Work work;
        questui::build(screen, model, work);
        canvas.checkHits();
        CHECK(trace.watchedSeen[0] && trace.watchedFits[0]);
        CHECK(trace.chrome.find(tr(STR_QUEST_CAM_ORDER_SHOP_NEXT_LEVEL)) == std::string::npos);
        model.profile = &hero;
        model.category = 0;
      }
      hero.xp = quest::threshold(100);
      hero.campaign.encounter = 59;
      hero.campaign.phase = quest::BattlePhase::Story;
      hero.campaign.state = quest::AdventureState::Active;
      hero.campaign.introducedBooks = 1;
      hero.campaign.narratedEncounters = 60;
      model.view = questui::View::Campaign;
      model.campaignTab = questui::CampaignTab::Encounter;
      render("encounter-idle");
      model.view = questui::View::AtlasPlace;
      model.selectedEncounter = 59;
      render("atlas-outcome");
      hero.campaign.encounter = 719;
      hero.campaign.phase = quest::BattlePhase::Complete;
      hero.campaign.state = quest::AdventureState::Peace;
      hero.campaign.introducedBooks = 12;
      hero.campaign.narratedEncounters = 720;
      model.view = questui::View::AtlasRegion;
      model.selectedBook = 0;
      model.page = 0;
      render("atlas-region");
    }
  }
}
int main(int argc, char** argv) {
  if (argc > 2 && std::string(argv[1]) == "--beasts-menu-check") {
    sdart::Pack art;
    CHECK(art.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT));
    for (auto language : {Language::EN, Language::ES})
      for (auto size : {std::pair<int, int>{480, 800}, {800, 480}})
        for (int state = 0; state < 4; ++state) {
          I18N.setLanguage(language);
          quest::Profile hero;
          readingstats::Summary reading;
          hero.campaign.initialized = true;
          if (state) {
            hero.campaign.prologueSeen = true;
            hero.campaign.introducedBooks = 12;
            hero.campaign.encounter = quest::battleAnchor(state == 3 ? 119 : 2);
            hero.campaign.phase = quest::BattlePhase::Story;
          }
          if (state == 2) {
            hero.activeRequest = quest::kStormHunt;
            hero.requestTargetMinutes = 180;
          }
          if (state == 3) {
            hero.campaign.phase = quest::BattlePhase::Complete;
            hero.campaign.state = quest::AdventureState::Peace;
            for (int id = 0; id < quest::kRequestCount; ++id) hero.requestClaims[id / 8] |= 1u << (id % 8);
          }
          questui::Model model;
          model.profile = &hero;
          model.summary = &reading;
          model.assets = &art;
          model.view = questui::View::Missions;
          Canvas canvas(size.first, size.second);
          NarrativeTrace trace(canvas.target);
          toybox::Frame frame(trace, canvas.device, canvas.input, canvas.interactions);
          toybox::Screen screen(frame);
          questui::Work work;
          trace.watched[0] = state == 2   ? questui::huntTitle(hero.activeRequest)
                             : state == 0 ? tr(STR_QUEST_CAM_REQUEST_START)
                             : state == 3 ? tr(STR_QUEST_CAM_REQUEST_COMPLETE_COLLECTION)
                                          : tr(STR_QUEST_CAM_HUNT_AVAILABLE_HINT);
          trace.watched[1] = tr(STR_QUEST_CAM_REQUEST_HINT);
          questui::build(screen, model, work);
          CHECK(trace.watchedSeen[0] && trace.watchedFits[0]);
          CHECK(trace.watchedSeen[1] && trace.watchedFits[1]);
          canvas.checkHits();
          if (language == Language::ES)
            canvas.write(std::string(argv[2]) + "/missions-" + std::to_string(size.first) + "-" +
                         std::to_string(state) + ".pbm");
          Canvas home(size.first, size.second);
          NarrativeTrace homeTrace(home.target);
          toybox::Frame homeFrame(homeTrace, home.device, home.input, home.interactions);
          toybox::Screen homeScreen(homeFrame);
          questui::Work homeWork;
          model.view = questui::View::Hero;
          homeTrace.watched[0] = tr(STR_QUEST_BEASTS_SHORT);
          questui::build(homeScreen, model, homeWork);
          CHECK(homeTrace.watchedSeen[0] && homeTrace.watchedFits[0]);
          if (language == Language::ES)
            home.write(std::string(argv[2]) + "/home-" + std::to_string(size.first) + "-" + std::to_string(state) +
                       ".pbm");
        }
    std::printf("Bestias and Missions: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }

  if (argc > 1 && std::string(argv[1]) == "--monster-opacity-check") {
    sdart::Pack base, enemies;
    CHECK(base.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT));
    CHECK(enemies.open("resources/apps/reading-adventure/guardians.bin", questassets::GUARDIANS_COUNT));
    for (unsigned chapter = 0; chapter < quest::CAMPAIGN_CHAPTERS; ++chapter)
      for (auto pose : {questenemy::Pose::Ready, questenemy::Pose::Weakened, questenemy::Pose::Defeated}) {
        Canvas canvas(200, 200);
        const auto clip = canvas.target.clipRect();
        canvas.target.fill(fui::makeRect(0, 0, 200, 200), fui::Paint::solid(fui::Color::Black));
        questenemy::thumbnail(canvas.target, fui::makeRect(10, 10, 180, 180), base, &enemies,
                              quest::battleAnchor(chapter), pose);
        unsigned white = 0;
        for (uint8_t byte : canvas.pixels)
          for (unsigned bit = 0; bit < 8; ++bit) white += (byte >> bit) & 1;
        CHECK(white > 100);
        CHECK(canvas.target.clipRect().x == clip.x && canvas.target.clipRect().y == clip.y &&
              canvas.target.clipRect().width == clip.width && canvas.target.clipRect().height == clip.height);
      }
    std::printf("Monster opacity: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }

  if (argc > 2 && std::string(argv[1]) == "--victory-economy-preview") {
    I18N.setLanguage(Language::ES);
    sdart::Pack base, rewards, enemies, outfits;
    CHECK(base.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT));
    CHECK(rewards.open("resources/apps/reading-adventure/rewards.bin", questassets::REWARDS_COUNT));
    CHECK(enemies.open("resources/apps/reading-adventure/guardians.bin", questassets::GUARDIANS_COUNT));
    CHECK(outfits.open("resources/apps/reading-adventure/bestiary-outfits.bin", questassets::BESTIARY_OUTFITS_COUNT));
    quest::Profile hero;
    readingstats::Summary summary;
    hero.campaign.initialized = hero.campaign.prologueSeen = true;
    hero.campaign.introducedBooks = 1;
    hero.campaign.encounter = quest::battleAnchor(0);
    hero.campaign.narratedEncounters = hero.campaign.encounter + 1;
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      questui::Model model;
      model.profile = &hero;
      model.summary = &summary;
      model.assets = &base;
      model.rewardAssets = &rewards;
      model.enemyAssets = &enemies;
      model.outfitAssets = &outfits;
      auto shot = [&](const char* label) {
        Canvas canvas(size.first, size.second);
        questui::Work work;
        questui::build(canvas.screen, model, work);
        canvas.checkHits();
        canvas.write(std::string(argv[2]) + "/" + label + "-" + std::to_string(size.first) + ".pbm");
      };
      hero.campaign.phase = quest::BattlePhase::Reading;
      model.view = questui::View::Campaign;
      model.campaignTab = questui::CampaignTab::Encounter;
      shot("live");
      questreturn::Report report;
      report.kind = questreturn::Kind::Combat;
      report.seconds = 999 * 60;
      report.target = quest::encounterTarget(hero);
      report.after = report.target;
      report.xp = quest::battleXp(hero);
      report.victory = true;
      hero.campaign.phase = quest::BattlePhase::Story;
      model.view = questui::View::ReadingReturn;
      model.readingReturn = &report;
      model.attackPhase = questreturn::Phase::Result;
      shot("victory");
      hero.xp = quest::threshold(20);
      hero.campaign.extraCoins = 10000;
      hero.campaign.phase = quest::BattlePhase::Reading;
      model.view = questui::View::Shop;
      model.preview = 96;
      shot("item");
      quest::grantItem(hero, quest::sagaReward(0, quest::Slot::Body));
      quest::offerOutfit(hero, quest::sagaReward(0, quest::Slot::Body));
      model.view = questui::View::TryOut;
      model.preview = -1;
      shot("try-out");
      model.view = questui::View::EndConfirm;
      shot("new-game");
    }
    std::printf("Victory economy UI: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }

  if (argc > 2 && std::string(argv[1]) == "--return-milestones") {
    sdart::Pack assets, story, rewards, enemies, outfits, celebrations;
    CHECK(assets.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT));
    CHECK(story.open("resources/apps/reading-adventure/story.bin", questassets::STORY_COUNT));
    CHECK(rewards.open("resources/apps/reading-adventure/rewards.bin", questassets::REWARDS_COUNT));
    CHECK(enemies.open("resources/apps/reading-adventure/guardians.bin", questassets::GUARDIANS_COUNT));
    CHECK(outfits.open("resources/apps/reading-adventure/bestiary-outfits.bin", questassets::BESTIARY_OUTFITS_COUNT));
    CHECK(celebrations.open("resources/apps/reading-adventure/celebrations.bin", questassets::CELEBRATIONS_COUNT));
    for (auto language : {Language::EN, Language::ES}) {
      I18N.setLanguage(language);
      for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
        for (int state = 0; state < 10; ++state) {
          Canvas canvas(size.first, size.second);
          quest::Profile hero;
          readingstats::Summary summary;
          hero.campaign.initialized = hero.campaign.prologueSeen = true;
          hero.campaign.introducedBooks = 12;
          hero.campaign.encounter = quest::battleAnchor(0);
          hero.campaign.narratedEncounters = hero.campaign.encounter + 1;
          hero.gender = state % 2 ? quest::Gender::Feminine : quest::Gender::Masculine;
          hero.xp = quest::threshold(state < 3 ? questmilestone::LEVELS[state] : 100);
          hero.celebratedMilestones = state < 3 ? uint8_t((1u << state) - 1) : 3;
          hero.noticedLevel = quest::level(hero.xp);
          questui::Model model;
          model.profile = &hero;
          model.summary = &summary;
          model.assets = &assets;
          model.storyAssets = &story;
          model.rewardAssets = &rewards;
          model.enemyAssets = &enemies;
          model.outfitAssets = &outfits;
          model.celebrationAssets = &celebrations;
          questreturn::Report report;
          if (state < 3) {
            model.view = questui::View::Celebration;
          } else if (state < 8) {
            model.view = questui::View::ReadingReturn;
            report.kind = questreturn::Kind::Combat;
            report.seconds = 1200;
            report.xp = 260;
            report.target = 3000;
            report.after = state == 3 ? 1200 : 3000;
            report.victory = state != 3;
            hero.campaign.phase = report.victory ? quest::BattlePhase::Story : quest::BattlePhase::Reading;
            hero.campaign.victoryCoins = 81;
            if (state >= 5) {
              report.requestId = quest::kStormHunt;
              report.requestAfter = state == 5 ? 20 : 105;
              report.requestTarget = 105;
              hero.activeRequest = report.requestId;
              hero.requestTargetMinutes = 105;
              if (state >= 6) model.returnPanel = questreturn::Panel::Request;
            }
            model.readingReturn = &report;
            model.attackPhase = questreturn::Phase::Result;
          } else {
            model.view = state == 8 ? questui::View::Hero : questui::View::Requests;
            hero.campaign.encounter = 719;
            hero.campaign.phase = quest::BattlePhase::Complete;
            hero.campaign.state = quest::AdventureState::Peace;
            hero.endingSeen = true;
          }
          questui::Work work;
          NarrativeTrace trace(canvas.target);
          if (state < 3) {
            static constexpr StrId messages[] = {StrId::STR_QUEST_MILESTONE_50, StrId::STR_QUEST_MILESTONE_100,
                                                 StrId::STR_QUEST_MILESTONE_150};
            trace.watched[0] = I18N.get(messages[state]);
            trace.watched[1] = questui::rankTitle(questmilestone::LEVELS[state]);
          }
          toybox::Frame frame(trace, canvas.device, canvas.input, canvas.interactions, nullptr);
          toybox::Screen screen(frame);
          questui::build(screen, model, work);
          canvas.checkHits();
          if (state < 3) {
            CHECK(canvas.has(questui::MilestoneContinue));
            CHECK(!canvas.has(questui::OpenView, int(questui::View::Settings)));
            CHECK(trace.watchedSeen[0] && trace.watchedFits[0]);
            CHECK(trace.watchedSeen[1] && trace.watchedFits[1]);
          }
          if (state == 8) CHECK(canvas.has(questui::OpenView, int(questui::View::Requests)));
          if (state == 9) CHECK(canvas.has(questui::OpenView, int(questui::View::Campaign)));
          const std::string suffix =
              std::to_string(state) + "-" + (language == Language::ES ? "es" : "en") + "-" + std::to_string(size.first);
          canvas.write(std::string(argv[2]) + "/return-" + suffix + ".pbm");
        }
      }
    }
    std::printf("Return/milestones: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--balance-return-check") {
    for (auto language : {Language::EN, Language::ES}) {
      I18N.setLanguage(language);
      for (auto size : {std::pair<int, int>{480, 800}, {800, 480}})
        for (auto values : {std::pair<unsigned, unsigned>{23280, 23040}, {100, 100}, {100, 130}, {100, 1120}}) {
          quest::Profile hero;
          hero.campaign.encounter = quest::battleAnchor(100);
          readingstats::Summary summary;
          questreturn::Report report;
          report.kind = questreturn::Kind::Combat;
          report.seconds = 20;
          report.before = values.first;
          report.after = values.second;
          report.victory = report.after <= report.before;
          report.target = report.victory ? report.after : 2000;
          const unsigned expectedSeconds = values.second > values.first ? values.second - values.first : 0;
          char expected[32];
          std::snprintf(
              expected, sizeof(expected),
              expectedSeconds >= 60 ? tr(STR_QUEST_CAM_RETURN_PROGRESS) : tr(STR_QUEST_CAM_RETURN_PROGRESS_SECONDS),
              expectedSeconds >= 60 ? expectedSeconds / 60 : expectedSeconds);
          if (report.victory) std::snprintf(expected, sizeof(expected), "%u", hero.campaign.victoryCoins);
          questui::Model model;
          model.profile = &hero;
          model.summary = &summary;
          model.view = questui::View::ReadingReturn;
          model.readingReturn = &report;
          model.attackPhase = questreturn::Phase::Result;
          Canvas canvas(size.first, size.second);
          NarrativeTrace trace(canvas.target);
          trace.watched[0] = expected;
          toybox::Frame frame(trace, canvas.device, canvas.input, canvas.interactions);
          toybox::Screen screen(frame);
          questui::Work work;
          questui::build(screen, model, work);
          canvas.checkHits();
          CHECK(trace.watchedSeen[0] && trace.watchedFits[0]);
          CHECK(canvas.has(questui::DismissReadingReturn));
        }
    }
    std::printf(
        "Balance return: %d checks, %d failures; shrinking, zero and positive progress in both "
        "languages/orientations\n",
        checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 2 && std::string(argv[1]) == "--balance-preview") {
    I18N.setLanguage(Language::ES);
    sdart::Pack base, rewards, enemies, outfits;
    CHECK(base.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT));
    CHECK(rewards.open("resources/apps/reading-adventure/rewards.bin", questassets::REWARDS_COUNT));
    CHECK(enemies.open("resources/apps/reading-adventure/guardians.bin", questassets::GUARDIANS_COUNT));
    CHECK(outfits.open("resources/apps/reading-adventure/bestiary-outfits.bin", questassets::BESTIARY_OUTFITS_COUNT));
    quest::Profile hero;
    hero.campaign.initialized = hero.campaign.prologueSeen = true;
    hero.campaign.introducedBooks = 12;
    hero.campaign.encounter = quest::battleAnchor(65);
    hero.campaign.narratedEncounters = hero.campaign.encounter + 1;
    readingstats::Summary summary;
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      questui::Model model;
      model.profile = &hero;
      model.summary = &summary;
      model.assets = &base;
      model.rewardAssets = &rewards;
      model.enemyAssets = &enemies;
      model.outfitAssets = &outfits;
      auto shot = [&](const char* label) {
        Canvas canvas(size.first, size.second);
        questui::Work work;
        questui::build(canvas.screen, model, work);
        canvas.checkHits();
        canvas.write(std::string(argv[2]) + "/" + label + "-" + std::to_string(size.first) + ".pbm");
      };
      for (unsigned level : {1u, 50u, 100u, 101u, 125u, 146u, 150u}) {
        hero.xp = quest::threshold(level);
        hero.noticedLevel = level;
        model.view = questui::View::Info;
        shot(("power-" + std::to_string(level)).c_str());
        model.view = questui::View::Hero;
        shot(("hero-" + std::to_string(level)).c_str());
        CHECK(std::string(questui::rankTitle(level)).size() > 0);
      }
      hero.xp = quest::threshold(100);
      model.view = questui::View::Shop;
      model.preview = 96;
      shot("gear");
      model.preview = -1;
      model.view = questui::View::Campaign;
      shot("battle");
    }
    std::printf("Balance previews: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--letter-attack-check") {
    sdart::Pack base, rewards, outfits;
    CHECK(base.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT));
    CHECK(rewards.open("resources/apps/reading-adventure/rewards.bin", questassets::REWARDS_COUNT));
    CHECK(outfits.open("resources/apps/reading-adventure/bestiary-outfits.bin", questassets::BESTIARY_OUTFITS_COUNT));
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}})
      for (auto sex : {quest::Gender::Masculine, quest::Gender::Feminine})
        for (int item : {0, quest::kOutfitCount - 1, quest::kKingBody, quest::kBestiaryOutfitStart}) {
          quest::Profile profile;
          profile.gender = sex;
          profile.equipped[0] = item;
          for (auto phase : {questreturn::Phase::Idle, questreturn::Phase::Raise, questreturn::Phase::Swing,
                             questreturn::Phase::Impact, questreturn::Phase::Result}) {
            Canvas canvas(size.first, size.second);
            const auto area = fui::makeRect(40, 110, size.first - 80, size.second - 180);
            const auto prior = area.inset({7, 9, 11, 13});
            CHECK(canvas.target.setClipRect(prior));
            questreturn::letters(canvas.target, area, base, profile, phase, 317, questavatar::Framing::Tall, &rewards,
                                 &outfits);
            const auto clip = canvas.target.clipRect();
            CHECK(clip.x == prior.x && clip.y == prior.y && clip.width == prior.width && clip.height == prior.height);
            int changed = 0, escaped = 0;
            for (int y = 0; y < size.second; ++y)
              for (int x = 0; x < size.first; ++x) {
                const bool set = (canvas.pixels[y * ((size.first + 7) / 8) + x / 8] & (0x80 >> (x % 8))) == 0;
                if (!set) continue;
                ++changed;
                if (x < prior.x || x >= prior.right() || y < prior.y || y >= prior.bottom()) ++escaped;
              }
            CHECK(!escaped);
            if ((changed > 0) != questreturn::isPlaying(phase))
              std::printf("Attack case %dx%d sex=%d item=%d phase=%d pixels=%d\n", size.first, size.second, int(sex),
                          item, int(phase), changed);
            CHECK((changed > 0) == questreturn::isPlaying(phase));
          }
        }
    std::printf(
        "Reading-letter attack: %d checks, %d failures; phases, clipping, sexes, outfit silhouettes and orientations\n",
        checks, failures);
    return failures ? 1 : 0;
  }

  if (argc > 1 && std::string(argv[1]) == "--shop-flow-check") {
    for (auto language : {Language::EN, Language::ES}) {
      I18N.setLanguage(language);
      for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
        quest::Profile hero;
        hero.xp = quest::threshold(50);
        readingstats::Summary summary;
        questui::Model model;
        model.profile = &hero;
        model.summary = &summary;
        for (auto view :
             {questui::View::Settings, questui::View::Info, questui::View::Shop, questui::View::PurchaseConfirm}) {
          model.view = view;
          model.preview = (view == questui::View::Shop || view == questui::View::PurchaseConfirm) ? 2 : -1;
          Canvas canvas(size.first, size.second);
          questui::Work work;
          questui::build(canvas.screen, model, work);
          canvas.checkHits();
          if (view == questui::View::Settings) CHECK(!canvas.has(questui::OpenView, int(questui::View::Plan)));
          if (view == questui::View::PurchaseConfirm) {
            CHECK(canvas.has(questui::ConfirmPurchase, 2));
            questui::focusConfirmation(canvas.interactions, view);
            CHECK(canvas.interactions.data()[canvas.interactions.focusedIndex()].action == questui::Back);
            questui::focusConfirmation(canvas.interactions, view, 1);
            CHECK(canvas.interactions.data()[canvas.interactions.focusedIndex()].action == questui::ConfirmPurchase);
          }
        }
        for (int notice = 0; notice < 3; ++notice) {
          model.failed = notice == 0;
          model.denied = notice == 1;
          model.storeNotice = notice == 2 ? quest::StoreNotice::Recovered : quest::StoreNotice::None;
          for (auto view : {questui::View::Shop, questui::View::PurchaseConfirm}) {
            model.view = view;
            Canvas canvas(size.first, size.second);
            questui::Work work;
            questui::build(canvas.screen, model, work);
            canvas.checkHits();
          }
        }
        model.failed = model.denied = false;
        model.storeNotice = quest::StoreNotice::None;
        questreturn::Report report;
        report.requestId = quest::kCrystalHunt;
        report.requestTarget = 180;
        report.requestBefore = 100;
        model.view = questui::View::ReadingReturn;
        model.preview = -1;
        model.readingReturn = &report;
        model.returnPanel = questreturn::Panel::Request;
        for (auto phase : {questreturn::Phase::Raise, questreturn::Phase::Swing, questreturn::Phase::Impact,
                           questreturn::Phase::Result})
          for (unsigned value : {120u, 180u}) {
            report.requestAfter = value;
            model.attackPhase = phase;
            Canvas canvas(size.first, size.second);
            questui::Work work;
            questui::build(canvas.screen, model, work);
            canvas.checkHits();
            CHECK(canvas.has(questui::DismissReadingReturn) == (phase == questreturn::Phase::Result));
            CHECK(!canvas.has(questui::CollectRequest, report.requestId));
          }
      }
    }
    std::printf("Shop/return UI: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }

  if (argc > 2 && std::string(argv[1]) == "--polish-preview") {
    I18N.setLanguage(Language::ES);
    sdart::Pack base, rewards, enemies, outfits;
    CHECK(base.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT));
    CHECK(rewards.open("resources/apps/reading-adventure/rewards.bin", questassets::REWARDS_COUNT));
    CHECK(enemies.open("resources/apps/reading-adventure/guardians.bin", questassets::GUARDIANS_COUNT));
    CHECK(outfits.open("resources/apps/reading-adventure/bestiary-outfits.bin", questassets::BESTIARY_OUTFITS_COUNT));
    quest::Profile hero;
    hero.xp = quest::threshold(50);
    hero.campaign.initialized = hero.campaign.prologueSeen = true;
    hero.campaign.introducedBooks = 1;
    hero.campaign.encounter = 29;
    hero.campaign.narratedEncounters = 30;
    readingstats::Summary summary;
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      questui::Model model;
      model.profile = &hero;
      model.summary = &summary;
      model.assets = &base;
      model.rewardAssets = &rewards;
      model.enemyAssets = &enemies;
      model.outfitAssets = &outfits;
      auto shot = [&](const char* name) {
        Canvas canvas(size.first, size.second);
        questui::Work work;
        questui::build(canvas.screen, model, work);
        canvas.checkHits();
        canvas.write(std::string(argv[2]) + "/" + name + "-" + std::to_string(size.first) + ".pbm");
      };
      model.view = questui::View::Settings;
      shot("settings");
      model.view = questui::View::Info;
      shot("bonuses");
      model.view = questui::View::Shop;
      model.preview = 2;
      shot("shop");
      model.view = questui::View::PurchaseConfirm;
      shot("purchase");
      model.failed = true;
      shot("purchase-error");
      model.failed = false;
      model.denied = true;
      shot("purchase-denied");
      model.denied = false;
      model.storeNotice = quest::StoreNotice::Recovered;
      shot("purchase-recovered");
      model.view = questui::View::Shop;
      shot("shop-recovered");
      model.storeNotice = quest::StoreNotice::None;
      questreturn::Report report;
      report.requestId = quest::kCrystalHunt;
      report.requestTarget = 180;
      report.requestBefore = 150;
      report.requestAfter = 180;
      model.view = questui::View::ReadingReturn;
      model.preview = -1;
      model.returnPanel = questreturn::Panel::Request;
      model.readingReturn = &report;
      model.attackPhase = questreturn::Phase::Result;
      shot("hunt-return");
      report.seconds = 120;
      report.kind = questreturn::Kind::Combat;
      report.target = 1200;
      report.before = 300;
      report.after = 420;
      for (int phase = 1; phase <= 3; ++phase) {
        model.attackPhase = questreturn::Phase(phase);
        model.returnPanel = questreturn::Panel::Reading;
        const std::string label = "letters-campaign-" + std::to_string(phase);
        shot(label.c_str());
        model.returnPanel = questreturn::Panel::Request;
        const std::string huntLabel = "letters-hunt-" + std::to_string(phase);
        shot(huntLabel.c_str());
      }
    }
    std::printf("Polish previews: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }

  if (argc > 3 && std::string(argv[1]) == "--collection-fixture") {
    std::array<uint8_t, quest::kProfileBytes> bytes{};
    FILE* input = std::fopen(argv[2], "rb");
    CHECK(input);
    if (!input) return 1;
    CHECK(std::fread(bytes.data(), 1, 512, input) == 512);
    std::fclose(input);
    quest::Profile hero;
    CHECK(quest::decode(bytes.data(), hero));
    const auto baseline = hero.campaign.observedSeconds;
    hero.activeRequest = quest::kRequestCount;
    hero.requestStarted = hero.requestBaseline = hero.requestReported = hero.requestCarried = 0;
    hero.pausedInkSeconds = 0;
    const int day = readingstats::dayIndex({2026, 11, 12});
    quest::reconcileCollection(hero);
    for (int item : {3, 4, 6, 9, 16, 20}) quest::grantItem(hero, item);
    for (int id : {0, quest::kStormHunt, quest::kCrystalHunt}) {
      CHECK(quest::acceptRequest(hero, id));
      hero.campaign.observedSeconds += quest::requestMinutes(id) * 60;
      hero.requestBooks = id == quest::kCrystalHunt ? 2 : 0;
      hero.requestBooks = id == quest::kCrystalHunt ? 2 : 0;
      CHECK(quest::collectRequest(hero, id, nullptr, day - 2));
    }
    hero.campaign.observedSeconds = baseline;
    CHECK(quest::toggleFavorite(hero, 3));
    CHECK(quest::toggleFavorite(hero, quest::huntOutfitItem(quest::kCrystalHunt)));
    CHECK(quest::acceptRequest(hero, quest::kBestiaryHuntStart + 2));
    hero.requestCarried = hero.requestElapsed = 1800;
    hero.savedDay = day;
    hero.endingSeen = true;
    quest::encode(hero, bytes.data());
    quest::Profile checked;
    CHECK(quest::decode(bytes.data(), checked));
    FILE* output = std::fopen(argv[3], "wb");
    CHECK(output);
    if (!output) return 1;
    CHECK(std::fwrite(bytes.data(), 1, bytes.size(), output) == bytes.size());
    std::fclose(output);
    return failures ? 1 : 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--victory-record-check") {
    for (auto language : {Language::EN, Language::ES}) {
      I18N.setLanguage(language);
      for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
        quest::Profile hero;
        hero.campaign.initialized = hero.campaign.prologueSeen = true;
        hero.campaign.introducedBooks = 12;
        hero.campaign.encounter = 719;
        hero.campaign.phase = quest::BattlePhase::Complete;
        hero.inkHuntDiscovered = true;
        hero.observedBooks = 0;
        readingstats::Summary summary;
        questui::Model model;
        model.profile = &hero;
        model.summary = &summary;
        const int day = readingstats::dayIndex({2026, 11, 12});
        for (unsigned n = 0; n < quest::CAMPAIGN_CHAPTERS; ++n) {
          hero.campaignVictories[n] = {unsigned(day), 18000, 777, 888, 2};
          model.view = questui::View::AtlasPlace;
          model.selectedEncounter = int(quest::battleAnchor(n));
          Canvas canvas(size.first, size.second);
          NarrativeTrace trace(canvas.target);
          trace.watched[0] = "12/11/2026";
          trace.watched[1] = "5 h 00 min";
          toybox::Frame frame(trace, canvas.device, canvas.input, canvas.interactions);
          toybox::Screen screen(frame);
          questui::Work work;
          questui::build(screen, model, work);
          canvas.checkHits();
          CHECK(work.pages == 1 && canvas.has(questui::Back));
          CHECK(!canvas.has(questui::BeginEncounter) && !canvas.has(questui::StoryPageOpen));
          CHECK(trace.watchedSeen[0] && trace.watchedFits[0]);
          CHECK(trace.watchedSeen[1] && trace.watchedFits[1]);
        }
        for (int id = 0; id < quest::kRequestCount; ++id) {
          if (quest::requestClaimed(hero, id)) continue;
          if (id < quest::kLegacyRequestCount && id != quest::huntForPath(quest::requestChapter(id))) continue;
          CHECK(quest::acceptRequest(hero, id));
          hero.requestBooks = 2;
          hero.campaign.observedSeconds += quest::requestMinutes(id) * 60;
          CHECK(quest::collectRequest(hero, id, nullptr, day));
          model.view = questui::View::Request;
          model.selectedRequest = id;
          Canvas canvas(size.first, size.second);
          NarrativeTrace trace(canvas.target);
          trace.watched[0] = "12/11/2026";
          toybox::Frame frame(trace, canvas.device, canvas.input, canvas.interactions);
          toybox::Screen screen(frame);
          questui::Work work;
          questui::build(screen, model, work);
          canvas.checkHits();
          CHECK(trace.watchedSeen[0] && trace.watchedFits[0]);
          CHECK(canvas.has(questui::BackToRequests) && !canvas.has(questui::AcceptRequest, id) &&
                !canvas.has(questui::CollectRequest, id));
        }
        for (int book = 0; book < 12; ++book) {
          model.view = questui::View::AtlasRegion;
          model.selectedBook = book;
          int total = 0;
          for (int page = 0; page < 3; ++page) {
            model.page = page;
            Canvas canvas(size.first, size.second);
            questui::Work work;
            questui::build(canvas.screen, model, work);
            canvas.checkHits();
            CHECK(work.pages == 3);
            for (size_t n = 0; n < canvas.interactions.count(); ++n)
              total += canvas.interactions.data()[n].action == questui::AtlasChapter;
          }
          CHECK(total == 10);
        }
      }
    }
    std::printf("Victory records: %d checks, %d failures; campaign, hunts, book counts, dates and both orientations\n",
                checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--collection-nav-check") {
    for (auto language : {Language::EN, Language::ES}) {
      I18N.setLanguage(language);
      for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
        quest::Profile hero;
        hero.campaign.initialized = hero.campaign.prologueSeen = true;
        hero.campaign.introducedBooks = 12;
        hero.campaign.encounter = 719;
        hero.campaign.phase = quest::BattlePhase::Complete;
        hero.endingSeen = true;
        quest::reconcileCollection(hero);
        CHECK(quest::grantItem(hero, 3));
        CHECK(quest::acceptRequest(hero, 0));
        hero.campaign.observedSeconds += 600;
        CHECK(quest::collectRequest(hero, 0, nullptr, 20300));
        readingstats::Summary summary;
        questui::Model m;
        m.profile = &hero;
        m.summary = &summary;
        hero.inkHuntDiscovered = true;
        m.view = questui::View::Requests;
        Canvas board(size.first, size.second);
        questui::Work w;
        NarrativeTrace boardText(board.target);
        boardText.watched[0] = "+9000";
        toybox::Frame boardFrame(boardText, board.device, board.input, board.interactions);
        toybox::Screen boardScreen(boardFrame);
        questui::build(boardScreen, m, w);
        CHECK(boardText.watchedSeen[0] && boardText.watchedFits[0]);
        board.checkHits();
        CHECK(!board.has(questui::Page, -1) && !board.has(questui::Page, 1));
        int offers = 0;
        for (size_t n = 0; n < board.interactions.count(); ++n) {
          const auto& hit = board.interactions.data()[n];
          offers += hit.action == questui::RequestInfo;
          if (hit.action == questui::OpenView && hit.value == int(questui::View::Settings))
            CHECK(hit.rect.right() <= size.first - 14);
        }
        CHECK(offers == 4);
        m.requestFilter = questui::RequestFilter::Collection;
        Canvas memories(size.first, size.second);
        questui::build(memories.screen, m, w);
        CHECK(memories.has(questui::RequestInfo, 0));
        m.view = questui::View::Request;
        m.selectedRequest = 0;
        hero.victories[0].coins = 777;
        Canvas won(size.first, size.second);
        NarrativeTrace earned(won.target);
        earned.watched[0] = "777";
        toybox::Frame earnedFrame(earned, won.device, won.input, won.interactions);
        toybox::Screen earnedScreen(earnedFrame);
        questui::build(earnedScreen, m, w);
        CHECK(earned.watchedSeen[0] && earned.watchedFits[0]);
        won.checkHits();
        CHECK(!won.has(questui::AcceptRequest, 0) && won.has(questui::BackToRequests));
        const int active = quest::kBestiaryHuntStart + 2;
        CHECK(quest::acceptRequest(hero, active));
        m.selectedRequest = active;
        Canvas fight(size.first, size.second);
        questui::build(fight.screen, m, w);
        fight.checkHits();
        CHECK(fight.has(questui::OpenView, int(questui::View::Hero)) && !fight.has(questui::BackToRequests));
        m.view = questui::View::Gear;
        m.preview = 3;
        Canvas gear(size.first, size.second);
        questui::build(gear.screen, m, w);
        gear.checkHits();
        CHECK(gear.has(questui::FavoriteItem, 3));
        m.preview = -1;
        Canvas list(size.first, size.second);
        questui::build(list.screen, m, w);
        list.checkHits();
        CHECK(list.has(questui::InventoryFilterChange, 0) && list.has(questui::InventoryFilterChange, 1));
      }
    }
    std::printf("Collection navigation: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 2 && std::string(argv[1]) == "--collection-preview") {
    I18N.setLanguage(Language::ES);
    sdart::Pack base, rewards, enemies, story, outfits;
    CHECK(base.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT));
    CHECK(rewards.open("resources/apps/reading-adventure/rewards.bin", questassets::REWARDS_COUNT));
    CHECK(enemies.open("resources/apps/reading-adventure/guardians.bin", questassets::GUARDIANS_COUNT));
    CHECK(story.open("resources/apps/reading-adventure/story.bin", questassets::STORY_COUNT));
    CHECK(outfits.open("resources/apps/reading-adventure/bestiary-outfits.bin", questassets::BESTIARY_OUTFITS_COUNT));
    quest::Profile hero;
    hero.campaign.initialized = hero.campaign.prologueSeen = true;
    hero.campaign.introducedBooks = 12;
    hero.campaign.encounter = 719;
    hero.campaign.narratedEncounters = 720;
    hero.campaign.phase = quest::BattlePhase::Complete;
    hero.campaign.originSeconds = hero.campaign.observedSeconds = 1000;
    const int day = readingstats::dayIndex({2026, 11, 12});
    hero.savedDay = day;
    hero.observedBooks = 10;
    for (unsigned n = 0; n < quest::CAMPAIGN_CHAPTERS; ++n)
      hero.campaignVictories[n] = {unsigned(day - (quest::CAMPAIGN_CHAPTERS - 1 - n)), unsigned((n + 4) * 60),
                                   200u + n * 20, 54u + n, n == 1 ? 1u : 0u};
    quest::reconcileCollection(hero);
    for (int item : {3, 4, 6, 9, 16, 20}) quest::grantItem(hero, item);
    for (int id : {0, quest::kStormHunt, quest::kCrystalHunt}) {
      CHECK(quest::acceptRequest(hero, id));
      hero.campaign.observedSeconds += quest::requestMinutes(id) * 60;
      hero.requestBooks = id == quest::kCrystalHunt ? 2 : 0;
      CHECK(quest::collectRequest(hero, id, nullptr, day - 2));
    }
    CHECK(quest::toggleFavorite(hero, 3));
    CHECK(quest::toggleFavorite(hero, quest::huntOutfitItem(quest::kCrystalHunt)));
    CHECK(quest::acceptRequest(hero, quest::kBestiaryHuntStart + 2));
    hero.campaign.observedSeconds += 1800;
    readingstats::Summary summary;
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      questui::Model model;
      model.profile = &hero;
      model.summary = &summary;
      model.today = day;
      model.assets = &base;
      model.rewardAssets = &rewards;
      model.enemyAssets = &enemies;
      model.storyAssets = &story;
      model.outfitAssets = &outfits;
      const auto shot = [&](const char* name) {
        Canvas canvas(size.first, size.second);
        questui::Work work;
        questui::build(canvas.screen, model, work);
        canvas.checkHits();
        canvas.write(std::string(argv[2]) + "/" + name + "-" + std::to_string(size.first) + ".pbm");
      };
      model.view = questui::View::AtlasRegion;
      model.selectedBook = 0;
      shot("atlas-region");
      model.view = questui::View::AtlasPlace;
      model.selectedEncounter = 11;
      shot("atlas-detail");
      model.selectedEncounter = 59;
      shot("atlas-saga-rewards");
      model.selectedEncounter = 719;
      shot("atlas-final-rewards");
      model.view = questui::View::Requests;
      model.requestFilter = questui::RequestFilter::Collection;
      shot("hunt-memories");
      model.requestFilter = questui::RequestFilter::Available;
      shot("hunts");
      model.view = questui::View::Request;
      model.selectedRequest = quest::kBestiaryHuntStart + 1;
      shot("preview");
      model.selectedRequest = quest::kCrystalHunt;
      shot("memory");
      model.view = questui::View::Gear;
      shot("gear");
      model.inventoryFilter = questui::InventoryFilter::Favorites;
      shot("favorites");
      model.inventoryFilter = questui::InventoryFilter::Recent;
      model.preview = quest::huntOutfitItem(quest::kCrystalHunt);
      shot("gear-detail");
      model.preview = -1;
      quest::Profile prologue;
      prologue.campaign.initialized = true;
      prologue.campaign.encounter = 5;
      model.profile = &prologue;
      model.view = questui::View::Campaign;
      model.campaignTab = questui::CampaignTab::Saga;
      for (int page = 0; page < 20; ++page) {
        Canvas canvas(size.first, size.second);
        questui::Work work;
        NarrativeTrace trace(canvas.target);
        trace.watched[0] = I18N.get(queststory::PROLOGUE_TITLES[1]);
        model.introductionPage = page;
        toybox::Frame frame(trace, canvas.device, canvas.input, canvas.interactions);
        toybox::Screen screen(frame);
        questui::build(screen, model, work);
        if (trace.watchedSeen[0]) {
          canvas.write(std::string(argv[2]) + "/story-" + std::to_string(size.first) + ".pbm");
          break;
        }
      }
    }
    std::printf("Collection previews: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }

  if (argc > 1 && std::string(argv[1]) == "--request-change-nav-check") {
    for (auto language : {Language::EN, Language::ES}) {
      I18N.setLanguage(language);
      for (auto size : {std::pair<int, int>{480, 800}, {800, 480}})
        for (auto view : {questui::View::RequestCancelConfirm, questui::View::RequestSwitchConfirm})
          for (bool writable : {false, true}) {
            quest::Profile hero;
            hero.campaign.initialized = hero.campaign.prologueSeen = true;
            hero.campaign.introducedBooks = 12;
            hero.campaign.encounter = 719;
            hero.campaign.phase = quest::BattlePhase::Complete;
            readingstats::Summary summary;
            CHECK(quest::acceptRequest(hero, 0));
            questui::Model model;
            questui::Work work;
            model.profile = &hero;
            model.summary = &summary;
            model.view = view;
            model.selectedRequest = view == questui::View::RequestSwitchConfirm ? 3 : 0;
            model.writable = writable;
            Canvas c(size.first, size.second);
            questui::build(c.screen, model, work);
            paintclock::notePainted();
            c.interactions.setFocusedIndex(-1);
            questui::focusRequestConfirmation(c.interactions);
            fui::InputSnapshot confirm;
            confirm.confirm = true;
            CHECK(c.interactions.route(confirm).action == questui::KeepRequest);
            for (int steps : {1, 2, -1, 3, -3}) {
              questui::focusRequestConfirmation(c.interactions, steps);
              const auto event = c.interactions.route(confirm);
              CHECK(event.action == questui::KeepRequest ||
                    (writable && event.action == questui::ConfirmRequestChange));
              if (event.action == questui::ConfirmRequestChange) CHECK(event.value == model.selectedRequest);
            }
            c.interactions.setFocusedIndex(-1);
            questui::focusRequestConfirmation(c.interactions);
            questui::focusRequestConfirmation(c.interactions, 1);
            CHECK(c.interactions.route(confirm).action ==
                  (writable ? questui::ConfirmRequestChange : questui::KeepRequest));
            questui::focusRequestConfirmation(c.interactions, -1);
            CHECK(c.interactions.route(confirm).action == questui::KeepRequest);
          }
    }
    std::printf(
        "Request modal navigation: %d checks, %d failures; safe initial focus, next/previous/confirm and disabled "
        "actions\n",
        checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--request-change-check") {
    for (const auto* face : {&toybox_14, &ubuntu_12_regular, &ubuntu_12_bold}) {
      FontBridge checkedFont(*face);
      for (auto language : {Language::EN, Language::ES}) {
        I18N.setLanguage(language);
        for (auto size : {std::pair<int, int>{480, 800}, {800, 480}})
          for (int id = 0; id < quest::kRequestCount; ++id) {
            if (id < quest::kCampaignHuntCount && id != quest::huntForPath(quest::requestChapter(id))) continue;
            for (bool failed : {false, true}) {
              quest::Profile hero;
              hero.campaign.initialized = hero.campaign.prologueSeen = true;
              hero.campaign.introducedBooks = 12;
              hero.campaign.encounter = 719;
              hero.campaign.phase = quest::BattlePhase::Complete;
              hero.inkHuntDiscovered = true;
              readingstats::Summary summary;
              CHECK(quest::acceptRequest(hero, id));
              for (auto view : {questui::View::RequestCancelConfirm, questui::View::RequestSwitchConfirm})
                for (bool resuming : {false, true}) {
                  if (resuming && (view != questui::View::RequestSwitchConfirm || id == quest::kInkHunt)) continue;
                  hero.pausedInkSeconds = resuming ? 7200 : 0;
                  Canvas c(size.first, size.second);
                  c.target.setFont(0, checkedFont.font);
                  NarrativeTrace trace(c.target);
                  questui::Model model;
                  questui::Work work;
                  model.profile = &hero;
                  model.summary = &summary;
                  model.view = view;
                  model.selectedRequest = resuming                                      ? quest::kInkHunt
                                          : view == questui::View::RequestCancelConfirm ? id
                                                                                        : (id == 0 ? 3 : 0);
                  model.failed = failed;
                  model.writable = !failed;
                  trace.watched[0] = questui::huntTitle(id);
                  trace.watched[1] = resuming                ? tr(STR_QUEST_HUNTS_RESUME_WARNING)
                                     : id == quest::kInkHunt ? tr(STR_QUEST_HUNTS_PAUSE_WARNING)
                                     : view == questui::View::RequestCancelConfirm
                                         ? tr(STR_QUEST_CAM_REQUEST_LOSE_PROGRESS)
                                         : tr(STR_QUEST_CAM_REQUEST_SWITCH_WARNING);
                  toybox::Frame frame(trace, c.device, c.input, c.interactions);
                  toybox::Screen screen(frame);
                  questui::build(screen, model, work);
                  c.checkHits();
                  CHECK(trace.watchedSeen[0] && trace.watchedFits[0]);
                  CHECK(trace.watchedSeen[1] && trace.watchedFits[1]);
                  CHECK(c.has(questui::KeepRequest));
                  CHECK(c.has(questui::ConfirmRequestChange, model.selectedRequest) == !failed);
                  CHECK(!c.has(questui::AcceptRequest) && !c.has(questui::CollectRequest));
                }
            }
          }
      }
    }
    std::printf(
        "Request change UI: %d checks, %d failures;unique guardians, cancellation/switch, save errors, EN/ES and "
        "orientations and actual font metrics\n",
        checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 2 && std::string(argv[1]) == "--request-change-preview") {
    I18N.setLanguage(Language::ES);
    sdart::Pack base, rewards, enemies;
    CHECK(base.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT));
    CHECK(rewards.open("resources/apps/reading-adventure/rewards.bin", questassets::REWARDS_COUNT));
    CHECK(enemies.open("resources/apps/reading-adventure/guardians.bin", questassets::GUARDIANS_COUNT));
    quest::Profile hero;
    hero.campaign.initialized = hero.campaign.prologueSeen = true;
    hero.campaign.introducedBooks = 12;
    hero.campaign.encounter = 719;
    hero.campaign.phase = quest::BattlePhase::Complete;
    readingstats::Summary summary;
    CHECK(quest::acceptRequest(hero, quest::kBestiaryHuntStart + 2));
    hero.campaign.observedSeconds = 1800;
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}})
      for (auto view : {questui::View::Requests, questui::View::Request, questui::View::RequestCancelConfirm,
                        questui::View::RequestSwitchConfirm}) {
        Canvas c(size.first, size.second);
        questui::Model model;
        questui::Work work;
        model.profile = &hero;
        model.summary = &summary;
        model.assets = &base;
        model.rewardAssets = &rewards;
        model.enemyAssets = &enemies;
        model.view = view;
        model.selectedRequest =
            view == questui::View::RequestSwitchConfirm ? quest::huntForPath(2) : int(hero.activeRequest);
        questui::build(c.screen, model, work);
        c.checkHits();
        c.write(std::string(argv[2]) + "/request-" + std::to_string(int(view)) + "-" + std::to_string(size.first) +
                ".pbm");
      }
    std::printf("Request change preview: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 2 && std::string(argv[1]) == "--home-card-preview") {
    sdart::Pack assets, enemies;
    CHECK(assets.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT));
    CHECK(enemies.open("resources/apps/reading-adventure/guardians.bin", questassets::GUARDIANS_COUNT));
    for (bool hunt : {false, true}) {
      Canvas iconCanvas(80, 80);
      NarrativeTrace iconTrace(iconCanvas.target);
      questicons::monsterHead(iconTrace, fui::makeRect(8, 8, 64, 64), hunt);
      CHECK(iconTrace.triangleCalls == 0);
      questicons::bow(iconTrace, fui::makeRect(8, 8, 64, 64));
      CHECK(iconTrace.triangleCalls == 0);
    }
    for (auto language : {Language::ES, Language::EN}) {
      I18N.setLanguage(language);
      for (auto size : {std::pair<int, int>{480, 800}, {800, 480}})
        for (int notice = 0; notice < 3; ++notice)
          for (int state = 0; state < 8 + quest::kBestiaryCount; ++state) {
            quest::Profile hero;
            hero.campaign.initialized = hero.campaign.prologueSeen = true;
            hero.campaign.encounter = quest::battleAnchor(119);
            hero.campaign.introducedBooks = 12;
            hero.campaign.narratedEncounters = quest::CAMPAIGN_ENCOUNTERS;
            hero.campaign.phase = quest::BattlePhase::Complete;
            hero.inkHuntDiscovered = true;
            const bool hunt = state == 1 || state == 2 || state >= 8;
            if (hunt || state == 3)
              CHECK(quest::acceptRequest(hero, state >= 8 ? quest::kBestiaryHuntStart + state - 8 : 0));
            if (state != 6 && state < 8) {
              hero.campaign.encounter = quest::battleAnchor(3);
              hero.campaign.phase = state == 3 ? quest::BattlePhase::Victory : quest::BattlePhase::Reading;
            }
            if (state == 2) hero.campaign.observedSeconds = quest::requestTarget(hero, hero.activeRequest) * 60;
            if (state == 4) hero.campaign.prologueSeen = false;
            if (state == 5) hero.campaign.narratedEncounters = 0;
            if (state == 7) hero.campaign.pendingReward = 200;
            readingstats::Summary summary;
            questui::Model model;
            model.profile = &hero;
            model.summary = &summary;
            model.view = questui::View::Hero;
            model.assets = &assets;
            model.enemyAssets = &enemies;
            model.failed = notice == 1;
            model.denied = notice == 2;
            for (int variant = 0; variant < 2; ++variant) {
              model.page = variant;
              Canvas canvas(size.first, size.second);
              NarrativeTrace trace(canvas.target);
              trace.watchLast = true;
              questui::Work expected;
              if (state == 4)
                trace.watched[0] = tr(STR_QUEST_CAM_START);
              else if (state == 5)
                trace.watched[0] = tr(STR_QUEST_CAM_AVAILABLE);
              else if (state == 6)
                trace.watched[0] = tr(STR_QUEST_CAM_DONE);
              else if (state == 2 || state == 3 || state == 7)
                trace.watched[0] = tr(STR_QUEST_CAM_RECEIVE);
              else if (hunt) {
                if (quest::requestGoal(hero, hero.activeRequest) == quest::RequestGoal::Minutes)
                  std::snprintf(expected.label, sizeof(expected.label), tr(STR_QUEST_CAM_ORDER_PROGRESS_MINUTES),
                                quest::requestValue(hero, hero.activeRequest),
                                quest::requestTarget(hero, hero.activeRequest));
                else
                  questui::requestui::goalText(expected, model, hero.activeRequest, true, true);
                trace.watched[0] = expected.label;
              } else {
                std::snprintf(expected.label, sizeof(expected.label), tr(STR_QUEST_CAM_ORDER_PROGRESS_MINUTES), 0u,
                              unsigned((quest::encounterTarget(hero) + 59) / 60));
                trace.watched[0] = expected.label;
              }
              trace.watched[1] = hunt                       ? questui::huntTitle(hero.activeRequest)
                                 : state == 3 || state == 0 ? questui::journeyTitle(hero.campaign.encounter)
                                 : state == 4               ? tr(STR_QUEST_CAM_BEGIN_INVITE)
                                 : state == 5               ? tr(STR_QUEST_CAM_BATTLE_CHOOSE_PATH)
                                 : state == 6               ? tr(STR_QUEST_CAM_WORLD_SAFE)
                                                            : nullptr;
              if (variant) trace.watched[0] = hunt ? tr(STR_QUEST_RETURN_HUNT_ONLY) : tr(STR_QUEST_CAM_TITLE);
              toybox::Frame frame(trace, canvas.device, canvas.input, canvas.interactions);
              toybox::Screen screen(frame);
              questui::Work work;
              questui::build(screen, model, work);
              canvas.checkHits();
              CHECK(trace.watchedSeen[0] && trace.watchedFits[0]);
              if (trace.watched[1]) {
                if (!trace.watchedSeen[1] || !trace.watchedFits[1])
                  std::printf(
                      "Home title: state=%d language=%d width=%d notice=%d value=%s font=%d box=%dx%d line=%d\n", state,
                      int(language), size.first, notice, trace.watched[1], int(trace.watchedFont[1]),
                      trace.watchedBox[1].width, trace.watchedBox[1].height,
                      canvas.target.lineHeight(trace.watchedFont[1]));
                CHECK(trace.watchedSeen[1] && trace.watchedFits[1]);
              }
              CHECK(canvas.has(questui::RequestInfo, hero.activeRequest) == hunt);
              CHECK(canvas.has(questui::ResumeJourney) == !hunt);
              if (language == Language::ES && !notice && state < 4)
                canvas.write(std::string(argv[2]) + "/home-" + std::to_string(state) + "-" +
                             std::to_string(size.first) + "-" + std::to_string(variant) + ".pbm");
            }
          }
    }
    for (auto language : {Language::ES, Language::EN}) {
      I18N.setLanguage(language);
      for (unsigned level = 1; level <= quest::MAX_LEVEL; level += quest::LEVELS_PER_TITLE)
        for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
          quest::Profile hero;
          hero.xp = quest::threshold(level);
          std::snprintf(hero.name, sizeof(hero.name), "MILO");
          readingstats::Summary summary;
          questui::Model model;
          model.profile = &hero;
          model.summary = &summary;
          model.view = questui::View::Hero;
          Canvas canvas(size.first, size.second);
          NarrativeTrace trace(canvas.target);
          trace.watched[0] = hero.name;
          trace.watched[1] = questui::rankTitle(quest::level(hero.xp));
          toybox::Frame frame(trace, canvas.device, canvas.input, canvas.interactions);
          toybox::Screen screen(frame);
          questui::Work work;
          questui::build(screen, model, work);
          CHECK(trace.watchedSeen[0] && trace.watchedFits[0]);
          CHECK(trace.watchedSeen[1] && trace.watchedFits[1]);
          CHECK(trace.watchedFont[0] == trace.watchedFont[1]);
          CHECK(trace.watchedBox[0].bottom() <= trace.watchedBox[1].y);
        }
    }
    std::printf("Home card UI: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 2 && std::string(argv[1]) == "--combat-layout-preview") {
    sdart::Pack assets, enemies, rewards, outfits;
    CHECK(assets.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT));
    CHECK(enemies.open("resources/apps/reading-adventure/guardians.bin", questassets::GUARDIANS_COUNT));
    CHECK(rewards.open("resources/apps/reading-adventure/rewards.bin", questassets::REWARDS_COUNT));
    CHECK(outfits.open("resources/apps/reading-adventure/bestiary-outfits.bin", questassets::BESTIARY_OUTFITS_COUNT));
    for (auto language : {Language::ES, Language::EN}) {
      I18N.setLanguage(language);
      for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
        for (int notice = 0; notice < 3; ++notice) {
          for (int state = 0; state < 8; ++state) {
            quest::Profile hero;
            hero.campaign.initialized = hero.campaign.prologueSeen = true;
            hero.campaign.encounter = quest::battleAnchor(3);
            hero.campaign.introducedBooks = 1;
            hero.campaign.narratedEncounters = hero.campaign.encounter + 1;
            hero.campaign.phase = quest::BattlePhase::Reading;
            readingstats::Summary summary;
            questreturn::Report report;
            report.kind = state == 4   ? questreturn::Kind::Journey
                          : state == 5 ? questreturn::Kind::Banked
                          : state == 6 ? questreturn::Kind::Complete
                          : state == 7 ? questreturn::Kind::Logged
                                       : questreturn::Kind::Combat;
            report.seconds = 1200;
            report.xp = 260;
            report.target = 1200;
            report.before = 0;
            report.after = state == 1 ? 600 : 1200;
            report.victory = state == 2;
            if (state) hero.campaign.encounterSeconds = report.after;
            questui::Model model;
            model.profile = &hero;
            model.summary = &summary;
            model.assets = &assets;
            model.enemyAssets = &enemies;
            model.rewardAssets = &rewards;
            model.outfitAssets = &outfits;
            model.view = state == 0 ? questui::View::Campaign : questui::View::ReadingReturn;
            model.campaignTab = questui::CampaignTab::Encounter;
            model.readingReturn = &report;
            model.attackPhase = state == 3 ? questreturn::Phase::Swing : questreturn::Phase::Result;
            model.failed = notice == 1;
            model.denied = notice == 2;
            Canvas canvas(size.first, size.second);
            NarrativeTrace trace(canvas.target);
            trace.watchLast = true;
            char count[80], session[80];
            if (state <= 4) {
              const auto value = state == 0 ? 0u : state == 3 ? report.before : report.after;
              const auto target = state == 0 ? quest::encounterTarget(hero) : report.target;
              std::snprintf(count, sizeof(count), tr(STR_QUEST_CAM_BATTLE_MINUTE_PROGRESS),
                            static_cast<unsigned long long>(value / 60),
                            static_cast<unsigned long long>((target + 59) / 60));
              trace.watched[0] = count;
            } else {
              trace.watched[0] = state == 5   ? tr(STR_QUEST_CAM_RETURN_BANKED)
                                 : state == 6 ? tr(STR_QUEST_CAM_RETURN_PEACE)
                                              : tr(STR_QUEST_CAM_RETURN_LOGGED);
            }
            if (state) {
              std::snprintf(session, sizeof(session), tr(STR_QUEST_CAM_RETURN_TIME), 20ull);
              trace.watched[1] = session;
            } else if (!notice) {
              trace.watched[1] = tr(STR_QUEST_CAM_BATTLE_HINT);
            }
            toybox::Frame frame(trace, canvas.device, canvas.input, canvas.interactions);
            toybox::Screen screen(frame);
            questui::Work work;
            questui::build(screen, model, work);
            canvas.checkHits();
            CHECK(trace.watchedSeen[0] && trace.watchedFits[0]);
            if (state || !notice) CHECK(trace.watchedSeen[1] && trace.watchedFits[1]);
            if (state) CHECK(canvas.has(questui::DismissReadingReturn) == (state != 3));
            if (state && state != 3)
              for (size_t hit = 0; hit < canvas.interactions.count(); ++hit)
                if (canvas.interactions.data()[hit].action == questui::DismissReadingReturn && state <= 4)
                  CHECK(trace.watchedBox[0].bottom() < canvas.interactions.data()[hit].rect.y);
            if (language == Language::ES && !notice && state < 4)
              canvas.write(std::string(argv[2]) + "/combat-" + std::to_string(state) + "-" +
                           std::to_string(size.first) + ".pbm");
          }
        }
      }
    }
    std::printf("Combat layout UI: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 2 && std::string(argv[1]) == "--hunt-detail-preview") {
    sdart::Pack base, enemies, rewards, outfits;
    CHECK(base.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT));
    CHECK(enemies.open("resources/apps/reading-adventure/guardians.bin", questassets::GUARDIANS_COUNT));
    CHECK(rewards.open("resources/apps/reading-adventure/rewards.bin", questassets::REWARDS_COUNT));
    CHECK(outfits.open("resources/apps/reading-adventure/bestiary-outfits.bin", questassets::BESTIARY_OUTFITS_COUNT));
    for (auto language : {Language::ES, Language::EN}) {
      I18N.setLanguage(language);
      static constexpr int cases[] = {0, 302, 303, 304, 305, 306, 361};
      for (int n = 0; n < 7 + quest::kRequestCount; ++n) {
        const int id = n < 7 ? cases[n] : n - 7;
        for (int state = 0; state < 4; ++state) {
          if (n >= 7 && state) continue;
          quest::Profile hero;
          hero.campaign.initialized = hero.campaign.prologueSeen = true;
          hero.campaign.encounter = quest::battleAnchor(119);
          hero.campaign.phase = quest::BattlePhase::Complete;
          hero.campaign.introducedBooks = 12;
          hero.campaign.narratedEncounters = quest::CAMPAIGN_ENCOUNTERS;
          hero.inkHuntDiscovered = true;
          if (n >= 7 && !quest::requestUnlocked(hero, id)) continue;
          if (state) CHECK(quest::acceptRequest(hero, id));
          if (state == 1) hero.campaign.observedSeconds += 120;
          if (state >= 2) hero.campaign.observedSeconds += quest::requestMinutes(id) * 60;
          if (state == 3) CHECK(quest::collectRequest(hero, id));
          readingstats::Summary summary;
          questui::Model model;
          model.profile = &hero;
          model.summary = &summary;
          model.assets = &base;
          model.enemyAssets = &enemies;
          model.rewardAssets = &rewards;
          model.outfitAssets = &outfits;
          model.selectedRequest = id;
          model.view = questui::View::Request;
          questreturn::Report report;
          report.requestId = id;
          report.requestTarget = quest::requestTarget(hero, id);
          report.requestAfter = quest::requestValue(hero, id);
          model.readingReturn = &report;
          model.returnPanel = questreturn::Panel::Request;
          model.attackPhase = questreturn::Phase::Result;
          for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
            for (auto view : {questui::View::Request, questui::View::ReadingReturn}) {
              if (view == questui::View::ReadingReturn && state != 1 && state != 2) continue;
              model.view = view;
              for (int notice : {0, 1, 2}) {
                if (n >= 7 && notice) continue;
                model.failed = notice == 1;
                model.denied = notice == 2;
                Canvas canvas(size.first, size.second);
                NarrativeTrace trace(canvas.target);
                trace.watchLast = true;
                trace.watched[0] = questui::requestui::outfitName(id);
                const int extra = quest::huntExtraItem(id, 0), decoration = quest::requestDecoration(id);
                if (quest::visibleItem(extra))
                  trace.watched[1] = questui::requestui::extraName(extra);
                else if (quest::visibleItem(decoration))
                  trace.watched[1] = questui::requestui::rewardName(id);
                toybox::Frame frame(trace, canvas.device, canvas.input, canvas.interactions);
                toybox::Screen screen(frame);
                questui::Work work;
                questui::build(screen, model, work);
                canvas.checkHits();
                CHECK(trace.watchedSeen[0] && trace.watchedFits[0]);
                if (trace.watched[1]) CHECK(trace.watchedSeen[1] && trace.watchedFits[1]);
                if (!trace.watchedFits[0] || (trace.watched[1] && !trace.watchedFits[1]))
                  std::printf("Hunt text: id=%d lang=%d width=%d state=%d notice=%d view=%d\n", id, int(language),
                              size.first, state, notice, int(view));
                for (size_t hit = 0; hit < canvas.interactions.count(); ++hit) {
                  const auto& slot = canvas.interactions.data()[hit];
                  if (slot.action != questui::AcceptRequest && slot.action != questui::CollectRequest &&
                      slot.action != questui::CancelRequest && slot.action != questui::DismissReadingReturn &&
                      slot.action != questui::BackToRequests)
                    continue;
                  for (int name = 0; name < 2; ++name)
                    if (trace.watched[name]) CHECK(trace.watchedBox[name].bottom() <= slot.rect.y);
                }
                if (view == questui::View::ReadingReturn)
                  CHECK(canvas.has(questui::DismissReadingReturn));
                else
                  CHECK(canvas.has(state == 0   ? questui::AcceptRequest
                                   : state == 1 ? questui::CancelRequest
                                   : state == 2 ? questui::CollectRequest
                                                : questui::BackToRequests,
                                   state == 3 ? 0 : id));
                if (language == Language::ES && state == 0 && notice == 0 && (n < 7 || id == 359))
                  canvas.write(std::string(argv[2]) + "/detail-" + std::to_string(id) + "-" +
                               std::to_string(size.first) + ".pbm");
                if (language == Language::ES && state == 2 && view == questui::View::ReadingReturn && notice == 0)
                  canvas.write(std::string(argv[2]) + "/return-" + std::to_string(id) + "-" +
                               std::to_string(size.first) + ".pbm");
                if (language == Language::ES && id == 303 && state == 1 && notice == 1 && size.first == 800)
                  canvas.write(std::string(argv[2]) + "/error-" + std::to_string(int(view)) + ".pbm");
              }
            }
          }
        }
      }
      for (const auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
        quest::Profile hero;
        hero.campaign.initialized = hero.campaign.prologueSeen = true;
        hero.campaign.encounter = quest::battleAnchor(1);
        readingstats::Summary summary;
        questreturn::Report report;
        report.kind = questreturn::Kind::Combat;
        report.seconds = 88 * 60;
        report.xp = 479;
        report.after = report.target = 17 * 60;
        report.victory = true;
        questui::Model model;
        model.profile = &hero;
        model.summary = &summary;
        model.assets = &base;
        model.enemyAssets = &enemies;
        model.rewardAssets = &rewards;
        model.outfitAssets = &outfits;
        model.view = questui::View::ReadingReturn;
        model.readingReturn = &report;
        model.attackPhase = questreturn::Phase::Result;
        Canvas canvas(size.first, size.second);
        NarrativeTrace trace(canvas.target);
        trace.watched[0] = "17 min";
        toybox::Frame frame(trace, canvas.device, canvas.input, canvas.interactions);
        toybox::Screen screen(frame);
        questui::Work work;
        questui::build(screen, model, work);
        canvas.checkHits();
        CHECK(trace.watchedSeen[0] && trace.watchedFits[0] && trace.watchedFont[0] == toybox::kSmallFont);
        if (language == Language::ES)
          canvas.write(std::string(argv[2]) + "/minutes-" + std::to_string(size.first) + ".pbm");
      }
    }
    std::printf("Hunt detail UI: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 2 && std::string(argv[1]) == "--bestiary-outfits-preview") {
    I18N.setLanguage(Language::ES);
    sdart::Pack base, enemies, rewards, outfits, story;
    CHECK(base.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT));
    CHECK(enemies.open("resources/apps/reading-adventure/guardians.bin", questassets::GUARDIANS_COUNT));
    CHECK(rewards.open("resources/apps/reading-adventure/rewards.bin", questassets::REWARDS_COUNT));
    CHECK(outfits.open("resources/apps/reading-adventure/bestiary-outfits.bin", questassets::BESTIARY_OUTFITS_COUNT));
    CHECK(story.open("resources/apps/reading-adventure/story.bin", questassets::STORY_COUNT));
    quest::Profile hero;
    auto& c = hero.campaign;
    c.initialized = c.prologueSeen = true;
    c.encounter = quest::battleAnchor(119);
    c.phase = quest::BattlePhase::Complete;
    c.narratedEncounters = quest::CAMPAIGN_ENCOUNTERS;
    c.introducedBooks = 12;
    readingstats::Summary summary;
    questui::Model model;
    model.profile = &hero;
    model.summary = &summary;
    model.assets = &base;
    model.enemyAssets = &enemies;
    model.rewardAssets = &rewards;
    model.outfitAssets = &outfits;
    model.storyAssets = &story;
    for (bool corrupt : {false, true}) {
      sdart::Pack unavailable;
      if (corrupt) {
        CHECK(argc > 3);
        if (argc <= 3) return 1;
        CHECK(!unavailable.open(argv[3], questassets::BESTIARY_OUTFITS_COUNT));
      }
      model.outfitAssets = &unavailable;
      Canvas error(480, 800);
      questui::Work work;
      questui::build(error.screen, model, work);
      error.checkHits();
      CHECK(error.has(questui::Back));
      CHECK(!error.has(questui::OpenView));
      CHECK(!error.has(questui::Start));
    }
    model.outfitAssets = &outfits;
    for (int id = quest::kBestiaryHuntStart; id < quest::kRequestCount; ++id) {
      hero.requestClaims[id / 8] |= 1u << (id % 8);
      const int item = quest::huntOutfitItem(id);
      CHECK(quest::equip(hero, item));
      model.selectedRequest = id;
      for (const auto gender : {quest::Gender::Masculine, quest::Gender::Feminine}) {
        hero.gender = gender;
        const auto sprite = questextra::figure(base, &rewards, item, gender, &outfits);
        CHECK(sprite.pack == &outfits && sprite.ink == unsigned((id - quest::kBestiaryHuntStart) * 2 + int(gender)));
        CHECK(sprite.mask == sprite.ink + 112 && outfits.image(sprite.ink).height == 208);
        for (auto view : {questui::View::Gear, questui::View::Hero, questui::View::Request}) {
          if (view != questui::View::Gear && (id - quest::kBestiaryHuntStart) % 8) continue;
          model.view = view;
          model.preview = view == questui::View::Gear ? item : -1;
          for (const auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
            Canvas canvas(size.first, size.second);
            questui::Work work;
            questui::build(canvas.screen, model, work);
            canvas.checkHits();
            if ((id - quest::kBestiaryHuntStart) % 8 == 0)
              canvas.write(std::string(argv[2]) + "/outfit-" + std::to_string(id) + "-" + std::to_string(int(gender)) +
                           "-" + std::to_string(int(view)) + "-" + std::to_string(size.first) + ".pbm");
          }
        }
      }
    }
    std::printf("Original outfit UI: %d checks, %d failures; Pack=%zu bytes\n", checks, failures, sizeof(sdart::Pack));
    return failures ? 1 : 0;
  }
  if (argc > 2 && std::string(argv[1]) == "--bestiary-preview") {
    sdart::Pack base, enemies, rewards;
    CHECK(base.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT));
    CHECK(enemies.open("resources/apps/reading-adventure/guardians.bin", questassets::GUARDIANS_COUNT));
    CHECK(rewards.open("resources/apps/reading-adventure/rewards.bin", questassets::REWARDS_COUNT));
    quest::Profile hero;
    auto& c = hero.campaign;
    c.initialized = c.prologueSeen = true;
    c.encounter = quest::battleAnchor(119);
    c.phase = quest::BattlePhase::Complete;
    c.narratedEncounters = quest::CAMPAIGN_ENCOUNTERS;
    c.introducedBooks = 12;
    readingstats::Summary summary;
    questui::Model model;
    model.profile = &hero;
    model.summary = &summary;
    model.assets = &base;
    model.enemyAssets = &enemies;
    model.rewardAssets = &rewards;
    for (const auto language : {Language::ES, Language::EN}) {
      I18N.setLanguage(language);
      for (const auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
        for (int id = quest::kBestiaryHuntStart; id < quest::kRequestCount; ++id) {
          CHECK(quest::acceptRequest(hero, id));
          model.view = questui::View::Request;
          model.selectedRequest = id;
          Canvas canvas(size.first, size.second);
          questui::Work work;
          NarrativeTrace trace(canvas.target);
          char expected[80];
          const auto minutes = quest::requestMinutes(id);
          const bool fractional = minutes >= 180 && minutes % 60 != 0;
          if (fractional) {
            std::snprintf(expected, sizeof(expected), tr(STR_QUEST_CAM_ORDER_PROGRESS_MINUTES), 0u, minutes);
            trace.watched[0] = expected;
          }
          toybox::Frame frame(trace, canvas.device, canvas.input, canvas.interactions);
          toybox::Screen screen(frame);
          questui::build(screen, model, work);
          if (fractional) CHECK(trace.watchedSeen[0] && trace.watchedFits[0]);
          canvas.checkHits();
          CHECK(canvas.has(questui::CancelRequest, id));
          if (language == Language::ES && size.first == 480 && (id - quest::kBestiaryHuntStart) % 8 == 0)
            canvas.write(std::string(argv[2]) + "/hunt-" + std::to_string(id) + ".pbm");
          CHECK(quest::cancelRequest(hero, id));
        }
      }
    }
    std::printf("Bestiary UI: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 2 && std::string(argv[1]) == "--ranked-hunt-preview") {
    I18N.setLanguage(Language::ES);
    sdart::Pack base, enemies, rewards;
    CHECK(base.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT));
    CHECK(enemies.open("resources/apps/reading-adventure/guardians.bin", questassets::GUARDIANS_COUNT));
    CHECK(rewards.open("resources/apps/reading-adventure/rewards.bin", questassets::REWARDS_COUNT));
    quest::Profile hero;
    hero.campaign.initialized = hero.campaign.prologueSeen = true;
    hero.campaign.introducedBooks = 3;
    hero.campaign.encounter = quest::battleAnchor(25);
    hero.campaign.narratedEncounters = hero.campaign.encounter;
    hero.inkHuntDiscovered = true;
    hero.pausedInkSeconds = 7200;
    readingstats::Summary summary;
    questui::Model model;
    model.profile = &hero;
    model.summary = &summary;
    model.assets = &base;
    model.enemyAssets = &enemies;
    model.rewardAssets = &rewards;
    model.selectedRequest = quest::kInkHunt;
    for (const auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      for (auto view : {questui::View::Requests, questui::View::Request, questui::View::Gear}) {
        model.view = view;
        model.preview = view == questui::View::Gear ? quest::huntOutfitItem(quest::kInkHunt) : -1;
        Canvas canvas(size.first, size.second);
        questui::Work work;
        questui::build(canvas.screen, model, work);
        canvas.checkHits();
        const char* prefix = view == questui::View::Requests  ? "/board-"
                             : view == questui::View::Request ? "/secret-"
                                                              : "/outfit-";
        canvas.write(std::string(argv[2]) + prefix + std::to_string(size.first) + ".pbm");
      }
      for (const auto language : {Language::EN, Language::ES}) {
        I18N.setLanguage(language);
        CHECK(quest::acceptRequest(hero, 0));
        model.view = questui::View::RequestSwitchConfirm;
        model.selectedRequest = quest::kInkHunt;
        model.preview = -1;
        Canvas canvas(size.first, size.second);
        NarrativeTrace trace(canvas.target);
        trace.watched[0] = tr(STR_QUEST_HUNTS_RESUME_WARNING);
        toybox::Frame frame(trace, canvas.device, canvas.input, canvas.interactions);
        toybox::Screen screen(frame);
        questui::Work work;
        questui::build(screen, model, work);
        CHECK(trace.watchedSeen[0] && trace.watchedFits[0]);
        CHECK(quest::cancelRequest(hero, 0));
      }
      I18N.setLanguage(Language::ES);
    }
    return failures ? 1 : 0;
  }
  if (argc > 2 && std::string(argv[1]) == "--hunt-preview") {
    I18N.setLanguage(Language::ES);
    sdart::Pack base, enemies, rewards;
    CHECK(base.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT));
    CHECK(enemies.open("resources/apps/reading-adventure/guardians.bin", questassets::GUARDIANS_COUNT));
    CHECK(rewards.open("resources/apps/reading-adventure/rewards.bin", questassets::REWARDS_COUNT));
    quest::Profile hero;
    hero.campaign.initialized = hero.campaign.prologueSeen = true;
    hero.campaign.introducedBooks = 1;
    hero.campaign.encounter = quest::battleAnchor(4);
    hero.campaign.narratedEncounters = hero.campaign.encounter;
    CHECK(quest::acceptRequest(hero, 3));
    hero.campaign.observedSeconds += 7 * 60;
    readingstats::Summary summary;
    questui::Model model;
    model.profile = &hero;
    model.summary = &summary;
    model.assets = &base;
    model.enemyAssets = &enemies;
    model.rewardAssets = &rewards;
    model.selectedRequest = 3;
    for (const auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      questui::Work work;
      for (auto view : {questui::View::Requests, questui::View::Request}) {
        Canvas canvas(size.first, size.second);
        model.view = view;
        questui::build(canvas.screen, model, work);
        canvas.checkHits();
        CHECK(view == questui::View::Requests ? work.pages >= 1 : work.page == 0 && work.pages == 1);
        CHECK(view == questui::View::Requests ? canvas.has(questui::RequestInfo, 3)
                                              : canvas.has(questui::CancelRequest, 3));
        canvas.write(std::string(argv[2]) + (view == questui::View::Requests ? "/board-" : "/hunt-") +
                     std::to_string(size.first) + ".pbm");
      }
    }
    return failures ? 1 : 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--order-check") {
    checkOrderScreens(argc > 2 ? argv[2] : nullptr);
    std::printf("Order UI: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 3 && std::string(argv[1]) == "--reader-preview") {
    I18N.setLanguage(Language::ES);
    FontBridge proseFont{ui_prose_10};
    sdart::Pack base, story, rewards, enemies;
    CHECK(base.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT));
    CHECK(story.open("resources/apps/reading-adventure/story.bin", questassets::STORY_COUNT));
    CHECK(rewards.open((std::string(argv[3]) + "/rewards.bin").c_str(), questassets::REWARDS_COUNT));
    CHECK(enemies.open((std::string(argv[3]) + "/guardians.bin").c_str(), questassets::GUARDIANS_COUNT));
    quest::Profile hero;
    hero.campaign.prologueSeen = true;
    hero.campaign.introducedBooks = 12;
    hero.campaign.encounter = 719;
    hero.campaign.narratedEncounters = 720;
    hero.campaign.phase = quest::BattlePhase::Complete;
    readingstats::Summary summary;
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      const auto render = [&](questui::View view, int page, int encounter, const char* label) {
        Canvas c(size.first, size.second);
        c.target.setFont(4, proseFont.font);
        questui::Model model;
        questui::Work work;
        model.profile = &hero;
        model.summary = &summary;
        model.view = view;
        model.selectedBook = 0;
        model.selectedEncounter = encounter;
        model.introductionPage = page;
        model.storyPageCount = 150;
        model.storyPageDraft = 42;
        model.assets = &base;
        model.storyAssets = &story;
        model.rewardAssets = &rewards;
        model.enemyAssets = &enemies;
        questui::build(c.screen, model, work);
        c.checkHits();
        c.write(std::string(argv[2]) + "/" + label + "-" + std::to_string(size.first) + ".pbm");
      };
      render(questui::View::Book, 17, 0, "saga");
      render(questui::View::StoryPages, 0, 0, "selector");
      for (int step = 0; step < 6; ++step)
        render(questui::View::Lore, 0, step, ("path-" + std::to_string(step)).c_str());
    }
    std::printf("Reader previews: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--closed-replay-check") {
    for (auto language : {Language::EN, Language::ES}) {
      I18N.setLanguage(language);
      for (auto size : {std::pair<int, int>{480, 800}, {800, 480}})
        for (unsigned encounter : {0u, 6u, 60u, 714u}) {
          quest::Profile hero;
          hero.campaign.state = quest::AdventureState::Unfinished;
          hero.campaign.encounter = encounter;
          hero.campaign.narratedEncounters = encounter;
          hero.campaign.prologueSeen = true;
          hero.campaign.introducedBooks = encounter / 60 + 1;
          readingstats::Summary summary;
          questui::Model model;
          questui::Work work;
          model.profile = &hero;
          model.summary = &summary;
          model.view = questui::View::Lore;
          model.selectedEncounter = encounter;
          model.introductionPage = 1000;
          Canvas c(size.first, size.second);
          NarrativeTrace target(c.target);
          toybox::Frame frame(target, c.device, c.input, c.interactions);
          toybox::Screen screen(frame);
          questui::build(screen, model, work);
          c.checkHits();
          CHECK(work.pages == 1 && work.page == 0 && c.has(questui::ReturnPath));
          CHECK(!c.has(questui::BeginEncounter) && target.prose.empty());
        }
    }
    std::printf("Closed replay: %d checks, %d failures; unseen chapter entry is bounded, empty and returnable\n",
                checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--saga-check") {
    FontBridge proseFont{ui_prose_10};
    int minPages = 1000, maxPages = 0;
    for (auto language : {Language::EN, Language::ES}) {
      I18N.setLanguage(language);
      for (auto size : {std::pair<int, int>{480, 800}, {800, 480}})
        for (unsigned book = 0; book < 12; ++book)
          for (unsigned completed = 0; completed <= 10; ++completed) {
            quest::Profile hero;
            hero.campaign.prologueSeen = true;
            hero.campaign.introducedBooks = 12;
            hero.campaign.encounter = quest::battleAnchor(book * 10 + std::min(completed, 9u));
            hero.campaign.phase = completed == 10 ? quest::BattlePhase::Story : quest::BattlePhase::Reading;
            const unsigned version = completed == 10 ? 4 : completed / 3;
            std::string expected = book == 0 ? expectedPrologue() : "", actual;
            expected += I18N.get(questscene::SAGA[book][version]);
            int pages = 1;
            for (int page = 0; page < pages; ++page) {
              Canvas c(size.first, size.second);
              c.target.setFont(4, proseFont.font);
              NarrativeTrace target(c.target);
              toybox::Frame frame(target, c.device, c.input, c.interactions);
              toybox::Screen screen(frame);
              questui::Model model;
              questui::Work work;
              readingstats::Summary summary;
              model.profile = &hero;
              model.summary = &summary;
              model.view = questui::View::Book;
              model.selectedBook = book;
              model.introductionPage = page;
              questui::build(screen, model, work);
              pages = work.pages;
              CHECK(pages >= 1 && pages <= 20 && work.page == page);
              minPages = std::min(minPages, pages);
              maxPages = std::max(maxPages, pages);
              CHECK(!target.prose.empty());
              CHECK(!c.has(questui::BeginEncounter));
              CHECK(c.has(questui::StoryPageOpen) == (pages > 1));
              CHECK(c.has(questui::IntroductionPage, -1) == (page > 0));
              c.checkHits();
              actual += target.prose;
              if (page == pages / 2 || (book == 0 && page < questui::PROLOGUE_SECTIONS)) {
                const auto anchor = work.storyPosition;
                hero.sagaPositions[book] = anchor;
                model.introductionPage = -1;
                Canvas resumed(size.first, size.second);
                resumed.target.setFont(4, proseFont.font);
                NarrativeTrace resumedText(resumed.target);
                toybox::Frame resumedFrame(resumedText, resumed.device, resumed.input, resumed.interactions);
                toybox::Screen resumedScreen(resumedFrame);
                questui::Work resumedWork;
                questui::build(resumedScreen, model, resumedWork);
                CHECK(resumedWork.page == page && resumedWork.storyPosition == anchor);
                CHECK(resumedText.prose == target.prose);
                Canvas rotated(size.second, size.first);
                rotated.target.setFont(4, proseFont.font);
                questui::Work rotatedWork;
                questui::build(rotated.screen, model, rotatedWork);
                const int prefix = book == 0 ? questui::PROLOGUE_SECTIONS : 0;
                CHECK(questscene::decodeBookmark(rotatedWork.storyPosition, prefix, version).section ==
                      questscene::decodeBookmark(anchor, prefix, version).section);
                rotated.checkHits();
              }
            }
            CHECK(withoutWhitespace(actual) == withoutWhitespace(expected));
            for (unsigned oldSection : {1u, 12u, 119u}) {
              hero.sagaPositions[book] = uint16_t((oldSection << 8) | 255);
              Canvas c(size.first, size.second);
              c.target.setFont(4, proseFont.font);
              NarrativeTrace target(c.target);
              toybox::Frame frame(target, c.device, c.input, c.interactions);
              toybox::Screen screen(frame);
              questui::Model model;
              questui::Work work;
              readingstats::Summary summary;
              model.profile = &hero;
              model.summary = &summary;
              model.view = questui::View::Book;
              model.selectedBook = book;
              model.introductionPage = -1;
              questui::build(screen, model, work);
              CHECK(work.storyPosition == questscene::encodeBookmark(book == 0 ? questui::PROLOGUE_SECTIONS : 0, 0,
                                                                     book == 0 ? questui::PROLOGUE_SECTIONS : 0,
                                                                     version));
              CHECK(!target.prose.empty() && work.page < work.pages);
            }
          }
    }
    std::printf(
        "Saga milestones: %d checks, %d failures; all 12 sagas and every progress step, %d–%d pages, old bookmark "
        "anchors, EN/ES and both "
        "orientations\n",
        checks, failures, minPages, maxPages);
    return failures ? 1 : 0;
  }
  if (argc > 2 && std::string(argv[1]) == "--saga-reward-preview") {
    I18N.setLanguage(Language::ES);
    sdart::Pack base, enemies, rewards;
    CHECK(base.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT));
    CHECK(enemies.open("resources/apps/reading-adventure/guardians.bin", questassets::GUARDIANS_COUNT));
    CHECK(rewards.open("resources/apps/reading-adventure/rewards.bin", questassets::REWARDS_COUNT));
    quest::Profile hero;
    hero.campaign.initialized = hero.campaign.prologueSeen = true;
    hero.campaign.introducedBooks = 1;
    hero.campaign.encounter = quest::battleAnchor(9);
    hero.campaign.narratedEncounters = hero.campaign.encounter + 1;
    hero.campaign.phase = quest::BattlePhase::Story;
    hero.campaign.victoryCoins = 81;
    quest::restoreExpansionRewards(hero);
    readingstats::Summary summary;
    questui::Model model;
    model.profile = &hero;
    model.summary = &summary;
    model.assets = &base;
    model.enemyAssets = &enemies;
    model.rewardAssets = &rewards;
    model.view = questui::View::Campaign;
    model.campaignTab = questui::CampaignTab::Encounter;
    for (const auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      Canvas canvas(size.first, size.second);
      questui::Work work;
      questui::build(canvas.screen, model, work);
      canvas.checkHits();
      CHECK(canvas.has(questui::StoryContinue));
      canvas.write(std::string(argv[2]) + "/saga-" + std::to_string(size.first) + ".pbm");
    }
    return failures ? 1 : 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--victory-summary-check") {
    CHECK(questenemy::progressPose(2, 50) == questenemy::Pose::Ready);
    CHECK(questenemy::progressPose(25, 50) == questenemy::Pose::Weakened);
    CHECK(questenemy::progressPose(45, 50) == questenemy::Pose::Defeated);
    for (auto lang : {Language::EN, Language::ES}) {
      I18N.setLanguage(lang);
      for (const auto size : {std::pair<int, int>{480, 800}, {800, 480}})
        for (const auto phase : {questreturn::Phase::Raise, questreturn::Phase::Swing, questreturn::Phase::Impact,
                                 questreturn::Phase::Result}) {
          quest::Profile hero;
          readingstats::Summary summary;
          hero.campaign.prologueSeen = hero.campaign.initialized = true;
          hero.campaign.introducedBooks = 1;
          hero.campaign.encounter = quest::battleAnchor(0);
          hero.campaign.narratedEncounters = 6;
          hero.campaign.phase = quest::BattlePhase::Story;
          questreturn::Report report;
          report.kind = questreturn::Kind::Combat;
          report.victory = true;
          report.seconds = 1200;
          report.xp = 260;
          report.target = report.after = quest::encounterTarget(hero);
          questui::Model m;
          m.profile = &hero;
          m.summary = &summary;
          m.view = questui::View::ReadingReturn;
          m.readingReturn = &report;
          m.attackPhase = phase;
          m.returnPanel = questreturn::Panel::Reading;
          Canvas canvas(size.first, size.second);
          NarrativeTrace target(canvas.target);
          char counter[96];
          std::snprintf(counter, sizeof(counter), tr(STR_QUEST_CAM_BATTLE_MINUTE_PROGRESS),
                        static_cast<unsigned long long>(report.target / 60),
                        static_cast<unsigned long long>(report.target / 60));
          target.watched[0] = counter;
          char session[64], xp[64];
          std::snprintf(session, sizeof(session), tr(STR_QUEST_CAM_RETURN_TIME), 20ull);
          std::snprintf(xp, sizeof(xp), tr(STR_QUEST_CAM_RETURN_XP), 260ull);
          target.watched[1] = session;
          toybox::Frame frame(target, canvas.device, canvas.input, canvas.interactions);
          toybox::Screen screen(frame);
          questui::Work work;
          questui::build(screen, m, work);
          canvas.checkHits();
          CHECK(canvas.has(questui::DismissReadingReturn) == (phase == questreturn::Phase::Result));
          CHECK((target.chrome + target.prose).find(tr(STR_QUEST_CAM_RETURN_TITLE)) != std::string::npos);
          CHECK((target.chrome + target.prose).find(tr(STR_QUEST_CAM_BATTLE_VICTORY)) == std::string::npos);
          if (phase != questreturn::Phase::Result) continue;
          CHECK((target.chrome + target.prose).find(I18N.get(queststory::CHAPTERS[0])) == std::string::npos);
          CHECK(target.watchedSeen[0] && target.watchedFits[0]);
          CHECK(target.watchedSeen[1] && target.watchedFits[1]);
          CHECK((target.chrome + target.prose).find(xp) != std::string::npos);
          const int left = size.first > size.second ? size.first * 53 / 100 : 10, right = size.first - 10;
          int longest = 0;
          for (int y = 120; y < size.second - 64; ++y) {
            int run = 0;
            for (int x = left; x < right; ++x) {
              const bool black = !(canvas.pixels[y * ((size.first + 7) / 8) + x / 8] & (0x80u >> (x % 8)));
              run = black ? run + 1 : 0;
              longest = std::max(longest, run);
            }
          }
          CHECK(longest >= right - left - 16);
          CHECK(work.pages == 1 && !canvas.has(questui::IntroductionPage));
          Canvas victory(size.first, size.second);
          NarrativeTrace victoryTrace(victory.target);
          toybox::Frame victoryFrame(victoryTrace, victory.device, victory.input, victory.interactions);
          toybox::Screen victoryScreen(victoryFrame);
          m.returnPanel = questreturn::Panel::Victory;
          questui::build(victoryScreen, m, work);
          CHECK(victory.has(questui::DismissReadingReturn));
          CHECK((victoryTrace.prose + victoryTrace.chrome).find(questui::journeyTitle(hero.campaign.encounter)) !=
                std::string::npos);
        }
    }
    std::printf(
        "Victory summary: %d checks,%d failures;manual continuation, read/required counters and solid completed "
        "bars in all attack phases/locales/orientations\n",
        checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--path-gate-check") {
    FontBridge approachFont{ubuntu_12_regular};
    for (auto lang : {Language::EN, Language::ES}) {
      I18N.setLanguage(lang);
      for (const auto size : {std::pair<int, int>{480, 800}, {800, 480}})
        for (unsigned chapter = 0; chapter < 120; ++chapter) {
          quest::Profile hero;
          readingstats::Summary summary;
          hero.campaign.initialized = hero.campaign.prologueSeen = true;
          hero.campaign.encounter = quest::battleAnchor(chapter);
          hero.campaign.narratedEncounters = hero.campaign.encounter;
          hero.campaign.introducedBooks = chapter / 10;
          questui::Model m;
          m.profile = &hero;
          m.summary = &summary;
          m.view = questui::View::Campaign;
          m.campaignTab = questui::CampaignTab::Encounter;
          Canvas idle(size.first, size.second);
          questui::Work idleWork;
          questui::build(idle.screen, m, idleWork);
          idle.checkHits();
          CHECK(idleWork.pages > 0 && !idle.has(questui::BeginEncounter));
          m.introductionPage = idleWork.pages - 1;
          Canvas openingEnd(size.first, size.second);
          questui::build(openingEnd.screen, m, idleWork);
          CHECK(openingEnd.has(questui::FinishSagaIntro));
          CHECK(!idle.has(questui::BeginEncounter) && quest::beginCampaign(hero));
          m.introductionPage = 0;
          Canvas notStarted(size.first, size.second);
          questui::build(notStarted.screen, m, idleWork);
          CHECK(notStarted.has(questui::CampaignSection, int(questui::CampaignTab::Path)));
          CHECK(!notStarted.has(questui::BeginEncounter) && quest::needsEncounterStory(hero));
          m.campaignTab = questui::CampaignTab::Path;
          m.page = int(chapter % 10) / (size.first > size.second ? 4 : 5);
          Canvas paths(size.first, size.second);
          questui::Work pathWork;
          questui::build(paths.screen, m, pathWork);
          paths.checkHits();
          CHECK(!paths.has(questui::BeginEncounter, int(hero.campaign.encounter)));
          CHECK(paths.has(questui::EncounterInfo, int(hero.campaign.encounter)));
          CHECK(!paths.has(questui::EncounterInfo, int(hero.campaign.encounter + 6)));
          if (chapter) {
            CHECK(!paths.has(questui::EncounterInfo, int(hero.campaign.encounter - 6)));
            CHECK(!paths.has(questui::BeginEncounter, int(hero.campaign.encounter - 6)));
          }
          m.view = questui::View::Lore;
          m.selectedEncounter = hero.campaign.encounter;
          int pages = 1;
          std::string prose;
          for (int page = 0; page < pages; ++page) {
            Canvas context(size.first, size.second);
            context.target.setFont(0, approachFont.font);
            NarrativeTrace target(context.target);
            target.battleProse = true;
            toybox::Frame frame(target, context.device, context.input, context.interactions);
            toybox::Screen screen(frame);
            questui::Work work;
            m.introductionPage = page;
            questui::build(screen, m, work);
            pages = work.pages;
            CHECK(pages >= 1 && pages <= 2);
            CHECK(context.has(questui::BeginEncounter, int(hero.campaign.encounter)) == (page + 1 == pages));
            context.checkHits();
            prose += target.prose;
          }
          const char* expected = I18N.get(questscene::APPROACH[chapter]);
          CHECK(withoutWhitespace(prose) == withoutWhitespace(expected));
          // A transient migration save error can leave the old cursor one step short.
          const auto anchor = hero.campaign.encounter;
          --hero.campaign.encounter;
          hero.campaign.introducedBooks = chapter / 10 + 1;
          hero.campaign.narratedEncounters = anchor;
          CHECK(!quest::needsIntroduction(hero) && !quest::needsEncounterStory(hero));
          m.introductionPage = 0;
          m.failed = true;
          Canvas legacy(size.first, size.second);
          legacy.target.setFont(0, approachFont.font);
          questui::Work legacyWork;
          questui::build(legacy.screen, m, legacyWork);
          CHECK(legacyWork.pages <= 3);
          m.introductionPage = legacyWork.pages - 1;
          Canvas retry(size.first, size.second);
          retry.target.setFont(0, approachFont.font);
          questui::build(retry.screen, m, legacyWork);
          CHECK(retry.has(questui::BeginEncounter, anchor));
          m.failed = false;
          CHECK(quest::prepareBattle(hero) && !quest::needsEncounterStory(hero) && hero.campaign.encounter == anchor);
          m.view = questui::View::Campaign;
          m.campaignTab = questui::CampaignTab::Encounter;
          m.introductionPage = 0;
          Canvas fight(size.first, size.second);
          NarrativeTrace target(fight.target);
          target.watched[0] = tr(STR_QUEST_CAM_BATTLE_HINT);
          toybox::Frame frame(target, fight.device, fight.input, fight.interactions);
          toybox::Screen screen(frame);
          questui::Work work;
          questui::build(screen, m, work);
          fight.checkHits();
          CHECK(target.watchedSeen[0] && target.watchedFits[0]);
          CHECK((target.prose + target.chrome).find(I18N.get(questscene::APPROACH[chapter])) == std::string::npos);
        }
    }
    std::printf(
        "Path gate UI: %d checks,%d failures;120 locked encounters -> illustrated brief -> active battle, "
        "context and readable hint EN/ES/orientations\n",
        checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--continuous-flow-check") {
    for (auto language : {Language::EN, Language::ES}) {
      I18N.setLanguage(language);
      for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
        quest::Profile hero;
        readingstats::Summary summary;
        hero.campaign.initialized = hero.campaign.prologueSeen = true;
        CHECK(quest::prepareBattle(hero));
        questui::Model model;
        model.profile = &hero;
        model.summary = &summary;
        model.view = questui::View::Campaign;
        model.campaignTab = questui::CampaignTab::Encounter;
        for (unsigned chapter = 0; chapter < 120; ++chapter) {
          CHECK(hero.campaign.encounter == quest::battleAnchor(chapter));
          hero.campaign.encounterSeconds = quest::encounterTarget(hero);
          CHECK(quest::settleBattle(hero) && quest::collectBattleResult(hero));
          Canvas canvas(size.first, size.second);
          NarrativeTrace target(canvas.target);
          target.battleProse = true;
          toybox::Frame frame(target, canvas.device, canvas.input, canvas.interactions);
          toybox::Screen screen(frame);
          questui::Work work;
          questui::build(screen, model, work);
          CHECK(work.page == 0 && work.pages == 1);
          CHECK((target.prose + target.chrome).find(I18N.get(questscene::APPROACH[chapter])) == std::string::npos);
          CHECK(canvas.has(questui::StoryContinue));
          CHECK(!canvas.has(questui::BattleReward) && !canvas.has(questui::StoryReward));
          CHECK(!canvas.has(questui::IntroductionPage));
          canvas.checkHits();
          const auto bank = hero.campaign.bankSeconds;
          CHECK(quest::advanceBattle(hero));
          CHECK(chapter == 119 || (quest::needsEncounterStory(hero) && hero.campaign.encounterSeconds == 0 &&
                                   hero.campaign.bankSeconds == bank));
          if (chapter < 119) CHECK(quest::prepareBattle(hero));
        }
        CHECK(hero.campaign.phase == quest::BattlePhase::Complete);
      }
    }
    std::printf(
        "Live battle flow: %d checks, %d failures;120 compact victories, one next-enemy action, "
        "EN/ES/orientations\n",
        checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--brief-path-check") {
    FontBridge proseFont{ubuntu_10_regular};
    for (auto language : {Language::EN, Language::ES}) {
      I18N.setLanguage(language);
      for (const auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
        for (unsigned chapter = 0; chapter < 120; ++chapter) {
          quest::Profile hero;
          readingstats::Summary summary;
          hero.campaign.initialized = hero.campaign.prologueSeen = true;
          hero.campaign.introducedBooks = 12;
          hero.campaign.encounter = quest::battleAnchor(std::min(chapter + 1, 119u));
          hero.campaign.narratedEncounters = hero.campaign.encounter;
          if (chapter == 119) hero.campaign.phase = quest::BattlePhase::Complete;
          for (auto view : {questui::View::AtlasPlace}) {
            questui::Model model;
            model.profile = &hero;
            model.summary = &summary;
            model.view = view;
            model.selectedEncounter = quest::battleAnchor(chapter);
            std::string actual;
            questui::Work work;
            int pages = 1;
            for (int page = 0; page < pages; ++page) {
              Canvas canvas(size.first, size.second);
              canvas.target.setFont(4, proseFont.font);
              NarrativeTrace target(canvas.target);
              toybox::Frame frame(target, canvas.device, canvas.input, canvas.interactions);
              toybox::Screen screen(frame);
              model.introductionPage = page;
              questui::build(screen, model, work);
              pages = work.pages;
              CHECK(work.page == page && pages >= 1 && pages <= 3);
              CHECK(!canvas.has(questui::BeginEncounter));
              canvas.checkHits();
              actual += target.prose;
            }
            CHECK(actual.empty());
            CHECK(work.pages == 1);
          }
        }
      }
    }
    std::printf("Atlas victory details: %d checks, %d failures; 120 read-only records, EN/ES and both orientations\n",
                checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--page-picker-check") {
    for (auto language : {Language::EN, Language::ES}) {
      I18N.setLanguage(language);
      for (auto size : {std::pair<int, int>{480, 800}, {800, 480}})
        for (int draft : {0, 1, 81, 150, 151}) {
          quest::Profile hero;
          readingstats::Summary summary;
          questui::Model model;
          questui::Work work;
          model.profile = &hero;
          model.summary = &summary;
          model.view = questui::View::StoryPages;
          model.storyPageCount = 150;
          model.storyPageDraft = draft;
          Canvas c(size.first, size.second);
          questui::build(c.screen, model, work);
          c.checkHits();
          for (int digit = 0; digit < 10; ++digit) CHECK(c.has(questui::StoryPageDigit, digit));
          CHECK(c.has(questui::StoryPageErase) && c.has(questui::Back));
          CHECK(c.has(questui::StoryPageApply) == (draft > 0 && draft <= 150));
        }
    }
    std::printf("Page picker: %d checks, %d failures; EN/ES, both orientations and bounds\n", checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 3 && std::string(argv[1]) == "--polish-preview") {
    I18N.setLanguage(Language::ES);
    sdart::Pack base, story, rewards, enemies;
    CHECK(base.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT));
    CHECK(story.open("resources/apps/reading-adventure/story.bin", questassets::STORY_COUNT));
    CHECK(rewards.open((std::string(argv[3]) + "/rewards.bin").c_str(), questassets::REWARDS_COUNT));
    CHECK(enemies.open((std::string(argv[3]) + "/guardians.bin").c_str(), questassets::GUARDIANS_COUNT));
    quest::Profile hero;
    hero.xp = quest::threshold(100);
    hero.campaign.initialized = hero.campaign.prologueSeen = true;
    hero.campaign.encounter = 385;
    hero.campaign.narratedEncounters = 386;
    hero.campaign.introducedBooks = 7;
    hero.gender = quest::Gender::Feminine;
    std::snprintf(hero.name, sizeof(hero.name), "LUNA");
    quest::restoreStoryRewards(hero);
    for (int item : {quest::kKingBody, quest::kKingBackground, quest::kKingPet, quest::kKingDecoration, 216, 217})
      quest::grantItem(hero, item);
    readingstats::Summary summary;
    summary.anchorDay = 20000;
    summary.seconds = 8400;
    summary.dailySeconds[0] = 900;
    summary.dailySeconds[1] = 6300;
    FontBridge proseFont{ui_prose_10};
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      const auto render = [&](questui::View view, int preview, int page, const char* label) {
        Canvas c(size.first, size.second);
        c.target.setFont(4, proseFont.font);
        questui::Work work;
        questui::Model model;
        model.profile = &hero;
        model.summary = &summary;
        model.today = 20000;
        model.view = view;
        model.preview = preview;
        model.page = page;
        model.category = int(quest::Slot::Decoration);
        model.assets = &base;
        model.storyAssets = &story;
        model.rewardAssets = &rewards;
        model.enemyAssets = &enemies;
        questui::build(c.screen, model, work);
        c.checkHits();
        c.write(std::string(argv[2]) + "/" + label + "-" + std::to_string(size.first) + ".pbm");
      };
      render(questui::View::Gear, quest::kKingBody, 0, "royal");
      render(questui::View::Gear, 216, 0, "decoration");
      render(questui::View::Shop, 2, 0, "shop");
      render(questui::View::Missions, -1, 0, "missions");
      render(questui::View::Requests, -1, 38, "requests");
      render(questui::View::Requests, -1, 63, "needle");
      for (int item : {216, 217, quest::kKingDecoration}) {
        CHECK(quest::equip(hero, item));
        render(questui::View::Hero, -1, 0, ("home-" + std::to_string(item)).c_str());
      }
    }
    std::printf("Polish previews: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 3 && std::string(argv[1]) == "--expansion-preview") {
    I18N.setLanguage(Language::ES);
    sdart::Pack base, story, rewards, enemies;
    if (!base.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT) ||
        !story.open("resources/apps/reading-adventure/story.bin", questassets::STORY_COUNT) ||
        !rewards.open((std::string(argv[3]) + "/rewards.bin").c_str(), questassets::REWARDS_COUNT) ||
        !enemies.open((std::string(argv[3]) + "/guardians.bin").c_str(), questassets::GUARDIANS_COUNT))
      return 2;
    readingstats::Summary summary;
    quest::Profile hero;
    hero.campaign.initialized = true;
    hero.campaign.prologueSeen = true;
    hero.campaign.introducedBooks = 12;
    hero.campaign.encounter = 719;
    hero.campaign.narratedEncounters = 720;
    hero.campaign.phase = quest::BattlePhase::Complete;
    hero.campaign.state = quest::AdventureState::Peace;
    std::snprintf(hero.name, sizeof(hero.name), "LUNA");
    quest::restoreStoryRewards(hero);
    for (int item : {quest::kKingBody, quest::kKingBackground, quest::kKingPet, quest::kKingDecoration})
      CHECK(quest::equip(hero, item));
    CHECK(quest::grantItem(hero, 217) && quest::equip(hero, 217));
    toybox::HeaderStatus status;
    std::snprintf(status.clock, sizeof(status.clock), "13:00");
    for (auto gender : {quest::Gender::Masculine, quest::Gender::Feminine}) {
      hero.gender = gender;
      for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
        for (auto view : {questui::View::Ending, questui::View::Hero, questui::View::Request, questui::View::Gear}) {
          Canvas canvas(size.first, size.second, status);
          questui::Work work;
          questui::Model model;
          model.profile = &hero;
          model.summary = &summary;
          model.assets = &base;
          model.storyAssets = &story;
          model.rewardAssets = &rewards;
          model.enemyAssets = &enemies;
          model.view = view;
          model.selectedRequest = 0;
          model.category = int(quest::Slot::Decoration);
          model.page = 4;
          chrome::lightHeader = false;
          questui::build(canvas.screen, model, work);
          canvas.checkHits();
          canvas.write(std::string(argv[2]) + "/view-" + std::to_string(int(view)) + "-" + std::to_string(size.first) +
                       "-" + std::to_string(int(gender)) + ".pbm");
        }
      }
    }
    for (auto language : {Language::EN, Language::ES}) {
      I18N.setLanguage(language);
      for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
        int gifts = 0;
        for (unsigned chapter = 0; chapter < quest::CAMPAIGN_CHAPTERS; ++chapter) {
          const int item = quest::chapterDecoration(chapter);
          if (!quest::visibleItem(item)) continue;
          ++gifts;
          quest::Profile gift;
          gift.campaign.initialized = true;
          gift.campaign.prologueSeen = true;
          gift.campaign.encounter = chapter * 6 + 5;
          gift.campaign.narratedEncounters = gift.campaign.encounter + 1;
          gift.campaign.introducedBooks = chapter / 10 + 1;
          gift.campaign.phase = quest::BattlePhase::Story;
          gift.campaign.pendingReward = item;
          quest::grantItem(gift, item);
          Canvas canvas(size.first, size.second, status);
          NarrativeTrace words(canvas.target);
          toybox::Frame frame(words, canvas.device, canvas.input, canvas.interactions);
          toybox::Screen screen(frame);
          questui::Model model;
          questui::Work work;
          model.profile = &gift;
          model.summary = &summary;
          model.assets = &base;
          model.storyAssets = &story;
          model.rewardAssets = &rewards;
          model.enemyAssets = &enemies;
          model.view = questui::View::Campaign;
          model.campaignTab = questui::CampaignTab::Encounter;
          questui::build(screen, model, work);
          canvas.checkHits();
          CHECK(canvas.has(questui::StoryReward, 1) && canvas.has(questui::StoryReward, 0));
          const char* expected =
              quest::decorationPlace(item) == 0 ? tr(STR_QUEST_CAM_DECOR_UPPER) : tr(STR_QUEST_CAM_DECOR_GROUND);
          CHECK(words.chrome.find(expected) != std::string::npos);
        }
        CHECK(gifts == 50);
      }
    }
    std::printf("Expansion UI: %d checks, %d failures;all50 chapter discoveries in both languages/orientations\n",
                checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 2 && std::string(argv[1]) == "--request-atlas-preview") {
    I18N.setLanguage(Language::ES);
    sdart::Pack assets, story;
    if (!assets.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT) ||
        !story.open("resources/apps/reading-adventure/story.bin", questassets::STORY_COUNT))
      return 2;
    readingstats::Summary summary;
    quest::Profile hero;
    hero.campaign.initialized = true;
    hero.campaign.prologueSeen = true;
    hero.campaign.introducedBooks = 1;
    hero.campaign.encounter = 6;
    hero.campaign.narratedEncounters = 7;
    hero.campaign.observedSeconds = 10000;
    hero.gender = quest::Gender::Feminine;
    toybox::HeaderStatus status;
    std::snprintf(status.clock, sizeof(status.clock), "11:42");
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      for (auto view : {questui::View::Atlas, questui::View::AtlasRegion, questui::View::AtlasPlace,
                        questui::View::Requests, questui::View::Request, questui::View::Gear, questui::View::Shop}) {
        hero.campaign.introducedBooks = view == questui::View::Atlas ? 7 : 1;
        hero.campaign.encounter = view == questui::View::Atlas ? 390 : view == questui::View::AtlasRegion ? 24 : 6;
        Canvas canvas(size.first, size.second, status);
        questui::Model model;
        questui::Work work;
        model.profile = &hero;
        model.summary = &summary;
        model.assets = &assets;
        model.storyAssets = &story;
        model.view = view;
        model.selectedRequest = 0;
        model.selectedEncounter = 5;
        chrome::lightHeader = false;
        questui::build(canvas.screen, model, work);
        canvas.checkHits();
        canvas.write(std::string(argv[2]) + "/view-" + std::to_string(int(view)) + "-" + std::to_string(size.first) +
                     ".pbm");
      }
    }
    std::printf("Enhancement preview: %d checks, %d failures; Profile=%zu\n", checks, failures, sizeof(hero));
    return failures ? 1 : 0;
  }

  if (argc > 1 && std::string(argv[1]) == "--mission-check") {
    checkMissionScreens(argc > 2 ? argv[2] : nullptr);
    checkCardFocus();
    std::printf("Mission routing/header: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--journey-check") {
    checkJourneyPages();
    std::printf("Journey UI: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 2 && std::string(argv[1]) == "--journey-preview") {
    I18N.setLanguage(Language::ES);
    sdart::Pack assets, story;
    if (!assets.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT) ||
        !story.open("resources/apps/reading-adventure/story.bin", questassets::STORY_COUNT))
      return 2;
    quest::Profile hero;
    hero.campaign.initialized = true;
    hero.campaign.prologueSeen = true;
    hero.campaign.introducedBooks = 7;
    hero.campaign.encounter = 390;
    hero.campaign.narratedEncounters = 391;
    std::snprintf(hero.name, sizeof(hero.name), "Luna");
    hero.gender = quest::Gender::Feminine;
    readingstats::Summary summary;
    const std::pair<questui::View, const char*> views[] = {
        {questui::View::Atlas, "atlas"},      {questui::View::AtlasRegion, "region"},
        {questui::View::AtlasPlace, "place"}, {questui::View::Missions, "requests"},
        {questui::View::Request, "request"},  {questui::View::Thanks, "thanks"},
        {questui::View::Recap, "recap"},      {questui::View::Campaign, "activity"}};
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}})
      for (auto view : views) {
        Canvas canvas(size.first, size.second);
        questui::Work work;
        questui::Model model;
        model.profile = &hero;
        model.summary = &summary;
        model.assets = &assets;
        model.storyAssets = &story;
        model.view = view.first;
        model.selectedBook = 0;
        model.selectedEncounter = 17;
        model.selectedRequest = 150;
        model.today = 20000;
        model.selectedRequest = 0;
        model.campaignTab = questui::CampaignTab::Encounter;
        questui::build(canvas.screen, model, work);
        canvas.checkHits();
        canvas.write(std::string(argv[2]) + "/" + view.second + "-" + std::to_string(size.first) + ".pbm");
      }
    for (int step = 0; step < 6; ++step) {
      hero.campaign.encounter = 390 + step;
      hero.campaign.narratedEncounters = 391 + step;
      Canvas canvas(480, 800);
      questui::Work work;
      questui::Model model;
      model.profile = &hero;
      model.summary = &summary;
      model.assets = &assets;
      model.storyAssets = &story;
      model.view = questui::View::Campaign;
      model.campaignTab = questui::CampaignTab::Encounter;
      questui::build(canvas.screen, model, work);
      canvas.checkHits();
      canvas.write(std::string(argv[2]) + "/step-" + std::to_string(step) + ".pbm");
    }
    std::printf("Journey preview: %d checks, %d failures; Model=%zu Work=%zu Profile=%zu\n", checks, failures,
                sizeof(questui::Model), sizeof(questui::Work), sizeof(quest::Profile));
    return failures ? 1 : 0;
  }
  if (argc > 2 && std::string(argv[1]) == "--coherence-preview") {
    checkNarrativePages(argv[2]);
    std::printf("Narrative continuity: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }
  if (argc > 2 && std::string(argv[1]) == "--reward-preview") {
    I18N.setLanguage(Language::ES);
    sdart::Pack assets;
    if (!assets.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT)) return 1;
    quest::Profile hero;
    hero.xp = 50000;
    hero.noticedLevel = quest::level(hero.xp);
    std::strcpy(hero.name, "MILO");
    hero.campaign.prologueSeen = true;
    hero.campaign.introducedBooks = 1;
    hero.campaign.encounter = 11;
    hero.campaign.narratedEncounters = 12;
    hero.campaign.phase = quest::BattlePhase::Story;
    hero.campaign.pendingReward = 200;
    quest::grantItem(hero, 200);
    quest::grantItem(hero, 201);
    hero.decorations[0] = 200;
    hero.decorations[1] = 201;
    readingstats::Summary summary;
    questui::Model model;
    model.profile = &hero;
    model.summary = &summary;
    model.assets = &assets;
    model.view = questui::View::Campaign;
    model.campaignTab = questui::CampaignTab::Encounter;
    for (auto view : {questui::View::Campaign, questui::View::Hero, questui::View::Gear, questui::View::Shop}) {
      Canvas canvas(480, 800);
      questui::Work work;
      model.view = view;
      model.category = 3;
      questui::build(canvas.screen, model, work);
      canvas.checkHits();
      canvas.write(std::string(argv[2]) + "/reward-" + std::to_string(int(view)) + ".pbm");
    }
    for (auto view : {questui::View::Gear, questui::View::Shop}) {
      Canvas canvas(800, 480);
      questui::Work work;
      model.view = view;
      model.failed = true;
      questui::build(canvas.screen, model, work);
      canvas.checkHits();
      canvas.write(std::string(argv[2]) + "/compact-" + std::to_string(int(view)) + ".pbm");
    }
    return failures ? 1 : 0;
  }
  if (argc > 2 && std::string(argv[1]) == "--battle-preview") {
    I18N.setLanguage(Language::ES);
    sdart::Pack assets, story, enemies, rewards;
    CHECK(assets.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT));
    CHECK(story.open("resources/apps/reading-adventure/story.bin", questassets::STORY_COUNT));
    CHECK(enemies.open("resources/apps/reading-adventure/guardians.bin", questassets::GUARDIANS_COUNT));
    CHECK(rewards.open("resources/apps/reading-adventure/rewards.bin", questassets::REWARDS_COUNT));
    quest::Profile hero;
    hero.campaign.initialized = hero.campaign.prologueSeen = true;
    hero.campaign.encounter = quest::battleAnchor(0);
    hero.campaign.introducedBooks = 1;
    hero.campaign.narratedEncounters = hero.campaign.encounter + 1;
    hero.campaign.encounterSeconds = quest::encounterTarget(hero);
    hero.campaign.victoryCoins = 54;
    hero.campaign.phase = quest::BattlePhase::Victory;
    CHECK(quest::collectBattleResult(hero));
    questreturn::Report report;
    report.kind = questreturn::Kind::Combat;
    report.victory = true;
    report.seconds = 1200;
    report.xp = quest::battleXp(hero) + 60;
    report.after = report.target = quest::encounterTarget(hero);
    questui::Model model;
    readingstats::Summary summary;
    model.profile = &hero;
    model.summary = &summary;
    model.assets = &assets;
    model.storyAssets = &story;
    model.enemyAssets = &enemies;
    model.rewardAssets = &rewards;
    model.view = questui::View::ReadingReturn;
    model.readingReturn = &report;
    model.returnPanel = questreturn::Panel::Victory;
    for (const auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      Canvas canvas(size.first, size.second);
      questui::Work work;
      questui::build(canvas.screen, model, work);
      canvas.checkHits();
      canvas.write(std::string(argv[2]) + "/result-" + std::to_string(size.first) + ".pbm");
      std::printf("Result %dx%d: %d pages\n", size.first, size.second, work.pages);
      model.returnPanel = questreturn::Panel::Reading;
      model.attackPhase = questreturn::Phase::Result;
      Canvas held(size.first, size.second);
      questui::Work heldWork;
      questui::build(held.screen, model, heldWork);
      held.checkHits();
      held.write(std::string(argv[2]) + "/summary-" + std::to_string(size.first) + ".pbm");
      model.returnPanel = questreturn::Panel::Reading;
      hero.campaign.phase = quest::BattlePhase::Reading;
      hero.campaign.encounterSeconds = 0;
      model.view = questui::View::Campaign;
      model.campaignTab = questui::CampaignTab::Encounter;
      Canvas fight(size.first, size.second);
      questui::Work fightWork;
      questui::build(fight.screen, model, fightWork);
      fight.checkHits();
      fight.write(std::string(argv[2]) + "/fight-" + std::to_string(size.first) + ".pbm");
      hero.campaign.narratedEncounters = hero.campaign.encounter;
      model.view = questui::View::Lore;
      model.selectedEncounter = hero.campaign.encounter;
      Canvas context(size.first, size.second);
      questui::Work contextWork;
      questui::build(context.screen, model, contextWork);
      context.checkHits();
      context.write(std::string(argv[2]) + "/context-" + std::to_string(size.first) + ".pbm");
      model.view = questui::View::Campaign;
      hero.campaign.encounter = quest::battleAnchor(1);
      hero.campaign.narratedEncounters = hero.campaign.encounter + 1;
      model.campaignTab = questui::CampaignTab::Path;
      Canvas path(size.first, size.second);
      questui::Work pathWork;
      questui::build(path.screen, model, pathWork);
      path.checkHits();
      path.write(std::string(argv[2]) + "/path-" + std::to_string(size.first) + ".pbm");
      hero.campaign.encounter = quest::battleAnchor(0);
      hero.campaign.narratedEncounters = hero.campaign.encounter + 1;
      hero.campaign.phase = quest::BattlePhase::Story;
      model.view = questui::View::ReadingReturn;
      model.returnPanel = questreturn::Panel::Victory;
    }
    return failures ? 1 : 0;
  }
  if (argc > 2 && std::string(argv[1]) == "--reading-return-preview") {
    I18N.setLanguage(Language::ES);
    sdart::Pack assets;
    if (!assets.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT)) return 1;
    quest::Profile hero;
    hero.campaign.prologueSeen = true;
    hero.campaign.encounter = 365;
    hero.campaign.introducedBooks = 7;
    hero.campaign.narratedEncounters = 366;
    hero.equipped[0] = 59;
    hero.equipped[2] = 179;
    readingstats::Summary summary;
    questreturn::Report report;
    report.kind = questreturn::Kind::Combat;
    report.seconds = 1200;
    report.xp = 64;
    report.before = 600;
    report.after = 1800;
    report.target = quest::encounterTarget(hero);
    hero.campaign.encounterSeconds = report.after;
    questui::Model model;
    model.profile = &hero;
    model.summary = &summary;
    model.assets = &assets;
    model.view = questui::View::ReadingReturn;
    model.readingReturn = &report;
    model.attackPhase = questreturn::Phase::Impact;
    Canvas canvas(480, 800);
    questui::Work work;
    questui::build(canvas.screen, model, work);
    canvas.checkHits();
    canvas.write(std::string(argv[2]) + "/return.pbm");
    hero.campaign.encounter = 0;
    hero.campaign.introducedBooks = 0;
    hero.campaign.narratedEncounters = 0;
    model.view = questui::View::Campaign;
    model.campaignTab = questui::CampaignTab::Path;
    Canvas gate(480, 800);
    questui::Work gateWork;
    questui::build(gate.screen, model, gateWork);
    gate.checkHits();
    gate.write(std::string(argv[2]) + "/tall-scene.pbm");
    return failures ? 1 : 0;
  }
  if (argc > 2 && std::string(argv[1]) == "--saga-preview") {
    I18N.setLanguage(Language::ES);
    sdart::Pack assets;
    if (!assets.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT)) return 1;
    quest::Profile hero;
    hero.campaign.prologueSeen = true;
    std::snprintf(hero.name, sizeof(hero.name), "MILO");
    readingstats::Summary summary;
    questui::Model model;
    model.profile = &hero;
    model.summary = &summary;
    model.assets = &assets;
    model.view = questui::View::Campaign;
    model.campaignTab = questui::CampaignTab::Saga;
    Canvas canvas(480, 800);
    questui::Work work;
    questui::build(canvas.screen, model, work);
    canvas.checkHits();
    canvas.write(std::string(argv[2]) + "/saga.pbm");
    for (int book = 0; book < 12; ++book) {
      hero.campaign.encounter = book * 60;
      hero.campaign.introducedBooks = book;
      model.view = questui::View::Book;
      model.selectedBook = book;
      Canvas detail(480, 800);
      questui::Work bookWork;
      questui::build(detail.screen, model, bookWork);
      detail.checkHits();
      detail.write(std::string(argv[2]) + "/detail-" + std::to_string(book) + ".pbm");
    }
    return failures ? 1 : 0;
  }
  if (argc > 2 && std::string(argv[1]) == "--path-story-preview") {
    sdart::Pack assets, storyAssets;
    if (!assets.open("resources/apps/reading-adventure/graphics.bin", questassets::COUNT) ||
        !storyAssets.open("resources/apps/reading-adventure/story.bin", questassets::STORY_COUNT))
      return 1;
    quest::Profile hero;
    hero.campaign.prologueSeen = true;
    hero.campaign.introducedBooks = 12;
    std::snprintf(hero.name, sizeof(hero.name), "MILO");
    readingstats::Summary summary;
    questui::Model model;
    model.profile = &hero;
    model.summary = &summary;
    model.assets = &assets;
    model.storyAssets = &storyAssets;
    model.view = questui::View::Lore;
    {
      sdart::Pack missing;
      model.storyAssets = &missing;
      Canvas error(480, 800);
      questui::Work work;
      questui::build(error.screen, model, work);
      error.checkHits();
      CHECK(error.has(questui::Back));
      CHECK(!error.has(questui::BeginEncounter));
      model.storyAssets = &storyAssets;
    }
    for (auto language : {Language::EN, Language::ES}) {
      I18N.setLanguage(language);
      for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
        for (int book = 0; book < 12; ++book) {
          for (unsigned step = 0; step < quest::CHAPTERS_PER_BOOK; ++step) {
            for (bool completed : {false, true}) {
              model.selectedEncounter = quest::battleAnchor(book * quest::CHAPTERS_PER_BOOK + step);
              model.recalling = completed;
              hero.campaign.phase = quest::BattlePhase::Reading;
              hero.campaign.encounter = model.selectedEncounter + int(completed);
              hero.campaign.narratedEncounters = model.selectedEncounter;
              model.introductionPage = 0;
              int pages = 1;
              for (int page = 0; page < pages; ++page) {
                model.introductionPage = page;
                Canvas canvas(size.first, size.second);
                questui::Work work;
                questui::build(canvas.screen, model, work);
                canvas.checkHits();
                CHECK(assets.ready() && storyAssets.ready());
                CHECK(work.pages > 0 && work.pages < 64 && work.page == page);
                pages = work.pages;
                if (language == Language::ES && !completed && page == 0)
                  canvas.write(std::string(argv[2]) + "/scene-" + std::to_string(book) + "-" + std::to_string(step) +
                               "-" + std::to_string(size.first) + ".pbm");
              }
            }
          }
        }
      }
    }
    std::printf("Path story UI: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
  }
  const bool questOnly = argc > 1 && std::string(argv[1]) == "--quest";
  if (questOnly) {
    checkOrderScreens();
    checkNarrativePages();
  }
  for (auto language : {Language::EN, Language::ES}) {
    I18N.setLanguage(language);
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      quest::Profile p;
      readingstats::Summary summary;
      p.campaign.encounter = 11;
      p.campaign.phase = quest::BattlePhase::Story;
      p.campaign.prologueSeen = true;
      p.campaign.pendingReward = 200;
      quest::grantItem(p, 200);
      CHECK(quest::normalizeBattles(p));
      questui::Model model;
      model.profile = &p;
      model.summary = &summary;
      model.view = questui::View::Campaign;
      model.campaignTab = questui::CampaignTab::Encounter;
      Canvas reward(size.first, size.second);
      questui::Work rewardWork;
      questui::build(reward.screen, model, rewardWork);
      reward.checkHits();
      CHECK(!reward.has(questui::StoryReward, 1) && !reward.has(questui::StoryReward, 0));
      CHECK(reward.has(questui::StoryContinue) == (rewardWork.page + 1 == rewardWork.pages));
      CHECK(!reward.has(questui::BattleReward));
      model.view = questui::View::Shop;
      model.category = 3;
      model.preview = 201;
      Canvas locked(size.first, size.second);
      questui::Work lockedWork;
      questui::build(locked.screen, model, lockedWork);
      locked.checkHits();
      CHECK(!locked.has(questui::Outfit, 201));
    }
  }
  for (int pet = 0; pet < quest::kPetCount; ++pet) {
    const auto& ink = questart::pets[pet];
    const auto& mask = questart::petsMasks[pet];
    CHECK(ink.width == mask.width && ink.height == mask.height);
    CHECK(mask.width <= 280 && mask.height >= questartmeta::minimumPetHeight);
    CHECK(questart::petIcons[pet].width == 64 && questart::petIcons[pet].height == 80);
    CHECK(questart::petIconMasks[pet].width == 64 && questart::petIconMasks[pet].height == 80);
    flashbitmap::RowDecoder inkRows(ink), maskRows(mask);
    int first = mask.height, last = -1;
    bool insideMask = true;
    for (int y = 0; y < mask.height; ++y)
      for (int x = 0; x < (mask.width + 7) / 8; ++x) {
        const auto shape = maskRows.next(), dark = inkRows.next();
        if (dark & ~shape) insideMask = false;
        if (shape) {
          first = std::min(first, y);
          last = y;
        }
      }
    CHECK(insideMask);
    CHECK(last - first + 1 >= questartmeta::minimumPetHeight);
    CHECK(last == mask.height - 1);
  }
  for (auto language : {Language::EN, Language::ES}) {
    I18N.setLanguage(language);
    std::string previous;
    for (uint32_t n = 1; n <= quest::MAX_LEVEL; ++n) {
      const std::string title = questui::rankTitle(n);
      CHECK(!title.empty());
      if (n > 1) CHECK((title != previous) == (n <= quest::TITLE_COUNT * quest::LEVELS_PER_TITLE && (n - 1) % 5 == 0));
      previous = title;
    }
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      for (auto kind : {questreturn::Kind::Combat, questreturn::Kind::Banked, questreturn::Kind::Complete}) {
        for (auto phase : {questreturn::Phase::Raise, questreturn::Phase::Swing, questreturn::Phase::Impact,
                           questreturn::Phase::Result}) {
          Canvas c(size.first, size.second);
          quest::Profile hero;
          readingstats::Summary summary;
          questreturn::Report report;
          report.kind = kind;
          report.seconds = 1200;
          report.xp = 60;
          report.after = 1200;
          report.target = 6000;
          questui::Model model;
          model.profile = &hero;
          model.summary = &summary;
          model.view = questui::View::ReadingReturn;
          model.readingReturn = &report;
          model.attackPhase = phase;
          questui::Work work;
          questui::build(c.screen, model, work);
          c.checkHits();
          CHECK(c.has(questui::DismissReadingReturn) == (phase == questreturn::Phase::Result));
          CHECK(!c.has(questui::OpenView, int(questui::View::Settings)));
        }
      }
    }
    for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
      for (uint32_t n : {2u, 6u, 55u, 96u, 100u}) {
        Canvas c(size.first, size.second);
        quest::Profile p;
        p.xp = quest::threshold(n);
        p.noticedLevel = n - 1;
        readingstats::Summary summary;
        questui::Work work;

        questui::build(c.screen, {&p, &summary}, work);
        c.interactions.clear();
        questui::buildLevelUp(c.screen, p, false, work);
        CHECK(c.interactions.count() == 1 && c.has(questui::LevelContinue));
        CHECK(!c.has(questui::Tab) && !c.has(questui::Claim) && !c.has(questui::Gender));
        c.checkHits();
      }
    }
  }
  I18N.setLanguage(Language::EN);

  for (auto size : {std::pair<int, int>{480, 800}, {800, 480}}) {
    auto shot = [&](const Canvas& c, const std::string& name) {
      if (argc > 1 && !questOnly)
        c.write(std::string(argv[1]) + "/" + name + "-" + std::to_string(size.first) + ".pbm");
    };
    for (int pet = 0; pet < quest::kPetCount; ++pet)
      for (int gender = 0; gender < 2; ++gender) {
        Canvas c(size.first, size.second);
        quest::Profile profile;
        profile.gender = quest::Gender(gender);
        profile.equipped[int(quest::Slot::Body)] = gender ? 99 : 0;
        profile.equipped[int(quest::Slot::Pet)] = quest::first(quest::Slot::Pet) + pet;
        profile.equipped[int(quest::Slot::Background)] =
            gender ? quest::first(quest::Slot::Background) + 49 : quest::kGearCount;
        questui::Work work;

        questui::build(c.screen, {&profile}, work);
        c.checkHits();
      }
    if (!questOnly) {
      for (int k = 0; k < 3; ++k)
        for (int alt = 0; alt < 2; ++alt) {
          Canvas c(size.first, size.second);
          toolsui::Model m;
          toolsui::Work w;
          m.kind = static_cast<toolsui::Kind>(k);
          m.year = 2026;
          m.month = 10;
          m.day = 16;
          m.today = readingstats::dayIndex({2026, 10, 16});
          m.hour = 19;
          m.minute = 42;
          m.validTime = true;
          m.count = 13542;
          m.analog = alt;
          m.sunday = alt;

          toolsui::build(c.screen, m, w);
          c.checkHits();
          shot(c, "tool-" + std::to_string(k) + "-" + std::to_string(alt));
        }
    }
    quest::Profile p;
    std::strcpy(p.name, "MILO");
    p.xp = 4200;
    quest::equip(p, 1);
    quest::equip(p, 51);
    quest::equip(p, 101);
    readingstats::Summary summary;
    summary.anchorDay = readingstats::dayIndex({2026, 10, 16});
    for (int n = 19; n >= 0; --n) {
      readingstats::Record rec;
      rec.localDay = summary.anchorDay - n;
      rec.seconds = 2400;
      rec.firstCompletion = n % 7 == 0;
      summary.add(rec);
    }
    for (int tab = 0; tab < 3; ++tab) {
      Canvas c(size.first, size.second);
      questui::Work w;

      questui::build(c.screen,
                     {&p, &summary,
                      tab == 0   ? questui::View::Hero
                      : tab == 1 ? questui::View::Missions
                                 : questui::View::Shop,
                      summary.anchorDay},
                     w);
      c.checkHits();
      shot(c, "quest-" + std::to_string(tab));
    }
    for (auto view : {questui::View::Setup, questui::View::Name, questui::View::Profile})
      for (bool error : {false, true})
        for (const char* name : {"", "MILO", "PEÑA"}) {
          Canvas c(size.first, size.second);
          questui::Model model;
          model.profile = &p;
          model.summary = &summary;
          model.view = view;
          model.failed = error;
          model.chosenGender = 1;
          model.nameDraft = name;
          questui::Work work;
          questui::build(c.screen, model, work);
          c.checkHits();
          if (view == questui::View::Name) {
            for (int row = 0; row < 3; ++row)
              for (int n = 0; n < wordlekeys::length(row, work.keys.spanish); ++n) {
                const auto key = wordlekeys::keyRect(work.keys, row, n);
                CHECK(key.y >= 0 && key.bottom() <= size.second);
                CHECK(wordlekeys::keyAt(work.keys, key.x + key.width / 2, key.y + key.height / 2) ==
                      wordlekeys::ROWS[row][n]);
                fui::InputSnapshot tap;
                tap.touchReleased = true;
                tap.touchX = key.x + key.width / 2;
                tap.touchY = key.y + key.height / 2;
                const auto event = c.interactions.route(tap);
                CHECK(event.action == questui::NameKey && event.value == 0);
              }
          }
        }
    for (int category = 0; category < quest::kCategoryCount; ++category)
      for (int last = 0; last < 2; ++last) {
        Canvas c(size.first, size.second);
        questui::Work w;
        questui::Model model;
        model.profile = &p;
        model.summary = &summary;
        model.view = questui::View::Shop;
        model.category = category;
        model.page = last ? 999 : 0;

        questui::build(c.screen, model, w);
        c.checkHits();
        shot(c, "gear-" + std::to_string(category) + "-" + std::to_string(last));
      }
    for (auto view : {questui::View::Settings, questui::View::Campaign, questui::View::ReadingData, questui::View::Plan,
                      questui::View::EndConfirm, questui::View::Ending, questui::View::Book, questui::View::Lore,
                      questui::View::Info})
      for (auto phase : {quest::BattlePhase::Reading, quest::BattlePhase::Victory, quest::BattlePhase::Story,
                         quest::BattlePhase::Complete})
        for (bool failed : {false, true})
          for (auto campaignTab :
               {questui::CampaignTab::Encounter, questui::CampaignTab::Path, questui::CampaignTab::Saga}) {
            Canvas c(size.first, size.second);
            quest::Profile campaignProfile = p;
            campaignProfile.campaign.initialized = true;
            campaignProfile.campaign.phase = phase;
            campaignProfile.campaign.encounter = phase == quest::BattlePhase::Complete ? 719 : 14;
            campaignProfile.campaign.introducedBooks = campaignProfile.campaign.encounter / 60 + 1;
            campaignProfile.campaign.prologueSeen = true;
            campaignProfile.campaign.narratedEncounters = campaignProfile.campaign.encounter + 1;
            campaignProfile.campaign.state =
                phase == quest::BattlePhase::Complete ? quest::AdventureState::Peace : quest::AdventureState::Active;
            questui::Model model;
            model.profile = &campaignProfile;
            model.summary = &summary;
            model.today = summary.anchorDay;
            model.view = view;
            model.failed = failed;
            model.selectedBook = 11;
            model.campaignTab = campaignTab;
            questui::Work work;
            questui::build(c.screen, model, work);
            c.checkHits();
            CHECK(!c.has(questui::Read));
            if (view == questui::View::EndConfirm) CHECK(c.has(questui::Restart) && c.has(questui::Back));
            if (view == questui::View::Ending) CHECK(c.has(questui::EndingContinue) && !c.has(questui::Restart));
          }
    for (int book : {0})
      for (int page = 0; page < 12; ++page)
        for (bool writable : {true, false}) {
          Canvas c(size.first, size.second);
          quest::Profile fresh;
          fresh.campaign.encounter = book * 60;
          fresh.campaign.introducedBooks = book;
          questui::Model model;
          model.profile = &fresh;
          model.summary = &summary;
          model.view = questui::View::Campaign;
          model.writable = writable;
          model.introductionPage = page;
          questui::Work work;
          questui::build(c.screen, model, work);
          c.checkHits();
          const bool next = work.page + 1 < work.pages;
          CHECK(c.has(questui::FinishPrologue) == (!next && writable));
          CHECK(c.has(questui::IntroductionPage, 1) == next);
          CHECK(!c.has(questui::CampaignSection) && !c.has(questui::BattleReward));
          shot(c, "intro-" + std::to_string(book) + "-" + std::to_string(page));
        }
    for (int book : {0, 6, 11}) {
      quest::Profile journey;
      journey.campaign.prologueSeen = true;
      journey.campaign.encounter = book * 60;
      journey.campaign.narratedEncounters = book * 60;
      journey.campaign.introducedBooks = book;
      questui::Model model;
      model.profile = &journey;
      model.summary = &summary;
      model.view = questui::View::Campaign;
      model.campaignTab = questui::CampaignTab::Saga;
      {
        Canvas c(size.first, size.second);
        questui::Work work;
        questui::build(c.screen, model, work);
        c.checkHits();
        for (size_t n = 0; n < c.interactions.count(); ++n) {
          const auto hit = c.interactions.data()[n];
          if (hit.action == questui::SelectBook) CHECK(hit.value <= book);
        }
        if (book == 0) CHECK(c.has(questui::SelectBook, 0) && !c.has(questui::SelectBook, 1));
        shot(c, "saga-gate-" + std::to_string(book));
      }
      for (int page = 0; page < 12; ++page) {
        Canvas c(size.first, size.second);
        questui::Work work;
        model.view = questui::View::Book;
        model.selectedBook = book;
        model.introductionPage = page;
        questui::build(c.screen, model, work);
        c.checkHits();
        CHECK(c.has(questui::CampaignSection, int(questui::CampaignTab::Saga)) == (work.page + 1 == work.pages));
        CHECK(!c.has(questui::BeginEncounter));
        shot(c, "saga-detail-" + std::to_string(book) + "-" + std::to_string(page));
      }
      journey.campaign.introducedBooks = book + 1;
      model.view = questui::View::Campaign;
      model.campaignTab = questui::CampaignTab::Path;
      model.introductionPage = 0;
      {
        Canvas c(size.first, size.second);
        questui::Work work;
        questui::build(c.screen, model, work);
        c.checkHits();
        CHECK(c.has(questui::EncounterInfo, int(quest::battleAnchor(book * 10))));
        CHECK(!c.has(questui::EncounterInfo, int(quest::battleAnchor(book * 10 + 1))));
        shot(c, "guided-path-" + std::to_string(book));
      }
      for (int page = 0; page < 12; ++page) {
        Canvas c(size.first, size.second);
        questui::Work work;
        model.view = questui::View::Lore;
        model.selectedEncounter = book * 60;
        model.introductionPage = page;
        questui::build(c.screen, model, work);
        c.checkHits();
        CHECK(!c.has(questui::BeginEncounter));
        shot(c, "encounter-intro-" + std::to_string(book) + "-" + std::to_string(page));
      }
    }
    for (int notice = 0; notice < 3; ++notice) {
      Canvas c(size.first, size.second);
      questui::Work w;
      questui::Model model;
      model.profile = &p;
      model.summary = &summary;
      model.view = questui::View::Missions;
      model.failed = notice == 1;
      model.denied = notice == 2;

      questui::build(c.screen, model, w);
      c.checkHits();
      CHECK(w.pages == 1);
      CHECK(!c.has(questui::Milestone));
    }
    for (int category = 0; category < quest::kSlotCount; ++category) {
      quest::Profile buyer;
      buyer.xp = 10000000;
      questui::Model model;
      model.profile = &buyer;
      model.summary = &summary;
      model.view = questui::View::Shop;
      model.category = category;
      const auto slot = quest::Slot(category);
      const int purchase = quest::shopItem(buyer, slot, 0);
      auto checkCatalog = [&](bool inventory) {
        Canvas c(size.first, size.second);
        questui::Work w;
        model.view = inventory ? questui::View::Gear : questui::View::Shop;

        questui::build(c.screen, model, w);
        c.checkHits();
        for (size_t n = 0; n < c.interactions.count(); ++n) {
          const auto& hit = c.interactions.data()[n];
          if (hit.action == questui::Preview) CHECK(quest::owns(buyer, hit.value) == inventory);
        }
        return c.has(questui::Preview, purchase);
      };
      CHECK(checkCatalog(false));
      CHECK(quest::equip(buyer, purchase));
      CHECK(!checkCatalog(false));
      CHECK(checkCatalog(true));
      for (int index = 0; index < quest::count(slot); ++index) quest::equip(buyer, quest::catalogItem(slot, index));
      Canvas empty(size.first, size.second);
      questui::Work work;
      model.view = questui::View::Shop;

      questui::build(empty.screen, model, work);
      CHECK(work.pages == 1);
      for (size_t n = 0; n < empty.interactions.count(); ++n)
        CHECK(empty.interactions.data()[n].action != questui::Preview);
    }
    {
      quest::Profile claimed = p;
      claimed.claimedDay = summary.anchorDay;
      Canvas c(size.first, size.second);
      questui::Work w;
      questui::Model model;
      model.profile = &claimed;
      model.summary = &summary;
      model.view = questui::View::Missions;
      model.today = summary.anchorDay;
      model.questPage = 99999;

      questui::build(c.screen, model, w);
      c.checkHits();
      CHECK(!c.has(questui::Claim));
      CHECK(w.page + 1 == w.pages);
    }
    for (int category = 0; category < quest::kCategoryCount; ++category)
      for (int female = 0; female < 2; ++female) {
        Canvas c(size.first, size.second);
        questui::Work w;
        questui::Model model;
        p.gender = quest::Gender(female);
        model.profile = &p;
        model.summary = &summary;
        model.view = questui::View::Shop;
        model.preview = quest::catalogItem(quest::Slot(category), quest::count(quest::Slot(category)) - 1);

        questui::build(c.screen, model, w);
        c.checkHits();
        shot(c, "preview-" + std::to_string(category) + "-" + std::to_string(female));
      }
    if (!questOnly) {
      for (int n = 0; n < intercept::nodeCount; ++n) {
        Canvas c(size.first, size.second);
        intercept::State state;
        state.node = n;
        interceptui::Work w;

        interceptui::build(c.screen, {&state, interceptui::View::Story}, w);
        c.checkHits();
        if (n < 2) shot(c, "story-" + std::to_string(n));
      }
      for (auto view : {interceptui::View::Menu, interceptui::View::About, interceptui::View::Restart}) {
        Canvas c(size.first, size.second);
        intercept::State state;
        interceptui::Work w;

        interceptui::build(c.screen, {&state, view}, w);
        c.checkHits();
        shot(c, "story-view-" + std::to_string(int(view)));
      }
      {
        Canvas c(size.first, size.second);
        for (int type = 1; type <= 6; ++type)
          for (int dark = 0; dark < 2; ++dark)
            chessstyle::draw(c.target, fui::makeRect(10 + (type - 1) * 74, 10 + dark * 66, 64, 64), type, dark);
        shot(c, "pieces");
      }
    }
  }
  std::printf("%d UI checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
