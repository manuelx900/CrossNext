#pragma once

#include "QuestAvatar.h"
#include "QuestCampaign.h"
#include "QuestHuntBounds.h"

namespace questenemy {
namespace fui = freeink::ui;
enum class Pose : uint8_t { Ready, Weakened, Defeated, Current };
inline unsigned family(unsigned encounter) { return std::min(quest::guardian(encounter), 72u); }
inline Pose progressPose(uint32_t progress, uint32_t target) {
  if (target && uint64_t(progress) * 10 >= uint64_t(target) * 9) return Pose::Defeated;
  return target && uint64_t(progress) * 2 >= target ? Pose::Weakened : Pose::Ready;
}
inline unsigned state(const quest::Profile& p) {
  return unsigned(p.campaign.phase == quest::BattlePhase::Reading
                      ? progressPose(p.campaign.encounterSeconds, quest::encounterTarget(p))
                      : Pose::Defeated);
}
inline questextra::Sprite sprite(sdart::Pack& assets, sdart::Pack* guardians, unsigned selected, unsigned frame) {
  const unsigned logical = family(selected);
  const int code = logical == 72 ? -13 : questexp::ENEMIES[logical];
  if (code >= 0 && guardians) {
    const unsigned image = unsigned(code) * 3 + frame;
    return {guardians, image, 180 + image};
  }
  const unsigned image = (code < 0 ? unsigned(-1 - code) : std::min(11u, selected / 60)) * 3 + frame;
  return {&assets, questassets::ENEMIES + image, questassets::ENEMY_MASKS + image};
}
inline void paint(fui::DrawTarget& target, fui::Rect area, sdart::Pack& assets, const quest::Profile& p,
                  Pose pose = Pose::Current, sdart::Pack* guardians = nullptr, int encounter = -1) {
  const unsigned selected = encounter >= 0 ? unsigned(encounter) : p.campaign.encounter;
  const unsigned frame = pose == Pose::Current ? state(p) : unsigned(pose);
  questextra::paint(target, area, sprite(assets, guardians, selected, frame));
}
inline void thumbnail(fui::DrawTarget& target, fui::Rect area, sdart::Pack& assets, sdart::Pack* guardians,
                      unsigned encounter, Pose pose) {
  if (area.empty() || pose == Pose::Current) return;
  constexpr unsigned baseFrames = questassets::ENEMY_FAMILIES * questassets::ENEMY_STATES;
  static_assert(sizeof(THUMBNAIL_INK) / sizeof(THUMBNAIL_INK[0]) == baseFrames + questassets::GUARDIANS_COUNT / 2);
  const auto art = sprite(assets, guardians, encounter, unsigned(pose));
  const auto& bounds = THUMBNAIL_INK[art.pack == guardians ? baseFrames + art.ink : art.ink - questassets::ENEMIES];
  constexpr int unit = 1024;
  const int scale = std::min(area.width * unit / bounds.width, area.height * unit / bounds.height);
  const int width = THUMBNAIL_WIDTH * scale / unit, height = THUMBNAIL_HEIGHT * scale / unit;
  const int x = area.x + (area.width - bounds.width * width / THUMBNAIL_WIDTH) / 2 - bounds.x * width / THUMBNAIL_WIDTH;
  const int y =
      area.y + (area.height - bounds.height * height / THUMBNAIL_HEIGHT) / 2 - bounds.y * height / THUMBNAIL_HEIGHT;
  const auto previous = target.clipRect();
  const auto clipped =
      fui::makeRect(std::max(area.x, previous.x), std::max(area.y, previous.y),
                    std::max(0, std::min(area.right(), previous.right()) - std::max(area.x, previous.x)),
                    std::max(0, std::min(area.bottom(), previous.bottom()) - std::max(area.y, previous.y)));
  if (clipped.empty()) return;
  if (target.setClipRect(clipped)) {
    art.pack->draw(target, fui::makeRect(x, y, width, height), art.mask, fui::BitmapMode::Stretch, fui::Color::White);
    art.pack->draw(target, fui::makeRect(x, y, width, height), art.ink, fui::BitmapMode::Stretch);
    target.setClipRect(previous);
  } else {
    questextra::paint(target, area, art);
  }
}
struct BattleLayout {
  fui::Rect hero{}, enemy{};
};
inline BattleLayout arrange(const sdart::Image& hero) {
  BattleLayout result;
  if (!hero.height) return result;
  const int heroHeight = 232, heroWidth = hero.width * heroHeight / hero.height;
  result.hero = fui::makeRect(12, 300 - heroHeight, heroWidth, heroHeight);
  const int enemyLeft = std::max(228, int(result.hero.right()) + 22);
  const int enemyWidth = std::max(0, 480 - enemyLeft), enemyHeight = enemyWidth * 280 / 248;
  result.enemy = fui::makeRect(enemyLeft, 300 - enemyHeight * 274 / 280, enemyWidth, enemyHeight);
  return result;
}
inline void battle(fui::DrawTarget& target, fui::Rect area, sdart::Pack& assets, const quest::Profile& p,
                   Pose pose = Pose::Current, questavatar::Framing framing = questavatar::Framing::Contain,
                   sdart::Pack* guardians = nullptr, sdart::Pack* rewards = nullptr, int encounter = -1,
                   const questextra::Sprite* replacement = nullptr, sdart::Pack* outfits = nullptr) {
  const unsigned selected = encounter >= 0 ? unsigned(encounter) : p.campaign.encounter;
  const auto scene =
      questavatar::storyBackground(target, area, assets, questassets::SAGA_SCENES + selected / 60 * 2, framing);
  if (scene.empty()) return;
  const auto box = [&](int x, int y, int width, int height) {
    const int left = scene.x + x * scene.width / 480, top = scene.y + y * scene.height / 320;
    return fui::makeRect(left, top, scene.x + (x + width) * scene.width / 480 - left,
                         scene.y + (y + height) * scene.height / 320 - top);
  };
  const auto sprite = questextra::figure(assets, rewards, p.equipped[0], p.gender, outfits);
  const auto hero = sprite.pack ? sprite.pack->image(sprite.ink) : sdart::Image{};
  if (!assets.ready() || !hero.height) return;
  const auto layout = arrange(hero);
  questextra::paint(target, box(layout.hero.x, layout.hero.y, layout.hero.width, layout.hero.height), sprite);
  const auto enemyBox = box(layout.enemy.x, layout.enemy.y, layout.enemy.width, layout.enemy.height);
  if (replacement)
    questextra::paint(target, enemyBox, *replacement);
  else
    paint(target, enemyBox, assets, p, pose, guardians, int(selected));
}
}  // namespace questenemy
