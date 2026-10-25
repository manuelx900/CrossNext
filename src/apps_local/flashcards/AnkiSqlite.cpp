#include "AnkiSqlite.h"

#include <algorithm>
#include <cstring>
namespace ankisql {
namespace {
uint32_t be(const uint8_t* p, int n) {
  uint32_t v = 0;
  while (n--) v = (v << 8) | *p++;
  return v;
}
uint32_t serialSize(uint64_t type) {
  if (type <= 4) return type;
  if (type == 5) return 6;
  if (type == 6 || type == 7) return 8;
  if (type == 8 || type == 9) return 0;
  if (type >= 12) return uint32_t((type - 12) / 2);
  return UINT32_MAX;
}
bool rowVarint(const Row& row, uint32_t& at, uint64_t& out) {
  out = 0;
  for (int n = 0; n < 9; ++n) {
    uint8_t c;
    if (!row.bytes(at++, &c, 1)) return false;
    if (n == 8) {
      out = (out << 8) | c;
      return true;
    }
    out = (out << 7) | (c & 127);
    if (!(c & 128)) return true;
  }
  return false;
}
}  // namespace
bool Database::read(uint64_t at, void* out, size_t size) {
  return at <= source.size() && size <= source.size() - at && source.read(at, out, size);
}
bool Database::open() {
  uint8_t h[100];
  if (!read(0, h, sizeof(h)) || std::memcmp(h, "SQLite format 3\0", 16)) return false;
  pageSize = be(h + 16, 2);
  if (pageSize == 1) pageSize = 65536;
  if (pageSize < 512 || pageSize > 65536 || (pageSize & (pageSize - 1)) || be(h + 56, 4) != 1 || h[20] > pageSize - 480)
    return false;
  usable = pageSize - h[20];
  pageCount = uint32_t(source.size() / pageSize);
  return pageCount > 0 && pageCount <= 1048576 && source.size() % pageSize == 0;
}
bool Database::header(uint32_t page, uint8_t* out, uint64_t& at) {
  if (page < 1 || page > pageCount) return false;
  at = uint64_t(page - 1) * pageSize + (page == 1 ? 100 : 0);
  return read(at, out, 12);
}
bool Database::varint(uint64_t& at, uint64_t& out) {
  out = 0;
  for (int n = 0; n < 9; ++n) {
    uint8_t c;
    if (!read(at++, &c, 1)) return false;
    if (n == 8) {
      out = (out << 8) | c;
      return true;
    }
    out = (out << 7) | (c & 127);
    if (!(c & 128)) return true;
  }
  return false;
}
bool Database::cell(uint32_t page, uint16_t index, uint8_t type, uint64_t& at) {
  uint8_t ptr[2];
  const uint64_t base = uint64_t(page - 1) * pageSize, header = base + (page == 1 ? 100 : 0);
  if (!read(header + (type == 5 ? 12 : 8) + uint64_t(index) * 2, ptr, 2)) return false;
  const uint32_t offset = be(ptr, 2);
  if (page < 1 || page > pageCount || (type != 5 && type != 13) ||
      header + (type == 5 ? 12 : 8) + uint64_t(index + 1) * 2 > base + usable ||
      offset < (page == 1 ? 100u : 0u) + (type == 5 ? 12u : 8u) || offset >= usable)
    return false;
  at = base + offset;
  return true;
}
bool Database::row(uint64_t at, Row& out) {
  const uint64_t end = (at / pageSize) * pageSize + usable;
  uint64_t payload, id;
  if (!varint(at, payload) || !varint(at, id) || payload > UINT32_MAX) return false;
  const uint32_t maximum = usable - 35, minimum = (usable - 12) * 32 / 255 - 23;
  uint32_t local = uint32_t(payload);
  if (payload > maximum) {
    local = minimum + uint32_t((payload - minimum) % (usable - 4));
    if (local > maximum) local = minimum;
  }
  if (at > end || local + (payload > local ? 4u : 0u) > end - at) return false;
  uint8_t p[4]{};
  if (payload > local && !read(at + local, p, 4)) return false;
  out = {this, at, id, local, uint32_t(payload), be(p, 4)};
  return true;
}
bool Row::bytes(uint32_t at, void* out, size_t size) const {
  if (!db || at > length || size > length - at) return false;
  auto* target = static_cast<uint8_t*>(out);
  if (at < local) {
    const size_t n = std::min<size_t>(size, local - at);
    if (!db->read(offset + at, target, n)) return false;
    at += n;
    target += n;
    size -= n;
  }
  if (!size) return true;
  uint32_t page = overflow, skip = at - local, visited = 0;
  const uint32_t capacity = db->usable - 4;
  while (size) {
    if (page < 1 || page > db->pageCount || ++visited > db->pageCount) return false;
    const uint64_t base = uint64_t(page - 1) * db->pageSize;
    uint8_t next[4];
    if (!db->read(base, next, 4)) return false;
    if (skip >= capacity)
      skip -= capacity;
    else {
      const size_t n = std::min<size_t>(size, capacity - skip);
      if (!db->read(base + 4 + skip, target, n)) return false;
      target += n;
      size -= n;
      skip = 0;
    }
    page = be(next, 4);
  }
  return true;
}
bool Row::field(unsigned index, uint32_t& at, uint32_t& size, uint64_t& type) const {
  uint32_t cursor = 0;
  uint64_t headerSize;
  if (!rowVarint(*this, cursor, headerSize) || headerSize > length) return false;
  at = uint32_t(headerSize);
  for (unsigned n = 0; n <= index; ++n) {
    if (cursor >= headerSize || !rowVarint(*this, cursor, type) || cursor > headerSize) return false;
    size = serialSize(type);
    if (size == UINT32_MAX || at > length || size > length - at) return false;
    if (n == index) return true;
    at += size;
  }
  return false;
}
bool Row::integer(unsigned fieldIndex, int64_t& out) const {
  uint32_t at, size;
  uint64_t type;
  if (!field(fieldIndex, at, size, type)) return false;
  if (type == 8 || type == 9) {
    out = type - 8;
    return true;
  }
  if (type == 0) {
    out = 0;
    return true;
  }
  if (type > 6) return false;
  uint8_t data[8];
  if (!bytes(at, data, size)) return false;
  uint64_t v = 0;
  for (uint32_t n = 0; n < size; ++n) v = (v << 8) | data[n];
  if (size < 8 && size && (data[0] & 128)) v |= UINT64_MAX << (size * 8);
  std::memcpy(&out, &v, 8);
  return true;
}
bool Row::text(unsigned fieldIndex, char* out, size_t capacity) const {
  uint32_t at, size;
  uint64_t type;
  if (!out || !capacity || !field(fieldIndex, at, size, type) || !(type & 1) || type < 13 || size >= capacity)
    return false;
  if (!bytes(at, out, size)) return false;
  out[size] = 0;
  return true;
}
bool Cursor::push(uint32_t page) {
  if (depth >= 16 || !db || ++visited > db->pageCount) return false;
  uint8_t h[12];
  uint64_t at;
  if (!db->header(page, h, at) || (h[0] != 5 && h[0] != 13)) return false;
  const uint32_t count = be(h + 3, 2);
  if (count > (db->usable - (page == 1 ? 100 : 0) - (h[0] == 5 ? 12 : 8)) / 2) return false;
  levels[depth++] = {page, 0, uint16_t(count), h[0]};
  return true;
}
bool Cursor::begin(Database& database, uint32_t root) {
  db = &database;
  depth = 0;
  visited = 0;
  return push(root);
}
Result Cursor::next(Row& out) {
  while (depth) {
    auto& level = levels[depth - 1];
    uint64_t at;
    if (level.type == 13) {
      if (level.index >= level.count) {
        --depth;
        continue;
      }
      if (!db->cell(level.page, level.index++, 13, at) || !db->row(at, out)) return Result::Error;
      return Result::Row;
    }
    if (level.index > level.count) {
      --depth;
      continue;
    }
    uint8_t child[4];
    if (level.index == level.count) {
      uint8_t h[12];
      if (!db->header(level.page, h, at)) return Result::Error;
      std::memcpy(child, h + 8, 4);
    } else if (!db->cell(level.page, level.index, 5, at) || !db->read(at, child, 4))
      return Result::Error;
    ++level.index;
    if (!push(be(child, 4))) return Result::Error;
  }
  return Result::End;
}
bool Database::table(const char* name, uint32_t& root) {
  Cursor cursor;
  if (!cursor.begin(*this, 1)) return false;
  Row row;
  char type[16], tableName[80];
  for (Result status = cursor.next(row); status == Result::Row; status = cursor.next(row)) {
    int64_t value;
    if (row.text(0, type, sizeof(type)) && !std::strcmp(type, "table") && row.text(1, tableName, sizeof(tableName)) &&
        !std::strcmp(name, tableName) && row.integer(3, value) && value > 0 && uint64_t(value) <= pageCount) {
      root = uint32_t(value);
      return true;
    }
  }
  return false;
}
bool Database::find(uint32_t root, uint64_t id, Row& out) {
  uint32_t page = root;
  for (int depth = 0; depth < 16; ++depth) {
    uint8_t h[12];
    uint64_t at;
    if (!header(page, h, at)) return false;
    const uint16_t count = be(h + 3, 2);
    if ((h[0] != 5 && h[0] != 13) || count > (usable - (page == 1 ? 100u : 0u) - (h[0] == 5 ? 12u : 8u)) / 2)
      return false;
    if (h[0] == 13) {
      for (uint16_t n = 0; n < count; ++n) {
        if (!cell(page, n, 13, at) || !row(at, out)) return false;
        if (out.id == id) return true;
        if (out.id > id) return false;
      }
      return false;
    }
    if (h[0] != 5) return false;
    uint32_t child = be(h + 8, 4);
    for (uint16_t n = 0; n < count; ++n) {
      uint8_t bytes[4];
      uint64_t key;
      if (!cell(page, n, 5, at) || !read(at, bytes, 4)) return false;
      at += 4;
      if (!varint(at, key)) return false;
      if (id <= key) {
        child = be(bytes, 4);
        break;
      }
    }
    page = child;
  }
  return false;
}
}  // namespace ankisql
