"""Package a standalone Windows x64 executable; no installer or Python required."""
from pathlib import Path
import hashlib, json, shutil, subprocess, zipfile, sys, urllib.request
from PIL import Image
ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent
RUNTIME = ROOT / 'dist/runtime'
DIST = ROOT / 'dist'
logo = Image.open(REPO / 'src/images/Logo120.png').convert('RGBA')
white = Image.new('RGBA', logo.size, 'white'); white.putalpha(logo.getchannel('A'))
white.save(ROOT / 'build/crossnext-white.png')
icon = Image.new('RGBA',(256,256),'black')
mark = white.resize((224,224),Image.Resampling.NEAREST)
icon.alpha_composite(mark,(16,16))
icon.save(ROOT/'build/crossnext.ico',sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
shutil.copy2(ROOT / 'src/worker.cjs', RUNTIME / 'worker.cjs')
shutil.copy2(ROOT / 'shell/card-storage.cjs', RUNTIME / 'card-storage.cjs')
shutil.copy2(shutil.which('node'), RUNTIME / 'node.exe')
version = subprocess.check_output([shutil.which('node'),'--version'],text=True).strip()
node_license = Path(shutil.which('node')).parent / 'LICENSE'
if node_license.exists(): shutil.copy2(node_license, RUNTIME / 'NODE-LICENSE.txt')
elif not (RUNTIME/'NODE-LICENSE.txt').exists() or not (RUNTIME/'NODE-VERSION.txt').exists() or (RUNTIME/'NODE-VERSION.txt').read_text().strip()!=version:
    urllib.request.urlretrieve('https://raw.githubusercontent.com/nodejs/node/'+version+'/LICENSE',RUNTIME/'NODE-LICENSE.txt')
(RUNTIME/'NODE-VERSION.txt').write_text(version,encoding='utf-8')
shutil.copy2(REPO / 'freeink-sdk/libs/font/FreeInkFont/third_party/freetype/LICENSE.TXT', RUNTIME / 'FREETYPE-LICENSE.txt')
shutil.copy2(REPO / 'LICENSE', RUNTIME / 'CROSSNEXT-LICENSE.txt')
shutil.copy2(REPO / 'THIRD-PARTY-NOTICES.md', RUNTIME / 'THIRD-PARTY-NOTICES.txt')
shutil.copy2(REPO / 'Resources/fonts/Ubuntu/UFL.txt', RUNTIME / 'UBUNTU-FONT-LICENSE.txt')
shutil.copy2(REPO / 'Resources/fonts/Noto Serif/OFL.txt', RUNTIME / 'NOTO-FONT-LICENSE.txt')
engine_build = json.loads((RUNTIME/'engine-build.json').read_text(encoding='utf-8'))
manifest = {'product':'CrossNext Book Preparer', 'channel':json.loads((ROOT/'package.json').read_text(encoding='utf8'))['version'], 'architecture':'Windows x64',
            'firmwareCommit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=REPO,text=True).strip(),
            'cacheVersion':engine_build['cacheVersion'], 'files':{}}
for p in sorted(RUNTIME.glob('*')):
    if p.is_file() and p.name != 'manifest.json': manifest['files'][p.name] = hashlib.sha256(p.read_bytes()).hexdigest()
(RUNTIME/'manifest.json').write_text(json.dumps(manifest,indent=2), encoding='utf-8')
archive = ROOT / 'build/runtime.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as out:
    for p in sorted(RUNTIME.iterdir()):
        if p.is_file(): out.write(p,p.name)
compiler = Path('C:/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe')
test = '--test' in sys.argv
name = 'CrossNext-Book-Preparer-Test.exe' if test else 'CrossNext-Book-Preparer-DPI.exe'
if test and '--test-name' in sys.argv:
    name = sys.argv[sys.argv.index('--test-name') + 1]
    if Path(name).name != name or not name.endswith('.exe'):
        raise ValueError('Use an executable basename for --test-name')
exe = DIST / name
subprocess.run([str(compiler),'/nologo','/target:winexe','/platform:x64','/optimize+',
    '/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll','/reference:System.Web.Extensions.dll',
    '/reference:System.IO.Compression.dll','/reference:System.IO.Compression.FileSystem.dll',
    '/win32manifest:'+str(ROOT/'src/app.manifest'),'/resource:'+str(archive)+',runtime.zip',
    '/win32icon:'+str(ROOT/'build/crossnext.ico'), '/resource:'+str(ROOT/'build/crossnext.ico')+',crossnext.ico',
    '/resource:'+str(ROOT/'build/crossnext-white.png')+',crossnext-white.png',
    *(['/define:PREPARER_TEST'] if test else []), '/out:'+str(exe),str(ROOT/'src/Program.cs'),str(ROOT/'src/Interface.cs'),str(ROOT/'src/DpiLayout.cs'),str(ROOT/'src/DpiTests.cs')],check=True,cwd=ROOT)
print(str(exe),exe.stat().st_size,hashlib.sha256(exe.read_bytes()).hexdigest())
if not test:
    for name in ('README.txt', 'BUILD.txt'):
        shutil.copy2(ROOT / name, DIST / name)
    sums = DIST / 'DPI-SHA256SUMS.txt'
    sums.write_text(hashlib.sha256(exe.read_bytes()).hexdigest() + '  ' + exe.name + '\n', encoding='utf-8')
    portable = DIST / 'CrossNext-Book-Preparer-DPI-Portable.zip'
    with zipfile.ZipFile(portable, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as out:
        for path in (exe, DIST/'README.txt', DIST/'BUILD.txt', sums):
            out.write(path, path.name)
    print(str(portable), portable.stat().st_size)
