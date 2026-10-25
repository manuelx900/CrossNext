#pragma once
#include <FreeInkUI.h>

#include <algorithm>

#include "QuestArtMeta.h"
#include "QuestAssets.h"
#include "QuestCore.h"

namespace questavatar {
namespace fui = freeink::ui;
inline void paint(sdart::Pack& assets, fui::DrawTarget& target, fui::Rect rect, uint32_t ink, uint32_t mask,
                  fui::BitmapMode mode = fui::BitmapMode::Contain) {
  assets.draw(target, rect, mask, mode, fui::Color::White);
  assets.draw(target, rect, ink, mode);
}
inline void draw(fui::DrawTarget& target, fui::Rect area, const quest::Profile& profile, sdart::Pack& assets,
                 int preview = -1) {
  target.fill(area, fui::Paint::solid(fui::Color::White));
  if (area.empty()) return;
  const auto equipped = [&](quest::Slot slot) {
    return quest::visibleItem(preview) && quest::slot(preview) == slot ? preview : int(profile.equipped[int(slot)]);
  };
  const int selectedBody = equipped(quest::Slot::Body);
  const int body = selectedBody >= 0 && selectedBody < quest::kOutfitCount ? selectedBody : 0;
  const int aura = equipped(quest::Slot::Aura) - quest::first(quest::Slot::Aura);
  const int pet = equipped(quest::Slot::Pet) - quest::first(quest::Slot::Pet);
  const bool hasAura = aura >= 0 && aura < quest::kAuraCount, hasPet = pet >= 0 && pet < quest::kPetCount;
  constexpr int bodyHeight = 256;
  const int figure = body * 2 + (profile.gender == quest::Gender::Feminine ? 1 : 0);
  const auto figureArt = assets.image(questassets::FIGURES + figure);
  if (!assets.ready()) return;
  const int figureWidth = int(figureArt.width) * bodyHeight / figureArt.height;
  constexpr int bodyWidth = questartmeta::figureFrameWidth;
  const auto petArt = hasPet ? assets.image(questassets::PETS + pet) : sdart::Image{};
  if (!assets.ready()) return;
  const int petHeight = petArt.height, petWidth = petArt.width;
  const int auraHeight =
      hasAura ? bodyHeight * (135 + (questartmeta::auraScale[aura] - 110) * 15 / 147) / 100 : bodyHeight;
  const int auraWidth = hasAura ? std::max(auraHeight, bodyWidth * 140 / 100) : bodyWidth;
  const int minX = std::min(0, (bodyWidth - auraWidth) / 2);
  const int maxX = std::max({bodyWidth, (bodyWidth + auraWidth) / 2, hasPet ? bodyWidth + 12 + petWidth : bodyWidth});
  const int minY = std::min({0, (bodyHeight - auraHeight) / 2, hasPet ? bodyHeight - petHeight : 0});
  const int maxY = std::max(bodyHeight, (bodyHeight + auraHeight) / 2);
  const int scale = std::min(int(area.width) * 1024 / (maxX - minX), int(area.height) * 1024 / (maxY - minY));
  if (scale <= 0) return;
  const int left = area.x + (area.width - (maxX - minX) * scale / 1024) / 2;
  const int top = area.y + (area.height - (maxY - minY) * scale / 1024) / 2;
  const auto rect = [&](int x, int y, int width, int height) {
    const int px = (x - minX) * scale / 1024, py = (y - minY) * scale / 1024;
    return fui::makeRect(left + px, top + py, (x + width - minX) * scale / 1024 - px,
                         (y + height - minY) * scale / 1024 - py);
  };
  if (hasAura)
    paint(assets, target, rect((bodyWidth - auraWidth) / 2, (bodyHeight - auraHeight) / 2, auraWidth, auraHeight),
          questassets::AURAS + aura, questassets::AURA_MASKS + aura);
  paint(assets, target, rect((bodyWidth - figureWidth) / 2, 0, figureWidth, bodyHeight), questassets::FIGURES + figure,
        questassets::FIGURE_MASKS + figure);
  if (hasPet)
    paint(assets, target, rect(bodyWidth + 12, bodyHeight - petHeight, petWidth, petHeight), questassets::PETS + pet,
          questassets::PET_MASKS + pet);
}
}  // namespace questavatar
