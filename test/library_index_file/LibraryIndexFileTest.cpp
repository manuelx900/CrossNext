#include <gtest/gtest.h>

#include <cstring>
#include <utility>
#include <vector>

#include "LibraryIndexFile.h"

namespace library {

std::string joinLibraryPath(const std::string_view folder, const std::string_view name) {
  return std::string(folder) + "/" + std::string(name);
}

}  // namespace library

namespace {

std::vector<uint8_t> makeBlob(const uint64_t pathHash, const std::initializer_list<uint8_t> fields) {
  std::vector<uint8_t> blob(sizeof(pathHash) + fields.size());
  std::memcpy(blob.data(), &pathHash, sizeof(pathHash));
  std::copy(fields.begin(), fields.end(), blob.begin() + sizeof(pathHash));
  return blob;
}

}  // namespace

TEST(LibraryIndexFile, BrowseSnapshotEliminatesRepeatedFacetIoAndReopensFresh) {
  library::ClixHeader header{};
  std::memcpy(header.magic, library::CLIX_MAGIC, sizeof(header.magic));
  header.formatVersion = library::CLIX_FORMAT_VERSION;
  header.foldVersion = library::CLIX_FOLD_VERSION;
  header.bookCount = library::CLIX_MAX_RECORDS;
  const auto blob = makeBlob(17, {'A', 1, 'B', 1, 'C', 1, 'D', 1, 'E', 1, 'F'});
  library::layoutSections(header, 0, blob.size());
  std::vector<uint8_t> bytes(header.selfSize, 0);
  std::memcpy(bytes.data(), &header, sizeof(header));
  std::memcpy(bytes.data() + header.nameStart, blob.data(), blob.size());
  for (uint16_t i = 0; i < header.bookCount; ++i) {
    library::ClixRecord record{};
    record.nameLen = 1;
    std::memcpy(bytes.data() + header.recordStart + i * sizeof(record), &record, sizeof(record));
    for (unsigned section = 0; section < 5; ++section)
      std::memcpy(bytes.data() + header.permStart + (section * header.bookCount + i) * sizeof(i), &i, sizeof(i));
  }
  Storage.setFile("/browse.clx", bytes);
  library::LibraryIndexFile index;
  ASSERT_TRUE(index.open("/browse.clx"));
  const auto scan = [&] {
    for (uint16_t row = 0; row < header.bookCount; ++row) {
      const auto ordinal = index.ordinalForRow(library::SortOrder::GenreAsc, row);
      ASSERT_EQ(ordinal, row);
      library::ClixRecord record{};
      ASSERT_TRUE(index.readRecord(ordinal, record));
      std::string value;
      ASSERT_TRUE(index.readGenre(record, value));
      ASSERT_EQ(value, "F");
      ASSERT_TRUE(index.readName(record, value));
      ASSERT_EQ(value, "A");
    }
  };
  HalFile::resetIoCounts();
  scan();
  const auto uncachedReads = HalFile::readCount();
  HalFile::resetIoCounts();
  ASSERT_TRUE(index.cacheForBrowsing());
  const auto coldReads = HalFile::readCount();
  std::printf("4096-record metadata scan: %zu file reads; snapshot load: %zu reads; warm scans: 0 reads expected\n",
              uncachedReads, coldReads);
  EXPECT_LE(coldReads, (header.selfSize + 32767) / 32768);
  EXPECT_GT(uncachedReads, coldReads * 1000);
  HalFile::resetIoCounts();
  scan();
  scan();
  ASSERT_TRUE(index.cacheForBrowsing());
  EXPECT_EQ(HalFile::readCount(), 0u);
  EXPECT_EQ(HalFile::seekCount(), 0u);
  index.dropBrowseCache();
  EXPECT_FALSE(index.cacheForBrowsing());
  scan();
  EXPECT_GT(HalFile::readCount(), 0u);
  index.close();
  bytes[header.nameStart + sizeof(uint64_t)] = 'Z';
  Storage.setFile("/browse.clx", std::move(bytes));
  ASSERT_TRUE(index.open("/browse.clx"));
  ASSERT_TRUE(index.cacheForBrowsing());
  library::ClixRecord record{};
  ASSERT_TRUE(index.readRecord(0, record));
  std::string name;
  ASSERT_TRUE(index.readName(record, name));
  EXPECT_EQ(name, "Z");
}

