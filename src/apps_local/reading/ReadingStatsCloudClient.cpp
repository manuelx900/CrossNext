#include "ReadingStatsCloudClient.h"

#include <DeviceSecret.h>
#include <HalMemory.h>
#include <KOReaderCredentialStore.h>
#include <Logging.h>
#include <Memory.h>
#include <SecureHttpClient.h>
#include <wolfssl/wolfcrypt/hash.h>

#include <cstring>

namespace readingstats::cloud {
Client::Client(Cancel cancelled, void* user) : cancel(cancelled), context(user) {}
Client::~Client() = default;
void Client::close() { http.reset(); }
bool Client::configured() {
  return KOREADER_STORE.hasCredentials() && KOREADER_STORE.getServerType() != KOReaderServerType::KOSYNC;
}
uint64_t Client::accountId() {
  uint64_t hash = 14695981039346656037ULL;
  const auto mix = [&](const char* text) {
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p)
      hash = (hash ^ *p) * 1099511628211ULL;
    hash = (hash ^ 0) * 1099511628211ULL;
  };
  mix(KOREADER_STORE.getBaseUrl().c_str());
  mix(KOREADER_STORE.getUsername().c_str());
  return hash;
}
Result Client::fetch(void* client, const char* path, const char* destination) {
  return static_cast<Client*>(client)->send("GET", path, nullptr, destination);
}
uint64_t Client::deviceTag() {
  uint8_t secret[32], input[48]{}, digest[32];
  if (!deviceSecret(secret)) return 0;
  std::memcpy(input, "crossnext-stats:", 16);
  std::memcpy(input + 16, secret, 32);
  if (wc_Sha256Hash(input, sizeof(input), digest)) return 0;
  uint64_t tag = 0;
  for (int n = 0; n < 8; ++n) tag |= uint64_t(digest[n]) << (8 * n);
  return tag;
}
Result Client::exchange(void* client, const char* method, const char* path, const char* body, const char* destination) {
  return static_cast<Client*>(client)->send(method, path, body, destination);
}
Result Client::send(const char* method, const char* path, const char* body, const char* destination) {
  if (!KOREADER_STORE.hasCredentials()) return Result::NoCredentials;
  if (KOREADER_STORE.getServerType() == KOReaderServerType::KOSYNC) return Result::Unsupported;
  if (cancelled()) return Result::Cancelled;
  const auto heap = HalMemory::getDefaultHeap();
  if (heap.freeBytes < 35000 || heap.largestBlockBytes < 20000) {
    LOG_ERR("STATS-SYNC", "Insufficient allocatable heap for TLS");
    return Result::LowMemory;
  }
  const auto base = KOREADER_STORE.getBaseUrl();
  if (base.size() > 512 || !path || std::strlen(path) > 1663) return Result::LimitExceeded;
  if (!http) {
    // One checked client per import; keep-alive avoids a TLS handshake per page.
    http = makeUniqueNoThrow<freeink::SecureHttpClient>();
    if (!http) {
      LOG_ERR("STATS-SYNC", "OOM: HTTP client");
      return Result::LowMemory;
    }
    http->setInsecure();
    http->setTimeout(10000);
  }
  if (!http->begin(base + path)) return Result::NetworkError;
  http->addHeader("Accept", "application/json");
  if (body) http->addHeader("Content-Type", "application/json");
  http->addHeader("x-auth-user", KOREADER_STORE.getUsername());
  http->addHeader("x-auth-key", KOREADER_STORE.getMd5Password());
  HalFile output;
  if (!Storage.openFileForWrite("STATS-SYNC", destination, output)) return Result::StorageError;
  // Download directly to SD. JSON's filtered parser, not the HTTP body, owns RAM.
  constexpr size_t maximumBytes = 2 * 1024 * 1024;
  size_t received = 0;
  bool stopped = false, storageFailed = false, tooLarge = false;
  const int status = http->sendRequest(
      method, reinterpret_cast<const uint8_t*>(body), body ? std::strlen(body) : 0,
      [&](const uint8_t* bytes, size_t size) {
        if (cancelled()) {
          stopped = true;
          return false;
        }
        if (size > maximumBytes - received) {
          tooLarge = true;
          return false;
        }
        if (output.write(bytes, size) != size) {
          storageFailed = true;
          return false;
        }
        received += size;
        return true;
      },
      [&] {
        if (cancelled()) stopped = true;
        return stopped;
      });
  const bool complete = http->responseComplete();
  if (stopped) return Result::Cancelled;
  if (storageFailed) return Result::StorageError;
  if (tooLarge) return Result::LimitExceeded;
  if (status == 401 || status == 403) return Result::AuthFailed;
  if (status == 404 || status == 405) return Result::Unsupported;
  if (status != 200 || !complete) return Result::NetworkError;
  return Result::Success;
}
}  // namespace readingstats::cloud
