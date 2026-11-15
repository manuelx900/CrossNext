#pragma once

#include <atomic>
#include <cstdint>

namespace questsim {
inline constexpr char READING_DOCUMENT[] = "71756573742d73696d756c61746f7231";
static_assert(sizeof(READING_DOCUMENT) == 33);
constexpr uint64_t unobservedPart(uint64_t start, uint64_t end, uint64_t observed) {
  const auto cursor = start > observed ? start : observed;
  return end > cursor ? end - cursor : 0;
}

enum class ReadingResult : uint16_t {
  Unavailable = 0,
  Ready = 1,
  Queued = 2,
  Scanning = 3,
  Done = 4,
  NeedsHero = 5,
  FinishScreen = 6,
  StorageError = 7,
  InvalidMinutes = 8,
  Busy = 9,
  NotReady = 10,
};

// One browser command, published atomically to the activity's loop task.
class ReadingMailbox {
 public:
  static constexpr int MAX_MINUTES = 1440;
  void activate() { word.store(pack(ReadingResult::Ready), std::memory_order_release); }
  void deactivate() { word.store(pack(ReadingResult::Unavailable), std::memory_order_release); }
  ReadingResult result() const { return state(word.load(std::memory_order_acquire)); }
  ReadingResult request(int minutes) {
    if (minutes < 0 || minutes > MAX_MINUTES) return ReadingResult::InvalidMinutes;
    auto expected = word.load(std::memory_order_acquire);
    switch (state(expected)) {
      case ReadingResult::Unavailable:
        return ReadingResult::Unavailable;
      case ReadingResult::Queued:
      case ReadingResult::Scanning:
        return ReadingResult::Busy;
      case ReadingResult::Ready:
      case ReadingResult::Done:
      case ReadingResult::NeedsHero:
      case ReadingResult::FinishScreen:
      case ReadingResult::StorageError:
      case ReadingResult::InvalidMinutes:
      case ReadingResult::Busy:
      case ReadingResult::NotReady:
        break;
    }
    return word.compare_exchange_strong(expected, pack(ReadingResult::Queued, minutes), std::memory_order_acq_rel)
               ? ReadingResult::Queued
               : ReadingResult::Busy;
  }
  bool begin(int& minutes) {
    auto expected = word.load(std::memory_order_acquire);
    if (state(expected) != ReadingResult::Queued) return false;
    const auto value = expected & 0xffff;
    if (!word.compare_exchange_strong(expected, pack(ReadingResult::Scanning, value), std::memory_order_acq_rel))
      return false;
    minutes = int(value);
    return true;
  }
  void finish(ReadingResult result) {
    auto expected = word.load(std::memory_order_acquire);
    if (state(expected) == ReadingResult::Scanning)
      word.compare_exchange_strong(expected, pack(result), std::memory_order_acq_rel);
  }

 private:
  static constexpr uint32_t pack(ReadingResult result, unsigned minutes = 0) {
    return (uint32_t(result) << 16) | minutes;
  }
  static constexpr ReadingResult state(uint32_t value) { return ReadingResult(value >> 16); }
  std::atomic<uint32_t> word{pack(ReadingResult::Unavailable)};
};
}  // namespace questsim
