#include <FreeInkUIDisplayTarget.h>

#include <array>
#include <cassert>
#include <cstdio>
#include <vector>

#include "../../src/apps_local/quest/QuestAvatar.h"
#include "../../src/apps_local/quest/QuestEnemyArt.h"

namespace fui = freeink::ui;
struct Surface final : fui::DrawTarget {
  int width, height;
  std::vector<uint8_t> pixels;
  std::vector<fui::Rect> characterMask;
  fui::DisplayTarget target;
  bool capture = false;
  unsigned fills = 0;
  Surface(int w, int h)
      : width(w),
        height(h),
        pixels((w + 7) / 8 * h, 255),
        target(pixels.data(), w, h, (w + 7) / 8, fui::Orientation::LandscapeCounterClockwise) {
    characterMask.reserve(4096);
  }
  void clear(bool collect = false) {
    std::fill(pixels.begin(), pixels.end(), 255);
    characterMask.clear();
    fills = 0;
    capture = collect;
  }
  bool black(int x, int y) const { return !(pixels[y * ((width + 7) / 8) + x / 8] & (0x80 >> (x % 8))); }
  fui::Rect clipRect() const override { return target.clipRect(); }
  bool setClipRect(fui::Rect r) override { return target.setClipRect(r); }
  fui::Size measureText(fui::FontId f, const char* t, fui::TextStyle s) const override {
    return target.measureText(f, t, s);
  }
  int16_t lineHeight(fui::FontId f) const override { return target.lineHeight(f); }
  fui::Rect inkBounds(fui::FontId f, const char* t, fui::TextStyle s) const override {
    return target.inkBounds(f, t, s);
  }
  void fill(fui::Rect r, fui::Paint p, uint8_t radius = 0, uint8_t corners = fui::CornersAll) override {
    if (capture && fills && p.color == fui::Color::White) characterMask.push_back(r);
    ++fills;
    target.fill(r, p, radius, corners);
  }
  void stroke(fui::Rect r, fui::Paint p, uint8_t w, uint8_t radius = 0, uint8_t corners = fui::CornersAll) override {
    target.stroke(r, p, w, radius, corners);
  }
  void line(fui::Point a, fui::Point b, uint8_t w, fui::Paint p) override { target.line(a, b, w, p); }
  void triangle(fui::Point a, fui::Point b, fui::Point c, fui::Paint p) override { target.triangle(a, b, c, p); }
  void text(fui::Rect r, const char* t, fui::TextStyle s) override { target.text(r, t, s); }
  void bitmap(fui::Rect r, fui::BitmapRef b, fui::BitmapMode m, fui::Paint p, fui::Rotation rotation) override {
    target.bitmap(r, b, m, p, rotation);
  }
};
int main(int argc, char** argv) {
  const char* folder = argc > 1 ? argv[1] : "resources/apps/reading-adventure";
  char path[512];
  sdart::Pack base, rewards, enemies, story;
  assert(base.open(questassets::PATH, questassets::COUNT));
  std::snprintf(path, sizeof(path), "%s/rewards.bin", folder);
  assert(rewards.open(path, questassets::REWARDS_COUNT));
  std::snprintf(path, sizeof(path), "%s/guardians.bin", folder);
  assert(enemies.open(path, questassets::GUARDIANS_COUNT));
  std::snprintf(path, sizeof(path), "%s/story.bin", folder);
  assert(story.open(path, questassets::STORY_COUNT));
  unsigned renders = 0;
  int leastVisible = 100000;
  for (auto size : {fui::Size{480, 320}, fui::Size{800, 480}}) {
    Surface baseline(size.width, size.height), decorated(size.width, size.height);
    const auto area = fui::makeRect(0, 0, size.width, size.height);
    for (int body : {0, 59, 99, quest::kKingBody})
      for (auto gender : {quest::Gender::Masculine, quest::Gender::Feminine}) {
        quest::Profile hero;
        hero.gender = gender;
        hero.requestClaims[quest::kInkHunt / 8] |= 1u << (quest::kInkHunt % 8);
        hero.equipped[0] = body;
        hero.equipped[1] = quest::kGearCount;
        hero.equipped[2] = body == quest::kKingBody ? quest::kKingPet : 199;
        baseline.clear(true);
        questavatar::draw(baseline, area, hero, base, -1, 0, questavatar::Framing::Contain, &rewards);
        assert(!baseline.characterMask.empty());
        for (int n = 0; n < quest::count(quest::Slot::Decoration); ++n) {
          const int item = quest::catalogItem(quest::Slot::Decoration, n);
          if (item != quest::kInkRelic) assert(quest::grantItem(hero, item));
          assert(quest::owns(hero, item));
          hero.decorations[0] = hero.decorations[1] = quest::kGearCount;
          hero.decorations[quest::decorationPlace(item)] = item;
          decorated.clear();
          questavatar::draw(decorated, area, hero, base, -1, 0, questavatar::Framing::Contain, &rewards);
          for (const auto r : baseline.characterMask)
            for (int y = r.y; y < r.bottom(); ++y)
              for (int x = r.x; x < r.right(); ++x) assert(baseline.black(x, y) == decorated.black(x, y));
          int visible = 0;
          for (int y = 0; y < size.height; ++y)
            for (int x = 0; x < size.width; ++x) visible += !baseline.black(x, y) && decorated.black(x, y);
          leastVisible = std::min(leastVisible, visible);
          assert(visible >= 40);
          assert(base.ready() && rewards.ready());
          ++renders;
        }
      }
  }
  bool seen[73]{};
  Surface art(480, 320);
  const auto area = fui::makeRect(0, 0, 480, 320);
  std::array<uint64_t, quest::CAMPAIGN_CHAPTERS> hashes{};
  for (unsigned chapter = 0; chapter < quest::CAMPAIGN_CHAPTERS; ++chapter) {
    const unsigned encounter = quest::battleAnchor(chapter);
    const auto family = questenemy::family(encounter);
    assert(family < 73);
    seen[family] = true;
    for (auto pose : {questenemy::Pose::Ready, questenemy::Pose::Weakened, questenemy::Pose::Defeated}) {
      quest::Profile hero;
      hero.campaign.encounter = encounter;
      art.clear();
      questenemy::battle(art, area, base, hero, pose, questavatar::Framing::Contain, &enemies, &rewards);
      assert(base.ready() && enemies.ready());
    }
    art.clear();
    assert(story.image(chapter * 2).width == 240 && story.image(chapter * 2 + 1).height == 320);
    questavatar::storyBackground(art, area, story, chapter * 2);
    assert(story.ready());
    uint64_t hash = 14695981039346656037ull;
    for (const auto value : art.pixels) hash = (hash ^ value) * 1099511628211ull;
    for (unsigned old = 0; old < chapter; ++old) assert(hashes[old] != hash);
    hashes[chapter] = hash;
  }
  for (bool found : seen) assert(found);
  std::printf(
      "Expansion scenes: %u decoration combinations preserve every hero/PET mask pixel; minimum %d visible black "
      "pixels; 73 guardian designs, 360 path poses and 120 unique story illustrations passed\n",
      renders, leastVisible);
}
