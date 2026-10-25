#pragma once

namespace flashcards {
// Cold path, only on Zoom: publish image data after verifying the original deck.
bool upgradeZoomImages(const char* directory);
}  // namespace flashcards
