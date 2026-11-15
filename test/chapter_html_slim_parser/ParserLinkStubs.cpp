#include <Epub.h>
#include <Epub/Page.h>
#include <Epub/TokenBoundary.h>
#include <Epub/blocks/ImageBlock.h>
#include <Epub/blocks/TextBlock.h>
#include <Epub/converters/ImageDecoderFactory.h>
#include <Epub/converters/ImageToFramebufferDecoder.h>
#include <Epub/hyphenation/Hyphenator.h>
#include <Epub/parsers/ChapterEventCache.h>
#include <GfxRenderer.h>

const char* lookupHtmlEntity(const char*, size_t) { return nullptr; }

#include <BidiUtils.h>

#ifndef READER_REAL_HYPHENATOR
bool isExplicitHyphen(uint32_t) { return false; }
bool isSoftHyphen(uint32_t) { return false; }

std::vector<Hyphenator::BreakInfo> Hyphenator::breakOffsets(const std::string&, bool) { return {}; }
uintptr_t Hyphenator::cacheIdentity() { return 0; }

#endif

ImageBlock::ImageBlock(const std::string& imagePath, const std::string& srcPath, int16_t width, int16_t height)
    : imagePath(imagePath), srcPath(srcPath), width(width), height(height) {}

bool ImageDecoderFactory::isFormatSupported(const std::string& path) {
  return Epub::enableGifProbeForTest && path.ends_with(".gif");
}
ImageToFramebufferDecoder* ImageDecoderFactory::getDecoder(const std::string&) { return nullptr; }

void ImageBlock::render(GfxRenderer&, int, int) {}
void ImageBlock::renderPlaceholder(GfxRenderer&, int, int) const {}
bool ImageBlock::needsDecode() const { return false; }
bool ImageBlock::serialize(Print&) { return false; }
std::unique_ptr<ImageBlock> ImageBlock::deserialize(HalFile&, PageReadBudget*) { return nullptr; }
std::unique_ptr<ImageBlock> ImageBlock::deserialize(serialization::ReadBuffer&, PageReadBudget*) { return nullptr; }

#ifndef TEST_REAL_CHAPTER_EVENT_CACHE
ChapterEventCache::~ChapterEventCache() = default;
bool ChapterEventCache::begin(const std::string&, const std::string&, bool) { return false; }
ChapterEventCache::Next ChapterEventCache::next(Record&) { return Next::End; }
void ChapterEventCache::startElement(const char*, const char**) {}
void ChapterEventCache::endElement(const char*) {}
void ChapterEventCache::text(Kind, const char*, int) {}
void ChapterEventCache::commit() {}
void ChapterEventCache::abort() {}
#endif
