from pathlib import Path
import argparse, os, shutil, subprocess
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(); a=p.parse_args()
out=root/'datos.local/reader-folders-2026-11-12/host'; out.mkdir(parents=True,exist_ok=True)
wasm=os.name=='nt'; compiler=str(root/'.venv/emsdk/upstream/emscripten/em++.exe') if wasm else 'c++'
flags=['-sENVIRONMENT=node','-sEXIT_RUNTIME=1','-sSTACK_SIZE=1048576','-sNODERAWFS=1'] if wasm else []
base=['-std=c++20','-O1','-Wall','-Wextra','-Werror','-Wno-comment']
runtime=['node'] if wasm else []
i18n=out/'i18n';i18n.mkdir(exist_ok=True)
for f in ('I18n.cpp','I18n.h','LanguagePack.cpp','LanguagePack.h'):shutil.copy2(root/'lib/I18n'/f,i18n/f)
subprocess.run([str(root/'.venv/Scripts/python.exe') if wasm else 'python3','scripts/gen_i18n.py','lib/I18n/translations',str(i18n),'--builtin-langs','en,es'],cwd=root,check=True,stdout=subprocess.DEVNULL)
inc=['-Itest/language_pack/stubs','-I'+str(i18n),'-Ilib/EpdFont','-Ifreeink-sdk/libs/ui/FreeInkUI/include','-Ifreeink-sdk/libs/assets/Icons/include']
src=['test/reader_font_sheet/ReaderFontSheetTest.cpp','src/apps_local/ui/PixelMenu.cpp',str(i18n/'LanguagePack.cpp'),str(i18n/'I18n.cpp'),str(i18n/'I18nStrings.cpp'),'freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp']
for v in [1]:
 proof=out/('variant'+str(v));proof.mkdir(exist_ok=True);exe=out/('reader'+str(v)+('.cjs' if wasm else ''))
 subprocess.run([compiler,*base,*flags,*inc,*src,'-o',str(exe)],cwd=root,check=True)
 subprocess.run([*runtime,str(exe),str(proof)],cwd=root,check=True)
exe=out/('folders.cjs' if wasm else 'folders')
subprocess.run([compiler,*base,*flags,'-Ihost-tests/book-dictionaries/stubs','-Ihost-tests/study-suite/stubs','-Itest/sd_card_font/stubs','-Ilib/FsHelpers','host-tests/folder-visibility/test.cpp','-o',str(exe)],cwd=root,check=True)
subprocess.run([*runtime,str(exe)],cwd=root,check=True)
