from pathlib import Path
import os,subprocess,sys
from PIL import Image
root=Path(__file__).resolve().parents[2];qa=Path(os.environ.get('CROSSNEXT_LIBRARY_QA',str(root/'qa-artifacts/library-entry-placeholders-2026-11-15')));out=qa/'entry-host';out.mkdir(parents=True,exist_ok=True)
sdk=(root/'.venv/emsdk').resolve();env={**os.environ,'PYTHONUTF8':'1','PYTHONIOENCODING':'utf-8','EM_CACHE':str(sdk.parent.parent/'.pio/em-cache-keyboard-audit')}
source=(root/'src/components/themes/BaseTheme.cpp').read_text('utf8');start=source.index('void BaseTheme::drawCoverPlaceholder(');end=source.index('\n}',start)+2
(out/'default-placeholder.inc').write_text(source[start:end].replace('BaseTheme::','FakeTheme::').replace('{','{ ++renderer.placeholders;',1),'utf8')
# Reuse the production grid contract and its exact pre-change final-pixel comparison.
sys.path.insert(0,str(root/'host-tests/library-first-frame'))
activity=(root/'src/activities/library/LibraryListActivity.cpp').read_text('utf8')
def method(text,signature):
 start=text.index(signature);brace=text.index('{',start);depth=1;end=brace+1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
assert 'drawLoadingScreen' not in method(activity,'void LibraryListActivity::initializeLibrary(')
methods=[method(activity,sig)for sig in ['int LibraryListActivity::entryBookCount(', 'void LibraryListActivity::buildGrid(','void LibraryListActivity::render(','void LibraryListActivity::runIdleWork(','bool LibraryListActivity::presentationComplete(']]
before_path=Path(os.environ.get('CROSSNEXT_LIBRARY_BASELINE',str(qa/'before/src/activities/library/LibraryListActivity.cpp')))
baseline_flags=[]
if before_path.is_file():
 before=before_path.read_text('utf8')
 methods.append(method(before,'void LibraryListActivity::buildGrid(').replace('LibraryListActivity::buildGrid(','LibraryListActivity::baselineBuildGrid(',1))
 baseline_flags=['-DHAS_LIBRARY_BASELINE=1']
(out/'activity-methods.inc').write_text('\n'.join(methods),'utf8')
variants=[0]
includes=['host-tests/library-entry/stubs','test/language_pack/stubs','host-tests/reading-cloud/stubs','host-tests/reading-suite/stubs','lib/I18n','lib/Epub','lib/EpdFont','freeink-sdk/libs/ui/FreeInkUI/include','freeink-sdk/libs/assets/Icons/include','src','.',str(out)]
sources=['host-tests/library-entry/screens.cpp','src/apps_local/library/LibraryEntryBackdrop.cpp','src/apps_local/ui/PixelMenu.cpp','lib/I18n/I18n.cpp','lib/I18n/I18nStrings.cpp','lib/I18n/LanguagePack.cpp','freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp']
for variant in variants:
 exe=out/f'entry-{variant}.cjs';shots=qa/f'entry-variant-{variant}';shots.mkdir(exist_ok=True)
 cmd=[str(sdk/'upstream/emscripten/em++.exe'),'-std=c++20','-O1','-Wall','-Wextra','-Werror','-Wno-comment','-Wno-missing-field-initializers',*baseline_flags,*['-I'+p for p in includes],*sources,'-sSTACK_SIZE=1048576','-sENVIRONMENT=node','-sNODERAWFS=1','-sEXIT_RUNTIME=1','-sALLOW_MEMORY_GROWTH=1','-o',str(exe)]
 with (qa/f'entry-build-{variant}.log').open('w',encoding='utf8')as log:subprocess.run(cmd,cwd=root,env=env,stdout=log,stderr=log,check=True)
 with (qa/f'entry-run-{variant}.log').open('w',encoding='utf8')as log:subprocess.run(['node',str(exe),str(shots)],cwd=root,env=env,stdout=log,stderr=log,check=True)
 for file in shots.glob('*.pbm'):Image.open(file).convert('RGB').save(file.with_suffix('.png'))
 print('Entry variant',variant,'passed',flush=True)
