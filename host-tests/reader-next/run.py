from pathlib import Path
import os,subprocess,shutil,sys
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'.pio/host-tests/reader-next';OUT.mkdir(parents=True,exist_ok=True)
CXX=str(ROOT/'.venv/emsdk/upstream/emscripten/em++.exe') if os.name=='nt' else os.environ.get('CXX','c++')
ENV={**os.environ,'PYTHONUTF8':'1','EM_CACHE':str(ROOT/'.pio/em-cache-next')}
FLAGS=['-sENVIRONMENT=node','-sEXIT_RUNTIME=1','-sALLOW_MEMORY_GROWTH=1'] if os.name=='nt' else []
def run(name,sources,includes,args=()):
 if '--only' in sys.argv and name!=sys.argv[sys.argv.index('--only')+1]:return
 exe=OUT/(name+('.cjs' if os.name=='nt' else ''))
 cmd=[CXX,'-std=c++20','-O1','-Wall','-Wextra','-Wno-unused-parameter',*FLAGS,*['-I'+str(ROOT/p) for p in includes],*sources,'-o',str(exe)]
 subprocess.run(cmd,cwd=ROOT,env=ENV,check=True)
 subprocess.run(([shutil.which('node') or 'node'] if os.name=='nt' else [])+[str(exe),*args],cwd=ROOT,env=ENV,check=True)
run('hal-policy',['host-tests/reader-next/hal.cpp','lib/hal/HalStorage.cpp'],['host-tests/reader-next/hal-stubs','lib/hal','test/chapter_html_slim_parser/stubs','lib/Memory','lib/Performance'])
run('cover-scenes',['host-tests/cover-scene/test.cpp','src/apps_local/home/CoverSceneCache.cpp','src/apps_local/library/ScenePack.cpp'],['lib/Performance','host-tests/cover-scene/stubs','host-tests/epub-event-cache/stubs','freeink-sdk/libs/ui/FreeInkUI/include'])
run('cover-scenes-oom',['host-tests/cover-scene/test.cpp','src/apps_local/home/CoverSceneCache.cpp','src/apps_local/library/ScenePack.cpp'],['lib/Performance','host-tests/cover-scene/stubs','host-tests/epub-event-cache/stubs','freeink-sdk/libs/ui/FreeInkUI/include'],['oom'])

section=(ROOT/'lib/Epub/Epub/Section.cpp').read_text(encoding='utf8')
start=section.index('bool Section::buildSomeMore(');brace=section.index('{',start);end=brace+1;depth=1
while depth:
 depth+=(section[end]=='{')-(section[end]=='}');end+=1
(OUT/'slice-under-test.inc').write_text(section[start:end],encoding='utf8')
run('build-slices',['host-tests/reader-next/slice.cpp'],[str(OUT),'lib/Epub','lib/Performance'])

run('scene-packs',['host-tests/reader-next/packs.cpp','src/apps_local/home/CoverSceneCache.cpp','src/apps_local/library/ScenePack.cpp'],['lib/Performance','host-tests/cover-scene/stubs','host-tests/epub-event-cache/stubs','freeink-sdk/libs/ui/FreeInkUI/include'])

def body(source,signature):
 start=source.index(signature);brace=source.index('{',start);end=brace+1;depth=1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]
reader=(ROOT/'src/activities/reader/EpubReaderActivity.cpp').read_text(encoding='utf8')
manager=(ROOT/'src/activities/ActivityManager.cpp').read_text(encoding='utf8')
(OUT/'lock-owner-under-test.inc').write_text(body(manager,'bool RenderLock::heldByCurrentTask()'),encoding='utf8')
(OUT/'page-release-under-test.inc').write_text(body(reader,'void EpubReaderActivity::releasePageReuse()'),encoding='utf8')
clip=body(reader,'void EpubReaderActivity::startClipSelection(')
(OUT/'clip-prepare-under-test.inc').write_text(clip[:clip.index('  if (buildViewportWidth == 0')]+ '\n  preparedPages = pages.size();\n}\n',encoding='utf8')
footnote=body(reader,'void EpubReaderActivity::openFootnoteSelect(')
(OUT/'footnote-prepare-under-test.inc').write_text(footnote[:footnote.index('  auto selector =')]+ '\n  preparedPages = page ? 1 : 0;\n}\n',encoding='utf8')
(OUT/'href-under-test.inc').write_text(body(reader,'void EpubReaderActivity::navigateToHref('),encoding='utf8')
run('page-lifecycle',['host-tests/reader-next/lifecycle.cpp'],[str(OUT)])
start=reader.index('void EpubReaderActivity::renderBook()');brace=reader.index('{',start)
end=reader.index('  // Runs under the render task',brace)
(OUT/'reader-prefix-under-test.inc').write_text(reader[brace+1:end],encoding='utf8')
start=reader.index('  {\n    const unsigned long loadStart = millis();',start)
# Locate the chosen block directly; the same indentation occurs earlier in the source.
brace=reader.index('{',start);end=brace+1;depth=1
while depth:
 depth+=(reader[end]=='{')-(reader[end]=='}');end+=1
(OUT/'reader-load-under-test.inc').write_text(reader[start:end],encoding='utf8')
(OUT/'reader-key-under-test.inc').write_text(body(reader,'readerpage::Key EpubReaderActivity::pageReuseKey()').replace('EpubReaderActivity::','ReuseWiringReader::'),encoding='utf8')
if '--only' not in sys.argv or sys.argv[sys.argv.index('--only')+1]=='reader-wiring':
 cc=str(ROOT/'.venv/emsdk/upstream/emscripten/emcc.exe') if os.name=='nt' else os.environ.get('CC','cc')
 obj=OUT/'minibidi.o';subprocess.run([cc,'-O1','-c','lib/MiniBidi/minibidi.c','-o',str(obj)],cwd=ROOT,env=ENV,check=True)
 run('reader-wiring',['host-tests/reader-next/wiring.cpp','test/chapter_html_slim_parser/ParserLinkStubs.cpp','lib/Epub/Epub/Page.cpp','lib/Epub/Epub/blocks/TextBlock.cpp','lib/Epub/Epub/CjkBreaks.cpp','lib/MiniBidi/BidiUtils.cpp','lib/Utf8/Utf8.cpp',str(obj)],
 [str(OUT),'test/chapter_html_slim_parser/stubs','src','lib','lib/Epub','lib/Epub/Epub','lib/Epub/Epub/parsers','lib/Epub/Epub/css','lib/AsciiText','lib/EpdFont','lib/Utf8','lib/Memory','lib/Serialization','lib/MiniBidi','lib/FsHelpers','lib/Performance'])
