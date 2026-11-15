#pragma once
#include <algorithm>

#include "QuestAssets.h"
#include "QuestCore.h"
#include "QuestExpansionPolicy.h"
#include "QuestRequests.h"

namespace questextra {
namespace fui = freeink::ui;
struct Sprite {
  sdart::Pack* pack = nullptr;
  uint32_t ink = 0, mask = 0;
};
inline fui::Rect decorationBox(int item, int place, fui::Rect hero, fui::Rect pet) {
  const auto left = place == 0 ? fui::makeRect(4, 4, 124, 140) : fui::makeRect(4, 204, 124, 108);
  const auto right = place == 0 ? fui::makeRect(352, 4, 124, 140) : fui::makeRect(352, 204, 124, 108);
  const auto overlap = [](fui::Rect a, fui::Rect b) {
    return std::max(0, std::min(int(a.right()), int(b.right())) - std::max(int(a.x), int(b.x))) *
           std::max(0, std::min(int(a.bottom()), int(b.bottom())) - std::max(int(a.y), int(b.y)));
  };
  // Compare the body core; the full opaque character mask is painted last.
  hero = hero.inset({0, int16_t(hero.width / 4), 0, int16_t(hero.width / 4)});
  pet = pet.inset({0, int16_t(pet.width / 5), 0, int16_t(pet.width / 5)});
  const int l = overlap(left, hero) + overlap(left, pet), r = overlap(right, hero) + overlap(right, pet);
  const bool preferRight =
      item == quest::kInkRelic ||
      (item >= quest::kExpandedItemStart ? questexp::DECORATIONS[item - quest::kExpandedItemStart].anchor != 0
                                         : place == 0);
  return l == r ? preferRight ? right : left : l < r ? left : right;
}
inline void paint(fui::DrawTarget& target, fui::Rect box, const Sprite& sprite) {
  if (!sprite.pack || !sprite.pack->ready()) return;
  sprite.pack->draw(target, box, sprite.mask, fui::BitmapMode::Contain, fui::Color::White);
  sprite.pack->draw(target, box, sprite.ink, fui::BitmapMode::Contain);
}
inline Sprite figure(sdart::Pack& base, sdart::Pack* rewards, int item, quest::Gender gender,
                     sdart::Pack* outfits = nullptr) {
  if (quest::huntOutfit(item)) {
    const int source = quest::huntOutfitSource(item);
    if (quest::bestiaryHunt(source)) {
      static_assert(quest::kBestiaryCount * 2 == questassets::BESTIARY_OUTFIT_MASKS);
      const unsigned index = (source - quest::kBestiaryHuntStart) * 2 + unsigned(gender);
      return {outfits, questassets::BESTIARY_OUTFIT_FIGURES + index, questassets::BESTIARY_OUTFIT_MASKS + index};
    }
    if (quest::specialHunt(source) && !quest::bestiaryHunt(source)) {
      const unsigned index = (source - quest::kCampaignHuntCount) * 2 + unsigned(gender);
      return {rewards, questassets::HUNT_FIGURES + index, questassets::HUNT_FIGURE_MASKS + index};
    }
    item = quest::huntOutfitBase(item);
  }
  if (item == quest::kKingBody)
    return {rewards, questassets::KING_FIGURES + unsigned(gender), questassets::KING_FIGURE_MASKS + unsigned(gender)};
  if (item < 0 || item >= quest::kOutfitCount) return {};
  const unsigned index = item * 2 + unsigned(gender);
  return {&base, questassets::FIGURES + index, questassets::FIGURE_MASKS + index};
}
inline Sprite pet(sdart::Pack& base, sdart::Pack* rewards, int item) {
  if (item == quest::kKingPet) return {rewards, questassets::KING_PET, questassets::KING_PET_MASK};
  const int index = item - quest::first(quest::Slot::Pet);
  if (index < 0 || index >= quest::kPetCount) return {};
  return {&base, questassets::PETS + unsigned(index), questassets::PET_MASKS + unsigned(index)};
}
inline Sprite decoration(sdart::Pack& base, sdart::Pack* rewards, int item) {
  if (quest::decorationPlace(item) < 0) return {};
  if (item == quest::kInkRelic) return {rewards, questassets::INK_RELIC, questassets::INK_RELIC_MASK};
  if (item >= quest::kExpandedItemStart) {
    const unsigned index = item - quest::kExpandedItemStart;
    return {rewards, questassets::EXTRA_DECOR + index, questassets::EXTRA_DECOR_MASKS + index};
  }
  const unsigned index = item - quest::kBaseGearCount;
  return {&base, questassets::DECORATIONS + index, questassets::DECORATION_MASKS + index};
}
inline void tiles(fui::DrawTarget& target, fui::Rect area, sdart::Pack& pack, unsigned first) {
  const int middle = area.x + area.width / 2;
  pack.draw(target, fui::makeRect(area.x, area.y, middle - area.x, area.height), first, fui::BitmapMode::Stretch);
  pack.draw(target, fui::makeRect(middle, area.y, area.right() - middle, area.height), first + 1,
            fui::BitmapMode::Stretch);
}
inline bool victory(fui::DrawTarget& target, fui::Rect area, sdart::Pack* rewards, quest::Gender gender) {
  if (!rewards || !rewards->ready()) return false;
  tiles(target, area, *rewards, questassets::VICTORY_SCENES + unsigned(gender) * 2);
  return true;
}
}  // namespace questextra
