#include <FreeInkUIGfxRenderer.h>
#include <I18n.h>
#include <I18nStrings.h>

#include <cassert>
#include <cctype>
#include <cstdio>

#include "../../src/apps_local/ui/FitText.h"
static std::string words(const std::string& value) {
  std::string result;
  for (unsigned char c : value)
    if (!std::isspace(c)) result += c;
  return result;
}
int main() {
  const char* copy = i18n_strings::STRINGS_EN_DATA + i18n_strings::OFFSETS_EN[size_t(StrId::STR_MYST_CREDIT_COPY)];
  GfxRenderer renderer;
  freeink::ui::GfxRendererTarget target(renderer);
  for (int id = 0; id < 6; ++id) target.setFont(id, id);
  const auto box = freeink::ui::makeRect(20, 100, 426, 240);
  assert(renderer.getTextWidth(4, copy) > box.width);
  for (bool styled : {false, true}) {
    renderer.drawn.clear();
    renderer.paragraphWraps = 0;
    if (styled) {
      freeink::ui::TextStyle style;
      style.font = 4;
      fittext::draw(target, box, copy, style, 10);
    } else
      fittext::draw(target, box, copy, 4, 10);
    std::string rendered;
    for (const auto& line : renderer.drawn) {
      assert(line.find('\n') == std::string::npos);
      assert(line != "TRUNCATED");
      rendered += line;
    }
    assert(renderer.paragraphWraps == 0);
    assert(words(rendered) == words(copy));
  }
  std::puts("Production GfxRendererTarget: complete Intercept credits, explicit newlines, both fitter overloads");
}
