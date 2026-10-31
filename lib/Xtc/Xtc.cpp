/**
 * Xtc.cpp
 *
 * Main XTC ebook class implementation
 * XTC ebook support for CrossPoint Reader
 */

#include "Xtc.h"

#include <Bitmap.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Xtc/CoverPixels.h"

namespace {
void yieldDuringThumbnail(uint8_t& rowsSinceYield) {
  if (++rowsSinceYield < 8) return;
  rowsSinceYield = 0;
  vTaskDelay(1);
}
}  // namespace

bool Xtc::load() {
  LOG_DBG("XTC", "Loading XTC: %s", filepath.c_str());

  // Initialize parser
  parser = makeUniqueNoThrow<xtc::XtcParser>();
  if (!parser) {
    LOG_ERR("XTC", "OOM: XtcParser");
    return false;
  }

  // Open XTC file
  xtc::XtcError err = parser->open(filepath.c_str());
  if (err != xtc::XtcError::OK) {
    LOG_ERR("XTC", "Failed to load: %s", xtc::errorToString(err));
    parser.reset();
    return false;
  }

  loaded = true;
  LOG_DBG("XTC", "Loaded XTC: %s (%lu pages)", filepath.c_str(), parser->getPageCount());
  return true;
}

bool Xtc::clearCache() const {
  if (!Storage.exists(cachePath.c_str())) {
    LOG_DBG("XTC", "Cache does not exist, no action needed");
    return true;
  }

  if (!Storage.removeDir(cachePath.c_str())) {
    LOG_ERR("XTC", "Failed to clear cache");
    return false;
  }

  LOG_DBG("XTC", "Cache cleared successfully");
  return true;
}

void Xtc::setupCacheDir() const {
  if (Storage.exists(cachePath.c_str())) {
    return;
  }

  // Create directories recursively
  for (size_t i = 1; i < cachePath.length(); i++) {
    if (cachePath[i] == '/') {
      Storage.mkdir(cachePath.substr(0, i).c_str());
    }
  }
  Storage.mkdir(cachePath.c_str());
}

std::string Xtc::getTitle() const {
  if (!loaded || !parser) {
    return "";
  }

  // Try to get title from XTC metadata first
  std::string title = parser->getTitle();
  if (!title.empty()) {
    return title;
  }

  // Fallback: extract filename from path as title
  size_t lastSlash = filepath.find_last_of('/');
  size_t lastDot = filepath.find_last_of('.');

  if (lastSlash == std::string::npos) {
    lastSlash = 0;
  } else {
    lastSlash++;
  }

  if (lastDot == std::string::npos || lastDot <= lastSlash) {
    return filepath.substr(lastSlash);
  }

  return filepath.substr(lastSlash, lastDot - lastSlash);
}

std::string Xtc::getAuthor() const {
  if (!loaded || !parser) {
    return "";
  }

  // Try to get author from XTC metadata
  return parser->getAuthor();
}

bool Xtc::hasChapters() const {
  if (!loaded || !parser) {
    return false;
  }
  return parser->hasChapters();
}

const std::vector<xtc::ChapterInfo>& Xtc::getChapters() {
  static const std::vector<xtc::ChapterInfo> kEmpty;
  if (!loaded || !parser) {
    return kEmpty;
  }
  return parser->getChapters();
}

std::string Xtc::getCoverBmpPath() const { return cachePath + (rotateCover ? "/cover_ccw.bmp" : "/cover.bmp"); }

