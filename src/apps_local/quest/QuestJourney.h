#pragma once

#include <algorithm>

#include "QuestCampaign.h"
#include "QuestEncounterKinds.h"

namespace questjourney {
enum class Kind : uint8_t { Combat, Explore, Rescue, Restore };
enum class Landmark : uint8_t {
  Forest,
  Town,
  Bridge,
  Library,
  Tower,
  Harbor,
  Mountain,
  Well,
  Clock,
  Palace,
  Garden,
  Gate,
  House,
  Fortress,
  Lighthouse,
  Count
};
inline Kind kind(unsigned encounter) {
  (void)encounter;
  return Kind::Combat;
}

// A chapter enters the atlas only after its final account has been acknowledged.
inline unsigned rememberedChapters(const quest::Profile& p) {
  return p.campaign.phase == quest::BattlePhase::Complete ? quest::CAMPAIGN_CHAPTERS : quest::chapter(p);
}
inline bool canVisit(const quest::Profile& p, unsigned chapter) {
  return p.campaign.prologueSeen && chapter < rememberedChapters(p);
}
inline unsigned chapterProgress(const quest::Profile& p, unsigned chapter) {
  if (chapter < rememberedChapters(p)) return 600;
  if (chapter != quest::chapter(p)) return 0;
  const auto target = quest::encounterTarget(p);
  return target ? std::min<uint64_t>(600, uint64_t(p.campaign.encounterSeconds) * 600 / target) : 0;
}
inline bool canResume(const quest::Profile& p) {
  return p.campaign.state == quest::AdventureState::Active && p.campaign.prologueSeen && p.campaign.introducedBooks &&
         p.campaign.narratedEncounters;
}
inline unsigned requestBook(const quest::Profile& p) {
  return p.campaign.introducedBooks ? std::min<unsigned>(quest::book(p), p.campaign.introducedBooks - 1) : 0;
}
struct Context {
  unsigned chapter = 0;
  unsigned beat = 0;
  bool prologue = true;
};
inline Context context(const quest::Profile& p) {
  Context result;
  if (!p.campaign.prologueSeen) return result;
  const auto chapter = quest::chapter(p);

  if (p.campaign.phase == quest::BattlePhase::Complete) return {quest::CAMPAIGN_CHAPTERS - 1, 3, false};
  if (quest::needsEncounterStory(p)) {
    return chapter ? Context{chapter - 1, 3, false} : result;
  }
  return {chapter, p.campaign.phase == quest::BattlePhase::Story ? 3u : 0u, false};
}
inline unsigned recallEncounter(const Context& context) {
  static constexpr unsigned STEPS[] = {0, 0, 2, 5};
  return context.chapter * quest::ENCOUNTERS_PER_CHAPTER + STEPS[std::min(3u, context.beat)];
}
}  // namespace questjourney
