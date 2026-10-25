#pragma once

#include <HalStorage.h>

#include "../study/StudyImages.h"

namespace flashcards {
// Import/Zoom-upgrade only: full progressive JPEG detail, with a PSRAM budget.
bool progressiveJpegDetail(HalFile& source, HalFile& destination, study::ImageRef& ref);
}  // namespace flashcards
