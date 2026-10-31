#include <LibraryFilter.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <string>
#include <vector>

#include "Epub.h"
#include "LibraryBuilder.h"
#include "LibraryIndexFile.h"

using namespace library;

namespace {

constexpr char INDEX[] = "/.crosspoint/library.idx";

std::string numbered(const char* prefix, const unsigned value) {
  char text[32];
  std::snprintf(text, sizeof(text), "%s%04u", prefix, value);
  return text;
}

std::string pathAt(LibraryIndexFile& index, const SortOrder order, const uint16_t row) {
  const uint16_t ordinal = index.ordinalForRow(order, row);
  if (ordinal == 0xFFFF) return {};
  ClixRecord record{};
  if (!index.readRecord(ordinal, record)) return {};
  std::string path;
  return index.readPath(record, path) ? path : std::string();
}

class LibraryBuilderTest : public ::testing::Test {
 protected:
  BuildStats stats;

  void SetUp() override {
    fake::reset();
    bookMetadata.clear();
    fake::add("/a.epub");
    fake::add("/b.epub");
  }

  void initial() { ASSERT_TRUE(buildLibraryIndex("/", stats, true)); }
};

}  // namespace

TEST_F(LibraryBuilderTest, XtcAndXtchUseEmbeddedTitlesAndAuthors) {
  fake::add("/Comics/one.xtc");
  fake::add("/Comics/two.xtch");
  bookMetadata["/Comics/one.xtc"] = {"Go Problems", "Cho Chikun", true};
  bookMetadata["/Comics/two.xtch"] = {"Sakata no Go", "Sakata Eio", true};
  initial();

  {
    LibraryIndexFile index;
    ASSERT_TRUE(index.open(INDEX));
    ASSERT_EQ(index.header().bookCount, 4u);
    int found = 0;
    for (uint16_t n = 0; n < index.header().bookCount; ++n) {
      ClixRecord record{};
      std::string path, title, author;
      ASSERT_TRUE(index.readRecord(n, record));
      ASSERT_TRUE(index.readPath(record, path));
      if (path != "/Comics/one.xtc" && path != "/Comics/two.xtch") continue;
      ASSERT_TRUE(index.readTitle(record, title));
      ASSERT_TRUE(index.readAuthor(record, author));
      EXPECT_EQ(title, bookMetadata[path].title);
      EXPECT_EQ(author, bookMetadata[path].author);
      ++found;
    }
    EXPECT_EQ(found, 2);
  }
  fake::parses = 0;
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::parses, 0u);
  EXPECT_EQ(stats.metadataReused, 4u);
  EXPECT_FALSE(stats.indexReplaced);
}

TEST_F(LibraryBuilderTest, XtchIsListedWithMetadataDisabledAndEnrichedWhenEnabled) {
  fake::add("/Comics/one.xtch");
  bookMetadata["/Comics/one.xtch"] = {"A Manga", "A Writer", true};
  ASSERT_TRUE(buildLibraryIndex("/", stats, false));
  EXPECT_EQ(fake::parses, 0u);
  {
    LibraryIndexFile index;
    ASSERT_TRUE(index.open(INDEX));
    EXPECT_EQ(index.header().bookCount, 3u);
  }
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  bool found = false;
  for (uint16_t n = 0; n < index.header().bookCount; ++n) {
    ClixRecord record{};
    std::string path, title, author;
    ASSERT_TRUE(index.readRecord(n, record));
    ASSERT_TRUE(index.readPath(record, path));
    if (path != "/Comics/one.xtch") continue;
    ASSERT_TRUE(index.readTitle(record, title));
    ASSERT_TRUE(index.readAuthor(record, author));
    EXPECT_EQ(title, "A Manga");
    EXPECT_EQ(author, "A Writer");
    found = true;
  }
  EXPECT_TRUE(found);
}

