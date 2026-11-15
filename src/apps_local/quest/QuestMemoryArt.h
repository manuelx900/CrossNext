#pragma once
#include "QuestEnemyArt.h"
#include "QuestSagaCovers.h"

namespace questmemory {
inline void draw(freeink::ui::DrawTarget& target, freeink::ui::Rect area, const quest::Profile& profile,
                 sdart::Pack* assets, unsigned chapter, questenemy::Pose pose, sdart::Pack* guardians,
                 sdart::Pack* rewards, sdart::Pack* outfits = nullptr) {
  if (chapter >= quest::CAMPAIGN_CHAPTERS) return;
  if (assets && assets->ready()) {
    questenemy::battle(target, area, *assets, profile, pose, questavatar::Framing::Contain, guardians, rewards,
                       int(quest::battleAnchor(chapter)), nullptr, outfits);
  } else {
    questcover::draw(target, area, chapter / quest::CHAPTERS_PER_BOOK);
  }
}
}  // namespace questmemory
