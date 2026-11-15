#include "PomodoroHistoryStore.h"

#include <Logging.h>

#include <algorithm>
#include <cstring>

namespace pomodoro {
namespace {
constexpr uint8_t header[16] = {'P', 'O', 'M', 'H', 1, 160, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
}
bool HistoryStore::read(HalFile& file, uint64_t offset, uint32_t expected, bool protect) {
  if (!file.seek64(offset) || file.read(buffer, kHistoryBytes) != int(kHistoryBytes)) {
    if (protect) readOnly = true;
    return false;
  }
  if (!std::memcmp(buffer, "PHJ", 3) && buffer[3] > '1') readOnly = true;
  if (!std::memcmp(buffer, "PHJ1", 4) && buffer[144] > 1) readOnly = true;
  uint32_t found = 0;
  return decodeHistory(buffer, scratch, decoded, found, &decodedOptions) && found == expected;
}
uint32_t HistoryStore::hash(const State& state, const DisplayOptions& options) {
  encode(state, 0, buffer);
  buffer[64] = uint8_t(options.accumulation);
  buffer[65] = options.showAccumulation;
  return readingstats::checksum(buffer, 66);
}
bool HistoryStore::load(State& state, DisplayOptions* options) {
  close();
  if (options) *options = {};
  if (!Storage.ready()) {
    readOnly = true;
    return false;
  }
  if (!Storage.exists(kHistoryPath)) {
    readOnly = !legacy.load(state);
    return !readOnly;
  }
  HalFile file;
  if (!Storage.openFileForRead("POMODORO", kHistoryPath, file)) {
    readOnly = true;
    return false;
  }
  const uint64_t size = file.fileSize64();
  if (size < sizeof(header)) {
    const size_t length = size_t(size);
    if ((length && file.read(buffer, length) != int(length)) || std::memcmp(buffer, header, length)) {
      readOnly = true;
      return false;
    }
    // A known header prefix cannot contain a checkpoint. Legacy state is
    // still authoritative until the first complete history record exists.
    readOnly = !legacy.load(state);
    return !readOnly;
  }
  if (file.read(buffer, sizeof(header)) != int(sizeof(header)) || std::memcmp(buffer, header, sizeof(header))) {
    readOnly = true;
    return false;
  }
  headerReady = true;
  const uint64_t records = (size - 16) / kHistoryBytes;
  if (records > UINT32_MAX) {
    readOnly = true;
    return false;
  }
  hadDamage = (size - 16) % kHistoryBytes != 0;
  // An incomplete final checkpoint may follow the last verified record.
  for (uint64_t n = records; n > 0 && records - n < 16; --n) {
    if (read(file, 16 + (n - 1) * kHistoryBytes, uint32_t(n))) {
      state = scratch;
      if (options) *options = decodedOptions;
      sequence = uint32_t(n);
      end = 16 + n * kHistoryBytes;
      hasRecord = true;
      savedHash = hash(state, decodedOptions);
      hadDamage = false;
      return !readOnly;
    }
    hadDamage = true;
    if (readOnly) break;
  }
  if (records) {
    readOnly = true;
    return false;
  }
  readOnly = !legacy.load(state);
  return !readOnly;
}
bool HistoryStore::save(const State& state, Pending& pending, const DisplayOptions& options) {
  if (readOnly || !valid(state) || sequence == UINT32_MAX || uint8_t(options.accumulation) > uint8_t(Period::Week))
    return false;
  const uint32_t expectedHash = hash(state, options);
  if (hasRecord && expectedHash == savedHash && pending.empty()) return true;
  if (!Storage.ensureDirectoryExists("/.crosspoint")) return false;
  if (!headerReady) {
    HalFile file;
    if (!Storage.openFileForWrite("POMODORO", kHistoryPath, file) || file.write(header, 16) != 16 || !file.close())
      return false;
    headerReady = true;
  }
  encodeHistory(state, pending, sequence + 1, buffer, options);
  {
    HalFile file;
    if (!Storage.openFileForUpdate("POMODORO", kHistoryPath, file) || !file.seek64(end) ||
        file.write(buffer, kHistoryBytes) != kHistoryBytes || !file.truncate(end + kHistoryBytes) || !file.close()) {
      LOG_ERR("POMODORO", "History checkpoint write failed");
      return false;
    }
  }
  {
    HalFile file;
    if (!Storage.openFileForRead("POMODORO", kHistoryPath, file) || !read(file, end, sequence + 1, false)) {
      LOG_ERR("POMODORO", "History checkpoint verification failed");
      return false;
    }
    encodeHistory(state, pending, sequence + 1, buffer, options);
    // Verification must cover study credit as well as the timer state.
    const uint32_t expected = readingstats::checksum(buffer, kHistoryBytes);
    if (!file.seek64(end) || file.read(buffer, kHistoryBytes) != int(kHistoryBytes) ||
        readingstats::checksum(buffer, kHistoryBytes) != expected)
      return false;
  }
  ++sequence;
  end += kHistoryBytes;
  savedHash = expectedHash;
  hasRecord = true;
  // The recovered tail is replaced; earlier scan damage remains an error.
  pending = {};
  return true;
}
bool HistoryStore::beginScan() {
  close();
  cursor = 16;
  scanEnd = end;
  return !readOnly && (!hasRecord || Storage.openFileForRead("POMODORO", kHistoryPath, scan));
}
Scan HistoryStore::next(Pending& delta) {
  if (cursor >= scanEnd) {
    close();
    return Scan::Finished;
  }
  if (cursor < bufferedAt || cursor + kHistoryBytes > bufferedAt + bufferedBytes) {
    bufferedAt = cursor;
    bufferedBytes = size_t(std::min(uint64_t(sizeof(readAhead)), scanEnd - cursor));
    if (!scan || !scan.seek64(cursor) || scan.read(readAhead, bufferedBytes) != int(bufferedBytes)) {
      hadDamage = readOnly = true;
      close();
      return Scan::Error;
    }
  }
  const auto* bytes = readAhead + size_t(cursor - bufferedAt);
  uint32_t found = 0;
  if (!decodeHistory(bytes, scratch, delta, found) || found != (cursor - 16) / kHistoryBytes + 1) {
    hadDamage = true;
    readOnly = true;
    close();
    return Scan::Error;
  }
  cursor += kHistoryBytes;
  return Scan::Record;
}
void HistoryStore::close() {
  scan.close();
  bufferedBytes = 0;
}
}  // namespace pomodoro