bool Xtc::generateCoverBmp() const {
  // Already generated
  if (Storage.exists(getCoverBmpPath().c_str())) {
    return true;
  }

  if (!loaded || !parser) {
    LOG_ERR("XTC", "Cannot generate cover BMP, file not loaded");
    return false;
  }

  if (parser->getPageCount() == 0) {
    LOG_ERR("XTC", "No pages in XTC file");
    return false;
  }

  // Setup cache directory
  setupCacheDir();

  // Get first page info for cover
  xtc::PageInfo pageInfo;
  if (!parser->getPageInfo(0, pageInfo)) {
    LOG_DBG("XTC", "Failed to get first page info");
    return false;
  }

  // Get bit depth
  const uint8_t bitDepth = parser->getBitDepth();

  // Allocate buffer for page data
  // XTG (1-bit): Row-major, ((width+7)/8) * height bytes
  // XTH (2-bit): Two column-major bit planes; each column occupies ceil(height/8) bytes
  size_t bitmapSize;
  if (bitDepth == 2) {
    bitmapSize = static_cast<size_t>(pageInfo.width) * ((static_cast<size_t>(pageInfo.height) + 7) / 8) * 2;
  } else {
    bitmapSize = ((pageInfo.width + 7) / 8) * pageInfo.height;
  }
  const unsigned width = rotateCover ? pageInfo.height : pageInfo.width;
  const unsigned height = rotateCover ? pageInfo.width : pageInfo.height;
  const uint32_t rowSize = ((width * bitDepth + 31) / 32) * 4;
  auto scratch = makeUniqueNoThrow<uint8_t[]>(bitmapSize + rowSize);
  if (!scratch) {
    LOG_ERR("XTC", "OOM: cover bitmap and row");
    return false;
  }
  auto* pixels = scratch.get();
  auto* row = pixels + bitmapSize;
  if (parser->loadPage(0, pixels, bitmapSize) != bitmapSize) {
    LOG_ERR("XTC", "Incomplete cover page");
    return false;
  }
  HalFile output;
  if (!Storage.openFileForWrite("XTC", getCoverBmpPath(), output)) return false;
  bool ok = true;
  if (bitDepth == 2) {
    uint8_t header[70] = {'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 70, 0, 0, 0, 40,
                          0,   0,   0, 0, 0, 0, 0, 0, 0, 0, 0,  1, 0, 2, 0};
    const uint32_t size = 70 + rowSize * height, image = rowSize * height;
    const int32_t w = width, h = -int32_t(height);
    std::memcpy(header + 2, &size, 4);
    std::memcpy(header + 18, &w, 4);
    std::memcpy(header + 22, &h, 4);
    std::memcpy(header + 34, &image, 4);
    header[46] = header[50] = 4;
    for (unsigned n = 0; n < 4; ++n) header[54 + n * 4] = header[55 + n * 4] = header[56 + n * 4] = n * 85;
    ok = output.write(header, sizeof(header)) == sizeof(header);
  } else {
    BmpHeader header;
    createBmpHeader(&header, width, height, BmpRowOrder::TopDown);
    ok = output.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) == sizeof(header);
  }
  uint8_t rows = 0;
  for (unsigned y = 0; ok && y < height; ++y) {
    std::memset(row, 0, rowSize);
    for (unsigned x = 0; x < width; ++x) {
      const auto gray = xtc::coverGray(pixels, pageInfo.width, pageInfo.height, bitDepth, x, y, rotateCover);
      if (bitDepth == 2)
        row[x / 4] |= (gray / 85) << (6 - 2 * (x % 4));
      else if (gray)
        row[x / 8] |= 0x80 >> (x % 8);
    }
    ok = output.write(row, rowSize) == rowSize;
    yieldDuringThumbnail(rows);
  }
  output.close();
  if (!ok) {
    Storage.remove(getCoverBmpPath().c_str());
    LOG_ERR("XTC", "Could not save cover");
    return false;
  }
  LOG_DBG("XTC", "Generated cover BMP: %s", getCoverBmpPath().c_str());
  return true;
}

std::string Xtc::getThumbBmpPath() const {
  return cachePath + (rotateCover ? "/thumb_ccw_[HEIGHT].bmp" : "/thumb_[HEIGHT].bmp");
}
std::string Xtc::getThumbBmpPath(int height) const {
  return cachePath + (rotateCover ? "/thumb_ccw_" : "/thumb_") + std::to_string(height) + ".bmp";
}

