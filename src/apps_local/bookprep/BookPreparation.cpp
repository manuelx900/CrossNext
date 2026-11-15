#include "BookPreparation.h"

#include <Epub/Page.h>
#include <GfxRenderer.h>
#include <Logging.h>
#include <Memory.h>
#include <strings.h>

#include <cstdio>
#include <cstring>

#include "PreparationKey.h"

namespace bookprep {
namespace {
constexpr char kFolders[] = "/.crosspoint/prepare-folders.tmp";
constexpr char kBooks[] = "/.crosspoint/prepare-books.tmp";
unsigned long chunkStarted = 0;
bool yieldChunk() { return millis() - chunkStarted >= 20; }
bool epubPath(const char* path) {
  const char* dot = std::strrchr(path, '.');
  return dot && !strcasecmp(dot, ".epub");
}
bool excluded(const char* name) {
  return !*name || *name == '.' || !strcasecmp(name, "System Volume Information") || !strcasecmp(name, "$RECYCLE.BIN");
}
}  // namespace
Job::Job() = default;
bool Job::writePath(HalFile& file, const char* value) {
  const size_t length = std::strlen(value);
  if (!length || length >= kPathBytes) return false;
  const uint8_t size[] = {uint8_t(length), uint8_t(length >> 8)};
  return file.write(size, sizeof(size)) == sizeof(size) && file.write(value, length) == length;
}
bool Job::readPath(HalFile& file, char* out, size_t capacity) {
  uint8_t size[2];
  if (file.read(size, sizeof(size)) != sizeof(size)) return false;
  const size_t length = size[0] | (size_t(size[1]) << 8);
  if (!length || length >= capacity || file.read(out, length) != int(length)) return false;
  out[length] = 0;
  return true;
}
bool Job::begin(uint16_t fonts) {
  if (fonts < 1 || fonts > 129 * 255 || !formats || (formats & ~AllFormats)) {
    fail();
    return false;
  }
  fontPasses = formats & FormatEpub ? fonts : 1;
  if (!Storage.ensureDirectoryExists("/.crosspoint") ||
      !(folders = Storage.open(kFolders, O_RDWR | O_CREAT | O_TRUNC)) ||
      !(books = Storage.open(kBooks, O_RDWR | O_CREAT | O_TRUNC)) || !writePath(folders, "/")) {
    fail();
    return false;
  }
  phase = Phase::Counting;
  return true;
}

bool Job::beginBook(std::shared_ptr<Epub> book) {
  if (!book) return false;
  epub = std::move(book);
  return beginLoadedBook();
}
bool Job::beginBook(std::shared_ptr<Xtc> book) {
  if (!book) return false;
  xtc = std::move(book);
  return beginLoadedBook();
}
bool Job::beginLoadedBook() {
  singleBook = true;
  const auto& source = epub ? epub->getPath() : xtc->getPath();
  if (source.size() >= sizeof(path)) return false;
  std::memcpy(path, source.c_str(), source.size() + 1);
  total = 1;
  chapterCount = epub ? epub->getSpineItemsCount() : 0;
  bookStep = epub ? BookStep::Chapters : BookStep::Cover;
  phase = Phase::Preparing;
  if (stampMatches()) {
    processed = 1;
    phase = Phase::Complete;
  }
  return true;
}
bool Job::stampMatches() {
  HalFile source;
  if (!Storage.openFileForRead("PREP", path, source)) return false;
  sourceSize = source.fileSize64();
  sourceTime = source.modificationTime();
  const auto key = preparationSignature(spec, epub != nullptr, rotateXtc, thumbnailHeights, kMaxThumbnailHeights);
  char suffix[48];
  std::snprintf(suffix, sizeof(suffix), "/ready-v1-%08lx.bin", static_cast<unsigned long>(key));
  stampPath = (epub ? epub->getCachePath() + "/sections" : xtc->getCachePath()) + suffix;
  HalFile stamp;
  uint64_t size = 0;
  uint32_t time = 0;
  if (!Storage.openFileForRead("PREP", stampPath, stamp) || stamp.size() != sizeof(size) + sizeof(time) ||
      stamp.read(&size, sizeof(size)) != sizeof(size) || stamp.read(&time, sizeof(time)) != sizeof(time) ||
      size != sourceSize || time != sourceTime)
    return false;
  for (const auto height : thumbnailHeights) {
    if (height <= 0) break;
    if (!Storage.exists((epub ? epub->getThumbBmpPath(height) : xtc->getThumbBmpPath(height)).c_str())) return false;
  }
  return true;
}
void Job::writeStamp() {
  if (stampPath.empty() || !sourceSize) {
    LOG_ERR("PREP", "Missing source identity: %s", path);
    bookFailed = true;
    return;
  }
  const std::string temp = stampPath + ".tmp";
  HalFile stamp;
  if (!Storage.openFileForWrite("PREP", temp, stamp) ||
      stamp.write(&sourceSize, sizeof(sourceSize)) != sizeof(sourceSize) ||
      stamp.write(&sourceTime, sizeof(sourceTime)) != sizeof(sourceTime) || !stamp.close() ||
      (Storage.exists(stampPath.c_str()) && !Storage.remove(stampPath.c_str())) ||
      !Storage.rename(temp.c_str(), stampPath.c_str())) {
    LOG_ERR("PREP", "Could not save completed preparation: %s", path);
    bookFailed = true;
  }
}
unsigned Job::progressTotal() const {
  unsigned covers = 0;
  for (auto height : thumbnailHeights) covers += height > 0;
  return unsigned(chapterCount) + covers;
}
unsigned Job::progressDone() const {
  return phase == Phase::Complete ? progressTotal() : unsigned(chapter) + thumbnail;
}
bool Job::nextFont(const ReaderRenderSpec& nextSpec) {
  if (phase != Phase::AwaitingFont || fontPass + 1 >= fontPasses) return false;
  if (!books.seek(0)) {
    fail();
    return false;
  }
  spec = nextSpec;
  ++fontPass;
  processed = 0;
  phase = Phase::Preparing;
  return true;
}
void Job::addThumbnailHeight(int height) {
  if (height <= 0) return;
  for (auto& entry : thumbnailHeights) {
    if (entry == height) return;
    if (!entry) {
      entry = height;
      return;
    }
  }
  LOG_ERR("PREP", "Thumbnail size limit: %d", height);
}
void Job::fail() {
  LOG_ERR("PREP", "Book preparation failed: %s", path);
  phase = Phase::Failed;
  close();
}
void Job::scanStep() {
  if (!directory) {
    if (directoryCursor == folders.size()) {
      folders.close();
      Storage.remove(kFolders);
      books.flush();
      if (!books.seek(0)) return fail();
      path[0] = 0;
      phase = total ? Phase::Preparing : Phase::Complete;
      if (!total) close();
      return;
    }
    if (!folders.seek(directoryCursor) || !readPath(folders, directoryPath, sizeof(directoryPath))) return fail();
    directoryCursor = folders.position();
    directory = Storage.open(directoryPath);
    if (!directory || !directory.isDirectory()) {
      ++skipped;
      directory.close();
    }
    return;
  }
  bool ioError = false;
  auto entry = directory.openNextFile(ioError);
  if (!entry) {
    if (ioError) ++skipped;
    directory.close();
    return;
  }
  if (++entries > kMaxEntries) return fail();
  if (entry.getName(name, sizeof(name)) >= sizeof(name) - 1) {
    ++skipped;
    return;
  }
  if (excluded(name)) return;
  const int length =
      std::snprintf(path, sizeof(path), "%s%s%s", directoryPath, std::strcmp(directoryPath, "/") ? "/" : "", name);
  if (length < 0 || size_t(length) >= sizeof(path)) {
    ++skipped;
    return;
  }
  if (entry.isDirectory()) {
    unsigned depth = 0;
    for (const char* p = path; *p; ++p) depth += *p == '/';
    if (depth >= kMaxDepth) {
      ++skipped;
      return;
    }
    if (!folders.seek(folders.size()) || !writePath(folders, path)) return fail();
  } else if (acceptsFormat(path, formats)) {
    if (!writePath(books, path)) return fail();
    ++total;
  }
}
void Job::finishBook() {
  if (singleBook && !bookFailed) writeStamp();
  imagePage.reset();
  section.reset();
  epub.reset();
  xtc.reset();
  if (bookFailed) ++failed;
  ++processed;
  bookFailed = false;
  chapter = chapterCount = 0;
  thumbnail = 0;
  bookStep = BookStep::Open;
  if (singleBook) {
    phase = failed ? Phase::Failed : Phase::Complete;
    return;
  }
  if (processed == total) {
    if (fontPass + 1 < fontPasses)
      phase = Phase::AwaitingFont;
    else {
      phase = Phase::Complete;
      close();
    }
  }
}
void Job::prepareStep(GfxRenderer& renderer) {
  switch (bookStep) {
    case BookStep::Open: {
      if (!readPath(books, path, sizeof(path))) return fail();
      if (fontPass && !epubPath(path)) return finishBook();
      if (epubPath(path)) {
        auto next = makeUniqueNoThrow<Epub>(path, "/.crosspoint");
        if (!next) {
          LOG_ERR("PREP", "OOM: EPUB");
          bookFailed = true;
          return finishBook();
        }
        epub = std::move(next);
        // The same scratch loan as the reader; the progress screen is redrawn
        // after it ends. No display callback runs while the buffer is loaned.
        GfxRenderer::FrameBufferLoan loan(renderer);
        if (!epub->load(true, !spec.embeddedStyle)) {
          bookFailed = true;
          return finishBook();
        }
        chapterCount = epub->getSpineItemsCount();
        bookStep = BookStep::Chapters;
      } else {
        xtc = makeUniqueNoThrow<Xtc>(path, "/.crosspoint", rotateXtc);
        if (!xtc || !xtc->load()) {
          LOG_ERR("PREP", "Could not load XTC: %s", path);
          bookFailed = true;
          return finishBook();
        }
        xtc->setupCacheDir();
        bookStep = BookStep::Cover;
      }
      return;
    }
    case BookStep::Chapters: {
      if (chapter == chapterCount) {
        bookStep = BookStep::Cover;
        return;
      }
      if (!section) {
        section = makeUniqueNoThrow<Section>(epub, chapter, renderer);
        if (!section) {
          LOG_ERR("PREP", "OOM: chapter");
          bookFailed = true;
          return finishBook();
        }
        if (section->loadPreparedSectionFile(spec) && !section->isPartial()) {
          imagePageNumber = 0;
          bookStep = BookStep::Images;
          return;
        }
        GfxRenderer::FrameBufferLoan loan(renderer);
        if (!section->startBuild(spec)) {
          bookFailed = true;
          return finishBook();
        }
        return;
      }
      GfxRenderer::FrameBufferLoan loan(renderer);
      chunkStarted = millis();
      if (!section->buildSomeMore(2, yieldChunk)) {
        bookFailed = true;
        return finishBook();
      }
      if (section->isBuildComplete()) {
        imagePageNumber = 0;
        bookStep = BookStep::Images;
      }
      return;
    }
    case BookStep::Images:
      prepareImages(renderer);
      return;
    case BookStep::Cover:
      if (fontPass) return finishBook();
      if (nativeCache.writing()) {
        nativeCache.step();
        return;
      }
      if (thumbnail < kMaxThumbnailHeights && thumbnailHeights[thumbnail] > 0) {
        const int height = thumbnailHeights[thumbnail++];
        if (epub && !epub->hasSupportedThumbnailSource()) {
          const auto placeholder = epub->getThumbBmpPath(height);
          if (!Storage.exists(placeholder.c_str())) {
            HalFile empty;
            if (!Storage.openFileForWrite("PREP", placeholder, empty) || !empty.close()) bookFailed = true;
          }
        } else if ((epub && !epub->generateThumbBmp(height)) || (xtc && !xtc->generateThumbBmp(height))) {
          bookFailed = true;
        }
        if (!bookFailed && !singleBook)
          for (const auto& area : nativeAreas) {
            if (area.height != height) continue;
            const auto source = epub ? epub->getThumbBmpPath(height) : xtc->getThumbBmpPath(height);
            if (Storage.openFileForRead("PREP", source.c_str(), nativeFile)) {
              const auto key = libraryui::PreparedCoverCache::key(renderer, area);
              if (!nativeCache.load(nativeFile, renderer, area, key, false))
                nativeCache.prepare(renderer, nativeBitmap, area, key, source.c_str());
              nativeFile.close();
            }
            break;
          }
        return;
      }
      finishBook();
      return;
  }
}
void Job::prepareImages(GfxRenderer& renderer) {
  if (imagePageNumber >= section->pageCount || (spec.imageRendering != 0 && spec.imageRendering != 3)) {
    imagePage.reset();
    section.reset();
    ++chapter;
    bookStep = BookStep::Chapters;
    return;
  }
  if (!imagePage) {
    imagePage = section->loadPage(imagePageNumber);
    imageElement = 0;
    if (!imagePage) {
      LOG_ERR("PREP", "Could not load image page: %d/%u", chapter, imagePageNumber);
      bookFailed = true;
      ++imagePageNumber;
      return;
    }
  }
  while (imageElement < imagePage->elements.size()) {
    const auto& element = imagePage->elements[imageElement++];
    if (element->getTag() != TAG_PageImage) continue;
    const auto& image = static_cast<const PageImage&>(*element).getImageBlock();
    if (!image.prepareCache(renderer, epub.get(), [](void* ctx, const char* src, const char* dest) {
          return static_cast<Epub*>(ctx)->extractItemToFile(src, dest);
        })) {
      LOG_ERR("PREP", "Could not prepare image: %s", image.getImagePath().c_str());
      bookFailed = true;
    }
    return;
  }
  imagePage.reset();
  ++imagePageNumber;
}
void Job::step(GfxRenderer& renderer) {
  switch (phase) {
    case Phase::Counting:
      scanStep();
      return;
    case Phase::Preparing:
      prepareStep(renderer);
      return;
    case Phase::Ready:
    case Phase::AwaitingFont:
    case Phase::Complete:
    case Phase::Cancelled:
    case Phase::Failed:
      return;
  }
}
void Job::cancel() {
  if (!running(phase)) return;
  phase = Phase::Cancelled;
  close();
}
void Job::close() {
  nativeFile.close();
  nativeCache.release();
  // Section's destructor commits any completed partial pages before EPUB dies.
  imagePage.reset();
  section.reset();
  epub.reset();
  xtc.reset();
  directory.close();
  folders.close();
  books.close();
  if (!singleBook) {
    Storage.remove(kFolders);
    Storage.remove(kBooks);
  }
}
Job::~Job() { close(); }
}  // namespace bookprep