TEST(LibraryIndexFile, BrowseSnapshotOomKeepsFileReaderAndAttemptsOncePerOpen) {
  library::ClixHeader header{};
  std::memcpy(header.magic, library::CLIX_MAGIC, sizeof(header.magic));
  header.formatVersion = library::CLIX_FORMAT_VERSION;
  header.foldVersion = library::CLIX_FOLD_VERSION;
  header.bookCount = 1;
  library::layoutSections(header, 0, 0);
  std::vector<uint8_t> bytes(header.selfSize, 0);
  std::memcpy(bytes.data(), &header, sizeof(header));
  Storage.setFile("/oom.clx", std::move(bytes));
  library::LibraryIndexFile index;
  ASSERT_TRUE(index.open("/oom.clx"));
  HalMemory::unavailable = true;
  HalMemory::attempts = 0;
  EXPECT_FALSE(index.cacheForBrowsing());
  EXPECT_FALSE(index.cacheForBrowsing());
  EXPECT_EQ(HalMemory::attempts, 1u);
  HalMemory::unavailable = false;
  library::ClixRecord record{};
  EXPECT_TRUE(index.readRecord(0, record));
  EXPECT_FALSE(index.ioFailed());
  index.close();
  ASSERT_TRUE(index.open("/oom.clx"));
  EXPECT_TRUE(index.cacheForBrowsing());
}

TEST(LibraryIndexFile, BrowseSnapshotRejectsOversizeAndShortRead) {
  library::ClixHeader header{};
  std::memcpy(header.magic, library::CLIX_MAGIC, sizeof(header.magic));
  header.formatVersion = library::CLIX_FORMAT_VERSION;
  header.foldVersion = library::CLIX_FOLD_VERSION;
  library::layoutSections(header, 0, library::LibraryIndexFile::BROWSE_CACHE_LIMIT);
  std::vector<uint8_t> bytes(header.selfSize, 0);
  std::memcpy(bytes.data(), &header, sizeof(header));
  Storage.setFile("/large.clx", std::move(bytes));
  library::LibraryIndexFile index;
  ASSERT_TRUE(index.open("/large.clx"));
  HalMemory::attempts = 0;
  EXPECT_FALSE(index.cacheForBrowsing());
  EXPECT_EQ(HalMemory::attempts, 0u);
  EXPECT_FALSE(index.ioFailed());
  index.close();
  library::layoutSections(header, 0, 0);
  bytes.assign(header.selfSize, 0);
  std::memcpy(bytes.data(), &header, sizeof(header));
  Storage.setFile("/short.clx", std::move(bytes));
  ASSERT_TRUE(index.open("/short.clx"));
  Storage.truncate(0);
  EXPECT_FALSE(index.cacheForBrowsing());
  EXPECT_TRUE(index.ioFailed());
}

TEST(LibraryIndexFile, MissingIndexDoesNotCloseAnUninitializedHandle) {
  Storage.clearFile();
  HalFile::resetInvalidCloseCount();

  {
    library::LibraryIndexFile index;
    EXPECT_FALSE(index.open("/missing.clx"));
  }

  EXPECT_EQ(HalFile::invalidCloseCount(), 0);
}

TEST(LibraryIndexFile, ReadsEveryStoredOrderInBothDirections) {
  library::ClixHeader header{};
  std::memcpy(header.magic, library::CLIX_MAGIC, sizeof(header.magic));
  header.formatVersion = library::CLIX_FORMAT_VERSION;
  header.foldVersion = library::CLIX_FOLD_VERSION;
  header.bookCount = 3;
  library::layoutSections(header, 0, 0);

  std::vector<uint8_t> bytes(header.selfSize, 0);
  std::memcpy(bytes.data(), &header, sizeof(header));
  const uint16_t authorOrder[] = {2, 0, 1};
  const uint16_t arrivalOrder[] = {1, 2, 0};
  std::memcpy(bytes.data() + library::authorOrderOffset(header, 0), authorOrder, sizeof(authorOrder));
  std::memcpy(bytes.data() + library::arrivalOrderOffset(header, 0), arrivalOrder, sizeof(arrivalOrder));
  for (unsigned section = 2; section < 5; ++section)
    std::memcpy(bytes.data() + header.permStart + section * header.bookCount * sizeof(uint16_t), authorOrder,
                sizeof(authorOrder));
  Storage.setFile("/library.clx", std::move(bytes));

  library::LibraryIndexFile index;
  ASSERT_TRUE(index.open("/library.clx"));

  const auto expectOrder = [&](const library::SortOrder order, const uint16_t a, const uint16_t b, const uint16_t c) {
    EXPECT_EQ(index.ordinalForRow(order, 0), a);
    EXPECT_EQ(index.ordinalForRow(order, 1), b);
    EXPECT_EQ(index.ordinalForRow(order, 2), c);
    EXPECT_EQ(index.ordinalForRow(order, 3), 0xFFFF);
  };
  expectOrder(library::SortOrder::RecentAsc, 1, 2, 0);
  expectOrder(library::SortOrder::RecentDesc, 0, 2, 1);
  expectOrder(library::SortOrder::TitleAsc, 0, 1, 2);
  expectOrder(library::SortOrder::TitleDesc, 2, 1, 0);
  expectOrder(library::SortOrder::AuthorAsc, 2, 0, 1);
  expectOrder(library::SortOrder::AuthorDesc, 1, 0, 2);
  expectOrder(library::SortOrder::AuthorFirstAsc, 2, 0, 1);
  expectOrder(library::SortOrder::AuthorFirstDesc, 1, 0, 2);
  expectOrder(library::SortOrder::SeriesAsc, 2, 0, 1);
  expectOrder(library::SortOrder::SeriesDesc, 1, 0, 2);
  expectOrder(library::SortOrder::GenreAsc, 2, 0, 1);
  expectOrder(library::SortOrder::GenreDesc, 1, 0, 2);
  expectOrder(library::SortOrder::DateAddedAsc, 1, 2, 0);
}

