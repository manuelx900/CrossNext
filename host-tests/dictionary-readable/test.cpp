#include <Arduino.h>
#include <HalStorage.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "../../src/apps_local/words/DictionaryHtml.h"
#include "../../src/util/DictZip.h"
#include "../../src/util/Dictionary.h"
#include "../../src/util/DictionaryRegistry.h"

namespace DictionaryRegistry {
bool resolveBasePath(const char*, std::string& out) {
  out = "/test";
  return true;
}
}  // namespace DictionaryRegistry
namespace DictZip {
bool extractEntry(const char* path, uint32_t offset, uint32_t size, HalFile& out, ExtractError*) {
  const auto& bytes = testStorage.files.at(path);
  return offset <= bytes.size() && size <= bytes.size() - offset && out.write(bytes.data() + offset, size) == size;
}
}  // namespace DictZip
static unsigned checks = 0;
#define CHECK(x)                                  \
  do {                                            \
    ++checks;                                     \
    if (!(x)) {                                   \
      std::printf("FAIL %d: %s\n", __LINE__, #x); \
      std::exit(1);                               \
    }                                             \
  } while (0)

void be32(std::vector<uint8_t>& out, uint32_t value) {
  for (int n = 3; n >= 0; --n) out.push_back(uint8_t(value >> (8 * n)));
}
void install(bool compressed = false, bool html = true, bool large = false) {
  testStorage = TestStorage{};
  testStorage.directories.insert("/.crosspoint");
  auto& index = testStorage.files["/test.idx"];
  auto& data = testStorage.files[compressed ? "/test.dict.dz" : "/test.dict"];
  const std::string ifo = std::string("sametypesequence=") + (html ? "h\n" : "m\n");
  testStorage.files["/test.ifo"] = {ifo.begin(), ifo.end()};
  const auto add = [&](const std::string& word, const std::string& text) {
    index.insert(index.end(), word.begin(), word.end());
    index.push_back(0);
    // Deliberately non-contiguous definitions: index adjacency is not data adjacency.
    data.insert(data.end(), 13, 'x');
    be32(index, uint32_t(data.size()));
    be32(index, uint32_t(text.size()));
    data.insert(data.end(), text.begin(), text.end());
  };
  for (unsigned n = 0; n < 254; ++n) {
    char word[16];
    std::snprintf(word, sizeof(word), "aaa%03u", n);
    add(word, "before");
  }
  add("country", large ? std::string(40000, 'a') : "adjective: campestre");
  add("country", large ? std::string(40000, 'b') : "noun: país");
  add("country", "verb: third sense");
  add("later", "after");
  auto& syn = testStorage.files["/test.syn"];
  const std::string alias = "nations";
  syn.insert(syn.end(), alias.begin(), alias.end());
  syn.push_back(0);
  be32(syn, 255);
}
void lookupTests(bool compressed, bool html) {
  install(compressed, html);
  Dictionary dictionary;
  CHECK(dictionary.open("test"));
  CHECK(dictionary.buildIndex());
  testStorage.readCalls = testStorage.seekCalls = testStorage.bytesRead = 0;
  std::string value = "stale", headword;
  Dictionary::LookupResult result;
  CHECK(dictionary.lookup("country", value, headword, &result));
  CHECK(result == Dictionary::LookupResult::Found);
  CHECK(headword == "country");
  CHECK(value.find("campestre") != value.npos && value.find("país") != value.npos &&
        value.find("third sense") != value.npos);
  CHECK(value.find(html ? "<hr/>" : "\n\n") != value.npos);
  CHECK(value.find("stale") == value.npos && value.find("before") == value.npos && value.find("after") == value.npos);
  std::printf("lookup %d/%d: %zu reads, %zu seeks, %zu bytes\n", compressed, html, testStorage.readCalls,
              testStorage.seekCalls, testStorage.bytesRead);
  const auto expected = value;
  CHECK(dictionary.lookup("nations", value, headword) && value == expected);
  CHECK(dictionary.lookup("countries", value, headword) && value == expected);
  CHECK(!dictionary.lookup("absent", value, headword) && value.empty());
  CHECK(dictionary.lookup("later", value, headword) && value == "after");
}
void htmlTests() {
  std::string country =
      "<div>/<font color=\"gray\">ˈkʌntɹi</font>/<br><div><font class=\"grammar\">noun</font></div>"
      "<ol><li>nation state<div>país</div></li></ol></div>";
  const char* allocation = country.data();
  vocabulary::prepareDictionaryHtml(country);
  CHECK(country.data() == allocation);
  CHECK(country.find("ˈkʌntɹi") == country.npos);
  CHECK(country.find("nation state<p>país</p>") != country.npos);
  std::string where =
      "<h4>Preposition</h4><ul><li>(UK) IPA: <span class=\"IPA\">/ʍɛə(r)/</span></li>"
      "<li>(US) <span title=\"SAMPA pronunciation\">/wEr/</span></li></ul>"
      "<ol><li>In what place?<dl><dd><i>Where do you live?</i></dd></dl></li></ol>";
  vocabulary::prepareDictionaryHtml(where);
  CHECK(where.find("IPA") == where.npos && where.find("SAMPA") == where.npos);
  CHECK(where.find("In what place?") != where.npos && where.find("<p><i>Where do you live?</i></p>") != where.npos);
  std::string ello =
      "<ul><li><b>yeísta</b> (<a title='Alfabeto Fonético Internacional'>AFI</a>): <span class='IPA'>[ˈeʝo]</span></li>"
      "<li><b>no yeísta</b> (<a title='Alfabeto Fonético Internacional'>AFI</a>): <span class='IPA'>[ˈeʎo]</span></li>"
      "<li><b>silabación</b>: e-llo</li></ul><dl><dt>1</dt><dd>Nominativo del pronombre personal.</dd></dl>";
  vocabulary::prepareDictionaryHtml(ello);
  CHECK(ello.find("IPA") == ello.npos && ello.find("AFI") == ello.npos);
  CHECK(ello.find("silabación") != ello.npos && ello.find("e-llo") != ello.npos);
  CHECK(ello.find("Nominativo del pronombre personal.") != ello.npos);
  std::string ordinary =
      "<ul><li>first meaning</li><li>Text about <span class=\"IPA\">IPA</span></li></ul>"
      "<p>par<b>ti</b>do &lt;example&gt;</p>";
  const auto original = ordinary;
  vocabulary::prepareDictionaryHtml(ordinary);
  CHECK(ordinary == original);
  std::string nested = "<ul><li>meaning <ul><li><span class='IPA'>/test/</span></li></ul></li></ul>";
  vocabulary::prepareDictionaryHtml(nested);
  CHECK(nested.find("meaning") != nested.npos);
  std::string broken = "<ul><li><span class=\"IPA\">broken";
  const auto originalBroken = broken;
  vocabulary::prepareDictionaryHtml(broken);
  CHECK(broken == originalBroken);
  std::string styled = "<style>.bad {}</style><script>bad()</script><DIV title='a > b'>sí</DIV>";
  vocabulary::prepareDictionaryHtml(styled);
  CHECK(styled == "<p>sí</p>");
}
void wordBoundaries() {
  for (const size_t length : {1u, 63u, 64u, 65u, 254u, 255u}) {
    install();
    auto& index = testStorage.files["/test.idx"];
    index.clear();
    const std::string target(length, 'a');
    index.insert(index.end(), target.begin(), target.end());
    index.push_back(0);
    be32(index, 0);
    be32(index, 4);
    testStorage.files["/test.dict"] = {'p', 'a', 's', 's'};
    Dictionary dict;
    CHECK(dict.open("test"));
    CHECK(dict.buildIndex());
    std::string body, word;
    CHECK(dict.lookup(target.c_str(), body, word));
    CHECK(body == "pass" && word == target);
  }
}
int main() {
  htmlTests();
  wordBoundaries();
  lookupTests(false, false);
  lookupTests(false, true);
  lookupTests(true, true);
  install(false, true, true);
  Dictionary dictionary;
  CHECK(dictionary.open("test"));
  CHECK(dictionary.buildIndex());
  std::string value, headword;
  CHECK(dictionary.lookup("country", value, headword) && value.size() == Dictionary::MAX_DEFINITION_BYTES);
  CHECK(value.substr(40000, 5) == "<hr/>" && value.back() == 'b');
  std::string empty;
  ESP.bytes = 32;
  Dictionary::LookupResult result;
  CHECK(!dictionary.lookup("country", empty, headword, &result));
  CHECK(result == Dictionary::LookupResult::LowMemory);
  ESP.bytes = 4 * 1024 * 1024;
  install();
  CHECK(dictionary.open("test"));
  testStorage.files["/test.dict"].resize(254 * (13 + 6) + 13 + 19);
  CHECK(!dictionary.lookup("country", value, headword, &result));
  CHECK(value.empty());
  std::printf("PASS: %u dictionary HTML, duplicate/synonym/boundary and resource checks\n", checks);
}
