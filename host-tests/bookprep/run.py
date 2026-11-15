from pathlib import Path
import argparse,os,shutil,subprocess
import re
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser()
p.add_argument("--output",type=Path,default=ROOT/".pio/host-tests/bookprep")
p.add_argument("--render",type=Path)
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
 subprocess.run([str(ROOT/".venv/Scripts/python.exe") if wasm else "python3","scripts/gen_i18n.py","lib/I18n/translations",str(i18n),"--builtin-langs","en,es"],cwd=ROOT,check=True,stdout=log,env={**os.environ,"PYTHONUTF8":"1","PYTHONIOENCODING":"utf-8"})
source=(ROOT/"lib/Epub/Epub/Section.cpp").read_text(encoding="utf-8")
parts=[]
for name in ("SECTION_FILE_VERSION","SECTION_FILE_INCOMPLETE_VERSION","SECTION_FILE_PARTIAL_VERSION","HEADER_SIZE"):
 parts.append(re.search(r"constexpr [^;]*\b"+name+r"\s*=[^;]*;",source).group())
for name in ("effectiveSpec","writeSectionFileHeader","loadSectionFile","loadReaderLayout","loadPreparedSectionFile","preparedLayoutPath","layoutSlotPath","loadLayoutCandidate","clearCache","getCachedPageCount"):
 start=source.rfind("\n",0,source.index("Section::"+name+"("))+1
 brace=source.index("{",start);end=brace+1;depth=1
 while depth:
  depth+=(source[end]=="{")-(source[end]=="}");end+=1
 parts.append(source[start:end])
(out/"section-under-test.inc").write_text("\n".join(parts),encoding="utf-8")
suites={
"section":(["host-tests/bookprep/test_section.cpp"],["-I"+str(out),"-Ihost-tests/bookprep/stubs","-I.","-Ilib/Memory","-Ilib/Epub"]),
"state":(["host-tests/bookprep/test.cpp","src/apps_local/bookprep/BookPreparation.cpp"],["-Ihost-tests/bookprep/stubs","-I.","-Ilib/Memory","-Ilib/Epub"]),
"screens":(["host-tests/bookprep/test_screens.cpp","src/apps_local/bookprep/BookPreparationScreens.cpp","src/apps_local/ui/PixelMenu.cpp",str(i18n/"I18n.cpp"),str(i18n/"I18nStrings.cpp"),str(i18n/"LanguagePack.cpp"),"freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp"],["-Itest/language_pack/stubs","-Ihost-tests/reading-cloud/stubs","-Ihost-tests/reading-suite/stubs","-I"+str(i18n),"-Ilib/Epub","-Ilib/EpdFont","-Ifreeink-sdk/libs/ui/FreeInkUI/include","-Ifreeink-sdk/libs/assets/Icons/include"])}
for name,(sources,includes) in suites.items():
 exe=out/(name+(".cjs" if wasm else ""))
 subprocess.run([compiler,*common,*includes,*flags,*sources,"-o",str(exe)],cwd=ROOT,check=True)
 args=[]
 if name=="screens" and a.render:
  a.render.mkdir(parents=True,exist_ok=True); args=[a.render.resolve().as_posix()]
 subprocess.run([*runtime,str(exe),*args],cwd=ROOT,check=True)
