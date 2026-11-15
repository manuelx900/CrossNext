#include <gtest/gtest.h>

#include <climits>
#include <cstring>

#define Storage HalStorage::getInstance()
#include "apps_local/ShelfOrder.h"
#undef Storage

namespace {
struct Item {
  const char* title;
};
struct Folder {
  const char* title;
  const Item* items;
  int count;
};
constexpr Item games[] = {{"CHESS"}, {"GO"}, {"PICROSS"}};
constexpr Item apps[] = {{"FOCUS"}, {"COUNTER"}};
constexpr Folder registry[] = {{"GAMES", games, 3}, {"APPS", apps, 2}};
void feed(const char* text, shelf::Order* orders) {
  shelf::OrderLine line;
  for (const char* c = text; *c; ++c)
    if (line.push(*c)) shelf::consumeOrderLine(line.text, registry, 2, orders);
  orders[0].complete(3);
  orders[1].complete(2);
}
}  // namespace

TEST(ShelfOrder, StableTitlesSurviveRegistryChangesAndKeepFoldersSeparate) {
  shelf::Order orders[2];
  feed("GAMES\tPICROSS\nAPPS\tCOUNTER\nGAMES\tCHESS\nGAMES\tREMOVED\n", orders);
  EXPECT_EQ(orders[0].at(0), 2);
  EXPECT_EQ(orders[0].at(1), 0);
  EXPECT_EQ(orders[0].at(2), 1);
  EXPECT_EQ(orders[1].at(0), 1);
  EXPECT_EQ(orders[1].at(1), 0);
}
TEST(ShelfOrder, MissingDuplicateAndMalformedRecordsCannotHideOrDuplicateItems) {
  shelf::Order orders[2];
  feed("GAMES\tGO\nGAMES\tGO\nOTHER\tCHESS\nGAMES\t\n\tCHESS\nGAMES\tCHESS\tEXTRA\n", orders);
  ASSERT_EQ(orders[0].count, 3);
  EXPECT_EQ(orders[0].at(0), 1);
  EXPECT_EQ(orders[0].at(1), 0);
  EXPECT_EQ(orders[0].at(2), 2);
  EXPECT_EQ(orders[1].count, 2);
}
TEST(ShelfOrder, ATruncatedLastRecordIsIgnoredAndNewItemsAppend) {
  shelf::Order orders[2];
  feed("GAMES\tPICROSS\nGAMES\tGO", orders);
  EXPECT_EQ(orders[0].at(0), 2);
  EXPECT_EQ(orders[0].at(1), 0);
  EXPECT_EQ(orders[0].at(2), 1);
}
TEST(ShelfOrder, WindowsLineEndingsAreAccepted) {
  shelf::Order orders[2];
  feed("APPS\tCOUNTER\r\nGAMES\tGO\r\n", orders);
  EXPECT_EQ(orders[0].at(0), 1);
  EXPECT_EQ(orders[1].at(0), 1);
}
TEST(ShelfOrder, OversizedAndBinaryRecordsAreSkippedWithoutPoisoningTheNextLine) {
  shelf::OrderLine line;
  for (int i = 0; i < 100; ++i) EXPECT_FALSE(line.push('x'));
  EXPECT_FALSE(line.push('\n'));
  EXPECT_FALSE(line.push('\0'));
  EXPECT_FALSE(line.push('\n'));
  for (const char* c = "GAMES\tGO"; *c; ++c) EXPECT_FALSE(line.push(*c));
  ASSERT_TRUE(line.push('\n'));
  EXPECT_STREQ(line.text, "GAMES\tGO");
}
TEST(ShelfOrder, MovingAndRollingBackPreserveThePermutation) {
  shelf::Order order;
  order.complete(3);
  ASSERT_TRUE(order.move(2, -1));
  EXPECT_EQ(order.at(1), 2);
  ASSERT_TRUE(order.move(1, 1));
  for (int i = 0; i < 3; ++i) EXPECT_EQ(order.at(i), i);
  EXPECT_FALSE(order.move(0, -1));
  EXPECT_FALSE(order.move(2, 1));
  EXPECT_FALSE(order.move(INT_MAX, 1));
  EXPECT_FALSE(order.move(1, INT_MAX));
}
TEST(ShelfOrder, CapacityAndRowAccessStayBounded) {
  shelf::Order order;
  order.complete(INT_MAX);
  EXPECT_EQ(order.count, shelf::Order::CAPACITY);
  EXPECT_EQ(order.at(-1), -1);
  EXPECT_EQ(order.at(shelf::Order::CAPACITY), -1);
  EXPECT_FALSE(order.add(64));
  EXPECT_FALSE(order.add(-1));
}

