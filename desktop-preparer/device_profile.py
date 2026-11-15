"""Read the cache geometry from the firmware board profile, not simulator defaults."""
from pathlib import Path
import re

BOARD_HEADER = Path('freeink-sdk/libs/hardware/BoardConfig/include/BoardConfig.h')


def x4pro_profile(repo):
    source = (repo / BOARD_HEADER).read_text(encoding='utf-8')
    source = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
    start = re.search(r'constexpr BoardProfile XTEINK_X4_PRO\s*=\s*\{', source)
    if not start:
        raise ValueError('X4 Pro hardware profile not found')
    fields, depth, begin = [], 0, start.end()
    for at in range(begin, len(source)):
        char = source[at]
        if char == '{':
            depth += 1
        elif char == '}':
            if depth == 0:
                fields.append(source[begin:at].strip())
                break
            depth -= 1
        elif char == ',' and depth == 0:
            fields.append(source[begin:at].strip())
            begin = at + 1
    # BoardProfile's tail is viewableInsets, batteryChargeStatusActiveHigh.
    # Fail the build if the hardware contract changes instead of guessing sizes.
    insets = re.fullmatch(r'\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}', fields[-2])
    if fields[0] != 'Board::XteinkX4Pro' or fields[3] != 'DisplayController::SSD1677' or fields[-1] not in ('true', 'false') or not insets:
        raise ValueError('X4 Pro hardware profile layout changed; update the desktop adapter')
    return {'board': 'xteink_x4_pro', 'panelWidth': int(fields[4]), 'panelHeight': int(fields[5]),
            'insets': [int(value) for value in insets.groups()]}


def write_header(repo, destination):
    profile = x4pro_profile(repo)
    text = '''#pragma once
#include <BoardConfig.h>
namespace desktopdevice {
inline void apply() {
  BoardConfig::ACTIVE = BoardConfig::XTEINK_X4_PRO;
  BoardConfig::ACTIVE.displayWidth = %d;
  BoardConfig::ACTIVE.displayHeight = %d;
  BoardConfig::ACTIVE.viewableInsets = {%s};
}
}
''' % (profile['panelWidth'], profile['panelHeight'], ', '.join(map(str, profile['insets'])))
    if not destination.exists() or destination.read_text(encoding='utf-8') != text:
        destination.write_text(text, encoding='utf-8')
    return profile
