#include <HalMemory.h>
#include <PreparedFontPairs.h>
#include <TtfEpdFont.h>

#include <array>
#include <cstdio>
#include <cstring>

struct File {
  std::array<uint8_t, 52> bytes{};
  unsigned reads = 0;
  static unsigned long read(void* ctx, unsigned long offset, unsigned char* buffer, unsigned long count) {
    auto& f = *static_cast<File*>(ctx);
    ++f.reads;
    if (offset > f.bytes.size() || count > f.bytes.size() - offset) return 0;
    memcpy(buffer, f.bytes.data() + offset, count);
    return count;
  }
  void set(unsigned pos, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[pos + i] = static_cast<uint8_t>(value >> (24 - i * 8));
  }
  File() {
    set(0, 0x00010000);
    set(4, 0x00010000);
    set(12, 0x43504c47);
    set(20, 28);
    set(24, 24);
    set(28, 0x434e4c47);
    set(32, 0x00010002);
    set(36, 0x0e010e34);
    set(40, 0xe000);
    set(44, 0xe0000e48);
    set(48, 0xe001);
  }
};
int main() {
  unsigned failures = 0, checks = 0;
  auto check = [&](bool ok) {
    ++checks;
    if (!ok) {
      ++failures;
      std::printf("FAILED %u\n", checks);
    }
  };
  {
    File f;
    TtfEpdFont font;
    font.addStreamSource(0, File::read, &f, f.bytes.size());
    check(font.load(18));
    check(HalMemory::active == 1);
    const unsigned reads = f.reads, allocations = HalMemory::allocations;
    for (uint8_t style = 0; style < 4; ++style) {
      check(font.addCoverage("กิ่", 1u << style));
      const char* suffix = "ิ่";
      const EpdFont face(font.family().getData(static_cast<EpdFontFamily::Style>(style)));
      check(face.applyLigatures(0x0e01, suffix) == 0xe001 && *suffix == '\0');
    }
    check(f.reads == reads && HalMemory::allocations == allocations);
    font.releaseResidentCaches();
    check(font.addCoverage("กิ่", 0x0f));
    check(f.reads == reads && HalMemory::allocations == allocations);
  }
  check(HalMemory::active == 0);
  {
    File regular, bold;
    TtfEpdFont font;
    font.addStreamSource(0, File::read, &regular, regular.bytes.size());
    font.addStreamSource(1, File::read, &bold, bold.bytes.size());
    check(font.load(14));
    check(HalMemory::active == 2);
    const unsigned reads = bold.reads;
    // Measuring the first bold run needs its substitutions before FreeType faults it in.
    const char* suffix = "ิ่";
    check(font.family().applyLigatures(0x0e01, suffix, EpdFontFamily::BOLD) == 0xe001);
    check(font.addCoverage("กิ่", 0x0f));
    check(bold.reads == reads);
  }
  check(HalMemory::active == 0);
  {
    File f;
    TtfEpdFont font;
    font.addResidentSource(0, f.bytes.data(), f.bytes.size());
    HalMemory::fail = true;
    check(!font.load(12));
    HalMemory::fail = false;
    check(HalMemory::active == 0);
  }
  for (unsigned broken : {24u, 28u, 32u, 40u, 44u, 48u}) {
    File f;
    f.set(broken, 0xffffffffu);
    TtfEpdFont font;
    font.addStreamSource(0, File::read, &f, f.bytes.size());
    check(!font.load(12));
  }
  check(HalMemory::active == 0);
  std::printf("Prepared reader fonts: %u checks, %u failures\n", checks, failures);
  return failures ? 1 : 0;
}
