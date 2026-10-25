#include <cstdio>
#include <initializer_list>

#include "apps_local/power/PowerButtonPolicy.h"
#include "apps_local/power/PowerWakePolicy.h"

namespace {
int checks = 0;
int failures = 0;
void check(const bool ok, const char* description) {
  ++checks;
  if (!ok) {
    ++failures;
    std::printf("FAIL: %s\n", description);
  }
}
}  // namespace

int main() {
  using namespace powermenu;
  check(acceptsWake(WakeMode::ShortPress, false), "released short wake is accepted");
  check(acceptsWake(WakeMode::ShortPress, true), "holding Power also wakes in short mode");
  check(!acceptsWake(WakeMode::LongPress, false), "released short wake is rejected in hold mode");
  check(acceptsWake(WakeMode::LongPress, true), "verified hold wakes in hold mode");
  check(acceptsWake(static_cast<WakeMode>(255), false), "invalid mode cannot lock out a short wake");

  for (const auto mode : {WakeMode::ShortPress, WakeMode::LongPress}) {
    if (!acceptsWake(mode, true)) continue;
    ButtonPolicy policy;
    check(policy.update(true, false, 5000, false) == ButtonAction::None, "wake hold during boot is ignored");
    check(policy.update(true, false, 6000, true) == ButtonAction::None, "inherited wake hold cannot open menu");
    check(policy.update(false, true, 6000, true) == ButtonAction::None, "wake release cannot suspend");
    check(policy.update(true, false, 20, true) == ButtonAction::None, "new short press starts a gesture");
    check(policy.update(false, true, 80, true) == ButtonAction::Sleep, "next short press still suspends");
    check(policy.update(true, false, 700, true) == ButtonAction::Menu, "new hold still opens power menu");
    check(policy.update(false, true, 800, true) == ButtonAction::None, "menu hold release cannot suspend");
  }

  std::printf("power-wake: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
