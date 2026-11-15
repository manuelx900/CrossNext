#include "CoverBitmapPainter.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalMemory.h>
#include <Logging.h>

#include <algorithm>
#include <optional>

#include "CoverBoxSampler.h"
#include "CoverFit.h"
#include "CoverScanline.h"

namespace homecover {
bool paintBitmap(const GfxRenderer& renderer, const Bitmap& bitmap, freeink::ui::Rect slot, bool fill, int offset,
                 bool (*continueFn)(void*), void* context, bool preservePolarity) {
  const int width = bitmap.getWidth(), height = bitmap.getHeight();
  if (slot.empty() || width <= 0 || height <= 0 || width > 4096 || height > 8192) return false;
  const int bound = std::max(renderer.getScreenWidth(), renderer.getScreenHeight());
  if (slot.width > bound || slot.height > bound) return false;
  const auto art = fill ? slot : contain(slot, width, height);
  if (art.empty()) return false;
  int cropWidth = width, cropHeight = height;
  if (fill) {
    if (int64_t(width) * art.height > int64_t(height) * art.width)
      cropWidth = std::max(1, int(int64_t(height) * art.width / art.height));
    else
      cropHeight = std::max(1, int(int64_t(width) * art.height / art.width));
  }
  const int sourceX = std::clamp((width - cropWidth) / 2 - offset, 0, width - cropWidth);
  const int sourceY = (height - cropHeight) / 2;
  const bool packed = bitmap.is1Bit();
  const bool filtered = packed && cropWidth >= art.width && cropHeight >= art.height &&
                        (cropWidth != art.width || cropHeight != art.height);
  const size_t columnsBytes = filtered ? art.width * sizeof(BoxColumn) : 0;
  const size_t bandBytes = filtered ? sizeof(BoxBand) : 0;
  const size_t prefixBytes = filtered ? ((width + 7) / 8) * sizeof(uint16_t) : 0;
  const size_t decodedBytes = packed ? 0 : (width + 3) / 4;
  const size_t decodeOffset = columnsBytes + bandBytes + prefixBytes;
  const size_t rowOffset = decodeOffset + decodedBytes;
  constexpr size_t READ_AHEAD = 4096;
  const size_t required = rowOffset + bitmap.getRowBytes() + READ_AHEAD;
  // One bounded row workspace is shared by serialized Home/Library renders.
  // PSRAM avoids an internal-heap/stack row allocation per cover or activity.
  static HalMemory::PsramBuffer scratch;
  static size_t capacity = 0;
  if (required > capacity) {
    auto memory = HalMemory::allocatePsram(required);
    if (!memory) {
      LOG_ERR("COVER", "PSRAM row workspace unavailable (%u bytes)", unsigned(required));
      return false;
    }
    scratch = std::move(memory);
    capacity = required;
  }
  auto* decoded = scratch.get() + decodeOffset;
  auto* row = scratch.get() + rowOffset;
  bitmap.setReadAheadBuffer(row + bitmap.getRowBytes(), READ_AHEAD);
  std::optional<BoxSampler> sampler;
  if (filtered)
    sampler.emplace(reinterpret_cast<BoxColumn*>(scratch.get()),
                    reinterpret_cast<BoxBand*>(scratch.get() + columnsBytes),
                    reinterpret_cast<uint16_t*>(scratch.get() + columnsBytes + bandBytes), width, sourceX, cropWidth,
                    cropHeight, art, 0);
  renderer.fillRect(slot.x, slot.y, slot.width, slot.height, false);
  bool ok = true;
  const auto draw = [&renderer](int x, int y, int w, int h) { renderer.fillRect(x, y, w, h, true); };
  for (int n = 0; n < height; ++n) {
    if (continueFn && !continueFn(context)) {
      ok = false;
      break;
    }
    const auto error = packed ? bitmap.readNext1BitRow(row) : bitmap.readNextRow(decoded, row);
    if (error != BmpReaderError::Ok) {
      ok = false;
      break;
    }
    const int y = (bitmap.isTopDown() ? n : height - 1 - n) - sourceY;
    if (y < 0 || y >= cropHeight) continue;
    if (sampler)
      sampler->paint(row, y, bitmap.isTopDown(), draw);
    else if (packed)
      paint1BitRow(row, sourceX, cropWidth, cropHeight, y, art, 0, draw);
    else
      paintRow(decoded, sourceX, cropWidth, cropHeight, y, art, 0, draw);
  }
  bitmap.setReadAheadBuffer(nullptr, 0);
  if (preservePolarity) renderer.preserveImagePolarity(art.x, art.y, art.width, art.height);
  return ok;
}
}  // namespace homecover
