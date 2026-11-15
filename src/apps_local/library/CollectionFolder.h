#pragma once
#include <array>
#include <string>

#include "CollectionStore.h"
namespace collections {
// A directory window indexes offsets in PSRAM; file names remain on SD.
class Folder {
 public:
  bool open(const std::string& directory);
  bool open(const std::string& directory, uint64_t revision);
  bool entry(unsigned row, std::string& name, bool& directory);
  unsigned count() const { return count_; }
  bool hasSubfolders() const { return directories_ != 0; }
  void close();
  ~Folder() { close(); }

 private:
  HalMemory::PsramBuffer offsets_;
  static constexpr unsigned SnapshotBytes = 32 * 1024;
  struct Snapshot {
    char path[PathBytes];
    unsigned bytes, count;
    bool complete;
    uint8_t data[SnapshotBytes];
  };
  HalMemory::PsramBuffer snapshots_;
  HalFile file_;
  char name_[PathBytes]{};
  unsigned count_ = 0, directories_ = 0, bytes_ = 0;
  int activeSnapshot_ = -1;
  bool cacheAttempted_ = false;
  uint64_t revision_ = 0;
  Snapshot* snapshot(unsigned slot);
  bool append(Snapshot* cache, const void* data, size_t bytes);
  bool flush(Snapshot* cache);
  bool restore(Snapshot& cache);
  void orderBooks(unsigned books);
};
bool bookFile(const char* name);
// Bulk selection includes subfolders, with bounded traversal depth.
enum class FolderSelection : uint8_t { None, Partial, All };
struct FolderSummary {
  FolderSelection state = FolderSelection::None;
  uint32_t total = 0, selected = 0;
  bool counted = false;
};
bool selectFolder(Selection& selection, const std::string& directory, bool checked, FolderSummary* summary = nullptr);
enum class FolderScanMode : uint8_t { Inspect, Select };
enum class FolderScanResult : uint8_t { Pending, Complete, Error };
class FolderScan {
 public:
  using KnownFolder = bool (*)(void*, const char*, FolderSummary&);
  bool start(const std::string& directory, FolderScanMode mode = FolderScanMode::Inspect, KnownFolder known = nullptr,
             void* context = nullptr);
  FolderScanResult step(Selection& selection, FolderSelection& state, unsigned budget = 8,
                        bool (*continueFn)(void*) = nullptr, void* context = nullptr);
  FolderSummary summary() const { return {state_, total_, selected_, counted_}; }
  void close();

 private:
  std::array<HalFile, 32> folders_;
  std::array<size_t, 32> lengths_{};
  char name_[PathBytes]{};
  std::string path_;
  unsigned depth_ = 0;
  bool any_ = false, missing_ = false, active_ = false;
  FolderScanMode mode_ = FolderScanMode::Inspect;
  FolderSelection state_ = FolderSelection::None;
  uint32_t total_ = 0, selected_ = 0;
  bool counted_ = false;
  KnownFolder known_ = nullptr;
  void* knownContext_ = nullptr;
};
bool folderSelection(Selection& selection, const std::string& directory, FolderSelection& state);
}  // namespace collections
