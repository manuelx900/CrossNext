#include "FlashMedia.h"

#include <Arduino.h>
#include <JpegToBmpConverter.h>
#include <Logging.h>
#include <PngToBmpConverter.h>
#include <ZipFile.h>
#include <strings.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "FlashJpegDetail.h"
#include "FlashText.h"

namespace flashcards {
namespace {
constexpr uint32_t kMaxMediaBytes = 16 * 1024 * 1024;
void put(uint8_t* p, uint32_t v, int n) {
  for (int i = 0; i < n; ++i) p[i] = uint8_t(v >> (8 * i));
}
uint32_t u32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
class Sink final : public Print {
 public:
  Sink(HalFile& f, ProgressReporter reporter) : file(f), progress(reporter) {}
  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t* p, size_t n) override {
    if (n > kMaxMediaBytes - total) return 0;
    const size_t result = file.write(p, n);
    total += result;
    if (total - yielded >= 8192) {
      progress.report(ImportPhase::Images);
      delay(1);
      yielded = total;
    }
    return result;
  }

 private:
  HalFile& file;
  ProgressReporter progress;
  size_t total = 0, yielded = 0;
};
class Reader {
 public:
  Reader(HalFile& f, uint8_t* b, ProgressReporter value) : file(f), buffer(b), progress(value) {}
  int read() {
    if (at == size) {
      const int n = file.read(buffer, 512);
      if (n <= 0) return -1;
      size = size_t(n);
      at = 0;
      bytes += size;
      if (bytes - yielded >= 8192) {
        progress.report(ImportPhase::Images);
        delay(1);
        yielded = bytes;
      }
    }
    return buffer[at++];
  }
  bool available() { return at < size || file.available(); }

 private:
  HalFile& file;
  uint8_t* buffer;
  ProgressReporter progress;
  size_t at = 0, size = 0, bytes = 0, yielded = 0;
};
// JSON media values are filenames, never filesystem destinations.
bool jsonString(Reader& f, char* out, size_t cap) {
  size_t at = 0;
  bool fits = true;
  for (int c = f.read(); c >= 0; c = f.read()) {
    if (c == '"') {
      out[at] = 0;
      return fits;
    }
    if (c == '\\') {
      c = f.read();
      if (c == 'u') {
        unsigned code = 0;
        for (int i = 0; i < 4; ++i) {
          const int h = f.read();
          const int digit = h >= '0' && h <= '9'   ? h - '0'
                            : h >= 'a' && h <= 'f' ? h - 'a' + 10
                            : h >= 'A' && h <= 'F' ? h - 'A' + 10
                                                   : -1;
          if (digit < 0) return false;
          code = code * 16 + unsigned(digit);
        }
        // Refuse surrogate names rather than resolving a different asset.
        if (!code || (code >= 0xd800 && code <= 0xdfff)) return false;
        char bytes[3];
        int n = 0;
        if (code < 128)
          bytes[n++] = char(code);
        else if (code < 2048) {
          bytes[n++] = char(0xc0 | (code >> 6));
          bytes[n++] = char(0x80 | (code & 63));
        } else {
          bytes[n++] = char(0xe0 | (code >> 12));
          bytes[n++] = char(0x80 | ((code >> 6) & 63));
          bytes[n++] = char(0x80 | (code & 63));
        }
        for (int i = 0; i < n; ++i) {
          if (at + 1 < cap)
            out[at++] = bytes[i];
          else
            fits = false;
        }
        continue;
      }
      if (c != '"' && c != '\\' && c != '/') return false;
    }
    if (c < 32) return false;
    if (at + 1 < cap)
      out[at++] = char(c);
    else
      fits = false;
  }
  return false;
}
bool firstImage(const char* html, char* name, size_t cap) {
  name[0] = 0;
  if (!html) return false;
  for (const char* p = html; *p; ++p) {
    if (*p != '<' || strncasecmp(p, "<img", 4)) continue;
    const char* end = std::strchr(p, '>');
    if (!end) return false;
    for (p += 4; p < end; ++p) {
      if (strncasecmp(p, "src", 3)) continue;
      p += 3;
      while (p < end && *p == ' ') ++p;
      if (p == end || *p++ != '=') continue;
      while (p < end && *p == ' ') ++p;
      const char quote = (*p == '"' || *p == '\'') ? *p++ : 0;
      size_t at = 0;
      while (p < end && (quote ? *p != quote : *p != ' ' && *p != '/')) {
        if (at + 1 >= cap) return false;
        name[at++] = *p++;
      }
      name[at] = 0;
      return at > 0;
    }
    return false;
  }
  return false;
}
}  // namespace

