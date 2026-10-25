#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

#include "ChapterEventCache.h"
using Cache = ChapterEventCache;
static constexpr const char* SOURCE = "/source.xhtml";
static constexpr const char* PATH = "/events/0.evc";
static uint32_t crc(const uint8_t* p, size_t n) {
  uint32_t v = ~0U;
  for (size_t i = 0; i < n; ++i) {
    v ^= p[i];
    for (int j = 0; j < 8; ++j) v = (v >> 1) ^ ((v & 1) ? 0xEDB88320U : 0);
  }
  return ~v;
}
static void put32(uint8_t* p, uint32_t v) {
  for (int i = 0; i < 4; ++i) p[i] = v >> (8 * i);
}
static std::vector<uint8_t>& bytes() { return Storage.entries.at(PATH)->bytes; }
static void record(bool large = false) {
  Cache c;
  assert(!c.begin(SOURCE, PATH, false));
  const char* attrs[] = {"id", "alpha", "class", "beta", nullptr};
  c.startElement("html", attrs);
  if (large) {
    const std::string text(4000, 'x');
    for (int i = 0; i < 4; ++i) c.text(Cache::Kind::Text, text.data(), int(text.size()));
  } else {
    c.text(Cache::Kind::Text, "A & B", 5);
    c.text(Cache::Kind::DefaultData, "&nbsp;", 6);
  }
  c.endElement("html");
  c.commit();
  assert(Storage.exists(PATH));
  assert(!Storage.exists("/events/0.evc.part"));
}
static bool replay() {
  Cache c;
  if (!c.begin(SOURCE, PATH, true)) return false;
  Cache::Record r;
  int events = 0;
  for (;;) {
    const auto n = c.next(r);
    if (n == Cache::Next::Error) {
      assert(c.hadReadError());
      return false;
    }
    if (n == Cache::Next::End) {
      assert(events >= 3);
      return true;
    }
    ++events;
  }
}
static void fixFirstBlock() {
  auto& b = bytes();
  const size_t n = b[20] | (b[21] << 8);
  put32(b.data() + 22, crc(b.data() + 26, n));
}
int main() {
  Storage.entries[SOURCE] = std::make_shared<Entry>();
  Storage.entries[SOURCE]->bytes.assign(11, 'x');
  record();
  assert(replay());
  const auto good = bytes();
  assert(HalMemory::active == 0);
  {
    Cache c;
    assert(c.begin(SOURCE, PATH, true));
    Cache::Record r;
    assert(c.next(r) == Cache::Next::Record);
    assert(r.kind == Cache::Kind::StartElement);
    assert(!strcmp(r.data, "html"));
    assert(!strcmp(r.attributes[0], "id"));
    assert(!strcmp(r.attributes[1], "alpha"));
    assert(!r.attributes[4]);
  }
  // Every byte alteration is rejected before completion; the body CRC covers callback data.
  for (size_t i = 0; i < good.size(); ++i) {
    bytes() = good;
    bytes()[i] ^= 0x40;
    assert(!replay());
  }
  for (size_t n = 0; n < good.size(); ++n) {
    bytes().assign(good.begin(), good.begin() + n);
    assert(!replay());
  }
  bytes() = good;
  bytes().push_back(1);
  assert(!replay());
  bytes() = good;
  bytes()[26] = 255;
  fixFirstBlock();
  assert(!replay());
  bytes() = good;
  bytes()[27] = 255;
  bytes()[28] = 255;
  fixFirstBlock();
  assert(!replay());
  bytes() = good;
  bytes()[29] = 33;
  fixFirstBlock();
  assert(!replay());
  bytes() = good;
  bytes()[29] = 0;
  fixFirstBlock();
  assert(!replay());
  bytes() = good;
  bytes()[34] = 'x';
  fixFirstBlock();
  assert(!replay());
  record(true);
  assert(replay());
  auto many = bytes();
  bytes()[5000] ^= 1;
  assert(!replay());
  bytes() = many;
  {
    Cache c;
    assert(!c.begin(SOURCE, PATH, false));
    c.startElement("html", nullptr);
    c.abort();
  }
  assert(bytes() == many);
  {
    Cache c;
    assert(!c.begin(SOURCE, PATH, false));
    const std::string text(4097, 'x');
    c.text(Cache::Kind::Text, text.data(), int(text.size()));
    c.commit();
  }
  assert(bytes() == many);
  {
    Cache a, b;
    assert(!a.begin(SOURCE, PATH, false));
    assert(!b.begin(SOURCE, "/events/1.evc", false));
    a.startElement("h", nullptr);
    a.endElement("h");
    a.commit();
    b.commit();
    assert(!Storage.exists("/events/1.evc"));
  }
  {
    Cache c;
    assert(!c.begin(SOURCE, PATH, false));
    c.startElement("h", nullptr);
    c.endElement("h");
    Storage.entries[SOURCE]->modified++;
    c.commit();
  }
  assert(!Storage.exists("/events/0.evc.part"));
  record();
  Storage.entries[SOURCE]->bytes.push_back('x');
  assert(!replay());
  record();
  Storage.entries[SOURCE]->modified++;
  assert(!replay());
  HalMemory::fail = true;
  assert(!replay());
  assert(HalMemory::active == 0);
  HalMemory::fail = false;
  HalMemory::absent = true;
  assert(!replay());
  HalMemory::absent = false;
  record();
  const auto prior = bytes();
  {
    Cache c;
    c.begin(SOURCE, PATH, false);
    for (int i = 0; i < 257; ++i) c.startElement("h", nullptr);
    c.commit();
  }
  assert(bytes() == prior);
  {
    Cache c;
    c.begin(SOURCE, PATH, false);
    const char* attrs[67];
    for (int i = 0; i < 66; ++i) attrs[i] = "x";
    attrs[66] = nullptr;
    c.startElement("h", attrs);
    c.commit();
  }
  assert(bytes() == prior);
  {
    Cache c;
    c.begin(SOURCE, PATH, false);
    const char* attrs[65];
    for (int i = 0; i < 64; ++i) attrs[i] = "x";
    attrs[64] = nullptr;
    c.startElement("h", attrs);
    const std::string text(4096, 'x');
    c.text(Cache::Kind::Text, text.data(), int(text.size()));
    c.endElement("h");
    c.commit();
  }
  assert(replay());
  HalFile::failWrite = true;
  {
    Cache c;
    assert(!c.begin(SOURCE, PATH, false));
    c.commit();
  }
  HalFile::failWrite = false;
  Storage.failReplace = true;
  {
    Cache c;
    c.begin(SOURCE, PATH, false);
    c.startElement("h", nullptr);
    c.endElement("h");
    c.commit();
  }
  Storage.failReplace = false;
  HalFile::failClose = true;
  {
    Cache c;
    c.begin(SOURCE, PATH, false);
    c.startElement("h", nullptr);
    c.endElement("h");
    c.commit();
  }
  HalFile::failClose = false;
  assert(!Storage.exists("/events/0.evc.part"));
  assert(HalMemory::active == 0);
  puts("PASS: event replay, CRC/bounds, truncation, abort, writer lease, stale source, OOM and I/O failure");
}