TEST(LibraryIndexFile, ResolvesRecentRowsByIdentity) {
  library::ClixHeader header{};
  std::memcpy(header.magic, library::CLIX_MAGIC, sizeof(header.magic));
  header.formatVersion = library::CLIX_FORMAT_VERSION;
  header.foldVersion = library::CLIX_FOLD_VERSION;
  header.bookCount = 3;
  // One 8-byte path hash blob per record.
  library::layoutSections(header, 0, 3 * sizeof(uint64_t));
  std::vector<uint8_t> bytes(header.selfSize, 0);
  std::memcpy(bytes.data(), &header, sizeof(header));

  // Ordinals 0 and 2 share a size, so only the hash can tell them apart.
  constexpr uint64_t HASHES[] = {11, 22, 33};
  constexpr uint32_t SIZES[] = {100, 200, 100};
  for (uint16_t ordinal = 0; ordinal < 3; ordinal++) {
    library::ClixRecord record{};
    record.fileSize = SIZES[ordinal];
    record.nameOff = ordinal * sizeof(uint64_t);
    std::memcpy(bytes.data() + library::recordOffset(header, ordinal), &record, sizeof(record));
    std::memcpy(bytes.data() + header.nameStart + record.nameOff, &HASHES[ordinal], sizeof(uint64_t));
  }
  const uint16_t arrivalOrder[] = {1, 2, 0};
  std::memcpy(bytes.data() + library::arrivalOrderOffset(header, 0), arrivalOrder, sizeof(arrivalOrder));
  Storage.setFile("/library.clx", std::move(bytes));

  library::LibraryIndexFile index;
  ASSERT_TRUE(index.open("/library.clx"));
  const library::BookIdentity books[] = {
      {HASHES[0], SIZES[0]},  // ordinal 0 -> ascending row 2
      {HASHES[2], SIZES[2]},  // same size as ordinal 0, hash picks ordinal 2 -> row 1
      {99, SIZES[0]},         // size matches, hash does not: absent
      {HASHES[1], 999},       // hash matches, size does not: absent
      {HASHES[1], 0},         // size unknown: the hash alone matches -> row 0
  };
  uint16_t rows[5] = {};
  ASSERT_TRUE(index.recentRowsFor(books, 5, rows));
  EXPECT_EQ(rows[0], 2);
  EXPECT_EQ(rows[1], 1);
  EXPECT_EQ(rows[2], 0xFFFF);
  EXPECT_EQ(rows[3], 0xFFFF);
  EXPECT_EQ(rows[4], 0);
}

