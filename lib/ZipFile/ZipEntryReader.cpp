#include "ZipEntryReader.h"

#include <Logging.h>

#include <algorithm>
#include <utility>

#include "ZipFile.h"

bool ZipEntryReader::open(const std::string& archive, const char* entry) {
  close();
  ZipFile zip(archive);
  ZipFile::FileStatSlim stat{};
  if (!zip.open() || !zip.loadFileStatSlim(entry, &stat)) return false;
  const long offset = zip.getDataOffset(stat);
  if (offset < 0 || (stat.method != 0 && stat.method != 8)) return false;
  if (stat.method == 0 && stat.compressedSize != stat.uncompressedSize) return false;
  method = stat.method;
  expected = stat.uncompressedSize;
  remaining = stat.compressedSize;
  file = std::move(zip.file);
  if (!file.seek(offset)) {
    close();
    return false;
  }
  // Reuse the existing ~8 KiB state + 32 KiB window for this open chapter.
  // Forward reads need this dictionary; a task-stack or whole-book buffer cannot replace it.
  if (method == 8) {
    if (!inflate.init(true)) {
      LOG_ERR("ZIP", "OOM: incremental entry inflater");
      close();
      return false;
    }
    inflate.setFill(fill, this);
  }
  return true;
}

size_t ZipEntryReader::fill(void* context, const uint8_t** bytes) {
  auto& self = *static_cast<ZipEntryReader*>(context);
  if (!self.remaining) return 0;
  const int read = self.file.read(self.input, std::min(self.remaining, sizeof(self.input)));
  if (read <= 0) return 0;
  self.remaining -= size_t(read);
  *bytes = self.input;
  return size_t(read);
}

ZipEntryReader::Status ZipEntryReader::read(uint8_t* output, size_t capacity, size_t& produced) {
  produced = 0;
  if (!file || !capacity) return Status::Error;
  Status status = Status::Ok;
  if (method == 0) {
    if (!remaining) return Status::Done;
    const int read = file.read(output, std::min(remaining, capacity));
    if (read <= 0) return Status::Error;
    produced = size_t(read);
    remaining -= produced;
    status = remaining ? Status::Ok : Status::Done;
  } else {
    status = inflate.readAtMost(output, capacity, &produced);
  }
  total += produced;
  if (total > expected || (status == Status::Done && total != expected)) return Status::Error;
  return status;
}

void ZipEntryReader::close() {
  inflate.deinit();
  if (file) file.close();
  remaining = expected = total = 0;
}
