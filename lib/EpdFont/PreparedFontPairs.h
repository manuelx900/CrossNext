#pragma once

#include <cstdint>

#include "EpdFontData.h"

// CPLG is an optional sfnt table produced by build_multilingual_reader_fonts.py.
// It contains big-endian CNLG/version/count and sorted (BMP pair, PUA output)
// records for pre-shaped Thai clusters. Ordinary TTFs do not use this table.
namespace preparedFont {
using Read = unsigned long (*)(void*, unsigned long, unsigned char*, unsigned long);
enum class Status : uint8_t { Absent, Ready, Invalid };
struct Table {
  uint32_t offset = 0;
  uint16_t count = 0;
};
inline uint16_t be16(const uint8_t* p) { return (uint16_t(p[0]) << 8) | p[1]; }
inline uint32_t be32(const uint8_t* p) { return (uint32_t(be16(p)) << 16) | be16(p + 2); }
inline Status findTable(Read read, void* ctx, unsigned long bytes, Table& table) {
  uint8_t entry[16];
  if (bytes < 12 || read(ctx, 0, entry, 12) != 12) return Status::Absent;
  const uint16_t tables = be16(entry + 4);
  if (tables > 256 || 12u + uint32_t(tables) * 16u > bytes) return Status::Invalid;
  for (uint16_t i = 0; i < tables; ++i) {
    if (read(ctx, 12u + uint32_t(i) * 16u, entry, 16) != 16) return Status::Invalid;
    if (be32(entry) != 0x43504c47u) continue;  // CPLG
    const uint32_t offset = be32(entry + 8), length = be32(entry + 12);
    if (offset > bytes || length > bytes - offset || length < 8 || read(ctx, offset, entry, 8) != 8)
      return Status::Invalid;
    const uint16_t count = be16(entry + 6);
    if (be32(entry) != 0x434e4c47u || be16(entry + 4) != 1 || count == 0 || count > 4096 ||
        length != 8u + uint32_t(count) * 8u)
      return Status::Invalid;
    table = {offset + 8, count};
    return Status::Ready;
  }
  return Status::Absent;
}
inline bool readPairs(Read read, void* ctx, const Table& table, EpdLigaturePair* pairs) {
  uint8_t block[128];
  uint32_t previous = 0;
  for (uint16_t i = 0; i < table.count; ++i) {
    if (i % 16 == 0) {
      const unsigned long bytes = (table.count - i < 16 ? table.count - i : 16) * 8u;
      if (read(ctx, table.offset + uint32_t(i) * 8u, block, bytes) != bytes) return false;
    }
    const uint8_t* raw = block + (i % 16) * 8;
    const uint32_t pair = be32(raw), output = be32(raw + 4);
    const uint32_t left = pair >> 16, right = pair & 0xffffu;
    const bool base = (left >= 0x0e01 && left <= 0x0e2e) || left == 0x25cc;
    const bool chained = left >= 0xe000 && left < output;
    const bool mark = right == 0x0e31 || right == 0x0e33 || (right >= 0x0e34 && right <= 0x0e3a) ||
                      (right >= 0x0e47 && right <= 0x0e4e);
    if (pair <= previous || (!base && !chained) || !mark || output < 0xe000 || output > 0xf8ff) return false;
    pairs[i] = {pair, output};
    previous = pair;
  }
  return true;
}
}  // namespace preparedFont
