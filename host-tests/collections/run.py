from pathlib import Path
import os,subprocess,shutil,sys
root=Path(__file__).resolve().parents[2]
out=root/".pio/host-tests/collections";out.mkdir(parents=True,exist_ok=True)
wasm=os.name=="nt";sdk=(root/".venv/emsdk").resolve()
if wasm: os.environ['EM_CACHE']=str(sdk.parent.parent/'.pio/em-cache-keyboard-audit')
compiler=str(sdk/"upstream/emscripten/em++.exe") if wasm else os.environ.get("CXX","c++")
exe=out/("store.cjs" if wasm else "store")
flags=["-sSTACK_SIZE=1048576","-sENVIRONMENT=node","-sEXIT_RUNTIME=1","-sALLOW_MEMORY_GROWTH=1"] if wasm else []
subprocess.run([compiler,"-std=c++20","-Wall","-Wextra","-Werror","-Wno-comment","-O2","-Ihost-tests/collections/stubs","-Ilib/Memory","-Isrc","-Ilib/FsHelpers","-I.",*flags,"host-tests/collections/test.cpp","src/apps_local/library/CollectionStore.cpp","src/apps_local/library/CollectionFolder.cpp","-o",str(exe)],cwd=root,check=True)
subprocess.run(([shutil.which("node")] if wasm else [])+[str(exe)],cwd=root,check=True)
browse=out/("browse.cjs" if wasm else "browse")
subprocess.run([compiler,"-std=c++20","-Wall","-Wextra","-Werror","-Wno-comment","-O2","-Ihost-tests/collections/stubs","-Ilib/Memory","-Isrc","-Ilib/FsHelpers","-I.",*flags,"host-tests/collections/browse.cpp","src/apps_local/library/CollectionStore.cpp","src/apps_local/library/CollectionFolder.cpp","src/apps_local/library/CollectionFolderStates.cpp","-o",str(browse)],cwd=root,check=True)
subprocess.run(([shutil.which("node")] if wasm else [])+[str(browse)],cwd=root,check=True)
source=(root/"src/apps_local/library/CollectionsActivity.cpp").read_text(encoding="utf-8")
bodies=[]
for name in ["changeDirectory", "cancelEditorScan", "settleEditorFolder", "toggleEntry", "activateIndex", "onBackButton", "continueEditorScan", "inspectEditorFolders", "runIdleWork"]:
 start=source.index(("bool" if name == "continueEditorScan" else "void")+" CollectionsActivity::"+name+"(")
 end=source.index("\n}",start)+2
 bodies.append(source[start:end])
start=source.index("bool CollectionsActivity::books(")
bodies.append(source[start:source.index("\n}",start)+2])
start=source.index("  if (pending_ && millis() - coverPaint_ > 250)")
(out/"cover-queue-production.h").write_text(source[start:source.index("\n  }",start)+4],encoding="utf-8")
(out/"activity-production.h").write_text("\n".join(bodies),encoding="utf-8")
activity=out/("activity.cjs" if wasm else "activity")
subprocess.run([compiler,"-std=c++20","-Wall","-Wextra","-Werror","-Wno-comment","-O2","-Ihost-tests/collections/stubs","-Ilib/Memory","-Isrc","-Ilib/FsHelpers","-I.","-I"+str(out),"-Ifreeink-sdk/libs/ui/FreeInkUI/include",*flags,"host-tests/collections/activity.cpp","src/apps_local/library/CollectionStore.cpp","src/apps_local/library/CollectionFolder.cpp","src/apps_local/library/CollectionFolderStates.cpp","-o",str(activity)],cwd=root,check=True)
subprocess.run(([shutil.which("node")] if wasm else [])+[str(activity)],cwd=root,check=True)
finished=out/("finished.cjs" if wasm else "finished")
subprocess.run([compiler,"-std=c++20","-Wall","-Wextra","-Werror","-Wno-comment","-O2","-Ihost-tests/collections/stubs","-Ihost-tests/reading-suite/stubs","-Ilib/Memory","-Isrc","-I.",*flags,"host-tests/collections/finished.cpp","src/apps_local/library/LibraryFinishedBooks.cpp","src/apps_local/reading/ReadingStatsStore.cpp","src/util/ReadingTimeMemo.cpp","src/apps_local/reading/ReadingStatsCore.cpp","-o",str(finished)],cwd=root,check=True)
subprocess.run(([shutil.which("node")] if wasm else [])+[str(finished)],cwd=root,check=True)
subprocess.run([sys.executable,"host-tests/collections/render.py",*sys.argv[1:]],cwd=root,check=True)
