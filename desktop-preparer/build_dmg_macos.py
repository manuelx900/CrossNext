"""Create installable DMGs from the verified macOS application ZIPs on a Mac."""
from pathlib import Path
import hashlib
import json
import plistlib
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parent
VERSION = json.loads((ROOT/'package.json').read_text(encoding='utf-8'))['version']
OUT = ROOT/'dist/desktop'

if __name__ == '__main__':
    if sys.platform != 'darwin':
        sys.exit('DMG creation requires macOS hdiutil. The application ZIPs work independently of DMG packaging.')
    for arch in ('arm64', 'x64'):
        archive = OUT/f'CrossNext-Book-Preparer-macOS-{arch}-v{VERSION}-unsigned.zip'
        with zipfile.ZipFile(archive) as package:
            assert package.testzip() is None, 'Damaged application archive'
            app = f'CrossNext Book Preparer {VERSION}.app'
            assert plistlib.loads(package.read(app+'/Contents/Info.plist'))['CFBundleVersion'] == VERSION
        with tempfile.TemporaryDirectory(prefix='CrossNext-dmg-') as temp:
            staging = Path(temp)/'volume'
            staging.mkdir()
            # ditto preserves the framework symlinks and executable modes.
            subprocess.run(['ditto','-x','-k',str(archive),str(staging)],check=True)
            (staging/'Applications').symlink_to('/Applications',target_is_directory=True)
            disk = OUT/f'CrossNext-Book-Preparer-macOS-{arch}-v{VERSION}.dmg'
            subprocess.run(['hdiutil','create','-ov','-fs','HFS+','-format','UDZO',
                            '-volname',f'CrossNext {VERSION}','-srcfolder',str(staging),str(disk)],check=True)
            subprocess.run(['hdiutil','verify',str(disk)],check=True)
            print(disk)
            print(hashlib.file_digest(disk.open('rb'),'sha256').hexdigest())
