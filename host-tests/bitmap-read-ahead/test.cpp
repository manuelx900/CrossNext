#include <Bitmap.h>
#include <HalStorage.h>

#include <algorithm>
#include <cstdio>
#include <vector>

namespace {
int checks = 0, failures = 0;
void check(bool ok) {
  ++checks;
  if (!ok) ++failures;
}
void put(std::vector<uint8_t>& bytes, size_t at, uint32_t value, int count) {
  for (int n = 0; n < count; ++n) bytes[at + n] = value >> (n * 8);
}
std::vector<uint8_t> fixture(int width, int height, int bits, bool topDown) {
  const int palette = bits <= 8 ? 1 << bits : 0, row = (width * bits + 31) / 32 * 4;
  const int offset = 54 + palette * 4;
  std::vector<uint8_t> bytes(offset + row * height);
  put(bytes, 0, 0x4d42, 2);
  put(bytes, 2, bytes.size(), 4);
  put(bytes, 10, offset, 4);
  put(bytes, 14, 40, 4);
  put(bytes, 18, width, 4);
  put(bytes, 22, topDown ? -height : height, 4);
  put(bytes, 26, 1, 2);
  put(bytes, 28, bits, 2);
  for (int n = 0; n < palette; n++)
    for (int c = 0; c < 3; c++) bytes[54 + n * 4 + c] = n * 255 / (palette - 1);
  for (size_t n = offset; n < bytes.size(); n++) bytes[n] = static_cast<uint8_t>(n * 37 + n / 19);
  return bytes;
}
std::vector<uint8_t> decode(HalFile& file, Bitmap& bitmap) {
  std::vector<uint8_t> decoded((bitmap.getWidth() + 3) / 4), source(bitmap.getRowBytes()), all;
  for (int row = 0; row < bitmap.getHeight(); row++) {
    check(bitmap.readNextRow(decoded.data(), source.data()) == BmpReaderError::Ok);
    all.insert(all.end(), decoded.begin(), decoded.end());
  }
  return all;
}
}  // namespace
int main() {
  for (int bits : {1, 2, 4, 8, 24, 32})
    for (bool topDown : {false, true})
      for (int width : {1, 31, 257}) {
        const auto bytes = fixture(width, 37, bits, topDown);
        testStorage.files["/test.bmp"] = bytes;
        HalFile file;
        check(Storage.openFileForRead("TEST", "/test.bmp", file));
        Bitmap bitmap(file);
        const auto calls = testStorage.readCalls;
        check(bitmap.parseHeaders() == BmpReaderError::Ok);
        check(testStorage.readCalls - calls == size_t(1 + (bits <= 8 ? 1 << bits : 0)));
        check(bitmap.getWidth() == width && bitmap.getHeight() == 37 && bitmap.getBpp() == bits);
        check(bitmap.isTopDown() == topDown);
        const auto expected = decode(file, bitmap);
        const int rowBytes = bitmap.getRowBytes();
        for (size_t capacity :
             {size_t(0), size_t(rowBytes - 1), size_t(rowBytes), size_t(rowBytes + 3), size_t(4096)}) {
          std::vector<uint8_t> cache(std::max(capacity, size_t(1)));
          bitmap.setReadAheadBuffer(cache.data(), capacity);
          check(bitmap.parseHeaders() == BmpReaderError::Ok);
          check(decode(file, bitmap) == expected);
          check(bitmap.rewindToData() == BmpReaderError::Ok);
          check(decode(file, bitmap) == expected);
          check(bitmap.parseHeaders() == BmpReaderError::Ok);
          check(decode(file, bitmap) == expected);
          // Refill must stop at the final source row, never consume unrelated trailing bytes.
          check(file.position() == bytes.size());
        }
        file.close();
        testStorage.files["/test.bmp"].pop_back();
        check(Storage.openFileForRead("TEST", "/test.bmp", file));
        std::vector<uint8_t> cache(4096), source(rowBytes), decoded((width + 3) / 4);
        bitmap.setReadAheadBuffer(cache.data(), cache.size());
        check(bitmap.parseHeaders() == BmpReaderError::Ok);
        for (int row = 0; row < 36; row++)
          check(bitmap.readNextRow(decoded.data(), source.data()) == BmpReaderError::Ok);
        check(bitmap.readNextRow(decoded.data(), source.data()) == BmpReaderError::ShortReadRow);
      }
  const auto completeHeader = fixture(31, 37, 1, false);
  for (bool topDown : {false, true})
    for (int width : {1, 7, 8, 31, 257, 2048})
      for (auto palette : {0, 1, 2, 3}) {
        auto bytes = fixture(width, 9, 1, topDown);
        for (int n = 0; n < 2; ++n)
          for (int c = 0; c < 3; ++c) bytes[54 + n * 4 + c] = (palette & (1 << n)) ? 255 : 0;
        testStorage.files["/packed.bmp"] = bytes;
        HalFile file;
        check(Storage.openFileForRead("TEST", "/packed.bmp", file));
        Bitmap bitmap(file);
        check(bitmap.parseHeaders() == BmpReaderError::Ok);
        const auto expected = decode(file, bitmap);
        for (int capacity : {0, 4096}) {
          check(bitmap.rewindToData() == BmpReaderError::Ok);
          std::vector<uint8_t> packed(bitmap.getRowBytes()), cache(std::max(1, capacity));
          bitmap.setReadAheadBuffer(cache.data(), capacity);
          const auto calls = testStorage.readCalls;
          for (int y = 0; y < 9; ++y) {
            check(bitmap.readNext1BitRow(packed.data()) == BmpReaderError::Ok);
            for (int x = 0; x < width; ++x) {
              const bool white = ((expected[y * ((width + 3) / 4) + x / 4] >> (6 - x % 4 * 2)) & 3) == 3;
              check(bool(packed[x / 8] & (0x80 >> (x % 8))) == white);
            }
          }
          check(testStorage.readCalls - calls ==
                size_t(capacity
                           ? (9 * bitmap.getRowBytes() + (capacity / bitmap.getRowBytes()) * bitmap.getRowBytes() - 1) /
                                 ((capacity / bitmap.getRowBytes()) * bitmap.getRowBytes())
                           : 9));
          check(bitmap.readNext1BitRow(packed.data()) == BmpReaderError::ShortReadRow);
        }
      }
  for (size_t length = 0; length < 54; ++length) {
    testStorage.files["/short.bmp"] = std::vector<uint8_t>(completeHeader.begin(), completeHeader.begin() + length);
    HalFile shortFile;
    check(Storage.openFileForRead("TEST", "/short.bmp", shortFile));
    Bitmap shortBitmap(shortFile);
    check(shortBitmap.parseHeaders() != BmpReaderError::Ok);
  }
  std::printf("bitmap-read-ahead: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