FlashMedia::~FlashMedia() { cleanup(); }
void FlashMedia::cleanup() {
  entries.close();
  pixels.close();
  zoomEntries.close();
  zoomPixels.close();
  if (!directory) return;
  for (const auto* suffix : {".media", ".images.index", ".images.pixels", ".zoom.index", ".zoom.pixels", ".picture",
                             ".bitmap", "images.dat.part", "images.zoom.dat.part"}) {
    std::snprintf(path, sizeof(path), "%s/%s", directory, suffix);
    if (Storage.exists(path) && !Storage.remove(path)) LOG_ERR("FLASH", "Could not remove image scratch %s", path);
  }
}
bool FlashMedia::begin(const char* source, const char* folder) {
  archive = source;
  directory = folder;
  std::snprintf(path, sizeof(path), "%s/.images.index", directory);
  if (!Storage.openFileForWrite("FLASH", path, entries)) return false;
  std::snprintf(path, sizeof(path), "%s/.images.pixels", directory);
  if (!Storage.openFileForWrite("FLASH", path, pixels)) return false;
  std::snprintf(path, sizeof(path), "%s/.zoom.index", directory);
  if (!Storage.openFileForWrite("FLASH", path, zoomEntries)) return false;
  std::snprintf(path, sizeof(path), "%s/.zoom.pixels", directory);
  if (!Storage.openFileForWrite("FLASH", path, zoomPixels)) return false;
  if (!archive) return true;
  const std::string sourcePath(archive);
  ZipFile zip(sourcePath);
  size_t size = 0;
  if (!zip.getInflatedFileSize("media", &size) || size > kMaxMediaBytes) return true;
  std::snprintf(path, sizeof(path), "%s/.media", directory);
  HalFile file;
  if (!Storage.openFileForWrite("FLASH", path, file)) return false;
  Sink sink(file, progress);
  available = zip.readFileToStream("media", sink, 4096);
  return available;
}
bool FlashMedia::locate() {
  std::snprintf(path, sizeof(path), "%s/.media", directory);
  HalFile file;
  if (!Storage.openFileForRead("FLASH", path, file)) return false;
  Reader reader(file, ioBuffer, progress);
  // Single bounded pair at a time, independent of the media map's total size.
  while (reader.available()) {
    int c = reader.read();
    if (c != '"') continue;
    if (!jsonString(reader, member, sizeof(member))) return false;
    do {
      c = reader.read();
    } while (c == ' ' || c == '\n' || c == '\r' || c == '\t');
    if (c != ':') return false;
    do {
      c = reader.read();
    } while (c == ' ' || c == '\n' || c == '\r' || c == '\t');
    if (c != '"' || !jsonString(reader, value, sizeof(value))) return false;
    if (!std::strcmp(value, filename)) return std::strspn(member, "0123456789") == std::strlen(member) && member[0];
  }
  return false;
}
bool FlashMedia::convert(bool answer) {
  progress.report(ImportPhase::Images);
  if (!locate()) return false;
  const std::string sourcePath(archive);
  ZipFile zip(sourcePath);
  size_t size = 0;
  if (!zip.getInflatedFileSize(member, &size) || !size || size > kMaxMediaBytes) return false;
  std::snprintf(path, sizeof(path), "%s/.picture", directory);
  HalFile image;
  if (!Storage.openFileForWrite("FLASH", path, image)) return false;
  Sink sink(image, progress);
  if (!zip.readFileToStream(member, sink, 4096)) return false;
  image.close();
  if (!convertBitmap(refs[answer ? 1 : 0], pixels, study::kMaxImageWidth, study::kMaxImageHeight)) return false;
  ++pictures;
  if (!convertBitmap(zoomRefs[answer ? 1 : 0], zoomPixels, study::kMaxZoomWidth, study::kMaxZoomHeight)) {
    LOG_ERR("FLASH", "High-resolution image unavailable: %s", filename);
    zoomRefs[answer ? 1 : 0] = {};
  }
  return true;
}
bool FlashMedia::convertBitmap(study::ImageRef& ref, HalFile& destination, int maxWidth, int maxHeight) {
  std::snprintf(path, sizeof(path), "%s/.picture", directory);
  HalFile image;
  if (!Storage.openFileForRead("FLASH", path, image)) return false;
  std::snprintf(path, sizeof(path), "%s/.bitmap", directory);
  HalFile bitmap;
  if (!Storage.openFileForWrite("FLASH", path, bitmap)) return false;
  uint8_t header[62]{};
  if (image.read(header, 8) != 8 || !image.seekSet(0)) return false;
  Sink converted(bitmap, progress);
  bool ok = false;
  // A zero target retains decoder-native pixels; a generous fit box would upscale.
  const bool native = maxWidth == study::kMaxZoomWidth && maxHeight == study::kMaxZoomHeight;
  const int targetWidth = native ? 0 : maxWidth, targetHeight = native ? 0 : maxHeight;
  if (native && header[0] == 0xff && header[1] == 0xd8) {
    if (progressiveJpegDetail(image, destination, ref)) return true;
    if (!image.seekSet(0)) return false;
  }
  if (!std::memcmp(header, "\x89PNG\r\n\x1a\n", 8))
    ok = native ? PngToBmpConverter::pngFileToGrayBmpStreamNative(image, converted)
                : PngToBmpConverter::pngFileTo1BitBmpStreamFitWithin(image, converted, targetWidth, targetHeight);
  else if (header[0] == 0xff && header[1] == 0xd8)
    ok = native ? JpegToBmpConverter::jpegFileToGrayBmpStreamNative(image, converted)
                : JpegToBmpConverter::jpegFileTo1BitBmpStreamFitWithin(image, converted, targetWidth, targetHeight);
  bitmap.close();
  if (!ok || !Storage.openFileForRead("FLASH", path, bitmap) || bitmap.read(header, 62) != 62) return false;
  const uint32_t width = u32(header + 18), rawHeight = u32(header + 22), offset = u32(header + 10);
  const int32_t signedHeight = int32_t(rawHeight);
  const uint32_t height = signedHeight < 0 ? uint32_t(-int64_t(signedHeight)) : rawHeight;
  if (header[0] != 'B' || header[1] != 'M' || header[28] != (native ? 8 : 1) || header[29] || u32(header + 30) ||
      !width || width > unsigned(maxWidth) || !height || height > unsigned(maxHeight))
    return false;
  const uint32_t sourceStride = (width * (native ? 8 : 1) + 31) / 32 * 4;
  if (uint64_t(offset) + uint64_t(sourceStride) * height > bitmap.fileSize64()) return false;
  if (destination.position() > 512u * 1024 * 1024 - height * (native ? width : (width + 7) / 8)) return false;
  ref = {uint32_t(destination.position()), uint16_t(width), uint16_t(height),
         uint16_t(native ? width : (width + 7) / 8), uint8_t(native ? 8 : 1)};
  auto* row = ioBuffer;
  const bool zeroIsBlack = header[54] + header[55] + header[56] < header[58] + header[59] + header[60];
  uint32_t nearBlackWhite = 0;
  for (uint32_t y = 0; y < height; ++y) {
    const uint32_t sourceY = signedHeight < 0 ? y : height - 1 - y;
    if (!bitmap.seekSet(offset + sourceY * sourceStride) || bitmap.read(row, ref.stride) != ref.stride) return false;
    if (native)
      for (uint32_t x = 0; x < width; ++x) nearBlackWhite += row[x] < 32 || row[x] > 223;
    if (!native && zeroIsBlack)
      for (unsigned x = 0; x < ref.stride; ++x) row[x] = ~row[x];
    if (destination.write(row, ref.stride) != ref.stride) return false;
    if (!(y % 16)) {
      progress.report(ImportPhase::Images);
      delay(1);
    }
  }
  ref.highContrast = native && uint64_t(nearBlackWhite) * 100 >= uint64_t(width) * height * 95;
  return true;
}
void FlashMedia::prepare(const char* question, const char* answer) {
  refs[0] = {};
  refs[1] = {};
  zoomRefs[0] = {};
  zoomRefs[1] = {};
  prepareFace(question, false);
  prepareFace(answer, true);
}
void FlashMedia::prepareFace(const char* html, bool answer) {
  refs[answer ? 1 : 0] = {};
  zoomRefs[answer ? 1 : 0] = {};
  if (!available || !firstImage(html, filename, sizeof(filename))) return;
  if (!flashtext::unescape(filename, value, sizeof(value))) return;
  size_t at = 0;
  for (const char* p = value; *p;) {
    if (*p == '%' && p[1] && p[2]) {
      char hex[3] = {p[1], p[2], 0};
      char* end = nullptr;
      const long code = std::strtol(hex, &end, 16);
      if (end == hex + 2 && code > 0) {
        filename[at++] = char(code);
        p += 3;
        continue;
      }
    }
    filename[at++] = *p++;
  }
  filename[at] = 0;
  if (!convert(answer)) {
    LOG_ERR("FLASH", "Unsupported or missing image: %s", filename);
    refs[answer ? 1 : 0] = {};
  }
}