TEST_F(LibraryBuilderTest, UnchangedRebuildReusesMetadataAndDoesNotReplaceIndex) {
  initial();
  const auto old = fake::files[INDEX]->bytes;
  fake::parses = 0;

  ASSERT_TRUE(buildLibraryIndex("/", stats, true));

  EXPECT_EQ(fake::parses, 0u);
  EXPECT_EQ(stats.parsed, 0);
  EXPECT_EQ(stats.metadataReused, 2);
  EXPECT_FALSE(stats.indexReplaced);
  EXPECT_EQ(fake::files[INDEX]->bytes, old);
}

TEST_F(LibraryBuilderTest, FolderHeavyUnchangedReconciliationIoScalesLinearly) {
  const auto measure = [this](const unsigned count) {
    fake::reset();
    bookMetadata.clear();
    for (unsigned i = 0; i < count; i++) {
      fake::add("/folder" + numbered("", i) + "/book.txt");
    }
    if (!buildLibraryIndex("/", stats, false)) {
      ADD_FAILURE() << "initial build failed for " << count << " books";
      return 0u;
    }
    fake::resetIoCounters();
    if (!buildLibraryIndex("/", stats, false)) {
      ADD_FAILURE() << "unchanged build failed for " << count << " books";
      return 0u;
    }
    EXPECT_EQ(stats.metadataReused, count);
    EXPECT_FALSE(stats.indexReplaced);
    return fake::reads + fake::seeks;
  };

  const unsigned smallIo = measure(128);
  const unsigned largeIo = measure(256);
  EXPECT_LT(largeIo, smallIo * 3u);
}

TEST_F(LibraryBuilderTest, DirectoryEntriesAreEnumeratedOnce) {
  fake::add("/folder/c.txt");

  ASSERT_TRUE(buildLibraryIndex("/", stats, false));

  EXPECT_EQ(fake::directoryEntriesByPath["/a.epub"], 1u);
  EXPECT_EQ(fake::directoryEntriesByPath["/b.epub"], 1u);
  EXPECT_EQ(fake::directoryEntriesByPath["/folder"], 1u);
  EXPECT_EQ(fake::directoryEntriesByPath["/folder/c.txt"], 1u);
}

TEST_F(LibraryBuilderTest, DirectoryResumeFailureRetainsPreviousIndex) {
  initial();
  const auto old = fake::files[INDEX]->bytes;
  fake::add("/aa-folder/c.txt");
  fake::failDirectorySeek = true;

  EXPECT_FALSE(buildLibraryIndex("/", stats, false));
  EXPECT_EQ(fake::files[INDEX]->bytes, old);
}

TEST_F(LibraryBuilderTest, StagingAndIndexWritesAreBatched) {
  fake::reset();
  for (unsigned i = 0; i < 128; i++) fake::add("/book" + numbered("", i) + ".txt");

  ASSERT_TRUE(buildLibraryIndex("/", stats, false));

  EXPECT_LT(fake::writesByPath["/.crosspoint/library.stage"], 64u);
  EXPECT_LT(fake::writesByPath["/.crosspoint/library.new"], 32u);
}

TEST_F(LibraryBuilderTest, ParentDuplicateTrackingSurvivesDirectoryRecursion) {
  fake::add("/folder/c.txt");
  fake::duplicateDirectoryEntry("/a.epub");

  ASSERT_TRUE(buildLibraryIndex("/", stats, false));

  EXPECT_EQ(stats.books, 3);
  EXPECT_EQ(stats.duplicatesDropped, 1);
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  const auto writes = fake::writesByPath;
  fake::parses = 0;
  EXPECT_EQ(checkLibraryCard(index.header()), CardCheck::Unchanged);
  EXPECT_EQ(fake::writesByPath, writes);
  EXPECT_EQ(fake::parses, 0u);
}

TEST_F(LibraryBuilderTest, TimestampAndSizeChangesParseOnlyTheChangedBook) {
  initial();
  fake::files["/a.epub"]->time++;
  fake::parses = 0;
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::parses, 1u);
  EXPECT_EQ(stats.metadataReused, 1);

  fake::files["/b.epub"]->bytes.push_back('x');
  fake::parses = 0;
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::parses, 1u);
  EXPECT_EQ(stats.metadataReused, 1);
}

