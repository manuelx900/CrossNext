#include <climits>
#include <cstdio>
#include <initializer_list>

#include "../../src/apps_local/quest/QuestSimulatorReading.h"
#include "../../src/apps_local/reading/ReadingStatsCore.h"

int main() {
  int checks = 0, failures = 0;
  const auto check = [&](bool value) {
    ++checks;
    if (!value) ++failures;
  };
  using Result = questsim::ReadingResult;
  questsim::ReadingMailbox mailbox;
  int minutes = -99;
  check(readingstats::validDocument(questsim::READING_DOCUMENT));
  check(questsim::unobservedPart(600, 720, 600) == 120);
  check(questsim::unobservedPart(600, 720, 660) == 60);
  check(questsim::unobservedPart(600, 720, 720) == 0);
  check(questsim::unobservedPart(600, 720, 900) == 0);
  check(int(Result::Queued) == 2 && int(Result::Scanning) == 3 && int(Result::Done) == 4);
  check(mailbox.request(20) == Result::Unavailable);
  check(!mailbox.begin(minutes) && minutes == -99);
  mailbox.activate();
  for (int invalid : {INT_MIN, -1, 1441, INT_MAX}) {
    check(mailbox.request(invalid) == Result::InvalidMinutes);
    check(mailbox.result() == Result::Ready);
  }
  for (int value = 0; value <= questsim::ReadingMailbox::MAX_MINUTES; ++value) {
    check(mailbox.request(value) == Result::Queued);
    check(mailbox.request(5) == Result::Busy);
    check(mailbox.begin(minutes) && minutes == value);
    check(mailbox.result() == Result::Scanning);
    check(!mailbox.begin(minutes));
    check(mailbox.request(10) == Result::Busy);
    mailbox.finish(Result::Done);
    check(mailbox.result() == Result::Done);
  }
  check(mailbox.request(30) == Result::Queued);
  mailbox.deactivate();
  check(!mailbox.begin(minutes));
  mailbox.finish(Result::Done);
  check(mailbox.result() == Result::Unavailable);
  mailbox.activate();
  check(mailbox.request(1) == Result::Queued && mailbox.begin(minutes));
  mailbox.deactivate();
  mailbox.finish(Result::StorageError);
  check(mailbox.result() == Result::Unavailable);
  mailbox.activate();
  for (auto error : {Result::NeedsHero, Result::FinishScreen, Result::StorageError, Result::NotReady}) {
    check(mailbox.request(0) == Result::Queued && mailbox.begin(minutes) && minutes == 0);
    mailbox.finish(error);
    check(mailbox.result() == error);
  }
  std::printf(
      "Simulator reading mailbox: %d checks, %d failures;0..1440, duplicate requests, owner exit and retries; %zu "
      "bytes\n",
      checks, failures, sizeof(mailbox));
  return failures ? 1 : 0;
}
