#include "QuestCore.h"

#include <algorithm>
#include <cstring>

#include "QuestExpansionPolicy.h"
#include "QuestMilestones.h"
#include "QuestRequests.h"
namespace quest {
static_assert(kGearCount == questexp::EMPTY);
static_assert((kGearCount - kExpandedItemStart + 7) / 8 == 13);
namespace {
constexpr uint32_t LEGACY_BALANCE_MAX_LEVEL = 1000;
constexpr unsigned HUNT_WINS_OFFSET = 7124;
static_assert(HUNT_WINS_OFFSET + kHuntOutfitCount * 4 <= kProfileBytes - 4);
bool repeatProfile(const uint8_t* b) { return b[3] == 'A' && b[7110] >= 5; }

uint64_t originalThreshold(uint32_t n) {
  const uint64_t steps = n > 0 ? n - 1 : 0;
  return steps * 80 + steps * steps * 4;
}
uint32_t originalLevel(uint64_t xp) {
  uint32_t low = 1, high = LEGACY_BALANCE_MAX_LEVEL;
  while (low < high) {
    const auto middle = low + (high - low + 1) / 2;
    if (xp >= originalThreshold(middle))
      low = middle;
    else
      high = middle - 1;
  }
  return low;
}
void put(uint8_t* p, uint64_t v, int n) {
  for (int k = 0; k < n; ++k) p[k] = v >> (8 * k);
}
uint64_t get(const uint8_t* p, int n) {
  uint64_t v = 0;
  for (int k = 0; k < n; ++k) v |= uint64_t(p[k]) << (8 * k);
  return v;
}
bool bit(const uint8_t* b, int n) { return b[n / 8] & (1u << (n % 8)); }
void set(uint8_t* b, int n) { b[n / 8] |= 1u << (n % 8); }
bool reward(Profile& p, uint32_t amount) {
  if (p.bonus > UINT32_MAX - amount) return false;
  p.bonus += amount;
  p.xp = p.xp > UINT64_MAX - amount ? UINT64_MAX : p.xp + amount;
  return true;
}
struct StreakReward {
  uint16_t days, xp, coins;
};
constexpr StreakReward STREAK_REWARDS[] = {
    {3, 40, 10},    {7, 80, 25},    {14, 120, 50},   {30, 200, 100},
    {60, 300, 150}, {90, 400, 200}, {180, 600, 300}, {250, 800, 400},
};
const StreakReward* streakReward(int q) {
  if (q < 0 || q >= kQuestCount || q % 6 != 5) return nullptr;
  for (const auto& reward : STREAK_REWARDS)
    if (q / 6 + 1 == reward.days) return &reward;
  return nullptr;
}
uint32_t accumulatedLevelCoins(uint32_t n) { return 20 * (n - 1) + n * (n + 1) - 2; }
uint64_t earnedCoins(uint64_t xp, const uint8_t* claimed) {
  uint64_t total = xp / 10 + accumulatedLevelCoins(level(xp));
  for (const auto& reward : STREAK_REWARDS)
    if (bit(claimed, (reward.days - 1) * 6 + 5)) total += reward.coins;
  return total;
}
}  // namespace
uint64_t threshold(uint32_t n) {
  if (n < 2) return 0;
  const auto raw = originalThreshold(n);
  const auto at100 = originalThreshold(100);
  return n <= 100 ? raw : at100 + (raw - at100) * 4 / 5;
}
uint32_t level(uint64_t xp) {
  uint32_t low = 1, high = MAX_LEVEL;
  while (low < high) {
    const auto middle = low + (high - low + 1) / 2;
    if (xp >= threshold(middle))
      low = middle;
    else
      high = middle - 1;
  }
  return low;
}
uint64_t coins(const Profile& p) {
  const auto base = earnedCoins(p.xp, p.claimed);
  const auto earned = p.campaign.extraCoins > UINT64_MAX - base ? UINT64_MAX : base + p.campaign.extraCoins;
  return earned >= p.spent ? earned - p.spent : 0;
}
uint32_t levelRewardCoins(uint32_t n) { return n >= 2 && n <= MAX_LEVEL ? 20 + 2 * n : 0; }
bool hasLevelUp(const Profile& p) { return p.noticedLevel < level(p.xp); }
uint32_t pendingLevelCoins(const Profile& p) {
  return hasLevelUp(p) ? accumulatedLevelCoins(level(p.xp)) - accumulatedLevelCoins(p.noticedLevel) : 0;
}
bool acknowledgeLevel(Profile& p) {
  if (!hasLevelUp(p)) return false;
  p.noticedLevel = level(p.xp);
  return true;
}
void refresh(Profile& p, const readingstats::Summary& s) {
  const uint64_t minutes = s.seconds / 60, base = minutes > UINT64_MAX / 3 ? UINT64_MAX : minutes * 3;
  const uint64_t other = uint64_t(s.completed) * 120 + uint64_t(s.totalDays) * 15 + p.bonus;
  p.xp = std::max(p.xp, base > UINT64_MAX - other ? UINT64_MAX : base + other);
}
bool claim(Profile& p, int today, uint32_t seconds) {
  if (today < 18262 || today > 84005 || today <= p.claimedDay || seconds < DAILY_MINUTES * 60 || !reward(p, 60))
    return false;
  p.claimedDay = today;
  return true;
}
uint32_t questTarget(int q) {
  if (q < 0 || q >= kQuestCount) return 0;
  static constexpr uint32_t steps[] = {30, 25, 5, 2, 1, 1};
  return (q / 6 + 1) * steps[q % 6];
}
uint64_t questValue(int q, const readingstats::Summary& s) {
  switch (q % 6) {
    case 0:
      return s.seconds / 60;
    case 1:
      return s.pages;
    case 2:
      return s.sessions;
    case 3:
      return s.totalDays;
    case 4:
      return s.completed;
    case 5:
      return s.longest;
  }
  return 0;
}
bool isClaimed(const Profile& p, int q) { return q >= 0 && q < kQuestCount && bit(p.claimed, q); }
int orderedQuest(const Profile& p, const readingstats::Summary& stats, int index) {
  if (index < 0 || index >= kQuestCount) return -1;
  for (int group = 0; group < 3; ++group) {
    for (int q = 0; q < kQuestCount; ++q) {
      const int rank = isClaimed(p, q) ? 2 : questValue(q, stats) >= questTarget(q) ? 0 : 1;
      if (rank == group && index-- == 0) return q;
    }
  }
  return -1;
}
uint32_t questReward(int q) {
  if (q < 0 || q >= kQuestCount) return 0;
  if (const auto* reward = streakReward(q)) return reward->xp;
  static constexpr uint32_t rewards[] = {10, 10, 20, 20, 60, 10};
  return rewards[q % 6] + (q / 6) / 100 * 10;
}
uint32_t questCoinReward(int q) {
  const auto* reward = streakReward(q);
  return questReward(q) / 10 + (reward ? reward->coins : 0);
}
bool claimMilestone(Profile& p, int q, const readingstats::Summary& s) {
  if (q < 0 || q >= kQuestCount || isClaimed(p, q) || questValue(q, s) < questTarget(q) || !reward(p, questReward(q)))
    return false;
  set(p.claimed, q);
  return true;
}
Slot slot(int n) {
  if (n == kKingBody || huntOutfit(n)) return Slot::Body;
  if (n == kKingBackground) return Slot::Background;
  if (n == kKingPet) return Slot::Pet;
  return n < kOutfitCount                      ? Slot::Body
         : n < kOutfitCount + kBackgroundCount ? Slot::Background
         : n < kBaseGearCount                  ? Slot::Pet
                                               : Slot::Decoration;
}
int first(Slot s) {
  switch (s) {
    case Slot::Body:
      return 0;
    case Slot::Background:
      return kOutfitCount;
    case Slot::Pet:
      return kOutfitCount + kBackgroundCount;
    case Slot::Decoration:
      return kBaseGearCount;
  }
  return kGearCount;
}
int count(Slot s) {
  switch (s) {
    case Slot::Body:
      return kOutfitCount + 1 + kHuntOutfitCount;
    case Slot::Background:
      return kBackgroundCount + 1;
    case Slot::Pet:
      return kPetCount + 1;
    case Slot::Decoration:
      return kDecorationCount + 1;
  }
  return 0;
}
int catalogItem(Slot s, int index) {
  if (index < 0 || index >= count(s)) return kGearCount;
  switch (s) {
    case Slot::Body:
      if (index < kOutfitCount) return index;
      return index == kOutfitCount ? kKingBody : huntOutfitAt(index - kOutfitCount - 1);
    case Slot::Background:
      return index == kBackgroundCount ? kKingBackground : first(s) + index;
    case Slot::Pet:
      return index == kPetCount ? kKingPet : first(s) + index;
    case Slot::Decoration:
      return index == kDecorationCount ? kInkRelic : questexp::ACTIVE_DECORATIONS[index];
  }
  return kGearCount;
}
bool visibleItem(int item) { return (item >= 0 && item < kGearCount) || huntOutfit(item) || item == kInkRelic; }
bool availableItem(int item) {
  if (!visibleItem(item)) return false;
  if (slot(item) != Slot::Decoration || item == kInkRelic) return true;
  for (const auto active : questexp::ACTIVE_DECORATIONS)
    if (active == item) return true;
  return false;
}
int activeDecoration(const Profile& p) {
  for (int place = kDecorationPlaces - 1; place >= 0; --place) {
    const int item = p.decorations[place];
    if (availableItem(item) && decorationPlace(item) == place && owns(p, item)) return item;
  }
  return kGearCount;
}
bool normalizeDecorations(Profile& p) {
  const int active = activeDecoration(p);
  bool changed = false;
  for (int place = 0; place < kDecorationPlaces; ++place) {
    const int item = decorationPlace(active) == place ? active : kGearCount;
    changed |= p.decorations[place] != item;
    p.decorations[place] = item;
  }
  return changed;
}
int ownedCount(const Profile& p, Slot s) {
  int total = 0;
  for (int n = 0; n < count(s); ++n)
    if (owns(p, catalogItem(s, n))) ++total;
  return total;
}
int ownedItem(const Profile& p, Slot s, int index) {
  if (index < 0) return kGearCount;
  for (int n = 0; n < count(s); ++n)
    if (owns(p, catalogItem(s, n)) && index-- == 0) return catalogItem(s, n);
  return kGearCount;
}
int pathUnlockChapter(int item) {
  if (!availableItem(item) || storyExclusive(item)) return -1;
  for (unsigned n = 0; n < CAMPAIGN_CHAPTERS; ++n)
    if (chapterReward(n) == item || chapterDecoration(n) == item) return int(n);
  return -1;
}
bool shopUnlocked(const Profile& p, int item) {
  if (!availableItem(item) || storyExclusive(item)) return false;
  const int path = pathUnlockChapter(item);
  return path >= 0 ? completedPaths(p) > unsigned(path) : shopLevel(item) <= level(p.xp);
}
namespace {
bool listedInShop(const Profile& p, int item, ShopView view) {
  if (!availableItem(item) || storyExclusive(item) || owns(p, item)) return false;
  switch (view) {
    case ShopView::Available:
      return shopUnlocked(p, item);
    case ShopView::Catalogue:
      return true;
  }
  return false;
}
}  // namespace
int shopCount(const Profile& p, Slot s, ShopView view) {
  int total = 0;
  for (int n = 0; n < count(s); ++n) {
    const int item = catalogItem(s, n);
    if (listedInShop(p, item, view)) ++total;
  }
  return total;
}
int shopPage(const Profile& p, Slot s, int page, int pageSize, uint16_t (&items)[4], ShopView view) {
  std::fill(std::begin(items), std::end(items), uint16_t(kGearCount));
  if (pageSize < 1 || pageSize > 4) return 0;
  const int first = std::clamp(page, 0, kGearCount) * pageSize;
  int total = 0;
  for (int n = 0; n < count(s); ++n) {
    const int item = catalogItem(s, n);
    if (!listedInShop(p, item, view)) continue;
    if (total >= first && total - first < pageSize) items[total - first] = uint16_t(item);
    ++total;
  }
  return total;
}
int shopItem(const Profile& p, Slot s, int index, ShopView view) {
  if (index < 0) return kGearCount;
  for (int n = 0; n < count(s); ++n)
    if (const int item = catalogItem(s, n); listedInShop(p, item, view) && index-- == 0) return item;
  return kGearCount;
}
bool noteAcquired(Profile& p, int item, bool markNew) {
  if (!visibleItem(item) || !owns(p, item)) return false;
  for (unsigned n = 0; n < p.acquiredCount; ++n)
    if (p.acquired[n] == item) return false;
  if (p.acquiredCount >= kCollectionItemCount) return false;
  p.acquired[p.acquiredCount++] = uint16_t(item);
  if (markNew) set(p.unseen, item);
  return true;
}
void reconcileCollection(Profile& p) {
  for (int item = 0; item < kCollectionItemCount; ++item)
    if (visibleItem(item) && owns(p, item)) noteAcquired(p, item, false);
}
bool isNewItem(const Profile& p, int item) { return visibleItem(item) && owns(p, item) && bit(p.unseen, item); }
bool markItemSeen(Profile& p, int item) {
  if (!isNewItem(p, item)) return false;
  p.unseen[item / 8] &= uint8_t(~(1u << (item % 8)));
  return true;
}
bool isFavorite(const Profile& p, int item) { return visibleItem(item) && owns(p, item) && bit(p.favorites, item); }
bool toggleFavorite(Profile& p, int item) {
  if (!availableItem(item) || !owns(p, item)) return false;
  p.favorites[item / 8] ^= uint8_t(1u << (item % 8));
  return true;
}
int inventoryCount(const Profile& p, Slot category, bool favoritesOnly) {
  int count = 0;
  for (int n = 0; n < quest::count(category); ++n) {
    const int item = catalogItem(category, n);
    if (owns(p, item) && (!favoritesOnly || isFavorite(p, item))) ++count;
  }
  return count;
}
int inventoryItem(const Profile& p, Slot category, bool favoritesOnly, int index) {
  if (index < 0) return kGearCount;
  for (unsigned n = p.acquiredCount; n; --n) {
    const int item = p.acquired[n - 1];
    if (availableItem(item) && slot(item) == category && owns(p, item) && (!favoritesOnly || isFavorite(p, item)) &&
        !index--)
      return item;
  }
  for (int n = 0; n < quest::count(category); ++n) {
    const int item = catalogItem(category, n);
    if (!owns(p, item) || (favoritesOnly && !isFavorite(p, item))) continue;
    bool listed = false;
    for (unsigned at = 0; at < p.acquiredCount; ++at) listed |= p.acquired[at] == item;
    if (!listed && !index--) return item;
  }
  return kGearCount;
}
bool unequip(Profile& p, Slot s) {
  if (s != Slot::Background && s != Slot::Pet) return false;
  if (p.equipped[int(s)] == kGearCount) return false;
  p.equipped[int(s)] = kGearCount;
  return true;
}
uint32_t requiredLevel(int n) {
  if (!visibleItem(n)) return 101;
  if (n >= kExpandedItemStart) return 1;
  if (slot(n) == Slot::Decoration) return storyExclusive(n) ? 1 : 1 + (n - kBaseGearCount - 12) * 3;
  const int index = n - first(slot(n));
  return slot(n) == Slot::Body ? index + 1 : 1 + (index * 99 + 24) / 49;
}
uint32_t shopLevel(int item) {
  if (!availableItem(item) || storyExclusive(item)) return 1;
  const int path = pathUnlockChapter(item);
  if (path >= 0) return questbalance::recommendedLevel(unsigned(path));
  if (slot(item) == Slot::Decoration) return 1 + unsigned(item - kBaseGearCount - 12) * (MAX_LEVEL - 1) / 3;
  const auto original = std::min<uint32_t>(100, requiredLevel(item));
  return 1 + (original - 1) * (MAX_LEVEL - 1) / 99;
}
uint32_t price(int n) {
  if (!availableItem(n)) return UINT32_MAX;
  if (storyExclusive(n) || n == 0 || n == kOutfitCount) return 0;
  const auto tier = shopLevel(n);
  const uint32_t base = 40 + tier * 8 + tier * tier / 8;
  switch (slot(n)) {
    case Slot::Body:
      return base;
    case Slot::Background:
      return (base * 85 + 99) / 100;
    case Slot::Pet:
      return (base * 90 + 99) / 100;
    case Slot::Decoration:
      return (base * 110 + 99) / 100;
  }
  return base;
}
bool owns(const Profile& p, int n) {
  if (!visibleItem(n)) return false;
  if (huntOutfit(n)) return earnedHuntOutfit(p, n);
  if (n == kInkRelic) return hasHuntClaim(p, kInkHunt);
  if (n < kBaseGearCount) return bit(p.owned, n);
  if (n < kExpandedItemStart) return p.decorationOwned & (1u << (n - kBaseGearCount));
  return bit(p.expansionOwned, n - kExpandedItemStart);
}
int decorationPlace(int item) {
  static constexpr uint8_t PLACES[kLegacyDecorationCount] = {0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1};
  if (!visibleItem(item) || slot(item) != Slot::Decoration) return -1;
  if (item == kInkRelic) return 0;
  return item < kExpandedItemStart ? PLACES[item - kBaseGearCount]
                                   : questexp::DECORATIONS[item - kExpandedItemStart].place;
}
bool storyExclusive(int item) {
  const bool expandedExclusive =
      item >= kExpandedItemStart &&
      (item > kKingDecoration || questexp::DECORATIONS[item - kExpandedItemStart].source != questexp::Source::Chapter);
  return visibleItem(item) && (expandedExclusive || (slot(item) == Slot::Decoration && item < kBaseGearCount + 12) ||
                               rewardSaga(item) >= 0 || huntExtraSource(item) >= 0);
}
bool equipped(const Profile& p, int item) {
  if (!visibleItem(item)) return false;
  return slot(item) == Slot::Decoration ? p.decorations[decorationPlace(item)] == item
                                        : p.equipped[int(slot(item))] == item;
}
bool grantItem(Profile& p, int item) {
  if (!availableItem(item) || owns(p, item)) return false;
  if (huntOutfit(item) || item == kInkRelic) return false;
  if (item < kBaseGearCount)
    set(p.owned, item);
  else if (item < kExpandedItemStart)
    p.decorationOwned |= 1u << (item - kBaseGearCount);
  else
    set(p.expansionOwned, item - kExpandedItemStart);
  noteAcquired(p, item);
  return true;
}
bool removeItem(Profile& p, int item) {
  if (!equipped(p, item) || slot(item) == Slot::Body) return false;
  if (slot(item) == Slot::Decoration)
    p.decorations[decorationPlace(item)] = kGearCount;
  else
    p.equipped[int(slot(item))] = kGearCount;
  return true;
}
int chapterReward(unsigned chapter) {
  if (chapter >= CAMPAIGN_CHAPTERS) return kGearCount;
  const unsigned region = chapter / 10;
  switch (chapter % 10) {
    case 0:
      return 1 + region * 8;
    case 1:
      return region + 1 == CAMPAIGN_BOOKS ? kBaseGearCount + region : kGearCount;
    case 2:
      return first(Slot::Pet) + region * 4;
    case 3:
      return first(Slot::Background) + region * 4 + 1;
    case 4:
      return 3 + region * 8;
    case 5:
      return first(Slot::Background) + region * 4 + 2;
    case 6:
      return 5 + region * 8;
    case 7:
      return first(Slot::Pet) + region * 4 + 2;
    case 8:
      return 7 + region * 8;
    case 9:
      return first(Slot::Background) + (region == CAMPAIGN_BOOKS - 1 ? 49 : region * 4 + 4);
  }
  return kGearCount;
}
int chapterDecoration(unsigned chapter) {
  return chapter < CAMPAIGN_CHAPTERS ? questexp::CHAPTER_REWARDS[chapter] : kGearCount;
}
int sagaReward(unsigned book, Slot category) {
  static constexpr uint16_t SETS[][kCategoryCount] = {
      {6, 104, 159, 200},  {10, 108, 153, 201}, {54, 112, 193, 202},
      {52, 116, 195, 203}, {56, 120, 191, 204}, {40, 124, 185, 205},
      {46, 128, 179, 206}, {62, 132, 173, 207}, {78, 136, 177, 208},
      {58, 140, 189, 209}, {68, 144, 199, 210}, {kKingBody, kKingBackground, kKingPet, kKingDecoration},
  };
  static_assert(sizeof(SETS) / sizeof(SETS[0]) == CAMPAIGN_BOOKS);
  return book < CAMPAIGN_BOOKS && unsigned(category) < kCategoryCount ? SETS[book][unsigned(category)] : kGearCount;
}
CampaignRewards campaignRewards(unsigned chapter) {
  CampaignRewards result;
  if (chapter >= CAMPAIGN_CHAPTERS) return result;
  const auto add = [&](int item) {
    if (!visibleItem(item)) return;
    for (unsigned n = 0; n < result.count; ++n)
      if (result.items[n] == item) return;
    result.items[result.count++] = uint16_t(item);
  };
  if (lastPathInSaga(chapter))
    for (auto category : {Slot::Body, Slot::Background, Slot::Pet, Slot::Decoration})
      add(sagaReward(chapter / CHAPTERS_PER_BOOK, category));
  return result;
}
CampaignRewards campaignUnlocks(unsigned chapter) {
  CampaignRewards result;
  if (chapter >= CAMPAIGN_CHAPTERS) return result;
  for (int item : {chapterReward(chapter), chapterDecoration(chapter)}) {
    if (!availableItem(item) || storyExclusive(item)) continue;
    if (result.count && result.items[0] == item) continue;
    result.items[result.count++] = uint16_t(item);
  }
  return result;
}
int nextOutfitOffer(const Profile& p) {
  for (int item : p.outfitOffers)
    if (availableItem(item) && slot(item) == Slot::Body && owns(p, item) && !equipped(p, item)) return item;
  return kGearCount;
}
bool offerOutfit(Profile& p, int item) {
  unsigned kept = 0;
  for (int offered : p.outfitOffers)
    if (availableItem(offered) && owns(p, offered) && !equipped(p, offered)) p.outfitOffers[kept++] = uint16_t(offered);
  while (kept < 2) p.outfitOffers[kept++] = kGearCount;
  if (!availableItem(item) || slot(item) != Slot::Body || !owns(p, item) || equipped(p, item) || !isNewItem(p, item))
    return false;
  for (int offered : p.outfitOffers)
    if (offered == item) return false;
  for (auto& offered : p.outfitOffers)
    if (offered == kGearCount) {
      offered = uint16_t(item);
      return true;
    }
  return false;
}
bool finishOutfitOffer(Profile& p, int item, bool equipNow) {
  unsigned at = 0;
  while (at < 2 && p.outfitOffers[at] != item) ++at;
  if (at == 2 || !owns(p, item) || (equipNow && !equip(p, item))) return false;
  markItemSeen(p, item);
  for (; at + 1 < 2; ++at) p.outfitOffers[at] = p.outfitOffers[at + 1];
  p.outfitOffers[1] = kGearCount;
  return true;
}
int rewardSaga(int item) {
  if (!visibleItem(item)) return -1;
  const auto category = slot(item);
  for (unsigned book = 0; book < CAMPAIGN_BOOKS; ++book)
    if (sagaReward(book, category) == item) return int(book);
  return -1;
}
int requestDecoration(int request) {
  return request >= 0 && request < kLegacyRequestCount ? questexp::REQUEST_REWARDS[request] : kGearCount;
}
bool hasWon(const Profile& p) {
  return p.campaign.encounter == CAMPAIGN_ENCOUNTERS - 1 &&
         (p.campaign.phase == BattlePhase::Complete || p.campaign.state == AdventureState::Peace);
}
bool restoreExpansionRewards(Profile& p) {
  bool changed = normalizeDecorations(p);
  if (visibleItem(p.campaign.pendingReward) && !availableItem(p.campaign.pendingReward)) {
    p.campaign.pendingReward = kGearCount;
    changed = true;
  }
  if (hasWon(p) && p.campaign.state == AdventureState::Active) {
    p.campaign.state = AdventureState::Peace;
    p.campaign.bankSeconds = 0;
    p.endingSeen = false;
    changed = true;
  }
  if (p.campaign.state == AdventureState::Peace && p.campaign.phase != BattlePhase::Complete) {
    p.campaign.phase = BattlePhase::Complete;
    p.campaign.pendingReward = kGearCount;
    p.campaign.bankSeconds = 0;
    changed = true;
  }
  const unsigned recovered = completedPaths(p);
  for (unsigned book = 0; book < recovered / CHAPTERS_PER_BOOK; ++book)
    for (auto category : {Slot::Body, Slot::Background, Slot::Pet, Slot::Decoration})
      changed |= grantItem(p, sagaReward(book, category));
  for (int request = 0; request < kRequestCount; ++request)
    if (bit(p.requestClaims, request)) changed |= grantItem(p, requestDecoration(request));
  return changed;
}
int rewardEncounter(int item) {
  const int saga = rewardSaga(item);
  if (saga >= 0) return battleAnchor((unsigned(saga) + 1) * CHAPTERS_PER_BOOK - 1);
  for (unsigned chapter = 0; chapter < CAMPAIGN_CHAPTERS; ++chapter)
    if (chapterReward(chapter) == item) return chapter * ENCOUNTERS_PER_CHAPTER + ENCOUNTERS_PER_CHAPTER - 1;
  return -1;
}
int nextStoryReward(const Profile& p) {
  unsigned chapter = p.campaign.encounter / ENCOUNTERS_PER_CHAPTER;
  if ((p.campaign.phase == BattlePhase::Story || p.campaign.phase == BattlePhase::Complete) &&
      p.campaign.encounter % 6 == 5)
    ++chapter;
  return chapterReward(chapter);
}
void restoreStoryRewards(Profile& p) { restoreExpansionRewards(p); }
bool dismissStoryReward(Profile& p, bool place) {
  const int item = p.campaign.pendingReward;
  if (p.campaign.state != AdventureState::Active || p.campaign.phase != BattlePhase::Story || !owns(p, item))
    return false;
  if (place && !equip(p, item)) return false;
  const unsigned currentChapter = p.campaign.encounter / ENCOUNTERS_PER_CHAPTER;
  const int extra = chapterDecoration(currentChapter);
  if (item == chapterReward(currentChapter) && visibleItem(extra)) {
    grantItem(p, extra);
    p.campaign.pendingReward = extra;
  } else
    p.campaign.pendingReward = kGearCount;
  return true;
}
bool equip(Profile& p, int n) {
  if (!availableItem(n)) return false;
  if (!owns(p, n)) {
    if (!shopUnlocked(p, n) || coins(p) < price(n)) return false;
    p.spent += price(n);
    grantItem(p, n);
  }
  if (slot(n) == Slot::Decoration) {
    for (auto& item : p.decorations) item = kGearCount;
    p.decorations[decorationPlace(n)] = n;
  } else
    p.equipped[int(slot(n))] = n;
  return true;
}
namespace {
void commonFields(const uint8_t* b, Profile& p) {
  static constexpr Profile fresh;
  p = fresh;
  p.sequence = get(b + 4, 4);
  p.xp = get(b + 8, 8);
  p.spent = get(b + 16, 8);
  p.bonus = get(b + 24, 4);
  p.claimedDay = get(b + 28, 4);
  p.noticedLevel = level(p.xp);
  p.celebratedMilestones = questmilestone::reached(level(p.xp));
}
bool validCommon(const uint8_t* b, bool modern = false, uint64_t extraCoins = 0) {
  const uint64_t xp = get(b + 8, 8);
  uint64_t base = modern ? earnedCoins(xp, b + 64) : xp / 10;
  if (modern && b[3] == 'A' && b[7110] == 1) {
    const auto original = accumulatedLevelCoins(originalLevel(xp));
    const auto current = accumulatedLevelCoins(level(xp));
    if (original > current) base += original - current;
  }
  const uint64_t earned = extraCoins > UINT64_MAX - base ? UINT64_MAX : extraCoins + base;
  return get(b + 28, 4) <= 84005 && get(b + 16, 8) <= earned;
}
int migrateItem(int old) {
  if (old < 0 || old >= 375) return kGearCount;
  if (old < 150) return (old % 50) * 2;
  if (old < 350) return first(Slot::Background) + (old - 150) / 4;
  static constexpr uint8_t legacyPets[] = {3,  5,  4,  1,  0,  6,  7,  8,  2,  9,  10, 11, 12,
                                           13, 14, 32, 30, 39, 33, 40, 42, 41, 46, 47, 49};
  return first(Slot::Pet) + legacyPets[old - 350];
}
bool expandedLegacy(const uint8_t* b, Profile& p, bool v3) {
  const int checksumAt = v3 ? 316 : 252;
  if (std::memcmp(b, v3 ? "RQU3" : "RQU2", 4) || get(b + checksumAt, 4) != readingstats::checksum(b, checksumAt) ||
      !validCommon(b) || b[32] > 1 || (b[89] & 0x80) || (v3 && (b[277] & 0xF0)))
    return false;
  static constexpr int starts[] = {0, 50, 100, 150, 350};
  static constexpr int counts[] = {50, 50, 50, 200, 25};
  for (int n = 0; n < 5; ++n) {
    const int item = get(b + 33 + n * 2, 2);
    if (item != 375 && (item < starts[n] || item >= starts[n] + counts[n] || !bit(b + 43, item))) return false;
  }
  commonFields(b, p);
  p.gender = Gender(b[32]);
  // Retired head/foot purchases become the matching complete outfit. Currency is unchanged.
  for (int item = 0; item < 375; ++item)
    if (bit(b + 43, item)) set(p.owned, migrateItem(item));
  const int body = get(b + 35, 2), aura = get(b + 39, 2), pet = get(b + 41, 2);
  p.equipped[int(Slot::Body)] = body == 375 ? 0 : migrateItem(body);
  p.equipped[int(Slot::Background)] = migrateItem(aura);
  p.equipped[int(Slot::Pet)] = migrateItem(pet);
  std::memcpy(p.claimed, b + 90, v3 ? sizeof(p.claimed) : 125);
  return true;
}
}  // namespace
void encode(const Profile& p, uint8_t* b) {
  std::memset(b, 0, kProfileBytes);
  std::memcpy(b, "RQUA", 4);
  put(b + 4, p.sequence, 4);
  put(b + 8, p.xp, 8);
  put(b + 16, p.spent, 8);
  put(b + 24, p.bonus, 4);
  put(b + 28, uint32_t(p.claimedDay), 4);
  b[32] = uint8_t(p.gender);
  for (int n = 0; n < kSlotCount; ++n)
    put(b + 33 + n * 2, p.equipped[n] == kGearCount ? kBaseGearCount : p.equipped[n], 2);
  std::memcpy(b + 39, p.owned, sizeof(p.owned));
  std::memcpy(b + 64, p.claimed, sizeof(p.claimed));
  b[252] = std::min<unsigned>(100, p.noticedLevel);
  static_assert(253 + NAME_BYTES <= 278);
  std::memcpy(b + 253, p.name, NAME_BYTES);
  b[278] = 1;  // Optional name extension in the reserved RQU5 bytes.
  b[279] = p.inkHuntDiscovered;
  put(b + 280, p.pausedInkSeconds, 4);
  put(b + 284, p.requestReported, 2);
  static_assert(286 + (kSpecialHuntCount + 7) / 8 <= 316);
  for (int id = kCampaignHuntCount; id < kRequestCount; ++id)
    if (hasHuntClaim(p, id)) {
      const int index = id - kCampaignHuntCount;
      b[286 + index / 8] |= 1u << (index % 8);
    }
  put(b + 316, readingstats::checksum(b, 316), 4);
  const auto& c = p.campaign;
  put(b + 320, c.observedSeconds, 8);
  put(b + 328, c.originSeconds, 8);
  put(b + 336, c.bankSeconds, 8);
  put(b + 344, c.extraCoins, 8);
  put(b + 352, uint32_t(c.startedDay), 4);
  put(b + 356, c.startDaySeconds, 4);
  put(b + 360, c.originCompleted, 4);
  put(b + 364, c.encounterSeconds, 4);
  put(b + 368, c.victoryCoins, 4);
  put(b + 372, uint32_t(c.claimedWeek), 4);
  put(b + 376, c.encounter, 2);
  put(b + 378, c.xpRemainder, 2);
  b[380] = c.initialized;
  b[381] = uint8_t(c.phase);
  b[382] = uint8_t(c.state);
  b[383] = 1;
  std::memcpy(b + 384, c.storyClaims, sizeof(c.storyClaims));
  b[399] = c.firstDayRead;
  b[400] = c.introducedBooks;
  b[401] = 8;
  b[402] = c.prologueSeen;
  put(b + 403, c.narratedEncounters, 2);
  put(b + 405, p.decorationOwned, 2);
  for (int n = 0; n < kDecorationPlaces; ++n) put(b + 407 + n * 2, p.decorations[n], 2);
  put(b + 411, c.pendingReward, 2);
  constexpr unsigned legacyClaimBytes = (kCampaignHuntCount + 7) / 8;
  std::memcpy(b + 413, p.requestClaims, legacyClaimBytes);
  b[450] &= 0x3f;
  put(b + 451, p.requestStarted, 8);
  put(b + 459, p.activeRequest, 2);
  std::memcpy(b + 461, p.expansionOwned, sizeof(p.expansionOwned));
  b[474] = p.endingSeen;
  static_assert(476 + CAMPAIGN_BOOKS * 2 <= 508);
  b[475] = 1;
  for (unsigned n = 0; n < CAMPAIGN_BOOKS; ++n) put(b + 476 + n * 2, p.sagaPositions[n], 2);
  put(b + 500, (uint64_t(1) << 63) | (p.requestBaseline << 9) | uint8_t(p.requestGoal), 8);
  put(b + 508, readingstats::checksum(b, 508), 4);
  std::memcpy(b + 512, "QCOL", 4);
  b[516] = 1;
  put(b + 518, p.acquiredCount, 2);
  put(b + 520, p.savedDay, 4);
  std::memcpy(b + 524, p.favorites, kCollectionBits);
  std::memcpy(b + 524 + kCollectionBits, p.unseen, kCollectionBits);
  constexpr int itemsAt = 524 + 2 * kCollectionBits;
  for (int n = 0; n < kCollectionItemCount; ++n) put(b + itemsAt + n * 2, p.acquired[n], 2);
  constexpr int winsAt = itemsAt + kCollectionItemCount * 2;
  static_assert(winsAt + kHuntOutfitCount * 16 + 12 <= kProfileBytes - 4);
  for (int n = 0; n < kHuntOutfitCount; ++n) {
    put(b + winsAt + n * 16, p.victories[n].day, 4);
    put(b + winsAt + n * 16 + 4, p.victories[n].seconds, 4);
    put(b + winsAt + n * 16 + 8, p.victories[n].xp, 4);
    put(b + winsAt + n * 16 + 12, p.victories[n].coins, 4);
  }
  put(b + winsAt + kHuntOutfitCount * 16, p.requestCarried, 4);
  put(b + winsAt + kHuntOutfitCount * 16 + 4, p.requestElapsed, 4);
  put(b + winsAt + kHuntOutfitCount * 16 + 8, p.pausedInkElapsed, 4);
  std::memcpy(b + kV8ProfileBytes, "QWIN", 4);
  b[kV8ProfileBytes + 4] = 1;
  constexpr int campaignAt = kV8ProfileBytes + 8;
  for (unsigned n = 0; n < CAMPAIGN_CHAPTERS; ++n) {
    const auto& win = p.campaignVictories[n];
    const uint32_t values[] = {win.day, win.seconds, win.xp, win.coins, win.books};
    for (unsigned field = 0; field < 5; ++field) put(b + campaignAt + n * 20 + field * 4, values[field], 4);
  }
  constexpr int booksAt = campaignAt + CAMPAIGN_CHAPTERS * 20;
  for (int n = 0; n < kHuntOutfitCount; ++n) put(b + booksAt + n * 4, p.victories[n].books, 4);
  constexpr int countersAt = booksAt + kHuntOutfitCount * 4;
  const uint32_t counters[] = {p.observedBooks,  p.battleElapsed, p.battleBooks,       p.requestBooks,
                               p.pausedInkBooks, p.requestWinDay, p.requestWinSeconds, uint32_t(p.battleStatsKnown)};
  static_assert(countersAt + sizeof(counters) <= 7104);
  for (unsigned n = 0; n < sizeof(counters) / sizeof(*counters); ++n) put(b + countersAt + n * 4, counters[n], 4);
  std::memcpy(b + 7104, "QBAL", 4);
  put(b + 7108, p.noticedLevel, 2);
  b[7110] = BALANCE_REVISION;
  put(b + 7112, p.requestTargetMinutes, 2);
  put(b + 7114, p.pausedInkTargetMinutes, 2);
  b[7116] = p.celebratedMilestones;
  put(b + 7118, p.outfitOffers[0], 2);
  put(b + 7120, p.outfitOffers[1], 2);
  for (unsigned n = 0; n < kHuntOutfitCount; ++n) put(b + HUNT_WINS_OFFSET + n * 4, p.huntWins[n], 4);
  put(b + kProfileBytes - 4, readingstats::checksum(b, kProfileBytes - 4), 4);
}
namespace {
bool decodeCurrent(const uint8_t* b, Profile& p, char version) {
  const bool named = version >= '5', campaign = version >= '6';
  const int checksumAt = campaign ? 508 : 316;
  if (std::memcmp(b, "RQU", 3) || b[3] != version || get(b + checksumAt, 4) != readingstats::checksum(b, checksumAt) ||
      !validCommon(b, named, campaign ? get(b + 344, 8) : 0) ||
      (named && (b[252] < 1 || b[252] > level(get(b + 8, 8)))) || b[32] > 1 || (b[251] & 0xF0) || !(b[39] & 1))
    return false;
  commonFields(b, p);
  if (campaign && b[401] >= 4) {
    const bool legacyHunts = b[401] < 6;
    if (b[450] & (legacyHunts ? 0xf0 : 0xc0)) return false;
    std::memcpy(p.requestClaims, b + 413, (kCampaignHuntCount + 7) / 8);
    if (b[401] >= 7) {
      const int specialCount = b[401] == 7 ? 4 : kSpecialHuntCount;
      if (b[279] > 1 || (b[286 + specialCount / 8] & uint8_t(0xffu << (specialCount % 8)))) return false;
      p.inkHuntDiscovered = b[279];
      p.pausedInkSeconds = get(b + 280, 4);
      for (int index = 0; index < specialCount; ++index)
        if (b[286 + index / 8] & (1u << (index % 8))) set(p.requestClaims, kCampaignHuntCount + index);
      if (p.pausedInkSeconds >= INK_HUNT_SECONDS ||
          (!p.inkHuntDiscovered && (p.pausedInkSeconds || hasHuntClaim(p, kInkHunt))) ||
          (!repeatProfile(b) && hasHuntClaim(p, kInkHunt) && p.pausedInkSeconds))
        return false;
    }
  }
  if (campaign && b[401] >= 3) p.decorationOwned = get(b + 405, 2);
  if (campaign && b[401] >= 5) {
    std::memcpy(p.expansionOwned, b + 461, sizeof(p.expansionOwned));
    if (b[474] > 1) return false;
    p.endingSeen = b[474];
    if (b[475] > 1) return false;
    if (b[475]) {
      for (unsigned n = 0; n < CAMPAIGN_BOOKS; ++n) {
        const auto position = get(b + 476 + n * 2, 2);
        if ((position >> 8) > CHAPTERS_PER_BOOK * ENCOUNTERS_PER_CHAPTER * 2) return false;
        p.sagaPositions[n] = position;
      }
    }
  }
  std::memcpy(p.owned, b + 39, sizeof(p.owned));
  for (int n = 0; n < kSlotCount; ++n) {
    const int item = get(b + 33 + n * 2, 2);
    if (item == kBaseGearCount) {
      if (n == int(Slot::Body)) return false;
    } else if (!visibleItem(item) || int(slot(item)) != n || !owns(p, item))
      return false;
  }
  p.gender = Gender(b[32]);
  for (int n = 0; n < kSlotCount; ++n) {
    const int item = get(b + 33 + n * 2, 2);
    p.equipped[n] = item == kBaseGearCount ? kGearCount : item;
  }
  std::memcpy(p.owned, b + 39, sizeof(p.owned));
  std::memcpy(p.claimed, b + 64, sizeof(p.claimed));
  if (named) {
    p.noticedLevel = b[252];
    if (b[278] > 1) return false;
    if (b[278] == 1) {
      std::memcpy(p.name, b + 253, NAME_BYTES);
      if (!validName(p.name)) return false;
    }
  }
  if (campaign) {
    if (b[380] > 1 || b[381] > uint8_t(BattlePhase::Complete) || b[382] > uint8_t(AdventureState::Setup) ||
        b[383] != 1 || b[399] > 1 || get(b + 376, 2) >= CAMPAIGN_ENCOUNTERS || get(b + 378, 2) >= 6000 ||
        get(b + 352, 4) > 84005 || get(b + 372, 4) > 84005 || get(b + 364, 4) > 86400 ||
        (b[381] == uint8_t(BattlePhase::Complete) && get(b + 376, 2) != CAMPAIGN_ENCOUNTERS - 1) || b[401] > 8 ||
        (b[401] >= 1 && b[400] > get(b + 376, 2) / (CHAPTERS_PER_BOOK * ENCOUNTERS_PER_CHAPTER) + 1) ||
        (b[401] >= 2 && (b[402] > 1 || get(b + 403, 2) > get(b + 376, 2) + 1)))
      return false;
    auto& c = p.campaign;
    c.observedSeconds = get(b + 320, 8);
    c.originSeconds = get(b + 328, 8);
    c.bankSeconds = get(b + 336, 8);
    c.extraCoins = get(b + 344, 8);
    c.startedDay = get(b + 352, 4);
    c.startDaySeconds = get(b + 356, 4);
    c.originCompleted = get(b + 360, 4);
    c.encounterSeconds = get(b + 364, 4);
    c.victoryCoins = get(b + 368, 4);
    c.claimedWeek = get(b + 372, 4);
    c.encounter = get(b + 376, 2);
    c.xpRemainder = get(b + 378, 2);
    c.initialized = b[380];
    c.firstDayRead = b[399];
    c.phase = BattlePhase(b[381]);
    c.state = AdventureState(b[382]);
    const bool progressed = c.encounter || c.encounterSeconds || c.bankSeconds || c.phase != BattlePhase::Reading;
    c.introducedBooks = b[401] >= 1  ? b[400]
                        : progressed ? c.encounter / (CHAPTERS_PER_BOOK * ENCOUNTERS_PER_CHAPTER) + 1
                                     : 0;
    c.prologueSeen = b[401] >= 2 ? b[402] : c.introducedBooks != 0;
    c.narratedEncounters =
        b[401] >= 2
            ? get(b + 403, 2)
            : c.encounter + (c.introducedBooks > c.encounter / (CHAPTERS_PER_BOOK * ENCOUNTERS_PER_CHAPTER) ? 1 : 0);
    std::memcpy(c.storyClaims, b + 384, sizeof(c.storyClaims));
    if (b[401] >= 3) {
      p.decorationOwned = get(b + 405, 2);
      for (int n = 0; n < kDecorationPlaces; ++n) {
        int item = get(b + 407 + n * 2, 2);
        if (b[401] < 5 && item == kExpandedItemStart) item = kGearCount;
        if (item != kGearCount && (decorationPlace(item) != n || !owns(p, item))) return false;
        p.decorations[n] = item;
      }
      c.pendingReward = get(b + 411, 2);
      if (b[401] < 5 && c.pendingReward == kExpandedItemStart) c.pendingReward = kGearCount;
      const bool extraReward =
          c.pendingReward >= kExpandedItemStart && c.pendingReward <= kKingDecoration &&
          questexp::DECORATIONS[c.pendingReward - kExpandedItemStart].source == questexp::Source::Chapter &&
          questexp::DECORATIONS[c.pendingReward - kExpandedItemStart].sourceId == c.encounter / 6;
      const unsigned pendingChapter = c.encounter / ENCOUNTERS_PER_CHAPTER;
      const bool legacyRegionalReward = pendingChapter % CHAPTERS_PER_BOOK == 1 &&
                                        c.pendingReward == kBaseGearCount + pendingChapter / CHAPTERS_PER_BOOK;
      if (c.pendingReward != kGearCount &&
          (c.phase != BattlePhase::Story || c.encounter % 6 != 5 ||
           (c.pendingReward != chapterReward(pendingChapter) && !extraReward && !legacyRegionalReward) ||
           !owns(p, c.pendingReward)))
        return false;
    } else
      restoreStoryRewards(p);
    if (b[401] >= 4) {
      p.requestStarted = get(b + 451, 8);
      p.activeRequest = get(b + 459, 2);
      if (b[401] < 8) {
        const int oldSentinel = b[401] < 6    ? kLegacyRequestCount
                                : b[401] == 6 ? kCampaignHuntCount
                                              : kBestiaryHuntStart;
        if (p.activeRequest > oldSentinel) return false;
        if (p.activeRequest == oldSentinel) p.activeRequest = kRequestCount;
      }
      const auto requestExtension = get(b + 500, 8);
      if (requestExtension) {
        if (!(requestExtension >> 63) || (requestExtension & 7) > uint8_t(RequestGoal::Days)) return false;
        p.requestGoal = RequestGoal(requestExtension & 7);
        p.requestReported = b[401] >= 7 ? get(b + 284, 2) : (requestExtension >> 3) & 63;
        p.requestBaseline = (requestExtension >> 9) & REQUEST_METRIC_MAX;
      }
      if (p.activeRequest > kRequestCount) return false;
      p.requestTargetMinutes = b[3] == 'A'                     ? get(b + 7112, 2)
                               : validRequest(p.activeRequest) ? legacyRequestMinutes(p.activeRequest)
                                                               : 0;
      p.pausedInkTargetMinutes = b[3] == 'A' ? get(b + 7114, 2) : p.pausedInkSeconds ? 1800 : 0;
      if (p.activeRequest == kRequestCount) {
        if (p.requestStarted || p.requestBaseline || p.requestReported || p.requestGoal != RequestGoal::Minutes)
          return false;
      } else {
        if (!c.initialized || !c.prologueSeen || !huntReached(p, p.activeRequest) ||
            (!repeatProfile(b) && bit(p.requestClaims, p.activeRequest)) || p.requestStarted < c.originSeconds ||
            p.requestStarted > c.observedSeconds || (p.requestGoal == RequestGoal::Minutes && p.requestBaseline) ||
            (specialHunt(p.activeRequest) && p.requestGoal != RequestGoal::Minutes) ||
            p.requestReported > requestTarget(p, p.activeRequest))
          return false;
      }
    }
    if (p.endingSeen && c.state != AdventureState::Unfinished && !hasWon(p)) return false;
  }
  normalizeDecorations(p);
  if (visibleItem(p.campaign.pendingReward) && !availableItem(p.campaign.pendingReward))
    p.campaign.pendingReward = kGearCount;
  return true;
}
}  // namespace
bool decodeV6(const uint8_t* b, Profile& p) {
  if (!decodeCurrent(b, p, b[3] == '7' ? '7' : '6')) return false;
  p.pausedInkElapsed = p.pausedInkSeconds;
  if (validRequest(p.activeRequest)) p.requestBooks = UINT32_MAX;
  if (p.pausedInkSeconds) p.pausedInkBooks = UINT32_MAX;
  reconcileCollection(p);
  return true;
}
bool decodeV8(const uint8_t* b, Profile& p) {
  if (b[3] != '8' && b[3] != '9' && b[3] != 'A') return decodeV6(b, p);
  if (get(b + (b[3] == '8' ? kV8ProfileBytes : kProfileBytes) - 4, 4) !=
          readingstats::checksum(b, (b[3] == '8' ? kV8ProfileBytes : kProfileBytes) - 4) ||
      std::memcmp(b + 512, "QCOL", 4) || b[516] != 1 || b[517] || !decodeCurrent(b, p, char(b[3])))
    return false;
  p.acquiredCount = get(b + 518, 2);
  p.savedDay = get(b + 520, 4);
  if (p.acquiredCount > kCollectionItemCount || p.savedDay > 84005) return false;
  std::memcpy(p.favorites, b + 524, kCollectionBits);
  std::memcpy(p.unseen, b + 524 + kCollectionBits, kCollectionBits);
  constexpr int itemsAt = 524 + 2 * kCollectionBits;
  uint8_t seen[kCollectionBits]{};
  for (unsigned n = 0; n < p.acquiredCount; ++n) {
    const int item = get(b + itemsAt + n * 2, 2);
    if (!visibleItem(item) || !owns(p, item) || bit(seen, item)) return false;
    p.acquired[n] = item;
    set(seen, item);
  }
  for (int n = 0; n < kCollectionItemCount; ++n)
    if ((bit(p.favorites, n) || bit(p.unseen, n)) && (!visibleItem(n) || !owns(p, n))) return false;
  constexpr int winsAt = itemsAt + kCollectionItemCount * 2;
  for (int n = 0; n < kHuntOutfitCount; ++n) {
    p.victories[n] = {uint32_t(get(b + winsAt + n * 16, 4)), uint32_t(get(b + winsAt + n * 16 + 4, 4)),
                      uint32_t(get(b + winsAt + n * 16 + 8, 4)), uint32_t(get(b + winsAt + n * 16 + 12, 4))};
    const auto& win = p.victories[n];
    if (win.day > 84005 || ((win.day || win.seconds || win.xp || win.coins) && !earnedHuntOutfit(p, huntOutfitAt(n))))
      return false;
  }
  if (b[3] == '8') {
    if (validRequest(p.activeRequest)) p.requestBooks = UINT32_MAX;
    if (p.pausedInkSeconds) p.pausedInkBooks = UINT32_MAX;
  }
  p.requestCarried = uint32_t(get(b + winsAt + kHuntOutfitCount * 16, 4));
  p.requestElapsed = uint32_t(get(b + winsAt + kHuntOutfitCount * 16 + 4, 4));
  p.pausedInkElapsed = uint32_t(get(b + winsAt + kHuntOutfitCount * 16 + 8, 4));
  if ((p.requestElapsed && !validRequest(p.activeRequest)) ||
      (p.pausedInkElapsed && (!p.inkHuntDiscovered || (!repeatProfile(b) && hasHuntClaim(p, kInkHunt)))))
    return false;
  if (p.requestCarried && (p.activeRequest == kRequestCount || p.activeRequest == kInkHunt ||
                           p.requestCarried > uint64_t(requestTarget(p, p.activeRequest)) *
                                                  (p.requestGoal == RequestGoal::Minutes ? 60 : 1)))
    return false;
  return true;
}
bool decodeV9(const uint8_t* b, Profile& p) {
  if (b[3] != '9' && b[3] != 'A') return decodeV8(b, p);
  if (!decodeV8(b, p) || std::memcmp(b + kV8ProfileBytes, "QWIN", 4) || b[kV8ProfileBytes + 4] != 1) return false;
  constexpr int campaignAt = kV8ProfileBytes + 8;
  for (unsigned n = 0; n < CAMPAIGN_CHAPTERS; ++n) {
    const auto* at = b + campaignAt + n * 20;
    auto& win = p.campaignVictories[n];
    win = {uint32_t(get(at, 4)), uint32_t(get(at + 4, 4)), uint32_t(get(at + 8, 4)), uint32_t(get(at + 12, 4)),
           uint32_t(get(at + 16, 4))};
    const bool defeated = n < completedPaths(p) || (n == chapter(p) && p.campaign.phase != BattlePhase::Reading);
    if (win.day > 84005 || ((!defeated) && (win.day || win.seconds || win.xp || win.coins || win.books != UINT32_MAX)))
      return false;
  }
  constexpr int booksAt = campaignAt + CAMPAIGN_CHAPTERS * 20;
  for (int n = 0; n < kHuntOutfitCount; ++n) {
    p.victories[n].books = get(b + booksAt + n * 4, 4);
    if (p.victories[n].books != UINT32_MAX && !earnedHuntOutfit(p, huntOutfitAt(n))) return false;
  }
  constexpr int countersAt = booksAt + kHuntOutfitCount * 4;
  p.observedBooks = get(b + countersAt, 4);
  p.battleElapsed = get(b + countersAt + 4, 4);
  p.battleBooks = get(b + countersAt + 8, 4);
  p.requestBooks = get(b + countersAt + 12, 4);
  p.pausedInkBooks = get(b + countersAt + 16, 4);
  p.requestWinDay = get(b + countersAt + 20, 4);
  p.requestWinSeconds = get(b + countersAt + 24, 4);
  if (get(b + countersAt + 28, 4) > 1 || p.requestWinDay > 84005 ||
      ((p.requestBooks || p.requestWinDay || p.requestWinSeconds) && !validRequest(p.activeRequest)) ||
      (p.pausedInkBooks && (!p.inkHuntDiscovered || (!repeatProfile(b) && hasHuntClaim(p, kInkHunt)))))
    return false;
  p.battleStatsKnown = get(b + countersAt + 28, 4);
  return true;
}
bool hasNewerBalance(const uint8_t* b) {
  return !std::memcmp(b, "RQUA", 4) && !std::memcmp(b + 7104, "QBAL", 4) && b[7110] > BALANCE_REVISION;
}
bool decode(const uint8_t* b, Profile& p) {
  if (!decodeV9(b, p)) return false;
  if (b[3] == 'A') {
    if (std::memcmp(b + 7104, "QBAL", 4) || (b[7110] < 1 || b[7110] > BALANCE_REVISION)) return false;
    if (b[7110] >= 3) {
      if (b[7116] & ~questmilestone::reached(level(p.xp))) return false;
      p.celebratedMilestones = b[7116];
    }
    if (b[7110] >= 4) {
      for (unsigned n = 0; n < 2; ++n) {
        const auto item = get(b + 7118 + n * 2, 2);
        if (item != kGearCount && (!availableItem(item) || slot(item) != Slot::Body || !owns(p, item))) return false;
        p.outfitOffers[n] = uint16_t(item);
      }
      if (p.outfitOffers[0] != kGearCount && p.outfitOffers[0] == p.outfitOffers[1]) return false;
    }
    if (b[7110] >= 5) {
      for (unsigned n = 0; n < kHuntOutfitCount; ++n) {
        p.huntWins[n] = uint32_t(get(b + HUNT_WINS_OFFSET + n * 4, 4));
        if (p.huntWins[n] && !earnedHuntOutfit(p, huntOutfitAt(n))) return false;
      }
    }
    const bool original = b[7110] == 1;
    const auto noticed = get(b + 7108, 2);
    if (!noticed || noticed > (original ? LEGACY_BALANCE_MAX_LEVEL : MAX_LEVEL) ||
        (original ? originalThreshold(noticed) : threshold(noticed)) > p.xp)
      return false;
    p.noticedLevel = std::min<uint64_t>(noticed, MAX_LEVEL);
    if (original) {
      const auto oldCoins = accumulatedLevelCoins(originalLevel(p.xp));
      const auto newCoins = accumulatedLevelCoins(level(p.xp));
      if (oldCoins > newCoins) {
        const auto difference = oldCoins - newCoins;
        p.campaign.extraCoins =
            p.campaign.extraCoins > UINT64_MAX - difference ? UINT64_MAX : p.campaign.extraCoins + difference;
      }
    }
    if ((p.requestTargetMinutes && !validRequest(p.activeRequest)) ||
        (validRequest(p.activeRequest) && p.requestGoal == RequestGoal::Minutes && !p.requestTargetMinutes) ||
        p.requestTargetMinutes > 1800 || p.pausedInkTargetMinutes > 1800 ||
        (p.pausedInkTargetMinutes && (!p.inkHuntDiscovered || (!repeatProfile(b) && hasHuntClaim(p, kInkHunt)))))
      return false;
  }
  p.campaign.bankSeconds = 0;
  p.campaign.xpRemainder = 0;
  return true;
}
bool decodeV5(const uint8_t* b, Profile& p) { return decodeCurrent(b, p, '5'); }
bool decodeV4(const uint8_t* b, Profile& p) { return decodeCurrent(b, p, '4'); }
bool decodeV3(const uint8_t* b, Profile& p) { return expandedLegacy(b, p, true); }
bool decodeV2(const uint8_t* b, Profile& p) { return expandedLegacy(b, p, false); }
bool decodeLegacy(const uint8_t* b, Profile& p) {
  if (std::memcmp(b, "RQU1", 4) || get(b + 60, 4) != readingstats::checksum(b, 60) || !(b[32] & 1) || b[32] > 7 ||
      b[33] > 2 || !(b[32] & (1 << b[33])) || !validCommon(b))
    return false;
  commonFields(b, p);
  for (int n = 0; n < 3; ++n)
    if (b[32] & (1 << n)) set(p.owned, n * 2);
  p.equipped[int(Slot::Body)] = b[33] * 2;
  return true;
}
}  // namespace quest
