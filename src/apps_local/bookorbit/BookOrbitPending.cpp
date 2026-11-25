#include "BookOrbitPending.h"

#include <ContentVisibility.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <LibraryFormat.h>
#include <Logging.h>

#include <cstdio>
#include <mutex>
#include <string_view>

#include "RecentBooksStore.h"

namespace bookorbit {
namespace {

constexpr char DIRECTORY[] = "/.crosspoint/bookorbit-pending";
constexpr size_t PATH_LIMIT = 1024;
constexpr size_t MARKER_PATH_SIZE = sizeof(DIRECTORY) + 32;
std::mutex pendingMutex;

bool isBookPath(std::string_view path) {
  return !path.empty() && path.front() == '/' && path.size() <= PATH_LIMIT &&
         path.find('\0') == std::string_view::npos && FsHelpers::hasEpubExtension(path);
}

void markerPath(char (&out)[MARKER_PATH_SIZE], std::string_view path, const char* suffix = ".pending") {
  std::snprintf(out, sizeof(out), "%s/%016llx%s", DIRECTORY,
                static_cast<unsigned long long>(library::clixPathHash(path.data(), path.size())), suffix);
}

bool markLocked(const std::string& path) {
  char destination[MARKER_PATH_SIZE];
  markerPath(destination, path);
  bool replaceTruncated = false;
  {
    auto existing = Storage.open(destination);
    if (existing && existing.isDirectory()) {
      LOG_ERR("ORBIT", "Pending marker is a directory");
      return false;
    }
    if (existing && existing.fileSize() == path.size()) return true;
    replaceTruncated = bool(existing);
  }
  if (!Storage.ensureDirectoryExists(DIRECTORY)) {
    LOG_ERR("ORBIT", "Could not create pending directory");
    return false;
  }
  char temporary[MARKER_PATH_SIZE];
  markerPath(temporary, path, ".tmp");
  {
    HalFile file;
    if (!Storage.openFileForWrite("ORBIT", temporary, file) || file.write(path.data(), path.size()) != path.size() ||
        !file.close()) {
      LOG_ERR("ORBIT", "Could not save pending title");
      return false;
    }
  }
  if (replaceTruncated && !Storage.remove(destination)) {
    LOG_ERR("ORBIT", "Could not replace truncated pending title");
    return false;
  }
  if (Storage.rename(temporary, destination)) return true;
  LOG_ERR("ORBIT", "Could not install pending title");
  return false;
}

bool clearLocked(const std::string& path) {
  char file[MARKER_PATH_SIZE];
  markerPath(file, path);
  if (!Storage.exists(file) || Storage.remove(file)) return true;
  LOG_ERR("ORBIT", "Could not clear pending title");
  return false;
}

}  // namespace

bool markPending(const std::string& path) {
  if (content::hiddenContent(path)) return true;
  if (!isBookPath(path)) {
    LOG_ERR("ORBIT", "Invalid pending book path");
    return false;
  }
  std::lock_guard lock(pendingMutex);
  return markLocked(path);
}

bool clearPending(const std::string& path) {
  if (!isBookPath(path)) return true;
  std::lock_guard lock(pendingMutex);
  return clearLocked(path);
}

void movePending(const std::string& from, const std::string& to) {
  if (from == to || !isBookPath(from) || !isBookPath(to)) return;
  std::lock_guard lock(pendingMutex);
  char file[MARKER_PATH_SIZE];
  markerPath(file, from);
  if (Storage.exists(file) && markLocked(to)) clearLocked(from);
}

PendingBooks readPending(const std::string& preferred, size_t offset) {
  std::lock_guard lock(pendingMutex);
  PendingBooks result;
  auto directory = Storage.open(DIRECTORY);
  if (!directory || !directory.isDirectory()) {
    result.readError = !Storage.ready() || bool(directory) || Storage.exists(DIRECTORY);
    return result;
  }

  result.paths.reserve(RecentBooksStore::MAX_RECENT_BOOKS);
  if (isBookPath(preferred) && !content::hiddenContent(preferred)) {
    char path[MARKER_PATH_SIZE];
    markerPath(path, preferred);
    auto current = Storage.open(path);
    if (current && !current.isDirectory() && current.fileSize() == preferred.size() &&
        Storage.exists(preferred.c_str())) {
      result.paths.push_back(preferred);
    }
  }
  HalFile file;
  bool ioError = false;
  while (directory.openNextFile(file, ioError)) {
    if (file.isDirectory()) continue;
    const auto size = file.fileSize();
    if (!size || size > PATH_LIMIT) continue;
    char name[28]{};
    if (!file.getName(name, sizeof(name))) continue;
    const std::string_view fileName(name);
    if (fileName.size() != 24 || !fileName.ends_with(".pending")) continue;
    std::string path(size, '\0');
    if (file.read(path.data(), size) != static_cast<int>(size)) {
      result.readError = true;
      continue;
    }
    if (!isBookPath(path) || path == preferred || content::hiddenContent(path) || !Storage.exists(path.c_str()))
      continue;
    char expected[MARKER_PATH_SIZE];
    markerPath(expected, path);
    if (fileName != expected + sizeof(DIRECTORY)) continue;
    if (offset) {
      --offset;
      continue;
    }
    if (result.paths.size() == RecentBooksStore::MAX_RECENT_BOOKS) {
      result.hasMore = true;
      break;
    }
    result.paths.push_back(std::move(path));
  }
  result.readError |= ioError;
  return result;
}

}  // namespace bookorbit
