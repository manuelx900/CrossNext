#include <HalStorage.h>
#include <I18n.h>
#include <I18nPackKeys.h>
#include <I18nStrings.h>
#include <LanguagePack.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
uint64_t hash(const char* key) {
  uint64_t value = UINT64_C(14695981039346656037);
  while (*key) value = (value ^ static_cast<uint8_t>(*key++)) * UINT64_C(1099511628211);
  return value;
}
void put(std::vector<uint8_t>& out, size_t at, uint64_t value, size_t count) {
  for (size_t i = 0; i < count; ++i) out[at + i] = static_cast<uint8_t>(value >> (i * 8));
}
void checksum(std::vector<uint8_t>& out) {
  uint32_t crc = UINT32_MAX;
  for (size_t i = 112; i < out.size(); ++i) {
    crc ^= out[i];
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
  }
  put(out, 20, crc ^ UINT32_MAX, 4);
}
using Entry = std::pair<uint64_t, std::string>;
std::vector<uint8_t> pack(std::vector<Entry> entries = {{hash("STR_SETTINGS_TITLE"), "Réglages"}}) {
  std::sort(entries.begin(), entries.end());
  std::vector<uint8_t> out(112 + 16 * entries.size(), 0);
  memcpy(out.data(), "CPLANG01", 8);
  put(out, 8, 1, 2);
  put(out, 12, entries.size(), 4);
  memcpy(out.data() + 24, "fr", 3);
  memcpy(out.data() + 48, "Français", 10);
  size_t offset = 0;
  for (size_t i = 0; i < entries.size(); ++i) {
    put(out, 112 + i * 16, entries[i].first, 8);
    put(out, 120 + i * 16, offset, 4);
    put(out, 124 + i * 16, entries[i].second.size(), 4);
    out.insert(out.end(), entries[i].second.begin(), entries[i].second.end());
    out.push_back(0);
    offset += entries[i].second.size() + 1;
  }
  put(out, 16, offset, 4);
  checksum(out);
  return out;
}
class LanguagePackTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Storage.files.clear();
    Storage.reads = Storage.opens = 0;
    Storage.enumerateError = false;
    HalMemory::fail = false;
  }
  void TearDown() override { HalMemory::fail = false; }
  bool load(std::vector<uint8_t> bytes) {
    Storage.files["/languages/fr.cplang"] = std::move(bytes);
    languagepack::Pack value;
    return value.load("fr.cplang");
  }
};
TEST_F(LanguagePackTest, StableKeysAndDirectLookupWithoutStorageReads) {
  Storage.files["/languages/fr.cplang"] = pack();
  languagepack::Pack value;
  ASSERT_TRUE(value.load("fr.cplang"));
  EXPECT_STREQ(value.get(static_cast<size_t>(StrId::STR_SETTINGS_TITLE)), "Réglages");
  EXPECT_EQ(value.get(static_cast<size_t>(StrId::STR_LOADING_POPUP)), nullptr);
  const auto reads = Storage.reads, opens = Storage.opens;
  for (int i = 0; i < 10000; ++i) ASSERT_NE(value.get(static_cast<size_t>(StrId::STR_SETTINGS_TITLE)), nullptr);
  EXPECT_EQ(Storage.reads, reads);
  EXPECT_EQ(Storage.opens, opens);
  EXPECT_FALSE(value.load("fr.cplang"));
  EXPECT_STREQ(value.get(static_cast<size_t>(StrId::STR_SETTINGS_TITLE)), "Réglages");
}
TEST_F(LanguagePackTest, NewUnknownKeysRemainForwardCompatible) {
  EXPECT_TRUE(load(pack({{hash("STR_FUTURE_KEY"), "Future"}})));
}
TEST_F(LanguagePackTest, EmptySparsePackValid) { EXPECT_TRUE(load(pack({}))); }
TEST_F(LanguagePackTest, CompleteNarrativePackAbove640KiBKeepsDirectLookup) {
  std::vector<Entry> entries;
  entries.reserve(131);
  entries.push_back({hash("STR_SETTINGS_TITLE"), "Réglages"});
  for (uint64_t i = 0; i < 130; ++i) entries.push_back({i + 1, std::string(8000, 'a')});
  Storage.files["/languages/fr.cplang"] = pack(std::move(entries));
  languagepack::Pack value;
  ASSERT_TRUE(value.load("fr.cplang"));
  EXPECT_GT(value.info().textBytes, 640u * 1024);
  EXPECT_LE(value.info().textBytes, languagepack::MAX_TEXT_BYTES);
  EXPECT_STREQ(value.get(static_cast<size_t>(StrId::STR_SETTINGS_TITLE)), "Réglages");
  const auto reads = Storage.reads;
  EXPECT_NE(value.get(static_cast<size_t>(StrId::STR_SETTINGS_TITLE)), nullptr);
  EXPECT_EQ(Storage.reads, reads);
}
TEST_F(LanguagePackTest, UpdatedTextBudgetRemainsBounded) {
  constexpr size_t entriesToExceedLimit = languagepack::MAX_TEXT_BYTES / 8001 + 1;
  std::vector<Entry> entries;
  entries.reserve(entriesToExceedLimit);
  for (size_t i = 0; i < entriesToExceedLimit; ++i) entries.push_back({i + 1, std::string(8000, 'a')});
  const auto alive = HalMemory::alive;
  EXPECT_FALSE(load(pack(std::move(entries))));
  EXPECT_EQ(HalMemory::alive, alive);
}
TEST_F(LanguagePackTest, RejectsEveryTruncation) {
  const auto valid = pack();
  for (size_t length = 0; length < valid.size(); ++length)
    EXPECT_FALSE(load({valid.begin(), valid.begin() + length})) << length;
}
TEST_F(LanguagePackTest, ChecksumRejectsPayloadCorruption) {
  auto bytes = pack();
  bytes.back() = 1;
  EXPECT_FALSE(load(bytes));
}
TEST_F(LanguagePackTest, RejectsWrongVersionFlagsAndOversizedLengths) {
  for (auto offset : {8, 10, 12, 16}) {
    auto bytes = pack();
    put(bytes, offset, UINT32_MAX, offset < 12 ? 2 : 4);
    EXPECT_FALSE(load(bytes));
  }
}
TEST_F(LanguagePackTest, RejectsOffsetsAndEmbeddedNul) {
  auto bytes = pack();
  put(bytes, 120, 1, 4);
  checksum(bytes);
  EXPECT_FALSE(load(bytes));
  bytes = pack();
  bytes[129] = 0;
  checksum(bytes);
  EXPECT_FALSE(load(bytes));
}
TEST_F(LanguagePackTest, RejectsDuplicateHashes) {
  EXPECT_FALSE(load(pack({{hash("STR_SETTINGS_TITLE"), "A"}, {hash("STR_SETTINGS_TITLE"), "B"}})));
}
TEST_F(LanguagePackTest, RejectsInvalidUtf8AndControlCharacters) {
  for (const auto& text : {std::string("\xc0\xaf", 2), std::string("\xed\xa0\x80", 3),
                           std::string("\xf4\x90\x80\x80", 4), std::string("\x01", 1)})
    EXPECT_FALSE(load(pack({{hash("STR_SETTINGS_TITLE"), text}})));
}
TEST_F(LanguagePackTest, RejectsUnsafeMetadata) {
  auto bytes = pack();
  memset(bytes.data() + 24, 'a', 24);
  EXPECT_FALSE(load(bytes));
  bytes = pack();
  bytes[24] = '/';
  EXPECT_FALSE(load(bytes));
  bytes = pack();
  memset(bytes.data() + 48, 'a', 64);
  EXPECT_FALSE(load(bytes));
}
TEST_F(LanguagePackTest, NoInternalRamFallbackAndNoLeakOnFailure) {
  const auto alive = HalMemory::alive;
  HalMemory::fail = true;
  EXPECT_FALSE(load(pack()));
  HalMemory::fail = false;
  auto bytes = pack();
  bytes[20] ^= 1;
  EXPECT_FALSE(load(bytes));
  EXPECT_EQ(HalMemory::alive, alive);
}
TEST_F(LanguagePackTest, MissingFileAndTraversalRejected) {
  languagepack::Pack value;
  EXPECT_FALSE(value.load("missing.cplang"));
  for (const auto* name : {"../fr.cplang", "/fr.cplang", "fr..cplang", "fr.txt", "fr\\x.cplang"})
    EXPECT_FALSE(value.load(name));
  EXPECT_TRUE(languagepack::validFilename("pt-BR.cplang"));
}
TEST_F(LanguagePackTest, FormatArgumentOrderAndTypes) {
  EXPECT_TRUE(languagepack::formatsMatch("Go to %", "Ir al %"));
  EXPECT_TRUE(languagepack::formatsMatch("Progress: 100%?", "Avance: 100%?"));
  EXPECT_FALSE(languagepack::formatsMatch("Go to %", "Ir al %n"));
  EXPECT_FALSE(languagepack::formatsMatch("Go to %", "Ir al %s"));
  EXPECT_FALSE(languagepack::formatsMatch("Go to %",
                                          "Ir al %"
                                          " %d"));
  EXPECT_TRUE(languagepack::formatsMatch("%06lu %s %%", "%lu : %20s %%"));
  EXPECT_TRUE(languagepack::formatsMatch("%.*s %zu", "%.*s %zu"));
  for (const auto* unsafe : {"%n", "%s", "%u", "%1$lu", "%*lu", "%", "%ls"})
    EXPECT_FALSE(languagepack::formatsMatch("%lu", unsafe));
  EXPECT_FALSE(languagepack::formatsMatch("%s %u", "%u %s"));
  EXPECT_FALSE(languagepack::formatsMatch("%.*s", "%s"));
}
TEST_F(LanguagePackTest, RuntimeRejectsFormatEvenWithValidChecksum) {
  EXPECT_FALSE(load(pack({{hash("STR_BT_PAIRING_CODE"), "code %s"}})));
}
TEST_F(LanguagePackTest, TranslatedLiteralPercentLabel) {
  EXPECT_TRUE(load(pack({{hash("STR_GO_TO_PERCENT"), "Ir al %"}})));
  EXPECT_FALSE(load(pack({{hash("STR_GO_TO_PERCENT"), "Ir al %s"}})));
}
TEST_F(LanguagePackTest, CatalogSkipsInvalidFilesAndSortsCodes) {
  Storage.files["/languages/z.cplang"] = pack();
  Storage.files["/languages/a.cplang"] = pack();
  Storage.files["/languages/bad.cplang"] = {1, 2, 3};
  Storage.files["/languages/template.csv"] = {1};
  languagepack::Catalog catalog;
  ASSERT_TRUE(catalog.scan());
  ASSERT_EQ(catalog.size(), 2u);
  EXPECT_STREQ(catalog[0].filename, "a.cplang");
  catalog.clear();
  EXPECT_EQ(catalog.size(), 0u);
}
TEST_F(LanguagePackTest, CatalogHandlesIoFailureAndOom) {
  Storage.files["/languages/fr.cplang"] = pack();
  languagepack::Catalog catalog;
  Storage.enumerateError = true;
  EXPECT_FALSE(catalog.scan());
  EXPECT_EQ(catalog.size(), 0u);
  Storage.enumerateError = false;
  HalMemory::fail = true;
  EXPECT_FALSE(catalog.scan());
  EXPECT_EQ(catalog.size(), 0u);
}
TEST_F(LanguagePackTest, RejectsUnicodeFormatDigitsAndUnboundedNumericFields) {
  for (const auto* unsafe : {"%٠٦lu", "%999999999lu", "%129lu", "%.129lu"}) {
    EXPECT_FALSE(languagepack::formatsMatch("%lu", unsafe));
    EXPECT_FALSE(load(pack({{hash("STR_BT_PAIRING_CODE"), unsafe}})));
  }
  EXPECT_FALSE(load(pack({{hash("STR_PAGE_OVERALL_FORMAT"), "Page %d, %.100000000f%% overall"}})));
  EXPECT_TRUE(languagepack::formatsMatch("%lu", "%128lu"));
}
TEST_F(LanguagePackTest, RegionalPacksKeepKnownPrimaryLanguageBehavior) {
  EXPECT_EQ(I18n::languageFromCode("ar-EG"), Language::AR);
  EXPECT_EQ(I18n::languageFromCode("fa-IR"), Language::FA);
  EXPECT_EQ(I18n::languageFromCode("es-MX"), Language::ES);
  EXPECT_EQ(I18n::languageFromCode("pt-AO"), Language::PT);
  EXPECT_EQ(I18n::languageFromCode("sl-SI"), Language::SI);
  EXPECT_EQ(I18n::languageFromCode("pt-PT"), Language::P2);
  EXPECT_EQ(I18n::languageFromCode("fr"), Language::FR);
  EXPECT_EQ(I18n::languageFromCode(nullptr), Language::EN);
}
TEST_F(LanguagePackTest, AllCompiledRepositoryPacksLoad) {
  const char* folder = std::getenv("CROSSNEXT_LANGUAGE_PACK_FIXTURES");
  if (!folder) GTEST_SKIP() << "Set CROSSNEXT_LANGUAGE_PACK_FIXTURES to a language_packs.py bundle directory";
  for (size_t index = 0; index < getLanguageCount(); ++index) {
    if (index == static_cast<size_t>(Language::EN) || index == static_cast<size_t>(Language::ES)) continue;
    const std::string filename = std::string(LANGUAGE_TAGS[index]) + ".cplang";
    std::ifstream file(std::string(folder) + "/" + filename, std::ios::binary);
    ASSERT_TRUE(file.is_open()) << filename;
    Storage.files["/languages/" + filename] = std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
    languagepack::Pack value;
    ASSERT_TRUE(value.load(filename.c_str())) << filename;
  }
}
TEST_F(LanguagePackTest, SingletonUsesEnglishForMissingTranslationsAndKeepsBorrowedPointers) {
  Storage.files["/languages/fr.cplang"] = pack();
  I18N.setLanguage(Language::ES);
  ASSERT_TRUE(I18N.loadLanguagePack("fr.cplang"));
  EXPECT_STREQ(I18N.getLanguageName(), "Français");
  EXPECT_EQ(I18N.getLanguage(), Language::FR);
  const char* borrowed = tr(STR_SETTINGS_TITLE);
  EXPECT_STREQ(borrowed, "Réglages");
  EXPECT_STREQ(tr(STR_LOADING_POPUP),
               i18n_strings::STRINGS_EN_DATA + i18n_strings::OFFSETS_EN[static_cast<size_t>(StrId::STR_LOADING_POPUP)]);
  EXPECT_FALSE(I18N.loadLanguagePack("other.cplang"));
  EXPECT_STREQ(borrowed, "Réglages");
  I18N.setLanguage(Language::ES);
  EXPECT_STREQ(borrowed, "Réglages");
  EXPECT_FALSE(I18N.hasLanguagePack());
  EXPECT_EQ(I18n::languageFromCode("pt-BR"), Language::PT);
  EXPECT_EQ(I18n::languageFromCode("pt-PT"), Language::P2);
}
}  // namespace
