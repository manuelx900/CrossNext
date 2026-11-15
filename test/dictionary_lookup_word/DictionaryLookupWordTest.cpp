#include <gtest/gtest.h>

#include <span>

#include "util/DictionaryLookupWord.h"

namespace {
using dictionaryLookup::Fragment;
Fragment neighbor(const void* context, uint32_t boundary, bool previous) {
  const auto fragments = *static_cast<const std::span<const Fragment>*>(context);
  for (const auto& fragment : fragments)
    if ((previous ? fragment.end : fragment.start) == boundary) return fragment;
  return {};
}
std::string lookup(std::span<const Fragment> fragments, size_t hit) {
  return dictionaryLookup::joinWord(fragments[hit], &fragments, neighbor);
}
}  // namespace

TEST(DictionaryLookupWord, JoinsHyphenatedPartsAcrossLinesAndPages) {
  const Fragment fragments[] = {{"extra-", 0, 5, true}, {"ordi-", 5, 9, true}, {"nary", 9, 13}, {"next", 14, 18}};
  EXPECT_EQ(lookup(fragments, 0), "extraordinary");
  EXPECT_EQ(lookup(fragments, 1), "extraordinary");
  EXPECT_EQ(lookup(fragments, 2), "extraordinary");
  EXPECT_EQ(lookup(fragments, 3), "next");
}
TEST(DictionaryLookupWord, KeepsAuthoredHyphensAndJoinsStyleSpans) {
  const Fragment fragments[] = {{"mother-", 0, 7}, {"in-law", 7, 13}, {"rain", 14, 18}, {"bow", 18, 21}};
  EXPECT_EQ(lookup(fragments, 1), "mother-in-law");
  EXPECT_EQ(lookup(fragments, 2), "rainbow");
}
TEST(DictionaryLookupWord, DoesNotJoinParagraphsOrUnknownSourceRanges) {
  const Fragment fragments[] = {{"one", 0, 3}, {"two", 3, 6, false, true}, {"unknown"}};
  EXPECT_EQ(lookup(fragments, 0), "one");
  EXPECT_EQ(lookup(fragments, 1), "two");
  EXPECT_EQ(lookup(fragments, 2), "unknown");
}
TEST(DictionaryLookupWord, RefusesOverlongQueries) {
  const std::string first(150, 'a'), last(100, 'b');
  const Fragment fragments[] = {{first.c_str(), 0, 150}, {last.c_str(), 150, 250}};
  EXPECT_TRUE(lookup(fragments, 1).empty());
}
TEST(DictionaryLookupWord, DoesNotCrossNoBreakWhitespaceOrSyntheticListMarkers) {
  const Fragment spaced[] = {{"a", 0, 1}, {" ", 1, 2}, {"person", 2, 8}};
  EXPECT_EQ(lookup(spaced, 2), "person");
  EXPECT_EQ(lookup(spaced, 0), "a");
  const Fragment listed[] = {{"1.", 0, 2, false, true}, {"a", 0, 1}, {"person", 2, 8}};
  EXPECT_EQ(lookup(listed, 2), "person");
}
TEST(DictionaryLookupWord, PlainTextUsesWholeTokenAcrossVisualWraps) {
  const std::string definition = "A supercalifragilisticexpialidocious word.";
  EXPECT_EQ(dictionaryLookup::plainWord(definition, 16), "supercalifragilisticexpialidocious");
  EXPECT_EQ(dictionaryLookup::plainWord("Una imaginación viva.", 10), "imaginación");
  EXPECT_TRUE(dictionaryLookup::plainWord(definition, definition.size()).empty());
}
