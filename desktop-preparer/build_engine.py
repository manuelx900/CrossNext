"""Build the real firmware preparation engine as a 32-bit portable WASM worker."""
from pathlib import Path
import argparse, configparser, concurrent.futures, hashlib, importlib.util, json, os, re, shlex, subprocess, sys
from device_profile import BOARD_HEADER, write_header

ROOT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--firmware', type=Path, default=ROOT.parent,
                    help='Firmware checkout supplying the simulator sources and toolchain')
REPO = parser.parse_args().firmware.resolve()
BUILD = ROOT / 'build'
OUT = ROOT / 'dist' / 'runtime'
SDK = (REPO / '.venv/emsdk/upstream/emscripten').resolve()
os.environ['PATH'] = str(SDK) + os.pathsep + os.environ['PATH']
os.environ.setdefault('EM_CACHE', str(BUILD / 'emscripten-cache'))
spec = importlib.util.spec_from_file_location('wasmbuild', REPO / 'tools_local/wasm/build.py')
base = importlib.util.module_from_spec(spec); spec.loader.exec_module(base)
BUILD.mkdir(exist_ok=True); OUT.mkdir(parents=True, exist_ok=True)
base.OBJ = BUILD / 'objects'

# Match the firmware's optional FreeType modules even with an older simulator database.
FONT_FEATURES = {
    name: int(value) for name, value in re.findall(
        r'^\s*-D(FREEINK_FONT_ENABLE_[A-Z_]+)=([01])\s*$',
        (REPO / 'platformio.ini').read_text(encoding='utf-8'), re.M)
}

def source_hashes():
    directories = ['lib/Epub', 'lib/Xtc', 'lib/EpdFont', 'lib/GfxRenderer', 'lib/XmlParserUtils',
                   'src/apps_local/bookprep', 'src/components', 'src/apps_local/home',
                   'src/apps_local/library', 'src/apps_local/ui', 'src/apps_local/readerbars', 'lib/Performance',
                   'sim-stubs', 'tools_local/wasm/stubs']
    files = [p for directory in directories for p in (REPO / directory).rglob('*')
             if p.suffix in ('.cpp', '.h')]
    files += [REPO / name for name in ['compile_commands.json', '.pio/libdeps/simulator_x4_pro/simulator/src/HalStorage.h', '.pio/libdeps/simulator_x4_pro/simulator/src/HalStorage.cpp', 'platformio.ini', 'src/CrossPointSettings.cpp', 'src/CrossPointSettings.h',
              'src/ReaderFontSizes.cpp', 'src/ReaderFontSizes.h', 'src/SdCardFontSystem.cpp',
              'lib/hal/HalMemory.h', 'lib/hal/HalMemory.cpp',
              'src/util/ParagraphIndentMigration.h', BOARD_HEADER]]
    hashes = {p.relative_to(REPO).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
    for name in ['device_profile.py', 'src/engine.cpp', 'src/compatibility.h', 'build_engine.py']:
        hashes['desktop-preparer/' + name] = hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
    return hashes

def extract(text, signature):
    start = text.index(signature)
    at = text.index('{', start); depth = 1; end = at + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}'); end += 1
    return text[start:end] + '\n'

