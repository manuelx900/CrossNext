#pragma once

#include <cstdint>
#include <cstring>

namespace shelf {
struct Order {
  static constexpr int CAPACITY = 64;
  uint8_t items[CAPACITY]{};
  uint8_t count = 0;

  bool add(int item) {
    if (item < 0 || item >= CAPACITY || count >= CAPACITY) return false;
    for (int row = 0; row < count; ++row)
      if (items[row] == item) return false;
    items[count++] = static_cast<uint8_t>(item);
    return true;
  }
  void complete(int total) {
    if (total > CAPACITY) total = CAPACITY;
    for (int item = 0; item < total; ++item) add(item);
  }
  int at(int row) const { return row >= 0 && row < count ? items[row] : -1; }
  bool move(int row, int delta) {
    if ((delta != -1 && delta != 1) || row < 0 || row >= count) return false;
    const int next = row + delta;
    if (next < 0 || next >= count) return false;
    const auto item = items[row];
    items[row] = items[next];
    items[next] = item;
    return true;
  }
};

template <typename Hidden, typename TitleAt>
int orderedShownItem(const Order& order, const Hidden& hidden, int row, TitleAt titleAt) {
  if (row < 0) return -1;
  int visible = 0;
  for (int n = 0; n < order.count; ++n) {
    const int item = order.items[n];
    if (!hidden.contains(titleAt(item)) && visible++ == row) return item;
  }
  return -1;
}

template <typename Hidden, typename TitleAt>
int orderedShownRowFor(const Order& order, const Hidden& hidden, int item, TitleAt titleAt) {
  int before = 0;
  for (int row = 0; row < order.count; ++row) {
    const int current = order.items[row];
    if (current == item) return before;
    if (!hidden.contains(titleAt(current))) ++before;
  }
  return before;
}

struct OrderPaths {
  const char* saved;
  const char* temporary;
  const char* backup;
};

template <typename Store>
bool recoverOrderFile(Store& storage, const OrderPaths& paths) {
  return storage.exists(paths.saved) || !storage.exists(paths.backup) || storage.rename(paths.backup, paths.saved);
}

template <typename Store>
bool publishOrderFile(Store& storage, const OrderPaths& paths, bool writtenAndClosed) {
  if (!writtenAndClosed || !recoverOrderFile(storage, paths)) return false;
  const bool previous = storage.exists(paths.saved);
  if (previous) {
    if (storage.exists(paths.backup) && !storage.remove(paths.backup)) return false;
    if (!storage.rename(paths.saved, paths.backup)) return false;
  }
  if (!storage.rename(paths.temporary, paths.saved)) {
    if (previous) storage.rename(paths.backup, paths.saved);
    return false;
  }
  if (previous) storage.remove(paths.backup);
  return true;
}

struct OrderLine {
  char text[64]{};
  unsigned used = 0;
  bool overflow = false;
  bool push(char c) {
    if (c == '\r') return false;
    if (c == '\n') {
      text[used] = '\0';
      used = 0;
      const bool complete = !overflow;
      overflow = false;
      return complete;
    }
    if (!c || used + 1 >= sizeof(text))
      overflow = true;
    else
      text[used++] = c;
    return false;
  }
};

template <typename Folder>
void consumeOrderLine(char* line, const Folder* folders, int folderCount, Order* orders) {
  char* title = std::strchr(line, '\t');
  if (!title || title == line || !title[1] || std::strchr(title + 1, '\t')) return;
  *title++ = '\0';
  for (int folder = 0; folder < folderCount; ++folder) {
    if (std::strcmp(line, folders[folder].title) != 0) continue;
    for (int item = 0; item < folders[folder].count; ++item)
      if (std::strcmp(title, folders[folder].items[item].title) == 0) {
        orders[folder].add(item);
        return;
      }
    return;
  }
}
}  // namespace shelf
