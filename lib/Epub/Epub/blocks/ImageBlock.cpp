#include "ImageBlock.h"

#include <FontCacheManager.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalMemory.h>
#include <Logging.h>
#include <Memory.h>
#include <Performance.h>
#include <Serialization.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "Epub/PageReadBudget.h"
#include "Epub/converters/DirectPixelWriter.h"
#include "Epub/converters/ImageDecoderFactory.h"

// Cache file format:
// - uint16_t width
// - uint16_t height
// - uint8_t pixels[...] - 2 bits per pixel, packed (4 pixels per byte), row-major order

ImageBlock::ImageBlock(const std::string& imagePath, const std::string& srcPath, int16_t width, int16_t height)
    : imagePath(imagePath), srcPath(srcPath), width(width), height(height) {}

void* ImageBlock::extractCtx = nullptr;
ImageBlock::ExtractFn ImageBlock::extractFn = nullptr;

void ImageBlock::setExtractor(void* ctx, ExtractFn fn) {
  extractCtx = ctx;
  extractFn = fn;
}

namespace {

std::string getCachePath(const std::string& imagePath) {
  // Replace extension with .pxc (pixel cache)
  size_t dotPos = imagePath.rfind('.');
  if (dotPos != std::string::npos) {
    return imagePath.substr(0, dotPos) + ".pxc";
  }
  return imagePath + ".pxc";
}

std::string getSizedCachePath(const std::string& imagePath, int width, int height) {
  char suffix[32];
  std::snprintf(suffix, sizeof(suffix), ".%dx%d.pxc", width, height);
  return imagePath + suffix;
}

bool readValidCacheHeader(HalFile& cacheFile, const int expectedWidth, const int expectedHeight, uint16_t& cachedWidth,
                          uint16_t& cachedHeight) {
  if (cacheFile.read(&cachedWidth, 2) != 2 || cacheFile.read(&cachedHeight, 2) != 2) {
    return false;
  }
  if (!cachedWidth || !cachedHeight) return false;

  const int widthDiff = abs(cachedWidth - expectedWidth);
  const int heightDiff = abs(cachedHeight - expectedHeight);
  if (widthDiff > 1 || heightDiff > 1) {
    return false;
  }

  const size_t bytesPerRow = (cachedWidth + 3) / 4;
  const size_t expectedSize = 4 + bytesPerRow * cachedHeight;
  return cacheFile.size() >= expectedSize;
}

// Suppress repeated failures across the BW/grayscale passes of one page render.
// Clear before the next page render so transient memory/storage failures retry.
constexpr size_t MAX_RENDER_IMAGE_FAILURES = 16;
uint64_t failedImageHashes[MAX_RENDER_IMAGE_FAILURES];
size_t failedImageCount = 0;

uint64_t imagePathHash(const std::string& path) {
  uint64_t hash = 14695981039346656037ull;
  for (const char c : path) {
    hash ^= static_cast<uint8_t>(c);
    hash *= 1099511628211ull;
  }
  return hash;
}

bool imageFailedThisRender(const std::string& path) {
  const uint64_t hash = imagePathHash(path);
  for (size_t i = 0; i < failedImageCount; i++) {
    if (failedImageHashes[i] == hash) return true;
  }
  return false;
}

void rememberImageFailure(const std::string& path) {
  if (failedImageCount == MAX_RENDER_IMAGE_FAILURES || imageFailedThisRender(path)) return;
  failedImageHashes[failedImageCount++] = imagePathHash(path);
}

// --- Per-page-render RAM slot for the pixel cache ----------------------------
// The tiled grayscale flow re-renders an image page once for the BW
// double-refresh and again for every band of both gray planes, and each pass
// re-read the whole .pxc off SD (~100 ms for a full-page image, ~13 passes).
// Column clipping cannot reduce the SD traffic: the row stride (~100 B) is
// smaller than an SD sector, so every sector is touched regardless of the band
// window. Instead the first pass loads the payload into RAM and later passes
// render from it. Chunked allocation because a single full-image block (up to
// 96 KB) rarely fits the fragmented mid-render heap; each chunk is heap-gated
// and any failure falls back to the streaming path unchanged. The reader
// retains bounded PSRAM slots across redraws; internal chunks remain transient.
constexpr size_t PXC_CHUNK_SHIFT = 14;  // 16 KB chunks
constexpr size_t PXC_CHUNK_SIZE = 1u << PXC_CHUNK_SHIFT;
constexpr size_t PXC_MAX_CHUNKS = 6;  // 96 KB: a full-screen 2bpp image
constexpr size_t PXC_HEAP_RESERVE = 24 * 1024;
constexpr size_t PXC_MAX_ALLOC_RESERVE = 8 * 1024;
// Rows can straddle a chunk boundary; they are reassembled into a stack
// buffer. (screenWidth + 3) / 4 caps at 200 B for an 800px panel.
constexpr int PXC_MAX_BYTES_PER_ROW = 208;

struct PsramImage {
  HalMemory::PsramBuffer pixels;
  uint64_t hash = 0;
  uint16_t width = 0, height = 0;
  uint32_t render = 0;
  uint32_t contentRevision = 0, mediaRevision = 0;
};
// Three image resources, at most 96 KiB each, share the serialized page render.
// They are never allocated from internal RAM; the reader releases them on exit.
PsramImage psramImages[3];
uint32_t imageRender = 1;

std::unique_ptr<uint8_t[]> pxcChunks[PXC_MAX_CHUNKS];
uint64_t pxcSlotHash = 0;
uint16_t pxcSlotWidth = 0;
uint16_t pxcSlotHeight = 0;

void releasePxcSlot() {
  for (auto& image : psramImages) image = {};
  for (auto& chunk : pxcChunks) chunk.reset();
  pxcSlotHash = 0;
  pxcSlotWidth = 0;
  pxcSlotHeight = 0;
}

const uint8_t* pxcRowPtr(size_t rowStart, int bytesPerRow, uint8_t* tempRow) {
  const size_t chunk = rowStart >> PXC_CHUNK_SHIFT;
  const size_t offset = rowStart & (PXC_CHUNK_SIZE - 1);
  if (offset + bytesPerRow <= PXC_CHUNK_SIZE) {
    return pxcChunks[chunk].get() + offset;
  }
  const size_t firstPart = PXC_CHUNK_SIZE - offset;
  memcpy(tempRow, pxcChunks[chunk].get() + offset, firstPart);
  memcpy(tempRow + firstPart, pxcChunks[chunk + 1].get(), bytesPerRow - firstPart);
  return tempRow;
}

// cacheFile is positioned just past the header. True when the slot holds the
// full pixel payload for this cache path afterward.
bool loadPxcSlot(uint64_t cacheHash, HalFile& cacheFile, uint16_t cachedWidth, uint16_t cachedHeight, int bytesPerRow) {
  releasePxcSlot();
  if (bytesPerRow > PXC_MAX_BYTES_PER_ROW) {
    return false;
  }
  size_t remaining = (size_t)bytesPerRow * cachedHeight;
  const size_t chunkCount = (remaining + PXC_CHUNK_SIZE - 1) >> PXC_CHUNK_SHIFT;
  if (chunkCount == 0 || chunkCount > PXC_MAX_CHUNKS) {
    return false;
  }
  for (size_t i = 0; i < chunkCount; i++) {
    const size_t want = remaining < PXC_CHUNK_SIZE ? remaining : PXC_CHUNK_SIZE;
    if (ESP.getFreeHeap() < remaining + PXC_HEAP_RESERVE || ESP.getMaxAllocHeap() < want + PXC_MAX_ALLOC_RESERVE) {
      releasePxcSlot();
      return false;
    }
    pxcChunks[i] = makeUniqueNoThrow<uint8_t[]>(want);
    if (!pxcChunks[i] || cacheFile.read(pxcChunks[i].get(), want) != static_cast<int>(want)) {
      releasePxcSlot();
      return false;
    }
    remaining -= want;
  }
  pxcSlotHash = cacheHash;
  pxcSlotWidth = cachedWidth;
  pxcSlotHeight = cachedHeight;
  return true;
}

void renderRowsFromPxcSlot(GfxRenderer& renderer, int x, int y, const PsramImage* image = nullptr) {
  const int width = image ? image->width : pxcSlotWidth;
  const int height = image ? image->height : pxcSlotHeight;
  const int bytesPerRow = (width + 3) / 4;
  // The render owner serializes this 208-byte fallback scratch with the cache.
  static uint8_t tempRow[PXC_MAX_BYTES_PER_ROW];

  DirectPixelWriter pw;
  pw.init(renderer);

  for (int row = 0; row < height; row++) {
    const uint8_t* rowBuffer = image ? image->pixels.get() + size_t(row) * bytesPerRow
                                     : pxcRowPtr((size_t)row * bytesPerRow, bytesPerRow, tempRow);
    pw.beginRow(y + row);
    int colStart, colEnd;
    pw.bandColRange(x, width, colStart, colEnd);
    for (int col = colStart; col < colEnd; col++) {
      const int byteIdx = col >> 2;            // col / 4
      const int bitShift = 6 - (col & 3) * 2;  // MSB first within byte
      const uint8_t pixelValue = (rowBuffer[byteIdx] >> bitShift) & 0x03;
      pw.writePixel(x + col, pixelValue);
    }
  }
}

bool renderFromCache(GfxRenderer& renderer, const std::string& cachePath, int x, int y, int expectedWidth,
                     int expectedHeight, bool prefetch = false, ImageBlock::ContinueFn continueWork = nullptr,
                     void* workContext = nullptr) {
  // Valid resident payloads serve both later planes and later redraws.
  const uint64_t cacheHash = imagePathHash(cachePath);
  for (auto& image : psramImages) {
    if (image.pixels && image.hash == cacheHash && image.contentRevision && image.mediaRevision &&
        image.contentRevision == Storage.resourceRevision() && image.mediaRevision == Storage.mediaRevision() &&
        std::strcmp(reinterpret_cast<const char*>(image.pixels.get() + size_t((image.width + 3) / 4) * image.height),
                    cachePath.c_str()) == 0 &&
        abs(int(image.width) - expectedWidth) <= 1 && abs(int(image.height) - expectedHeight) <= 1) {
      image.render = imageRender;
      if (!prefetch) {
        performance::count(performance::Counter::ImageRamHit);
        renderRowsFromPxcSlot(renderer, x, y, &image);
      }
      return true;
    }
  }
  if (!prefetch && pxcSlotHash == cacheHash && pxcSlotWidth != 0) {
    renderRowsFromPxcSlot(renderer, x, y);
    return true;
  }

  if (prefetch && (!HalMemory::getPsramHeap().totalBytes || (continueWork && !continueWork(workContext)))) return false;
  const auto sourceContent = Storage.resourceRevision(), sourceMedia = Storage.mediaRevision();
  HalFile cacheFile;
  if (!Storage.openFileForRead("IMG", cachePath, cacheFile)) {
    return false;
  }

  uint16_t cachedWidth, cachedHeight;
  if (!readValidCacheHeader(cacheFile, expectedWidth, expectedHeight, cachedWidth, cachedHeight)) {
    LOG_ERR("IMG", "Invalid image cache: %s", cachePath.c_str());
    return false;
  }

  performance::count(performance::Counter::ImageDiskHit);
  LOG_DBG("IMG", "Loading from cache: %s (%dx%d)", cachePath.c_str(), cachedWidth, cachedHeight);

  const int bytesPerRow = (cachedWidth + 3) / 4;  // 2 bits per pixel, 4 pixels per byte
  const auto psram = HalMemory::getPsramHeap();
  if (psram.totalBytes && sourceContent && sourceMedia) {
    PsramImage* candidate = nullptr;
    for (auto& slot : psramImages) {
      if (slot.render == imageRender && slot.pixels) continue;
      if (!candidate || !slot.pixels || (candidate->pixels && slot.render < candidate->render)) candidate = &slot;
      if (!slot.pixels) break;
    }
    if (candidate) {
      auto& image = *candidate;
      const size_t bytes = size_t(bytesPerRow) * cachedHeight;
      const size_t pathBytes = cachePath.size() + 1;
      if (bytes <= PXC_MAX_CHUNKS * PXC_CHUNK_SIZE && pathBytes <= 1025 &&
          psram.freeBytes >= bytes + pathBytes + 512 * 1024) {
        image = {};
        // Pixels and exact path share one bounded PSRAM allocation per slot.
        image.pixels = HalMemory::allocatePsram(bytes + pathBytes);
        if (!image.pixels) {
          LOG_ERR("IMG", "PSRAM image cache unavailable (%u bytes); streaming", unsigned(bytes + pathBytes));
        } else {
          size_t read = 0;
          while (read < bytes && (!prefetch || !continueWork || continueWork(workContext))) {
            const size_t count = prefetch ? std::min<size_t>(4096, bytes - read) : bytes - read;
            if (cacheFile.read(image.pixels.get() + read, count) != int(count)) break;
            read += count;
          }
          if (read == bytes && (!prefetch || !continueWork || continueWork(workContext)) &&
              sourceContent == Storage.resourceRevision() && sourceMedia == Storage.mediaRevision()) {
            memcpy(image.pixels.get() + bytes, cachePath.c_str(), pathBytes);
            image.hash = cacheHash;
            image.width = cachedWidth;
            image.height = cachedHeight;
            image.render = imageRender;
            image.contentRevision = sourceContent;
            image.mediaRevision = sourceMedia;
            if (!prefetch) renderRowsFromPxcSlot(renderer, x, y, &image);
            return true;
          }
          image = {};
          if (prefetch || sourceContent != Storage.resourceRevision() || sourceMedia != Storage.mediaRevision())
            return false;
        }
      }
    }
  }

  if (prefetch) return false;

  // First pass of a page render: try to pull the payload into the RAM slot so
  // the remaining ~12 passes skip SD entirely. Only an EMPTY slot is claimed:
  // the slot lives until the page render completes, so a populated slot with a
  // different hash means another image on this same page owns it. Evicting it
  // here would make 2+ image pages reload each other from SD on every pass
  // (all the SD traffic of streaming plus the slot alloc churn); instead later
  // images take the streaming path below, unchanged from pre-cache behavior.
  if (!psram.totalBytes && pxcSlotHash == 0 &&
      loadPxcSlot(cacheHash, cacheFile, cachedWidth, cachedHeight, bytesPerRow)) {
    renderRowsFromPxcSlot(renderer, x, y);
    LOG_DBG("IMG", "Cache render complete (payload now in RAM)");
    return true;
  }

  // Streaming fallback (slot didn't fit). A failed slot load may have consumed
  // part of the payload; rewind to just past the header.
  cacheFile.seek(4);

  // Read several rows per SD access. A one-row-per-read loop here means
  // cachedHeight (~728) tiny reads through the storage mutex + SdFat; batching
  // rows into a ~4KB buffer cuts that to ~20 reads per pass without holding the
  // whole image.
  int rowsPerRead = 4096 / bytesPerRow;
  if (rowsPerRead < 1) rowsPerRead = 1;
  if (rowsPerRead > cachedHeight) rowsPerRead = cachedHeight;
  uint8_t* readBuffer = (uint8_t*)malloc((size_t)rowsPerRead * bytesPerRow);
  if (!readBuffer) {
    // Fall back to a single-row buffer under memory pressure.
    rowsPerRead = 1;
    readBuffer = (uint8_t*)malloc(bytesPerRow);
  }
  if (!readBuffer) {
    LOG_ERR("IMG", "Failed to allocate row buffer");
    return false;
  }

  DirectPixelWriter pw;
  pw.init(renderer);

  int rowsInBuffer = 0;
  int bufferRow = 0;
  for (int row = 0; row < cachedHeight; row++) {
    if (bufferRow >= rowsInBuffer) {
      const int toRead = (cachedHeight - row < rowsPerRead) ? (cachedHeight - row) : rowsPerRead;
      const size_t bytes = (size_t)toRead * bytesPerRow;
      if (cacheFile.read(readBuffer, bytes) != static_cast<int>(bytes)) {
        LOG_ERR("IMG", "Cache read error at row %d", row);
        free(readBuffer);
        return false;
      }
      rowsInBuffer = toRead;
      bufferRow = 0;
    }

    const uint8_t* rowBuffer = readBuffer + (size_t)bufferRow * bytesPerRow;
    bufferRow++;

    const int destY = y + row;
    pw.beginRow(destY);
    // On a grayscale strip pass only a narrow column window of the image is in
    // the active band; skip the rest instead of unpacking+clipping every pixel.
    int colStart, colEnd;
    pw.bandColRange(x, cachedWidth, colStart, colEnd);
    for (int col = colStart; col < colEnd; col++) {
      const int byteIdx = col >> 2;            // col / 4
      const int bitShift = 6 - (col & 3) * 2;  // MSB first within byte
      uint8_t pixelValue = (rowBuffer[byteIdx] >> bitShift) & 0x03;

      pw.writePixel(x + col, pixelValue);
    }
  }

  free(readBuffer);
  LOG_DBG("IMG", "Cache render complete");
  return true;
}

}  // namespace