bool FlashMedia::append() {
  uint8_t bytes[20]{};
  for (int store = 0; store < 2; ++store) {
    const auto* images = store ? zoomRefs : refs;
    for (int n = 0; n < 2; ++n) {
      put(bytes + n * 10, images[n].offset, 4);
      put(bytes + n * 10 + 4, images[n].width, 2);
      put(bytes + n * 10 + 6, images[n].height, 2);
      put(bytes + n * 10 + 8, images[n].stride | (store && images[n].highContrast ? 0x8000 : 0), 2);
    }
    auto& index = store ? zoomEntries : entries;
    if (index.write(bytes, sizeof(bytes)) != sizeof(bytes)) return false;
  }
  return true;
}
bool FlashMedia::finish(uint32_t count) {
  progress.report(ImportPhase::SavingImages);
  entries.close();
  pixels.close();
  zoomEntries.close();
  zoomPixels.close();
  if (!pictures) {
    cleanup();
    return true;
  }
  const bool ok = finishStore(count, false) && finishStore(count, true);
  cleanup();
  return ok;
}
bool FlashMedia::finishStore(uint32_t count, bool zoom) {
  auto& index = zoom ? zoomEntries : entries;
  auto& data = zoom ? zoomPixels : pixels;
  std::snprintf(path, sizeof(path), "%s/%s.part", directory, zoom ? "images.zoom.dat" : "images.dat");
  HalFile file;
  if (!Storage.openFileForWrite("FLASH", path, file)) return false;
  uint8_t bytes[64]{};
  std::memcpy(bytes, "XSTUDYI", 7);
  put(bytes + 8, zoom ? 4 : 2, 2);
  put(bytes + 12, count, 4);
  if (file.write(bytes, 16) != 16) return false;
  std::snprintf(path, sizeof(path), "%s/%s", directory, zoom ? ".zoom.index" : ".images.index");
  if (!Storage.openFileForRead("FLASH", path, index) || index.fileSize64() != uint64_t(count) * 20) return false;
  const uint32_t base = 16 + count * 20;
  for (uint32_t n = 0; n < count * 2; ++n) {
    if (index.read(bytes, 10) != 10) return false;
    if (bytes[4] || bytes[5]) {
      const uint64_t at = uint64_t(base) + u32(bytes);
      if (at > UINT32_MAX) return false;
      put(bytes, uint32_t(at), 4);
    }
    if (file.write(bytes, 10) != 10) return false;
    if (!(n % 512)) {
      progress.report(ImportPhase::SavingImages);
      delay(1);
    }
  }
  std::snprintf(path, sizeof(path), "%s/%s", directory, zoom ? ".zoom.pixels" : ".images.pixels");
  if (!Storage.openFileForRead("FLASH", path, data)) return false;
  while (data.available()) {
    const int n = data.read(ioBuffer, sizeof(ioBuffer));
    if (n <= 0 || file.write(ioBuffer, n) != size_t(n)) return false;
    if (!(data.position() % 8192)) {
      progress.report(ImportPhase::SavingImages, data.position(), data.fileSize64());
      delay(1);
    }
  }
  file.close();
  std::snprintf(path, sizeof(path), "%s/%s.part", directory, zoom ? "images.zoom.dat" : "images.dat");
  char target[160];
  std::snprintf(target, sizeof(target), "%s/%s", directory, zoom ? "images.zoom.dat" : "images.dat");
  return Storage.replaceFile(path, target);
}
}  // namespace flashcards