TEST(LibraryIndexFile, ResolvesAllThirtyRecentRows) {
  constexpr uint16_t COUNT = 30;
  library::ClixHeader header{};
  std::memcpy(header.magic, library::CLIX_MAGIC, sizeof(header.magic));
  header.formatVersion = library::CLIX_FORMAT_VERSION;
  header.foldVersion = library::CLIX_FOLD_VERSION;
  header.bookCount = COUNT;
  library::layoutSections(header, 0, COUNT * sizeof(uint64_t));
  std::vector<uint8_t> bytes(header.selfSize, 0);
  std::memcpy(bytes.data(), &header, sizeof(header));
  library::BookIdentity books[COUNT]{};
  for (uint16_t ordinal = 0; ordinal < COUNT; ++ordinal) {
    books[ordinal] = {uint64_t(ordinal + 100), 200};
    library::ClixRecord record{};
    record.fileSize = 200;
    record.nameOff = ordinal * sizeof(uint64_t);
    std::memcpy(bytes.data() + library::recordOffset(header, ordinal), &record, sizeof(record));
    std::memcpy(bytes.data() + header.nameStart + record.nameOff, &books[ordinal].pathHash, sizeof(uint64_t));
    const uint16_t reversed = COUNT - ordinal - 1;
    std::memcpy(bytes.data() + library::arrivalOrderOffset(header, ordinal), &reversed, sizeof(reversed));
  }
  Storage.setFile("/library.clx", std::move(bytes));
  library::LibraryIndexFile index;
  ASSERT_TRUE(index.open("/library.clx"));
  uint16_t rows[COUNT]{};
  for (const auto count : {size_t(17), size_t(COUNT)}) {
    ASSERT_TRUE(index.recentRowsFor(books, count, rows));
    for (size_t n = 0; n < count; ++n) EXPECT_EQ(rows[n], COUNT - n - 1);
  }
}

TEST(LibraryIndexFile, RejectsInvalidPermutationOrdinal) {
  library::ClixHeader header{};
  std::memcpy(header.magic, library::CLIX_MAGIC, sizeof(header.magic));
  header.formatVersion = library::CLIX_FORMAT_VERSION;
  header.foldVersion = library::CLIX_FOLD_VERSION;
  header.bookCount = 1;
  library::layoutSections(header, 0, 0);
  std::vector<uint8_t> bytes(header.selfSize, 0);
  std::memcpy(bytes.data(), &header, sizeof(header));
  const uint16_t invalid = 1;
  std::memcpy(bytes.data() + library::authorOrderOffset(header, 0), &invalid, sizeof(invalid));
  Storage.setFile("/library.clx", std::move(bytes));

  library::LibraryIndexFile index;
  ASSERT_TRUE(index.open("/library.clx"));
  EXPECT_EQ(index.ordinalForRow(library::SortOrder::AuthorAsc, 0), 0xFFFF);
}

TEST(LibraryIndexFile, ReadsPathHashAndEveryPublicBlobField) {
  library::ClixHeader header{};
  std::memcpy(header.magic, library::CLIX_MAGIC, sizeof(header.magic));
  header.formatVersion = library::CLIX_FORMAT_VERSION;
  header.foldVersion = library::CLIX_FOLD_VERSION;
  header.bookCount = 1;
  const uint8_t folder[] = {6, '/', 'b', 'o', 'o', 'k', 's'};
  constexpr uint64_t PATH_HASH = 0x0123456789ABCDEFULL;
  const auto blob = makeBlob(PATH_HASH, {'x', 1, 'a', 1, 't', 8, 'O', 'r', 'i', 'g', 'i', 'n', 'a', 'l'});
  header.folderCount = 1;
  library::layoutSections(header, sizeof(folder), blob.size());
  std::vector<uint8_t> bytes(header.selfSize, 0);
  std::memcpy(bytes.data(), &header, sizeof(header));
  std::memcpy(bytes.data() + header.folderStart, folder, sizeof(folder));
  std::memcpy(bytes.data() + header.nameStart, blob.data(), blob.size());
  library::ClixRecord record{};
  record.nameLen = 1;
  Storage.setFile("/library.clx", bytes);

  library::LibraryIndexFile index;
  ASSERT_TRUE(index.open("/library.clx"));
  uint64_t pathHash = 0;
  ASSERT_TRUE(index.readPathHash(record, pathHash));
  EXPECT_EQ(pathHash, PATH_HASH);
  std::string name;
  ASSERT_TRUE(index.readName(record, name));
  EXPECT_EQ(name, "x");
  std::string author;
  ASSERT_TRUE(index.readAuthor(record, author));
  EXPECT_EQ(author, "a");
  std::string title;
  ASSERT_TRUE(index.readTitle(record, title));
  EXPECT_EQ(title, "t");
  ASSERT_TRUE(index.readSourceAuthor(record, author));
  EXPECT_EQ(author, "Original");
  std::string path;
  ASSERT_TRUE(index.readPath(record, path));
  EXPECT_EQ(path, "/books/x");
  index.close();

  bytes[header.nameStart + sizeof(PATH_HASH) + 5] = 255;
  Storage.setFile("/library.clx", std::move(bytes));
  ASSERT_TRUE(index.open("/library.clx"));
  EXPECT_FALSE(index.readSourceAuthor(record, author));
}

