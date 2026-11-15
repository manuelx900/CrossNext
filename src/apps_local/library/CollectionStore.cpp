#include "CollectionStore.h"

#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace collections {
namespace {
constexpr char Root[] = "/.crosspoint/collections";
constexpr char Draft[] = "/.crosspoint/collections/edit.tmp";
constexpr char Pending[] = "/.crosspoint/collections/pending.tmp";
constexpr size_t Header = 16, Record = 132;
uint32_t crc(uint32_t value, const uint8_t* data, size_t count) {
  while (count--) {
    value ^= *data++;
    for (int n = 0; n < 8; ++n) value = (value >> 1) ^ (0xedb88320u & (0u - (value & 1)));
  }
  return value;
}
uint32_t u32(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
void put32(uint8_t* p, uint32_t value) {
  for (int n = 0; n < 4; ++n) p[n] = uint8_t(value >> (n * 8));
}
void filename(char* out, size_t size, uint32_t id, int slot) {
  if (!id)
    std::snprintf(out, size, "%s/catalog-%c.bin", Root, slot ? 'b' : 'a');
  else
    std::snprintf(out, size, "%s/books-%lu-%c.bin", Root, static_cast<unsigned long>(id), slot ? 'b' : 'a');
}
bool validName(const char* name) {
  if (!name || !*name || std::strlen(name) >= NameBytes) return false;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(name); *p; ++p)
    if (*p < 32 || *p == 127) return false;
  return true;
}
struct Version {
  uint32_t generation = 0, count = 0;
  int slot = -1;
};
bool validate(uint32_t id, int slot, Version& result) {
  char name[64];
  filename(name, sizeof(name), id, slot);
  HalFile file;
  if (!Storage.openFileForRead("COLL", name, file)) return false;
  uint8_t head[Header], block[132];
  if (file.read(head, Header) != Header || std::memcmp(head, id ? "CNB1" : "CNC1", 4)) return false;
  const auto count = u32(head + 8);
  if (count > (id ? MaxBooks : MaxCollections) || file.size() < Header ||
      file.size() > Header + (id ? MaxBooks * (PathBytes + 2) : MaxCollections * Record))
    return false;
  uint32_t checksum = crc(0xffffffffu, head, 12);
  if (!id) {
    if (file.size() != Header + count * Record) return false;
    for (uint32_t n = 0; n < count; ++n) {
      if (file.read(block, Record) != Record || u32(block) < 1 || !std::memchr(block + 12, 0, NameBytes) ||
          !validName(reinterpret_cast<const char*>(block + 12)))
        return false;
      checksum = crc(checksum, block, Record);
    }
  } else {
    for (uint32_t n = 0; n < count; ++n) {
      uint8_t length[2];
      if (file.read(length, 2) != 2) return false;
      const unsigned bytes = length[0] | unsigned(length[1]) << 8;
      if (!bytes || bytes >= PathBytes) return false;
      checksum = crc(checksum, length, 2);
      unsigned left = bytes;
      while (left) {
        const auto part = std::min(left, unsigned(sizeof(block)));
        if (file.read(block, part) != int(part)) return false;
        checksum = crc(checksum, block, part);
        left -= part;
      }
      if ((n & 63) == 0) delay(1);
    }
    if (file.position() != file.size()) return false;
  }
  if (~checksum != u32(head + 12)) return false;
  result = {u32(head + 4), count, slot};
  return true;
}
bool latest(uint32_t id, Version& version) {
  Version a, b;
  const bool av = validate(id, 0, a), bv = validate(id, 1, b);
  if (av || bv) {
    version = !av || (bv && b.generation > a.generation) ? b : a;
    return true;
  }
  char name[64];
  for (int slot = 0; slot < 2; ++slot) {
    filename(name, sizeof(name), id, slot);
    if (Storage.exists(name)) {
      LOG_ERR("COLL", "Invalid collection data: %s", name);
      return false;
    }
  }
  version = {};
  return true;
}
uint32_t initialChecksum(uint32_t id, uint32_t generation, uint32_t count) {
  uint8_t head[12]{};
  std::memcpy(head, id ? "CNB1" : "CNC1", 4);
  put32(head + 4, generation);
  put32(head + 8, count);
  return crc(0xffffffffu, head, sizeof(head));
}
bool start(HalFile& file) {
  if (!Storage.ensureDirectoryExists(Root)) return false;
  uint8_t empty[Header]{};
  return Storage.openFileForWrite("COLL", Pending, file) && file.write(empty, Header) == Header;
}
bool finish(HalFile& file, uint32_t id, int slot, uint32_t generation, uint32_t count, uint32_t checksum) {
  uint8_t head[Header]{};
  std::memcpy(head, id ? "CNB1" : "CNC1", 4);
  put32(head + 4, generation);
  put32(head + 8, count);
  put32(head + 12, ~checksum);
  if (!file.seek(0) || file.write(head, Header) != Header) return false;
  if (!file.close()) return false;
  char name[64];
  filename(name, sizeof(name), id, slot);
  if (Storage.exists(name) && !Storage.remove(name)) return false;
  return Storage.rename(Pending, name);
}
uint64_t hashPath(const char* value) {
  uint64_t hash = 14695981039346656037ULL;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(value); *p; ++p)
    hash = (hash ^ *p) * 1099511628211ULL;
  return hash ? hash : 1;
}
void encode(const Collection& entry, uint8_t* bytes) {
  std::memset(bytes, 0, Record);
  put32(bytes, entry.id);
  put32(bytes + 4, entry.count);
  bytes[8] = entry.favorite;
  std::memcpy(bytes + 12, entry.name, NameBytes);
}
}  // namespace