def sources():
    write_header(REPO, BUILD / 'DeviceProfile.h')
    entries = json.loads((REPO / 'compile_commands.json').read_text())
    template = max(entries, key=lambda e: e.get('command', '').count('-I'))
    template = base.split_command(template['command'])
    # A generated adapter shares only pure profile accessors. It never writes settings.
    original = (REPO / 'src/CrossPointSettings.cpp').read_text(encoding='utf-8')
    shim = '#include "CrossPointSettings.h"\n#include "ReaderFontSizes.h"\n#include "ReaderLineSpacing.h"\n'
    for method in ['CrossPointSettings::StatusBarSpec CrossPointSettings::statusBarSpec()',
                   'ReaderRenderSpec CrossPointSettings::readerRenderSpec(',
                   'float CrossPointSettings::getReaderLineCompression()',
                   'int CrossPointSettings::getReaderFontId()']:
        shim += extract(original, method)
    shim += '\nvoid CrossPointSettings::clearSdFontFamily() {}\n'
    shim += 'void CrossPointSettings::toJson(JsonDocument&) const {}\n'
    (BUILD / 'Settings.cpp').write_text(shim, encoding='utf-8')
    storage_path = REPO / '.pio/libdeps/simulator_x4_pro/simulator/src/HalStorage.cpp'
    storage = storage_path.read_text(encoding='utf-8').replace('#include <cerrno>', '#include <cerrno>\n#include <ctime>')
    storage = storage.replace('return result == 0 ? static_cast<uint32_t>(info.st_mtime) : 0;', '''if (result != 0) return 0;
  std::tm local{};
  localtime_r(&info.st_mtime, &local);
  if (local.tm_year < 80 || local.tm_year > 207) return 0;
  const uint16_t date = ((local.tm_year - 80) << 9) | ((local.tm_mon + 1) << 5) | local.tm_mday;
  const uint16_t time = (local.tm_hour << 11) | (local.tm_min << 5) | (local.tm_sec / 2);
  return (uint32_t(date) << 16) | time;''')
    (BUILD / 'HalStorage.cpp').write_text(storage, encoding='utf-8')
    # Desktop cannot observe external filesystem edits through the device mutation
    # protocol. Zero stamps disable resident reuse; persisted caches are verified.
    header = storage_path.with_suffix('.h').read_text(encoding='utf-8')
    shims = {
        'contentRevision': 'uint32_t contentRevision() const { return 0; }',
        'resourceRevision': 'uint32_t resourceRevision() const { return 0; }',
        'mediaRevision': 'uint32_t mediaRevision() const { return 0; }',
        'stateFileRevision': 'uint32_t stateFileRevision(const char*) { return 0; }',
        'removeDerivedCache': 'bool removeDerivedCache(const char* path) { return remove(path); }',
        'renameDerivedCache': 'bool renameDerivedCache(const char* from, const char* to) { return rename(from, to); }',
        'replaceDerivedCache': 'bool replaceDerivedCache(const char* from, const char* to) { return replaceFile(from, to); }',
        'openDerivedCacheForWrite': 'bool openDerivedCacheForWrite(const char* module, const char* path, HalFile& file) { return openFileForWrite(module, path, file); }',
        'openDerivedCoverForUpdate': 'bool openDerivedCoverForUpdate(const char* module, const char* path, size_t, HalFile& file) { return openFileForUpdate(module, path, file); }',
    }
    missing = [definition for name, definition in shims.items() if not re.search(r'\b' + name + r'\s*\(', header)]
    header = header.replace('  bool ready() const;', '  bool ready() const;\n  ' + '\n  '.join(missing))
    if not re.search(r'bool\s+openNextFile\s*\(HalFile\s*&', header):
        header = header.replace('  HalFile openNextFile();', '''  bool openNextFile(HalFile& entry, bool& ioError) {
    entry = openNextFile(ioError);
    return bool(entry);
  }
  HalFile openNextFile();''')
    (BUILD / 'HalStorage.h').write_text(header, encoding='utf-8')

    # Keep source-owned includes intact while adapting libc++ hashes to the device ABI.
    import shutil
    for folder in ['Epub', 'Xtc']:
        for path in (REPO / 'lib' / folder).rglob('*'):
            if not path.is_file() or path.suffix not in ('.cpp', '.h'): continue
            relative = path.relative_to(REPO / 'lib')
            target = BUILD / 'device-sources' / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            text = path.read_text(encoding='utf-8').replace('std::hash<std::string>{}', 'deviceStringHash')
            if path.name == 'PixelCache.h':
                # A JPEG MCU can be taller than a tiny image; only visible rows enter its cache.
                text = text.replace('if (wantRows < maxBlockDstRows)', 'if (wantRows < std::min(maxBlockDstRows, h))')
                # The desktop can hold a full 800x480 2-bit image; the device keeps its 24 KiB ceiling.
                text = text.replace('MAX_BAND_BYTES = 24 * 1024', 'MAX_BAND_BYTES = 128 * 1024')
            if not target.exists() or target.read_text(encoding='utf-8') != text:
                target.write_text(text, encoding='utf-8')
    chosen = {
        'src/SdCardFontSystem.cpp', 'src/ReaderFontSizes.cpp',
        'src/apps_local/bookprep/BookPreparation.cpp', 'src/apps_local/bookprep/BookPreparationGeometry.cpp',
        'src/apps_local/home/HomeScreen.cpp', 'src/components/UITheme.cpp',
        'src/components/CoverGridHomeUi.cpp', 'src/components/UIScale.cpp',
        'src/apps_local/ui/ToyboxTheme.cpp', 'src/apps_local/ui/PixelMenu.cpp',
        'src/components/UiTarget.cpp', 'src/components/SharedUiTheme.cpp', 'src/components/LanguageUiFonts.cpp',
        'src/apps_local/home/CoverBitmapPainter.cpp',
        'src/apps_local/library/PreparedCoverCache.cpp',
    }
    # Use the firmware's allocation API with the simulator's heap implementation.
    (BUILD / 'HalMemory.h').write_bytes((REPO / 'lib/hal/HalMemory.h').read_bytes())
    jobs = []
    for entry in entries:
        p = Path(entry['file']); p = p if p.is_absolute() else REPO / p
        if not p.exists(): continue
        rel = p.relative_to(REPO).as_posix() if p.is_relative_to(REPO) else ''
        if not (rel.startswith(('lib/', '.pio/libdeps/', 'freeink-sdk/', 'src/components/themes/')) or rel in chosen): continue
        if p.name in ('simulator_main.cpp', 'HttpDownloader.cpp'): continue
        args = base.split_command(entry['command']) if 'command' in entry else entry['arguments']
        if p == storage_path: p = BUILD / 'HalStorage.cpp'
        if rel == '.pio/libdeps/simulator_x4_pro/simulator/src/HalMemory.cpp':
            p = REPO / 'lib/hal/HalMemory.cpp'
        if rel.startswith(('lib/Epub/', 'lib/Xtc/')):
            p = BUILD / 'device-sources' / Path(rel).relative_to('lib')
        jobs.append((p, args))
    # The simulator database can predate newly added shared preparation sources.
    # Include new EPUB units and the same bar geometry used by the reader.
    present = {p.resolve() for p, _ in jobs}
    for original in (REPO / 'lib/Epub').rglob('*.cpp'):
        source = BUILD / 'device-sources/Epub' / original.relative_to(REPO / 'lib/Epub')
        if original.name != 'ContentProtection.cpp' and source.resolve() not in present:
            jobs.append((source, template))
    jobs.append((REPO / 'src/apps_local/readerbars/ReaderBars.cpp', template))
    jobs += [(ROOT / 'src/engine.cpp', template), (BUILD / 'Settings.cpp', template),
             (REPO / 'tools_local/wasm/src/sdl_browser.cpp', template)]
    return jobs

