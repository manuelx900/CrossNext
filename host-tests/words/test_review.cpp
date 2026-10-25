#include <array>
#include <cstdio>
#include <cstring>

#include "../../src/apps_local/words/WordsReview.h"
using namespace vocabulary;
int failures = 0, checks = 0;
#define CHECK(x)                                  \
  do {                                            \
    ++checks;                                     \
    if (!(x)) {                                   \
      ++failures;                                 \
      std::printf("FAIL %d: %s\n", __LINE__, #x); \
    }                                             \
  } while (0)
int main() {
  Word w;
  Context c;
  study::Fsrs fsrs;
  study::Scheduler scheduler(fsrs, study::Steps::defaults());
  copy(w.id, sizeof(w.id), "00000000000000000000000000000001");
  copy(c.id, sizeof(c.id), "00000000000000000000000000000002");
  ReviewSession session;
  session.clear();
  session.add(w, c);
  CHECK(session.count() == 1);
  copy(c.id, sizeof(c.id), "00000000000000000000000000000003");
  session.add(w, c);
  CHECK(session.count() == 1);
  CHECK(session.next(100, 0) == 0);
  CHECK(!difficult(w));
  rate(w, study::Rating::Again, scheduler, 100, 0);
  CHECK(w.failures == 1 && w.lastRating == 1 && difficult(w));
  CHECK(w.card.reps == 1 && w.card.lapses == 0);
  session.answered(0, w.card);
  CHECK(session.next(100, 0) == -1 && session.waitingMinutes(100, 0) == 1);
  CHECK(session.next(100, 1) == 0);
  rate(w, study::Rating::Hard, scheduler, 100, 1);
  CHECK(w.failures == 1 && w.lastRating == 2 && difficult(w));
  CHECK(w.card.lapses == 0);
  session.answered(0, w.card);
  CHECK(session.next(100, 1) == -1);
  CHECK(session.waitingMinutes(100, 1) == 6);
  CHECK(session.next(100, 7) == 0);
  rate(w, study::Rating::Good, scheduler, 100, 7);
  CHECK(w.card.stepIndex == 1 && w.card.reps == 3);
  session.answered(0, w.card);
  CHECK(session.waitingMinutes(100, 7) == 10);
  rate(w, study::Rating::Easy, scheduler, 100, 17);
  session.answered(0, w.card);
  CHECK(session.next(100, 17) == -1 && session.waitingMinutes(100, 17) == -1);
  CHECK(w.card.state == uint8_t(study::State::Review) && w.card.dueDay > 100 && w.card.reps == 4);
  const auto before = w.card;
  const auto expected = scheduler.answer(before, study::Rating::Again, 150, 1439);
  rate(w, study::Rating::Again, scheduler, 150, 1439);
  CHECK(w.card.dueDay == expected.card.dueDay && w.card.dueMinute == expected.card.dueMinute);
  CHECK(w.card.stability == expected.card.stability && w.card.difficulty == expected.card.difficulty);
  CHECK(w.card.lapses == 1 && w.failures == 2);
  session.clear(ReviewSession::Selection::Difficult);
  for (int n = 99; n >= 0; --n) {
    std::snprintf(w.id, sizeof(w.id), "%032x", n);
    std::snprintf(c.id, sizeof(c.id), "%032x", n + 1000);
    w.card = {};
    w.card.difficulty = float(n % 10);
    w.failures = 0;
    session.add(w, c);
  }
  CHECK(session.count() == ReviewSession::kCapacity);
  for (int n = 1; n < session.count(); ++n) CHECK(session.entry(n - 1).difficulty >= session.entry(n).difficulty);
  session.clear();
  for (int n = 60; n >= 0; --n) {
    std::snprintf(w.id, sizeof(w.id), "%032x", n);
    std::snprintf(c.id, sizeof(c.id), "%032x", n + 100);
    w.card.dueDay = n;
    session.add(w, c);
  }
  CHECK(session.count() == ReviewSession::kCapacity && session.entry(0).day == 0 && session.entry(47).day == 47);
  for (bool hard : {false, true}) {
    for (auto state : {study::State::Learning, study::State::Relearning}) {
      for (bool learningFirst : {false, true}) {
        session.clear(hard ? ReviewSession::Selection::Difficult : ReviewSession::Selection::Due);
        for (int n = 0; n <= 60; ++n) {
          const bool learning = n == (learningFirst ? 0 : 60);
          std::snprintf(w.id, sizeof(w.id), "%032x", learning ? 0xffff : n);
          std::snprintf(c.id, sizeof(c.id), "%032x", n + 1000);
          w.card = {};
          w.failures = learning ? 0 : 10;
          w.card.difficulty = learning ? 1.0f : 10.0f;
          if (learning) {
            w.card.state = uint8_t(state);
            w.card.dueDay = 100;
            w.card.dueMinute = 1;
          }
          session.add(w, c);
        }
        CHECK(session.count() == ReviewSession::kCapacity);
        CHECK(!std::strcmp(session.entry(0).word, "0000000000000000000000000000ffff"));
        CHECK(session.next(100, 1) == 0);
        CHECK(session.next(100, 0) > 0);
      }
    }
  }
  // Stream all saved identities through several bounded batches, regardless
  // of directory order, known flags or future scheduling state.
  session.clear(ReviewSession::Selection::AllSaved);
  int studied = 0;
  for (int batch = 0; batch < 4; ++batch) {
    for (int n = 122; n >= 0; --n) {
      std::snprintf(w.id, sizeof(w.id), "%032x", n);
      std::snprintf(c.id, sizeof(c.id), "%032x", n + 1000);
      w.known = n % 2;
      w.card = {};
      w.card.state = uint8_t(study::State::Learning);
      w.card.dueDay = 9999;
      session.add(w, c);
      session.add(w, c);
    }
    CHECK(session.count() == (batch == 0 || batch == 1 ? 48 : batch == 2 ? 27 : 0));
    while (session.next(100, 0) >= 0) {
      const int at = session.next(100, 0);
      char expected[33];
      std::snprintf(expected, sizeof(expected), "%032x", studied++);
      CHECK(!std::strcmp(session.entry(at).word, expected));
      auto card = study::CardState{};
      card.state = uint8_t(study::State::Review);
      session.answered(at, card);
    }
    CHECK(session.waitingMinutes(100, 0) == -1);
    CHECK(session.nextBatch() == (batch < 3));
  }
  CHECK(studied == 123);
  session.clear(ReviewSession::Selection::AllSaved);
  session.add(w, c);
  auto learning = study::CardState{};
  learning.state = uint8_t(study::State::Learning);
  learning.dueDay = 100;
  learning.dueMinute = 1;
  session.answered(0, learning);
  CHECK(session.next(100, 0) == -1 && session.waitingMinutes(100, 0) == 1);
  CHECK(session.next(100, 1) == 0);
  w.card = {};
  w.known = true;
  w.lastRating = 4;
  w.failures = UINT16_MAX;
  rate(w, study::Rating::Again, scheduler, 100, 0);
  CHECK(w.failures == UINT16_MAX && !w.known);
  std::printf("Words review: %d checks, %d failures; session %zu bytes\n", checks, failures, sizeof(session));
  return failures ? 1 : 0;
}