TEST_F(LibraryBuilderTest, ZeroTimestampAndFailedExtractionAreNeverFresh) {
  fake::files["/a.epub"]->time = 0;
  bookMetadata["/b.epub"].success = false;
  initial();
  fake::parses = 0;

  ASSERT_TRUE(buildLibraryIndex("/", stats, true));

  EXPECT_EQ(fake::parses, 2u);
  EXPECT_EQ(stats.metadataReused, 0);
  EXPECT_TRUE(stats.indexReplaced);
}

TEST_F(LibraryBuilderTest, MetadataModeChangesInvalidateCachedMetadata) {
  initial();
  fake::parses = 0;

  ASSERT_TRUE(buildLibraryIndex("/", stats, false));
  EXPECT_EQ(fake::parses, 0u);
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(index.header().metadataEnabled, 0);
  index.close();

  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::parses, 2u);
}

TEST_F(LibraryBuilderTest, RebuildVotesFromSourceAuthorInsteadOfPriorCanonicalAuthor) {
  fake::add("/c.epub");
  bookMetadata["/a.epub"].author = "Victor Hugo";
  bookMetadata["/b.epub"].author = "Hugo Victor";
  bookMetadata["/c.epub"].author = "Hugo Victor";
  initial();
  ASSERT_TRUE(Storage.remove("/b.epub"));
  ASSERT_TRUE(Storage.remove("/c.epub"));
  fake::parses = 0;

  ASSERT_TRUE(buildLibraryIndex("/", stats, true));

  EXPECT_EQ(fake::parses, 0u);
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  ClixRecord record{};
  std::string author;
  ASSERT_TRUE(index.readRecord(0, record));
  ASSERT_TRUE(index.readAuthor(record, author));
  EXPECT_EQ(author, "Victor Hugo");
}

TEST_F(LibraryBuilderTest, EqualBasenamesInDifferentFoldersReconcileIndependently) {
  fake::add("/one/same.epub");
  fake::add("/two/same.epub");
  bookMetadata["/one/same.epub"].title = "One";
  bookMetadata["/two/same.epub"].title = "Two";
  initial();
  fake::files["/two/same.epub"]->time++;
  fake::parses = 0;

  ASSERT_TRUE(buildLibraryIndex("/", stats, true));

  EXPECT_EQ(fake::parses, 1u);
  EXPECT_EQ(stats.metadataReused, 3);
}

TEST_F(LibraryBuilderTest, ArrivalOrderUsesDiscoveryRatherThanOriginalPublicationTime) {
  // Copying a new book can preserve an old source timestamp.
  fake::add("/c.epub", "book c", /*time=*/0);
  fake::add("/d.epub", "book d", /*time=*/9);
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));

  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 0), "/a.epub");
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 1), "/b.epub");
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 2), "/c.epub");
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 3), "/d.epub");
  EXPECT_EQ(pathAt(index, SortOrder::RecentDesc, 0), "/d.epub");
}

TEST_F(LibraryBuilderTest, AddedRemovedMovedAndRenamedBooksKeepArrivalOrder) {
  initial();
  fake::add("/c.epub");
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));

  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 0), "/a.epub");
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 1), "/b.epub");
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 2), "/c.epub");
  index.close();

  ASSERT_TRUE(Storage.remove("/b.epub"));
  ASSERT_TRUE(Storage.rename("/a.epub", "/moved.epub"));
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(stats.removed, 1);
  EXPECT_EQ(stats.renamed, 1);
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 0), "/moved.epub");
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 1), "/c.epub");
  index.close();

  ASSERT_TRUE(Storage.rename("/moved.epub", "/renamed.epub"));
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(stats.renamed, 1);
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 0), "/renamed.epub");
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 1), "/c.epub");
}

