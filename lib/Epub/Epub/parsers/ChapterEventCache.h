#pragma once

#include <HalMemory.h>
#include <HalStorage.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

// Optional, layout-neutral XML callback stream. Pages and resolved pixel geometry
// remain the responsibility of ChapterHtmlSlimParser.
class ChapterEventCache {
 public:
  enum class Kind : uint8_t { End, StartElement, EndElement, Text, DefaultData };
  enum class Next { Record, End, Error };
  struct Record {
    Kind kind = Kind::End;
    const char* data = nullptr;
    const char** attributes = nullptr;
    uint16_t length = 0;
  };

  ~ChapterEventCache();
  bool begin(const std::string& sourcePath, const std::string& cachePath, bool allowReplay);
  bool replaying() const { return mode == Mode::Replay; }
  bool hadReadError() const { return readError; }
  size_t bytesConsumed() const { return consumed; }
  size_t totalBytes() const { return streamSize; }
  Next next(Record& record);
  void startElement(const char* name, const char** attributes);
  void endElement(const char* name);
  void text(Kind kind, const char* data, int length);
  void commit();
  void abort();

 private:
  enum class Mode { Disabled, Write, Replay };
  static constexpr size_t IO_BYTES = 4096;
  static constexpr size_t RECORD_BYTES = 4096;
  static constexpr size_t BUFFER_BYTES = IO_BYTES + RECORD_BYTES;
  static constexpr uint8_t MAX_ATTRIBUTES = 32;
  static constexpr uint16_t MAX_DEPTH = 256;
  static constexpr size_t MAX_CACHE_BYTES = 8 * 1024 * 1024;
  Mode mode = Mode::Disabled;
  HalFile file;
  HalMemory::PsramBuffer buffer;
  std::array<const char*, MAX_ATTRIBUTES * 2 + 1> attributes{};
  std::string path;
  std::string temporaryPath;
  std::string sourcePath;
  uint32_t sourceSize = 0;
  uint32_t sourceModified = 0;
  size_t ioLength = 0;
  size_t ioOffset = 0;
  size_t consumed = 0;
  size_t streamSize = 0;
  uint16_t depth = 0;
  bool ownsWriter = false;
  bool readError = false;

  bool sourceMatches() const;
  bool allocateBuffer();
  bool openReplay();
  void beginWrite();
  bool flushBlock();
  bool readBlock();
  bool writeBytes(const void* data, size_t length);
  bool readBytes(void* data, size_t length);
  bool writeRecord(Kind kind, const void* data, size_t length);
  void writeFailed(const char* reason);
  Next invalidRecord();
};
