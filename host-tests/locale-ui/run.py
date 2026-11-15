"""Measure PET UI against real firmware fonts and every current language catalogue."""
from pathlib import Path
import argparse, importlib.util, json, os, subprocess, sys
R = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, default=R / '.cache/locale-ui/pet')
parser.add_argument('--emsdk', type=Path, default=R / '.venv/emsdk')
args = parser.parse_args()
Q = args.output.resolve()
Q.mkdir(parents=True, exist_ok=True)
spec = importlib.util.spec_from_file_location('quest_runner', R / 'host-tests/quest-locales/run.py')
quest = importlib.util.module_from_spec(spec)
spec.loader.exec_module(quest)
sources = ['host-tests/locale-ui/pet.cpp', 'src/apps_local/pet/PetGameScreens.cpp', 'src/apps_local/pet/PetNoticeScreen.cpp', 'src/apps_local/pet/PetState.cpp', 'src/apps_local/pet/PetScreens.cpp', 'src/apps_local/pet/PetAlbumScreens.cpp', 'src/apps_local/pet/PetReadingScreen.cpp', 'src/apps_local/reading/ReadingStatsCore.cpp', 'src/apps_local/pet/PetAffectionScreen.cpp', 'src/apps_local/pet/PetNameScreen.cpp', 'src/apps_local/pet/PetSetupScreens.cpp', 'src/apps_local/pet/PetDetailScreens.cpp', 'src/apps_local/pet/PetCareScreens.cpp', 'src/apps_local/pet/PetLifeScreens.cpp', 'src/apps_local/pet/PetFarewellScreen.cpp', 'src/apps_local/pet/PetCollectionScreen.cpp', 'src/apps_local/ui/PixelMenu.cpp', 'lib/EpdFont/EpdFont.cpp', 'lib/EpdFont/EpdFontFamily.cpp', 'lib/Utf8/Utf8.cpp', 'lib/MiniBidi/BidiUtils.cpp', 'freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp']
sdk = args.emsdk.resolve()
suffix = '.exe' if os.name == 'nt' else ''
compiler = sdk / 'upstream/emscripten' / ('em++' + suffix)
cc = compiler.with_name('emcc' + suffix)
nodes = sorted((sdk / 'node').glob('*'))
node = nodes[-1] / ('node.exe' if os.name == 'nt' else 'bin/node')
for script in ['host-tests/quest-locales/prepare.py', 'host-tests/quest-locales/check_fonts.py']:
    subprocess.run([sys.executable, script], cwd=R, check=True)
counts = Q / 'counts.cjs'
subprocess.run([str(compiler), '-std=c++20', '-O1', '-Ilib/I18n', 'host-tests/locale-ui/counts.cpp',
                '-sENVIRONMENT=node', '-sNODERAWFS=1', '-sALLOW_MEMORY_GROWTH=1', '-sEXIT_RUNTIME=1',
                '-o', str(counts)], cwd=R, check=True)
subprocess.run([str(node), str(counts), str(R / '.cache/locale-ui/fixtures')], cwd=R, check=True)
subprocess.run([str(cc), '-O1', '-c', 'lib/MiniBidi/minibidi.c', '-o', str(Q / 'bidi.o')], cwd=R, check=True)
program = Q / 'pet.cjs'
flags = ['-std=c++20', '-O1', '-Wno-comment', '-sENVIRONMENT=node', '-sNODERAWFS=1', '-sALLOW_MEMORY_GROWTH=1', '-sEXIT_RUNTIME=1', '-sSTACK_SIZE=1048576']
subprocess.run([str(compiler), *flags, *['-I' + p for p in quest.INCLUDES], *sources, str(Q / 'bidi.o'), '-o', str(program)], cwd=R, check=True)
result = subprocess.run([str(node), str(program), str(R), str(Q)], cwd=R, text=True, encoding='utf8', capture_output=True)
(Q / 'run.log').write_text(result.stdout + result.stderr, 'utf8')
print(result.stdout + result.stderr)
result.check_returncode()

# Probe centered status-message contracts separately from PET artwork/layout.
width_spec = importlib.util.spec_from_file_location('system_widths', R / 'host-tests/i18nwidth/measure_screens.py')
widths = importlib.util.module_from_spec(width_spec)
width_spec.loader.exec_module(widths)
cases = {(4 if int(font) == 10 else 0, bold, key, 1) for font, bold, key, _ in widths.sites()}
# Study's deck warning uses the small font and one line across the body.
cases.add((3, False, 'STR_UI_SOME_REVIEWS_DID_NOT_SAVE_BB1305', 1))
wrapped = {
    'STR_CLOCK_SYNC_NO_WIFI_HINT': (4, False, 3),
    'STR_CLOCK_SYNC_FAIL_HINT': (4, False, 3),
    'STR_RECOVERY_MODE_HINT': (4, False, 3),
    'STR_SYNC_READY': (4, False, 3),
    'STR_CHECK_SERIAL_OUTPUT': (4, False, 3),
    'STR_CALIBRE_STARTING': (0, False, 2),
    'STR_STARTING_HOTSPOT': (4, False, 2),
    'STR_FONT_INSTALL_FAILED': (4, True, 2),
    'STR_NO_CREDENTIALS_MSG': (4, True, 2),
    'STR_KOREADER_SETUP_HINT': (4, True, 3),
    'STR_OPEN_URL_HINT': (4, True, 2),
    'STR_SCAN_QR_HINT': (4, True, 2),
}
for n in range(1, 5):
    wrapped['STR_CLEAR_CACHE_WARNING_' + str(n)] = (4, n == 2, 2)
for key, (font, bold, lines) in wrapped.items():
    cases.add((font, bold, key, lines))
fixture = R / '.cache/locale-ui/fixtures/centered.inc'
fixture.write_text('#include <I18n.h>\nstruct Centered { int font; bool bold; StrId key; int lines; };\nstatic constexpr Centered centered[] = {\n' + ''.join('  {%d,%s,StrId::%s,%d},\n' % (font, str(bold).lower(), key, lines) for font, bold, key, lines in sorted(cases)) + '};\n', 'utf8')
centered = Q / 'centered.cjs'
common_sources = ['host-tests/locale-ui/centered.cpp', 'lib/EpdFont/EpdFont.cpp', 'lib/EpdFont/EpdFontFamily.cpp', 'lib/Utf8/Utf8.cpp', 'lib/MiniBidi/BidiUtils.cpp', 'freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp']
subprocess.run([str(compiler), *flags, *['-I' + p for p in quest.INCLUDES], *common_sources, str(Q / 'bidi.o'), '-o', str(centered)], cwd=R, check=True)
subprocess.run([str(node), str(centered), str(R), str(Q)], cwd=R, check=True)

# Exercise the actual production SDK adapter, not only the measuring draw target.
fitter = Q / 'gfx-fitter.cjs'
fitter_sources = ['host-tests/locale-ui/test_gfx_fitter.cpp', 'lib/EpdFont/EpdFont.cpp', 'lib/EpdFont/EpdFontFamily.cpp', 'lib/Utf8/Utf8.cpp', 'lib/I18n/I18nStrings.cpp', 'freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp']
fitter_includes = ['host-tests/locale-ui/stubs', 'freeink-sdk/libs/ui/FreeInkUI/include', 'lib/EpdFont', 'lib/Utf8', 'lib/I18n']
subprocess.run([str(compiler), *flags, *['-I' + p for p in fitter_includes], *fitter_sources, '-o', str(fitter)], cwd=R, check=True)
subprocess.run([str(node), str(fitter)], cwd=R, check=True)
