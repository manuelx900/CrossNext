"""Assemble unsigned macOS preview bundles on any host, preserving Unix modes and symlinks.

For signed distribution, run `npm run dist:mac` on macOS with a signing identity.
"""
from pathlib import Path
import argparse, copy, hashlib, json, plistlib, stat, sys, urllib.request, zipfile

ROOT = Path(__file__).resolve().parent
APP_VERSION = json.loads((ROOT / 'package.json').read_text(encoding='utf-8'))['version']
APP = f'CrossNext Book Preparer {APP_VERSION}.app'
OUT = ROOT / 'dist/desktop'
CACHE = ROOT / 'build/electron-downloads'


def digest(filename):
    with filename.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def add_file(archive, filename, destination):
    info = zipfile.ZipInfo(destination)
    info.create_system = 3
    info.external_attr = (stat.S_IFREG | 0o644) << 16
    info.compress_type = zipfile.ZIP_DEFLATED
    archive.writestr(info, filename.read_bytes())


def build(arch):
    version = json.loads((ROOT / 'package.json').read_text(encoding='utf-8'))['devDependencies']['electron']
    name = f'electron-v{version}-darwin-{arch}.zip'
    source = CACHE / name
    expected = json.loads((ROOT / 'node_modules/electron/checksums.json').read_text())[name]
    if not source.exists() or digest(source) != expected:
        print(f'Downloading Electron {version} for macOS {arch}…', flush=True)
        urllib.request.urlretrieve(f'https://github.com/electron/electron/releases/download/v{version}/{name}', source)
    if digest(source) != expected:
        raise ValueError(f'Electron checksum mismatch: {name}')
    destination = OUT / f'CrossNext-Book-Preparer-macOS-{arch}-v{APP_VERSION}-unsigned.zip'
    print(f'Packaging {destination.name}…', flush=True)
    with zipfile.ZipFile(source) as original, zipfile.ZipFile(destination, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as bundle:
        for item in original.infolist():
            if item.filename in ('LICENSE', 'LICENSES.chromium.html'):
                entry = copy.copy(item)
                entry.filename = APP + '/Contents/Resources/' + ('LICENSE.electron.txt' if item.filename == 'LICENSE' else item.filename)
                bundle.writestr(entry, original.read(item))
                continue
            if not item.filename.startswith('Electron.app/'):
                continue
            relative = item.filename[len('Electron.app/'):]
            if relative.startswith('Contents/_CodeSignature/') or relative == 'Contents/Resources/default_app.asar':
                continue
            data = original.read(item)
            if relative == 'Contents/Info.plist':
                info = plistlib.loads(data)
                info.update(CFBundleName='CrossNext Book Preparer', CFBundleDisplayName='CrossNext Book Preparer',
                            CFBundleIdentifier='org.crossnext.bookpreparer', CFBundleShortVersionString=APP_VERSION,
                            CFBundleVersion=APP_VERSION, CFBundleIconFile='crossnext.icns',
                            LSApplicationCategoryType='public.app-category.books')
                # Engine and shell are plain JS/WASM. No host native addon is bundled.
                data = plistlib.dumps(info)
            entry = copy.copy(item)
            entry.filename = APP + '/' + relative
            entry.compress_type = zipfile.ZIP_DEFLATED
            bundle.writestr(entry, data)
        add_file(bundle, ROOT / 'shell/assets/icon.icns', APP + '/Contents/Resources/crossnext.icns')
        package = json.loads((ROOT / 'package.json').read_text(encoding='utf-8'))
        package = {key: package[key] for key in ('name', 'productName', 'version', 'description', 'main', 'author', 'license')}
        info = zipfile.ZipInfo(APP + '/Contents/Resources/app/package.json')
        info.create_system = 3
        info.external_attr = (stat.S_IFREG | 0o644) << 16
        bundle.writestr(info, json.dumps(package, indent=2))
        for source in (ROOT / 'shell').rglob('*'):
            if source.is_file() and (source.name in ('main.cjs', 'preload.cjs', 'platform.cjs', 'card-storage.cjs', 'cache-cleaner.cjs', 'cache-transaction.cjs') or 'ui' in source.parts or 'assets' in source.parts):
                add_file(bundle, source, APP + '/Contents/Resources/app/' + source.relative_to(ROOT).as_posix())
        for source in (ROOT / 'build/desktop-runtime').iterdir():
            if source.is_file():
                add_file(bundle, source, APP + '/Contents/Resources/runtime/' + source.name)
        note = ('CrossNext Book Preparer — macOS ' + arch + '\n\n'
                'Unsigned development preview, assembled on ' + sys.platform + '.\n'
                'This bundle has not been executed on macOS. A Mac is required to validate it.\n'
                'For a locally signed build on a Mac, use npm ci; npm run dist:mac.\n'
                'For public distribution, configure an Apple Developer identity and notarization.\n'
                'The app includes the existing firmware WebAssembly engine, not a Windows executable.\n'
                'Choose the card volume under /Volumes and safely eject it in Finder when finished.\n')
        bundle.writestr('READ-ME-FIRST.txt', note)
    with zipfile.ZipFile(destination) as bundle:
        executable = bundle.getinfo(APP + '/Contents/MacOS/Electron')
        assert executable.external_attr >> 16 & 0o111, 'Executable permissions lost'
        assert any(stat.S_ISLNK(item.external_attr >> 16) for item in bundle.infolist()), 'Framework symlinks lost'
        assert bundle.read(APP + '/Contents/Resources/runtime/engine.wasm') == (ROOT / 'build/desktop-runtime/engine.wasm').read_bytes()
        assert plistlib.loads(bundle.read(APP + '/Contents/Info.plist'))['CFBundleIdentifier'] == 'org.crossnext.bookpreparer'
    print(f'Validated archive structure, Unix permissions, symlinks and engine: {destination.name}', flush=True)
    return destination


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--arch', choices=('arm64', 'x64', 'all'), default='all')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    CACHE.mkdir(parents=True, exist_ok=True)
    outputs = [build(arch) for arch in (('arm64', 'x64') if args.arch == 'all' else (args.arch,))]
    (OUT / 'MACOS-SHA256SUMS.txt').write_text(''.join(f'{digest(file)}  {file.name}\n' for file in outputs), encoding='utf-8')
