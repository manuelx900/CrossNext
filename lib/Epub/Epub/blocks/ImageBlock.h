#pragma once
#include <HalStorage.h>
#include <ReadBuffer.h>

#include <memory>
#include <string>

#include "Block.h"

class PageReadBudget;

class ImageBlock final : public Block {
 public:
  ImageBlock(const std::string& imagePath, const std::string& srcPath, int16_t width, int16_t height);
  ~ImageBlock() override = default;

  const std::string& getImagePath() const { return imagePath; }
  int16_t getWidth() const { return width; }
  int16_t getHeight() const { return height; }
  size_t retainedBytes() const { return sizeof(ImageBlock) + imagePath.capacity() + srcPath.capacity() + 98; }

  bool hasValidCache() const;
  bool needsDecode() const;
  void renderPlaceholder(GfxRenderer& renderer, int x, int y) const;
  static void clearRenderFailures();

  // Reuse pixel payloads across BW/gray passes: three bounded PSRAM slots,
  // or one transient chunked slot without PSRAM. Release retained slots on exit.
  static void releaseRenderCache();
  static void beginRenderCache();
  static void endRenderCache();

  // Lazy extraction hook: the section build only header-probes images for their
  // dimensions; the file at imagePath is extracted out of the book on first
  // render, via this callback (function pointer + context, not std::function —
  // this is render-loop code). Registered by the reader activity that owns the
  // Epub, cleared on its exit.
  using ExtractFn = bool (*)(void* ctx, const char* srcPath, const char* destPath);
  static void setExtractor(void* ctx, ExtractFn fn);
  // Prepares the SD pixel cache without changing the framebuffer. Speculative
  // JPEG/PNG work polls the continuation between extraction chunks and decode rows.
  using ContinueFn = bool (*)(void*);
  bool prefetchPixels(GfxRenderer& renderer, ContinueFn continueWork, void* workContext) const;
  bool prepareCache(GfxRenderer& renderer, void* ctx, ExtractFn extract, int x = 0, int y = 0,
                    ContinueFn continueWork = nullptr, void* workContext = nullptr) const;

  BlockType getType() override { return IMAGE_BLOCK; }
  bool isEmpty() override { return false; }

  void render(GfxRenderer& renderer, const int x, const int y);
  bool serialize(Print& file);
  static std::unique_ptr<ImageBlock> deserialize(HalFile& file, PageReadBudget* budget = nullptr);
  static std::unique_ptr<ImageBlock> deserialize(serialization::ReadBuffer& file, PageReadBudget* budget = nullptr);

 private:
  std::string imagePath;
  std::string srcPath;  // book-internal source href; empty once known-extracted
  int16_t width;
  int16_t height;

  static void* extractCtx;
  static ExtractFn extractFn;
  bool decodeAndCache(GfxRenderer& renderer, int x, int y, void* ctx, ExtractFn extract, bool cacheOnly = false,
                      ContinueFn continueWork = nullptr, void* workContext = nullptr) const;
};
