"""Compile the real Unicode game against the existing SD fault-injection stub."""
from pathlib import Path
import json,os,subprocess,sys
import unicodedata
import regex
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'datos.local/language-ui-input/native-tests';OUT.mkdir(parents=True,exist_ok=True)
codes=[]
for path in sorted((ROOT/'lib/I18n/translations').glob('*.yaml')):
    codes.append(next(json.loads(x.split(':',1)[1]) for x in path.read_text(encoding='utf-8').splitlines() if x.startswith('_language_code:')))
(OUT/'I18n.h').write_text('#pragma once\nenum class Language { '+','.join(codes)+',_COUNT };\ninline constexpr const char* LANGUAGE_CODES[] = {'+','.join(json.dumps(c) for c in codes)+'};\n',encoding='utf-8')
sys.path.insert(0,str(ROOT/'tools_local'))
from build_wordle_languages import catalogue, lower
tags=dict(catalogue())
fixtures=json.loads((ROOT/'test/language_pack/common_wordle_words.json').read_text(encoding='utf-8'))
examples=[]
for tag,words in fixtures.items():
    for word in words:
        forms={word,unicodedata.normalize('NFD',word)}
        if lower(word.upper(),tag)==lower(word,tag): forms.add(word.upper())
        for form in sorted(forms):
            examples.append('{Language::'+tags[tag]+','+str(len(regex.findall(r'\X',lower(word,tag))))+',u8'+json.dumps(form,ensure_ascii=False)+'}')
(OUT/'WordleCoverage.h').write_text('struct WordleExample { Language language; unsigned letters; const char* word; };\ninline constexpr WordleExample COMMON_WORDS[] = {\n'+',\n'.join(examples)+'\n};\n',encoding='utf-8')
wasm=os.name=='nt'
compiler=str(ROOT/'.venv/emsdk/upstream/emscripten/em++.exe') if wasm else os.environ.get('CXX','c++')
output=OUT/('native.cjs' if wasm else 'native')
args=[compiler,'-std=c++17','-Wall','-Wextra','-Werror','-Wno-comment','-I'+str(OUT),'-Ihost-tests/study-suite/stubs','-Ilib/Utf8',
      'host-tests/nativewordle/test_native.cpp','src/apps_local/wordle/NativeWordle.cpp','lib/Utf8/Utf8.cpp','-o',str(output)]
if wasm: args+=['-sENVIRONMENT=node','-sNODERAWFS=1','-sALLOW_MEMORY_GROWTH=1','-sEXIT_RUNTIME=1']
subprocess.run(args,cwd=ROOT,check=True)
subprocess.run(([str(ROOT/'.venv/emsdk/node/24.19.0_64bit/node.exe')] if wasm else [])+[str(output)],cwd=ROOT,check=True)

# The chooser is the production input route for mobile accents and kana groups.
(OUT/'I18n.h').write_text((OUT/'I18n.h').read_text(encoding='utf-8')+'\ninline constexpr unsigned getLanguageCount(){return unsigned(Language::_COUNT); }\nstruct TestI18n { Language language=Language::EN; void setLanguage(Language x){language=x;} Language getLanguage() const{return language;} }; inline TestI18n testI18n;\n#define I18N testI18n\n',encoding='utf-8')
(OUT/'MappedInputManager.h').write_text('#pragma once\nclass MappedInputManager {public: enum class Button {Back,NavNext,NavPrevious,Confirm}; bool wasReleased(Button)const{return false;} bool wasBackGesture()const{return false;} bool wasScreenTouchDown(int&,int&)const{return false;} bool isScreenTouchHeld(int&,int&)const{return false;} bool wasScreenTouchReleased()const{return false;} bool wasScreenTapped(int&,int&)const{return false;}};\n',encoding='utf-8')

# Exercise the actual descriptor loader and candidate pagination.
(OUT/'I18n.h').write_text((OUT/'I18n.h').read_text(encoding='utf-8')+'\nenum StrId { STR_KEY_PINYIN };\n#define tr(x) "Pinyin"\n',encoding='utf-8')
(OUT/'HalMemory.h').write_text('#pragma once\n#include <memory>\n#include <cstdlib>\nclass HalMemory { public: struct PsramDeleter { void operator()(uint8_t* p) const { std::free(p); } }; using PsramBuffer=std::unique_ptr<uint8_t[],PsramDeleter>; static PsramBuffer allocatePsram(size_t n) { return PsramBuffer(static_cast<uint8_t*>(std::malloc(n))); } };\n',encoding='utf-8')
keyout=OUT/('keyboards.cjs' if wasm else 'keyboards')
args=[compiler,'-std=c++20','-fno-char8_t','-Wall','-Wextra','-Werror','-Wno-comment','-Wno-missing-field-initializers', '-I'+str(OUT),'-Ihost-tests/study-suite/stubs','-Ilib/Utf8','-Ifreeink-sdk/libs/ui/FreeInkUI/include',
'host-tests/nativewordle/test_keyboards.cpp','src/apps_local/ui/KeyboardAlternates.cpp','src/activities/util/LanguageKeyboard.cpp','src/activities/util/PinyinInput.cpp','lib/Utf8/Utf8.cpp','freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp','-o',str(keyout)]
if wasm: args+=['-sENVIRONMENT=node','-sNODERAWFS=1','-sALLOW_MEMORY_GROWTH=1','-sEXIT_RUNTIME=1']
subprocess.run(args,cwd=ROOT,check=True)
subprocess.run(([str(ROOT/'.venv/emsdk/node/24.19.0_64bit/node.exe')] if wasm else [])+[str(keyout)],cwd=ROOT,check=True)


layoutout=OUT/('layouts.cjs' if wasm else 'layouts')
args=[compiler,'-std=c++20','-Wall','-Wextra','-Werror','-Wno-comment','-Wno-missing-field-initializers', '-I'+str(OUT),'-Ihost-tests/keyboard-language/stubs','-Ihost-tests/study-suite/stubs','-Ifreeink-sdk/libs/ui/FreeInkUI/include',
'host-tests/nativewordle/test_layouts.cpp','src/activities/util/KeyboardLayoutSet.cpp','-o',str(layoutout)]
if wasm: args+=['-sENVIRONMENT=node','-sNODERAWFS=1','-sALLOW_MEMORY_GROWTH=1','-sEXIT_RUNTIME=1']
subprocess.run(args,cwd=ROOT,check=True)
subprocess.run(([str(ROOT/'.venv/emsdk/node/24.19.0_64bit/node.exe')] if wasm else [])+[str(layoutout)],cwd=ROOT,check=True)
