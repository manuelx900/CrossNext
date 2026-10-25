#include "FlashJpegDetail.h"

#include <HalMemory.h>
#include <Logging.h>

#include <algorithm>
#include <atomic>
#include <cstring>

namespace {
constexpr size_t kDecodeBudget = 6 * 1024 * 1024;
// stb owns each payload until STBI_FREE; the prefix accounts for its PSRAM cost.
struct BlockHeader {
  size_t bytes;
};
std::atomic_flag decoderBusy = ATOMIC_FLAG_INIT;
size_t liveBytes = 0;
void* imageAllocate(size_t bytes) {
  if (bytes > kDecodeBudget - sizeof(BlockHeader) || liveBytes > kDecodeBudget - bytes - sizeof(BlockHeader))
    return nullptr;
  auto memory = HalMemory::allocatePsram(bytes + sizeof(BlockHeader));
  if (!memory) return nullptr;
  auto* block = reinterpret_cast<BlockHeader*>(memory.release());
  block->bytes = bytes;
  liveBytes += bytes + sizeof(BlockHeader);
  return block + 1;
}
void imageFree(void* pointer) {
  if (!pointer) return;
  auto* block = static_cast<BlockHeader*>(pointer) - 1;
  liveBytes -= block->bytes + sizeof(BlockHeader);
  HalMemory::PsramDeleter{}(reinterpret_cast<uint8_t*>(block));
}
void* imageReallocate(void* pointer, size_t oldBytes, size_t newBytes) {
  void* next = imageAllocate(newBytes);
  if (!next) return nullptr;
  if (pointer) std::memcpy(next, pointer, std::min(oldBytes, newBytes));
  imageFree(pointer);
  return next;
}
}  // namespace

#define STB_IMAGE_STATIC
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_SIMD
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_THREAD_LOCALS
#define STBI_MAX_DIMENSIONS 3072
#define STBI_ASSERT(value) ((void)0)
#define STBI_MALLOC(bytes) imageAllocate(bytes)
#define STBI_REALLOC_SIZED(pointer, oldBytes, newBytes) imageReallocate(pointer, oldBytes, newBytes)
#define STBI_FREE(pointer) imageFree(pointer)
#define STB_IMAGE_IMPLEMENTATION
#include "../../../third_party/stb/stb_image.h"

namespace flashcards {
namespace {
bool progressive(HalFile& file) {
  uint8_t bytes[2];
  // stb tolerates truncated progressive scans; they cannot provide full detail.
  if (file.fileSize64() < 4 || !file.seek64(file.fileSize64() - 2) || file.read(bytes, 2) != 2 || bytes[0] != 0xff ||
      bytes[1] != 0xd9)
    return false;
  if (!file.seekSet(0) || file.read(bytes, 2) != 2 || bytes[0] != 0xff || bytes[1] != 0xd8) return false;
  for (int segment = 0; segment < 256; ++segment) {
    if (file.read(bytes, 2) != 2 || bytes[0] != 0xff) return false;
    while (bytes[1] == 0xff)
      if (file.read(bytes + 1, 1) != 1) return false;
    if (bytes[1] == 0xc2) return true;
    if (bytes[1] == 0xc0 || bytes[1] == 0xda || bytes[1] == 0xd9) return false;
    if (bytes[1] == 0x01 || (bytes[1] >= 0xd0 && bytes[1] <= 0xd7)) continue;
    if (file.read(bytes, 2) != 2) return false;
    const unsigned length = unsigned(bytes[0]) * 256 + bytes[1];
    if (length < 2 || !file.seekCur(length - 2)) return false;
  }
  return false;
}
struct Input {
  HalFile& file;
  unsigned bytes = 0;
  bool failed = false;
};
int read(void* user, char* out, int bytes) {
  auto& in = *static_cast<Input*>(user);
  if (in.failed) return 0;
  const int count = in.file.read(out, bytes);
  if (count > 0) in.bytes += count;
  if (in.bytes >= 8192) {
    in.bytes = 0;
    delay(1);
  }
  return std::max(0, count);
}
void skip(void* user, int bytes) {
  auto& in = *static_cast<Input*>(user);
  const int64_t next = int64_t(in.file.position()) + bytes;
  if (next < 0 || uint64_t(next) > in.file.fileSize64() || !in.file.seek64(uint64_t(next))) in.failed = true;
}
int eof(void* user) {
  const auto& in = *static_cast<Input*>(user);
  return in.failed || !in.file.available();
}
}  // namespace

bool progressiveJpegDetail(HalFile& source, HalFile& destination, study::ImageRef& ref) {
  if (!progressive(source) || decoderBusy.test_and_set()) return false;
  struct Release {
    ~Release() { decoderBusy.clear(); }
  } release;
  Input input{source};
  const stbi_io_callbacks callbacks{read, skip, eof};
  int width = 0, height = 0, components = 0;
  if (!source.seekSet(0)) return false;
  if (!stbi_info_from_callbacks(&callbacks, &input, &width, &height, &components) || width <= 0 || height <= 0 ||
      width > study::kMaxZoomWidth || height > study::kMaxZoomHeight)
    return false;
  if (!source.seekSet(0)) return false;
  // Full progressive coefficients need random access. All decoder-owned blocks
  // use PSRAM only, with an aggregate 6 MiB cap; normal cards never enter here.
  auto* pixels = stbi_load_from_callbacks(&callbacks, &input, &width, &height, &components, 1);
  if (!pixels || input.failed) {
    if (pixels) stbi_image_free(pixels);
    LOG_ERR("FLASH", "Progressive JPEG detail unavailable: %s", stbi_failure_reason());
    return false;
  }
  struct Pixels {
    uint8_t* data;
    ~Pixels() { stbi_image_free(data); }
  } owner{pixels};
  if (destination.position() > 512u * 1024 * 1024 - unsigned(width * height)) return false;
  ref = {uint32_t(destination.position()), uint16_t(width), uint16_t(height), uint16_t(width), 8};
  uint32_t nearBlackWhite = 0;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) nearBlackWhite += pixels[y * width + x] < 32 || pixels[y * width + x] > 223;
    if (destination.write(pixels + y * width, width) != size_t(width)) return false;
    if (!(y % 16)) delay(1);
  }
  ref.highContrast = uint64_t(nearBlackWhite) * 100 >= uint64_t(width) * height * 95;
  return true;
}
}  // namespace flashcards
