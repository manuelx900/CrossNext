#pragma once

#include <HalMemory.h>
#include <HalStorage.h>

#include <cstdint>
#include <string>

namespace collections {
constexpr uint32_t Favorites = 1, Finished = 2;
constexpr unsigned MaxCollections = 128, MaxBooks = 32768, PathBytes = 1024, NameBytes = 120;
struct Collection {
  uint32_t id = 0;
  uint32_t count = 0;
  bool favorite = false;
  char name[NameBytes]{};
};

// Catalog uses two checksummed generations. Membership is stored separately;
// collection removal never visits or modifies the referenced book paths.
class Store {
 public:
  bool open();
  unsigned count() const { return count_; }
  const Collection& at(unsigned index) const { return entries_[index]; }
  const Collection* find(uint32_t id) const;
  unsigned order(uint16_t* rows, bool includeFinished = true) const;
  uint32_t create(const char* name);
  bool rename(uint32_t id, const char* name);
  bool favorite(uint32_t id, bool value);
  bool erase(uint32_t id);
  bool updateCount(uint32_t id, uint32_t count);

 private:
  Collection entries_[MaxCollections]{};
  unsigned count_ = 0;
  uint32_t generation_ = 0;
  int active_ = -1;
  bool save();
};

// One bounded PSRAM table and ordinal map, owned by the collection flow.
// Paths stay on SD; edits append only new paths to a disposable draft.
class Selection {
 public:
  bool open(uint32_t id, bool editable);
  bool openFinishedCache(bool editable);
  bool hasSnapshot() const { return active_ >= 0; }
  uint32_t generation() const { return generation_; }
  bool has(const char* path, bool& selected);
  // False proves there are no selected descendants; true requires an exact lookup.
  bool mayContainSelected(const char* directory) const;
  bool set(const char* path, bool selected);
  bool path(unsigned ordinal, std::string& out);
  bool selected(unsigned ordinal) const;
  unsigned size() const { return size_; }
  unsigned count() const { return count_; }
  bool save();
  void close();
  ~Selection() { close(); }

 private:
  struct Slot {
    uint64_t hash = 0;
    uint32_t offset = 0;
    uint16_t length = 0;
    bool selected = false;
    uint8_t reserved = 0;
  };
  static constexpr unsigned Slots = MaxBooks * 2;
  static constexpr unsigned PrefixBytes = 8192;
  HalMemory::PsramBuffer table_, order_, prefixes_;
  HalFile paths_;
  char scratch_[PathBytes]{}, incoming_[PathBytes]{};
  uint32_t id_ = 0, generation_ = 0;
  unsigned size_ = 0, count_ = 0;
  int active_ = -1;
  bool editable_ = false;
  bool openImpl(uint32_t id, bool editable);
  Slot* slots() { return reinterpret_cast<Slot*>(table_.get()); }
  const Slot* slots() const { return reinterpret_cast<const Slot*>(table_.get()); }
  uint16_t* order() { return reinterpret_cast<uint16_t*>(order_.get()); }
  const uint16_t* order() const { return reinterpret_cast<const uint16_t*>(order_.get()); }
  bool read(const Slot& slot);
  bool locate(const char* path, unsigned& index, bool& found);
  void addPrefixes(const char* path);
};

bool validBookPath(const char* path);
bool isFavorite(const char* path, bool& value);
bool setFavorite(const char* path, bool value);
}  // namespace collections
