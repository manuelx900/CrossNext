#include "ChapterEventCache.h"

#include <Logging.h>
#include <Performance.h>

#include <algorithm>
#include <atomic>
#include <cstring>

namespace {
constexpr uint32_t MAGIC = 0x31435645;  // EVC1
// Table chapters are no longer admitted: column planning requires the raw XML parser.
constexpr uint16_t VERSION = 2;
constexpr size_t HEADER_BYTES = 20;
std::atomic_flag writerLease = ATOMIC_FLAG_INIT;
std::atomic_flag replayLease = ATOMIC_FLAG_INIT;
bool retainReplay = false;
constexpr size_t REPLAY_LIMIT = 256 * 1024;
constexpr size_t REPLAY_HEADROOM = 1024 * 1024;
struct ReplayMemo {
  HalMemory::PsramBuffer bytes;
  char path[256]{};
  size_t size = 0;
  size_t length = 0;
  uint32_t modified = 0;
};
ReplayMemo replayMemo;
void clearReplayMemo() {
  replayMemo.bytes.reset();
  replayMemo.path[0] = '\0';
  replayMemo.size = 0;
  replayMemo.length = 0;
  replayMemo.modified = 0;
}

constexpr auto makeCrcTable() {
  std::array<uint32_t, 256> table{};
  for (uint32_t i = 0; i < table.size(); ++i) {
    uint32_t value = i;
    for (int bit = 0; bit < 8; ++bit) value = (value >> 1) ^ ((value & 1) ? 0xEDB88320U : 0U);
    table[i] = value;
  }
  return table;
}
constexpr auto CRC_TABLE = makeCrcTable();
uint32_t crc(const uint8_t* bytes, const size_t length) {
  uint32_t value = 0xFFFFFFFFU;
  for (size_t i = 0; i < length; ++i) value = CRC_TABLE[(value ^ bytes[i]) & 255] ^ (value >> 8);
  return value ^ 0xFFFFFFFFU;
}
void put16(uint8_t* out, const uint16_t value) {
  out[0] = value;
  out[1] = value >> 8;
}
void put32(uint8_t* out, const uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) out[i] = value >> (i * 8);
}
uint16_t get16(const uint8_t* in) { return uint16_t(in[0]) | (uint16_t(in[1]) << 8); }
uint32_t get32(const uint8_t* in) {
  return uint32_t(in[0]) | (uint32_t(in[1]) << 8) | (uint32_t(in[2]) << 16) | (uint32_t(in[3]) << 24);
}
}  // namespace

ChapterEventCache::~ChapterEventCache() { abort(); }

void ChapterEventCache::retainReaderReplay(bool enabled) {
  retainReplay = enabled;
  clearReplayMemo();
}

void ChapterEventCache::prepareReplay(const uint8_t* header) {
  if (!retainReplay || !sourceModified || streamSize > REPLAY_LIMIT || path.size() >= sizeof(replayMemo.path)) return;
  const auto modified = file.modificationTime();
  if (!modified || replayLease.test_and_set()) return;
  ownsReplay = true;
  replayModified = modified;
  if (HalMemory::getPsramHeap().freeBytes < REPLAY_HEADROOM) clearReplayMemo();
  if (replayMemo.bytes && replayMemo.size == streamSize && replayMemo.modified == modified && path == replayMemo.path &&
      !memcmp(replayMemo.bytes.get(), header, HEADER_BYTES)) {
    performance::count(performance::Counter::EventMemoHit);
    replayBuffer = std::move(replayMemo.bytes);
    replayOffset = HEADER_BYTES;
    replayLength = replayMemo.length;
    LOG_DBG("EVC", "Reused reader replay (%u bytes)", unsigned(streamSize));
    return;
  }
  performance::count(performance::Counter::EventMemoMiss);
  clearReplayMemo();
  if (HalMemory::getPsramHeap().freeBytes < streamSize + REPLAY_HEADROOM) return;
  // Up to 256 KiB, PSRAM only, moved back to the reader memo on parser release.
  // A stack buffer cannot hold the chapter and internal RAM remains available to layout.
  replayBuffer = HalMemory::allocatePsram(streamSize);
  if (!replayBuffer) {
    LOG_ERR("EVC", "No PSRAM replay snapshot (%u bytes); streaming", unsigned(streamSize));
    return;
  }
  memcpy(replayBuffer.get(), header, HEADER_BYTES);
  replayLength = replayOffset = HEADER_BYTES;
}

