"""Exercise production page gesture dispatch and render settings option icons."""
from pathlib import Path
import argparse,subprocess,sys
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=root/'.pio/host-tests/settings-reader-ui');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
canvas=(root/'test/reader_font_sheet/ReaderFontSheetTest.cpp').read_text(encoding='utf-8').split('int main(')[0]
bridge=(root/'host-tests/calculator/test_screens.cpp').read_text(encoding='utf-8');start=bridge.index('struct Font {');bridge=bridge[start:bridge.index('\n};',start)+3].replace('struct Font {','struct FontBridge {').replace('explicit Font(','explicit FontBridge(')
start=canvas.index('struct FontBridge {');end=canvas.index('\n};',start)+3;canvas=canvas[:start]+bridge+canvas[end:]
canvas=canvas.replace('#pragma GCC diagnostic pop','#include "lib/EpdFont/builtinFonts/ubuntu_12_regular.h"\n#include "lib/EpdFont/builtinFonts/ubuntu_10_regular.h"\n#pragma GCC diagnostic pop')
src=(root/'src/activities/settings/TextSettingsActivity.cpp').read_text(encoding='utf-8');start=src.index('bool TextSettingsActivity::handleCustomInput()');body=src[start:src.index('\n}',start)+2]
settings=(root/'src/activities/settings/SettingsActivity.cpp').read_text(encoding='utf-8')
def method(name):
 start=settings.index('void SettingsActivity::'+name) if name!='handleButtons' else settings.index('bool SettingsActivity::handleButtons')
 return settings[start:settings.index('\n}',start)+2]
nav=(root/'host-tests/settings-reader-ui/SettingsNavigation.h').read_text(encoding='utf-8')
nav=nav.replace('PRODUCTION_NAV_METHODS','\n'.join(method(x) for x in ['openManga','closeManga','handleButtons']))
rebuild=method('rebuildSettingsLists')
entry=rebuild.splitlines()[1]
returns=rebuild[rebuild.rindex('  if (categoryHome)'):rebuild.rindex('\n}')]
nav=nav.replace('PRODUCTION_REBUILD_ENTRY',entry).replace('PRODUCTION_REBUILD_RETURN',returns)
(out/'SettingsNavigation.generated.h').write_text(nav,encoding='utf-8')
program=canvas+(root/'host-tests/settings-reader-ui/test.cpp').read_text(encoding='utf-8');program=program.replace('PRODUCTION_INPUT_METHOD',body)
cpp=out/'production-ui.cpp';cpp.write_text(program,encoding='utf-8')
compiler=root/'.venv/emsdk/upstream/emscripten/em++.exe';exe=out/'tests.cjs'
includes=[str(out),'.','test/reader_font_sheet','test/language_pack/stubs','host-tests/reading-suite/stubs','lib/I18n','lib/EpdFont','freeink-sdk/libs/ui/FreeInkUI/include','freeink-sdk/libs/assets/Icons/include']
subprocess.run([str(compiler),'-std=c++20','-O1','-Wall','-Wextra','-Werror','-Wno-comment',*['-I'+x for x in includes],str(cpp),'src/apps_local/ui/PixelMenu.cpp','lib/I18n/I18n.cpp','lib/I18n/I18nStrings.cpp','lib/I18n/LanguagePack.cpp','freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp','-sENVIRONMENT=node','-sNODERAWFS=1','-sEXIT_RUNTIME=1','-sSTACK_SIZE=1048576','-sALLOW_MEMORY_GROWTH=1','-o',str(exe)],cwd=root,check=True)
subprocess.run(['node',str(exe),out.as_posix()],cwd=root,check=True)
