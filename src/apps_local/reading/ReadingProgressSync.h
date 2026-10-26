#pragma once

#include "ReadingProgressBooks.h"
#include "ReadingStatsJson.h"
#include "ReadingStatsUpload.h"

namespace readingstats::cloud {
class ProgressSync {
 public:
  explicit ProgressSync(GfxRenderer& renderer) : books(renderer), json(&budget) {}
  Result begin(Exchange exchange, void* context);
  Result step();
  void cancel();
  uint32_t processed() const { return count; }

 private:
  enum class Stage { Scan, Prepare, Primary, Alternate, Resolve, Upload, Apply };
  ProgressBooks books;
  ProgressBook book;
  KOReaderProgress primary{}, alternate{};
  CrossPointPosition destination{};
  JsonBudget budget{65536};
  JsonDocument json;
  std::unique_ptr<char[]> body;
  char path[128]{};
  Exchange exchange = nullptr;
  void* context = nullptr;
  Stage stage = Stage::Scan;
  Result result = Result::Idle;
  uint32_t count = 0;
  bool hasPrimary = false, hasAlternate = false;
  Result fetch(const std::string& document, KOReaderProgress& remote, bool& found);
  Result upload();
  Result stop(Result reason);
  Result completeBook();
};
}  // namespace readingstats::cloud
