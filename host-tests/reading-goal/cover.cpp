#include <Bitmap.h>
#include <Xtc.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include "../../lib/Xtc/Xtc/CoverPixels.h"
std::vector<uint8_t> fixture(bool gray, uint64_t metadataOffset) {
  constexpr uint32_t table = 384, pages = 4, width = 24, height = 16;
  const uint32_t payload = width * height / 8 * (gray ? 2 : 1);
  const uint32_t pageSize = sizeof(xtc::XtgPageHeader) + payload;
  std::vector<uint8_t> bytes(table + pages * sizeof(xtc::PageTableEntry) + pages * pageSize, 0);
  xtc::XtcHeader header{};
  header.magic = gray ? xtc::XTCH_MAGIC : xtc::XTC_MAGIC;
  header.versionMajor = 1;
  header.pageCount = pages;
  header.hasMetadata = 1;
  header.metadataOffset = metadataOffset;
  header.pageTableOffset = table;
  header.dataOffset = table + pages * sizeof(xtc::PageTableEntry);
  std::memcpy(bytes.data(), &header, sizeof(header));
  // Zero-offset files retain the legacy fixed metadata location.
  const uint64_t at = metadataOffset ? metadataOffset : sizeof(header);
  if (at <= table && 192 <= table - at) {
    std::memcpy(bytes.data() + at, "Reading test", 13);
    std::memcpy(bytes.data() + at + 128, "Test author", 12);
  }
  for (uint32_t n = 0; n < pages; ++n) {
    xtc::PageTableEntry entry{header.dataOffset + n * pageSize, pageSize, width, height};
    std::memcpy(bytes.data() + table + n * sizeof(entry), &entry, sizeof(entry));
    xtc::XtgPageHeader page{gray ? xtc::XTH_MAGIC : xtc::XTG_MAGIC, width, height, 0, 0, payload, 0};
    std::memcpy(bytes.data() + entry.dataOffset, &page, sizeof(page));
    std::memset(bytes.data() + entry.dataOffset + sizeof(page), 0x55, payload);
  }
  return bytes;
}
int main() {
  int checks = 0, failures = 0;
  auto check = [&](bool v) {
    ++checks;
    if (!v) {
      ++failures;
      std::printf("Cover check %d failed\n", checks);
    }
  };
  for (bool gray : {false, true})
    for (bool ccw : {false, true}) {
      testStorage = {};
      auto bytes = fixture(gray, 64);
      constexpr unsigned w = 24, h = 16;
      const auto* header = reinterpret_cast<const xtc::XtcHeader*>(bytes.data());
      xtc::PageTableEntry entry;
      std::memcpy(&entry, bytes.data() + header->pageTableOffset, sizeof(entry));
      auto* pixels = bytes.data() + entry.dataOffset + sizeof(xtc::XtgPageHeader);
      for (unsigned n = 0; n < w * h / 8 * (gray ? 2 : 1); ++n) pixels[n] = uint8_t(n * 41 + n / 7);
      testStorage.files["/book.xtc"] = bytes;
      Xtc book("/book.xtc", "/.crosspoint", ccw);
      check(book.load());
      check(book.generateCoverBmp());
      auto& bmp = testStorage.files[book.getCoverBmpPath()];
      auto get = [&](size_t at, unsigned n) {
        uint32_t v = 0;
        for (unsigned k = 0; k < n; ++k) v |= uint32_t(bmp[at + k]) << (8 * k);
        return v;
      };
      check(get(18, 4) == (ccw ? h : w));
      check(int32_t(get(22, 4)) == -int32_t(ccw ? w : h));
      const auto off = get(10, 4), width = get(18, 4);
      const int height = -int32_t(get(22, 4));
      const unsigned depth = gray ? 2 : 1, row = ((width * depth + 31) / 32) * 4;
      check(bmp.size() == off + row * height);
      for (unsigned y = 0; y < unsigned(height); ++y)
        for (unsigned x = 0; x < width; ++x) {
          const unsigned v = gray ? ((bmp[off + y * row + x / 4] >> (6 - 2 * (x % 4))) & 3) * 85
                                  : (bmp[off + y * row + x / 8] & (128 >> (x % 8)) ? 255 : 0);
          check(v == xtc::coverGray(pixels, w, h, depth, x, y, ccw));
        }
      check(book.generateThumbBmp(100));
      check(testStorage.files[book.getThumbBmpPath(100)] == bmp);
    }
  std::printf("XTC/XTCH generated BMPs: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
