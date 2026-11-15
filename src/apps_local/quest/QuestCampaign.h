#pragma once

#include "QuestCore.h"

namespace quest {
// Legacy encounter IDs remain chapter-end anchors in saved profiles.
constexpr unsigned battleAnchor(unsigned chapter) {
  return chapter * ENCOUNTERS_PER_CHAPTER + ENCOUNTERS_PER_CHAPTER - 1;
}
constexpr unsigned guardian(unsigned encounter) {
  constexpr unsigned roster[] = {0, 1, 2, 3, 4, 0, 1, 2, 3, 5};
  const unsigned path = encounter / ENCOUNTERS_PER_CHAPTER;
  if (path == 110) return 71;
  if (path >= 118) return 72 + (path == 119);
  return path / CHAPTERS_PER_BOOK * 6 + roster[path % CHAPTERS_PER_BOOK];
}
bool normalizeBattles(Profile&);
bool prepareBattle(Profile&);
bool collectBattleResult(Profile&);
bool advanceBattle(Profile&);
uint32_t recordSecondsOnDay(const readingstats::Record&, int day);
struct GearBonus {
  uint16_t power = 0;
  uint8_t experience = 0, fortune = 0, affinity = 0;
};
inline constexpr uint32_t WEEKLY_MINUTES = 120;
GearBonus gearBonus(int item, unsigned book);
GearBonus bonuses(const Profile&, int preview = -1);
uint32_t heroPower(const Profile&, int preview = -1);
uint32_t huntMinutes(const Profile&, int id, int preview = -1);
uint32_t baseMinutes(unsigned encounter);
uint32_t encounterTarget(const Profile&, int preview = -1);
uint32_t remainingMinutes(const Profile&);
struct CampaignMinutes {
  uint64_t read = 0, target = 0;
};
CampaignMinutes campaignMinutes(const Profile&);
uint32_t campaignProgress(const Profile&);  // Basis points, including partial encounters.
constexpr unsigned chapter(const Profile& p) { return p.campaign.encounter / ENCOUNTERS_PER_CHAPTER; }
unsigned book(const Profile&);
unsigned defeatedBeasts(const Profile&);
constexpr unsigned completedPaths(const Profile& p) {
  if (p.campaign.phase == BattlePhase::Complete) return CAMPAIGN_CHAPTERS;
  const bool won = p.campaign.phase == BattlePhase::Story && p.campaign.encounter == battleAnchor(chapter(p));
  return chapter(p) + unsigned(won);
}
constexpr bool lastPathInSaga(unsigned chapter) { return (chapter + 1) % CHAPTERS_PER_BOOK == 0; }
bool needsIntroduction(const Profile&);
bool needsPrologue(const Profile&);
bool acknowledgePrologue(Profile&);
bool needsEncounterStory(const Profile&);
bool beginCampaign(Profile&);
bool beginEncounter(Profile&);
struct ReadingCredit {
  uint64_t withoutReserve = 0;
};
bool syncCampaign(Profile&, const readingstats::Summary&, int today, ReadingCredit = {});
bool settleBattle(Profile&);
uint32_t battleXp(const Profile&);
uint32_t battleCoins(const Profile&);
bool collectBattle(Profile&);
bool continueCampaign(Profile&);
bool finishAdventure(Profile&);
void restartAdventure(Profile&, const readingstats::Summary&, int today);
uint64_t adventureSeconds(const Profile&, const readingstats::Summary&);
uint32_t adventureBooks(const Profile&, const readingstats::Summary&);
uint32_t adventureDay(const Profile&, const readingstats::Summary&, int day);
uint64_t adventureWeek(const Profile&, const readingstats::Summary&, int today);
uint32_t adventureStreak(const Profile&, const readingstats::Summary&, int today);
enum class Mission : uint8_t { Daily, Weekly, Story };
uint64_t missionValue(const Profile&, const readingstats::Summary&, int today, Mission);
uint32_t missionTarget(Mission);
uint32_t missionXp(Mission);
uint32_t missionCoins(Mission);
bool missionClaimed(const Profile&, int today, Mission);
unsigned missionChapter(const Profile&);
bool claimMission(Profile&, const readingstats::Summary&, int today, Mission);
}  // namespace quest
