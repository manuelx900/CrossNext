#pragma once

#include <HalStorage.h>
#include <Logging.h>
#include <stdint.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>

// Stream exact 2-bit pixels through a bounded reusable row band. Decoder blocks
// arrive in raster order, so rows below the next block are final. Uncovered
// clipped/gap rows retain their zero fill.
struct PixelCache {
  uint8_t* buffer;  // bandRows packed rows
  int width;
  int height;
  int bytesPerRow;
  int originX;       // config.x - to convert screen coords to cache coords
  int originY;       // config.y
  int maxBlockRows;  // maximum rows written by one decoder callback
  int bandRows;      // rows held in the band buffer
  int bandStart;     // image-local row index of band buffer row 0
  HalFile file;
  std::string cachePathStr;
  bool ok;

  PixelCache()
      : buffer(nullptr),
        width(0),
        height(0),
        bytesPerRow(0),
        originX(0),
        originY(0),
        maxBlockRows(0),
        bandRows(0),
        bandStart(0),
        ok(false) {}
  PixelCache(const PixelCache&) = delete;
  PixelCache& operator=(const PixelCache&) = delete;

  static constexpr int MIN_BAND_ROWS = 16;
  static constexpr size_t MAX_BAND_BYTES = 24 * 1024;  // band working-set ceiling

  // Open the cache file, write the header, and allocate a band buffer big enough
  // to hold the tallest single decode block (maxBlockDstRows output rows).
  bool begin(const std::string& cachePath, int w, int h, int ox, int oy, int maxBlockDstRows) {
    if (w <= 0 || h <= 0 || w > UINT16_MAX || h > UINT16_MAX || maxBlockDstRows <= 0) {
      LOG_ERR("IMG", "Invalid pixel cache dimensions");
      return false;
    }
    maxBlockRows = maxBlockDstRows;
    width = w;
    height = h;
    originX = ox;
    originY = oy;
    bytesPerRow = (w + 3) / 4;  // 2 bits per pixel, 4 pixels per byte
    bandStart = 0;
    ok = false;

    int wantRows = maxBlockDstRows + 2;
    if (wantRows < MIN_BAND_ROWS) wantRows = MIN_BAND_ROWS;
    if (wantRows > h) wantRows = h;

    size_t maxRowsByMem = MAX_BAND_BYTES / (size_t)bytesPerRow;
    if (maxRowsByMem < 1) maxRowsByMem = 1;
    if ((size_t)wantRows > maxRowsByMem) wantRows = (int)maxRowsByMem;

    // A single decode block must fit inside the band, otherwise streaming would
    // drop rows. This only fails for pathological upscales that could not be
    // cached at all; fall back to the no-cache path.
    if (wantRows < maxBlockDstRows) {
      LOG_ERR("IMG", "Cache band too small (%d < %d rows) for %dx%d", wantRows, maxBlockDstRows, w, h);
      return false;
    }
    bandRows = wantRows;

    const size_t bufSize = size_t(bandRows) * bytesPerRow;
    buffer = (uint8_t*)malloc(bufSize);
    if (!buffer) {
      LOG_ERR("IMG", "OOM cache band: %u bytes", (unsigned)bufSize);
      return false;
    }
    memset(buffer, 0, bufSize);

    if (!Storage.openFileForWrite("IMG", cachePath, file)) {
      LOG_ERR("IMG", "Failed to open cache file for writing: %s", cachePath.c_str());
      free(buffer);
      buffer = nullptr;
      return false;
    }
    cachePathStr = cachePath;

    uint16_t w16 = (uint16_t)w;
    uint16_t h16 = (uint16_t)h;
    if (file.write(&w16, 2) != 2 || file.write(&h16, 2) != 2) {
      LOG_ERR("IMG", "Failed to write cache header: %s", cachePath.c_str());
      abort();
      return false;
    }

    LOG_DBG("IMG", "Cache stream started: %s (%dx%d, band %d rows)", cachePath.c_str(), w, h, bandRows);
    ok = true;
    return true;
  }

  // Keep complete rows until the next decoder block would exceed the band.
  // Coordinates remain relative to bandStart, including retained scanlines.
  bool advanceTo(int newTopRow) {
    if (!ok) return false;
    if (newTopRow <= bandStart) return true;
    if (newTopRow > height) newTopRow = height;
    const int neededRows = std::min(maxBlockRows, height - newTopRow);
    if (newTopRow - bandStart + neededRows <= bandRows) return true;
    return flushTo(newTopRow);
  }

  bool flushTo(int row) {
    const int rows = row - bandStart;
    const int bufferedRows = std::min(rows, bandRows);
    const size_t bytes = size_t(bufferedRows) * bytesPerRow;
    if (bytes && file.write(buffer, bytes) != bytes) {
      LOG_ERR("IMG", "Cache band write error at row %d", bandStart);
      ok = false;
      return false;
    }
    // The same band supplies clipped/gap rows; no second write buffer is needed.
    memset(buffer, 0, size_t(bandRows) * bytesPerRow);
    for (int remaining = rows - bufferedRows; remaining > 0;) {
      const int count = std::min(remaining, bandRows);
      const size_t zeroBytes = size_t(count) * bytesPerRow;
      if (file.write(buffer, zeroBytes) != zeroBytes) {
        LOG_ERR("IMG", "Cache gap write error");
        ok = false;
        return false;
      }
      remaining -= count;
    }
    bandStart = row;
    return true;
  }

  // Flush the final band and zero-fill any rows never covered (image clipped by
  // the screen), then close the file.
  bool finalize() {
    if (!ok) {
      abort();
      return false;
    }
    if (!flushTo(height)) {
      abort();
      return false;
    }
    if (!file.close()) {
      LOG_ERR("IMG", "Failed to close image cache");
      abort();
      return false;
    }
    LOG_DBG("IMG", "Cache written: %s (%dx%d, %d bytes)", cachePathStr.c_str(), width, height,
            4 + bytesPerRow * height);
    ok = false;  // file handed off; nothing left to clean up
    return true;
  }

  // Drop a partial/failed cache so a later decode re-creates it cleanly.
  void abort() {
    if (file.isOpen()) file.close();
    if (!cachePathStr.empty()) {
      Storage.remove(cachePathStr.c_str());
    }
    ok = false;
  }

  ~PixelCache() {
    if (file.isOpen()) {
      // The file is still open, so neither finalize() nor abort() ran, or a
      // mid-stream write failed (advanceTo() cleared ok but left the file open).
      // Drop the partial cache so we leave no corrupt file behind.
      abort();
    }
    if (buffer) {
      free(buffer);
      buffer = nullptr;
    }
  }
};