TEST_F(LibraryBuilderTest, WholeFolderRenameWithUniqueSizePreservesArrivalOrder) {
  fake::add("/old/unique.epub", "a uniquely sized book");
  initial();
  ASSERT_TRUE(Storage.mkdir("/new"));
  ASSERT_TRUE(Storage.rename("/old/unique.epub", "/new/unique.epub"));
  fake::parses = 0;

  ASSERT_TRUE(buildLibraryIndex("/", stats, true));

  EXPECT_EQ(stats.renamed, 1);
  EXPECT_EQ(stats.removed, 0);
  EXPECT_EQ(fake::parses, 1u);
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, 2), "/new/unique.epub");
}

TEST_F(LibraryBuilderTest, DuplicateDetectionRemainsBoundedAndFindsTrackedKeysAfterTheCap) {
  fake::duplicateDirectoryEntry("/a.epub");
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(stats.books, 2);
  EXPECT_EQ(stats.duplicatesDropped, 1);
  EXPECT_FALSE(stats.dedupDegraded);

  fake::reset();
  bookMetadata.clear();
  for (unsigned i = 0; i <= LIBRARY_MAX_DEDUP_KEYS; i++) {
    fake::add("/book" + numbered("", i) + ".txt");
  }
  fake::duplicateDirectoryEntry("/book0000.txt");
  ASSERT_TRUE(buildLibraryIndex("/", stats, false));
  EXPECT_EQ(stats.books, LIBRARY_MAX_DEDUP_KEYS + 1);
  EXPECT_EQ(stats.duplicatesDropped, 1);
  EXPECT_TRUE(stats.dedupDegraded);
  EXPECT_LT(fake::delays, 2000u);
}

TEST_F(LibraryBuilderTest, ReadWriteCloseAndAllocationFailuresRetainPreviousIndex) {
  initial();
  const auto old = fake::files[INDEX]->bytes;

  fake::failRead = 0;
  EXPECT_FALSE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::files[INDEX]->bytes, old);
  fake::failRead = -1;

  fake::files["/a.epub"]->time++;
  fake::failWrite = 0;
  EXPECT_FALSE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::files[INDEX]->bytes, old);
  fake::failWrite = -1;

  fake::failWritePath = "/.crosspoint/library.new";
  EXPECT_FALSE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::files[INDEX]->bytes, old);

  fake::failClosePath = "/.crosspoint/library.new";
  EXPECT_FALSE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::files[INDEX]->bytes, old);

  fake::failAlloc = 3;
  EXPECT_FALSE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::files[INDEX]->bytes, old);
  fake::failAlloc = -1;

  fake::failRename = 1;
  EXPECT_FALSE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::files[INDEX]->bytes, old);
}

TEST_F(LibraryBuilderTest, TruncatedPersistedPathHashAbortsAndRetainsTheLiveIndex) {
  initial();
  auto& bytes = fake::files[INDEX]->bytes;
  ClixHeader header{};
  std::memcpy(&header, bytes.data(), sizeof(header));
  ClixRecord record{};
  std::memcpy(&record, bytes.data() + recordOffset(header, 0), sizeof(record));
  record.nameOff = header.nameLen - 4;
  std::memcpy(bytes.data() + recordOffset(header, 0), &record, sizeof(record));
  const auto corrupted = bytes;

  EXPECT_FALSE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::files[INDEX]->bytes, corrupted);
  EXPECT_FALSE(Storage.exists("/.crosspoint/library.stage"));
  EXPECT_FALSE(Storage.exists("/.crosspoint/library.stage.f"));
}