TEST(LibraryIndexFile, RejectsTruncatedAndOverflowingPathHashes) {
  library::ClixHeader header{};
  std::memcpy(header.magic, library::CLIX_MAGIC, sizeof(header.magic));
  header.formatVersion = library::CLIX_FORMAT_VERSION;
  header.foldVersion = library::CLIX_FOLD_VERSION;
  header.bookCount = 1;
  library::layoutSections(header, 0, sizeof(uint64_t) - 1);
  std::vector<uint8_t> bytes(header.selfSize, 0);
  std::memcpy(bytes.data(), &header, sizeof(header));
  Storage.setFile("/library.clx", std::move(bytes));

  library::LibraryIndexFile index;
  ASSERT_TRUE(index.open("/library.clx"));
  library::ClixRecord record{};
  uint64_t hash = 1;
  EXPECT_FALSE(index.readPathHash(record, hash));
  EXPECT_EQ(hash, 0u);
  EXPECT_TRUE(index.ioFailed());

  record.nameOff = UINT32_MAX;
  EXPECT_FALSE(index.readPathHash(record, hash));
}

TEST(LibraryIndexFile, RejectsFolderRecordBeyondFolderBlob) {
  library::ClixHeader header{};
  std::memcpy(header.magic, library::CLIX_MAGIC, sizeof(header.magic));
  header.formatVersion = library::CLIX_FORMAT_VERSION;
  header.foldVersion = library::CLIX_FOLD_VERSION;
  header.bookCount = 1;
  const uint8_t folder[] = {5, '/'};
  const auto blob = makeBlob(1, {'x', 0, 0, 0});
  library::layoutSections(header, sizeof(folder), blob.size());
  std::vector<uint8_t> bytes(header.selfSize, 0);
  std::memcpy(bytes.data(), &header, sizeof(header));
  std::memcpy(bytes.data() + header.folderStart, folder, sizeof(folder));
  std::memcpy(bytes.data() + header.nameStart, blob.data(), blob.size());
  library::ClixRecord record{};
  record.nameLen = 1;
  Storage.setFile("/library.clx", std::move(bytes));

  library::LibraryIndexFile index;
  ASSERT_TRUE(index.open("/library.clx"));
  std::string path;
  EXPECT_FALSE(index.readPath(record, path));
}

TEST(LibraryIndexFile, BatchFolderLookupUsesLinearIoAndSupportsRandomOrder) {
  constexpr uint16_t count = library::CLIX_MAX_RECORDS;
  library::ClixHeader header{};
  std::memcpy(header.magic, library::CLIX_MAGIC, sizeof(header.magic));
  header.formatVersion = library::CLIX_FORMAT_VERSION;
  header.foldVersion = library::CLIX_FOLD_VERSION;
  header.bookCount = header.folderCount = count;
  std::vector<uint8_t> folders;
  folders.reserve(size_t(count) * 7);
  for (unsigned n = 0; n < count; ++n) {
    char name[16];
    const auto len = std::snprintf(name, sizeof(name), "/f%04u", n);
    folders.push_back(uint8_t(len));
    folders.insert(folders.end(), name, name + len);
  }
  library::layoutSections(header, folders.size(), 0);
  std::vector<uint8_t> bytes(header.selfSize, 0);
  std::memcpy(bytes.data(), &header, sizeof(header));
  std::memcpy(bytes.data() + header.folderStart, folders.data(), folders.size());
  Storage.setFile("/library.clx", std::move(bytes));
  library::LibraryIndexFile index;
  ASSERT_TRUE(index.open("/library.clx"));
  std::vector<uint32_t> offsets(count + 1, UINT32_MAX);
  EXPECT_FALSE(index.readFolderOffsets(offsets.data(), count - 1));
  HalFile::resetIoCounts();
  ASSERT_TRUE(index.readFolderOffsets(offsets.data(), count));
  EXPECT_EQ(HalFile::readCount(), count);
  EXPECT_EQ(HalFile::seekCount(), count);
  EXPECT_EQ(offsets[count], UINT32_MAX);
  std::string folder;
  folder.reserve(256);
  for (unsigned n = count; n > 0; --n) {
    ASSERT_TRUE(index.readFolder(n - 1, offsets.data(), count, folder));
    char expected[16];
    std::snprintf(expected, sizeof(expected), "/f%04u", n - 1);
    EXPECT_EQ(folder, expected);
  }
  EXPECT_EQ(HalFile::readCount(), size_t(count) * 3);
  EXPECT_EQ(HalFile::seekCount(), size_t(count) * 3);
  EXPECT_FALSE(index.readFolder(count, offsets.data(), count, folder));
  offsets[0] = header.recordStart;
  EXPECT_FALSE(index.readFolder(0, offsets.data(), count, folder));
}
