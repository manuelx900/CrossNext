#pragma once

#include <HalStorage.h>

#include "../study/StudyImages.h"
#include "FlashProgress.h"

namespace flashcards {
// Import-owned SD scratch. Images are decoded one at a time by the reader's
// existing PNG/JPEG converters, then streamed into the deck's bitmap store.
class FlashMedia {
 public:
  ~FlashMedia();
  void setProgressReporter(ProgressReporter value) { progress = value; }
  bool begin(const char* archive, const char* directory);
  void prepare(const char* question, const char* answer);
  void prepareFace(const char* html, bool answer);
  bool hasMedia() const { return available; }
  bool question() const { return refs[0].valid(); }
  bool answer() const { return refs[1].valid(); }
  void reverse() {
    const auto q = refs[0];
    refs[0] = refs[1];
    refs[1] = q;
    const auto z = zoomRefs[0];
    zoomRefs[0] = zoomRefs[1];
    zoomRefs[1] = z;
  }
  bool append();
  bool finish(uint32_t count);

 private:
  ProgressReporter progress;
  HalFile entries, pixels, zoomEntries, zoomPixels;
  const char* archive = nullptr;
  const char* directory = nullptr;
  bool available = false;
  uint32_t pictures = 0;
  study::ImageRef refs[2];
  study::ImageRef zoomRefs[2];
  uint8_t ioBuffer[study::kMaxZoomWidth]{};
  char path[160]{}, filename[256]{}, value[256]{}, member[24]{};
  bool locate();
  bool convert(bool answer);
  bool convertBitmap(study::ImageRef&, HalFile& destination, int maxWidth, int maxHeight);
  bool finishStore(uint32_t count, bool zoom);
  void cleanup();
};
}  // namespace flashcards
