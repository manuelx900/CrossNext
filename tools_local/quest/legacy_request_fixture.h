#pragma once

#include "../../src/apps_local/quest/QuestRequestGoals.h"
#include "../../src/apps_local/quest/QuestRequests.h"

namespace questfixture {
inline quest::RequestGoal goal(int id) { return quest::REQUEST_GOALS[id]; }

inline bool activateLegacyRequest(quest::Profile& profile, int id, const readingstats::Summary& summary) {
  if (id < 0 || id >= quest::kLegacyRequestCount || profile.activeRequest != quest::kRequestCount ||
      quest::completedPaths(profile) <= quest::requestChapter(id) || quest::requestClaimed(profile, id))
    return false;
  profile.activeRequest = id;
  profile.requestTargetMinutes = quest::legacyRequestMinutes(id);
  profile.requestStarted = profile.campaign.observedSeconds;
  profile.requestReported = 0;
  profile.requestBaseline = 0;
  profile.requestGoal = goal(id);
  if (profile.requestGoal != quest::RequestGoal::Minutes)
    profile.requestBaseline = quest::requestMetric(profile, profile.requestGoal, &summary);
  return true;
}
}  // namespace questfixture
