#pragma once

#include <FreeInkUI.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Activity;
class GfxRenderer;
class MappedInputManager;

namespace bookorbit {

enum class BatchOutcome : uint8_t { Synced, Skipped, Failed, Count };
constexpr int BATCH_OUTCOME_COUNT = static_cast<int>(BatchOutcome::Count);
using BatchCounts = std::array<unsigned, BATCH_OUTCOME_COUNT>;

struct BatchBook {
  std::string path;
  BatchOutcome outcome = BatchOutcome::Failed;
};

void drawBatchCards(freeink::ui::Frame<24>& frame, freeink::ui::Rect body, const BatchCounts& counts,
                    freeink::ui::ActionId action, int selection);
// The parent activity keeps the completed books alive until the list is dismissed.
std::unique_ptr<Activity> makeBatchBookList(GfxRenderer&, MappedInputManager&, const std::vector<BatchBook>&,
                                            BatchOutcome);

}  // namespace bookorbit