original_translate = base.translate
def translate(args, obj, is_c):
    flags = original_translate(args, obj, is_c)
    if not is_c:
        flags[1:1] = ['-include', str(ROOT / 'src/compatibility.h'), '-include', str(REPO / 'lib/Memory/Memory.h'),
                      '-I' + str(BUILD), '-I' + str(REPO / 'lib/Performance'),
                      '-I' + str(REPO / 'lib/XmlParserUtils'),
                      '-I' + str(BUILD / 'device-sources/Xtc'), '-I' + str(BUILD / 'device-sources/Epub')]
    # Desktop CPU budget: keep the same algorithms and ABI, optimize them for throughput.
    prefixes = {str(REPO): '/crossnext', REPO.as_posix(): '/crossnext',
                str(Path.home()): '/builder', Path.home().as_posix(): '/builder'}
    maps = ['-ffile-prefix-map=' + local + '=' + public
            for local, public in sorted(prefixes.items(), key=lambda item: -len(item[0]))]
    feature_flags = ['-D' + name + '=' + str(value) for name, value in sorted(FONT_FEATURES.items())]
    return [f for f in flags if f not in ('-Oz', '-Os', '-Og') and
            not any(f.startswith('-D' + name + '=') for name in FONT_FEATURES)] + ['-O2', *feature_flags, *maps]
base.translate = translate

