"""Encode the Focus illustration as a flash-resident, one-bit FreeInkUI bitmap."""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]


def main():
    image = Image.open(ROOT / 'assets_local/focus/bookshelf-source.png').convert('L')
    image = image.resize((432, 144), Image.Resampling.LANCZOS)
    bitmap = image.point(lambda value: 255 if value > 155 else 0).convert('1')
    bitmap.save(ROOT / 'assets_local/focus/bookshelf.png')
    packed = bytes(value ^ 255 for value in bitmap.tobytes())
    lines = ['#pragma once', '', '#include <FreeInkUI.h>', '', 'namespace focusart {',
             '// Packed 1-bit version of assets_local/focus/bookshelf.png.',
             'inline constexpr uint8_t bookshelfBits[] = {']
    lines.extend('    ' + ', '.join(f'0x{value:02x}' for value in packed[offset:offset + 19]) + ','
                 for offset in range(0, len(packed), 19))
    lines.extend(['};', 'inline constexpr freeink::ui::BitmapRef bookshelf{bookshelfBits, 432, 144, freeink::ui::BitmapFormat::BW1};',
                  '}  // namespace focusart', ''])
    (ROOT / 'src/apps_local/focus/FocusBookshelf.h').write_text('\n'.join(lines), encoding='utf-8')


if __name__ == '__main__':
    main()
