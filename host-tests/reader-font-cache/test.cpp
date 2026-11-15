#include <FtFont.h>
#include <TtfEpdFont.h>

#include <cstdio>
int main() {
  using namespace freeink::font;
  unsigned checks = 0, failures = 0;
  const auto check = [&](bool ok) {
    ++checks;
    failures += !ok;
  };
  TtfEpdFont font;
  const uint8_t source = 0;
  font.addResidentSource(0, &source, 1);
  check(font.load(36));
  const auto identity = font.metricsIdentity(1);
  check(identity != 0);
  check(font.metricsIdentity(2) == 0);
  auto family = font.family();
  rasterCalls = 0;
  check(font.addCoverage("ABC", 2));
  check(rasterCalls == 3);
  check(font.metricsIdentity(2) == identity);
  for (const char cp : {'A', 'B', 'C'}) check(family.getGlyph(cp, EpdFontFamily::BOLD) != nullptr);
  check(rasterCalls == 3);  // Bold scan prepared the actual face, not unused regular glyphs.
  check(font.addCoverage("BCD", 2));
  check(rasterCalls == 4);
  font.discardFailedGlyphs();
  check(font.addCoverage("ABCD", 2));
  check(rasterCalls == 4);
  check(font.addCoverage("ABC", 4));
  check(rasterCalls == 7);
  check(font.build("D", 1));
  check(rasterCalls == 8);
  check(font.addCoverage("ABCD", 2));
  check(rasterCalls == 8);  // Building regular preserves bold.
  kernCalls = 0;
  for (unsigned n = 0; n < 1000; ++n)
    check(family.getKerning(100 + n, 2000, EpdFontFamily::BOLD) == int((2100 + n) % 5));
  check(kernCalls == 1000);
  kernCalls = 0;
  for (unsigned n = 0; n < 1000; ++n)
    check(family.getKerning(100 + n, 2000, EpdFontFamily::BOLD) == int((2100 + n) % 5));
  check(kernCalls == 488);
  std::printf(
      "Vector cache: %u checks, %u failures; repeated varied page: %u FreeType kern calls (1000 without retention)\n",
      checks, failures, kernCalls);
  font.releaseResidentCaches();
  check(font.addCoverage("ABC", 2));
  check(rasterCalls == 11);
  check(font.load(36, true, 1024 * 1024, 4096));
  family = font.family();
  kernCalls = 0;
  for (unsigned n = 0; n < 1000; ++n) family.getKerning(100 + n, 2000, EpdFontFamily::BOLD);
  check(kernCalls == 1000);
  kernCalls = 0;
  for (unsigned n = 0; n < 1000; ++n) family.getKerning(100 + n, 2000, EpdFontFamily::BOLD);
  check(kernCalls == 0);
  std::printf("S3 reader budget: 0 repeated FreeType calls for 1000 distinct pairs; %u total checks, %u failures\n",
              checks, failures);
  return failures ? 1 : 0;
}
