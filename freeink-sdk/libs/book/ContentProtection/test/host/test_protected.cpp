// decryptedSize() must report the plaintext size from encryption.xml's
// OriginalLength, not the zip entry size (the encrypted blob). Callers size
// their output buffer from it and reject any mismatch.
#include <ProtectedBook.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using freeink::content::ProtectedBook;

struct Source : freeink::content::ByteSource {
  std::vector<uint8_t> bytes;
  uint64_t size() const override { return bytes.size(); }
  int32_t readAt(uint64_t offset, void* dst, uint32_t len) override {
    if (offset > bytes.size() || len > bytes.size() - offset) return -1;
    std::memcpy(dst, bytes.data() + offset, len);
    return static_cast<int32_t>(len);
  }
};

void put(std::vector<uint8_t>& b, uint32_t v, int n) {
  for (int i = 0; i < n; i++) b.push_back(static_cast<uint8_t>(v >> (8 * i)));
}

// Minimal stored (method 0) zip: local headers + data, central directory, EOCD.
Source storedZip(const std::vector<std::pair<std::string, std::string>>& files) {
  Source s;
  std::vector<uint8_t> cd;
  for (const auto& f : files) {
    const uint32_t offset = static_cast<uint32_t>(s.bytes.size());
    const uint32_t size = static_cast<uint32_t>(f.second.size());
    put(s.bytes, 0x04034b50, 4);
    for (int i = 0; i < 4; i++) put(s.bytes, 0, 2);  // version, flags, method, time
    put(s.bytes, 0, 2);                              // date
    put(s.bytes, 0, 4);                              // crc (not checked)
    put(s.bytes, size, 4);
    put(s.bytes, size, 4);
    put(s.bytes, static_cast<uint32_t>(f.first.size()), 2);
    put(s.bytes, 0, 2);
    s.bytes.insert(s.bytes.end(), f.first.begin(), f.first.end());
    s.bytes.insert(s.bytes.end(), f.second.begin(), f.second.end());

    put(cd, 0x02014b50, 4);
    for (int i = 0; i < 6; i++) put(cd, 0, 2);  // versions, flags, method, time, date
    put(cd, 0, 4);
    put(cd, size, 4);
    put(cd, size, 4);
    put(cd, static_cast<uint32_t>(f.first.size()), 2);
    for (int i = 0; i < 4; i++) put(cd, 0, 2);  // extra, comment, disk, internal attrs
    put(cd, 0, 4);
    put(cd, offset, 4);
    cd.insert(cd.end(), f.first.begin(), f.first.end());
  }
  const uint32_t cdOffset = static_cast<uint32_t>(s.bytes.size());
  s.bytes.insert(s.bytes.end(), cd.begin(), cd.end());
  put(s.bytes, 0x06054b50, 4);
  put(s.bytes, 0, 2);
  put(s.bytes, 0, 2);
  put(s.bytes, static_cast<uint32_t>(files.size()), 2);
  put(s.bytes, static_cast<uint32_t>(files.size()), 2);
  put(s.bytes, static_cast<uint32_t>(cd.size()), 4);
  put(s.bytes, cdOffset, 4);
  put(s.bytes, 0, 2);
  return s;
}

// The shape a Readium LCP server emits, namespace attributes included.
std::string encEntry(const char* uri, const char* compression) {
  std::string e =
      "<EncryptedData xmlns=\"http://www.w3.org/2001/04/xmlenc#\">"
      "<EncryptionMethod xmlns=\"http://www.w3.org/2001/04/xmlenc#\" "
      "Algorithm=\"http://www.w3.org/2001/04/xmlenc#aes256-cbc\"></EncryptionMethod>"
      "<CipherData xmlns=\"http://www.w3.org/2001/04/xmlenc#\">"
      "<CipherReference xmlns=\"http://www.w3.org/2001/04/xmlenc#\" URI=\"";
  e += uri;
  e += "\"></CipherReference></CipherData>";
  if (compression) {
    e += "<EncryptionProperties><EncryptionProperty><Compression xmlns=\"http://www.idpf.org/2016/encryption#compression\" ";
    e += compression;
    e += "></Compression></EncryptionProperty></EncryptionProperties>";
  }
  return e + "</EncryptedData>";
}

int main() {
  const std::string xml = "<?xml version=\"1.0\"?><encryption xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">" +
                          encEntry("text/a.xhtml", "Method=\"8\" OriginalLength=\"7175\"") +
                          encEntry("text/b.xhtml", "Method=\"0\" OriginalLength=\"20\"") +
                          encEntry("text/c.xhtml", nullptr) + "</encryption>";
  auto src = storedZip({{"mimetype", "application/epub+zip"},
                        {"META-INF/encryption.xml", xml},
                        {"text/a.xhtml", std::string(48, 'x')},
                        {"text/b.xhtml", std::string(48, 'y')},
                        {"text/c.xhtml", std::string(32, 'z')}});

  ProtectedBook book;
  assert(book.open(src));
  assert(book.isProtected());
  assert(book.isEncrypted("text/a.xhtml") && book.isEncrypted("text/c.xhtml"));
  assert(book.decryptedSize("text/a.xhtml") == 7175);  // deflated: OriginalLength
  assert(book.decryptedSize("text/b.xhtml") == 20);    // stored: OriginalLength, not 48
  assert(book.decryptedSize("text/c.xhtml") == 32);    // no property: zip size fallback
  assert(book.decryptedSize("mimetype") == 20);
  assert(book.decryptedSize("missing") == 0);

  std::puts("test_protected: all assertions passed");
  return 0;
}