TEST_F(LibraryBuilderTest, LibrariesPastOldGateAndAtFormatCeilingKeepAllOrders) {
  for (const unsigned count : {513u, static_cast<unsigned>(CLIX_MAX_RECORDS)}) {
    fake::reset();
    bookMetadata.clear();
    std::vector<unsigned> authorOrder(count);
    std::iota(authorOrder.begin(), authorOrder.end(), 0u);
    for (unsigned i = 0; i < count; i++) {
      const std::string path = "/book" + numbered("", i) + ".epub";
      fake::add(path);
      bookMetadata[path].title = numbered("Title ", count - 1 - i);
      bookMetadata[path].author = numbered("Writer ", (i * (count == 513 ? 257u : 2053u)) % count);
    }

    ASSERT_TRUE(buildLibraryIndex("/", stats, true)) << count;
    ASSERT_EQ(stats.books, count);
    EXPECT_FALSE(stats.ranksDegraded);

    if (count == CLIX_MAX_RECORDS) {
      const auto old = fake::files[INDEX]->bytes;
      fake::parses = 0;
      fake::resetIoCounters();
      ASSERT_TRUE(buildLibraryIndex("/", stats, true));
      EXPECT_EQ(fake::parses, 0u);
      EXPECT_EQ(stats.metadataReused, CLIX_MAX_RECORDS);
      EXPECT_FALSE(stats.indexReplaced);
      EXPECT_EQ(fake::files[INDEX]->bytes, old);
      EXPECT_LT(fake::delays, 10000u);
    }

    std::sort(authorOrder.begin(), authorOrder.end(), [count](const unsigned a, const unsigned b) {
      return (a * (count == 513 ? 257u : 2053u)) % count < (b * (count == 513 ? 257u : 2053u)) % count;
    });
    LibraryIndexFile index;
    ASSERT_TRUE(index.open(INDEX));
    for (uint16_t row = 0; row < count; row++) {
      EXPECT_EQ(pathAt(index, SortOrder::RecentAsc, row), "/book" + numbered("", row) + ".epub") << count << ':' << row;
      EXPECT_EQ(pathAt(index, SortOrder::TitleAsc, row), "/book" + numbered("", count - 1 - row) + ".epub")
          << count << ':' << row;
      EXPECT_EQ(pathAt(index, SortOrder::AuthorAsc, row), "/book" + numbered("", authorOrder[row]) + ".epub")
          << count << ':' << row;
    }
  }
}

TEST_F(LibraryBuilderTest, SortAllocationFailureProducesValidDegradedIndex) {
  fake::reset();
  for (unsigned i = 0; i < 513; i++) fake::add("/book" + numbered("", i) + ".txt");
  fake::failAlloc = 6;

  ASSERT_TRUE(buildLibraryIndex("/", stats, false));
  EXPECT_TRUE(fake::failureTriggered);
  EXPECT_TRUE(stats.ranksDegraded);
  EXPECT_TRUE(stats.indexReplaced);

  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(index.bookCount(), 513);
}

TEST_F(LibraryBuilderTest, RootResourcesAreExcludedAndNestedBookFoldersRemain) {
  fake::add("/fonts/font.epub");
  fake::add("/licenses/license.txt");
  fake::add("/apps/game/story.epub");
  fake::add("/dictionaries/entry.txt");
  fake::add("/Books/fonts/guide.epub");
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(index.bookCount(), 3);
  EXPECT_EQ(index.header().reserved[0], 1);
}
TEST_F(LibraryBuilderTest, OldVisibilityPolicyUpdatesOnceWithoutReparsingBooks) {
  initial();
  fake::files[INDEX]->bytes[offsetof(ClixHeader, reserved)] = 0;
  fake::parses = 0;
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_TRUE(stats.indexReplaced);
  EXPECT_EQ(stats.unchanged, 2);
  EXPECT_EQ(fake::parses, 0u);
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(index.header().reserved[0], 1);
  index.close();
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_FALSE(stats.indexReplaced);
}

