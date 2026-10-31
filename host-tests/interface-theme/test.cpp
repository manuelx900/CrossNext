#include <cstdio>

#include "util/InterfaceThemeStyle.h"

int main() {
  using namespace interfacetheme;
  int checks = 0, failures = 0;
  const auto check = [&](bool condition) {
    ++checks;
    if (!condition) ++failures;
  };
  for (int home = 0; home < int(Style::Count); ++home)
    for (int general = 0; general < int(Override::Count); ++general)
      for (int section = 0; section < menufont::kSectionCount; ++section)
        for (int choice = 0; choice < int(Override::Count); ++choice) {
          const auto group = menufont::Section(section);
          const auto result = resolve(home, general, group, choice);
          const int expected = group == menufont::Section::Home                ? home
                               : group != menufont::Section::General && choice ? choice - 1
                               : general                                       ? general - 1
                                                                               : home;
          check(int(result) == expected);
          check(light(result) == (expected == 2 || expected == 3));
          for (int font = 0; font < int(menufont::Style::Count); ++font) {
            check(menufont::resolve(font, 0) == menufont::normalized(font));
            check(resolve(home, general, group, choice) == result);
          }
        }
  for (int invalid = int(Override::Count); invalid < 256; ++invalid) check(normalized(invalid) == Override::General);
  check(resolve(3, 1, menufont::Section::Home, 0) == Style::Licorice);
  check(resolve(3, 1, menufont::Section::Files, 0) == Style::Framed);
  check(resolve(3, 1, menufont::Section::Games, 3) == Style::Picco);
  check(resolve(3, 0, menufont::Section::Files, 0) == Style::Licorice);
  std::printf("Interface theme: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
