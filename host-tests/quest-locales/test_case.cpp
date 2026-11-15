#include <cassert>
#include <cstdio>
#include <cstring>
#include <utility>

#include "../../src/apps_local/ui/PixelText.h"

int main() {
  char out[64];
  for (const auto& pair :
       {std::pair{"Zpět", "ZPĚT"}, std::pair{"Zmień", "ZMIEŃ"}, std::pair{"Книга", "КНИГА"},
        std::pair{"Українська", "УКРАЇНСЬКА"}, std::pair{"Đọc", "ĐỌC"}, std::pair{"Straße", "STRAẞE"}}) {
    pixelui::copyCaps(out, sizeof(out), pair.first);
    assert(!std::strcmp(out, pair.second));
    std::strcpy(out, pair.first);
    pixelui::copyCaps(out, sizeof(out), out);
    assert(!std::strcmp(out, pair.second));
  }
  pixelui::turkicCase = true;
  assert(pixelui::copyCaps(out, sizeof(out), "isim"));
  assert(!std::strcmp(out, "İSİM"));
  std::strcpy(out, "ıi");
  pixelui::copyCaps(out, sizeof(out), out);
  assert(!std::strcmp(out, "Iİ"));
  std::strcpy(out, "iı");
  pixelui::copyCaps(out, sizeof(out), out);
  assert(!std::strcmp(out, "İI"));
  char bounded[4] = "iii";
  pixelui::copyCaps(bounded, sizeof(bounded), bounded);
  assert(!std::strcmp(bounded, "İ"));
  pixelui::uppercaseLabels = false;
  pixelui::preserveTranslatedCase = true;
  pixelui::copyNatural(out, sizeof(out), "ZMIEŃ");
  assert(!std::strcmp(out, "ZMIEŃ"));
  std::puts("UI case: Unicode, Turkic, aliases, capacity and translated case verified");
}
