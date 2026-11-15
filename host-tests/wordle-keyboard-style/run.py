"""Render actual Wordle keyboards with real UI fonts, outside the device."""
from pathlib import Path
import argparse,importlib.util,json,os,re,subprocess
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=ROOT/'.cache/wordle-keyboard-style');p.add_argument('--compiler',default=os.environ.get('EMXX',str(ROOT/'.venv/emsdk/upstream/emscripten/em++.exe')));p.add_argument('--node',default=os.environ.get('NODE',str(ROOT/'.venv/emsdk/node/24.19.0_64bit/node.exe')));a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
spec=importlib.util.spec_from_file_location('i18n',ROOT/'scripts/gen_i18n.py');i18n=importlib.util.module_from_spec(spec);spec.loader.exec_module(i18n)
header=(ROOT/'lib/I18n/I18nKeys.h').read_text(encoding='utf8');keys=re.findall(r'^  (STR_\w+),',header,re.M);langs=re.findall(r'^  (\w+)\s*=\s*(\d+),\s*//\s*(\S+)',header,re.M)
catalogues={d['_bcp47']:d for path in (ROOT/'lib/I18n/translations').glob('*.yaml') for d in [i18n.parse_yaml_file(str(path))]}
(out/'locales.inc').write_text('static constexpr const char* localeCodes[] = {'+','.join(json.dumps(tag) for _,_,tag in langs)+'};',encoding='utf8')
for _,index,tag in langs:(out/(index+'.bin')).write_bytes(b''.join(str(catalogues[tag].get(k,catalogues['en'][k])).encode('utf8')+b'\0' for k in keys))
stubs=out/'stubs';stubs.mkdir(exist_ok=True)
s=(ROOT/'test/language_pack/stubs/HalStorage.h').read_text().replace('  bool seek(size_t pos) {','  size_t size() const {return fileSize64();}\n  bool seek(size_t pos) {').replace('  std::map<std::string, std::vector<uint8_t>> files;', '  std::map<std::string, std::vector<uint8_t>> files;\n  bool exists(const char* p)const{return files.count(p)!=0;}')
s=s.replace('  bool seek(size_t pos) {','  void close(){bytes=nullptr;directory=false;cursor=0;}\n  bool seekSet(size_t pos){return seek(pos);}\n  bool seek(size_t pos) {')
(stubs/'HalStorage.h').write_text(s)
(stubs/'Logging.h').write_text('#pragma once\n#define LOG_DBG(...) ((void)0)\n#define LOG_INF(...) ((void)0)\n#define LOG_ERR(...) ((void)0)\n')
compiler=Path(a.compiler);node=Path(a.node)
includes=[out,stubs,ROOT,ROOT/'host-tests/wordle-keyboard-style/stubs',ROOT/'host-tests/keyboard-layout-selector',ROOT/'host-tests/keyboard-language/stubs',ROOT/'host-tests/collections/stubs',ROOT/'host-tests/study-suite/stubs']
includes += [ROOT/n for n in ['freeink-sdk/libs/ui/FreeInkUI/include','freeink-sdk/libs/assets/Icons/include','lib/EpdFont','lib/I18n','lib/Utf8','lib/Memory','lib/MiniBidi']]
bidi=out/'minibidi.o';subprocess.run([str(compiler.with_name('emcc.exe' if compiler.suffix=='.exe' else 'emcc')),'-O1','-c','lib/MiniBidi/minibidi.c','-o',str(bidi)],cwd=ROOT,check=True)
sources=['host-tests/wordle-keyboard-style/render.cpp','src/activities/util/EmbeddedKeyboard.cpp','src/activities/util/KeyboardLayoutSet.cpp','src/activities/util/LanguageKeyboard.cpp','src/activities/util/PinyinInput.cpp','src/apps_local/ui/KeyboardAlternates.cpp','src/apps_local/ui/PixelMenu.cpp','lib/I18n/I18nStrings.cpp','lib/EpdFont/EpdFont.cpp','lib/EpdFont/EpdFontFamily.cpp','lib/Utf8/Utf8.cpp','lib/MiniBidi/BidiUtils.cpp','freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp',bidi]
exe=out/'render.cjs'
subprocess.run([str(compiler),'-std=c++20','-O1','-Wno-comment',*['-I'+str(n) for n in includes],*[str(n) for n in sources],'-sENVIRONMENT=node','-sNODERAWFS=1','-sEXIT_RUNTIME=1','-sSTACK_SIZE=1048576','-sALLOW_MEMORY_GROWTH=1','-o',str(exe)],cwd=ROOT,check=True)
subprocess.run([str(node),str(exe),ROOT.as_posix(),out.as_posix()],cwd=ROOT,check=True)