size_t ChapterEventCache::replayPosition() const { return replayBuffer ? replayOffset : file.position(); }

size_t ChapterEventCache::readReplay(void* data, size_t length) {
  if (!replayBuffer) return file.read(data, length);
  if (replayOffset > streamSize || length > streamSize - replayOffset) return 0;
  const size_t cached = replayOffset < replayLength ? std::min(length, replayLength - replayOffset) : 0;
  if (cached) memcpy(data, replayBuffer.get() + replayOffset, cached);
  replayOffset += cached;
  if (cached == length) return length;
  if (file.position() != replayOffset && !file.seekSet(replayOffset)) return cached;
  const int result = file.read(static_cast<uint8_t*>(data) + cached, length - cached);
  if (result <= 0) return cached;
  const size_t read = size_t(result);
  memcpy(replayBuffer.get() + replayOffset, static_cast<uint8_t*>(data) + cached, read);
  replayOffset += read;
  replayLength = replayOffset;
  return cached + read;
}

bool ChapterEventCache::allocateBuffer() {
  if (buffer) return true;
  // Two 4 KiB workspaces are reused for the parse lifetime. They exceed the
  // stack budget; PSRAM-only allocation leaves internal RAM to layout/drivers.
  buffer = HalMemory::allocatePsram(BUFFER_BYTES);
  if (!buffer) LOG_ERR("EVC", "No PSRAM workspace (%u bytes); using XHTML", unsigned(BUFFER_BYTES));
  return buffer != nullptr;
}

bool ChapterEventCache::sourceMatches() const {
  HalFile source;
  return Storage.openFileForRead("EVC", sourcePath, source) && source.size() == sourceSize &&
         source.modificationTime() == sourceModified;
}

bool ChapterEventCache::begin(const std::string& source, const std::string& cache, const bool allowReplay) {
  abort();
  readError = false;
  consumed = 0;
  streamSize = 0;
  if (cache.empty() || HalMemory::getPsramHeap().totalBytes == 0) return false;
  sourcePath = source;
  path = cache;
  temporaryPath = path + ".part";
  {
    HalFile input;
    if (!Storage.openFileForRead("EVC", sourcePath, input) || input.size() > UINT32_MAX) return false;
    sourceSize = static_cast<uint32_t>(input.size());
    sourceModified = input.modificationTime();
  }
  if (!allocateBuffer()) return false;
  if (allowReplay && openReplay()) return true;
  beginWrite();
  return false;
}

bool ChapterEventCache::openReplay() {
  if (!Storage.exists(path.c_str()) || !Storage.openFileForRead("EVC", path, file)) return false;
  uint8_t header[HEADER_BYTES]{};
  streamSize = file.size();
  const bool valid = streamSize >= HEADER_BYTES + 9 && streamSize <= MAX_CACHE_BYTES &&
                     file.read(header, sizeof(header)) == sizeof(header) && get32(header) == MAGIC &&
                     get16(header + 4) == VERSION && get16(header + 6) == HEADER_BYTES &&
                     get32(header + 8) == sourceSize && get32(header + 12) == sourceModified &&
                     get32(header + 16) == crc(header, 16);
  if (!valid) {
    file.close();
    LOG_DBG("EVC", "Stale or invalid event cache; using XHTML");
    return false;
  }
  mode = Mode::Replay;
  prepareReplay(header);
  if (readError) {
    abort();
    return false;
  }
  depth = 0;
  consumed = HEADER_BYTES;
  ioLength = ioOffset = 0;
  LOG_DBG("EVC", "Replay %s (%u bytes)", path.c_str(), unsigned(streamSize));
  return true;
}

void ChapterEventCache::beginWrite() {
  if (path == replayMemo.path) clearReplayMemo();
  if (writerLease.test_and_set()) {
    buffer.reset();
    return;
  }
  ownsWriter = true;
  const auto slash = path.find_last_of('/');
  const auto directory = path.substr(0, slash);
  if (slash == std::string::npos || (!Storage.exists(directory.c_str()) && !Storage.mkdir(directory.c_str()))) {
    writeFailed("directory");
    return;
  }
  if (Storage.exists(temporaryPath.c_str())) Storage.removeDerivedCache(temporaryPath.c_str());
  if (!Storage.openDerivedCacheForWrite("EVC", temporaryPath.c_str(), file)) {
    writeFailed("open");
    return;
  }
  uint8_t header[HEADER_BYTES]{};
  put32(header, MAGIC);
  put16(header + 4, VERSION);
  put16(header + 6, HEADER_BYTES);
  put32(header + 8, sourceSize);
  put32(header + 12, sourceModified);
  put32(header + 16, crc(header, 16));
  if (file.write(header, sizeof(header)) != sizeof(header)) {
    writeFailed("header");
    return;
  }
  mode = Mode::Write;
  depth = 0;
  consumed = HEADER_BYTES;
  ioLength = ioOffset = 0;
}

