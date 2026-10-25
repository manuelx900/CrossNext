#include <FreeInkUIDisplayTarget.h>
#include <I18n.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../../assets_local/quest/encoded/QuestArt.h"
#include "../../src/apps_local/chess/ChessStyle.h"
#include "../../src/apps_local/intercept/InterceptScreens.h"
#include "../../src/apps_local/quest/QuestAvatar.h"
#include "../../src/apps_local/quest/QuestScreens.h"
#include "../../src/apps_local/tools/ToolsScreens.h"
#include "../../src/apps_local/ui/PixelMenu.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../src/apps_local/ui/fonts/toybox_10.h"
#include "../../src/apps_local/ui/fonts/toybox_14.h"
#include "../../src/apps_local/ui/fonts/toybox_20.h"
#include "../../src/apps_local/ui/fonts/toybox_30.h"
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

  Canvas(int w, int h)
      : width(w),
        height(h),
        pixels((w + 7) / 8 * h, 255),
        target(pixels.data(), w, h, (w + 7) / 8, fui::Orientation::LandscapeCounterClockwise),
        device(makeDevice(target)),
        frame(target, device, input, interactions),
        screen(frame) {
    target.setFont(3, tiny.font);
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

}  // namespace
int main(int argc, char** argv) {
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
    Canvas canvas(480, 800);
    fui::TextStyle style;
    style.font = 3;
    for (int q = 0; q < quest::kQuestCount; ++q) {
      char label[48];
      std::snprintf(label, sizeof(label), tr(STR_QUEST_CLAIM_REWARD), quest::questReward(q));
      pixelui::copyCaps(label, sizeof(label), label);
      CHECK(canvas.target.measureText(style.font, label, style).width <= 104);
    }
  }
  for (auto language : {Language::EN, Language::ES}) {
    I18N.setLanguage(language);
    std::string previous;
    for (uint32_t n = 1; n <= quest::MAX_LEVEL; ++n) {
      const std::string title = questui::rankTitle(n);
      CHECK(!title.empty());
      if (n > 1) CHECK((title != previous) == ((n - 1) % 5 == 0));
      previous = title;
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
      if (argc > 1) c.write(std::string(argv[1]) + "/" + name + "-" + std::to_string(size.first) + ".pbm");
    };
    for (int pet = 0; pet < quest::kPetCount; ++pet)
      for (int gender = 0; gender < 2; ++gender) {
        Canvas c(size.first, size.second);
        quest::Profile profile;
        profile.gender = quest::Gender(gender);
        profile.equipped[int(quest::Slot::Body)] = gender ? 99 : 0;
        profile.equipped[int(quest::Slot::Pet)] = quest::first(quest::Slot::Pet) + pet;
        profile.equipped[int(quest::Slot::Aura)] = gender ? quest::first(quest::Slot::Aura) + 49 : quest::kGearCount;
        questui::Work work;
        questui::build(c.screen, {&profile}, work);
        c.checkHits();
      }
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
    quest::Profile p;
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
      questui::build(c.screen, {&p, &summary, tab, summary.anchorDay}, w);
      c.checkHits();
      shot(c, "quest-" + std::to_string(tab));
    }
    for (int category = 0; category < quest::kSlotCount; ++category)
      for (int last = 0; last < 2; ++last) {
        Canvas c(size.first, size.second);
        questui::Work w;
        questui::Model model;
        model.profile = &p;
        model.summary = &summary;
        model.tab = 2;
        model.category = category;
        model.page = last ? 999 : 0;
        questui::build(c.screen, model, w);
        c.checkHits();
        shot(c, "gear-" + std::to_string(category) + "-" + std::to_string(last));
      }
    for (int notice = 0; notice < 3; ++notice) {
      Canvas c(size.first, size.second);
      questui::Work w;
      questui::Model model;
      model.profile = &p;
      model.summary = &summary;
      model.tab = 1;
      model.failed = notice == 1;
      model.denied = notice == 2;
      questui::build(c.screen, model, w);
      c.checkHits();
      CHECK(w.pages == (size.first < size.second ? 375 : 1500));
      CHECK(c.has(questui::Milestone, 0));
    }
    for (int category = 0; category < quest::kSlotCount; ++category) {
      quest::Profile buyer;
      buyer.xp = 10000000;
      questui::Model model;
      model.profile = &buyer;
      model.summary = &summary;
      model.tab = 2;
      model.category = category;
      const auto slot = quest::Slot(category);
      const int purchase = quest::shopItem(buyer, slot, 0);
      auto checkCatalog = [&](bool inventory) {
        Canvas c(size.first, size.second);
        questui::Work w;
        model.ownedGear = inventory;
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
      for (int item = quest::first(slot); item < quest::first(slot) + quest::count(slot); ++item)
        quest::equip(buyer, item);
      Canvas empty(size.first, size.second);
      questui::Work work;
      model.ownedGear = false;
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
      model.tab = 1;
      model.today = summary.anchorDay;
      model.questPage = 99999;
      questui::build(c.screen, model, w);
      c.checkHits();
      CHECK(!c.has(questui::Claim));
      CHECK(w.page + 1 == w.pages);
    }
    for (int category = 0; category < quest::kSlotCount; ++category)
      for (int female = 0; female < 2; ++female) {
        Canvas c(size.first, size.second);
        questui::Work w;
        questui::Model model;
        p.gender = quest::Gender(female);
        model.profile = &p;
        model.summary = &summary;
        model.tab = 2;
        model.preview = quest::first(quest::Slot(category)) + quest::count(quest::Slot(category)) - 1;
        questui::build(c.screen, model, w);
        c.checkHits();
        shot(c, "preview-" + std::to_string(category) + "-" + std::to_string(female));
      }
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
  std::printf("%d UI checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
