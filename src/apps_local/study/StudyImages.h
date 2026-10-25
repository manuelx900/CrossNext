#pragma once

// Bounded preview/HD images read from SD a band at a time. Version 1 carries
// answer photographs; version 2 carries a question/answer pair per card.
// No full-image allocation or second framebuffer is needed.

#include <cstdint>

#include "StudyDeck.h"

namespace study {

// Sized from the format's own ceiling: 448px wide is 56 bytes a row, and
// sixteen rows a time turns 620 SD reads into 39 without putting a kilobyte on
// the stack.
inline constexpr int kMaxImageWidth = 448;
inline constexpr int kMaxImageHeight = 620;
inline constexpr int kImageRowBytes = (kMaxImageWidth + 7) / 8;
inline constexpr int kImageBandRows = 16;
inline constexpr int kImageBandBytes = kImageRowBytes * kImageBandRows;
// Optional v3 monochrome / v4 grayscale store, opened only after Zoom +.
inline constexpr int kMaxZoomWidth = 2048, kMaxZoomHeight = 3072;
inline constexpr int kZoomRowBytes = kMaxZoomWidth;
inline constexpr int kZoomBandBytes = kZoomRowBytes * kImageBandRows;

struct ImageRef {
  uint32_t offset = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  uint16_t stride = 0;        // bytes per row, rows are byte-padded
  uint8_t bits = 1;           // v1-v3 monochrome; v4 stores source grayscale for Zoom
  bool highContrast = false;  // v4 text hint, encoded in stride bit 15

  // A width of zero is how the file says "this card has no photograph"; the
  // offset is meaningless then and must not be followed.
  bool valid() const { return width > 0 && height > 0 && stride > 0; }
};

class StudyImages {
 public:
  // Parse the header. Returns false for a missing or malformed file, which is
  // not an error worth failing a deck over -- a deck simply has no photographs.
  bool open(ByteSource& images, bool zoom = false);

  bool ready() const { return count_ > 0; }
  int count() const { return count_; }

  // The entry for one card. `index` is the note's index, which is also its
  // index in cards.dat: make_images.py reads the order out of the deck itself
  // rather than re-deriving it, so the two cannot drift apart.
  ImageRef at(ByteSource& images, int index, bool answer = true) const;

  // Read up to kImageBandRows rows starting at `firstRow` into `out`, which
  // must hold kImageBandBytes (v1/v2), or kZoomBandBytes (v3).
  int readBand(ByteSource& images, const ImageRef& image, int firstRow, uint8_t* out) const;

 private:
  int count_ = 0;
  uint32_t size_ = 0;
  uint16_t version_ = 1;
};

}  // namespace study
