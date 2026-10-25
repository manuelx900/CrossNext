#include "StudyImages.h"

#include <cstring>

namespace study {

namespace {

constexpr uint8_t kMagic[8] = {'X', 'S', 'T', 'U', 'D', 'Y', 'I', 0};

constexpr uint32_t kHeaderBytes = 16;
constexpr uint32_t kEntryBytes = 10;

uint16_t readU16(const uint8_t* p) {
  uint16_t v;
  std::memcpy(&v, p, sizeof(v));
  return v;
}

uint32_t readU32(const uint8_t* p) {
  uint32_t v;
  std::memcpy(&v, p, sizeof(v));
  return v;
}

}  // namespace

bool StudyImages::open(ByteSource& images, bool zoom) {
  count_ = 0;
  size_ = images.size();
  if (size_ < kHeaderBytes) return false;

  uint8_t header[kHeaderBytes];
  if (!images.read(0, header, sizeof(header))) return false;
  if (std::memcmp(header, kMagic, sizeof(kMagic)) != 0) return false;
  version_ = readU16(header + 8);
  if (version_ < 1 || version_ > 4) return false;
  if ((version_ >= 3) != zoom) return false;

  const uint32_t count = readU32(header + 12);
  // The index alone has to fit, or the file is truncated -- the realistic
  // failure for something copied onto an SD card.
  if (count == 0 || count > 4000000u) return false;
  if (kHeaderBytes + static_cast<uint64_t>(count) * kEntryBytes * (version_ >= 2 ? 2 : 1) > size_) return false;

  count_ = static_cast<int>(count);
  return true;
}

ImageRef StudyImages::at(ByteSource& images, const int index, const bool answer) const {
  ImageRef out;
  if (index < 0 || index >= count_ || (version_ == 1 && !answer)) return out;
  const uint32_t entryIndex =
      version_ >= 2 ? static_cast<uint32_t>(index) * 2 + (answer ? 1 : 0) : static_cast<uint32_t>(index);

  uint8_t entry[kEntryBytes];
  if (!images.read(kHeaderBytes + entryIndex * kEntryBytes, entry, sizeof(entry))) {
    return out;
  }
  const uint32_t offset = readU32(entry);
  const uint16_t width = readU16(entry + 4);
  const uint16_t height = readU16(entry + 6);
  const uint16_t encodedStride = readU16(entry + 8);
  const uint16_t stride = version_ == 4 ? encodedStride & 0x7fff : encodedStride;
  if (width == 0 || height == 0) return out;  // this card has no photograph

  // Everything below is a corrupt or truncated file rather than an absent
  // image, and is refused rather than drawn: a wrong stride reads a picture
  // sheared into diagonal noise, which looks like a rendering bug and would be
  // chased in the wrong place.
  if (width > (version_ >= 3 ? kMaxZoomWidth : kMaxImageWidth) ||
      height > (version_ >= 3 ? kMaxZoomHeight : kMaxImageHeight))
    return out;
  const int rowBytes = version_ == 4 ? width : (width + 7) / 8;
  const int maximumStride = version_ == 4 ? kZoomRowBytes : version_ == 3 ? (kMaxZoomWidth + 7) / 8 : kImageRowBytes;
  if (stride < rowBytes || stride > maximumStride) return out;
  const uint64_t bytes = static_cast<uint64_t>(stride) * height;
  if (offset < kHeaderBytes + static_cast<uint64_t>(count_) * kEntryBytes * (version_ >= 2 ? 2 : 1) ||
      offset + bytes > size_)
    return out;

  out.offset = offset;
  out.width = width;
  out.height = height;
  out.stride = stride;
  out.bits = version_ == 4 ? 8 : 1;
  out.highContrast = version_ == 4 && (encodedStride & 0x8000);
  return out;
}

int StudyImages::readBand(ByteSource& images, const ImageRef& image, const int firstRow, uint8_t* out) const {
  if (!image.valid() || out == nullptr) return 0;
  if (image.stride > (version_ >= 3 ? kZoomRowBytes : kImageRowBytes)) return 0;
  if (firstRow < 0 || firstRow >= image.height) return 0;

  int rows = image.height - firstRow;
  if (rows > kImageBandRows) rows = kImageBandRows;
  const uint32_t bytes = static_cast<uint32_t>(rows) * image.stride;
  if (!images.read(image.offset + static_cast<uint32_t>(firstRow) * image.stride, out, bytes)) return 0;
  return rows;
}

}  // namespace study
