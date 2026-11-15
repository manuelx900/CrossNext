#include <JsonListParser.h>
#include <PluginCatalogPaging.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

using plugincatalogpaging::Paging;
using Headers = std::vector<std::pair<std::string, std::string>>;

TEST(PluginCatalogPaging, DetectsActiveUrlBodyAndHeaderTemplates) {
  const auto pagingOf = [](std::string_view url, std::string_view body, const Headers& headers) {
    plugincatalogpaging::RequestVariables variables;
    variables.inspect(url);
    variables.inspect(body);
    for (const auto& header : headers) variables.inspect(header.second);
    return variables.paging();
  };
  EXPECT_EQ(pagingOf("https://example/catalog.json", "", Headers{}), Paging::Static);
  EXPECT_EQ(pagingOf("https://example?q={query}&limit={limit}", "", Headers{}), Paging::Static);
  EXPECT_EQ(pagingOf("https://example?page={page}", "", Headers{}), Paging::PageNumber);
  EXPECT_EQ(pagingOf("https://example?page={page}&limit={limit}", "", Headers{}), Paging::SizedPages);
  EXPECT_EQ(pagingOf("https://example", "{\"page\":{page},\"limit\":{limit}}", Headers{}), Paging::SizedPages);
  EXPECT_EQ(pagingOf("https://example", "", Headers{{"X-Page", "{page}"}}), Paging::PageNumber);
  EXPECT_EQ(pagingOf("https://example?page={page}", "", Headers{{"X-Offset", "{offset}"}}), Paging::Offset);
}

TEST(PluginCatalogPaging, SizedServerPagesNeverSkipTheLookaheadEntry) {
  EXPECT_EQ(plugincatalogpaging::limitFor(Paging::SizedPages, 16), 16);
  EXPECT_EQ(plugincatalogpaging::limitFor(Paging::Offset, 16), 17);
  for (size_t page = 0; page < 3; ++page) {
    plugincatalogpaging::PageWindow window(0, 16);
    const size_t first = page * plugincatalogpaging::limitFor(Paging::SizedPages, 16);
    for (size_t row = first; row < first + 16; ++row) EXPECT_TRUE(window.take());
    EXPECT_TRUE(window.hasMore(Paging::SizedPages));
  }
  plugincatalogpaging::PageWindow empty(0, 16);
  EXPECT_FALSE(empty.hasMore(Paging::SizedPages));
}

TEST(PluginCatalogPaging, StreamsDistinctStaticPagesWithoutRetainingTheWholeCatalog) {
  std::string json = "{\"plugins\":[";
  for (int i = 0; i < 35; ++i) {
    if (i) json += ',';
    json += "{\"title\":\"Plugin " + std::to_string(i) + "\",\"id\":\"" + std::to_string(i) + "\"}";
    if (i == 15) json += ",{\"title\":\"\"},{\"description\":\"no title\"}";
  }
  json += "]}";
  const std::string title = "title", id = "id", unused;
  const std::string* const fields[] = {&title, &unused, &id, &unused, &unused, &unused};
  for (size_t page = 0; page < 4; ++page) {
    struct Page {
      plugincatalogpaging::PageWindow window;
      std::vector<std::string> ids;
    } output{{page * 16, 16}, {}};
    output.ids.reserve(16);
    JsonListParser parser(
        "plugins", fields, unused,
        [](void* ctx, JsonListParser::Row& row) {
          auto& out = *static_cast<Page*>(ctx);
          const bool shown = !row.field[JsonListParser::F_TITLE].empty();
          if (out.window.take(plugincatalogpaging::countsTowardPage(Paging::Static, shown)) && shown)
            out.ids.push_back(row.field[JsonListParser::F_ID]);
        },
        &output);
    struct Reader {
      const std::string& json;
      size_t offset = 0;
    } reader{json};
    ASSERT_TRUE(parser.parse(
        [](void* ctx, char* dst, size_t len) {
          auto& in = *static_cast<Reader*>(ctx);
          const size_t n = std::min({len, size_t(13), in.json.size() - in.offset});
          std::memcpy(dst, in.json.data() + in.offset, n);
          in.offset += n;
          return int(n);
        },
        &reader));
    const size_t count = page < 2 ? 16 : page == 2 ? 3 : 0;
    ASSERT_EQ(output.ids.size(), count);
    for (size_t i = 0; i < count; ++i) EXPECT_EQ(output.ids[i], std::to_string(page * 16 + i));
    EXPECT_EQ(output.window.hasMore(Paging::Static), page < 2);
  }
}

TEST(PluginCatalogPaging, CountsUntitledServerRowsWithoutOfferingATruncatedPageAgain) {
  plugincatalogpaging::PageWindow window(0, 3);
  EXPECT_TRUE(window.take(plugincatalogpaging::countsTowardPage(Paging::SizedPages, false)));
  EXPECT_TRUE(window.take());
  EXPECT_TRUE(window.take());
  EXPECT_TRUE(window.hasMore(Paging::SizedPages));
  plugincatalogpaging::PageWindow offset(0, 3);
  EXPECT_TRUE(offset.take());
  EXPECT_TRUE(offset.take());
  EXPECT_TRUE(offset.take());
  EXPECT_FALSE(offset.hasMore(Paging::Offset));
  EXPECT_FALSE(offset.take());
  EXPECT_TRUE(offset.hasMore(Paging::Offset));
}