TEST_F(LibraryBuilderTest, SeriesUsesNumericVolumesAndKeepsUnknownBooksLast) {
  bookMetadata["/a.epub"] = {"A ten", "Jane Austen", true, "Saga", "Fiction", 10.f};
  bookMetadata["/b.epub"] = {"B two", "Lewis Carroll", true, "Saga", "Adventure", 2.f};
  fake::add("/c.epub");
  bookMetadata["/c.epub"] = {"C half", "Jane Austen", true, "Saga", "Fiction", 2.5f};
  fake::add("/d.epub");
  bookMetadata["/d.epub"] = {"D unknown", "Lewis Carroll", true};
  fake::add("/e.epub");
  bookMetadata["/e.epub"] = {"E one", "Jane Austen", true, "Saga", "Fiction", 1.f};
  initial();
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(pathAt(index, SortOrder::SeriesAsc, 0), "/e.epub");
  EXPECT_EQ(pathAt(index, SortOrder::SeriesAsc, 1), "/b.epub");
  EXPECT_EQ(pathAt(index, SortOrder::SeriesAsc, 2), "/c.epub");
  EXPECT_EQ(pathAt(index, SortOrder::SeriesAsc, 3), "/a.epub");
  EXPECT_EQ(pathAt(index, SortOrder::SeriesAsc, 4), "/d.epub");
  EXPECT_EQ(pathAt(index, SortOrder::SeriesDesc, 0), "/d.epub");
  EXPECT_EQ(pathAt(index, SortOrder::AuthorFirstAsc, 0), "/a.epub");
  EXPECT_EQ(pathAt(index, SortOrder::GenreAsc, 0), "/b.epub");
  index.close();
  fake::parses = 0;
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::parses, 0u);
}

TEST_F(LibraryBuilderTest, SeriesFullNamesAreComparedBeforeVolume) {
  bookMetadata["/a.epub"] = {"A", "A Person", true, "A long shared series prefix Beta", "One", 1.f};
  bookMetadata["/b.epub"] = {"B", "B Person", true, "A long shared series prefix Alpha", "Two", 10.f};
  initial();
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(pathAt(index, SortOrder::SeriesAsc, 0), "/b.epub");
  EXPECT_EQ(pathAt(index, SortOrder::SeriesAsc, 1), "/a.epub");
}

TEST_F(LibraryBuilderTest, VersionThreeRebuildPreservesArrivalHistoryAndLoadsNewMetadata) {
  initial();
  auto& bytes = fake::files[INDEX]->bytes;
  ClixHeader old{};
  memcpy(&old, bytes.data(), sizeof(old));
  old.formatVersion = 3;
  layoutSections(old, old.folderLen, old.nameLen);
  memcpy(bytes.data(), &old, sizeof(old));
  fake::parses = 0;
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_EQ(fake::parses, 2u);
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(index.header().formatVersion, 4u);
  EXPECT_EQ(pathAt(index, SortOrder::DateAddedAsc, 0), "/a.epub");
}

TEST_F(LibraryBuilderTest, FacetFiltersCombineAndClearWithoutChangingOrder) {
  bookMetadata["/a.epub"] = {"A", "Lucía Prado", true, "Saga", "Ficción", 2.f};
  bookMetadata["/b.epub"] = {"B", "Lucía Prado", true, "Saga", "Aventura", 1.f};
  fake::add("/c.epub");
  bookMetadata["/c.epub"] = {"C", "", true};
  initial();
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  LibraryFilter filter;
  std::string scratch;
  auto count = [&] {
    int total = 0;
    for (uint16_t row = 0; row < index.bookCount(); ++row) {
      ClixRecord record{};
      EXPECT_TRUE(index.readRecord(index.ordinalForRow(SortOrder::SeriesAsc, row), record));
      total += filter.matches(index, record, scratch);
    }
    return total;
  };
  EXPECT_EQ(count(), 3);
  filter.author = {true, "LUCIA PRADO"};
  EXPECT_EQ(count(), 2);
  filter.series = {true, "saga"};
  filter.genre = {true, "FICCION"};
  EXPECT_EQ(count(), 1);
  filter.genre = {true, "History"};
  EXPECT_EQ(count(), 0);
  filter = {};
  EXPECT_EQ(count(), 3);
  filter.series = {true, ""};
  EXPECT_EQ(count(), 1);
  filter.author = {true, ""};
  EXPECT_EQ(count(), 1);
  EXPECT_EQ(pathAt(index, SortOrder::SeriesAsc, 0), "/b.epub");
}

