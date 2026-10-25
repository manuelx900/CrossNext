#pragma once
#include <cstddef>
#include <cstdint>
namespace ankisql {
class Source {
 public:
  virtual ~Source() = default;
  virtual bool read(uint64_t, void*, size_t) = 0;
  virtual uint64_t size() const = 0;
};
class Database;
struct Row {
  Database* db = nullptr;
  uint64_t offset = 0, id = 0;
  uint32_t local = 0, length = 0, overflow = 0;
  bool bytes(uint32_t, void*, size_t) const;
  bool integer(unsigned, int64_t&) const;
  bool text(unsigned, char*, size_t) const;

 private:
  bool field(unsigned, uint32_t&, uint32_t&, uint64_t&) const;
};
enum class Result : uint8_t { Row, End, Error };
class Cursor {
 public:
  bool begin(Database&, uint32_t);
  Result next(Row&);

 private:
  Database* db = nullptr;
  struct Level {
    uint32_t page = 0;
    uint16_t index = 0, count = 0;
    uint8_t type = 0;
  };
  Level levels[16]{};
  int depth = 0;
  uint32_t visited = 0;
  bool push(uint32_t);
};
class Database {
 public:
  explicit Database(Source& s) : source(s) {}
  bool open();
  bool table(const char*, uint32_t&);
  bool find(uint32_t, uint64_t, Row&);
  uint32_t pages() const { return pageCount; }

 private:
  friend struct Row;
  friend class Cursor;
  Source& source;
  uint32_t pageSize = 0, usable = 0, pageCount = 0;
  bool read(uint64_t, void*, size_t);
  bool header(uint32_t, uint8_t*, uint64_t&);
  bool cell(uint32_t, uint16_t, uint8_t, uint64_t&);
  bool row(uint64_t, Row&);
  bool varint(uint64_t&, uint64_t&);
};
}  // namespace ankisql