bool ChapterEventCache::flushBlock() {
  if (!ioLength) return true;
  uint8_t header[6];
  put16(header, static_cast<uint16_t>(ioLength));
  put32(header + 2, crc(buffer.get(), ioLength));
  if (consumed + sizeof(header) + ioLength > MAX_CACHE_BYTES || file.write(header, sizeof(header)) != sizeof(header) ||
      file.write(buffer.get(), ioLength) != ioLength)
    return false;
  consumed += sizeof(header) + ioLength;
  ioLength = 0;
  return true;
}

bool ChapterEventCache::writeBytes(const void* bytes, size_t length) {
  auto* data = static_cast<const uint8_t*>(bytes);
  while (length) {
    const size_t count = std::min(length, IO_BYTES - ioLength);
    memcpy(buffer.get() + ioLength, data, count);
    ioLength += count;
    data += count;
    length -= count;
    if (ioLength == IO_BYTES && !flushBlock()) return false;
  }
  return true;
}

bool ChapterEventCache::writeRecord(const Kind kind, const void* data, const size_t length) {
  if (mode != Mode::Write) return false;
  if (length > RECORD_BYTES) {
    writeFailed("oversized event");
    return false;
  }
  uint8_t header[3]{static_cast<uint8_t>(kind), 0, 0};
  put16(header + 1, static_cast<uint16_t>(length));
  if (!writeBytes(header, sizeof(header)) || !writeBytes(data, length)) {
    writeFailed("event write");
    return false;
  }
  return true;
}

void ChapterEventCache::startElement(const char* name, const char** atts) {
  if (mode != Mode::Write) return;
  // Both EPUB engines share this stream; modern table planning needs raw XML.
  if (strcmp(name, "table") == 0) {
    abort();
    return;
  }
  size_t count = 0;
  while (atts && atts[count * 2]) {
    if (count == MAX_ATTRIBUTES || !atts[count * 2 + 1]) {
      writeFailed("attribute count");
      return;
    }
    ++count;
  }
  size_t length = 1 + strlen(name) + 1;
  for (size_t i = 0; i < count * 2; ++i) {
    const size_t size = strlen(atts[i]) + 1;
    if (length > RECORD_BYTES || size > RECORD_BYTES - length) {
      writeFailed("element bounds");
      return;
    }
    length += size;
  }
  if (length > RECORD_BYTES || depth == MAX_DEPTH) {
    writeFailed("element bounds");
    return;
  }
  auto* record = buffer.get() + IO_BYTES;
  record[0] = static_cast<uint8_t>(count);
  size_t offset = 1;
  const auto append = [&](const char* text) {
    const size_t size = strlen(text) + 1;
    memcpy(record + offset, text, size);
    offset += size;
  };
  append(name);
  for (size_t i = 0; i < count * 2; ++i) append(atts[i]);
  if (writeRecord(Kind::StartElement, record, length)) ++depth;
}

void ChapterEventCache::endElement(const char* name) {
  if (mode != Mode::Write) return;
  if (!depth) {
    writeFailed("element depth");
    return;
  }
  if (writeRecord(Kind::EndElement, name, strlen(name) + 1)) --depth;
}

void ChapterEventCache::text(const Kind kind, const char* data, const int length) {
  if (mode != Mode::Write) return;
  if (length < 0 || (kind != Kind::Text && kind != Kind::DefaultData)) {
    writeFailed("text bounds");
    return;
  }
  writeRecord(kind, data, static_cast<size_t>(length));
}

bool ChapterEventCache::readBlock() {
  uint8_t header[6];
  if (replayPosition() > streamSize || streamSize - replayPosition() < sizeof(header) ||
      readReplay(header, sizeof(header)) != sizeof(header))
    return false;
  const size_t count = get16(header);
  if (!count || count > IO_BYTES || count > streamSize - replayPosition() || readReplay(buffer.get(), count) != count ||
      crc(buffer.get(), count) != get32(header + 2))
    return false;
  ioLength = count;
  ioOffset = 0;
  consumed += sizeof(header);
  return true;
}