bool validBookPath(const char* path) {
  if (!path || path[0] != '/' || !path[1] || std::strlen(path) >= PathBytes || std::strstr(path, "/../") ||
      std::strstr(path, "/./") || std::strchr(path, '\\') || std::strchr(path, '\n') ||
      !std::strncmp(path, "/.crosspoint", 12))
    return false;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(path); *p; ++p)
    if (*p < 32 || *p == 127) return false;
  const auto length = std::strlen(path);
  return path[length - 1] != '/' && std::strcmp(path + (length > 3 ? length - 3 : 0), "/..") &&
         std::strcmp(path + (length > 2 ? length - 2 : 0), "/.");
}
bool Store::open() {
  Version version;
  if (!latest(0, version)) return false;
  count_ = version.count;
  generation_ = version.generation;
  active_ = version.slot;
  if (active_ < 0) {
    entries_[0] = {};
    entries_[0].id = Favorites;
    std::strcpy(entries_[0].name, "favorites");
    count_ = 1;
    return true;
  }
  char name[64];
  filename(name, sizeof(name), 0, active_);
  HalFile file;
  if (!Storage.openFileForRead("COLL", name, file) || !file.seek(Header)) return false;
  uint8_t bytes[Record];
  for (unsigned n = 0; n < count_; ++n) {
    if (file.read(bytes, Record) != Record) return false;
    entries_[n].id = u32(bytes);
    entries_[n].count = u32(bytes + 4);
    entries_[n].favorite = bytes[8] != 0;
    std::memcpy(entries_[n].name, bytes + 12, NameBytes);
    for (unsigned previous = 0; previous < n; ++previous)
      if (entries_[previous].id == entries_[n].id) return false;
  }
  unsigned kept = 0;
  for (unsigned n = 0; n < count_; ++n)
    if (entries_[n].id != Finished) entries_[kept++] = entries_[n];
  count_ = kept;
  return true;
}
const Collection* Store::find(uint32_t id) const {
  for (unsigned n = 0; n < count_; ++n)
    if (entries_[n].id == id) return entries_ + n;
  return nullptr;
}
unsigned Store::order(uint16_t* rows, bool includeFinished) const {
  unsigned count = 0;
  for (unsigned n = 0; n < count_; ++n)
    if (includeFinished || entries_[n].id != Finished) rows[count++] = n;
  std::sort(rows, rows + count, [this](unsigned a, unsigned b) {
    const auto& first = entries_[a];
    const auto& second = entries_[b];
    if (first.favorite != second.favorite) return first.favorite;
    if (first.id < 3 || second.id < 3) return first.id < second.id;
    return std::strcmp(first.name, second.name) < 0;
  });
  return count;
}
bool Store::save() {
  HalFile file;
  const int next = active_ == 0 ? 1 : 0;
  if (!start(file)) return false;
  uint32_t checksum = initialChecksum(0, generation_ + 1, count_);
  uint8_t bytes[Record];
  for (unsigned n = 0; n < count_; ++n) {
    encode(entries_[n], bytes);
    checksum = crc(checksum, bytes, Record);
    if (file.write(bytes, Record) != Record) return false;
  }
  if (!finish(file, 0, next, generation_ + 1, count_, checksum)) {
    LOG_ERR("COLL", "Catalog save failed");
    return false;
  }
  ++generation_;
  active_ = next;
  return true;
}
uint32_t Store::create(const char* name) {
  if (!validName(name) || count_ >= MaxCollections) return 0;
  uint32_t id = 3;
  for (unsigned n = 0; n < count_; ++n) id = std::max(id, entries_[n].id + 1);
  if (id < 3) return 0;
  char path[64];
  for (;;) {
    filename(path, sizeof(path), id, 0);
    const bool first = Storage.exists(path);
    filename(path, sizeof(path), id, 1);
    if (!first && !Storage.exists(path)) break;
    if (++id < 3) return 0;
  }
  auto& entry = entries_[count_++];
  entry = {};
  entry.id = id;
  std::snprintf(entry.name, sizeof(entry.name), "%s", name);
  if (!save()) {
    --count_;
    return 0;
  }
  return id;
}
bool Store::rename(uint32_t id, const char* name) {
  if (id < 3 || !validName(name)) return false;
  for (unsigned n = 0; n < count_; ++n)
    if (entries_[n].id == id) {
      char old[NameBytes];
      std::memcpy(old, entries_[n].name, NameBytes);
      std::snprintf(entries_[n].name, NameBytes, "%s", name);
      if (save()) return true;
      std::memcpy(entries_[n].name, old, NameBytes);
      return false;
    }
  return false;
}
bool Store::favorite(uint32_t id, bool value) {
  for (unsigned n = 0; n < count_; ++n)
    if (entries_[n].id == id) {
      if (entries_[n].favorite == value) return true;
      entries_[n].favorite = value;
      if (save()) return true;
      entries_[n].favorite = !value;
      return false;
    }
  return false;
}
bool Store::updateCount(uint32_t id, uint32_t count) {
  for (unsigned n = 0; n < count_; ++n)
    if (entries_[n].id == id) {
      const auto old = entries_[n].count;
      if (old == count) return true;
      entries_[n].count = count;
      if (save()) return true;
      entries_[n].count = old;
      return false;
    }
  return id == Favorites;
}
bool Store::erase(uint32_t id) {
  if (id < 3) return false;
  for (unsigned n = 0; n < count_; ++n)
    if (entries_[n].id == id) {
      const auto old = entries_[n];
      for (unsigned p = n; p + 1 < count_; ++p) entries_[p] = entries_[p + 1];
      --count_;
      if (!save()) {
        for (unsigned p = count_; p > n; --p) entries_[p] = entries_[p - 1];
        entries_[n] = old;
        ++count_;
        return false;
      }
      char path[64];
      for (int slot = 0; slot < 2; ++slot) {
        filename(path, sizeof(path), id, slot);
        if (Storage.exists(path)) Storage.remove(path);
      }
      return true;
    }
  return false;
}

