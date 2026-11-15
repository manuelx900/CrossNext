from pathlib import Path
import os,subprocess,shutil
R=Path(__file__).resolve().parents[2]; Q=R/'qa-artifacts/epub-format-speed'
E={**os.environ,'PYTHONUTF8':'1','EM_CACHE':str(R/'.pio/em-cache-speed-all')}
C=str(R/'.venv/emsdk/upstream/emscripten/em++.exe')
def run(name,sources,includes):
 out=Q/(name+'.cjs')
 with (Q/(name+'.log')).open('w',encoding='utf8') as f:
  r=subprocess.run([C,'-std=c++20','-O1','-Wall','-Wextra','-Werror','-Wno-character-conversion','-Wno-sign-compare','-Wno-range-loop-construct','-sENVIRONMENT=node','-sEXIT_RUNTIME=1','-sALLOW_MEMORY_GROWTH=1',*['-I'+str(R/p) for p in includes],*sources,'-o',str(out)],cwd=R,env=E,stdout=f,stderr=subprocess.STDOUT)
  if not r.returncode:r=subprocess.run([shutil.which('node'),str(out)],cwd=R,env=E,stdout=f,stderr=subprocess.STDOUT)
 print(name,r.returncode,flush=True)
 if r.returncode: print((Q/(name+'.log')).read_text()[-6000:]);raise SystemExit(r.returncode)
run('deferred-page-writes',['host-tests/page-write-batching/test.cpp','lib/Epub/Epub/PageCacheWriter.cpp'],['host-tests/page-write-batching/stubs','lib/Epub/Epub'])

g=Path(os.environ['CROSSNEXT_GTEST'])
run('css-residency',['test/css_parser/CssParserTest.cpp','lib/Epub/Epub/css/CssParser.cpp',str(R/'.pio/host-tests/reader-io/gtest-all.o'),str(R/'.pio/host-tests/reader-io/gtest_main.o')],['test/css_parser/stubs','lib/Epub/Epub/css','lib/AsciiText','lib/Memory',str(g/'include')])

def block(text,signature):
 start=text.index(signature); brace=text.index('{',start);end=brace+1;depth=1
 while depth: depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
reader=(R/'src/activities/reader/EpubReaderActivity.cpp').read_text()
(Q/'format-reader-under-test.inc').write_text(block(reader,'EpubReaderActivity::FormatPreparation EpubReaderActivity::prepareFormatPage(')+'\n'+block(reader,'void EpubReaderActivity::applyReaderTextSettings(')+'\n'+block(reader,'void EpubReaderActivity::rememberReflowTarget('))
run('format-reader',['host-tests/epub-format/reader.cpp'],[str(Q),'lib/Epub','lib/Memory','lib/Performance'])

section=(R/'lib/Epub/Epub/Section.cpp').read_text()
(Q/'format-offset-under-test.inc').write_text(block(section,'std::optional<uint16_t> Section::getPageForVisibleTextOffset('))
run('format-offset',['host-tests/epub-format/offset.cpp'],[str(Q),'test/chapter_html_slim_parser/stubs','lib/Serialization'])

(Q/'format-lifetime-under-test.inc').write_text(block(section,'Section::~Section()')+'\n'+block(reader,'bool EpubReaderActivity::flushFormatProgress(')+'\n'+block(reader,'void EpubReaderActivity::prepareForSleep(')+'\n'+block(reader,'bool EpubReaderActivity::saveProgress(')+'\n'+block(reader,'bool EpubReaderActivity::saveFinishedPosition(')+'\n'+block(reader,'ChapterPosition EpubReaderActivity::chapterPosition(')+'\n'+block(reader,'int EpubReaderActivity::bookPercentFor(')+'\n'+block(reader,'ScreenshotInfo EpubReaderActivity::getScreenshotInfo(')+'\n'+block(reader,'int EpubReaderActivity::getProgressBasisPoints('))
assert 'flushFormatProgress();' in block(reader,'void EpubReaderActivity::onExit(')
run('format-lifetime',['host-tests/epub-format/lifetime.cpp'],[str(Q)])
