from pathlib import Path
import os, subprocess, shutil, sys
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'.pio/host-tests/home-epub';OUT.mkdir(parents=True,exist_ok=True)
CXX=str((ROOT/'.venv/emsdk/upstream/emscripten/em++.exe').resolve()) if os.name=='nt' else os.environ.get('CXX','c++')
ENV={**os.environ,'PYTHONUTF8':'1','EM_CACHE':os.environ.get('EM_CACHE',str(ROOT/'.pio/em-cache-next'))}
FLAGS=['-sENVIRONMENT=node','-sEXIT_RUNTIME=1','-sALLOW_MEMORY_GROWTH=1','-sSTACK_SIZE=1048576'] if os.name=='nt' else []
def run(name,sources,includes,args=()):
    if '--only' in sys.argv and name!=sys.argv[sys.argv.index('--only')+1]: return
    exe=OUT/(name+('.cjs' if os.name=='nt' else ''))
    subprocess.run([CXX,'-std=c++20','-O1','-Wall','-Wextra','-Werror','-Wno-comment',*FLAGS,*['-I'+str(ROOT/p) for p in includes],*sources,'-o',str(exe)],cwd=ROOT,env=ENV,check=True)
    subprocess.run(([shutil.which('node') or 'node'] if os.name=='nt' else [])+[str(exe),*args],cwd=ROOT,env=ENV,check=True)
includes=['host-tests/reading-cloud/stubs','host-tests/reading-suite/stubs','lib/Memory','src']
run('memo',['host-tests/home-epub/memo.cpp','src/util/ReadingTimeMemo.cpp'],includes)
run('memo-oom',['host-tests/home-epub/memo.cpp','src/util/ReadingTimeMemo.cpp'],includes,['oom'])
core=['src/apps_local/reading/ReadingStatsCore.cpp','src/apps_local/reading/ReadingStatsStore.cpp','src/util/ReadingTimeMemo.cpp','src/activities/reader/ReaderSession.cpp']
run('stats-core',['host-tests/reading-suite/test_core.cpp',*core,'src/apps_local/home/HomeMenuCore.cpp','src/apps_local/home/HomeMenuStore.cpp','src/apps_local/game2048/Game2048Core.cpp','src/apps_local/game2048/Game2048Codec.cpp','src/apps_local/game2048/Game2048Store.cpp'],includes)
run('stats-completion',['host-tests/reading-suite/test_completion.cpp',*core],includes)

def block(source, signature):
    start=source.index(signature);brace=source.index('{',start);end=brace+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
source=(ROOT/'src/components/CoverGridHomeUi.cpp').read_text(encoding='utf8')
home=(ROOT/'src/activities/home/HomeActivity.cpp').read_text(encoding='utf8')
shell=home[home.index('void HomeActivity::render('):home.index('  libraryWarmup.cancel();',home.index('void HomeActivity::render('))]+'}\n'
(OUT/'home-shell.inc').write_text(shell,encoding='utf8')
notice=(ROOT/'src/apps_local/home/CoverLoadingNotice.h').read_text(encoding='utf8')
(OUT/'home-notice-area.inc').write_text(notice[notice.index('namespace homeui'):].replace('freeink::ui::Rect','Rect'),encoding='utf8')
init=home[home.index('  if (SETTINGS.homeCoverNotice(SETTINGS.homeLayout))',home.index('shelf::autostartFromEnv')):home.index('\nvoid HomeActivity::initializeContent()')].rsplit('}',1)[0]
(OUT/'home-init.inc').write_text(init,encoding='utf8')
run('loading-shell',['host-tests/home-epub/shell.cpp'],[str(OUT)])
for name,path in [('current',ROOT/'src/components/CoverGridHomeUi.cpp'),('baseline',ROOT/'qa-artifacts/home-epub-speed/baseline/src/components/CoverGridHomeUi.cpp')]:
    text=path.read_text(encoding='utf8');start=text.index('  const auto layout = homeui::shelfLayout(screen.device());');end=text.index('  const auto pager =',start)
    (OUT/('grid-'+name+'.inc')).write_text(text[start:end]+'\n  (void)footer;\n',encoding='utf8')
(OUT/'grid-helpers.inc').write_text(block(source,'void softCoverOutline(')+'\n'+block(source,'void coverBadge('))
layout=(ROOT/'src/apps_local/home/LicoriceLayout.h').read_text();start=layout.index('struct ShelfLayout');end=layout.index('}  // namespace homeui')
(OUT/'shelf-layout.inc').write_text(layout[start:end].replace('freeink::ui::','fui::').replace('  namespace fui = freeink::ui;',''))
run('shelf-grid',['host-tests/home-epub/grid.cpp'],[str(OUT)])

source=(ROOT/'src/components/HomeCoverCache.cpp').read_text()
source=source.replace('#include "UITheme.h"','').replace('"../apps_local/','"apps_local/')
(OUT/'home-cover-production.inc').write_text(source)
run('native-home',['host-tests/home-epub/native-home.cpp','src/apps_local/library/PreparedCoverCache.cpp','src/apps_local/home/CoverBitmapPainter.cpp','lib/GfxRenderer/Bitmap.cpp','lib/GfxRenderer/BitmapHelpers.cpp'],[str(OUT),'src/components','host-tests/library-prefetch/stubs','host-tests/home-cache/stubs',*includes,'lib/GfxRenderer','lib/Performance','freeink-sdk/libs/ui/FreeInkUI/include'])
