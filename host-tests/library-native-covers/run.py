from pathlib import Path
import os,shutil,subprocess
root=Path(__file__).resolve().parents[2];out=root/'qa-artifacts/library-native-covers-2026-11-13/checks';out.mkdir(exist_ok=True,parents=True)
wasm=os.name=='nt';compiler=str(root/'.venv/emsdk/upstream/emscripten/em++.exe') if wasm else os.environ.get('CXX','c++')
env=os.environ.copy()
if wasm:env.setdefault('EM_CACHE',str(out/'emscripten-cache'))
flags=['-sENVIRONMENT=node','-sEXIT_RUNTIME=1','-sALLOW_MEMORY_GROWTH=1'] if wasm else []
includes=['lib/Performance','host-tests/library-prefetch/stubs','host-tests/home-cache/stubs','host-tests/reading-suite/stubs','lib/Memory','lib/GfxRenderer','src','freeink-sdk/libs/ui/FreeInkUI/include']
sources=['host-tests/library-native-covers/test.cpp','src/apps_local/library/PreparedCoverCache.cpp','src/apps_local/library/LibraryCoverPrefetch.cpp','src/apps_local/home/CoverBitmapPainter.cpp','lib/GfxRenderer/Bitmap.cpp','lib/GfxRenderer/BitmapHelpers.cpp']
binary=out/('native.cjs' if wasm else 'native');log=out/'native.log'
with log.open('w',encoding='utf-8') as file:
 result=subprocess.run([compiler,'-std=c++20','-O1','-Wall','-Wextra','-Werror','-Wno-comment',*flags,*['-I'+str(root/p) for p in includes],*sources,'-o',str(binary)],cwd=root,env=env,stdout=file,stderr=subprocess.STDOUT)
if result.returncode:print(log.read_text()[-10000:]);raise SystemExit(result.returncode)
for args in [[],['oom']]:
 result=subprocess.run(([shutil.which('node') or 'node'] if wasm else [])+[str(binary),*args],cwd=root,capture_output=True,text=True)
 with log.open('a',encoding='utf-8') as file:file.write(result.stdout+result.stderr)
 print(result.stdout[-2000:],flush=True)
 if result.returncode:raise SystemExit(result.returncode)
header=(root/'src/RecentBooksStore.h').read_text(encoding='utf-8')
record=header[header.index('struct RecentBook {'):header.index('};',header.index('struct RecentBook {'))+2]
source=(root/'src/RecentBooksStore.cpp').read_text(encoding='utf-8');start=source.index('bool RecentBooksStore::isMissing(')
end=source.index('\n}',start)+2
mutation=source[source.index('namespace {'):source.index('\n}',source.index('namespace {'))+2]
invalidate=source[source.index('void RecentBooksStore::invalidatePresence()'):source.index('\n',source.index('void RecentBooksStore::invalidatePresence()'))]
(out/'presence-production.h').write_text(record+'\nclass RecentBooksStore { public: static bool isMissing(const RecentBook&); static void invalidatePresence(); };\n'+mutation+'\n'+source[start:end]+'\n'+invalidate+'\n',encoding='utf-8')
presence=out/('presence.cjs' if wasm else 'presence')
subprocess.run([compiler,'-std=c++20','-O1',*flags,'-I'+str(out),'-I'+str(root/'lib/LibraryIndex'),str(root/'host-tests/library-native-covers/presence.cpp'),'-o',str(presence)],cwd=root,env=env,check=True)
subprocess.run(([shutil.which('node') or 'node'] if wasm else [])+[str(presence)],cwd=root,check=True)
home=(root/'src/activities/home/HomeActivity.cpp').read_text(encoding='utf-8')
start=home.index('void HomeActivity::runIdleWork()');end=home.index('\nvoid HomeActivity::render(',start)
guard=home[home.index('  const bool shelfWork ='):home.index('  if (coverGridUi &&',home.index('  const bool shelfWork ='))]
preamble=home[home.index('void HomeActivity::loop() {')+len('void HomeActivity::loop() {'):home.index('  const int options =',home.index('void HomeActivity::loop() {'))]
lock_at=home.index('  // Keep this input frame')
input_lock=home[lock_at:home.index('  if (!libraryWarmup.enabled())',lock_at)]
(out/'warmup-production.h').write_text(home[start:end]+'\nvoid HomeActivity::schedule() { const bool warmInput=false;\n'+guard+'}\nvoid HomeActivity::pollInput() {\n'+preamble+input_lock+'if(mappedInput.tap) ++delivered;\n}\n',encoding='utf-8')
warmup=out/('warmup.cjs' if wasm else 'warmup')
subprocess.run([compiler,'-std=c++20','-O1',*flags,'-I'+str(out),str(root/'host-tests/library-native-covers/warmup.cpp'),'-o',str(warmup)],cwd=root,env=env,check=True)
subprocess.run(([shutil.which('node') or 'node'] if wasm else [])+[str(warmup)],cwd=root,check=True)
