from pathlib import Path
import subprocess,os
r=Path(__file__).resolve().parents[2];out=r/'datos.local/daily-goal-ui-2026-10-30/goal-host';out.mkdir(exist_ok=True)
em=r/'.venv/emsdk/upstream/emscripten/em++.exe';node=next((r/'.venv/emsdk/node').glob('*/bin/node.exe'),None) or next((r/'.venv/emsdk/node').glob('*/node.exe'))
sources=['host-tests/reading-goal/test.cpp']+['src/apps_local/reading/'+n+'.cpp' for n in ['ReadingGoalCache','ReadingStatsCore','ReadingStatsStore','ReadingStatsProjection','ReadingStatsCloudStore']]
includes=['host-tests/reading-cloud/stubs','host-tests/reading-suite/stubs','lib/Memory','lib/Serialization']
subprocess.run([str(em),'-std=c++20','-Wall','-Wextra','-Werror','-O1','-sSTACK_SIZE=1048576','-sENVIRONMENT=node','-sEXIT_RUNTIME=1','-sALLOW_MEMORY_GROWTH=1',*['-I'+i for i in includes],*sources,'-o',str(out/'tests.js')],cwd=r,check=True)
subprocess.run([str(node),str(out/'tests.js')],cwd=r,check=True)

subprocess.run([str(em),'-std=c++20','-Wall','-Wextra','-Werror','-Wno-parentheses','-O1','-sSTACK_SIZE=1048576','-sENVIRONMENT=node','-sEXIT_RUNTIME=1','-sALLOW_MEMORY_GROWTH=1',*['-I'+i for i in ['host-tests/xtc-reading/stubs',*includes,'lib/Xtc','lib/GfxRenderer']],'host-tests/reading-goal/cover.cpp','lib/Xtc/Xtc.cpp','lib/Xtc/Xtc/XtcParser.cpp','lib/GfxRenderer/BitmapHelpers.cpp','-o',str(out/'cover.js')],cwd=r,check=True)
subprocess.run([str(node),str(out/'cover.js')],cwd=r,check=True)
