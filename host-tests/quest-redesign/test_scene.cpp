#include <cassert>
#include <cstdio>
#include <tuple>
#include <vector>

#include "../../src/apps_local/quest/QuestAvatar.h"
#include "../../src/apps_local/quest/QuestEnemyArt.h"
#include "../../src/apps_local/quest/QuestReadingReturnArt.h"

namespace fui = freeink::ui;
struct Trace final : fui::DrawTarget {
  using Bounds = std::tuple<int, int, int, int>;
  std::vector<Bounds> whites;
  fui::Rect clip = fui::makeRect(0, 0, 1000, 1000), area;
  unsigned fills = 0, letters = 0, halos = 0;
  Trace() { whites.reserve(4096); }
  void reset(fui::Rect value) {
    area = value;
    whites.clear();
    fills = letters = halos = 0;
  }
  fui::Rect clipRect() const override { return clip; }
  bool setClipRect(fui::Rect value) override {
    clip = value;
    return true;
  }
  fui::Size measureText(fui::FontId, const char*, fui::TextStyle) const override { return {}; }
  int16_t lineHeight(fui::FontId) const override { return 20; }
  fui::Rect inkBounds(fui::FontId, const char*, fui::TextStyle) const override { return {}; }
  void fill(fui::Rect r, fui::Paint p, uint8_t = 0, uint8_t = fui::CornersAll) override {
    r = fui::makeRect(std::max(r.x, clip.x), std::max(r.y, clip.y),
                      std::min(r.right(), clip.right()) - std::max(r.x, clip.x),
                      std::min(r.bottom(), clip.bottom()) - std::max(r.y, clip.y));
    if (r.empty()) return;
    assert(r.x >= area.x && r.y >= area.y && r.right() <= area.right() && r.bottom() <= area.bottom());
    if (fills++ && p.color == fui::Color::White) whites.emplace_back(r.x, r.y, r.width, r.height);
  }
  void stroke(fui::Rect, fui::Paint, uint8_t, uint8_t = 0, uint8_t = fui::CornersAll) override {}
  void line(fui::Point, fui::Point, uint8_t, fui::Paint) override {}
  void triangle(fui::Point, fui::Point, fui::Point, fui::Paint) override {}
  void text(fui::Rect, const char* glyph, fui::TextStyle style) override {
    assert(glyph[0] >= 'A' && glyph[0] <= 'Z' && !glyph[1]);
    if (style.color == fui::Color::Black)
      ++letters;
    else if (style.color == fui::Color::White)
      ++halos;
  }
  void bitmap(fui::Rect, fui::BitmapRef, fui::BitmapMode, fui::Paint, fui::Rotation) override {}
};
int main() {
  sdart::Pack art;
  assert(art.open(questassets::PATH, questassets::COUNT));
  assert(questreturn::monsterPose(questreturn::Phase::Raise) == questenemy::Pose::Ready);
  assert(questreturn::monsterPose(questreturn::Phase::Swing) == questenemy::Pose::Weakened);
  assert(questreturn::monsterPose(questreturn::Phase::Impact) == questenemy::Pose::Defeated);
  assert(questreturn::monsterPose(questreturn::Phase::Result) == questenemy::Pose::Current);
  Trace trace;
  for (int figure = 0; figure < 200; ++figure)
    for (int companion : {6, 47, 49, -1}) {
      const auto hero = art.image(questassets::FIGURES + figure);
      (void)companion;
      const auto layout = questenemy::arrange(hero);
      assert(layout.hero.height == 232 && layout.hero.bottom() == 300);
      assert(layout.enemy.x >= layout.hero.right() + 12 && layout.enemy.width >= 160 && layout.enemy.right() == 480);
      assert(layout.enemy.y + layout.enemy.height * 274 / 280 == 300);
    }
  for (int body : {0, 59, 99})
    for (int sex : {0, 1})
      for (int companion : {6, 47, 49}) {
        const auto area = fui::makeRect(10, 150, 460, 500);
        quest::Profile p;
        p.equipped[0] = body;
        p.gender = quest::Gender(sex);
        p.equipped[2] = 150 + companion;
        const auto previous = trace.clipRect();
        trace.reset(area);
        questavatar::draw(trace, area, p, art, -1, questassets::SAGA_SCENES, questavatar::Framing::Tall);
        assert(art.ready() && trace.clipRect().x == previous.x && trace.clipRect().width == previous.width);
        trace.reset(area);
        questenemy::battle(trace, area, art, p, questenemy::Pose::Current, questavatar::Framing::Tall);
        for (const auto phase : {questreturn::Phase::Raise, questreturn::Phase::Swing, questreturn::Phase::Impact}) {
          trace.reset(area);
          questreturn::letters(trace, area, art, p, phase, 42, questavatar::Framing::Tall);
          assert(art.ready() && trace.letters == 6 && trace.halos == 48 && !trace.fills);
        }

        assert(art.ready() && trace.clipRect().y == previous.y && trace.clipRect().height == previous.height);
      }
  for (const auto area : {fui::makeRect(10, 200, 460, 320), fui::makeRect(20, 90, 430, 290)}) {
    for (const int pet : {0, 3, 5, 40, 47, 49}) {
      quest::Profile p;
      p.equipped[int(quest::Slot::Pet)] = quest::first(quest::Slot::Pet) + pet;
      for (int gender = 0; gender < 2; ++gender) {
        p.gender = quest::Gender(gender);
        trace.reset(area);
        questavatar::draw(trace, area, p, art);
        assert(art.ready() && !trace.whites.empty());
        const auto reference = trace.whites;
        for (int bg = 0; bg < quest::kBackgroundCount; ++bg) {
          p.equipped[int(quest::Slot::Background)] = quest::first(quest::Slot::Background) + bg;
          trace.reset(area);
          questavatar::draw(trace, area, p, art);
          assert(art.ready() && trace.whites == reference);
        }
      }
    }
    for (int body = 0; body < 200; ++body) {
      quest::Profile p;
      p.equipped[int(quest::Slot::Body)] = body / 2;
      p.gender = quest::Gender(body % 2);
      p.equipped[int(quest::Slot::Pet)] = 199;
      trace.reset(area);
      questavatar::draw(trace, area, p, art);
      assert(art.ready() && !trace.whites.empty());
    }
  }
  art.close();
  assert(!art.open(questassets::PATH, 900));
  std::puts(
      "Quest scenes: all 50 backgrounds preserve foreground placement; 200 heroes and largest pets fit both viewports; "
      "legacy pack rejected");
}
