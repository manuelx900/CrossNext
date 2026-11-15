from pathlib import Path
import os,shutil,subprocess
r=Path(__file__).resolve().parents[2];out=r/"qa-artifacts/ui-four-fixes/tests";out.mkdir(parents=True,exist_ok=True)
i18n=out/"i18n";i18n.mkdir(exist_ok=True)
for f in ("I18n.cpp","I18n.h","LanguagePack.cpp","LanguagePack.h"):shutil.copy2(r/"lib/I18n"/f,i18n/f)
wasm=os.name=="nt"
with (out/"i18n.log").open("w",encoding="utf-8")as log:
 subprocess.run([str(r/".venv/Scripts/python.exe")if wasm else "python3","scripts/gen_i18n.py","lib/I18n/translations",str(i18n),"--builtin-langs","en,es"],cwd=r,stdout=log,check=True,env={**os.environ,"PYTHONUTF8":"1","PYTHONIOENCODING":"utf-8"})
compiler=str(r/".venv/emsdk/upstream/emscripten/em++.exe")if wasm else os.environ.get("CXX","c++")
node=shutil.which("node")or next((r/".venv/emsdk/node").glob("*/bin/node.exe"),None)
flags=["-sSTACK_SIZE=1048576","-sENVIRONMENT=node","-sEXIT_RUNTIME=1","-sALLOW_MEMORY_GROWTH=1","-sNODERAWFS=1"]if wasm else[]
inc=["test/language_pack/stubs","host-tests/reading-cloud/stubs","host-tests/reading-suite/stubs",str(i18n),"lib/EpdFont","freeink-sdk/libs/ui/FreeInkUI/include","freeink-sdk/libs/assets/Icons/include"]
sources=["host-tests/ui-four-fixes/test.cpp","src/apps_local/home/HomeScreen.cpp","src/apps_local/focus/FocusScreens.cpp","src/apps_local/focus/FocusCore.cpp","src/apps_local/pomodoro/PomodoroCore.cpp","src/apps_local/pomodoro/PomodoroScreens.cpp","src/apps_local/pomodoro/PomodoroStatsScreens.cpp", "src/apps_local/pomodoro/PomodoroHistory.cpp","src/apps_local/reading/ReadingStatsCore.cpp","src/apps_local/picross/PicrossCore.cpp","src/apps_local/picross/PicrossScreens.cpp","src/apps_local/pomodoro/TimerSettingsUi.cpp","src/apps_local/ui/PixelMenu.cpp",str(i18n/"I18n.cpp"),str(i18n/"I18nStrings.cpp"),str(i18n/"LanguagePack.cpp"),"freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp"]
exe=out/("test.cjs"if wasm else"test");shots=out/"shots";shots.mkdir(exist_ok=True)
subprocess.run([compiler,"-std=c++20","-O2","-Wall","-Wextra","-Werror","-Wno-comment",*["-I"+p for p in inc],*flags,*sources,"-o",str(exe)],cwd=r,check=True)
subprocess.run(([str(node)]if wasm else[])+[str(exe),shots.as_posix()],cwd=r,check=True)
