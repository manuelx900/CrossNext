"""Pack the user-supplied Focus menu artwork as a flash-resident BW1 bitmap."""
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SIDE = 230


def main():
    source = Image.open(ROOT / 'assets_local/focus/pomodoro-source.png').convert('L')
    bounds = source.point(lambda value: 255 if value < 224 else 0).getbbox()
    if bounds is None:
        raise ValueError('Pomodoro source has no visible artwork')
    left, top, right, bottom = bounds
    side = max(right - left, bottom - top) + 32
    x, y = (left + right - side) // 2, (top + bottom - side) // 2
    image = source.crop((x, y, x + side, y + side))
    image = image.point(lambda value: 255 if value >= 235 else 0 if value <= 32 else value)
    image = image.resize((SIDE, SIDE), Image.Resampling.NEAREST)
    bitmap = image.convert('1', dither=Image.Dither.FLOYDSTEINBERG)
    bitmap.save(ROOT / 'assets_local/focus/pomodoro.png')
    packed = bytes(value ^ 255 for value in bitmap.tobytes())
    lines = ['#pragma once', '', '#include <FreeInkUI.h>', '', 'namespace focusart {',
             '// Packed 1-bit version of assets_local/focus/pomodoro.png.',
             'inline constexpr uint8_t pomodoroBits[] = {']
    lines.extend('    ' + ', '.join(f'0x{value:02x}' for value in packed[offset:offset + 19]) + ','
                 for offset in range(0, len(packed), 19))
    lines.extend(['};',
                  f'inline constexpr freeink::ui::BitmapRef pomodoro{{pomodoroBits, {SIDE}, {SIDE}, freeink::ui::BitmapFormat::BW1}};',
                  '}  // namespace focusart', ''])
    (ROOT / 'src/apps_local/focus/FocusPomodoroArt.h').write_text('\n'.join(lines), encoding='utf-8')
    print(f'Packed {SIDE} x {SIDE}: {len(packed)} bytes')


if __name__ == '__main__':
    main()
