#include "CollectionFolder.h"

#include <ContentVisibility.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <array>
#include <cstring>
namespace collections {
namespace {
constexpr char Browse[] = "/.crosspoint/collections/browse.tmp";
}
bool bookFile(const char* name) {
  const auto length = std::strlen(name);
  const char* extension = std::strrchr(name, '.');
  if (!length || !extension) return false;
  const std::string_view ext(extension);
  return content::asciiEqual(ext, ".epub") || content::asciiEqual(ext, ".xtc") || content::asciiEqual(ext, ".xtch") ||
         content::asciiEqual(ext, ".txt") || content::asciiEqual(ext, ".md");
}
Folder::Snapshot* Folder::snapshot(unsigned slot) { return reinterpret_cast<Snapshot*>(snapshots_.get()) + slot; }
void Folder::orderBooks(unsigned books) {
  auto* offsets = reinterpret_cast<uint32_t*>(offsets_.get());
  std::reverse(offsets + MaxBooks - books, offsets + MaxBooks);
  if (books) std::memmove(offsets + directories_, offsets + MaxBooks - books, books * sizeof(uint32_t));
}
bool Folder::restore(Snapshot& cache) {
  count_ = directories_ = 0;
  unsigned at = 0, books = 0;
  auto* offsets = reinterpret_cast<uint32_t*>(offsets_.get());
  while (at < cache.bytes) {
    if (cache.bytes - at < 3 || count_ == MaxBooks) return false;
    const auto* header = cache.data + at;
    const unsigned length = header[1] | unsigned(header[2]) << 8;
    if (header[0] > 1 || !length || length >= PathBytes || length > cache.bytes - at - 3) return false;
    if (header[0])
      offsets[directories_++] = at;
    else
      offsets[MaxBooks - 1 - books++] = at;
    ++count_;
    at += 3 + length;
  }
  if (count_ != cache.count) return false;
  orderBooks(books);
  return true;
}
bool Folder::flush(Snapshot* cache) {
  for (unsigned at = 0; at < cache->bytes; at += 4096) {
    const unsigned length = std::min(4096u, cache->bytes - at);
    if (file_.write(cache->data + at, length) != length) return false;
  }
  cache->bytes = 0;
  return true;
}
bool Folder::append(Snapshot* cache, const void* input, size_t bytes) {
  if (!cache) {
    if (file_.write(input, bytes) != bytes) return false;
    bytes_ += bytes;
    return true;
  }
  const auto* source = static_cast<const uint8_t*>(input);
  while (bytes) {
    if (cache->bytes == SnapshotBytes) {
      if (!file_) file_ = Storage.open(Browse, O_RDWR | O_CREAT | O_TRUNC);
      if (!file_ || !flush(cache)) return false;
    }
    const size_t count = std::min(bytes, size_t(SnapshotBytes - cache->bytes));
    std::memcpy(cache->data + cache->bytes, source, count);
    cache->bytes += count;
    bytes_ += count;
    source += count;
    bytes -= count;
  }
  return true;
}
bool Folder::open(const std::string& directory) { return open(directory, revision_ + 1); }
bool Folder::open(const std::string& directory, uint64_t revision) {
  file_.close();
  count_ = directories_ = bytes_ = 0;
  if (!offsets_) {
    // The existing 128 KiB ordinal workspace is reused across directory changes.
    offsets_ = HalMemory::allocatePsram(MaxBooks * sizeof(uint32_t));
    if (!offsets_) {
      LOG_ERR("COLL", "OOM: folder offsets");
      return false;
    }
  }
  if (!cacheAttempted_) {
    cacheAttempted_ = true;
    // Two 32 KiB name snapshots plus exact paths (~66 KiB). Reused on back;
    // PSRAM avoids per-frame SD name reads and per-directory allocations.
    snapshots_ = HalMemory::allocatePsram(2 * sizeof(Snapshot));
    if (snapshots_)
      std::memset(snapshots_.get(), 0, 2 * sizeof(Snapshot));
    else
      LOG_ERR("COLL", "Optional folder snapshots unavailable");
  }
  if (revision != revision_) {
    if (snapshots_)
      for (unsigned n = 0; n < 2; ++n) snapshot(n)->complete = false;
    activeSnapshot_ = -1;
    revision_ = revision;
  }
  if (snapshots_)
    for (unsigned n = 0; n < 2; ++n) {
      auto& cached = *snapshot(n);
      if (cached.complete && !std::strcmp(cached.path, directory.c_str())) {
        if (restore(cached)) {
          activeSnapshot_ = int(n);
          return true;
        }
        cached.complete = false;
      }
    }
  const int slot = activeSnapshot_ == 0 ? 1 : 0;
  Snapshot* cache = snapshots_ && directory.size() < PathBytes ? snapshot(slot) : nullptr;
  activeSnapshot_ = -1;
  if (cache) {
    std::strcpy(cache->path, directory.c_str());
    cache->bytes = cache->count = 0;
    cache->complete = false;
  } else {
    file_ = Storage.open(Browse, O_RDWR | O_CREAT | O_TRUNC);
    if (!file_) return false;
  }
  const auto failed = [&] {
    file_.close();
    count_ = directories_ = 0;
    return false;
  };
  auto folder = Storage.open(directory.c_str());
  if (!folder || !folder.isDirectory()) return failed();
  unsigned books = 0;
  auto* offsets = reinterpret_cast<uint32_t*>(offsets_.get());
  for (;;) {
    bool error = false;
    auto child = folder.openNextFile(error);
    if (error) return failed();
    if (!child) break;
    const auto length = child.getName(name_, sizeof(name_));
    const bool isDirectory = child.isDirectory();
    if (!length || length >= sizeof(name_) - 1 || name_[0] == '.' ||
        (directory == "/" && isDirectory && content::appFolder(name_)) || (!isDirectory && !bookFile(name_)))
      continue;
    if (count_ == MaxBooks) {
      LOG_ERR("COLL", "Folder limit reached");
      return failed();
    }
    if (isDirectory)
      offsets[directories_++] = bytes_;
    else
      offsets[MaxBooks - 1 - books++] = bytes_;
    ++count_;
    const uint8_t header[] = {uint8_t(isDirectory), uint8_t(length), uint8_t(length >> 8)};
    if (!append(cache, header, sizeof(header)) || !append(cache, name_, length)) return failed();
    if ((count_ & 63) == 0) delay(1);
  }
  orderBooks(books);
  if (cache) {
    if (file_) {
      if (!flush(cache)) return failed();
    } else {
      cache->count = count_;
      cache->complete = true;
      activeSnapshot_ = slot;
    }
  }
  return true;
}
bool Folder::entry(unsigned row, std::string& name, bool& directory) {
  if (row >= count_) return false;
  const auto at = reinterpret_cast<uint32_t*>(offsets_.get())[row];
  if (activeSnapshot_ >= 0) {
    const auto& cache = *snapshot(activeSnapshot_);
    if (at > cache.bytes || cache.bytes - at < 3) return false;
    const auto* header = cache.data + at;
    const unsigned length = header[1] | unsigned(header[2]) << 8;
    if (!length || length >= PathBytes || length > cache.bytes - at - 3) return false;
    name.assign(reinterpret_cast<const char*>(header + 3), length);
    directory = header[0] != 0;
    return true;
  }
  if (!file_.seek(at)) return false;
  uint8_t header[3];
  if (file_.read(header, 3) != 3) return false;
  const unsigned length = header[1] | unsigned(header[2]) << 8;
  if (!length || length >= sizeof(name_) || file_.read(name_, length) != int(length)) return false;
  name_[length] = 0;
  name.assign(name_);
  directory = header[0] != 0;
  return true;
}
void Folder::close() {
  file_.close();
  offsets_.reset();
  snapshots_.reset();
  count_ = directories_ = bytes_ = 0;
  activeSnapshot_ = -1;
  cacheAttempted_ = false;
  revision_ = 0;
}
void FolderScan::close() {
  for (auto& folder : folders_) folder.close();
  active_ = false;
  known_ = nullptr;
  knownContext_ = nullptr;
}
bool FolderScan::start(const std::string& directory, FolderScanMode mode, KnownFolder known, void* context) {
  close();
  if (directory.empty() || directory.size() >= PathBytes) return false;
  path_.reserve(PathBytes);
  path_ = directory;
  mode_ = mode;
  known_ = known;
  knownContext_ = context;
  depth_ = 0;
  any_ = missing_ = false;
  total_ = selected_ = 0;
  counted_ = false;
  state_ = FolderSelection::None;
  folders_[0] = Storage.open(path_.c_str());
  lengths_[0] = path_.size();
  active_ = folders_[0] && folders_[0].isDirectory();
  return active_;
}
FolderScanResult FolderScan::step(Selection& selection, FolderSelection& state, unsigned budget,
                                  bool (*continueFn)(void*), void* context) {
  if (!active_) return FolderScanResult::Error;
  const auto error = [&] {
    close();
    return FolderScanResult::Error;
  };
  for (unsigned n = 0; n < budget; ++n) {
    if (continueFn && !continueFn(context)) return FolderScanResult::Pending;
    bool failed = false;
    auto entry = folders_[depth_].openNextFile(failed);
    if (failed) return error();
    if (!entry) {
      folders_[depth_].close();
      if (!depth_) {
        state = any_ ? FolderSelection::All : FolderSelection::None;
        if (mode_ == FolderScanMode::Select) state = total_ ? FolderSelection::All : FolderSelection::None;
        state_ = state;
        counted_ = true;
        close();
        return FolderScanResult::Complete;
      }
      --depth_;
      path_.resize(lengths_[depth_]);
      continue;
    }
    entry.getName(name_, sizeof(name_));
    if (!*name_ || name_[0] == '.' || (path_ == "/" && entry.isDirectory() && content::appFolder(name_))) continue;
    path_.resize(lengths_[depth_]);
    if (path_.size() + (path_.back() == '/' ? 0 : 1) + std::strlen(name_) >= PathBytes) return error();
    if (path_.back() != '/') path_ += '/';
    path_ += name_;
    if (entry.isDirectory()) {
      FolderSummary cached;
      if (mode_ == FolderScanMode::Inspect && known_ && known_(knownContext_, path_.c_str(), cached) &&
          (cached.counted || cached.state == FolderSelection::Partial)) {
        if (cached.counted) {
          total_ += cached.total;
          selected_ += cached.selected;
          any_ |= cached.selected != 0;
          missing_ |= cached.selected < cached.total;
        }
        if (!cached.counted || (any_ && missing_)) {
          state = state_ = FolderSelection::Partial;
          close();
          return FolderScanResult::Complete;
        }
        path_.resize(lengths_[depth_]);
        continue;
      }
      if (depth_ + 1 == folders_.size()) return error();
      ++depth_;
      lengths_[depth_] = path_.size();
      folders_[depth_] = std::move(entry);
    } else {
      if (bookFile(name_)) {
        ++total_;
        switch (mode_) {
          case FolderScanMode::Inspect: {
            bool checked = false;
            if (!selection.has(path_.c_str(), checked)) return error();
            any_ |= checked;
            missing_ |= !checked;
            selected_ += checked;
            if (any_ && missing_) {
              state = FolderSelection::Partial;
              state_ = state;
              close();
              return FolderScanResult::Complete;
            }
            break;
          }
          case FolderScanMode::Select:
            if (!selection.set(path_.c_str(), true)) return error();
            ++selected_;
            break;
        }
      }
      path_.resize(lengths_[depth_]);
    }
  }
  return FolderScanResult::Pending;
}
namespace {
bool walkFolder(Selection& selection, const std::string& directory, FolderScanMode mode, FolderSelection& state,
                FolderSummary* summary = nullptr) {
  auto scan = makeUniqueNoThrow<FolderScan>();
  if (!scan) {
    LOG_ERR("COLL", "OOM: folder walk");
    return false;
  }
  if (!scan->start(directory, mode)) return false;
  for (;;) {
    const auto result = scan->step(selection, state);
    if (result != FolderScanResult::Pending) {
      if (summary && result == FolderScanResult::Complete) *summary = scan->summary();
      return result == FolderScanResult::Complete;
    }
    delay(1);
  }
}
}  // namespace
bool folderSelection(Selection& selection, const std::string& directory, FolderSelection& state) {
  state = FolderSelection::None;
  return walkFolder(selection, directory, FolderScanMode::Inspect, state);
}
bool selectFolder(Selection& selection, const std::string& directory, bool checked, FolderSummary* summary) {
  std::string path;
  path.reserve(PathBytes);
  const auto under = [&](const char* book) {
    return directory == "/" ||
           (!std::strncmp(book, directory.c_str(), directory.size()) && book[directory.size()] == '/');
  };
  if (!checked) {
    if (summary) *summary = {};
    for (unsigned n = 0; n < selection.size(); ++n) {
      if (!selection.selected(n)) continue;
      if (!selection.path(n, path)) return false;
      if (under(path.c_str()) && !selection.set(path.c_str(), false)) return false;
    }
    return true;
  }
  FolderSelection state;
  return walkFolder(selection, directory, FolderScanMode::Select, state, summary);
}
}  // namespace collections
