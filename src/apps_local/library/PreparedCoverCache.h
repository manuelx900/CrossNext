#pragma once

#include <FreeInkUI.h>
#include <HalMemory.h>
#include <HalStorage.h>

#include <cstddef>
#include <cstdint>

class Bitmap;
class GfxRenderer;
namespace libraryui {
// One optional native raster trails the BMP's declared source bytes. Regenerating
// the thumbnail truncates that derived raster along with the old BMP.
class PreparedCoverCache {
 public:
  static constexpr size_t Capacity = 16 * 1024, Chunk = 4096, FooterBytes = 40;
  static constexpr size_t SourceLimit = 128 * 1024;
  struct Info {
    uint32_t source = 0, bytes = 0, checksum = 0, sourceChecksum = 0;
  };
  struct SourceStamp {
    uint32_t content = 0, media = 0;
    bool operator==(const SourceStamp& other) const { return content == other.content && media == other.media; }
  };
  static SourceStamp sourceStamp();
  static bool sourceValidated(const char* path, uint64_t key, uint64_t fileBytes, const Info& info, SourceStamp stamp);
  static void rememberSource(const char* path, uint64_t key, uint64_t fileBytes, const Info& info, SourceStamp stamp);
  static uint64_t key(const GfxRenderer& renderer, freeink::ui::Rect area);
  static bool canDraw(const GfxRenderer& renderer, freeink::ui::Rect area);
  static uint32_t checksum(const uint8_t* data, size_t bytes);
  static uint32_t updateChecksum(uint32_t value, const uint8_t* data, size_t bytes);
  static uint32_t sourceBytes(HalFile& file);
  static bool inspect(HalFile& file, uint64_t key, size_t bytes, Info& info);
  bool begin();
  bool load(HalFile& file, GfxRenderer& renderer, freeink::ui::Rect area, uint64_t key, bool draw = true,
            const char* path = nullptr, const SourceStamp* beforeOpen = nullptr);
  bool prepare(GfxRenderer& renderer, Bitmap& bitmap, freeink::ui::Rect area, uint64_t key, const char* path,
               bool (*continueFn)(void*) = nullptr, void* context = nullptr);
  // Borrow only immediately after successful prepare(), before load/prepare/release.
  const uint8_t* preparedPixels() const { return arena.get(); }
  bool writing() const { return writer.isOpen(); }
  bool enabled() const { return !attempted || arena != nullptr; }
  void step();
  void cancel();
  void release();

 private:
  bool store(const char* path, uint64_t key, size_t bytes, uint32_t sourceHash, size_t sourceBytes);
  HalMemory::PsramBuffer arena;
  HalFile writer;
  uint8_t footer[FooterBytes]{};
  size_t written = 0, length = 0;
  uint32_t sourceLength = 0, sourceRead = 0, sourceHash = 2166136261u;
  uint32_t expectedSourceHash = 0;
  enum class Phase : uint8_t { Source, Pixels, Footer };
  Phase phase = Phase::Source;
  bool attempted = false;
};
}  // namespace libraryui
