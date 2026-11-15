"""Run production selector methods against SDK lists and actual interface fonts."""
from pathlib import Path
import argparse,os,subprocess
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=ROOT/'qa-artifacts/keyboard-selector-2026-11-14');p.add_argument('--compiler',default=os.environ.get('EMXX',str(ROOT/'.venv/emsdk/upstream/emscripten/em++.exe')));p.add_argument('--node',default=os.environ.get('NODE',str(ROOT/'.venv/emsdk/node/24.19.0_64bit/node.exe')));a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
import importlib.util,re,json
spec=importlib.util.spec_from_file_location('selector_i18n',ROOT/'scripts/gen_i18n.py');i18n=importlib.util.module_from_spec(spec);spec.loader.exec_module(i18n)
header=(ROOT/'lib/I18n/I18nKeys.h').read_text(encoding='utf8');keys=re.findall(r'^  (STR_\w+),',header,re.M);langs=re.findall(r'^  (\w+)\s*=\s*(\d+),\s*//\s*(\S+)',header,re.M)
assert len(langs)==39 and keys
catalogues={d['_bcp47']:d for path in (ROOT/'lib/I18n/translations').glob('*.yaml') for d in [i18n.parse_yaml_file(str(path))]}
fixtures=out/'fixtures';fixtures.mkdir(exist_ok=True)
(out/'locales.inc').write_text('static constexpr const char* localeCodes[] = {'+','.join(json.dumps(tag) for _,_,tag in langs)+'};',encoding='utf8')
for _,index,tag in langs:(fixtures/(index+'.bin')).write_bytes(b''.join(str(catalogues[tag].get(k,catalogues['en'][k])).encode('utf8')+b'\0' for k in keys))
source=(ROOT/'src/activities/settings/KeyboardLayoutsActivity.cpp').read_text(encoding='utf8')
starts=['void KeyboardLayoutsActivity::onEnter()','void KeyboardLayoutsActivity::onExit()','const char* KeyboardLayoutsActivity::headerTitle()','bool KeyboardLayoutsActivity::checked(','bool KeyboardLayoutsActivity::isLocked(','void KeyboardLayoutsActivity::activateIndex(','bool KeyboardLayoutsActivity::handleCustomInput()','bool KeyboardLayoutsActivity::isAvailable(','void KeyboardLayoutsActivity::provideRow(','void KeyboardLayoutsActivity::buildScreen(']
methods=[]
for name in starts:
 start=source.index(name);end=source.index('\n}',start)+2 if name!='const char* KeyboardLayoutsActivity::headerTitle()' else source.index('\n',start)
 methods.append(source[start:end])
program=(ROOT/'host-tests/keyboard-layout-selector/selector.cpp').read_text(encoding='utf8').replace('PRODUCTION_METHODS','\n'.join(methods))
fontSource=(ROOT/'src/components/LanguageUiFonts.cpp').read_text();start=fontSource.index('bool LanguageUiFonts::supportsText');support=fontSource[start:fontSource.index('\n}',start)+2];constants=fontSource[fontSource.index('namespace {'):fontSource.index('}  // namespace')+len('}  // namespace')];program=program.replace('PRODUCTION_FONT_SUPPORT',constants+'\n'+support)
(out/'selector.cpp').write_text(program,encoding='utf8')
stubs=out/'selector-stubs';stubs.mkdir(exist_ok=True)
s=(ROOT/'host-tests/keyboard-language/stubs/CrossPointSettings.h').read_text().replace('  uint64_t keyboardLanguageLayouts = 0;','  uint64_t keyboardLanguageLayouts = 0;\n  int saves=0; bool saveToFile(){++saves;return true;}')
(stubs/'CrossPointSettings.h').write_text(s)
ss=(ROOT/'test/language_pack/stubs/HalStorage.h').read_text().replace('  bool seek(size_t pos) {','  size_t size() const {return fileSize64();}\n  bool seek(size_t pos) {').replace('  std::map<std::string, std::vector<uint8_t>> files;', '  std::map<std::string, std::vector<uint8_t>> files;\n  bool exists(const char* p)const{return files.count(p)!=0;}')
(stubs/'HalStorage.h').write_text(ss)
(stubs/'Logging.h').write_text('#pragma once\n#define LOG_DBG(...) ((void)0)\n#define LOG_INF(...) ((void)0)\n#define LOG_ERR(...) ((void)0)\n')

compiler=Path(a.compiler);node=Path(a.node)
includes=[out,stubs,ROOT/'host-tests/keyboard-layout-selector',ROOT,ROOT/'test/language_pack/stubs',ROOT/'host-tests/sdart/stubs',ROOT/'host-tests/study-suite/stubs']
includes += [ROOT/n for n in ['freeink-sdk/libs/ui/FreeInkUI/include','freeink-sdk/libs/assets/Icons/include','lib/EpdFont','lib/I18n','lib/Utf8','lib/Memory','lib/MiniBidi']]
bidi=out/'minibidi.o';subprocess.run([str(compiler.with_name('emcc.exe' if compiler.suffix=='.exe' else 'emcc')),'-O1','-c','lib/MiniBidi/minibidi.c','-o',str(bidi)],cwd=ROOT,check=True)
exe=out/'selector.cjs'
sources=[out/'selector.cpp','src/activities/util/KeyboardLayoutSet.cpp','src/apps_local/ui/PixelMenu.cpp','lib/I18n/I18nStrings.cpp','lib/EpdFont/EpdFont.cpp','lib/EpdFont/EpdFontFamily.cpp','lib/Utf8/Utf8.cpp','lib/MiniBidi/BidiUtils.cpp','freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp',bidi]
subprocess.run([str(compiler),'-std=c++20','-O1','-Wno-comment',*['-I'+str(n) for n in includes],*[str(n) for n in sources],'-sENVIRONMENT=node','-sNODERAWFS=1','-sEXIT_RUNTIME=1','-sSTACK_SIZE=1048576','-sALLOW_MEMORY_GROWTH=1','-o',str(exe)],cwd=ROOT,check=True)
subprocess.run([str(node),str(exe),ROOT.as_posix(),out.as_posix(),"0"],cwd=ROOT,check=True)