bool Selection::open(uint32_t id, bool editable) {
  if (id == Finished) {
    close();
    return false;
  }
  return openImpl(id, editable);
}
bool Selection::openFinishedCache(bool editable) { return openImpl(Finished, editable); }
bool Selection::openImpl(uint32_t id, bool editable) {
  close();
  if (!id) return false;
  Version version;
  if (!latest(id, version)) {
    if (id != Finished || !editable) return false;
    version = {};
  }
  // Up to 1 MiB lookup slots + 64 KiB ordinals; PSRAM only, released on exit.
  // A stack or global library-sized path array would outlive this optional flow.
  table_ = HalMemory::allocatePsram(Slots * sizeof(Slot));
  order_ = HalMemory::allocatePsram(MaxBooks * sizeof(uint16_t));
  if (!table_ || !order_) {
    LOG_ERR("COLL", "OOM: collection selection");
    close();
    return false;
  }
  std::memset(table_.get(), 0, Slots * sizeof(Slot));
  // Optional add-only prefix filter: 8 KiB PSRAM avoids unrelated directory scans.
  // OOM and hash collisions retain the exact inspection path; no selected path is omitted.
  if (editable) {
    prefixes_ = HalMemory::allocatePsram(PrefixBytes);
    if (prefixes_) std::memset(prefixes_.get(), 0, PrefixBytes);
  }
  id_ = id;
  generation_ = version.generation;
  active_ = version.slot;
  editable_ = editable;
  char name[64];
  filename(name, sizeof(name), id, active_);
  HalFile source;
  if (active_ >= 0 && (!Storage.openFileForRead("COLL", name, source) || !source.seek(Header))) {
    close();
    return false;
  }
  if (editable) {
    if (!Storage.ensureDirectoryExists(Root)) {
      close();
      return false;
    }
    paths_ = Storage.open(Draft, O_RDWR | O_CREAT | O_TRUNC);
    if (!paths_) {
      close();
      return false;
    }
    uint8_t block[128];
    while (source && source.available()) {
      const int bytes = source.read(block, sizeof(block));
      if (bytes <= 0 || paths_.write(block, bytes) != size_t(bytes)) {
        close();
        return false;
      }
    }
  } else if (active_ >= 0) {
    source.close();
    if (!Storage.openFileForRead("COLL", name, paths_)) {
      close();
      return false;
    }
  }
  unsigned cursor = editable ? 0 : Header;
  for (unsigned n = 0; n < version.count; ++n) {
    uint8_t length[2];
    if (!paths_.seek(cursor) || paths_.read(length, 2) != 2) {
      close();
      return false;
    }
    const unsigned bytes = length[0] | unsigned(length[1]) << 8;
    if (!bytes || bytes >= PathBytes || paths_.read(incoming_, bytes) != int(bytes)) {
      close();
      return false;
    }
    incoming_[bytes] = 0;
    unsigned at = 0;
    bool found = false;
    if (std::strlen(incoming_) != bytes || !validBookPath(incoming_) || !locate(incoming_, at, found) || found) {
      close();
      return false;
    }
    slots()[at] = {hashPath(incoming_), cursor + 2, uint16_t(bytes), true, 0};
    order()[size_++] = uint16_t(at);
    ++count_;
    addPrefixes(incoming_);
    cursor += 2 + bytes;
  }
  return true;
}
bool Selection::read(const Slot& slot) {
  if (!paths_ || slot.length >= PathBytes || !paths_.seek(slot.offset) ||
      paths_.read(scratch_, slot.length) != slot.length)
    return false;
  scratch_[slot.length] = 0;
  return true;
}
bool Selection::locate(const char* path, unsigned& at, bool& found) {
  const uint64_t hash = hashPath(path);
  at = unsigned(hash) & (Slots - 1);
  for (unsigned n = 0; n < Slots; ++n, at = (at + 1) & (Slots - 1)) {
    const auto& slot = slots()[at];
    if (!slot.hash) {
      found = false;
      return true;
    }
    if (slot.hash == hash) {
      if (!read(slot)) return false;
      if (!std::strcmp(scratch_, path)) {
        found = true;
        return true;
      }
    }
  }
  return false;
}
bool Selection::has(const char* path, bool& selected) {
  selected = false;
  if (!table_ || !validBookPath(path)) return false;
  unsigned at = 0;
  bool found = false;
  if (!locate(path, at, found)) return false;
  selected = found && slots()[at].selected;
  return true;
}
void Selection::addPrefixes(const char* path) {
  if (!prefixes_) return;
  uint64_t hash = 14695981039346656037ULL;
  const auto add = [this](uint64_t value) {
    if (!value) value = 1;
    for (const auto bit : {uint32_t(value) & (PrefixBytes * 8 - 1), uint32_t(value >> 32) & (PrefixBytes * 8 - 1)})
      prefixes_.get()[bit / 8] |= uint8_t(1u << (bit % 8));
  };
  for (const auto* p = reinterpret_cast<const unsigned char*>(path); *p; ++p) {
    if (*p == '/' && p != reinterpret_cast<const unsigned char*>(path)) add(hash);
    hash = (hash ^ *p) * 1099511628211ULL;
    if (p == reinterpret_cast<const unsigned char*>(path)) add(hash);
  }
}
bool Selection::mayContainSelected(const char* directory) const {
  if (!count_) return false;
  if (!prefixes_ || !directory || directory[0] != '/') return true;
  auto length = std::strlen(directory);
  while (length > 1 && directory[length - 1] == '/') --length;
  uint64_t hash = 14695981039346656037ULL;
  for (size_t n = 0; n < length; ++n) hash = (hash ^ static_cast<unsigned char>(directory[n])) * 1099511628211ULL;
  if (!hash) hash = 1;
  for (const auto bit : {uint32_t(hash) & (PrefixBytes * 8 - 1), uint32_t(hash >> 32) & (PrefixBytes * 8 - 1)})
    if (!(prefixes_.get()[bit / 8] & (1u << (bit % 8)))) return false;
  return true;
}
bool Selection::set(const char* path, bool selected) {
  if (!editable_ || !validBookPath(path)) return false;
  unsigned at = 0;
  bool found = false;
  if (!locate(path, at, found)) return false;
  if (found) {
    if (slots()[at].selected != selected) {
      count_ += selected ? 1 : -1;
      slots()[at].selected = selected;
      if (selected)
        addPrefixes(path);
      else if (!count_ && prefixes_)
        std::memset(prefixes_.get(), 0, PrefixBytes);
    }
    return true;
  }
  if (!selected) return true;
  if (size_ >= MaxBooks) {
    LOG_ERR("COLL", "Collection limit reached");
    return false;
  }
  const auto bytes = std::strlen(path);
  uint8_t length[2] = {uint8_t(bytes), uint8_t(bytes >> 8)};
  if (!paths_.seek(paths_.size())) return false;
  const auto offset = paths_.position() + 2;
  if (paths_.write(length, 2) != 2 || paths_.write(path, bytes) != bytes) return false;
  slots()[at] = {hashPath(path), uint32_t(offset), uint16_t(bytes), true, 0};
  order()[size_++] = uint16_t(at);
  ++count_;
  addPrefixes(path);
  return true;
}
bool Selection::selected(unsigned ordinal) const { return ordinal < size_ && slots()[order()[ordinal]].selected; }
bool Selection::path(unsigned ordinal, std::string& out) {
  if (ordinal >= size_ || !read(slots()[order()[ordinal]])) return false;
  out.assign(scratch_);
  return true;
}
bool Selection::save() {
  if (!editable_) return false;
  HalFile file;
  const int next = active_ == 0 ? 1 : 0;
  if (!start(file)) return false;
  uint32_t checksum = initialChecksum(id_, generation_ + 1, count_);
  for (unsigned n = 0; n < size_; ++n) {
    const auto& slot = slots()[order()[n]];
    if (!slot.selected) continue;
    if (!read(slot)) return false;
    uint8_t length[2] = {uint8_t(slot.length), uint8_t(slot.length >> 8)};
    checksum = crc(checksum, length, 2);
    checksum = crc(checksum, reinterpret_cast<uint8_t*>(scratch_), slot.length);
    if (file.write(length, 2) != 2 || file.write(scratch_, slot.length) != slot.length) return false;
    if ((n & 63) == 0) delay(1);
  }
  if (!finish(file, id_, next, generation_ + 1, count_, checksum)) {
    LOG_ERR("COLL", "Member save failed");
    return false;
  }
  active_ = next;
  ++generation_;
  return true;
}
void Selection::close() {
  paths_.close();
  table_.reset();
  order_.reset();
  prefixes_.reset();
  if (editable_ && Storage.exists(Draft)) Storage.remove(Draft);
  editable_ = false;
  id_ = generation_ = size_ = count_ = 0;
  active_ = -1;
}
bool isFavorite(const char* path, bool& value) {
  // Cold long-press lookup; the optional PSRAM working set is scoped to this call.
  auto set = makeUniqueNoThrow<Selection>();
  if (!set) {
    LOG_ERR("COLL", "OOM: favorites lookup");
    return false;
  }
  return set->open(Favorites, false) && set->has(path, value);
}
bool setFavorite(const char* path, bool value) {
  auto set = makeUniqueNoThrow<Selection>();
  if (!set) {
    LOG_ERR("COLL", "OOM: favorites edit");
    return false;
  }
  if (!set->open(Favorites, true) || !set->set(path, value) || !set->save()) return false;
  auto store = makeUniqueNoThrow<Store>();
  if (!store) {
    LOG_ERR("COLL", "OOM: favorites count");
    return false;
  }
  return store->open() && store->updateCount(Favorites, set->count());
}
}  // namespace collections
