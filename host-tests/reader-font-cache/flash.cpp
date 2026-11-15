#include <EpdFontFamily.h>
#include <builtinFonts/ubuntu_12_bold.h>
#include <builtinFonts/ubuntu_12_regular.h>

#include <cstdio>
#include <initializer_list>
int main() {
  EpdFont regular(&ubuntu_12_regular), bold(&ubuntu_12_bold);
  EpdFontFamily family(12, &regular, &bold);
  unsigned checks = 0, failures = 0;
  const auto check = [&](bool ok) {
    ++checks;
    failures += !ok;
  };
  for (auto style : {EpdFontFamily::REGULAR, EpdFontFamily::BOLD, EpdFontFamily::ITALIC, EpdFontFamily::BOLD_ITALIC}) {
    const auto* data = family.getData(style);
    check(data != nullptr);
    check(data->groups == nullptr);
    for (unsigned cp : {65u, 103u, 48u, 0xe1u, 0xe9u, 0xf1u}) {
      const auto* glyph = family.getGlyph(cp, style);
      check(glyph != nullptr);
      if (glyph) {
        check(glyph->advanceX > 0);
        check(glyph->width > 0);
        check(glyph->height > 0);
        check(data->bitmap != nullptr);
      }
    }
    for (const char* text : {"España English 0123456789", "office AV To", "áéíóú ü ñ ¿¡"}) {
      int width = 0, height = 0, warm = 0, wh = 0;
      family.getTextDimensions(text, &width, &height, style);
      family.getTextDimensions(text, &warm, &wh, style);
      check(width > 0 && height > 0);
      check(width == warm && height == wh);
    }
  }
  std::printf("Flash reader fallback: %u checks, %u failures (regular/bold and absent-style fallback)\n", checks,
              failures);
  return failures ? 1 : 0;
}
