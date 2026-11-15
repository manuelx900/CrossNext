"""Measure Quest UI using the firmware glyphs and every current locale."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
SOURCES = [
    'host-tests/quest-locales/audit.cpp',
    'src/apps_local/quest/QuestScreens.cpp',
    'src/apps_local/quest/QuestJourneyScreens.cpp',
    'src/apps_local/quest/QuestCore.cpp',
    'src/apps_local/quest/QuestCampaign.cpp',
    'src/apps_local/reading/ReadingStatsCore.cpp',
    'src/apps_local/ui/PixelMenu.cpp',
    'src/apps_local/ui/SdBitmapPack.cpp',
    'lib/EpdFont/EpdFont.cpp', 'lib/EpdFont/EpdFontFamily.cpp',
    'lib/Utf8/Utf8.cpp', 'lib/MiniBidi/BidiUtils.cpp',
    'freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp',
]
INCLUDES = [
    'freeink-sdk/libs/ui/FreeInkUI/include',
    'freeink-sdk/libs/assets/Icons/include', 'lib/EpdFont', 'lib/I18n',
    'lib/Utf8', 'lib/Memory', 'lib/MiniBidi',
    'host-tests/sdart/stubs', 'src/apps_local/quest',
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emsdk', type=Path, help='Use Emscripten and its bundled Node runtime')
    parser.add_argument('--output', type=Path, default=ROOT / '.cache/locale-ui/result')
    parser.add_argument('--language', type=int, help='One Language enum index; default is all 39')
    parser.add_argument('--board', choices=['x4pro', 'sticky', 'both'], default='both',
                        help='Use the configured portrait/landscape bezel insets')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    build = ROOT / '.cache/locale-ui'
    build.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, str(Path(__file__).with_name('prepare.py'))], cwd=ROOT, check=True)
    subprocess.run([sys.executable, str(Path(__file__).with_name('check_fonts.py'))], cwd=ROOT, check=True)
    flags = ['-std=c++20', '-O1', '-Wno-comment']
    runtime = []
    if args.emsdk:
        compiler = args.emsdk / 'upstream/emscripten' / ('em++.exe' if os.name == 'nt' else 'em++')
        c_compiler = args.emsdk / 'upstream/emscripten' / ('emcc.exe' if os.name == 'nt' else 'emcc')
        nodes = sorted((args.emsdk / 'node').glob('*'))
        runtime = [str(nodes[-1] / ('node.exe' if os.name == 'nt' else 'bin/node'))]
        flags += ['-sENVIRONMENT=node', '-sNODERAWFS=1', '-sALLOW_MEMORY_GROWTH=1',
                  '-sEXIT_RUNTIME=1', '-sSTACK_SIZE=1048576']
        program = build / 'audit.cjs'
    else:
        compiler = os.environ.get('CXX', 'c++')
        c_compiler = os.environ.get('CC', 'cc')
        program = build / 'audit'
    bidi = build / 'minibidi.o'
    subprocess.run([str(c_compiler), '-O1', '-c', 'lib/MiniBidi/minibidi.c', '-o', str(bidi)], cwd=ROOT, check=True)
    subprocess.run([str(compiler), *flags, *['-I' + x for x in INCLUDES], *SOURCES, str(bidi),
                    '-o', str(program)], cwd=ROOT, check=True)
    bounds = build / ('test-bounds.cjs' if args.emsdk else 'test-bounds')
    subprocess.run([str(compiler), *flags, '-Ilib/EpdFont', '-Ilib/Utf8',
                    'host-tests/quest-locales/test_bounds.cpp', 'lib/EpdFont/EpdFont.cpp',
                    'lib/EpdFont/EpdFontFamily.cpp', 'lib/Utf8/Utf8.cpp', '-o', str(bounds)], cwd=ROOT, check=True)
    subprocess.run([*runtime, str(bounds)], cwd=ROOT, check=True)
    casing = build / ('test-case.cjs' if args.emsdk else 'test-case')
    subprocess.run([str(compiler), *flags, '-Ilib/Utf8', 'host-tests/quest-locales/test_case.cpp',
                    '-o', str(casing)], cwd=ROOT, check=True)
    subprocess.run([*runtime, str(casing)], cwd=ROOT, check=True)
    boards = ['x4pro', 'sticky'] if args.board == 'both' else [args.board]
    issues, logs, frame_count = [], [], 0
    for board in boards:
        destination = args.output / board
        destination.mkdir(exist_ok=True)
        command = [*runtime, str(program), str(ROOT), str(destination),
                   str(args.language if args.language is not None else -1),
                   str(0 if board == 'x4pro' else 1)]
        result = subprocess.run(command, cwd=ROOT, check=True, capture_output=True, text=True, encoding='utf8')
        print(board + ':\n' + result.stdout, end='')
        logs.append(board + ':\n' + result.stdout)
        (destination / 'run.log').write_text(result.stdout, encoding='utf8')
        found = [json.loads(line) for line in (destination / 'issues.jsonl').read_text('utf8').splitlines() if line]
        issues.extend(dict(issue, board=board) for issue in found)
        total = re.search(r'TOTAL (\d+) frames', result.stdout)
        frame_count += int(total[1]) if total else 0
    (args.output / 'run.log').write_text(''.join(logs), encoding='utf8')
    (args.output / 'issues.jsonl').write_text(''.join(json.dumps(issue, ensure_ascii=False) + '\n' for issue in issues), encoding='utf8')
    summary = {'frames': frame_count, 'issues': len(issues),
               'languages': 39 if args.language is None else 1, 'boards': boards,
               'orientations': ['480x800', '800x480'], 'hostPreview': True}
    (args.output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf8')
    return bool(issues)


if __name__ == '__main__':
    sys.exit(main())
