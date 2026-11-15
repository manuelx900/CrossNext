"""Audit shipped non-Latin key labels against the real SD font metrics.

This checks horizontal glyph coverage and fit, not physical display rendering.
Latin labels are covered by run-keyboard.py's built-in firmware font previews.
"""
from pathlib import Path
import json
import re
import struct
import unicodedata

ROOT = Path(__file__).resolve().parents[2]


class Font:
    def __init__(self, points):
        self.data = (ROOT / f"assets_local/language-fonts/Interface_{points}.cpfont").read_bytes()
        assert self.data[:8] == b"CPFONT\0\0"
        count = struct.unpack_from("<I", self.data, 36)[0]
        offset = struct.unpack_from("<I", self.data, 56)[0]
        self.intervals = [struct.unpack_from("<III", self.data, offset + n * 12) for n in range(count)]
        self.glyphs = offset + count * 12

    def width(self, char):
        for first, last, index in self.intervals:
            if first <= ord(char) <= last:
                width, _, advance = struct.unpack_from("<BBH", self.data, self.glyphs + (index + ord(char) - first) * 16)
                return max(width, (advance + 8) // 16)
        raise AssertionError(f"Missing glyph: U+{ord(char):04X}")


class BuiltinFont:
    def __init__(self, name):
        text = (ROOT / f"lib/EpdFont/builtinFonts/{name}.h").read_text(encoding="utf-8")
        glyphs = text.split(f"{name}Glyphs[] = {{", 1)[1].split("};", 1)[0]
        intervals = text.split(f"{name}Intervals[] = {{", 1)[1].split("};", 1)[0]
        self.glyphs = [tuple(int(value.strip(), 0) for value in row.split(","))
                       for row in re.findall(r"\{([^{}]+)\}", glyphs)]
        self.intervals = [tuple(int(value.strip(), 0) for value in row.split(","))
                          for row in re.findall(r"\{([^{}]+)\}", intervals)]

    def width(self, char):
        for first, last, index in self.intervals:
            if first <= ord(char) <= last:
                width, _, advance, *_ = self.glyphs[index + ord(char) - first]
                return max(width, (advance + 8) // 16)
        return None


isolated = {}
for code in range(0xFB50, 0xFF00):
    parts = unicodedata.decomposition(chr(code)).split()
    if len(parts) == 2 and parts[0] == "<isolated>":
        isolated[chr(int(parts[1], 16))] = chr(code)

small, body = Font(8), Font(12)
builtin_small, builtin_body = BuiltinFont("notosans_8_regular"), BuiltinFont("ubuntu_12_regular")
layouts = json.loads((ROOT / "assets_local/language-keyboards/SOURCE.json").read_text(encoding="utf-8"))
checked = compact = 0
for tag, layout in layouts.items():
    for layer in ("rows", "shift"):
        columns = max(map(len, layout[layer]))
        if len(layout[layer][-1]) <= 9: columns = max(columns, len(layout[layer][-1]) + 2)
        key_width = (480 - 8 - (columns - 1) * 6) // columns
        primary = body
        builtin = builtin_body
        for char in "".join(layout[layer]):
            if ord(char) < 256:
                continue
            # The renderer shapes each isolated Arabic-script letter before measurement.
            label = isolated.get(char, char)
            if char in "\u3099\u309a":
                label = chr(ord(char) + 2)
            if tag == "th" and (char == "\u0e31" or "\u0e34" <= char <= "\u0e3a" or "\u0e47" <= char <= "\u0e4e"):
                label = "\u25cc" + char
            # Combining marks overlay the base instead of advancing the pen.
            width = builtin.width(char)
            if width is None:
                width = max(primary.width(c) for c in label)
            padding = 8
            if width > key_width - padding:
                width = builtin_small.width(char)
                if width is None:
                    width = max(small.width(c) for c in label)
                compact += 1
            assert width <= key_width - padding, (tag, layer, char, width, key_width - padding)
            alternate = layout.get("alts", {}).get(char)
            if alternate:
                assert small.width(isolated.get(alternate, alternate)) <= key_width - 12, (tag, char, alternate)
            checked += 1

# Regression: Persian's wide isolated letters need compact labels in 12-key rows.
for char in "سشصض":
    label = isolated[char]
    assert body.width(label) > 25
    assert small.width(label) <= 25

print(f"Real font metrics: {checked} non-Latin labels and their alternates passed across 30 packs; {compact} use compact fallback")

# Every shipped accent catalog entry must have a real built-in or SD glyph.
catalog=(ROOT / "src/apps_local/ui/KeyboardAlternates.cpp").read_text(encoding="utf-8")
values=re.findall(r'"([^"\n]*\|[^"\n]*)"',catalog)
variants=set("".join(values).replace("|","")) | set("◌")
for zero in (0x660,0x6f0,0xe50):variants.update(chr(zero+n) for n in range(10))
for char in variants:
    if ord(char)<128: continue
    shaped=isolated.get(char,char)
    if builtin_body.width(shaped) is None:
        assert body.width(shaped)<=36, ("popup variant",char)
print(f"Alternatives font coverage: {len(variants)} catalog characters checked")
