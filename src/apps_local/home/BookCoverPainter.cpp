#include "BookCoverPainter.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <functional>
#include <optional>

#include "../../RecentBooksStore.h"
#include "../../components/UITheme.h"
#include "CoverBoxSampler.h"
#include "CoverScanline.h"
#include "CoverSceneCache.h"
namespace fui = freeink::ui;
bool BookCoverPainter::paintBitmap(fui::Rect slot, int taper) {
  const int width = coverBitmap.getWidth(), height = coverBitmap.getHeight();
  if (width <= 0 || height <= 0 || slot.empty()) return false;
  int cropWidth = width, cropHeight = height;
  if (!taper) {
    if (width * slot.height > height * slot.width)
      cropWidth = std::max(1, height * slot.width / slot.height);
    else
      cropHeight = std::max(1, width * slot.height / slot.width);
  }
  const int sourceX = (width - cropWidth) / 2, sourceY = (height - cropHeight) / 2;
  const bool packed = coverBitmap.is1Bit();
  bool filtered = packed && cropWidth >= slot.width && cropHeight >= slot.height &&
                  (taper || cropWidth != slot.width || cropHeight != slot.height);
  const size_t decoded = packed ? 0 : (width + 3) / 4;
  const size_t columnsBytes = filtered ? slot.width * sizeof(homecover::BoxColumn) : 0;
  const size_t bandsBytes =
      filtered ? homecover::BoxSampler::bandCapacity(slot, taper) * sizeof(homecover::BoxBand) : 0;
  size_t filterBytes = filtered ? columnsBytes + bandsBytes + ((width + 7) / 8) * sizeof(uint16_t) : 0;
  constexpr size_t READ_AHEAD_BYTES = 4096;
  const size_t needed = filterBytes + decoded + coverBitmap.getRowBytes() + READ_AHEAD_BYTES;
  // One reusable row allocation: packed sampling scratch replaces 2bpp expansion.
  const auto reserve = [this](size_t bytes) {
    if (bytes <= rowCapacity) return true;
    auto next = makeUniqueNoThrow<uint8_t[]>(bytes);
    if (!next) {
      LOG_ERR("HOME", "OOM: cover row (%u bytes)", unsigned(bytes));
      return false;
    }
    row = std::move(next);
    rowCapacity = bytes;
    return true;
  };
  if (!reserve(needed)) {
    if (!filtered) return false;
    filtered = false;
    filterBytes = 0;
    if (!reserve(decoded + coverBitmap.getRowBytes() + READ_AHEAD_BYTES)) return false;
  }
  auto* source = row.get() + filterBytes + decoded;
  coverBitmap.setReadAheadBuffer(source + coverBitmap.getRowBytes(), READ_AHEAD_BYTES);
  std::optional<homecover::BoxSampler> sampler;
  if (filtered) {
    sampler.emplace(reinterpret_cast<homecover::BoxColumn*>(row.get()),
                    reinterpret_cast<homecover::BoxBand*>(row.get() + columnsBytes),
                    reinterpret_cast<uint16_t*>(row.get() + columnsBytes + bandsBytes), width, sourceX, cropWidth,
                    cropHeight, slot, taper);
  }
  const auto draw = [this](int x, int y, int w, int h) { renderer.fillRect(x, y, w, h, true); };
  renderer.fillRect(slot.x, slot.y, slot.width, slot.height, false);
  for (int line = 0; line < height; ++line) {
    const auto error = packed ? coverBitmap.readNext1BitRow(source) : coverBitmap.readNextRow(row.get(), source);
    if (error != BmpReaderError::Ok) return false;
    const int sourceLine = (coverBitmap.isTopDown() ? line : height - 1 - line) - sourceY;
    if (sourceLine < 0 || sourceLine >= cropHeight) continue;
    if (sampler)
      sampler->paint(source, sourceLine, coverBitmap.isTopDown(), draw);
    else if (packed)
      homecover::paint1BitRow(source, sourceX, cropWidth, cropHeight, sourceLine, slot, taper, draw);
    else
      homecover::paintRow(row.get(), sourceX, cropWidth, cropHeight, sourceLine, slot, taper, draw);
  }
  renderer.preserveImagePolarity(slot.x, slot.y, slot.width, slot.height);
  return true;
}
void BookCoverPainter::paint(const RecentBook& book, fui::Rect slot, int taper) {
  bool drawn = false;
  char thumb[112]{};
  if (FsHelpers::hasEpubExtension(book.path) || FsHelpers::hasXtcExtension(book.path)) {
    std::snprintf(thumb, sizeof(thumb), "/.crosspoint/%s_%lu/thumb_%d.bmp",
                  FsHelpers::hasXtcExtension(book.path) ? "xtc" : "epub",
                  static_cast<unsigned long>(std::hash<std::string>{}(book.path)), slot.height);
  }
  std::string fallback;
  const char* path = thumb;
  if (!*thumb || !Storage.exists(thumb)) {
    fallback = UITheme::getCoverThumbPath(book.coverBmpPath, slot.height);
    path = fallback.c_str();
  }
  if (*path && Storage.openFileForRead("HOME", path, coverFile)) {
    if (coverBitmap.parseHeaders() == BmpReaderError::Ok) drawn = paintBitmap(slot, taper);
    homecover::recordSource(path, coverFile, drawn && (!*thumb || path == thumb));
    coverFile.close();
  } else {
    homecover::recordSource(path, coverFile, false);
  }
  if (!drawn) GUI.drawCoverPlaceholder(renderer, Rect{slot.x, slot.y, slot.width, slot.height});
  if (!taper) {
    renderer.drawRect(slot.x, slot.y, slot.width, slot.height);
    return;
  }
  homecover::finishTaper(
      slot, taper,
      [this](int x, int y, int w, int h) {
        target.fill(fui::makeRect(x, y, w, h), fui::Paint::solid(fui::Color::White));
      },
      [this](int x, int y, int w, int h) {
        target.fill(fui::makeRect(x, y, w, h), fui::Paint::solid(fui::Color::Black));
      });
}