bool Xtc::generateThumbBmp(int height) const {
  // Already generated
  if (Storage.exists(getThumbBmpPath(height).c_str())) {
    return true;
  }

  if (!loaded || !parser) {
    LOG_ERR("XTC", "Cannot generate thumb BMP, file not loaded");
    return false;
  }

  if (parser->getPageCount() == 0) {
    LOG_ERR("XTC", "No pages in XTC file");
    return false;
  }

  // Setup cache directory
  setupCacheDir();

  // Get first page info for cover
  xtc::PageInfo pageInfo;
  if (!parser->getPageInfo(0, pageInfo)) {
    LOG_DBG("XTC", "Failed to get first page info");
    return false;
  }

  // Get bit depth
  const uint8_t bitDepth = parser->getBitDepth();

  const unsigned coverWidth = rotateCover ? pageInfo.height : pageInfo.width;
  const unsigned coverHeight = rotateCover ? pageInfo.width : pageInfo.height;
  // Calculate target dimensions for thumbnail (fit within 240x400 Continue Reading card)
  int THUMB_TARGET_WIDTH = height * 0.6;
  int THUMB_TARGET_HEIGHT = height;

  // Calculate scale factor
  float scaleX = static_cast<float>(THUMB_TARGET_WIDTH) / coverWidth;
  float scaleY = static_cast<float>(THUMB_TARGET_HEIGHT) / coverHeight;
  float scale = (scaleX > scaleY) ? scaleX : scaleY;  // for cropping

  // Only scale down, never up
  if (scale >= 1.0f) {
    // Page is already small enough, just use cover.bmp
    // Copy cover.bmp to thumb.bmp
    if (generateCoverBmp()) {
      HalFile src, dst;
      if (Storage.openFileForRead("XTC", getCoverBmpPath(), src)) {
        if (Storage.openFileForWrite("XTC", getThumbBmpPath(height), dst)) {
          uint8_t buffer[192];
          while (src.available()) {
            size_t bytesRead = src.read(buffer, sizeof(buffer));
            dst.write(buffer, bytesRead);
          }
        }
      }
      LOG_DBG("XTC", "Copied cover to thumb (no scaling needed)");
      return Storage.exists(getThumbBmpPath(height).c_str());
    }
    return false;
  }

  uint16_t thumbWidth = static_cast<uint16_t>(coverWidth * scale);
  uint16_t thumbHeight = static_cast<uint16_t>(coverHeight * scale);

  LOG_DBG("XTC", "Generating thumb BMP: %dx%d -> %dx%d (scale: %.3f)", pageInfo.width, pageInfo.height, thumbWidth,
          thumbHeight, scale);

  // Allocate buffer for page data
  size_t bitmapSize;
  if (bitDepth == 2) {
    bitmapSize = static_cast<size_t>(pageInfo.width) * ((static_cast<size_t>(pageInfo.height) + 7) / 8) * 2;
  } else {
    bitmapSize = ((pageInfo.width + 7) / 8) * pageInfo.height;
  }
  const uint32_t rowSize = (thumbWidth + 31) / 32 * 4;
  auto thumbScratch = makeUniqueNoThrow<uint8_t[]>(bitmapSize + rowSize);
  if (!thumbScratch) {
    LOG_ERR("XTC", "Failed to allocate thumb buffers (%lu bytes)", static_cast<unsigned long>(bitmapSize + rowSize));
    return false;
  }
  uint8_t* pageBuffer = thumbScratch.get();
  uint8_t* rowBuffer = thumbScratch.get() + bitmapSize;

  // Load first page (cover)
  size_t bytesRead = const_cast<xtc::XtcParser*>(parser.get())->loadPage(0, pageBuffer, bitmapSize);
  if (bytesRead == 0) {
    LOG_ERR("XTC", "Failed to load cover page for thumb");
    return false;
  }

  // Create thumbnail BMP file - use 1-bit format for fast home screen rendering (no gray passes)
  HalFile thumbBmp;
  if (!Storage.openFileForWrite("XTC", getThumbBmpPath(height), thumbBmp)) {
    LOG_DBG("XTC", "Failed to create thumb BMP file");
    return false;
  }

  // Write 1-bit BMP header (top-down row order)
  BmpHeader bmpHeader;
  createBmpHeader(&bmpHeader, thumbWidth, thumbHeight, BmpRowOrder::TopDown);
  thumbBmp.write(reinterpret_cast<const uint8_t*>(&bmpHeader), sizeof(bmpHeader));

  // Fixed-point scale factor (16.16)
  uint32_t scaleInv_fp = static_cast<uint32_t>(65536.0f / scale);

  uint8_t rowsSinceYield = 0;

  for (uint16_t dstY = 0; dstY < thumbHeight; dstY++) {
    memset(rowBuffer, 0xFF, rowSize);  // Start with all white (bit 1)

    // Calculate source Y range with bounds checking
    uint32_t srcYStart = (static_cast<uint32_t>(dstY) * scaleInv_fp) >> 16;
    uint32_t srcYEnd = (static_cast<uint32_t>(dstY + 1) * scaleInv_fp) >> 16;
    if (srcYStart >= coverHeight) srcYStart = coverHeight - 1;
    if (srcYEnd > coverHeight) srcYEnd = coverHeight;
    if (srcYEnd <= srcYStart) srcYEnd = srcYStart + 1;
    if (srcYEnd > coverHeight) srcYEnd = coverHeight;

    for (uint16_t dstX = 0; dstX < thumbWidth; dstX++) {
      // Calculate source X range with bounds checking
      uint32_t srcXStart = (static_cast<uint32_t>(dstX) * scaleInv_fp) >> 16;
      uint32_t srcXEnd = (static_cast<uint32_t>(dstX + 1) * scaleInv_fp) >> 16;
      if (srcXStart >= coverWidth) srcXStart = coverWidth - 1;
      if (srcXEnd > coverWidth) srcXEnd = coverWidth;
      if (srcXEnd <= srcXStart) srcXEnd = srcXStart + 1;
      if (srcXEnd > coverWidth) srcXEnd = coverWidth;

      // Area averaging: sum grayscale values (0-255 range)
      uint32_t graySum = 0;
      uint32_t totalCount = 0;

      for (uint32_t srcY = srcYStart; srcY < srcYEnd && srcY < coverHeight; srcY++) {
        for (uint32_t srcX = srcXStart; srcX < srcXEnd && srcX < coverWidth; srcX++) {
          const uint8_t grayValue =
              xtc::coverGray(pageBuffer, pageInfo.width, pageInfo.height, bitDepth, srcX, srcY, rotateCover);

          graySum += grayValue;
          totalCount++;
        }
      }

      // Calculate average grayscale and quantize to 1-bit with noise dithering
      uint8_t avgGray = (totalCount > 0) ? static_cast<uint8_t>(graySum / totalCount) : 255;

      // Hash-based noise dithering for 1-bit output
      uint32_t hash = static_cast<uint32_t>(dstX) * 374761393u + static_cast<uint32_t>(dstY) * 668265263u;
      hash = (hash ^ (hash >> 13)) * 1274126177u;
      const int threshold = static_cast<int>(hash >> 24);           // 0-255
      const int adjustedThreshold = 128 + ((threshold - 128) / 2);  // Range: 64-192

      // Quantize to 1-bit: 0=black, 1=white
      uint8_t oneBit = (avgGray >= adjustedThreshold) ? 1 : 0;

      // Pack 1-bit value into row buffer (MSB first, 8 pixels per byte)
      const size_t byteIndex = dstX / 8;
      const size_t bitOffset = 7 - (dstX % 8);
      // Bounds check for row buffer access
      if (byteIndex < rowSize) {
        if (oneBit) {
          rowBuffer[byteIndex] |= (1 << bitOffset);  // Set bit for white
        } else {
          rowBuffer[byteIndex] &= ~(1 << bitOffset);  // Clear bit for black
        }
      }
    }

    // Write row (already padded to 4-byte boundary by rowSize)
    thumbBmp.write(rowBuffer, rowSize);
    yieldDuringThumbnail(rowsSinceYield);
  }
  LOG_DBG("XTC", "Generated thumb BMP (%dx%d): %s", thumbWidth, thumbHeight, getThumbBmpPath(height).c_str());
  return true;
}