if __name__ == '__main__':
    os.chdir(REPO)
    inputs = source_hashes()
    jobs = sources(); print('Compiling', len(jobs), 'units', flush=True)
    ready = subprocess.run([str(SDK / ('emcc.exe' if os.name == 'nt' else 'emcc')), '-x', 'c', '-fsyntax-only', '-'],
                           input='', capture_output=True, text=True)
    if ready.returncode:
        sys.exit(ready.stderr)
    objects = []; errors = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=min(12, os.cpu_count() or 4)) as pool:
        for src, obj, code, err in pool.map(base.compile_one, jobs):
            if code: errors.append((str(src), err))
            else: objects.append(str(obj))
    if errors:
        (BUILD / 'compile-errors.txt').write_text('\n'.join(p + '\n' + e for p,e in errors), encoding='utf-8')
        print('\n'.join(p + '\n' + e[-1800:] for p,e in errors[:6])); sys.exit(1)
    flags = [*objects, '-o', str(OUT / 'engine.cjs'), '-pthread', '-O2', '-Wl,--error-limit=0',
             '-sSTACK_SIZE=4MB', '-sDEFAULT_PTHREAD_STACK_SIZE=4MB', '-sALLOW_MEMORY_GROWTH=1',
             '-sINITIAL_MEMORY=134217728', '-sMAXIMUM_MEMORY=2147483648', '-sEXIT_RUNTIME=0',
             '-sMODULARIZE=1', '-sEXPORT_NAME=createPreparationEngine', '-sENVIRONMENT=node',
             '-lnodefs.js', '-sEXPORTED_RUNTIME_METHODS=ccall,FS,ENV,PThread',
             '-sEXPORTED_FUNCTIONS=_main,_prepare_initialize,_prepare_orientation,_prepare_font,_prepare_open,_prepare_step,_prepare_status,_prepare_cancel,_prepare_hash,_prepare_stamp,_prepare_verify,_prepare_native_covers']
    response = BUILD / 'link.rsp'
    response.write_text(shlex.join(flags), encoding='utf-8')
    r = subprocess.run([str(SDK / ('em++.exe' if os.name == 'nt' else 'em++')), '@' + str(response)], cwd=REPO, text=True, capture_output=True)
    (BUILD / 'link.log').write_text(r.stdout + r.stderr, encoding='utf-8')
    if r.returncode == 0:
        import re
        if inputs != source_hashes():
            sys.exit('Firmware sources changed during compilation; rebuild before packaging.')
        version = int(re.search(r'SECTION_FILE_VERSION = (\d+)',(REPO/'lib/Epub/Epub/Section.cpp').read_text()).group(1))
        import shutil
        license_files = {}
        for directory in ['lib', 'freeink-sdk', 'licenses', 'desktop-preparer/licenses', '.pio/libdeps/simulator_x4_pro']:
            for source in sorted((REPO / directory).rglob('*')):
                if not source.is_file() or not source.name.lower().startswith(('license', 'licence', 'copying', 'copyright', 'notice', 'ofl', 'ufl', 'ftl', 'gpl')):
                    continue
                relative = source.relative_to(REPO).as_posix()
                name = 'LICENSE-' + hashlib.sha256(relative.encode()).hexdigest()[:12] + '-' + source.name
                shutil.copy2(source, OUT / name)
                license_files[name] = {'source': relative, 'sha256': hashlib.sha256(source.read_bytes()).hexdigest()}
        for source, name in [(REPO / 'LICENSE', 'CROSSNEXT-LICENSE.txt'), (REPO / 'THIRD-PARTY-NOTICES.md', 'THIRD-PARTY-NOTICES.txt')]:
            shutil.copy2(source, OUT / name)
            license_files[name] = {'source': source.relative_to(REPO).as_posix(), 'sha256': hashlib.sha256(source.read_bytes()).hexdigest()}
        config = configparser.ConfigParser(interpolation=None)
        config.read(REPO / 'platformio.ini', encoding='utf8')
        (OUT/'engine-build.json').write_text(json.dumps({'firmwareVersion': config.get('crossplay', 'version'), 'applicationVersion':json.loads((ROOT/'package.json').read_text())['version'], 'cacheVersion':version,'fontFeatures':FONT_FEATURES,'sourceHashes':inputs,'licenseFiles':license_files},indent=2),encoding='utf-8')
    print(r.stderr[-7000:]); sys.exit(r.returncode)
