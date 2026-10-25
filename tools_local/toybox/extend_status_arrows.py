"""Append a pixel right arrow without changing existing font glyphs or metrics.

Run after regenerating Toybox/prose fonts. Repeated runs are byte-identical.
The U+2192 glyph is drawn on the font's integer pixel grid, with a visible shaft.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
FONTS = ("toybox_10", "toybox_14", "toybox_20", "toybox_30", "ui_prose_10")


def extend(path):
    source = path.read_text(encoding="utf-8")
    name = path.stem
    intervals = re.search(r"static const EpdUnicodeInterval " + name + r"Intervals\[\] = \{(.*?)\};", source, re.S)
    entries = [tuple(int(n, 0) for n in row.split(","))
               for row in re.findall(r"\{([^{}]+)\}", intervals[1])]
    if any(a <= 0x2192 <= b for a, b, _ in entries):
        return
    glyphs = re.search(r"static const EpdGlyph " + name + r"Glyphs\[\] = \{(.*?)\};", source, re.S)
    metrics = [tuple(map(int, row.split(","))) for row in re.findall(r"\{([-\d, \t]+)\}", glyphs[1])]
    h_index = next(offset + ord("H") - a for a, b, offset in entries if a <= ord("H") <= b)
    cap = metrics[h_index]
    unit = max(1, cap[1] // 9)
    width, height = 9 * unit, 7 * unit
    rows = [[False] * width for _ in range(height)]
    for x in range(8 * unit):
        for dy in range(unit):
            rows[3 * unit + dy][x] = True
    for step in range(4):
        for dy in range(unit):
            for dx in range(unit):
                rows[step * unit + dy][(5 + step) * unit + dx] = True
                rows[(6 - step) * unit + dy][(5 + step) * unit + dx] = True
    packed = bytearray((width * height + 7) // 8)
    for i, value in enumerate(v for row in rows for v in row):
        if value:
            packed[i // 8] |= 128 >> (i % 8)
    bitmap = re.search(r"static const uint8_t " + name + r"Bitmaps\[(\d+)\] = \{(.*?)\};", source, re.S)
    offset = int(bitmap[1])
    top = cap[4] - (cap[1] - height) // 2
    extra = f"    {{{width}, {height}, {(width + 2 * unit) * 16}, 0, {top}, {len(packed)}, {offset}}},  // U+2192\n"
    source = source[:glyphs.end(1)] + extra + source[glyphs.end(1):]
    intervals = re.search(r"static const EpdUnicodeInterval " + name + r"Intervals\[\] = \{(.*?)\};", source, re.S)
    entries.append((0x2192, 0x2192, len(metrics)))
    entries.sort()
    block = "\n" + "".join(f"    {{0x{a:X}, 0x{b:X}, 0x{i:X}}},\n" for a, b, i in entries)
    source = source[:intervals.start(1)] + block + source[intervals.end(1):]
    source = re.sub(r"(" + name + r"Intervals,\s*)\d+,", lambda m: m[1] + str(len(entries)) + ",", source)
    bitmap = re.search(r"static const uint8_t " + name + r"Bitmaps\[(\d+)\] = \{(.*?)\};", source, re.S)
    data = bitmap[2] + "    " + ", ".join(f"0x{v:02X}" for v in packed) + ",\n"
    block = f"static const uint8_t {name}Bitmaps[{offset + len(packed)}] = {{{data}}};"
    source = source[:bitmap.start()] + block + source[bitmap.end():]
    path.write_text(source, encoding="utf-8", newline="\n")
    print(name, "+", len(packed), "bitmap bytes")


if __name__ == "__main__":
    for name in FONTS:
        extend(ROOT / "src/apps_local/ui/fonts" / (name + ".h"))