namespace {
struct FakeHidden {
  const char* title = "GO";
  bool contains(const char* value) const { return std::strcmp(title, value) == 0; }
};
struct OrderStorage {
  bool saved = true, temporary = true, backup = false;
  bool failPublish = false, failRestore = false, failRemove = false;
  int renames = 0;
  bool& slot(const char* path) {
    if (std::strcmp(path, "saved") == 0) return saved;
    return std::strcmp(path, "temp") == 0 ? temporary : backup;
  }
  bool exists(const char* path) { return slot(path); }
  bool remove(const char* path) {
    if (failRemove) return false;
    slot(path) = false;
    return true;
  }
  bool rename(const char* from, const char* to) {
    ++renames;
    if ((failPublish && std::strcmp(from, "temp") == 0) || (failRestore && std::strcmp(from, "backup") == 0))
      return false;
    if (!slot(from) || slot(to)) return false;
    slot(from) = false;
    slot(to) = true;
    return true;
  }
};
constexpr shelf::OrderPaths paths{"saved", "temp", "backup"};
}  // namespace
TEST(ShelfOrder, HiddenRowsUseTheSamePermutationForOpeningAndResume) {
  shelf::Order order;
  order.add(2);
  order.add(1);
  order.add(0);
  FakeHidden hidden;
  const auto titleAt = [](int item) { return games[item].title; };
  EXPECT_EQ(shelf::orderedShownItem(order, hidden, 0, titleAt), 2);
  EXPECT_EQ(shelf::orderedShownItem(order, hidden, 1, titleAt), 0);
  EXPECT_EQ(shelf::orderedShownItem(order, hidden, -1, titleAt), -1);
  EXPECT_EQ(shelf::orderedShownItem(order, hidden, 2, titleAt), -1);
  EXPECT_EQ(shelf::orderedShownRowFor(order, hidden, 2, titleAt), 0);
  EXPECT_EQ(shelf::orderedShownRowFor(order, hidden, 1, titleAt), 1);
  EXPECT_EQ(shelf::orderedShownRowFor(order, hidden, 0, titleAt), 1);
  hidden.title = "PICROSS";
  EXPECT_EQ(shelf::orderedShownItem(order, hidden, 0, titleAt), 1);
}
TEST(ShelfOrder, ACloseFailureNeverPublishesTheTemporaryFile) {
  OrderStorage storage;
  EXPECT_FALSE(shelf::publishOrderFile(storage, paths, false));
  EXPECT_TRUE(storage.saved);
  EXPECT_FALSE(storage.backup);
  EXPECT_EQ(storage.renames, 0);
}
TEST(ShelfOrder, RenameFailureRestoresThePreviousFile) {
  OrderStorage storage;
  storage.failPublish = true;
  EXPECT_FALSE(shelf::publishOrderFile(storage, paths, true));
  EXPECT_TRUE(storage.saved);
  EXPECT_FALSE(storage.backup);
}
TEST(ShelfOrder, RestartRecoversABackupWhenPublicationAndRollbackFail) {
  OrderStorage storage;
  storage.failPublish = storage.failRestore = true;
  EXPECT_FALSE(shelf::publishOrderFile(storage, paths, true));
  EXPECT_FALSE(storage.saved);
  EXPECT_TRUE(storage.backup);
  storage.failRestore = false;
  EXPECT_TRUE(shelf::recoverOrderFile(storage, paths));
  EXPECT_TRUE(storage.saved);
  EXPECT_FALSE(storage.backup);
}
TEST(ShelfOrder, InterruptedBackupCleanupKeepsThePublishedFile) {
  OrderStorage storage;
  storage.failRemove = true;
  EXPECT_TRUE(shelf::publishOrderFile(storage, paths, true));
  EXPECT_TRUE(storage.saved);
  EXPECT_TRUE(storage.backup);
  EXPECT_TRUE(shelf::recoverOrderFile(storage, paths));
  EXPECT_FALSE(shelf::publishOrderFile(storage, paths, true));
  EXPECT_TRUE(storage.saved);
  EXPECT_TRUE(storage.backup);
}
TEST(ShelfOrder, FirstSaveDoesNotNeedAnExistingFile) {
  OrderStorage storage;
  storage.saved = false;
  EXPECT_TRUE(shelf::publishOrderFile(storage, paths, true));
  EXPECT_TRUE(storage.saved);
  EXPECT_FALSE(storage.temporary);
  EXPECT_FALSE(storage.backup);
}