TEST_F(LibraryBuilderTest, EmptyVersionThreeIndexStillUpgrades) {
  fake::reset();
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  auto& bytes = fake::files[INDEX]->bytes;
  ClixHeader old{};
  std::memcpy(&old, bytes.data(), sizeof(old));
  old.formatVersion = 3;
  layoutSections(old, old.folderLen, old.nameLen);
  std::memcpy(bytes.data(), &old, sizeof(old));
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_TRUE(stats.indexReplaced);
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(index.header().formatVersion, CLIX_FORMAT_VERSION);
}

TEST(LibraryFiltering, RecentPinsKeepDisplayOrderAndDoNotDuplicateTail) {
  const uint16_t pins[] = {1, 3, 1, UINT16_MAX};
  const uint16_t expected[] = {2, 0, 1, 3};
  for (uint16_t row = 0; row < 4; ++row) EXPECT_EQ(recentFilterRow(row, 4, pins, 4), expected[row]);
  EXPECT_EQ(recentFilterRow(4, 4, pins, 4), UINT16_MAX);
  EXPECT_EQ(recentFilterRow(0, 0, nullptr, 0), UINT16_MAX);
  for (uint16_t row = 0; row < 4; ++row) EXPECT_EQ(recentFilterRow(row, 4, nullptr, 0), row);
}

TEST(LibraryFiltering, ReversingRecentUsesTheSameHistorySequence) {
  const uint16_t pins[] = {0, 1};
  for (uint16_t n = 0; n < 4; ++n) {
    const auto descending = recentSortRow(SortOrder::RecentDesc, n, 4, pins, 2);
    const auto ascending = recentSortRow(SortOrder::RecentAsc, 3 - n, 4, pins, 2);
    EXPECT_EQ(ascending, 3 - descending);
  }
  const uint16_t two[] = {0, 1};
  EXPECT_EQ(recentSortRow(SortOrder::RecentDesc, 0, 2, two, 2), 1);
  EXPECT_EQ(recentSortRow(SortOrder::RecentAsc, 0, 2, two, 2), 1);
}

TEST_F(LibraryBuilderTest, CardCheckSkipsParsingAndWritesWhenNamesAndSizesMatch) {
  fake::add("/Comics/test.XTCH");
  fake::add("/books/sub/volume.txt");
  initial();
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  fake::parses = 0;
  const auto writes = fake::writesByPath;
  fake::resetIoCounters();
  EXPECT_EQ(checkLibraryCard(index.header()), CardCheck::Unchanged);
  EXPECT_EQ(fake::parses, 0u);
  EXPECT_EQ(fake::writesByPath, writes);
  EXPECT_EQ(fake::reads, 0u);
}
TEST_F(LibraryBuilderTest, CardCheckRecognizesAddedDeletedRenamedAndResizedBooks) {
  initial();
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  const auto header = index.header();
  fake::add("/new.epub");
  EXPECT_EQ(checkLibraryCard(header), CardCheck::Changed);
  fake::files.erase("/new.epub");
  EXPECT_EQ(checkLibraryCard(header), CardCheck::Unchanged);
  const auto book = fake::files["/a.epub"];
  fake::files.erase("/a.epub");
  EXPECT_EQ(checkLibraryCard(header), CardCheck::Changed);
  fake::files["/renamed.epub"] = book;
  EXPECT_EQ(checkLibraryCard(header), CardCheck::Changed);
  fake::files.erase("/renamed.epub");
  fake::files["/a.epub"] = book;
  book->bytes.push_back('x');
  EXPECT_EQ(checkLibraryCard(header), CardCheck::Changed);
}
TEST_F(LibraryBuilderTest, CardCheckIgnoresHiddenAndAppContentAndChecksLegacyOnce) {
  initial();
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  auto header = index.header();
  fake::add("/.hidden/one.epub");
  fake::add("/flashcards/one.txt");
  fake::add("/image.png");
  fake::add("/empty.epub", "");
  EXPECT_EQ(checkLibraryCard(header), CardCheck::Unchanged);
  header.reserved[1] = 0;
  EXPECT_EQ(checkLibraryCard(header), CardCheck::Changed);
}
TEST_F(LibraryBuilderTest, CardCheckFailuresRetainTheExistingIndex) {
  initial();
  const auto original = fake::files[INDEX]->bytes;
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  fake::failAlloc = 0;
  EXPECT_EQ(checkLibraryCard(index.header()), CardCheck::Unavailable);
  fake::failDirectoryRead = 1;
  EXPECT_EQ(checkLibraryCard(index.header()), CardCheck::Unavailable);
  EXPECT_EQ(fake::files[INDEX]->bytes, original);
}
TEST(CardSignature, OrderIndependentButSensitiveToNameAndSize) {
  ClixHeader h{};
  CardSignature one;
  one.add(15, 123);
  one.add(25, 456);
  h.bookCount = one.count;
  storeCardSignature(h, one);
  CardSignature reordered;
  reordered.add(25, 456);
  reordered.add(15, 123);
  EXPECT_TRUE(matchesCardSignature(h, reordered));
  CardSignature changed;
  changed.add(25, 457);
  changed.add(15, 123);
  EXPECT_FALSE(matchesCardSignature(h, changed));
}

