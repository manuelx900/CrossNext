#include <algorithm>
#include <cassert>
#include <cstdio>

#include "../../src/apps_local/quest/QuestCampaign.h"
#include "../../src/apps_local/quest/QuestCore.h"
#include "../../src/apps_local/quest/QuestRequests.h"
void gear(quest::Profile& p, int mode) {
  if (mode == 0) return;
  for (int slot = 0; slot < quest::kCategoryCount; ++slot) {
    auto category = quest::Slot(slot);
    int best = slot < 3 ? p.equipped[slot] : quest::activeDecoration(p);
    unsigned score = 0;
    for (int i = 0; i < quest::count(category); ++i) {
      int item = quest::catalogItem(category, i);
      if (!quest::availableItem(item)) continue;
      if (!quest::owns(p, item) &&
          (mode == 1 || !quest::shopUnlocked(p, item) || quest::price(item) > quest::coins(p) / 2))
        continue;
      auto b = quest::gearBonus(item, quest::book(p));
      unsigned value = b.power + b.affinity + (mode == 3 ? b.experience * 3 : 0);
      if (value > score) {
        score = value;
        best = item;
      }
    }
    if (quest::availableItem(best)) quest::equip(p, best);
  }
}
int main() {
  for (bool hunts : {false, true})
    for (int mode = 0; mode < 4; ++mode) {
      if (hunts && mode == 0) continue;
      for (int daily : {15, 30, 60}) {
        quest::Profile p;
        readingstats::Summary s;
        int day = 20500;
        quest::syncCampaign(p, s, day);
        quest::acknowledgePrologue(p);
        unsigned battles = 0, victories = 0;
        double campaignHours = 0;
        const auto readDay = [&] {
          std::move_backward(s.dailySeconds, s.dailySeconds + readingstats::kHistoryDays - 1,
                             s.dailySeconds + readingstats::kHistoryDays);
          s.dailySeconds[0] = 0;
          s.anchorDay = day;
          readingstats::Record rec;
          rec.localDay = day;
          rec.secondOfDay = 40000;
          rec.seconds = daily * 60;
          s.add(rec);
          quest::syncCampaign(p, s, day);
          if (mode) {
            quest::claimMission(p, s, day, quest::Mission::Daily);
            quest::claimMission(p, s, day, quest::Mission::Weekly);
          }
          ++day;
        };
        while (day < 24500) {
          if (quest::needsIntroduction(p)) quest::beginCampaign(p);
          if (quest::needsEncounterStory(p)) quest::beginEncounter(p);
          gear(p, mode);
          if (hunts && !quest::validRequest(p.activeRequest)) {
            for (int n = 0; n < quest::requestCount(p, false); ++n) {
              const int id = quest::requestAt(p, false, n);
              if (!quest::requestClaimed(p, id)) {
                assert(quest::acceptRequest(p, id));
                break;
              }
            }
          }
          if (quest::completedPaths(p) == quest::CAMPAIGN_CHAPTERS &&
              (!hunts ||
               (!quest::validRequest(p.activeRequest) && quest::requestCount(p, true) == quest::kHuntOutfitCount)))
            break;
          readDay();
          if (hunts && quest::validRequest(p.activeRequest) &&
              quest::requestValue(p, p.activeRequest, &s) >= quest::requestTarget(p, p.activeRequest)) {
            assert(quest::collectRequest(p, p.activeRequest, &s, day - 1));
            ++victories;
          }
          if (p.campaign.phase == quest::BattlePhase::Victory) {
            ++battles;
            assert(quest::collectBattleResult(p));
            while (quest::visibleItem(p.campaign.pendingReward)) quest::dismissStoryReward(p, false);
            assert(quest::continueCampaign(p));
          }
          if (!campaignHours && quest::completedPaths(p) == quest::CAMPAIGN_CHAPTERS) {
            campaignHours = s.seconds / 3600.;
            std::printf("CAMPAIGN hunts%d mode%d daily%d hours=%.2f level=%u\n", hunts, mode, daily, campaignHours,
                        quest::level(p.xp));
          }
        }
        assert(battles == quest::CAMPAIGN_CHAPTERS);
        assert(campaignHours > 0 && campaignHours < 420);
        std::printf("ALL hunts%d mode%d daily%d hours=%.2f level=%u xp=%llu victories=%u\n", hunts, mode, daily,
                    s.seconds / 3600., quest::level(p.xp), (unsigned long long)p.xp, victories);
        if (hunts) {
          assert(victories == quest::kHuntOutfitCount);
          while (quest::level(p.xp) < quest::MAX_LEVEL && day < 32500) readDay();
          assert(quest::level(p.xp) == quest::MAX_LEVEL);
          std::printf("MAX hunts%d mode%d daily%d hours=%.2f level=%u\n", hunts, mode, daily, s.seconds / 3600.,
                      quest::level(p.xp));
          readDay();
          assert(quest::level(p.xp) == quest::MAX_LEVEL);
        }
      }
    }
}
