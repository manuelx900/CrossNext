#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace bookorbit {

struct PendingBooks {
  std::vector<std::string> paths;
  bool hasMore = false;
  bool readError = false;
};

// A durable marker names the book, not a snapshot of its position. Sync reads
// the latest position from the reader's existing progress file.
bool markPending(const std::string& path);
bool clearPending(const std::string& path);
void movePending(const std::string& from, const std::string& to);
PendingBooks readPending(const std::string& preferred, size_t offset = 0);

}  // namespace bookorbit
