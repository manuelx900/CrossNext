#pragma once
#include <cstdint>

#include "../reading/ReadingStatsCore.h"
#include "QuestBalance.h"
#include "QuestName.h"
namespace quest {
constexpr int kProfileBytes = 8192, kV8ProfileBytes = 4096, kV6ProfileBytes = 512, kV5ProfileBytes = 320,
              kV2ProfileBytes = 256, kBaseGearCount = 200, kLegacyDecorationCount = 16, kExpandedItemStart = 216,
              kDecorationCount = 60, kKingBody = 317, kKingBackground = 318, kKingPet = 319, kKingDecoration = 316,
              kGearCount = 320, kQuestCount = 1500;
constexpr int kLegacyRequestCount = 300, kRequestsPerRegion = 25;
constexpr int kShadowHunt = kLegacyRequestCount, kKingHunt = kShadowHunt + 1, kCampaignHuntCount = kKingHunt + 1;
constexpr int kStormHunt = kCampaignHuntCount, kCrystalHunt = kStormHunt + 1, kPhoenixHunt = kCrystalHunt + 1,
              kInkHunt = kPhoenixHunt + 1, kBestiaryHuntStart = kInkHunt + 1, kBestiaryCount = 56,
              kRequestCount = kBestiaryHuntStart + kBestiaryCount;
constexpr int kCampaignHuntFamilies = 74, kSpecialHuntCount = kRequestCount - kCampaignHuntCount;
constexpr int kHuntOutfitStart = kGearCount + 1, kHuntOutfitCount = kCampaignHuntFamilies + kSpecialHuntCount,
              kInkRelic = 399, kBestiaryOutfitStart = kInkRelic + 1;
constexpr int kCollectionItemCount = kBestiaryOutfitStart + kBestiaryCount;
constexpr int kCollectionBits = (kCollectionItemCount + 7) / 8;
constexpr int kSlotCount = 3, kOutfitCount = 100, kBackgroundCount = 50, kPetCount = 50;
constexpr int kCategoryCount = 4, kDecorationPlaces = 2;
constexpr uint32_t MAX_LEVEL = questbalance::MAX_LEVEL, LEVELS_PER_TITLE = 5, TITLE_COUNT = 30;
constexpr uint32_t DAILY_MINUTES = 15;
constexpr uint8_t BALANCE_REVISION = 5;
bool hasNewerBalance(const uint8_t* bytes);
// Slot values and equipment IDs are shared with the existing RQU save formats.
enum class Slot : uint8_t { Body, Background, Pet, Decoration };
enum class Gender : uint8_t { Masculine, Feminine };
enum class RequestGoal : uint8_t { Minutes, Pages, Sessions, Books, Days };
enum class HuntRank : uint8_t { Basic, Elite, Legendary, Secret };
enum class BattlePhase : uint8_t { Reading, Victory, Story, Complete };
enum class AdventureState : uint8_t { Active, Unfinished, Peace, Setup };
inline constexpr unsigned CAMPAIGN_BOOKS = 12, CHAPTERS_PER_BOOK = 10, ENCOUNTERS_PER_CHAPTER = 6;
inline constexpr unsigned CAMPAIGN_CHAPTERS = CAMPAIGN_BOOKS * CHAPTERS_PER_BOOK;
inline constexpr unsigned CAMPAIGN_ENCOUNTERS = CAMPAIGN_CHAPTERS * ENCOUNTERS_PER_CHAPTER;
struct Campaign {
  uint64_t observedSeconds = 0, originSeconds = 0, bankSeconds = 0, extraCoins = 0;
  int startedDay = 0, claimedWeek = 0;
  uint32_t startDaySeconds = 0, originCompleted = 0, encounterSeconds = 0, victoryCoins = 0;
  uint16_t encounter = 0, xpRemainder = 0;
  bool initialized = false;
  bool firstDayRead = false;
  uint8_t introducedBooks = 0;
  bool prologueSeen = false;
  uint16_t narratedEncounters = 0;
  uint16_t pendingReward = kGearCount;
  BattlePhase phase = BattlePhase::Reading;
  AdventureState state = AdventureState::Active;
  uint8_t storyClaims[(CAMPAIGN_CHAPTERS + 7) / 8]{};
};
struct HuntVictory {
  uint32_t day = 0;
  uint32_t seconds = 0;
  uint32_t xp = 0, coins = 0;
  uint32_t books = UINT32_MAX;
};
struct Profile {
  uint32_t sequence = 0;
  uint64_t xp = 0, spent = 0;
  uint32_t bonus = 0;
  int claimedDay = 0;
  Gender gender = Gender::Masculine;
  uint16_t noticedLevel = 1;
  char name[NAME_BYTES]{};
  uint16_t equipped[kSlotCount] = {0, kOutfitCount, kOutfitCount + kBackgroundCount + 6};
  uint8_t owned[(kBaseGearCount + 7) / 8] = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 16};
  uint16_t decorationOwned = 0;
  uint16_t decorations[kDecorationPlaces] = {kGearCount, kGearCount};
  uint8_t claimed[(kQuestCount + 7) / 8]{};
  Campaign campaign;
  uint64_t requestStarted = 0;
  uint64_t requestBaseline = 0;
  uint16_t activeRequest = kRequestCount;
  RequestGoal requestGoal = RequestGoal::Minutes;
  uint32_t requestReported = 0;
  uint8_t requestClaims[(kRequestCount + 7) / 8]{};
  uint32_t pausedInkSeconds = 0;
  bool inkHuntDiscovered = false;
  uint8_t expansionOwned[(kGearCount - kExpandedItemStart + 7) / 8]{};
  bool endingSeen = false;
  // Bookmark within the prologue or a specific Saga milestone.
  uint16_t sagaPositions[CAMPAIGN_BOOKS]{};
  uint16_t acquiredCount = 0;
  uint16_t acquired[kCollectionItemCount]{};
  uint8_t favorites[kCollectionBits]{}, unseen[kCollectionBits]{};
  HuntVictory victories[kHuntOutfitCount]{};
  uint32_t huntWins[kHuntOutfitCount]{};
  uint32_t savedDay = 0;
  uint32_t requestCarried = 0;
  uint16_t requestTargetMinutes = 0, pausedInkTargetMinutes = 0;
  uint32_t requestElapsed = 0, pausedInkElapsed = 0;
  HuntVictory campaignVictories[CAMPAIGN_CHAPTERS]{};
  uint32_t observedBooks = UINT32_MAX;
  uint32_t battleElapsed = 0, battleBooks = 0;
  uint32_t requestBooks = 0, pausedInkBooks = 0, requestWinDay = 0, requestWinSeconds = 0;
  bool battleStatsKnown = false;
  uint8_t celebratedMilestones = 0;
  uint16_t outfitOffers[2] = {kGearCount, kGearCount};
};
// Activity-owned scratch keeps persistence and transaction copies off the task stack.
enum class StoreNotice : uint8_t { None, Recovered };
struct StoreWork {
  StoreNotice notice = StoreNotice::None;
  bool retiredCopiesChecked = false;
  uint8_t bytes[kProfileBytes]{};
  Profile candidate;
};
uint32_t level(uint64_t xp);
uint64_t threshold(uint32_t level);
uint64_t coins(const Profile&);
uint32_t levelRewardCoins(uint32_t level);
uint32_t pendingLevelCoins(const Profile&);
bool hasLevelUp(const Profile&);
bool acknowledgeLevel(Profile&);
void refresh(Profile&, const readingstats::Summary&);
bool claim(Profile&, int today, uint32_t seconds);
bool claimMilestone(Profile&, int quest, const readingstats::Summary&);
uint64_t questValue(int quest, const readingstats::Summary&);
uint32_t questTarget(int quest);
uint32_t questReward(int quest);
uint32_t questCoinReward(int quest);
bool isClaimed(const Profile&, int quest);
// Ready rewards first, then unfinished quests, then previously claimed quests.
int orderedQuest(const Profile&, const readingstats::Summary&, int index);
Slot slot(int item);
int first(Slot);
int count(Slot);
int catalogItem(Slot, int index);
bool visibleItem(int item);
bool availableItem(int item);
int activeDecoration(const Profile&);
bool normalizeDecorations(Profile&);
int ownedCount(const Profile&, Slot);
int ownedItem(const Profile&, Slot, int index);
enum class ShopView : uint8_t { Available, Catalogue };
int shopCount(const Profile&, Slot, ShopView = ShopView::Available);
int shopPage(const Profile&, Slot, int page, int pageSize, uint16_t (&items)[4], ShopView = ShopView::Available);
bool shopUnlocked(const Profile&, int item);
int pathUnlockChapter(int item);
int shopItem(const Profile&, Slot, int index, ShopView = ShopView::Available);
bool unequip(Profile&, Slot);
uint32_t shopLevel(int item);
uint32_t requiredLevel(int item);
uint32_t price(int item);
bool owns(const Profile&, int item);
bool equipped(const Profile&, int item);
bool grantItem(Profile&, int item);
bool storyExclusive(int item);
int decorationPlace(int item);
int chapterDecoration(unsigned chapter);
int sagaReward(unsigned book, Slot slot);
struct CampaignRewards {
  uint16_t items[6]{};
  uint8_t count = 0;
};
CampaignRewards campaignRewards(unsigned chapter);
CampaignRewards campaignUnlocks(unsigned chapter);
int nextOutfitOffer(const Profile&);
bool offerOutfit(Profile&, int item);
bool finishOutfitOffer(Profile&, int item, bool equipNow);
int rewardSaga(int item);
int requestDecoration(int request);
bool hasWon(const Profile&);
bool restoreExpansionRewards(Profile&);
int chapterReward(unsigned chapter);
int rewardEncounter(int item);
int nextStoryReward(const Profile&);
void restoreStoryRewards(Profile&);
bool dismissStoryReward(Profile&, bool place);
bool removeItem(Profile&, int item);
bool equip(Profile&, int item);
void encode(const Profile&, uint8_t*);
bool decodeV9(const uint8_t*, Profile&);
bool decode(const uint8_t*, Profile&);
bool decodeV8(const uint8_t*, Profile&);
bool decodeV6(const uint8_t*, Profile&);
bool decodeV5(const uint8_t*, Profile&);
bool decodeV4(const uint8_t*, Profile&);
bool decodeV3(const uint8_t*, Profile&);
bool decodeV2(const uint8_t*, Profile&);
bool decodeLegacy(const uint8_t*, Profile&);
bool load(Profile&, bool& writable, StoreWork&, bool* foundProfile = nullptr);
bool save(Profile&, bool writable, StoreWork&);
void reconcileCollection(Profile&);
bool noteAcquired(Profile&, int item, bool markNew = true);
bool isNewItem(const Profile&, int item);
bool markItemSeen(Profile&, int item);
bool isFavorite(const Profile&, int item);
bool toggleFavorite(Profile&, int item);
int inventoryCount(const Profile&, Slot, bool favoritesOnly);
int inventoryItem(const Profile&, Slot, bool favoritesOnly, int index);
}  // namespace quest
