#include <OpdsFeedParser.h>
#include <OpdsSearchTemplate.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>

#include "util/OpdsPolicy.h"
#include "util/UrlUtils.h"

namespace {
void feed(OpdsFeedParser& parser, const std::string& body, size_t chunk = 1) {
  for (size_t i = 0; i < body.size(); i += chunk)
    EXPECT_EQ(parser.write(reinterpret_cast<const uint8_t*>(body.data() + i), std::min(chunk, body.size() - i)),
              std::min(chunk, body.size() - i));
  parser.flush();
}
const char* publication =
    R"({"metadata":{"title":{"en":"Test book","es":"Libro de prueba"},"author":[{"name":"Ana"},"Luis"]},"links":[{"rel":"http://opds-spec.org/acquisition/open-access","type":"application/epub+zip","href":"books/test.epub"}]})";
}  // namespace
TEST(OpdsFeed, DetectsJsonWithSplitBomAndWhitespace) {
  OpdsFeedParser parser;
  parser.setPreferredLanguage("es");
  feed(parser, std::string("\xEF\xBB\xBF \n{") + "\"publications\":[" + publication + "]}");
  ASSERT_FALSE(parser.error());
  EXPECT_TRUE(parser.isOpds2());
  auto entries = parser.takeEntries();
  ASSERT_EQ(entries.size(), 1);
  EXPECT_EQ(entries[0].title, "Libro de prueba");
  EXPECT_EQ(entries[0].href, "books/test.epub");
  EXPECT_EQ(entries[0].type, OpdsEntryType::BOOK);
  EXPECT_NE(entries[0].author.find("Ana"), std::string::npos);
}
TEST(OpdsFeed, KeepsAtomNavigationAndBooks) {
  OpdsFeedParser parser;
  feed(
      parser,
      R"(<?xml version="1.0"?><feed xmlns="http://www.w3.org/2005/Atom"><link rel="next" href="?page=2"/><entry><title>Catalog</title><link type="application/atom+xml" href="/catalog"/></entry><entry><title>Book</title><author><name>Author</name></author><link rel="http://opds-spec.org/acquisition" type="application/epub+zip" href="/book.epub"/></entry></feed>)",
      7);
  ASSERT_FALSE(parser.error());
  EXPECT_FALSE(parser.isOpds2());
  EXPECT_EQ(parser.getNextPageUrl(), "?page=2");
  auto entries = parser.takeEntries();
  ASSERT_EQ(entries.size(), 2);
  EXPECT_EQ(entries[0].type, OpdsEntryType::NAVIGATION);
  EXPECT_EQ(entries[1].author, "Author");
}
TEST(OpdsFeed, ParsesNavigationGroupsSearchAndPagination) {
  OpdsFeedParser parser;
  feed(
      parser,
      std::string(
          R"({"navigation":[{"title":"All books","href":"/all","type":"application/opds+json"}],"groups":[{"metadata":{"title":"New"},"publications":[)") +
          publication +
          R"(]}],"links":[{"rel":"next","href":"?page=2"},{"rel":"previous","href":"?page=0"},{"rel":"search","href":"/search{?query}","templated":true}]})",
      11);
  ASSERT_FALSE(parser.error());
  EXPECT_EQ(parser.getNextPageUrl(), "?page=2");
  EXPECT_EQ(parser.getPrevPageUrl(), "?page=0");
  EXPECT_EQ(parser.getSearchTemplate(), "/search{?query}");
  auto entries = parser.takeEntries();
  EXPECT_EQ(entries.size(), 2);
  EXPECT_TRUE(std::any_of(entries.begin(), entries.end(), [](const auto& e) { return e.title == "Test book"; }));
}
TEST(OpdsFeed, DecodesEscapedUnicode) {
  OpdsFeedParser parser;
  feed(parser, R"({"navigation":[{"title":"Caf\u00e9 \ud83d\udcd6","href":"/books","type":"application/opds+json"}]})");
  ASSERT_FALSE(parser.error());
  auto entries = parser.takeEntries();
  ASSERT_EQ(entries.size(), 1);
  EXPECT_EQ(entries[0].title, "Café 📖");
}
TEST(OpdsFeed, RejectsIncompleteJson) {
  OpdsFeedParser parser;
  feed(parser, "{\"publications\":[");
  EXPECT_TRUE(parser.error());
}
TEST(OpdsFeed, RejectsMalformedJsonAndEmptyBody) {
  OpdsFeedParser parser;
  feed(parser, "{]}broken");
  EXPECT_TRUE(parser.error());
  parser.reset();
  feed(parser, "   ");
  EXPECT_TRUE(parser.error());
}
TEST(OpdsFeed, BoundsEntriesAcrossLargeFeed) {
  OpdsFeedParser parser;
  std::string text = "{\"publications\":[";
  text.reserve(80000);
  for (int i = 0; i < 200; ++i) {
    if (i) text += ',';
    text += publication;
  }
  text += "]}";
  feed(parser, text, 1024);
  ASSERT_FALSE(parser.error());
  EXPECT_TRUE(parser.truncated());
  EXPECT_LE(parser.takeEntries().size(), OpdsLimits::MAX_ENTRIES);
}
TEST(OpdsFeed, ResetAllowsDifferentBackend) {
  OpdsFeedParser parser;
  feed(parser, "{\"navigation\":[]}");
  ASSERT_FALSE(parser.error());
  parser.reset();
  feed(parser, "<feed/>");
  EXPECT_FALSE(parser.error());
  EXPECT_FALSE(parser.isOpds2());
}
TEST(OpdsFeed, DistinguishesProtectedAndIndirectAcquisitions) {
  OpdsFeedParser parser;
  feed(
      parser,
      R"({"publications":[{"metadata":{"title":"Indirect"},"links":[{"rel":"http://opds-spec.org/acquisition","type":"application/opds-publication+json","href":"/indirect"}]},{"metadata":{"title":"License"},"links":[{"rel":"http://opds-spec.org/acquisition","type":"application/vnd.readium.lcp.license.v1.0+json","href":"/license"}]}]})");
  ASSERT_FALSE(parser.error());
  auto entries = parser.takeEntries();
  ASSERT_EQ(entries.size(), 2);
  EXPECT_TRUE(entries[0].indirect);
  EXPECT_TRUE(entries[1].lcpLicense);
}
TEST(OpdsSearch, ExpandsJsonAndAtomTemplates) {
  EXPECT_EQ(expandOpdsSearchTemplate("/search{?query}", "caf%C3%A9"), "/search?query=caf%C3%A9");
  EXPECT_EQ(expandOpdsSearchTemplate("/search?q={searchTerms}&page={startPage?}", "test"), "/search?q=test&page=");
}

TEST(OpdsFeed, OmitsSelfOnlyAndProtectedPublications) {
  OpdsFeedParser parser;
  feed(
      parser,
      std::string(
          R"({"publications":[{"metadata":{"title":"Details only"},"links":[{"rel":"self","type":"application/opds-publication+json","href":"/publication"}]},)") +
          publication + "]}");
  ASSERT_FALSE(parser.error());
  const auto entries = parser.takeEntries();
  ASSERT_EQ(entries.size(), 2);
  EXPECT_TRUE(entries[0].href.empty());
  EXPECT_FALSE(canBrowseOpdsEntry(entries[0]));
  EXPECT_TRUE(canBrowseOpdsEntry(entries[1]));
  auto book = entries[1];
  book.purchase = true;
  EXPECT_FALSE(canBrowseOpdsEntry(book));
  book.purchase = false;
  book.indirect = true;
  EXPECT_FALSE(canBrowseOpdsEntry(book));
  book.indirect = false;
  book.lcpLicense = true;
  EXPECT_FALSE(canBrowseOpdsEntry(book));
}

TEST(OpdsUrls, ResolvesReferencesAgainstTheCurrentDocument) {
  const std::string base = "https://catalog.test/opds/feed.json?old=1#top";
  EXPECT_EQ(UrlUtils::resolveReference(base, "?page=2"), "https://catalog.test/opds/feed.json?page=2");
  EXPECT_EQ(UrlUtils::resolveReference(base, "books/test.epub"), "https://catalog.test/opds/books/test.epub");
  EXPECT_EQ(UrlUtils::resolveReference(base, "../books/test.epub"), "https://catalog.test/books/test.epub");
  EXPECT_EQ(UrlUtils::resolveReference(base, "/books/./a/../test.epub"), "https://catalog.test/books/test.epub");
  EXPECT_EQ(UrlUtils::resolveReference(base, "//cdn.test/test.epub"), "https://cdn.test/test.epub");
  EXPECT_EQ(UrlUtils::resolveReference(base, "#toc"), "https://catalog.test/opds/feed.json?old=1#toc");
  EXPECT_EQ(UrlUtils::resolveReference(base, ""), "https://catalog.test/opds/feed.json?old=1");
  EXPECT_EQ(UrlUtils::resolveReference("https://catalog.test", "feed.json"), "https://catalog.test/feed.json");
  EXPECT_EQ(UrlUtils::resolveReference("https://catalog.test?lang=es", "?page=2"), "https://catalog.test/?page=2");
  EXPECT_EQ(UrlUtils::resolveReference(base, "./"), "https://catalog.test/opds/");
  EXPECT_EQ(UrlUtils::resolveReference(base, "../../../../test.epub"), "https://catalog.test/test.epub");
  EXPECT_EQ(UrlUtils::resolveReference(base, "../search?q=libro nuevo"), "https://catalog.test/search?q=libro%20nuevo");
}

TEST(OpdsUrls, SearchResolvesAtTheCurrentCatalog) {
  EXPECT_EQ(UrlUtils::resolveReference("https://catalog.test/genres/history/feed.json",
                                       expandOpdsSearchTemplate("search{?query}", "caf%C3%A9")),
            "https://catalog.test/genres/history/search?query=caf%C3%A9");
}
