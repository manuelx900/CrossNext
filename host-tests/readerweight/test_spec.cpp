#include <Epub/ReaderRenderSpec.h>

#include <cstdio>
#include <initializer_list>
int main() {
  int checks = 0, failures = 0;
  for (int font : {1, -928381217, 840051567}) {
    for (int orientation = 0; orientation < 4; ++orientation) {
      ReaderRenderSpec regular;
      regular.fontId = font;
      regular.viewportWidth = orientation % 2 ? 780 : 460;
      regular.viewportHeight = orientation % 2 ? 440 : 760;
      regular.embeddedStyle = orientation < 2;
      regular.focusReadingEnabled = orientation % 2;
      ReaderRenderSpec bold = regular;
      bold.forceBold = true;
      const auto first = readerRenderSpecSignature(regular);
      const auto second = readerRenderSpecSignature(bold);
      ++checks;
      if (!first || !second || first == second) ++failures;
      bold.forceBold = false;
      ++checks;
      if (readerRenderSpecSignature(bold) != first) ++failures;
    }
  }
  std::printf("reader weight layout signature: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