bool ChapterEventCache::readBytes(void* bytes, size_t length) {
  auto* data = static_cast<uint8_t*>(bytes);
  while (length) {
    if (ioOffset == ioLength && !readBlock()) return false;
    const size_t count = std::min(length, ioLength - ioOffset);
    memcpy(data, buffer.get() + ioOffset, count);
    data += count;
    ioOffset += count;
    consumed += count;
    length -= count;
  }
  return true;
}

ChapterEventCache::Next ChapterEventCache::invalidRecord() {
  readError = true;
  LOG_ERR("EVC", "Invalid event stream; retrying XHTML");
  abort();
  return Next::Error;
}

ChapterEventCache::Next ChapterEventCache::next(Record& record) {
  if (mode != Mode::Replay) return Next::Error;
  uint8_t header[3];
  if (!readBytes(header, sizeof(header))) return invalidRecord();
  const size_t length = get16(header + 1);
  uint8_t* data = buffer.get() + IO_BYTES;
  if (length > RECORD_BYTES || !readBytes(data, length)) return invalidRecord();
  record.kind = static_cast<Kind>(header[0]);
  record.data = reinterpret_cast<const char*>(data);
  record.length = static_cast<uint16_t>(length);
  record.attributes = nullptr;
  size_t offset = 0;
  const auto string = [&]() -> const char* {
    if (offset >= length) return nullptr;
    const auto* end = static_cast<const uint8_t*>(memchr(data + offset, 0, length - offset));
    if (!end) return nullptr;
    const char* result = reinterpret_cast<const char*>(data + offset);
    offset = end - data + 1;
    return result;
  };
  switch (record.kind) {
    case Kind::End:
      if (length || depth || ioOffset != ioLength || replayPosition() != streamSize) return invalidRecord();
      return Next::End;
    case Kind::StartElement: {
      if (length < 3 || data[0] > MAX_ATTRIBUTES || depth == MAX_DEPTH) return invalidRecord();
      const size_t count = data[offset++];
      record.data = string();
      if (!record.data || !record.data[0]) return invalidRecord();
      for (size_t i = 0; i < count * 2; ++i) {
        attributes[i] = string();
        if (!attributes[i] || (!(i & 1) && !attributes[i][0])) return invalidRecord();
      }
      attributes[count * 2] = nullptr;
      record.attributes = attributes.data();
      if (offset != length) return invalidRecord();
      ++depth;
      break;
    }
    case Kind::EndElement:
      record.data = string();
      if (!record.data || !record.data[0] || offset != length || !depth) return invalidRecord();
      --depth;
      break;
    case Kind::Text:
    case Kind::DefaultData:
      if (memchr(data, 0, length)) return invalidRecord();
      break;
  }
  if (header[0] > static_cast<uint8_t>(Kind::DefaultData)) return invalidRecord();
  return Next::Record;
}

void ChapterEventCache::writeFailed(const char* reason) {
  LOG_DBG("EVC", "Event cache skipped: %s", reason);
  abort();
}

void ChapterEventCache::commit() {
  if (mode != Mode::Write) return;
  if (depth || !writeRecord(Kind::End, nullptr, 0) || !flushBlock()) {
    writeFailed("incomplete stream");
    return;
  }
  const bool closed = file.close();
  if (!closed || !sourceMatches() || !Storage.replaceDerivedCache(temporaryPath.c_str(), path.c_str())) {
    writeFailed("commit");
    return;
  }
  LOG_DBG("EVC", "Committed %s (%u bytes)", path.c_str(), unsigned(consumed));
  abort();
}

void ChapterEventCache::abort() {
  if (file) file.close();
  if (ownsReplay) {
    if (retainReplay && replayBuffer && replayLength > HEADER_BYTES && !readError && sourceMatches()) {
      replayMemo.bytes = std::move(replayBuffer);
      replayMemo.size = streamSize;
      replayMemo.length = replayLength;
      replayMemo.modified = replayModified;
      memcpy(replayMemo.path, path.c_str(), path.size() + 1);
    }
    replayBuffer.reset();
    ownsReplay = false;
    replayLease.clear();
  }
  replayOffset = replayLength = 0;
  if (ownsWriter) {
    if (!temporaryPath.empty() && Storage.exists(temporaryPath.c_str()))
      Storage.removeDerivedCache(temporaryPath.c_str());
    ownsWriter = false;
    writerLease.clear();
  }
  buffer.reset();
  mode = Mode::Disabled;
  ioOffset = ioLength = 0;
}