bool ImageBlock::hasValidCache() const {
  const auto cachePath = getSizedCachePath(imagePath, width, height);
  HalFile cacheFile;
  if (!Storage.openFileForRead("IMG", cachePath, cacheFile)) {
    if (!Storage.openFileForRead("IMG", getCachePath(imagePath), cacheFile)) return false;
  }

  uint16_t cachedWidth, cachedHeight;
  return readValidCacheHeader(cacheFile, width, height, cachedWidth, cachedHeight);
}

bool ImageBlock::prefetchPixels(GfxRenderer& renderer, ContinueFn continueWork, void* workContext) const {
  if (renderFromCache(renderer, getSizedCachePath(imagePath, width, height), 0, 0, width, height, true, continueWork,
                      workContext))
    return true;
  if (continueWork && !continueWork(workContext)) return false;
  return renderFromCache(renderer, getCachePath(imagePath), 0, 0, width, height, true, continueWork, workContext);
}

bool ImageBlock::needsDecode() const { return !imageFailedThisRender(imagePath) && !hasValidCache(); }

void ImageBlock::clearRenderFailures() { failedImageCount = 0; }

void ImageBlock::releaseRenderCache() { releasePxcSlot(); }

void ImageBlock::beginRenderCache() {
  if (++imageRender == 0) {
    releasePxcSlot();
    imageRender = 1;
  }
  const auto heap = HalMemory::getPsramHeap();
  if (heap.totalBytes && heap.freeBytes < 512 * 1024) releasePxcSlot();
  for (auto& image : psramImages) {
    if (image.pixels &&
        (image.contentRevision != Storage.resourceRevision() || image.mediaRevision != Storage.mediaRevision()))
      image = {};
  }
}