uint32_t Xtc::getPageCount() const {
  if (!loaded || !parser) {
    return 0;
  }
  return parser->getPageCount();
}

uint16_t Xtc::getPageWidth() const {
  if (!loaded || !parser) {
    return 0;
  }
  return parser->getWidth();
}

uint16_t Xtc::getPageHeight() const {
  if (!loaded || !parser) {
    return 0;
  }
  return parser->getHeight();
}

uint8_t Xtc::getBitDepth() const {
  if (!loaded || !parser) {
    return 1;  // Default to 1-bit
  }
  return parser->getBitDepth();
}

size_t Xtc::loadPage(uint32_t pageIndex, uint8_t* buffer, size_t bufferSize) const {
  if (!loaded || !parser) {
    return 0;
  }
  return const_cast<xtc::XtcParser*>(parser.get())->loadPage(pageIndex, buffer, bufferSize);
}

xtc::XtcError Xtc::loadPageStreaming(uint32_t pageIndex,
                                     std::function<void(const uint8_t* data, size_t size, size_t offset)> callback,
                                     size_t chunkSize) const {
  if (!loaded || !parser) {
    return xtc::XtcError::FILE_NOT_FOUND;
  }
  return const_cast<xtc::XtcParser*>(parser.get())->loadPageStreaming(pageIndex, callback, chunkSize);
}

uint8_t Xtc::calculateProgress(uint32_t currentPage) const {
  if (!loaded || !parser || parser->getPageCount() == 0) {
    return 0;
  }
  return static_cast<uint8_t>((currentPage + 1) * 100 / parser->getPageCount());
}

xtc::XtcError Xtc::getLastError() const {
  if (!parser) {
    return xtc::XtcError::FILE_NOT_FOUND;
  }
  return parser->getLastError();
}
