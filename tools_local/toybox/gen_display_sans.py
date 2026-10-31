"""Generate one-bit Noto Sans display, calculator and tabular numeral assets."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from fontTools import subset

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "lib/EpdFont/builtinFonts/source/NotoSans/NotoSans-Regular.ttf"
FONTCONVERT = ROOT / "lib/EpdFont/scripts/fontconvert.py"
LATIN = "U+0020-007E,U+00A1,U+00BF,U+00C1,U+00C9,U+00CD,U+00D1,U+00D3,U+00DA,U+00DC,U+00E1,U+00E9,U+00ED,U+00F1,U+00F3,U+00FA,U+00FC,U+2026"
MATH = LATIN + ",U+00B0-00B1,U+00D7,U+00F7,U+03C0,U+2212,U+221A"

def convert(directory, name, points, chars, output=None):
    font = directory / (name + ".ttf")
    subset.main([str(SOURCE), "--unicodes=" + chars + ",U+007C", "--output-file=" + str(font)])
    result = subprocess.run([sys.executable, str(FONTCONVERT), name, str(points), str(font),
                             "--force-autohint"], capture_output=True, text=True, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(result.stderr[-4000:])
    source = re.sub(r" \* Command used:.*", " * Source: tools_local/toybox/gen_display_sans.py (Noto Sans, SIL OFL)", result.stdout)
    source = "\n".join(line for line in source.splitlines() if not line.endswith("extracted")) + "\n"
    if output:
        (ROOT / output).write_text(source, encoding="utf-8")
    return source

with tempfile.TemporaryDirectory() as temporary:
    directory = Path(temporary)
    for points in (22, 40, 56):
        name = f"display_sans_{points}"
        convert(directory, name, points, LATIN, f"src/apps_local/ui/fonts/{name}.h")
    for slot, points in ((20, 12), (26, 18), (28, 16), (34, 26), (44, 34), (56, 42)):
        name = f"calc_sans_{slot}"
        chars = MATH if slot in (20, 28) else "U+0020,U+0025,U+002B-002F,U+0030-003A,U+0045,U+0065,U+2212"
        convert(directory, name, points, chars, f"src/apps_local/calculator/fonts/{name}.h")
    chars = " %+,-./0123456789:HMNIS"
    name = "noto_numerals"
    source = convert(directory, name, 56, ",".join(f"U+{ord(ch):04X}" for ch in chars))
    data = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]+)", re.search(name + r"Bitmaps\[\d+\] = \{(.*?)\};", source, re.S)[1]))
    glyphs = [list(map(int, row.split(","))) for row in re.findall(r"^\s*\{([-\d, \t]+)\},", re.search(name + r"Glyphs\[\] = \{(.*?)\};", source, re.S)[1], re.M)]
    intervals = [[int(value.strip(), 0) for value in row.split(",")] for row in re.findall(r"\{([0-9a-fA-Fx, \t]+)\}", re.search(name + r"Intervals\[\] = \{(.*?)\};", source, re.S)[1])]
    packed = bytearray()
    rows = []
    for ch in chars:
        code = ord(ch)
        index = next(offset + code - first for first, last, offset in intervals if first <= code <= last)
        width, height, advance, left, top, count, offset = glyphs[index]
        rows.append((code, width, height, (advance + 8) // 16, left, top, len(packed)))
        packed.extend(data[offset:offset + count])
    digits = [row for row in rows if 48 <= row[0] <= 57]
    text = """#pragma once
// Generated from the bundled OFL Noto Sans by gen_display_sans.py.
// Inline constants share one flash copy; no font cache or heap.
#include <cstdint>
namespace numerals {
struct Glyph { uint8_t code, width, height, advance; int16_t left, top; uint16_t offset; };
"""
    text += f"inline constexpr int kDigitAdvance = {max(row[3] for row in digits)}, kDigitHeight = {max(row[5] for row in digits)};\n"
    text += f"inline constexpr int kColonAdvance = {next(row[3] for row in rows if row[0] == 58)};\n"
    text += "inline constexpr Glyph glyphs[] = {\n" + "".join("    {" + ", ".join(map(str, row)) + "},\n" for row in rows) + "};\n"
    text += "inline constexpr uint8_t bits[] = {\n" + "".join("    " + ", ".join(f"0x{value:02x}" for value in packed[n:n + 18]) + ",\n" for n in range(0, len(packed), 18)) + "};\n}  // namespace numerals\n"
    (ROOT / "src/apps_local/ui/fonts/Numerals.h").write_text(text, encoding="utf-8")
    print("Noto Sans numeral bitmap bytes:", len(packed))
