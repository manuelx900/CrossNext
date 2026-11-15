#include <HalStorage.h>
#include <I18n.h>
#include <I18nPackKeys.h>
#include <LanguagePack.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#define HEX 16
#include "../../src/apps_local/ui/WebUiScript.h"
#undef HEX

int main(int argc, char** argv) {
  if (argc != 5) return 2;
  const auto language = I18n::languageFromCode(argv[2]);
  if (language == Language::EN || language == Language::ES) {
    I18N.setLanguage(language);
  } else {
    const std::string filename = std::string(argv[2]) + ".cplang";
    std::ifstream in(std::string(argv[1]) + "/" + filename, std::ios::binary);
    if (!in) return 3;
    Storage.files["/languages/" + filename] = {std::istreambuf_iterator<char>(in), {}};
    if (!I18N.loadLanguagePack(filename.c_str())) return 4;
  }
  std::ifstream fixture(argv[3], std::ios::binary);
  std::vector<char> expected{std::istreambuf_iterator<char>(fixture), {}};
  size_t at = 0;
  for (unsigned id = 0; id < unsigned(StrId::_COUNT); ++id) {
    if (at >= expected.size()) return 5;
    if (std::strcmp(I18N.get(StrId(id)), expected.data() + at) && language != Language::OOK) {
      std::fprintf(stderr, "%s: lookup mismatch at %u\n", argv[2], id);
      return 6;
    }
    at += std::strlen(expected.data() + at) + 1;
  }
  const auto response = webui::script();
  if (!response.data || response.data[response.size]) return 7;
  std::ofstream out(argv[4], std::ios::binary);
  out.write(response.data.get(), response.size);
  std::printf("%s: %u lookups, %zu JS bytes\n", argv[2], unsigned(StrId::_COUNT), response.size);
}
