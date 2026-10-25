#pragma once

// FreeInk — Readium LCP license document (basic profile).
//
// Parses the fields the device needs out of a license.lcpl and performs the
// basic-profile key derivation and checks. The production (1.0) profile
// requires a confidential user-key transform and is out of scope here; a
// license carrying it parses but fails isBasicProfile().
//
// Freestanding C++17. JSON via JsonSax (StreamingJsonParser); crypto injected.

#include <stdint.h>

#include <string>

namespace freeink {
namespace content {

class Crypto;

// The FreeInk LCP service endpoints. /fulfill
// embeds a POSTed license document into its encrypted publication; /unlock
// exchanges a license + sha256(passphrase) for the content key — profile
// key derivation (including the production profile's confidential
// transform, once certified) lives server-side only.
inline constexpr const char* LCP_FULFILL_URL = "https://lcp.freeink.org/fulfill";
inline constexpr const char* LCP_UNLOCK_URL = "https://lcp.freeink.org/unlock";

struct LcpLicense {
  std::string id;
  std::string provider;       // issuing provider URI; keys per-provider passphrase reuse
  std::string profile;        // encryption.profile URI
  std::string contentKeyB64;  // encryption.content_key.encrypted_value
  std::string keyCheckB64;    // encryption.user_key.key_check
  std::string hint;           // encryption.user_key.text_hint (passphrase prompt)
  std::string rightsEnd;      // rights.end (ISO 8601); empty = no expiry

  bool valid() const { return !id.empty() && !contentKeyB64.empty() && !keyCheckB64.empty(); }
  bool isBasicProfile() const { return profile == "http://readium.org/lcp/basic-profile"; }
};

// Parses an LCP license document. False on malformed JSON or when a required
// field (id, content key, key check) is missing.
bool parseLcpLicense(const char* json, size_t len, LcpLicense* out);

// Basic profile: user key = SHA-256(passphrase), no further transform.
void lcpUserKey(Crypto& crypto, const char* passphrase, size_t passphraseLen, uint8_t out[32]);

// True when userKey decrypts user_key.key_check back to exactly the license
// id — the spec's wrong-passphrase test.
bool lcpCheckUserKey(Crypto& crypto, const LcpLicense& license, const uint8_t userKey[32]);

// Decrypts encryption.content_key.encrypted_value with the user key into the
// 32-byte AES-256 content key.
bool lcpContentKey(Crypto& crypto, const LcpLicense& license, const uint8_t userKey[32], uint8_t out[32]);

// Unwraps a /unlock response's content key with the device's X25519 private
// key: shared = X25519(priv, epk), kek = HKDF-SHA256(shared, info
// "lcp-key-wrap"), content key = AES-256-GCM(kek, iv, ct, tag). All inputs
// raw bytes (RFC 7748 little-endian keys); the caller handles base64.
bool lcpUnwrapContentKey(Crypto& crypto, const uint8_t devicePriv[32], const uint8_t epk[32], const uint8_t iv[12],
                         const uint8_t ct[32], const uint8_t tag[16], uint8_t out[32]);

// "2026-03-01T10:00:00Z" (optional fractional seconds, Z or ±hh:mm offset)
// to epoch seconds; 0 on parse failure.
int64_t lcpParseIso8601(const char* s);

}  // namespace content
}  // namespace freeink
