// LCP license tests: document parse, ISO 8601, and the blob unwrap logic
// (base64 + IV split + PKCS#7) through an identity-cipher stub. Real AES-256
// is wolfSSL's job and is exercised on device.
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "Crypto.h"
#include "LcpLicense.h"

using namespace freeink::content;

namespace {

// "Decrypts" by copying: lets the blob tests validate the IV/padding
// handling with predictable bytes.
class IdentityCrypto : public Crypto {
 public:
  int32_t rsaPrivateRaw(const uint8_t*, size_t, const uint8_t*, size_t, uint8_t*, size_t) override { return -1; }
  bool aes128CbcDecrypt(const uint8_t*, const uint8_t*, const uint8_t* in, size_t len, uint8_t* out) override {
    memmove(out, in, len);
    return true;
  }
  bool aes256CbcDecrypt(const uint8_t*, const uint8_t*, const uint8_t* in, size_t len, uint8_t* out) override {
    memmove(out, in, len);
    return true;
  }
  void sha1(const uint8_t*, size_t, uint8_t out[20]) override { memset(out, 0, 20); }
  void sha256(const uint8_t* data, size_t len, uint8_t out[32]) override {
    // Not a hash; enough to observe the passphrase reaches the derivation.
    memset(out, 0, 32);
    for (size_t i = 0; i < len && i < 32; i++) out[i] = data[i];
  }
  bool rsaGenerate(RsaKeyPairDer*) override { return false; }
  bool rsaPublicEncrypt(const uint8_t*, size_t, const uint8_t*, size_t, uint8_t*, size_t, size_t*) override {
    return false;
  }
  bool rsaPrivateSignRaw(const uint8_t*, size_t, const uint8_t*, uint8_t*) override { return false; }
  bool aes128CbcEncrypt(const uint8_t*, const uint8_t*, const uint8_t*, size_t, uint8_t*) override { return false; }
  bool pkcs12Extract(const uint8_t*, size_t, const std::string&, std::vector<uint8_t>*, std::vector<uint8_t>*) override {
    return false;
  }
  void randomBytes(uint8_t*, size_t) override {}
};

const char* B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string b64(const uint8_t* data, size_t len) {
  std::string out;
  for (size_t i = 0; i < len; i += 3) {
    uint32_t v = data[i] << 16;
    if (i + 1 < len) v |= data[i + 1] << 8;
    if (i + 2 < len) v |= data[i + 2];
    out += B64[(v >> 18) & 63];
    out += B64[(v >> 12) & 63];
    out += i + 1 < len ? B64[(v >> 6) & 63] : '=';
    out += i + 2 < len ? B64[v & 63] : '=';
  }
  return out;
}

// IV || plaintext with W3C xmlenc padding (random-looking fill, count in the
// last byte) — what the identity cipher "decrypts" to. Real LCP servers pad
// this way; strict PKCS#7 fill validation would reject it.
std::string identityBlob(const std::string& plain) {
  uint8_t buf[256];
  memset(buf, 0xA5, 16);  // IV
  const size_t pad = 16 - (plain.size() % 16);
  memcpy(buf + 16, plain.data(), plain.size());
  for (size_t i = 0; i < pad; i++) buf[16 + plain.size() + i] = static_cast<uint8_t>(0x30 + i);  // junk fill
  buf[16 + plain.size() + pad - 1] = static_cast<uint8_t>(pad);
  return b64(buf, 16 + plain.size() + pad);
}

}  // namespace

int main() {
  // --- document parse -------------------------------------------------------
  const std::string id = "df09ac25-a386-4c5c-b167-33ce4c36ca65";
  const std::string license = std::string(R"({
    "provider": "https://front-test.edrlab.org",
    "id": ")") + id + R"(",
    "issued": "2026-10-01T09:00:00Z",
    "encryption": {
      "profile": "http://readium.org/lcp/basic-profile",
      "content_key": {
        "algorithm": "http://www.w3.org/2001/04/xmlenc#aes256-cbc",
        "encrypted_value": "Y29udGVudC1rZXk="
      },
      "user_key": {
        "algorithm": "http://www.w3.org/2001/04/xmlenc#sha256",
        "text_hint": "The passphrase is the word you chose",
        "key_check": "a2V5LWNoZWNr"
      }
    },
    "links": [
      {"rel": "publication", "href": "https://x/book.epub", "type": "application/epub+zip"},
      {"rel": "hint", "href": "https://x/hint", "profile": "http://not-the-encryption-profile"}
    ],
    "rights": {"print": 10, "copy": 2048, "start": "2026-10-01T09:00:00Z", "end": "2026-10-31T09:00:00Z"},
    "user": {"id": "u1"},
    "signature": {"value": "sig"}
  })";

  LcpLicense doc;
  assert(parseLcpLicense(license.data(), license.size(), &doc));
  assert(doc.id == id);
  assert(doc.isBasicProfile());
  assert(doc.hint == "The passphrase is the word you chose");
  assert(doc.rightsEnd == "2026-10-31T09:00:00Z");
  assert(doc.provider == "https://front-test.edrlab.org");
  // The hint link's "profile" must not clobber encryption.profile.
  assert(doc.profile == "http://readium.org/lcp/basic-profile");

  LcpLicense bad;
  const char* notLicense = R"({"id": "x", "links": []})";
  assert(!parseLcpLicense(notLicense, strlen(notLicense), &bad));

  // --- ISO 8601 --------------------------------------------------------------
  assert(lcpParseIso8601("1970-01-01T00:00:00Z") == 0);  // 0 doubles as "no expiry"; fine at epoch
  assert(lcpParseIso8601("2026-10-31T09:00:00Z") == 1793437200);
  assert(lcpParseIso8601("2026-10-31T10:00:00+01:00") == 1793437200);
  assert(lcpParseIso8601("2026-10-31T09:00:00.500Z") == 1793437200);
  assert(lcpParseIso8601("not a date") == 0);

  // --- key check / content key via the identity cipher ------------------------
  IdentityCrypto crypto;
  uint8_t userKey[32];
  lcpUserKey(crypto, "secret", 6, userKey);
  assert(userKey[0] == 's' && userKey[5] == 't');

  LcpLicense keyed = doc;
  keyed.keyCheckB64 = identityBlob(id);
  assert(lcpCheckUserKey(crypto, keyed, userKey));
  keyed.keyCheckB64 = identityBlob("wrong-id");
  assert(!lcpCheckUserKey(crypto, keyed, userKey));

  std::string rawKey(32, '\0');
  for (int i = 0; i < 32; i++) rawKey[static_cast<size_t>(i)] = static_cast<char>(i + 1);
  keyed.contentKeyB64 = identityBlob(rawKey);
  uint8_t contentKey[32];
  assert(lcpContentKey(crypto, keyed, userKey, contentKey));
  assert(contentKey[0] == 1 && contentKey[31] == 32);
  keyed.contentKeyB64 = identityBlob("too-short");
  assert(!lcpContentKey(crypto, keyed, userKey, contentKey));

  printf("test_lcp: all assertions passed\n");
  return 0;
}
