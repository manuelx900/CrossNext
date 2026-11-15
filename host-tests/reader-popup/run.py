"""Exercise the production snapshot and popup repaint methods with fault injection."""
from pathlib import Path
import os,re,subprocess
r=Path(__file__).resolve().parents[2]; d=r/'datos.local/language-wordle-polish/popup-test';d.mkdir(exist_ok=True)
parts=[]
for file,names in [('lib/GfxRenderer/GfxRenderer.cpp',['bool GfxRenderer::copyStoredBwBuffer(', 'void GfxRenderer::restoreBwBuffer(']),('src/activities/reader/EpubReaderActivity.cpp',['void EpubReaderActivity::paintOverlayPopup('])]:
 text=(r/file).read_text(encoding='utf-8')
 for name in names:
  start=text.index(name);brace=text.index('{',start);end=brace+1;depth=1
  while depth:
   depth+=(text[end]=='{')-(text[end]=='}');end+=1
  parts.append(text[start:end])
(d/'production.inc').write_text('\n'.join(parts),encoding='utf-8')
env=os.environ.copy();em=r/'.venv/emsdk/upstream/emscripten';env['PATH']=str(em)+os.pathsep+env.get('PATH','')
subprocess.run([str(em/('em++.exe' if os.name=='nt' else 'em++')),'-std=c++20','-O1','-Wall','-Wextra','-Werror','-Wno-unused-variable','-Wno-comment','-I'+str(d),str(r/'host-tests/reader-popup/test.cpp'),'-sENVIRONMENT=node','-sEXIT_RUNTIME=1','-o',str(d/'test.cjs')],cwd=r,env=env,check=True)
subprocess.run(['node',str(d/'test.cjs')],cwd=r,check=True)
