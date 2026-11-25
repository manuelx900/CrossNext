#pragma once

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace bookorbit {
struct Position {
  std::string anchor;
  float fraction = 0;
};

struct LandingPage {
  int spine = -1;
  int page = -1;
  std::optional<uint32_t> offset;
};

struct Baseline {
  Position local;
  Position remote;
  LandingPage landing;
  std::string serverUrl = {};
  std::string instanceId = {};
};

enum class IdentityStatus { Unavailable, Legacy, Verified };

struct ServerIdentity {
  IdentityStatus status = IdentityStatus::Unavailable;
  std::string instanceId;
};

enum class Decision { Skip, Equal, Download, Upload };

constexpr float POSITION_TOLERANCE = 0.00001f;

inline bool samePosition(const Position& first, const Position& second) {
  if (!first.anchor.empty() && !second.anchor.empty() && first.anchor == second.anchor) return true;
  return std::abs(first.fraction - second.fraction) <= POSITION_TOLERANCE &&
         (first.anchor.empty() || second.anchor.empty());
}

inline Decision decide(const Baseline& baseline, const Position& local, const Position& remote) {
  if (samePosition(local, remote)) return Decision::Equal;
  const bool localChanged = !samePosition(baseline.local, local);
  const bool remoteChanged = !samePosition(baseline.remote, remote);
  if (localChanged == remoteChanged) return Decision::Skip;
  return remoteChanged ? Decision::Download : Decision::Upload;
}

inline bool matchesBaseline(const Baseline& baseline, const ServerIdentity& identity, std::string_view url) {
  if (!baseline.instanceId.empty())
    return identity.status == IdentityStatus::Verified && baseline.instanceId == identity.instanceId;
  return identity.status == IdentityStatus::Legacy && !baseline.serverUrl.empty() && baseline.serverUrl == url;
}

inline Decision decideOnSleep(const Baseline* baseline, const Position& local, const Position& remote,
                              bool latestWriteFromReader) {
  if (samePosition(local, remote)) return Decision::Equal;
  const auto decision = baseline ? decide(*baseline, local, remote) : Decision::Skip;
  // A successful PUT can outlive its confirmation. The server's device attribution
  // lets the next sleep resume from the reader's own write.
  if (decision == Decision::Skip && latestWriteFromReader && (!baseline || !samePosition(baseline->remote, remote)))
    return Decision::Upload;
  return decision;
}

inline Decision decideOnWake(const Baseline* baseline, const Position& local, const Position& remote) {
  const auto decision = decideOnSleep(baseline, local, remote, false);
  return decision == Decision::Upload ? Decision::Skip : decision;
}

inline bool canUploadFirstPosition(bool hasBaseline, const Position& local) {
  return !hasBaseline && local.fraction > POSITION_TOLERANCE;
}

}  // namespace bookorbit
