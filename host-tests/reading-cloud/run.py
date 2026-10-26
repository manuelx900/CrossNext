"""Host tests. Emscripten runs the C++ code under Node on Windows/Linux/macOS."""
from pathlib import Path
import argparse,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser();parser.add_argument('--emxx',default=str(root/'.venv/emsdk/upstream/emscripten/em++.exe'));args=parser.parse_args()
base=['-std=c++20','-O1','-Wall','-Wextra','-Werror','-Wno-comment','-sENVIRONMENT=node','-sEXIT_RUNTIME=1','-sALLOW_MEMORY_GROWTH=1','-sSTACK_SIZE=1048576']
core=['src/apps_local/reading/'+name+'.cpp' for name in ['ReadingStatsCloudStore','ReadingStatsCloudImport','ReadingStatsProjection','ReadingStatsCore']]
includes=['host-tests/reading-cloud/stubs','host-tests/reading-suite/stubs','lib/Memory','lib/Serialization','.pio/libdeps/simulator_x4_pro/ArduinoJson/src']
with tempfile.TemporaryDirectory(prefix='crossnext-stat-tests-') as folder:
    for name,files in [('import',['host-tests/reading-cloud/test.cpp',*core]),('sync',['host-tests/reading-cloud/test_sync.cpp','src/apps_local/reading/ReadingStatsUpload.cpp',*core])]:
        output=Path(folder)/(name+'.cjs')
        subprocess.run([args.emxx,*base,*['-I'+x for x in includes],*files,'-o',str(output)],cwd=root,check=True)
        subprocess.run(['node',str(output)],cwd=root,check=True)
    output=Path(folder)/'ui.cjs'
    include=['freeink-sdk/libs/ui/FreeInkUI/include','freeink-sdk/libs/assets/Icons/include','lib/EpdFont','lib/I18n']
    files=['host-tests/reading-cloud/test_ui.cpp','src/apps_local/reading/ReadingStatsScreens.cpp','src/apps_local/reading/ReadingStatsCore.cpp','src/apps_local/ui/PixelMenu.cpp','lib/I18n/I18n.cpp','lib/I18n/I18nStrings.cpp','freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp']
    subprocess.run([args.emxx,*base,*['-I'+x for x in include],*files,'-o',str(output)],cwd=root,check=True)
    subprocess.run(['node',str(output)],cwd=root,check=True)