void ImageBlock::endRenderCache() {
  // The internal-RAM fallback is transient; only bounded PSRAM slots survive.
  for (auto& chunk : pxcChunks) chunk.reset();
  pxcSlotHash = 0;
  pxcSlotWidth = pxcSlotHeight = 0;
}

void ImageBlock::renderPlaceholder(GfxRenderer& renderer, const int x, const int y) const {
  renderer.fillRect(x, y, width, height, true);
  if (width > 2 && height > 2) {
    renderer.fillRect(x + 1, y + 1, width - 2, height - 2, false);
  }
}

void ImageBlock::render(GfxRenderer& renderer, const int x, const int y) {
  // The font-prewarm scan pass only accumulates glyphs; an image contributes
  // none, and its DirectPixelWriter output bypasses the renderer's scan-mode
  // suppression, so it would otherwise do a full (discarded) cache render every
  // page view. Skip it here. The image still draws in the real BW/grayscale
  // passes; on first view this just moves the one-time decode to the BW pass.
  FontCacheManager* fcm = renderer.getFontCacheManager();
  if (fcm && fcm->isScanning()) return;

  LOG_DBG("IMG", "Rendering image at %d,%d: %s (%dx%d)", x, y, imagePath.c_str(), width, height);

  const int screenWidth = renderer.getScreenWidth();
  const int screenHeight = renderer.getScreenHeight();

  // Bounds check render position using logical screen dimensions
  if (x < 0 || y < 0 || x + width > screenWidth || y + height > screenHeight) {
    LOG_ERR("IMG", "Invalid render position: (%d,%d) size (%dx%d) screen (%dx%d)", x, y, width, height, screenWidth,
            screenHeight);
    return;
  }

  // Tiled grayscale (#2190): skip the whole image when it doesn't touch the
  // active band. The per-pixel writer already clips off-band pixels, but without
  // this each of the ~7 bands per plane re-ran the full cache load / pixel walk
  // and discarded the result — the dominant cost of AA on image pages. The check
  // is orientation-aware and returns true when no strip is active, so the BW
  // pass and non-tiled controllers render the image exactly as before.
  if (!renderer.glyphIntersectsStrip(x, y, x + width - 1, y + height - 1)) {
    return;
  }

  if (imageFailedThisRender(imagePath)) {
    renderPlaceholder(renderer, x, y);
    return;
  }

  // Try to render from cache first
  std::string cachePath = getSizedCachePath(imagePath, width, height);
  if (renderFromCache(renderer, cachePath, x, y, width, height) ||
      renderFromCache(renderer, getCachePath(imagePath), x, y, width, height)) {
    renderer.preserveImagePolarity(x, y, width, height);
    return;  // Successfully rendered from cache
  }

  if (!decodeAndCache(renderer, x, y, extractCtx, extractFn)) {
    rememberImageFailure(imagePath);
    renderPlaceholder(renderer, x, y);
    return;
  }
  renderer.preserveImagePolarity(x, y, width, height);
}

