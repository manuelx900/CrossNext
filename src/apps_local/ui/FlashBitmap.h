#pragma once
#include <FreeInkUI.h>

#include <algorithm>
#include <cstdint>

namespace flashbitmap {
struct Node {
  uint16_t left, right;
};
struct Sprite {
  const uint8_t* data;
  const Node* tree;
  uint16_t bytes, width, height, root;
  uint8_t predictor;
};
class RowDecoder {
 public:
  explicit RowDecoder(const Sprite& sprite)
      : cursor(sprite.data),
        end(sprite.data + sprite.bytes),
        tree(sprite.tree),
        root(sprite.root),
        distance(sprite.predictor == 3 ? 1 : sprite.predictor * ((sprite.width + 7) / 8)) {}
  uint8_t next() {
    if (!remaining) {
      const auto control = symbol();
      literal = control < 128;
      remaining = literal ? control + 1 : control - 127;
      if (!literal) repeated = symbol();
    }
    --remaining;
    const uint8_t value = (literal ? symbol() : repeated) ^ (distance ? history[position] : 0);
    if (distance) {
      history[position] = value;
      if (++position == distance) position = 0;
    }
    return value;
  }

 private:
  uint8_t symbol() {
    uint16_t node = root;
    for (int depth = 0; depth < 24; ++depth) {
      if (!bits) {
        if (cursor == end) return 0;
        input = *cursor++;
        bits = 8;
      }
      const bool right = (input & 0x80) != 0;
      input <<= 1;
      --bits;
      node = right ? tree[node].right : tree[node].left;
      if (node >= 256) return uint8_t(node - 256);
    }
    return 0;
  }
  const uint8_t* cursor;
  const uint8_t* end;
  const Node* tree;
  uint16_t root, remaining = 0;
  uint8_t history[70] = {};
  uint8_t distance = 0, position = 0;
  uint8_t input = 0, bits = 0, repeated = 0;
  bool literal = false;
};
inline void draw(freeink::ui::DrawTarget& target, freeink::ui::Rect area, const Sprite& sprite,
                 freeink::ui::BitmapMode mode = freeink::ui::BitmapMode::Contain,
                 freeink::ui::Color color = freeink::ui::Color::Black, bool clear = false) {
  namespace fui = freeink::ui;
  if (!sprite.width || sprite.width > 280 || !sprite.height || sprite.predictor > 3 || area.width <= 0 ||
      area.height <= 0)
    return;
  if (clear) target.fill(area, fui::Paint::solid(fui::Color::White));
  auto rect = area;
  if (mode == fui::BitmapMode::Contain) {
    const int width = std::min(int(area.width), int(area.height) * sprite.width / sprite.height);
    const int height = width * sprite.height / sprite.width;
    rect = fui::makeRect(area.x + (area.width - width) / 2, area.y + (area.height - height) / 2, width, height);
  }
  RowDecoder decoder(sprite);
  uint8_t row[35];
  const int bytes = (sprite.width + 7) / 8;
  for (int y = 0; y < sprite.height; ++y) {
    for (int b = 0; b < bytes; ++b) row[b] = decoder.next();
    const int top = rect.y + y * rect.height / sprite.height;
    const int bottom = rect.y + (y + 1) * rect.height / sprite.height;
    if (bottom > top)
      target.bitmap(fui::makeRect(rect.x, top, rect.width, bottom - top),
                    {row, sprite.width, 1, fui::BitmapFormat::BW1}, fui::BitmapMode::Stretch, fui::Paint::solid(color));
  }
}
}  // namespace flashbitmap
