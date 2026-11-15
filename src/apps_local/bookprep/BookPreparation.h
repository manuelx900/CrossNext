#pragma once

#include <Bitmap.h>
#include <Epub.h>
#include <Epub/ReaderRenderSpec.h>
#include <Epub/Section.h>
#include <HalStorage.h>
#include <Xtc.h>

#include <cstdint>
#include <memory>

#include "../library/PreparedCoverCache.h"
#include "PreparationChoices.h"

class GfxRenderer;
class Page;

namespace bookprep {
enum class Phase : uint8_t { Ready, Counting, Preparing, AwaitingFont, Complete, Cancelled, Failed };
inline bool running(Phase phase) {
  return phase == Phase::Counting || phase == Phase::Preparing || phase == Phase::AwaitingFont;
}

// The queue lives on SD, not in a library-sized RAM array. One book and one
// incremental chapter builder are retained; their ordinary cache limits apply.
class Job {
 public:
  Job();
  static constexpr size_t kPathBytes = 1024;
  static constexpr unsigned kMaxDepth = 32;
  static constexpr unsigned kMaxEntries = 1000000;
  Phase phase = Phase::Ready;
  uint32_t total = 0, processed = 0, failed = 0, skipped = 0;
  uint16_t fontPass = 0, fontPasses = 1;
  int chapter = 0, chapterCount = 0;
  char path[kPathBytes]{};
  char directoryPath[kPathBytes]{};
  char name[256]{};
  ReaderRenderSpec spec;
  static constexpr unsigned kMaxThumbnailHeights = 48;
  int thumbnailHeights[kMaxThumbnailHeights]{};
  bool rotateXtc = false;
  freeink::ui::Rect nativeAreas[2]{};
  uint8_t formats = AllFormats;

  bool begin(uint16_t layouts = 1);
  bool beginBook(std::shared_ptr<Epub> book);
  bool beginBook(std::shared_ptr<Xtc> book);
  unsigned progressDone() const;
  unsigned progressTotal() const;
  bool nextFont(const ReaderRenderSpec& nextSpec);
  void addThumbnailHeight(int height);
  void step(GfxRenderer& renderer);
  void cancel();
  void close();
  void abort() { fail(); }
  ~Job();

 private:
  enum class BookStep : uint8_t { Open, Chapters, Images, Cover };
  BookStep bookStep = BookStep::Open;
  HalFile folders, books, directory;
  size_t directoryCursor = 0;
  uint32_t entries = 0;
  bool bookFailed = false;
  unsigned thumbnail = 0;
  std::shared_ptr<Epub> epub;
  std::shared_ptr<Xtc> xtc;
  std::unique_ptr<Section> section;
  std::unique_ptr<Page> imagePage;
  uint16_t imagePageNumber = 0;
  size_t imageElement = 0;
  bool singleBook = false;
  std::string stampPath;
  libraryui::PreparedCoverCache nativeCache;
  HalFile nativeFile;
  Bitmap nativeBitmap{nativeFile};
  uint64_t sourceSize = 0;
  uint32_t sourceTime = 0;
  bool beginLoadedBook();
  bool stampMatches();
  void writeStamp();
  void prepareImages(GfxRenderer& renderer);
  static bool writePath(HalFile& file, const char* value);
  static bool readPath(HalFile& file, char* out, size_t size);
  void scanStep();
  void prepareStep(GfxRenderer& renderer);
  void finishBook();
  void fail();
};
}  // namespace bookprep
