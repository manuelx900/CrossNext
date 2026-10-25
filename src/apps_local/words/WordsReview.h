#pragma once
#include <algorithm>
#include <cstring>

#include "Vocabulary.h"

namespace vocabulary {
inline bool difficult(const Word& word) {
  return word.lastRating == uint8_t(study::Rating::Again) || word.lastRating == uint8_t(study::Rating::Hard) ||
         word.card.difficulty >= 6.0f || word.card.lapses >= 2 || (word.failures && word.card.stability < 7.0f);
}
inline void rate(Word& word, study::Rating rating, const study::Scheduler& scheduler, int today, int minute) {
  word.card = scheduler.answer(word.card, rating, today, minute).card;
  word.lastRating = uint8_t(rating);
  if (rating == study::Rating::Again && word.failures < UINT16_MAX) ++word.failures;
  word.known = false;
}
// Activity-owned, bounded working set. All entry points update the same Word
// record through StudyScheduler; this queue only selects and times a session.
class ReviewSession {
 public:
  static constexpr int kCapacity = 48;
  enum class Selection : uint8_t { Due, Difficult, AllSaved };
  struct Entry {
    char context[kIdBytes]{}, word[kIdBytes]{};
    int32_t day = 0;
    uint16_t minute = 0;
    float difficulty = 0;
    bool learning = false, finished = false;
  };
  void clear(Selection selection = Selection::Due) {
    count_ = 0;
    selection_ = selection;
    after_[0] = 0;
  }
  bool nextBatch() {
    if (selection_ != Selection::AllSaved || !count_) return false;
    copy(after_, sizeof(after_), entries_[count_ - 1].word);
    count_ = 0;
    return true;
  }
  int count() const { return count_; }
  const Entry& entry(int index) const { return entries_[index]; }
  void add(const Word& word, const Context& context) {
    if (selection_ == Selection::AllSaved && std::strcmp(word.id, after_) <= 0) return;
    for (int n = 0; n < count_; ++n)
      if (!std::strcmp(entries_[n].word, word.id)) return;
    Entry candidate;
    copy(candidate.context, sizeof(candidate.context), context.id);
    copy(candidate.word, sizeof(candidate.word), word.id);
    if (selection_ != Selection::AllSaved) {
      candidate.day = word.card.dueDay;
      candidate.minute = word.card.dueMinute;
      candidate.learning =
          word.card.state == uint8_t(study::State::Learning) || word.card.state == uint8_t(study::State::Relearning);
    }
    candidate.difficulty =
        word.card.difficulty + std::min(10u, unsigned(word.failures)) + std::min(10u, unsigned(word.card.lapses));
    int at = 0;
    while (at < count_ && !before(candidate, entries_[at])) ++at;
    if (at == kCapacity) return;
    const int end = std::min(count_, kCapacity - 1);
    for (int n = end; n > at; --n) entries_[n] = entries_[n - 1];
    entries_[at] = candidate;
    if (count_ < kCapacity) ++count_;
  }
  int next(int today, int minute) const {
    int ready = -1;
    for (int n = 0; n < count_; ++n) {
      const auto& e = entries_[n];
      if (e.finished || (e.learning && (e.day > today || (e.day == today && e.minute > minute)))) continue;
      if (ready < 0 || (e.learning && !entries_[ready].learning) ||
          (e.learning == entries_[ready].learning && before(e, entries_[ready])))
        ready = n;
    }
    return ready;
  }
  int waitingMinutes(int today, int minute) const {
    int delay = INT32_MAX;
    for (int n = 0; n < count_; ++n) {
      const auto& e = entries_[n];
      if (!e.finished)
        delay = std::min(delay, int(std::max<int64_t>(0, int64_t(e.day - today) * 1440 + e.minute - minute)));
    }
    return delay == INT32_MAX ? -1 : delay;
  }
  void answered(int index, const study::CardState& card) {
    auto& e = entries_[index];
    e.learning = card.state == uint8_t(study::State::Learning) || card.state == uint8_t(study::State::Relearning);
    e.finished = !e.learning;
    e.day = card.dueDay;
    e.minute = card.dueMinute;
  }

 private:
  bool before(const Entry& a, const Entry& b) const {
    if (selection_ == Selection::AllSaved) return std::strcmp(a.word, b.word) < 0;
    if (a.learning != b.learning) return a.learning;
    if (selection_ == Selection::Difficult && a.difficulty != b.difficulty) return a.difficulty > b.difficulty;
    if (a.day != b.day) return a.day < b.day;
    if (a.minute != b.minute) return a.minute < b.minute;
    return std::strcmp(a.word, b.word) < 0;
  }
  Entry entries_[kCapacity]{};
  int count_ = 0;
  Selection selection_ = Selection::Due;
  char after_[kIdBytes]{};
};
}  // namespace vocabulary
