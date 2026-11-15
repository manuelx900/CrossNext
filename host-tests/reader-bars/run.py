from pathlib import Path
import os,subprocess,shutil,sys
root=Path(__file__).resolve().parents[2];out=root/'.pio/host-tests/reader-bars';out.mkdir(parents=True,exist_ok=True)
wasm=os.name=='nt';compiler=str((root/'.venv/emsdk/upstream/emscripten/em++.exe').resolve()) if wasm else os.environ.get('CXX','c++')
node=(shutil.which('node') or str(next((root/'.venv/emsdk/node').glob('*/bin/node.exe')))) if wasm else None
os.environ['EM_CACHE']=str(out/'emscripten-cache')
common=['-std=c++20','-Wall','-Wextra','-Werror','-Wno-comment','-Wno-missing-field-initializers','-O2','-I.', '-Ihost-tests/reader-bars/stubs','-Ifreeink-sdk/libs/ui/FreeInkUI/include']
if wasm:common+=['-sENVIRONMENT=node','-sEXIT_RUNTIME=1','-sSTACK_SIZE=1048576','-sALLOW_MEMORY_GROWTH=1','-sNODERAWFS=1']
json=Path(os.environ['ARDUINOJSON_INCLUDE']) if 'ARDUINOJSON_INCLUDE' in os.environ else next((root/'.pio/libdeps').glob('*/ArduinoJson/src'),None)
if json is None:raise SystemExit('Install the project dependencies before this suite (platformio pkg install -e x4pro).')
common+=['-I'+str(json)]
for name,src,extra in [('core',['host-tests/reader-bars/test.cpp'],[])]+[('render',['host-tests/reader-bars/render.cpp','src/apps_local/readerbars/ReaderBars.cpp'],[])]:
 exe=out/(name+('.cjs' if wasm else ''))
 subprocess.run([compiler,*common,*extra,*src,'-o',str(exe)],cwd=root,check=True)
 subprocess.run(([node] if wasm else [])+[str(exe),str(out/name)],cwd=root,check=True)
