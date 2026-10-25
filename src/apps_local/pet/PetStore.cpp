#include "PetStore.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

namespace pet {
namespace {
constexpr const char* PATHS[] = {"/.crosspoint/pet/state-a.bin", "/.crosspoint/pet/state-b.bin"};
}
bool Store::load(Profile& p, bool& writable) {
  p = {};
  writable = true;
  bool exists = false, found = false;
  for (const auto* path : PATHS) {
    if (!Storage.exists(path)) continue;
    exists = true;
    HalFile file;
    if (!Storage.openFileForRead("PET", path, file)) continue;
    const auto length = file.size();
    if (length != STATE_BYTES && length != LEGACY_STATE_BYTES) {
      if (file.read(bytes, 4) == 4 && std::memcmp(bytes, "PET", 3) == 0 && bytes[3] != '1' && bytes[3] != '2' &&
          bytes[3] != '3' && bytes[3] != '4' && bytes[3] != '5' && bytes[3] != '6' && bytes[3] != '7') {
        writable = false;
        LOG_ERR("PET", "Unsupported save size/version");
        return false;
      }
      continue;
    }
    if (file.read(bytes, length) != int(length)) continue;
    if (std::memcmp(bytes, "PET", 3) == 0 && bytes[3] != '1' && bytes[3] != '2' && bytes[3] != '3' && bytes[3] != '4' &&
        bytes[3] != '5' && bytes[3] != '6' && bytes[3] != '7') {
      writable = false;
      LOG_ERR("PET", "Unsupported save version");
      return false;
    }
    if ((!std::memcmp(bytes, "PET4", 4) || !std::memcmp(bytes, "PET5", 4) || !std::memcmp(bytes, "PET6", 4) ||
         !std::memcmp(bytes, "PET7", 4)) &&
        bytes[MOMENTS_VERSION_OFFSET] > (bytes[3] >= '6' ? 2 : 1)) {
      uint32_t storedChecksum = 0;
      for (unsigned i = 0; i < 4; ++i) storedChecksum |= uint32_t(bytes[length - 4 + i]) << (8 * i);
      if (storedChecksum != checksum(bytes, length - 4)) continue;
      writable = false;
      LOG_ERR("PET", "Unsupported memories version");
      return false;
    }
    if (decode(bytes, candidate, length) && (!found || int32_t(candidate.sequence - p.sequence) > 0)) {
      p = candidate;
      found = true;
    }
  }
  writable = found || !exists;
  if (!writable) LOG_ERR("PET", "No valid saved profile");
  return writable;
}
bool Store::save(Profile& p, bool writable) {
  if (!writable || !Storage.ensureDirectoryExists("/.crosspoint") ||
      !Storage.ensureDirectoryExists("/.crosspoint/pet")) {
    LOG_ERR("PET", "Save directory unavailable");
    return false;
  }
  candidate = p;
  candidate.sequence = p.sequence + 1;
  encode(candidate, bytes);
  const auto* path = PATHS[candidate.sequence & 1u];
  {
    HalFile file;
    if (!Storage.openFileForWrite("PET", path, file) || file.write(bytes, sizeof(bytes)) != sizeof(bytes)) {
      LOG_ERR("PET", "Could not write profile");
      return false;
    }
  }
  HalFile verify;
  if (!Storage.openFileForRead("PET", path, verify) || verify.size() != STATE_BYTES ||
      verify.read(bytes, sizeof(bytes)) != sizeof(bytes) || !decode(bytes, candidate) ||
      candidate.sequence != p.sequence + 1) {
    LOG_ERR("PET", "Profile read-back failed");
    return false;
  }
  p = candidate;
  return true;
}
}  // namespace pet