bool ImageBlock::prepareCache(GfxRenderer& renderer, void* ctx, ExtractFn extract, int x, int y,
                              ContinueFn continueWork, void* workContext) const {
  if (continueWork && (x < 0 || y < 0 || width <= 0 || height <= 0 || x + width > renderer.getScreenWidth() ||
                       y + height > renderer.getScreenHeight()))
    return false;
  if (continueWork && !continueWork(workContext)) return false;
  if (hasValidCache()) return true;
  if (continueWork && !FsHelpers::hasJpgExtension(imagePath) && !FsHelpers::hasPngExtension(imagePath)) return false;
  if (!renderer.hasFrameBuffer()) return false;
  return decodeAndCache(renderer, x, y, ctx, extract, true, continueWork, workContext) && hasValidCache();
}

bool ImageBlock::decodeAndCache(GfxRenderer& renderer, int x, int y, void* ctx, ExtractFn extract, bool cacheOnly,
                                ContinueFn continueWork, void* workContext) const {
  if (continueWork && !continueWork(workContext)) return false;
  if (!srcPath.empty() && extract && !Storage.exists(imagePath.c_str())) {
    LOG_DBG("IMG", "Lazy-extracting %s -> %s", srcPath.c_str(), imagePath.c_str());
    performance::Scope extracting(performance::Stage::Extract);
    if (!extract(ctx, srcPath.c_str(), imagePath.c_str())) {
      LOG_ERR("IMG", "Lazy extraction failed: %s", srcPath.c_str());
      return false;
    }
  }

  // No cache - need to decode the image
  // Check if image file exists
  HalFile file;
  if (!Storage.openFileForRead("IMG", imagePath, file)) {
    LOG_ERR("IMG", "Image file not found: %s", imagePath.c_str());
    return false;
  }
  size_t fileSize = file.size();
  file.close();

  if (fileSize == 0) {
    LOG_ERR("IMG", "Image file is empty: %s", imagePath.c_str());
    return false;
  }

  LOG_DBG("IMG", "Decoding and caching: %s", imagePath.c_str());

  RenderConfig config;
  config.x = x;
  config.y = y;
  config.maxWidth = width;
  config.maxHeight = height;
  config.useGrayscale = true;
  config.useDithering = true;
  config.performanceMode = false;
  config.useExactDimensions = true;  // Use pre-calculated dimensions to avoid rounding mismatches
  config.cacheOnly = cacheOnly;
  config.continueWork = continueWork;
  config.workContext = workContext;
  config.cachePath = getSizedCachePath(imagePath, width, height);

  ImageToFramebufferDecoder* decoder = ImageDecoderFactory::getDecoder(imagePath);
  if (!decoder) {
    LOG_ERR("IMG", "No decoder found for image: %s", imagePath.c_str());
    return false;
  }

  LOG_DBG("IMG", "Using %s decoder", decoder->getFormatName());

  performance::count(performance::Counter::ImageDecode);
  performance::Scope decoding(performance::Stage::Decode);
  bool success = decoder->decodeToFramebuffer(imagePath, renderer, config);
  if (!success) {
    LOG_ERR("IMG", "Failed to decode image: %s", imagePath.c_str());
    return false;
  }

  LOG_DBG("IMG", "Decode successful");
  return true;
}

