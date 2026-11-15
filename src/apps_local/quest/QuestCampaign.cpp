#include "QuestCampaign.h"

#include <algorithm>

#include "QuestExpansionPolicy.h"
#include "QuestRequests.h"

namespace quest {
namespace {
constexpr uint8_t BACKGROUND_REGIONS[] = {0, 0, 0,  2, 0,  4,  5, 0,  0,  0,  1,  1, 1, 4,  1, 7, 2,
                                          6, 0, 7,  3, 4,  4,  6, 3,  9,  8,  10, 9, 9, 2,  7, 8, 9,
                                          8, 0, 10, 8, 10, 10, 8, 10, 10, 10, 8,  8, 8, 10, 8, 11};
bool hasBit(const uint8_t* bits, unsigned n) { return bits[n / 8] & (1u << (n % 8)); }
void mark(uint8_t* bits, unsigned n) { bits[n / 8] |= 1u << (n % 8); }
void clearRequest(Profile& p) {
  p.activeRequest = kRequestCount;
  p.requestCarried = 0;
  p.requestTargetMinutes = 0;
  p.requestElapsed = 0;
  p.requestBooks = 0;
  p.requestWinDay = p.requestWinSeconds = 0;
  p.requestStarted = 0;
  p.requestBaseline = 0;
  p.requestReported = 0;
  p.requestGoal = RequestGoal::Minutes;
}
bool huntDefeated(const Profile& p, int id) {
  if (id >= kLegacyRequestCount) return hasBit(p.requestClaims, id);
  const auto key = guardian(huntEncounter(id));
  const int first = id / kRequestsPerRegion * kRequestsPerRegion;
  for (int n = first; n < first + kRequestsPerRegion; ++n)
    if (hasBit(p.requestClaims, n) && guardian(huntEncounter(n)) == key) return true;
  return false;
}
unsigned completedChapters(const Profile& p) {
  return p.campaign.phase == BattlePhase::Complete ? CAMPAIGN_CHAPTERS : p.campaign.encounter / ENCOUNTERS_PER_CHAPTER;
}
bool addReward(Profile& p, uint32_t xp, uint32_t money) {
  if (p.campaign.extraCoins > UINT64_MAX - money || p.xp > UINT64_MAX - xp || p.bonus > UINT32_MAX - xp) return false;
  p.xp += xp;
  p.bonus += xp;
  p.campaign.extraCoins += money;
  return true;
}
void baseline(Profile& p, const readingstats::Summary& s, int today) {
  auto& c = p.campaign;
  c.observedSeconds = c.originSeconds = s.seconds;
  c.originCompleted = s.completed;
  p.observedBooks = s.completed;
  c.startedDay = today;
  c.startDaySeconds = s.onDay(today);
  c.initialized = true;
}
}  // namespace
uint32_t recordSecondsOnDay(const readingstats::Record& record, int day) {
  const int64_t start = int64_t(day - record.localDay) * 86400 - record.secondOfDay;
  const auto from = std::max<int64_t>(0, start);
  const auto to = std::min<int64_t>(record.seconds, start + 86400);
  return to > from ? uint32_t(to - from) : 0;
}
unsigned book(const Profile& p) { return chapter(p) / CHAPTERS_PER_BOOK; }
bool needsPrologue(const Profile& p) { return p.campaign.state == AdventureState::Active && !p.campaign.prologueSeen; }
bool acknowledgePrologue(Profile& p) {
  if (!needsPrologue(p)) return false;
  p.campaign.prologueSeen = true;
  return true;
}
bool needsIntroduction(const Profile& p) {
  return p.campaign.state == AdventureState::Active && p.campaign.phase == BattlePhase::Reading &&
         p.campaign.introducedBooks <= book(p);
}
bool needsEncounterStory(const Profile& p) {
  return p.campaign.state == AdventureState::Active && p.campaign.phase == BattlePhase::Reading &&
         p.campaign.narratedEncounters <= p.campaign.encounter;
}
bool beginCampaign(Profile& p) {
  if (needsPrologue(p) || !needsIntroduction(p)) return false;
  p.campaign.introducedBooks = book(p) + 1;
  return true;
}
bool beginEncounter(Profile& p) {
  if (needsPrologue(p) || needsIntroduction(p) || !needsEncounterStory(p)) return false;
  auto& c = p.campaign;
  c.narratedEncounters = c.encounter + 1;
  p.battleElapsed = 0;
  p.battleBooks = p.observedBooks == UINT32_MAX ? UINT32_MAX : 0;
  p.battleStatsKnown = true;
  return true;
}
GearBonus gearBonus(int item, unsigned region) {
  if (!visibleItem(item)) return {};
  const unsigned index = item - first(slot(item));
  unsigned tier = requiredLevel(item);
  const auto origin = rewardEncounter(item);
  if (origin >= 0) tier = questbalance::recommendedLevel(unsigned(origin) / ENCOUNTERS_PER_CHAPTER);
  if (huntOutfit(item)) {
    const int source = huntOutfitSource(item);
    tier = questbalance::recommendedLevel(huntEncounter(source) / ENCOUNTERS_PER_CHAPTER);
    switch (huntRank(source)) {
      case HuntRank::Basic:
        return {uint16_t(10 + tier), 2, 4, 0};
      case HuntRank::Elite:
        return {uint16_t(14 + tier), 4, 6, 0};
      case HuntRank::Legendary:
        return {uint16_t(20 + tier), 6, 10, 0};
      case HuntRank::Secret:
        return {100, 10, 15, 0};
    }
  }
  if (item == kInkRelic) return {35, 8, 12, 0};
  if (item == kKingBody) return {160, 10, 6, 0};
  if (item == kKingBackground) return {55, 8, 8, 0};
  if (item == kKingPet) return {60, 6, 10, 0};
  if (item >= kExpandedItemStart) {
    const auto& decoration = questexp::DECORATIONS[item - kExpandedItemStart];
    const unsigned regionTier =
        decoration.source == questexp::Source::Chapter ? decoration.sourceId / CHAPTERS_PER_BOOK : 5;
    return {uint16_t(10 + regionTier * 3 + decoration.power * 3), decoration.experience, decoration.fortune, 0};
  }
  switch (slot(item)) {
    case Slot::Body:
      if (index == 0) return {12, 0, 0, 0};
      return {uint16_t(12 + tier * (index % 3 == 0 ? 12 : 9) / 10), uint8_t(index % 3 == 1 ? 6 + tier / 10 : 0),
              uint8_t(index % 3 == 2 ? 5 + tier / 12 : 0), 0};
    case Slot::Background:
      return {uint16_t(4 + tier / 2), uint8_t(index % 3 == 1 ? 6 : 0), uint8_t(index % 3 == 2 ? 5 : 0),
              uint8_t(BACKGROUND_REGIONS[index] == region ? 8 : 0)};
    case Slot::Pet:
      return {uint16_t(6 + tier / 2), uint8_t(index % 3 == 1 ? 8 : 0), uint8_t(index % 3 == 2 ? 8 : 0), 0};
    case Slot::Decoration:
      return {uint16_t(8 + tier / 2), uint8_t(index % 3 == 0 ? 4 : 0), uint8_t(index % 3 == 2 ? 4 : 0), 0};
  }
  return {};
}

namespace {
GearBonus bonusesInRegion(const Profile& p, unsigned region, int preview) {
  GearBonus total;
  for (int position = 0; position < kSlotCount; ++position) {
    const auto item = visibleItem(preview) && int(slot(preview)) == position ? preview : p.equipped[position];
    const auto value = gearBonus(item, region);
    total.power += value.power + value.affinity;
    total.experience += value.experience;
    total.fortune += value.fortune;
  }
  const int decoration = availableItem(preview) && slot(preview) == Slot::Decoration ? preview : activeDecoration(p);
  const auto value = gearBonus(decoration, region);
  total.power += value.power;
  total.experience += value.experience;
  total.fortune += value.fortune;
  total.experience = std::min<unsigned>(questbalance::XP_CAP, total.experience);
  total.fortune = std::min<unsigned>(questbalance::COIN_CAP, total.fortune);
  return total;
}
}  // namespace
GearBonus bonuses(const Profile& p, int preview) { return bonusesInRegion(p, book(p), preview); }

uint32_t heroPower(const Profile& p, int preview) {
  return questbalance::levelPower(level(p.xp)) + bonuses(p, preview).power;
}
uint32_t baseMinutes(unsigned encounter) {
  return encounter < CAMPAIGN_ENCOUNTERS ? questbalance::campaignMinutes(encounter / ENCOUNTERS_PER_CHAPTER) : 0;
}
uint32_t encounterTarget(const Profile& p, int preview) {
  if (preview < 0 && (p.campaign.phase == BattlePhase::Victory || p.campaign.phase == BattlePhase::Story) &&
      p.campaign.encounterSeconds)
    return p.campaign.encounterSeconds;
  return questbalance::campaignDuration(chapter(p), heroPower(p, preview)) * 60;
}
uint32_t huntMinutes(const Profile& p, int id, int preview) {
  if (!validRequest(id)) return 0;
  const auto path = huntEncounter(id) / ENCOUNTERS_PER_CHAPTER;
  return std::min<uint32_t>(requestMinutes(id),
                            questbalance::duration(requestMinutes(id), questbalance::referencePower(path),
                                                   heroPower(p, preview), huntRank(id) == HuntRank::Secret ? 75 : 45));
}
uint32_t remainingMinutes(const Profile& p) {
  if (p.campaign.phase != BattlePhase::Reading) return 0;
  const auto target = encounterTarget(p);
  return p.campaign.encounterSeconds < target ? (target - p.campaign.encounterSeconds + 59) / 60 : 0;
}
CampaignMinutes campaignMinutes(const Profile& p) {
  const auto& c = p.campaign;
  const uint64_t seconds = c.observedSeconds >= c.originSeconds ? c.observedSeconds - c.originSeconds : 0;
  CampaignMinutes result{seconds / 60, seconds / 60};
  if (c.state != AdventureState::Active || c.phase == BattlePhase::Complete) return result;
  uint64_t remaining = 0;
  unsigned battles = 0, previousRegion = CAMPAIGN_BOOKS, power = 0;
  for (unsigned n = chapter(p); n < CAMPAIGN_CHAPTERS; ++n) {
    if (n == chapter(p) && c.phase != BattlePhase::Reading) continue;
    const unsigned region = n / CHAPTERS_PER_BOOK;
    if (region != previousRegion) {
      power = questbalance::levelPower(level(p.xp)) + bonusesInRegion(p, region, -1).power;
      previousRegion = region;
    }
    const uint32_t target =
        questbalance::campaignDuration(
            n, n == chapter(p) ? power : std::max<uint32_t>(power, questbalance::referencePower(n))) *
        60;
    remaining += n == chapter(p) ? target > c.encounterSeconds ? target - c.encounterSeconds : 0 : target;
    ++battles;
  }
  remaining = std::max<uint64_t>(remaining, battles);
  result.target += (remaining + 59) / 60;
  return result;
}
uint32_t campaignProgress(const Profile& p) {
  if (p.campaign.phase == BattlePhase::Complete) return 10000;
  const unsigned fraction = p.campaign.phase == BattlePhase::Reading
                                ? std::min<uint64_t>(10000, uint64_t(p.campaign.encounterSeconds) * 10000 /
                                                                std::max<uint32_t>(1, encounterTarget(p)))
                                : 10000;
  return (uint32_t(chapter(p)) * 10000 + fraction) / CAMPAIGN_CHAPTERS;
}
uint32_t battleXp(const Profile& p) {
  const auto recorded = p.campaignVictories[std::min<unsigned>(CAMPAIGN_CHAPTERS - 1, chapter(p))].xp;
  return p.campaign.phase != BattlePhase::Reading && recorded
             ? recorded
             : questbalance::victoryXp((encounterTarget(p) + 59) / 60, bonuses(p).experience);
}
uint32_t battleCoins(const Profile& p) {
  return p.campaign.phase == BattlePhase::Reading
             ? (questbalance::campaignCoins(chapter(p)) * (100 + bonuses(p).fortune) + 50) / 100
             : p.campaign.victoryCoins;
}
bool settleBattle(Profile& p) {
  auto& c = p.campaign;
  if (c.state != AdventureState::Active || c.phase != BattlePhase::Reading || needsPrologue(p) ||
      needsIntroduction(p) || needsEncounterStory(p))
    return false;
  const auto target = encounterTarget(p);
  if (c.encounterSeconds < target) return false;
  c.bankSeconds = 0;
  c.encounterSeconds = target;
  c.victoryCoins = battleCoins(p);
  c.phase = BattlePhase::Victory;
  p.campaignVictories[chapter(p)] = {p.savedDay, p.battleStatsKnown ? p.battleElapsed : 0, battleXp(p), c.victoryCoins,
                                     p.battleStatsKnown ? p.battleBooks : UINT32_MAX};
  return true;
}
bool syncCampaign(Profile& p, const readingstats::Summary& s, int today, ReadingCredit) {
  auto& c = p.campaign;
  if (!c.initialized) {
    const bool existing = p.xp || p.spent || p.name[0];
    baseline(p, s, today);
    if (existing) {
      c.originSeconds = 0;
      c.originCompleted = 0;
      c.startedDay = 18262;
      c.startDaySeconds = 0;
      c.firstDayRead = true;
    }
    return true;
  }
  if (c.state == AdventureState::Setup) return false;
  p.savedDay = uint32_t(std::clamp(today, 0, 84005));
  const uint32_t books =
      p.observedBooks != UINT32_MAX && s.completed >= p.observedBooks ? s.completed - p.observedBooks : 0;
  const bool booksChanged = p.observedBooks != s.completed;
  p.observedBooks = s.completed;
  const auto addBooks = [&](uint32_t& value) {
    if (value != UINT32_MAX) value = std::min<uint64_t>(UINT32_MAX - 1, uint64_t(value) + books);
  };
  if (validRequest(p.activeRequest) && !p.requestWinDay) addBooks(p.requestBooks);
  if (c.state == AdventureState::Active && c.phase == BattlePhase::Reading && !needsPrologue(p) &&
      !needsIntroduction(p) && !needsEncounterStory(p))
    addBooks(p.battleBooks);
  const bool normalized = normalizeBattles(p);
  bool restored = restoreExpansionRewards(p) || normalized;
  const auto recordHuntVictory = [&] {
    if (!validRequest(p.activeRequest) || p.requestWinDay ||
        requestValue(p, p.activeRequest, &s) < requestTarget(p, p.activeRequest))
      return false;
    p.requestWinDay = p.savedDay;
    const auto elapsed = c.observedSeconds >= p.requestStarted ? c.observedSeconds - p.requestStarted : 0;
    p.requestWinSeconds = std::min<uint64_t>(
        UINT32_MAX, uint64_t(p.requestElapsed) + (p.activeRequest == kInkHunt ? p.pausedInkElapsed : 0) + elapsed);
    return true;
  };
  if (c.observedSeconds == s.seconds) {
    const bool recorded = recordHuntVictory();
    return restored || booksChanged || recorded;
  }
  if (c.observedSeconds > s.seconds) {
    if (validRequest(p.activeRequest)) {
      const auto elapsed = c.observedSeconds >= p.requestStarted ? c.observedSeconds - p.requestStarted : 0;
      p.requestElapsed =
          std::min<uint64_t>(UINT32_MAX, uint64_t(p.requestElapsed) + std::min<uint64_t>(elapsed, UINT32_MAX));
      const auto target = requestTarget(p, p.activeRequest);
      if (p.activeRequest == kInkHunt)
        p.pausedInkSeconds = std::min<uint64_t>(
            INK_HUNT_SECONDS - 1,
            p.pausedInkSeconds + (c.observedSeconds >= p.requestStarted ? c.observedSeconds - p.requestStarted : 0));
      else if (p.requestGoal == RequestGoal::Minutes)
        p.requestCarried = std::min<uint64_t>(
            uint64_t(target) * 60,
            p.requestCarried + (c.observedSeconds >= p.requestStarted ? c.observedSeconds - p.requestStarted : 0));
      else
        p.requestCarried = std::min<uint32_t>(p.requestReported, target);
      p.requestStarted = s.seconds;
      p.requestBaseline = p.requestGoal == RequestGoal::Minutes ? 0 : requestMetric(p, p.requestGoal, &s);
    }
    c.observedSeconds = s.seconds;
    if (validRequest(p.activeRequest)) p.requestStarted = std::min(p.requestStarted, s.seconds);
    c.originSeconds = std::min(c.originSeconds, s.seconds);
    c.originCompleted = std::min(c.originCompleted, s.completed);
    c.startDaySeconds = std::min(c.startDaySeconds, s.onDay(c.startedDay));
    return true;
  }
  c.firstDayRead |= adventureDay(p, s, c.startedDay) > 0;
  const uint64_t delta = s.seconds - c.observedSeconds;
  c.xpRemainder = 0;
  c.bankSeconds = 0;
  c.observedSeconds = s.seconds;
  if (c.state == AdventureState::Active && c.phase != BattlePhase::Complete) {
    const bool fighting =
        c.phase == BattlePhase::Reading && !needsPrologue(p) && !needsIntroduction(p) && !needsEncounterStory(p);
    if (fighting) {
      p.battleElapsed = std::min<uint64_t>(UINT32_MAX, uint64_t(p.battleElapsed) + delta);
      const auto room = encounterTarget(p) > c.encounterSeconds ? encounterTarget(p) - c.encounterSeconds : 0;
      c.encounterSeconds += std::min<uint64_t>(delta, room);
      settleBattle(p);
    }
  }
  recordHuntVictory();
  return true;
}
bool collectBattle(Profile& p) {
  auto& c = p.campaign;
  if (c.state != AdventureState::Active || c.phase != BattlePhase::Victory ||
      !addReward(p, battleXp(p), c.victoryCoins))
    return false;
  c.phase = BattlePhase::Story;
  c.pendingReward = kGearCount;
  return true;
}
bool continueCampaign(Profile& p) {
  auto& c = p.campaign;
  if (c.state != AdventureState::Active || c.phase != BattlePhase::Story || visibleItem(c.pendingReward)) return false;
  if (c.encounter == CAMPAIGN_ENCOUNTERS - 1) {
    c.phase = BattlePhase::Complete;
    c.state = AdventureState::Peace;
    p.endingSeen = false;
    c.bankSeconds = 0;
    restoreExpansionRewards(p);
    return true;
  }
  c.encounter = battleAnchor(chapter(p) + 1);
  c.phase = BattlePhase::Reading;
  c.encounterSeconds = 0;
  p.battleElapsed = p.battleBooks = 0;
  p.battleStatsKnown = false;
  return true;
}
bool normalizeBattles(Profile& p) {
  auto& c = p.campaign;
  if (c.state != AdventureState::Active) return false;
  bool changed = false;
  const unsigned anchor = battleAnchor(chapter(p));
  if (c.encounter != anchor) {
    const bool known = c.narratedEncounters > c.encounter;
    c.encounter = anchor;
    c.narratedEncounters = known ? anchor + 1 : anchor;
    const auto target = encounterTarget(p);
    if (c.phase == BattlePhase::Reading && c.encounterSeconds > target) {
      c.bankSeconds = 0;
      c.encounterSeconds = target;
    }
    changed = true;
  }
  if (c.phase == BattlePhase::Story) {
    for (unsigned n = 0; n < 2 && visibleItem(c.pendingReward); ++n) changed |= dismissStoryReward(p, false);
  }
  return changed;
}
bool prepareBattle(Profile& p) {
  if (p.campaign.state != AdventureState::Active || needsPrologue(p) || p.campaign.phase != BattlePhase::Reading)
    return false;
  normalizeBattles(p);
  if (needsIntroduction(p)) beginCampaign(p);
  return needsEncounterStory(p) ? beginEncounter(p) : true;
}
bool collectBattleResult(Profile& p) {
  if (!collectBattle(p)) return false;
  for (unsigned n = 0; n < 2 && visibleItem(p.campaign.pendingReward); ++n) dismissStoryReward(p, false);
  if (lastPathInSaga(chapter(p))) {
    restoreExpansionRewards(p);
    offerOutfit(p, sagaReward(book(p), Slot::Body));
  }
  return true;
}
bool advanceBattle(Profile& p) {
  if (!continueCampaign(p)) return false;
  return true;
}
bool finishAdventure(Profile& p) {
  auto& c = p.campaign;
  if (c.state != AdventureState::Active) return false;
  const bool won = c.encounter == CAMPAIGN_ENCOUNTERS - 1 && c.phase != BattlePhase::Reading;
  c.state = won ? AdventureState::Peace : AdventureState::Unfinished;
  p.endingSeen = false;
  restoreExpansionRewards(p);
  return true;
}
void restartAdventure(Profile& p, const readingstats::Summary& s, int today) {
  const auto sequence = p.sequence;
  static constexpr Profile fresh;
  p = fresh;
  p.sequence = sequence;
  p.campaign.state = AdventureState::Setup;
  baseline(p, s, today);
}
uint64_t adventureSeconds(const Profile& p, const readingstats::Summary& s) {
  return s.seconds >= p.campaign.originSeconds ? s.seconds - p.campaign.originSeconds : 0;
}
uint32_t adventureBooks(const Profile& p, const readingstats::Summary& s) {
  return s.completed >= p.campaign.originCompleted ? s.completed - p.campaign.originCompleted : 0;
}
uint32_t adventureDay(const Profile& p, const readingstats::Summary& s, int day) {
  const auto& c = p.campaign;
  if (!c.initialized || day < c.startedDay) return 0;
  const auto seconds = s.onDay(day), before = day == c.startedDay ? c.startDaySeconds : 0;
  return seconds >= before ? seconds - before : 0;
}
uint64_t adventureWeek(const Profile& p, const readingstats::Summary& s, int today) {
  uint64_t seconds = 0;
  for (int day = readingstats::weekStart(today, false); day <= today; ++day) seconds += adventureDay(p, s, day);
  return seconds;
}
uint32_t adventureStreak(const Profile& p, const readingstats::Summary& s, int today) {
  const int end = adventureDay(p, s, today) ? today : today - 1;
  if (!adventureDay(p, s, end) || end < p.campaign.startedDay) return 0;
  const uint32_t bound = unsigned(end - p.campaign.startedDay) + (p.campaign.firstDayRead ? 1 : 0);
  return std::min(s.currentStreak(today), bound);
}
unsigned missionChapter(const Profile& p) {
  for (unsigned n = 0; n < completedChapters(p); ++n)
    if (!hasBit(p.campaign.storyClaims, n)) return n;
  return chapter(p);
}
uint32_t missionTarget(Mission mission) {
  switch (mission) {
    case Mission::Daily:
      return DAILY_MINUTES;
    case Mission::Weekly:
      return WEEKLY_MINUTES;
    case Mission::Story:
      return 1;
  }
  return 0;
}
uint32_t missionXp(Mission mission) {
  switch (mission) {
    case Mission::Daily:
      return 10;
    case Mission::Weekly:
      return 40;
    case Mission::Story:
      return 0;
  }
  return 0;
}
uint32_t missionCoins(Mission mission) {
  switch (mission) {
    case Mission::Daily:
      return 6;
    case Mission::Weekly:
      return 30;
    case Mission::Story:
      return 20;
  }
  return 0;
}
uint64_t missionValue(const Profile& p, const readingstats::Summary& s, int today, Mission mission) {
  switch (mission) {
    case Mission::Daily:
      return adventureDay(p, s, today) / 60;
    case Mission::Weekly:
      return adventureWeek(p, s, today) / 60;
    case Mission::Story:
      return completedChapters(p) > missionChapter(p) ? 1 : 0;
  }
  return 0;
}
bool missionClaimed(const Profile& p, int today, Mission mission) {
  switch (mission) {
    case Mission::Daily:
      return p.claimedDay >= today;
    case Mission::Weekly:
      return p.campaign.claimedWeek >= readingstats::weekStart(today, false);
    case Mission::Story:
      return hasBit(p.campaign.storyClaims, missionChapter(p));
  }
  return false;
}
bool claimMission(Profile& p, const readingstats::Summary& s, int today, Mission mission) {
  if (p.campaign.state == AdventureState::Setup || today < 18262 || today > 84005 ||
      missionClaimed(p, today, mission) || missionValue(p, s, today, mission) < missionTarget(mission) ||
      !addReward(p, missionXp(mission), missionCoins(mission)))
    return false;
  switch (mission) {
    case Mission::Daily:
      p.claimedDay = today;
      break;
    case Mission::Weekly:
      p.campaign.claimedWeek = readingstats::weekStart(today, false);
      break;
    case Mission::Story:
      mark(p.campaign.storyClaims, missionChapter(p));
      break;
  }
  return true;
}
bool requestClaimed(const Profile& p, int id) {
  return validRequest(id) && (specialHunt(id) ? hasHuntClaim(p, id) : huntDefeated(p, id));
}
bool requestUnlocked(const Profile& p, int id) {
  if (!validRequest(id) || !p.campaign.initialized || !p.campaign.prologueSeen || !huntReached(p, id)) return false;
  if (specialHunt(id)) return true;
  if (p.activeRequest != id && (id != huntForPath(huntEncounter(id) / ENCOUNTERS_PER_CHAPTER) ||
                                (validRequest(p.activeRequest) && !specialHunt(p.activeRequest) &&
                                 guardian(huntEncounter(p.activeRequest)) == guardian(huntEncounter(id)))))
    return false;
  return true;
}
uint64_t requestMetric(const Profile& p, RequestGoal goal, const readingstats::Summary* summary) {
  switch (goal) {
    case RequestGoal::Minutes:
      return p.campaign.observedSeconds;
    case RequestGoal::Pages:
      return summary ? summary->pages : 0;
    case RequestGoal::Sessions:
      return summary ? summary->sessions : 0;
    case RequestGoal::Books:
      return summary ? summary->completed : 0;
    case RequestGoal::Days:
      return summary ? summary->totalDays : 0;
  }
  return 0;
}
unsigned requestValue(const Profile& p, int id, const readingstats::Summary* summary) {
  if (!validRequest(id)) return 0;
  const unsigned target = requestTarget(p, id);
  if (p.activeRequest != id && requestClaimed(p, id)) return target;
  if (id == kInkHunt) {
    if (p.activeRequest == id && p.requestReported == target) return target;
    const auto elapsed = p.activeRequest == id && p.campaign.observedSeconds > p.requestStarted
                             ? p.campaign.observedSeconds - p.requestStarted
                             : 0;
    const auto seconds = p.pausedInkSeconds + std::min<uint64_t>(elapsed, INK_HUNT_SECONDS);
    return unsigned(std::min<uint64_t>(target, seconds / 60));
  }
  if (p.activeRequest != id) return 0;
  if (p.requestReported == target) return target;
  const auto current = requestMetric(p, p.requestGoal, summary);
  const auto baseline = p.requestGoal == RequestGoal::Minutes ? p.requestStarted : p.requestBaseline;
  if (current < baseline) return 0;
  const auto delta = current - baseline;
  const uint64_t accumulated = p.requestCarried + std::min<uint64_t>(delta, REQUEST_METRIC_MAX);
  return unsigned(std::min<uint64_t>(target, p.requestGoal == RequestGoal::Minutes ? accumulated / 60 : accumulated));
}
namespace {
uint32_t huntElapsed(const Profile& p) {
  const auto elapsed =
      p.campaign.observedSeconds >= p.requestStarted ? p.campaign.observedSeconds - p.requestStarted : 0;
  return std::min<uint64_t>(UINT32_MAX, uint64_t(p.requestElapsed) +
                                            (p.activeRequest == kInkHunt ? p.pausedInkElapsed : 0) +
                                            std::min<uint64_t>(elapsed, UINT32_MAX));
}
uint32_t inkCheckpoint(const Profile& p) {
  const auto elapsed =
      p.campaign.observedSeconds > p.requestStarted ? p.campaign.observedSeconds - p.requestStarted : 0;
  return std::min<uint64_t>(INK_HUNT_SECONDS, p.pausedInkSeconds + std::min<uint64_t>(INK_HUNT_SECONDS, elapsed));
}
void discoverInkHunt(Profile& p, int id) {
  if (p.inkHuntDiscovered) return;
  unsigned victories = 0;
  for (int hunt = 0; hunt < kRequestCount; ++hunt)
    if (hunt != kInkHunt) victories += hasHuntClaim(p, hunt);
  if (victories < 3) return;
  uint32_t roll = uint32_t(p.campaign.observedSeconds) ^ (uint32_t(p.campaign.startedDay) << 14) ^
                  (uint32_t(id + 1) * 0x9e3779b9u) ^ uint32_t(p.campaign.originSeconds);
  roll ^= roll >> 16;
  roll *= 0x7feb352du;
  roll ^= roll >> 15;
  p.inkHuntDiscovered = victories >= 12 || roll % 8 == 0;
}
bool startRequest(Profile& p, int id) {
  if (p.campaign.state == AdventureState::Setup || p.campaign.observedSeconds < p.campaign.originSeconds ||
      !requestUnlocked(p, id))
    return false;
  p.requestTargetMinutes = id == kInkHunt && p.pausedInkTargetMinutes ? p.pausedInkTargetMinutes : huntMinutes(p, id);
  p.activeRequest = id;
  p.requestGoal = RequestGoal::Minutes;
  p.requestWinDay = p.requestWinSeconds = 0;
  p.requestCarried = 0;
  p.requestElapsed = 0;
  p.requestBooks = p.observedBooks == UINT32_MAX ? UINT32_MAX : 0;
  p.requestStarted = p.campaign.observedSeconds;
  p.requestBaseline = 0;
  p.requestReported = id == kInkHunt ? p.pausedInkSeconds / 60 : 0;
  return true;
}
}  // namespace
bool acceptRequest(Profile& p, int id) { return p.activeRequest == kRequestCount && startRequest(p, id); }
bool cancelRequest(Profile& p, int id, const readingstats::Summary* summary) {
  if (!validRequest(id) || p.activeRequest != id || requestValue(p, id, summary) >= requestTarget(p, id)) return false;
  if (id == kInkHunt) {
    p.pausedInkTargetMinutes = p.requestTargetMinutes;
    p.pausedInkSeconds = inkCheckpoint(p);
    p.pausedInkElapsed = huntElapsed(p);
    p.pausedInkBooks = p.requestBooks == UINT32_MAX || p.pausedInkBooks == UINT32_MAX
                           ? UINT32_MAX
                           : std::min<uint64_t>(UINT32_MAX - 1, uint64_t(p.pausedInkBooks) + p.requestBooks);
  }
  clearRequest(p);
  return true;
}
bool replaceRequest(Profile& p, int id, const readingstats::Summary* summary) {
  if (!validRequest(p.activeRequest) || p.activeRequest == id ||
      requestValue(p, p.activeRequest, summary) >= requestTarget(p, p.activeRequest))
    return false;
  const auto pausedTarget = p.activeRequest == kInkHunt ? p.requestTargetMinutes : p.pausedInkTargetMinutes;
  const auto paused = p.activeRequest == kInkHunt ? inkCheckpoint(p) : p.pausedInkSeconds;
  const auto pausedElapsed = p.activeRequest == kInkHunt ? huntElapsed(p) : p.pausedInkElapsed;
  const auto pausedBooks =
      p.activeRequest != kInkHunt ? p.pausedInkBooks
      : p.requestBooks == UINT32_MAX || p.pausedInkBooks == UINT32_MAX
          ? UINT32_MAX
          : uint32_t(std::min<uint64_t>(UINT32_MAX - 1, uint64_t(p.pausedInkBooks) + p.requestBooks));
  if (!startRequest(p, id)) return false;
  p.pausedInkTargetMinutes = pausedTarget;
  p.pausedInkSeconds = paused;
  p.pausedInkElapsed = pausedElapsed;
  p.pausedInkBooks = pausedBooks;
  return true;
}
bool collectRequest(Profile& p, int id, const readingstats::Summary* summary, int day) {
  const auto rewardXp = requestXp(p, id), rewardCoins = requestCoins(p, id);
  if (p.campaign.state == AdventureState::Setup || p.activeRequest != id || !requestUnlocked(p, id) ||
      requestValue(p, id, summary) < requestTarget(p, id) || !addReward(p, rewardXp, rewardCoins))
    return false;
  p.victories[huntOutfitIndex(huntOutfitItem(id))] = {
      p.requestWinDay ? p.requestWinDay : uint32_t(std::clamp(day, 0, 84005)),
      p.requestWinSeconds ? p.requestWinSeconds : huntElapsed(p), rewardXp, rewardCoins,
      p.requestBooks == UINT32_MAX || (id == kInkHunt && p.pausedInkBooks == UINT32_MAX)
          ? UINT32_MAX
          : uint32_t(std::min<uint64_t>(UINT32_MAX - 1,
                                        uint64_t(p.requestBooks) + (id == kInkHunt ? p.pausedInkBooks : 0)))};
  const auto wins = huntWinCount(p, id);
  p.huntWins[huntOutfitIndex(huntOutfitItem(id))] = wins == UINT32_MAX ? wins : wins + 1;
  mark(p.requestClaims, unsigned(id));
  noteAcquired(p, huntOutfitItem(id));
  if (!wins) offerOutfit(p, huntOutfitItem(id));
  if (id == kInkHunt) noteAcquired(p, kInkRelic);
  grantItem(p, requestDecoration(id));
  for (unsigned n = 0; n < 2; ++n) grantItem(p, huntExtraItem(id, n));
  if (id == kInkHunt) {
    p.pausedInkTargetMinutes = 0;
    p.pausedInkSeconds = 0;
    p.pausedInkElapsed = 0;
    p.pausedInkBooks = 0;
  } else
    discoverInkHunt(p, id);
  clearRequest(p);
  return true;
}
int availableHunts(const Profile& p, int (&ids)[MAX_AVAILABLE_HUNTS]) {
  int count = 0;
  const auto add = [&](int id) {
    if (!requestUnlocked(p, id) || count == MAX_AVAILABLE_HUNTS) return;
    for (int n = 0; n < count; ++n)
      if (ids[n] == id) return;
    ids[count++] = id;
  };
  add(p.activeRequest);
  if (!requestClaimed(p, kInkHunt)) add(kInkHunt);
  uint64_t wins = 0;
  for (int source : HUNT_OUTFIT_SOURCES) wins += huntWinCount(p, source);
  const auto pick = [&](int first, int end, uint64_t turn, bool repeat) {
    const int before = count, range = end - first;
    for (int n = 0; n < range && count == before; ++n) {
      const int id = first + (n + int(turn % unsigned(range))) % range;
      if (requestClaimed(p, id) == repeat) add(id);
    }
  };
  for (bool repeat : {false, true}) {
    pick(0, kCampaignHuntCount, completedPaths(p) * 7 + wins, repeat);
    pick(kCampaignHuntCount, kRequestCount, completedPaths(p) + wins, repeat);
    for (int n = 0; n < MAX_AVAILABLE_HUNTS && count < MAX_AVAILABLE_HUNTS; ++n) {
      pick(0, kCampaignHuntCount, wins + n + completedPaths(p), repeat);
      pick(kCampaignHuntCount, kRequestCount, wins + n + completedPaths(p), repeat);
    }
  }
  return count;
}
unsigned defeatedBeasts(const Profile& p) { return completedPaths(p) + unsigned(requestCount(p, true)); }
int requestCount(const Profile& p, bool collection) {
  if (!collection) {
    int ids[MAX_AVAILABLE_HUNTS];
    return availableHunts(p, ids);
  }
  int count = 0;
  for (int index = 0; index < kHuntOutfitCount; ++index) count += earnedHuntOutfit(p, huntOutfitAt(index));
  return count;
}
int requestAt(const Profile& p, bool collection, int index) {
  if (index < 0) return -1;
  if (!collection) {
    int ids[MAX_AVAILABLE_HUNTS];
    const int count = availableHunts(p, ids);
    return index < count ? ids[index] : -1;
  }
  const auto source = [&](int item) {
    const int canonical = huntOutfitSource(item);
    if (hasHuntClaim(p, canonical) || specialHunt(canonical)) return canonical;
    for (int old = 0; old < kLegacyRequestCount; ++old)
      if (hasHuntClaim(p, old) && huntOutfitItem(old) == item) return old;
    return canonical;
  };
  for (unsigned n = p.acquiredCount; n; --n) {
    const int item = p.acquired[n - 1];
    if (huntOutfit(item) && earnedHuntOutfit(p, item) && !index--) return source(item);
  }
  for (int n = kHuntOutfitCount - 1; n >= 0; --n) {
    const int item = huntOutfitAt(n);
    if (!earnedHuntOutfit(p, item)) continue;
    bool listed = false;
    for (unsigned at = 0; at < p.acquiredCount; ++at) listed |= p.acquired[at] == item;
    if (!listed && !index--) return source(item);
  }
  return -1;
}

}  // namespace quest