TEST_F(LibraryBuilderTest, CardSignatureIsInstalledWhenUpgradingAnUnsignedIndex) {
  initial();
  // A pre-signature version-4 index has the same layout and metadata.
  fake::files[INDEX]->bytes[offsetof(ClixHeader, reserved) + 1] = 0;
  fake::parses = 0;
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  EXPECT_TRUE(stats.indexReplaced);
  EXPECT_EQ(fake::parses, 0u);
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(checkLibraryCard(index.header()), CardCheck::Unchanged);
}

TEST_F(LibraryBuilderTest, NewlyCopiedOldBookAppearsAheadOfPreviouslyIndexedBooks) {
  initial();
  fake::add("/z-old-source.epub", "an older EPUB source", 0);
  ASSERT_TRUE(buildLibraryIndex("/", stats, true));
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  EXPECT_EQ(pathAt(index, SortOrder::DateAddedDesc, 0), "/z-old-source.epub");
}

TEST_F(LibraryBuilderTest, CategoryValuesSelectOnlyMatchingBooks) {
  bookMetadata["/a.epub"] = {"Imaginación", "Jane Austen", true, "Stories", "Romance", 2.f};
  bookMetadata["/b.epub"] = {"Islands", "Jane Austen", true, "Stories", "Action", 1.f};
  fake::add("/c.epub");
  bookMetadata["/c.epub"] = {"Érase una vez", "Lewis Carroll", true, "", "Romance", 0.f};
  initial();
  LibraryIndexFile index;
  ASSERT_TRUE(index.open(INDEX));
  std::string scratch;
  auto matches = [&](Facet facet, const char* value) {
    LibraryFilter filter;
    filter.forFacet(facet) = {true, value};
    int count = 0;
    for (uint16_t row = 0; row < index.bookCount(); ++row) {
      ClixRecord record{};
      EXPECT_TRUE(index.readRecord(index.ordinalForRow(facetOrder(facet), row), record));
      count += filter.matches(index, record, scratch);
    }
    return count;
  };
  EXPECT_EQ(matches(Facet::Genre, "romance"), 2);
  EXPECT_EQ(matches(Facet::Genre, "Action"), 1);
  EXPECT_EQ(matches(Facet::Series, "Stories"), 2);
  EXPECT_EQ(matches(Facet::Series, ""), 1);
  EXPECT_EQ(matches(Facet::Author, "Jane Austen"), 2);
  EXPECT_EQ(matches(Facet::AuthorFirst, "Jane Austen"), 2);
  EXPECT_EQ(matches(Facet::TitleInitial, "i"), 2);
  EXPECT_EQ(matches(Facet::TitleInitial, "E"), 1);
  EXPECT_EQ(matches(Facet::TitleInitial, "Z"), 0);
}