bool ImageBlock::serialize(Print& file) {
  serialization::writeString(file, imagePath);
  serialization::writeString(file, srcPath);
  serialization::writePod(file, width);
  serialization::writePod(file, height);
  return true;
}

std::unique_ptr<ImageBlock> ImageBlock::deserialize(HalFile& file, PageReadBudget* budget) {
  serialization::ReadBuffer input(file);
  auto result = deserialize(input, budget);
  if (!input.finish()) return nullptr;
  return result;
}

std::unique_ptr<ImageBlock> ImageBlock::deserialize(serialization::ReadBuffer& file, PageReadBudget* budget) {
  constexpr size_t MAX_PATH = 1024;
  // Two bounded temporary strings and their block-owned copies fit the page budget.
  if (budget) {
    if (!budget->claim(sizeof(ImageBlock))) return nullptr;
    for (int n = 0; n < 4; ++n)
      if (!budget->claim(MAX_PATH + 1)) return nullptr;
  }
  std::string path;
  std::string src;
  int16_t w = 0, h = 0;
  if (!serialization::tryReadString(file, path, MAX_PATH) || !serialization::tryReadString(file, src, MAX_PATH) ||
      !serialization::tryReadPod(file, w) || !serialization::tryReadPod(file, h) || path.empty() || w <= 0 || h <= 0)
    return nullptr;
  auto block = makeUniqueNoThrow<ImageBlock>(path, src, w, h);
  if (!block) LOG_ERR("IMG", "OOM: ImageBlock");
  return block;
}
