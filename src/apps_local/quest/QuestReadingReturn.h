#pragma once

#include "QuestCampaign.h"
#include "QuestJourney.h"
#include "QuestRequests.h"

namespace questreturn {
enum class Kind : uint8_t { None, Combat, Banked, Complete, Journey, Logged };
enum class Phase : uint8_t { Idle, Raise, Swing, Impact, Result };
enum class Panel : uint8_t { Reading, Victory, Request };
inline bool isPlaying(Phase phase) { return phase == Phase::Raise || phase == Phase::Swing || phase == Phase::Impact; }
struct Report {
  uint64_t seconds = 0, xp = 0;
  uint32_t before = 0, after = 0, target = 0;
  Kind kind = Kind::None;
  bool victory = false;
  int requestId = -1;
  uint32_t requestBefore = 0, requestAfter = 0, requestTarget = 0;
};
inline bool huntOnly(const Report& report) {
  return report.requestId >= 0 && !report.victory && report.kind != Kind::Combat && report.kind != Kind::Journey;
}
inline bool sagaCelebration(const Report& report, const quest::Profile& p) {
  return report.victory && p.campaign.phase == quest::BattlePhase::Story && quest::lastPathInSaga(quest::chapter(p));
}
inline Panel nextPanel(Panel current, const Report& report, const quest::Profile& p) {
  if (current == Panel::Reading && sagaCelebration(report, p)) return Panel::Victory;
  if (current != Panel::Request && report.requestId >= 0) return Panel::Request;
  return current;
}
inline bool captureRequest(const quest::Profile& before, quest::Profile& after, const readingstats::Summary& summary,
                           Report& report) {
  const int id = before.activeRequest;
  if (!quest::validRequest(id) || after.activeRequest != id) return false;
  const auto target = quest::requestTarget(after, id);
  if (after.requestReported == target) return false;
  bool changed = false;
  if (after.requestGoal != quest::RequestGoal::Minutes) {
    const auto metric = quest::requestMetric(after, after.requestGoal, &summary);
    if (metric < after.requestBaseline) {
      after.requestBaseline = metric;
      changed = true;
    }
  }
  const auto value = quest::requestValue(after, id, &summary);
  if (value > before.requestReported) {
    report.requestId = id;
    report.requestBefore = before.requestReported;
    report.requestAfter = value;
    report.requestTarget = target;
  }
  // Save the completed hunt with the reading transaction, even before its report closes.
  if (value == target || value < before.requestReported) {
    after.requestReported = value;
    changed = true;
  }
  return changed;
}
inline bool acknowledgeRequest(quest::Profile& profile, const Report& report) {
  if (!quest::validRequest(report.requestId) || profile.activeRequest != report.requestId ||
      report.requestAfter <= profile.requestReported ||
      report.requestAfter > quest::requestTarget(profile, report.requestId))
    return false;
  profile.requestReported = report.requestAfter;
  return true;
}
enum class HuntAcknowledgement : uint8_t { Unchanged, Progress, Reward, Rejected };
inline HuntAcknowledgement finishHuntReport(quest::Profile& p, const Report& report,
                                            const readingstats::Summary& summary, int day) {
  if (!quest::validRequest(report.requestId) || p.activeRequest != report.requestId ||
      report.requestTarget != quest::requestTarget(p, report.requestId))
    return HuntAcknowledgement::Rejected;
  if (report.requestAfter >= report.requestTarget)
    return quest::collectRequest(p, report.requestId, &summary, day) ? HuntAcknowledgement::Reward
                                                                     : HuntAcknowledgement::Rejected;
  return acknowledgeRequest(p, report) ? HuntAcknowledgement::Progress : HuntAcknowledgement::Unchanged;
}
inline bool pendingEnding(const quest::Profile& profile) {
  return !profile.endingSeen && profile.campaign.state != quest::AdventureState::Setup &&
         (quest::hasWon(profile) || profile.campaign.state == quest::AdventureState::Unfinished);
}
inline Report capture(const quest::Profile& before, const quest::Profile& after, quest::ReadingCredit = {}) {
  Report result;
  if (!before.campaign.initialized || before.campaign.state == quest::AdventureState::Setup ||
      after.campaign.observedSeconds <= before.campaign.observedSeconds)
    return result;
  result.seconds = after.campaign.observedSeconds - before.campaign.observedSeconds;
  result.xp = after.xp >= before.xp ? after.xp - before.xp : 0;
  result.before = before.campaign.encounterSeconds;
  result.after = after.campaign.encounterSeconds;
  result.target = quest::encounterTarget(after);
  result.victory =
      after.campaign.phase == quest::BattlePhase::Victory ||
      (before.campaign.phase == quest::BattlePhase::Reading && after.campaign.phase == quest::BattlePhase::Story);
  result.kind =
      before.campaign.phase == quest::BattlePhase::Complete || before.campaign.state == quest::AdventureState::Peace ||
              before.campaign.state == quest::AdventureState::Unfinished
          ? Kind::Complete
      : result.victory || result.after > result.before
          ? (questjourney::kind(before.campaign.encounter) == questjourney::Kind::Combat ? Kind::Combat : Kind::Journey)
          : Kind::Logged;
  return result;
}
class Clock {
 public:
  void start(bool combat) {
    phase_ = combat ? Phase::Raise : Phase::Result;
    awaitingPaint = combat;
  }
  void cancel() {
    phase_ = Phase::Idle;
    awaitingPaint = false;
  }
  Phase phase() const { return phase_; }
  bool playing() const { return isPlaying(phase_); }
  void painted(uint32_t now, Phase phase) {
    if (!awaitingPaint || phase != phase_) return;
    paintedAt = now;
    awaitingPaint = false;
  }
  bool advance(uint32_t now) {
    if (!playing() || awaitingPaint || uint32_t(now - paintedAt) < 650) return false;
    switch (phase_) {
      case Phase::Raise:
        phase_ = Phase::Swing;
        break;
      case Phase::Swing:
        phase_ = Phase::Impact;
        break;
      case Phase::Impact:
        phase_ = Phase::Result;
        break;
      case Phase::Idle:
      case Phase::Result:
        return false;
    }
    awaitingPaint = playing();
    return true;
  }

 private:
  uint32_t paintedAt = 0;
  Phase phase_ = Phase::Idle;
  bool awaitingPaint = false;
};
}  // namespace questreturn
