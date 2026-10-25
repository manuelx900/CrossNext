#include "LcpLicense.h"

#include <StreamingJsonParser.h>
#include <stdio.h>
#include <string.h>

#include "Crypto.h"
#include "Util.h"

namespace freeink {
namespace content {

namespace {

// The license's interesting strings all live at known container paths; the
// scan tracks the enclosing key per depth and matches on (depth, parent,
// key). Everything else — links, signature, user fields — is skipped.
struct ParseCtx {
  LcpLicense* out = nullptr;
  char pending[32] = {0};
  char parent[8][32] = {{0}};
  int depth = 0;

  const char* parentAt(const int d) const { return (d >= 0 && d < 8) ? parent[d] : ""; }
};

void assignCapped(std::string& target, const char* value, size_t len, const size_t cap) {
  target.assign(value, len < cap ? len : cap);
}

void onKey(void* ud, const char* key, const size_t len) {
  auto& ctx = *static_cast<ParseCtx*>(ud);
  const size_t n = len < sizeof(ctx.pending) - 1 ? len : sizeof(ctx.pending) - 1;
  memcpy(ctx.pending, key, n);
  ctx.pending[n] = '\0';
}

void onContainerStart(void* ud) {
  auto& ctx = *static_cast<ParseCtx*>(ud);
  if (ctx.depth < 8) {
    memcpy(ctx.parent[ctx.depth], ctx.pending, sizeof(ctx.pending));
  }
  ctx.depth++;
  ctx.pending[0] = '\0';
}

void onContainerEnd(void* ud) {
  auto& ctx = *static_cast<ParseCtx*>(ud);
  if (ctx.depth > 0) ctx.depth--;
}

void onString(void* ud, const char* value, const size_t len) {
  auto& ctx = *static_cast<ParseCtx*>(ud);
  LcpLicense& lic = *ctx.out;
  const char* key = ctx.pending;
  switch (ctx.depth) {
    case 1:  // top-level object
      if (strcmp(key, "id") == 0) assignCapped(lic.id, value, len, 256);
      if (strcmp(key, "provider") == 0) assignCapped(lic.provider, value, len, 256);
      break;
    case 2:
      if (strcmp(ctx.parentAt(1), "encryption") == 0 && strcmp(key, "profile") == 0) {
        assignCapped(lic.profile, value, len, 128);
      } else if (strcmp(ctx.parentAt(1), "rights") == 0 && strcmp(key, "end") == 0) {
        assignCapped(lic.rightsEnd, value, len, 64);
      }
      break;
    case 3:
      if (strcmp(ctx.parentAt(1), "encryption") != 0) break;
      if (strcmp(ctx.parentAt(2), "content_key") == 0 && strcmp(key, "encrypted_value") == 0) {
        assignCapped(lic.contentKeyB64, value, len, 512);
      } else if (strcmp(ctx.parentAt(2), "user_key") == 0) {
        if (strcmp(key, "key_check") == 0) assignCapped(lic.keyCheckB64, value, len, 512);
        if (strcmp(key, "text_hint") == 0) assignCapped(lic.hint, value, len, 512);
      }
      break;
    default:
      break;
  }
}

void onIgnore(void*, const char*, size_t) {}
void onIgnoreBool(void*, bool) {}
void onIgnoreNull(void*) {}

// Decodes an LCP encrypted blob (base64 of IV || AES-256-CBC ciphertext),
// decrypts with the user key and strips PKCS#7 padding. Returns the
// plaintext length, or -1.
int32_t decryptBlob(Crypto& crypto, const std::string& b64, const uint8_t userKey[32], uint8_t* out,
                    const size_t outCap) {
  uint8_t blob[256 + 16];
  const int32_t blobLen = base64Decode(b64.data(), b64.size(), blob, sizeof(blob));
  if (blobLen < 32 || (blobLen - 16) % 16 != 0) return -1;
  const size_t cipherLen = static_cast<size_t>(blobLen) - 16;
  if (cipherLen > outCap) return -1;
  if (!crypto.aes256CbcDecrypt(userKey, blob, blob + 16, cipherLen, out)) return -1;
  // W3C xmlenc padding: random fill bytes, only the count byte matters. A
  // wrong key shows up as a failed key_check comparison, not a pad error.
  const uint8_t pad = out[cipherLen - 1];
  if (pad < 1 || pad > 16 || pad > cipherLen) return -1;
  return static_cast<int32_t>(cipherLen - pad);
}

}  // namespace

bool parseLcpLicense(const char* json, const size_t len, LcpLicense* out) {
  *out = LcpLicense{};
  ParseCtx ctx;
  ctx.out = out;
  JsonCallbacks callbacks = {};
  callbacks.ctx = &ctx;
  callbacks.onKey = onKey;
  callbacks.onString = onString;
  callbacks.onNumber = onIgnore;
  callbacks.onBool = onIgnoreBool;
  callbacks.onNull = onIgnoreNull;
  callbacks.onObjectStart = onContainerStart;
  callbacks.onObjectEnd = onContainerEnd;
  callbacks.onArrayStart = onContainerStart;
  callbacks.onArrayEnd = onContainerEnd;
  StreamingJsonParser parser(callbacks);
  parser.feed(json, len);
  return !parser.hasError() && out->valid();
}

void lcpUserKey(Crypto& crypto, const char* passphrase, const size_t passphraseLen, uint8_t out[32]) {
  crypto.sha256(reinterpret_cast<const uint8_t*>(passphrase), passphraseLen, out);
}

bool lcpCheckUserKey(Crypto& crypto, const LcpLicense& license, const uint8_t userKey[32]) {
  uint8_t plain[256];
  const int32_t n = decryptBlob(crypto, license.keyCheckB64, userKey, plain, sizeof(plain));
  if (n < 0) return false;
  return static_cast<size_t>(n) == license.id.size() && memcmp(plain, license.id.data(), n) == 0;
}

bool lcpContentKey(Crypto& crypto, const LcpLicense& license, const uint8_t userKey[32], uint8_t out[32]) {
  uint8_t plain[256];
  const int32_t n = decryptBlob(crypto, license.contentKeyB64, userKey, plain, sizeof(plain));
  if (n != 32) return false;
  memcpy(out, plain, 32);
  return true;
}

bool lcpUnwrapContentKey(Crypto& crypto, const uint8_t devicePriv[32], const uint8_t epk[32], const uint8_t iv[12],
                         const uint8_t ct[32], const uint8_t tag[16], uint8_t out[32]) {
  uint8_t shared[32];
  if (!crypto.x25519SharedSecret(devicePriv, epk, shared)) return false;
  uint8_t kek[32];
  static constexpr char INFO[] = "lcp-key-wrap";
  if (!crypto.hkdfSha256(shared, sizeof(shared), reinterpret_cast<const uint8_t*>(INFO), sizeof(INFO) - 1, kek,
                         sizeof(kek))) {
    return false;
  }
  return crypto.aes256GcmDecrypt(kek, iv, ct, 32, tag, out);
}

int64_t lcpParseIso8601(const char* s) {
  if (!s) return 0;
  int y, mo, d, h, mi, sec;
  int fields = sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d", &y, &mo, &d, &h, &mi, &sec);
  if (fields != 6 || y < 1970 || mo < 1 || mo > 12 || d < 1 || d > 31) return 0;

  // Days from civil date (Howard Hinnant's algorithm), then clock + offset.
  const int yAdj = y - (mo <= 2);
  const int era = yAdj / 400;
  const unsigned yoe = static_cast<unsigned>(yAdj - era * 400);
  const unsigned doy = (153u * static_cast<unsigned>(mo + (mo > 2 ? -3 : 9)) + 2u) / 5u + static_cast<unsigned>(d) - 1u;
  const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  const int64_t days = static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
  int64_t epoch = days * 86400 + h * 3600 + mi * 60 + sec;

  // ISO 8601 fields are fixed-width, so the clock ends at offset 19
  // ("YYYY-MM-DDThh:mm:ss"). Skip fractional seconds, then apply a Z /
  // ±hh:mm offset.
  if (strlen(s) < 19) return 0;
  const char* p = s + 19;
  if (*p == '.') {
    p++;
    while (*p >= '0' && *p <= '9') p++;
  }
  if (*p == '+' || *p == '-') {
    int oh = 0, om = 0;
    const int sign = *p == '+' ? 1 : -1;
    if (sscanf(p + 1, "%2d:%2d", &oh, &om) >= 1) epoch -= sign * (oh * 3600 + om * 60);
  }
  return epoch > 0 ? epoch : 0;
}

}  // namespace content
}  // namespace freeink
