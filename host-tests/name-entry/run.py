"""Exercise the real embedded naming surface and its input owner on the host."""
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / ".cache/name-entry-tests"
OUT.mkdir(parents=True, exist_ok=True)
codes = []
for path in sorted((ROOT / "lib/I18n/translations").glob("*.yaml")):
    codes.append(next(json.loads(line.split(":", 1)[1]) for line in path.read_text(encoding="utf-8").splitlines()
                      if line.startswith("_language_code:")))
(OUT / "I18n.h").write_text(
    '#pragma once\nenum class Language {' + ','.join(codes) + ',_COUNT};\n'
    'inline constexpr unsigned getLanguageCount(){return unsigned(Language::_COUNT); }\n'
    'inline constexpr const char* LANGUAGE_CODES[]={' + ','.join(json.dumps(c) for c in codes) + '};\n'
    'enum StrId { STR_OK_BUTTON, STR_PET_NAME_SPACE, STR_KEY_PINYIN, STR_WORDLE_OK, STR_KEY_SPACE, STR_KEY_RETURN, STR_KEY_LETTERS, STR_KEY_NUMBERS, STR_WORDLE_UNKNOWN, STR_WORDLE_SAVE_FAILED, STR_KEY_HALF_SPACE };\n'
    'inline const char* tr(StrId id){return id==STR_OK_BUTTON?"OK":id==STR_PET_NAME_SPACE||id==STR_KEY_SPACE?"Space":id==STR_KEY_NUMBERS?"123":id==STR_KEY_LETTERS?"ABC":id==STR_KEY_RETURN?"Enter":id==STR_KEY_HALF_SPACE?"Half-space":"Pinyin";}\n'
    'struct TestI18n { Language language=Language::EN; Language getLanguage() const{return language;} };\n'
    'inline TestI18n testI18n;\n#define I18N testI18n\n', encoding="utf-8")
(OUT / "HalMemory.h").write_text(
    '#pragma once\n#include <memory>\n#include <cstdlib>\n'
    'class HalMemory { public: struct Free {void operator()(uint8_t* p)const{std::free(p);}}; '
    'using PsramBuffer=std::unique_ptr<uint8_t[],Free>; '
    'static PsramBuffer allocatePsram(size_t n){return PsramBuffer(static_cast<uint8_t*>(std::malloc(n)));}};\n',
    encoding="utf-8")
(OUT / "MappedInputManager.h").write_text(
    '#pragma once\n#include <cstdint>\ninline uint32_t millis(){static uint32_t n=0; return ++n;}\n'
    'class MappedInputManager { public: enum class Button {Back,NavNext,NavPrevious,Confirm}; '
    'bool tap=false,confirm=false,held=false,ended=false,back=false,backGesture=false; int x=0,y=0,liveX=-1,liveY=-1; '
    'bool wasReleased(Button b)const{return (b==Button::Confirm&&confirm)||(b==Button::Back&&back);} '
    'bool wasScreenTapped(int& a,int& b)const{a=x;b=y;return tap;} '
    'bool wasBackGesture()const{return backGesture;} bool wasScreenTouchDown(int&,int&)const{return false;} '
    'bool isScreenTouchHeld(int& a,int& b)const{a=liveX>=0?liveX:x;b=liveY>=0?liveY:y;return held;} bool wasScreenTouchReleased()const{return ended;} };\n', encoding="utf-8")
wordle=(ROOT / "src/apps_local/wordle/WordleActivity.cpp").read_text(encoding="utf-8")
start=wordle.index("void WordleActivity::loop() {")
end=wordle.index("  int x = 0, y = 0;",start)
owner=(ROOT/"host-tests/name-entry/WordleOwnerTest.h").read_text(encoding="utf-8")
(OUT/"WordleOwnerTest.generated.h").write_text(owner.replace("PRODUCTION_WORDLE_LOOP",wordle[start:end]+"}\n"),encoding="utf-8")
compiler = str(ROOT / ".venv/emsdk/upstream/emscripten/em++.exe") if os.name == "nt" else os.environ.get("CXX", "c++")
output = OUT / ("tests.cjs" if os.name == "nt" else "tests")
args = [compiler, "-std=c++20", "-fno-char8_t", "-Wall", "-Wextra", "-Werror", "-Wno-comment",
        "-Wno-missing-field-initializers", *["-I" + str(p) for p in [OUT, ROOT / "host-tests/keyboard-language/stubs",
        ROOT / "host-tests/study-suite/stubs", ROOT / "lib/Utf8", ROOT / "freeink-sdk/libs/ui/FreeInkUI/include",
        ROOT / "freeink-sdk/libs/assets/Icons/include"]],
        "host-tests/name-entry/test.cpp", "src/activities/util/EmbeddedKeyboard.cpp",
        "src/apps_local/ui/KeyboardAlternates.cpp", "src/activities/util/LanguageKeyboard.cpp", "src/activities/util/PinyinInput.cpp",
        "src/activities/util/KeyboardLayoutSet.cpp", "src/apps_local/ui/PixelMenu.cpp", "lib/Utf8/Utf8.cpp",
        "freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp", "-o", str(output)]
if os.name == "nt":
    args += ["-sENVIRONMENT=node", "-sNODERAWFS=1", "-sALLOW_MEMORY_GROWTH=1", "-sEXIT_RUNTIME=1"]
subprocess.run(args, cwd=ROOT, check=True)
runtime = [str(ROOT / ".venv/emsdk/node/24.19.0_64bit/node.exe")] if os.name == "nt" else []
subprocess.run([*runtime, str(output)], cwd=ROOT, check=True)

# Actual PET and Quest codecs must preserve the newly accepted name character.
profile=OUT / ("profiles.cjs" if os.name=="nt" else "profiles")
profile_args=[compiler,"-std=c++20","-Wall","-Wextra","-Werror","-Wno-comment","-Ihost-tests/study-suite/stubs","-Ilib/Utf8","host-tests/name-entry/NamePersistenceTest.cpp","src/apps_local/pet/PetState.cpp","src/apps_local/quest/QuestCore.cpp","src/apps_local/quest/QuestCampaign.cpp","src/apps_local/reading/ReadingStatsCore.cpp","-o",str(profile)]
if os.name=="nt":profile_args += ["-sENVIRONMENT=node","-sEXIT_RUNTIME=1","-sSTACK_SIZE=1048576","-sALLOW_MEMORY_GROWTH=1"]
subprocess.run(profile_args,cwd=ROOT,check=True)
subprocess.run([*runtime,str(profile)],cwd=ROOT,check=True)
