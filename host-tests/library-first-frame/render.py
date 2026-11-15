from pathlib import Path
import argparse,os,shutil,subprocess
import sys
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser()
p.add_argument("--output",type=Path,default=ROOT/".pio/host-tests/library-first-frame")
p.add_argument("--render",type=Path,default=ROOT/"qa-artifacts/library-first-frame")
p.add_argument("--baseline",type=Path,help="Optional pre-change LibraryListActivity.cpp for exact final-grid comparison")
a=p.parse_args(); out=a.output.resolve(); out.mkdir(parents=True,exist_ok=True)
wasm=os.name=="nt"
compiler=str(ROOT/".venv/emsdk/upstream/emscripten/em++.exe") if wasm else os.environ.get("CXX","c++")
node=shutil.which("node") or (next((ROOT/".venv/emsdk/node").glob("*/bin/node.exe"),None) if wasm else None)
runtime=[str(node)] if wasm else []
common=["-std=c++20","-Wall","-Wextra","-Werror","-Wno-comment","-O2"]
flags=["-sSTACK_SIZE=1048576","-sENVIRONMENT=node","-sEXIT_RUNTIME=1","-sALLOW_MEMORY_GROWTH=1","-sNODERAWFS=1"] if wasm else []
i18n=out/"i18n"; i18n.mkdir(exist_ok=True)
for f in ("I18n.cpp","I18n.h","LanguagePack.cpp","LanguagePack.h"): shutil.copy2(ROOT/"lib/I18n"/f,i18n/f)
with (out/"i18n.log").open("w",encoding="utf-8") as log:
 subprocess.run([sys.executable,"scripts/gen_i18n.py","lib/I18n/translations",str(i18n),"--builtin-langs","en,es"],cwd=ROOT,check=True,stdout=log,env={**os.environ,"PYTHONUTF8":"1","PYTHONIOENCODING":"utf-8"})
def method(source, signature):
 start=source.index(signature); brace=source.index("{",start); depth=1; end=brace+1
 while depth:
  if source[end]=="{": depth+=1
  elif source[end]=="}": depth-=1
  end+=1
 return source[start:end]
source=(ROOT/"src/activities/library/LibraryListActivity.cpp").read_text(encoding="utf-8")
methods=[method(source,sig) for sig in ["int LibraryListActivity::entryBookCount(","void LibraryListActivity::buildGrid(","void LibraryListActivity::render(","void LibraryListActivity::runIdleWork(","bool LibraryListActivity::presentationComplete("]]
if a.baseline:
 baseline=a.baseline.read_text(encoding="utf-8")
 methods.append(method(baseline,"void LibraryListActivity::buildGrid(").replace("LibraryListActivity::buildGrid(","LibraryListActivity::baselineBuildGrid(",1))
 common.append("-DHAS_LIBRARY_BASELINE=1")
else:
 print("Historical grid pixel comparison omitted; pass --baseline to enable it.",flush=True)
(out/"activity-methods.inc").write_text("\n".join(methods),encoding="utf-8")
for variant in range(1):
 exe=out/("screens-"+str(variant)+".cjs")
 includes=["-Itest/language_pack/stubs","-Ihost-tests/reading-cloud/stubs","-Ihost-tests/reading-suite/stubs","-I"+str(i18n),"-I"+str(out),"-Ilib/Epub","-Ilib/EpdFont","-Ifreeink-sdk/libs/ui/FreeInkUI/include","-Ifreeink-sdk/libs/assets/Icons/include"]
 sources=["host-tests/library-first-frame/screens.cpp","src/apps_local/ui/PixelMenu.cpp",str(i18n/"I18n.cpp"),str(i18n/"I18nStrings.cpp"),str(i18n/"LanguagePack.cpp"),"freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp"]
 subprocess.run([compiler,*common,*includes,*flags,*sources,"-o",str(exe)],cwd=ROOT,check=True)
 render_dir=a.render/str(variant); render_dir.mkdir(parents=True,exist_ok=True)
 subprocess.run([*runtime,str(exe),render_dir.resolve().as_posix()],cwd=ROOT,check=True)
