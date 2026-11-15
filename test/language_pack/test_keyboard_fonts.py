"""Check actual SD glyphs, including alternate keys and Wordle alphabets."""
import json
from pathlib import Path
import struct
import unittest
import unicodedata

ROOT = Path(__file__).resolve().parents[2]


class KeyboardFontCoverageTest(unittest.TestCase):
    def test_keyboard_and_wordle_glyphs_exist_in_every_face(self):
        required = {0x25CC, 0x309B, 0x309C}  # visible labels for combining keys
        source = ROOT / 'assets_local/language-keyboards/SOURCE.json'
        for layout in json.loads(source.read_text(encoding='utf-8')).values():
            for text in layout['rows'] + layout['shift'] + list(layout.get('alts', {}).values()):
                required.update(map(ord, unicodedata.normalize('NFC', text)))
        glyphs = (ROOT / 'assets_local/wordle-languages/GLYPHS.txt').read_text(encoding='utf-8')
        required.update(map(ord, glyphs))
        required.update(map(ord, glyphs.upper()))
        required.add(0x1E9E)  # capital sharp s from external keyboards
        for path in (ROOT / 'assets_local/language-fonts').glob('*.cpfont'):
            data = path.read_bytes()
            for style in range(data[12]):
                toc = 32 + 32 * style
                count, glyph_count = struct.unpack_from('<II', data, toc + 4)
                start = struct.unpack_from('<I', data, toc + 24)[0]
                glyph_start = start + count * 12
                covered = set()
                for i in range(count):
                    first, last, offset = struct.unpack_from('<III', data, start + i * 12)
                    for cp in required.intersection(range(first, last + 1)):
                        index = offset + cp - first
                        self.assertLess(index, glyph_count)
                        width, height, advance = struct.unpack_from('<BBH', data, glyph_start + 16 * index)
                        if width and height or cp == 32 and advance:
                            covered.add(cp)
                missing = required - covered
                self.assertFalse(missing, (path.name, style, [f'U+{cp:04X}' for cp in sorted(missing)]))


if __name__ == '__main__':
    unittest.main()
