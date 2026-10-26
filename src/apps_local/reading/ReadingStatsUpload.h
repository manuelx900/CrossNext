#pragma once
#include "ReadingStatsProjection.h"
namespace readingstats::cloud {
using Exchange = Result (*)(void*, const char* method, const char* path, const char* body, const char* destination);
class Uploader {
 public:
  Result begin(int today, uint64_t device, uint64_t account, Exchange exchange, void* context);
  Result step();
  void cancel() {
    result = Result::Cancelled;
    projection.close();
  }
  Projection& data() { return projection; }
  const char* deviceId() const { return device; }

 private:
  enum class Stage { Check, CheckBooks, Days, Books, Done };
  Stage stage = Stage::Check;
  Result result = Result::Idle;
  Projection projection;
  Book book;
  Exchange exchange = nullptr;
  void* context = nullptr;
  char device[96]{}, path[1664]{}, history[125]{}, response[256]{};
  char cursor[513]{}, lastDocument[33]{};
  uint32_t checks = 0;
  uint32_t nextBook = 0;
  int nextDay = 10957;
  Result request(const char* route, const char* body, uint32_t books, uint32_t days);
  Result global();
  Result books();
  Result check();
  Result checkBooks();
};
}  // namespace readingstats::cloud
