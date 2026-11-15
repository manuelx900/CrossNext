#!/usr/bin/env python3
"""Run decimal arithmetic and themed calculator/modal checks."""
import argparse, os, shutil, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--render',type=Path)
    args=ap.parse_args(); out=ROOT/'.pio/host-tests/calculator'; out.mkdir(parents=True,exist_ok=True)
    windows=os.name=='nt'; sdk=ROOT/'.venv/emsdk/upstream/emscripten'
    cc=str(sdk/'emcc.exe') if windows else os.environ.get('CC','cc')
    cpp=str(sdk/'em++.exe') if windows else os.environ.get('CXX','c++')
    node=shutil.which('node') or next((ROOT/'.venv/emsdk/node').glob('*/bin/node.exe'),None)
    runtime=[str(node)] if windows else []
    flags=['-sSTACK_SIZE=1048576','-sENVIRONMENT=node','-sEXIT_RUNTIME=1','-sALLOW_MEMORY_GROWTH=1','-sNODERAWFS=1'] if windows else []
    def run(cmd): subprocess.run(cmd,cwd=ROOT,check=True)
    common=['-std=c++20','-Wall','-Wextra','-Werror','-Wno-comment','-O2','-Ilib/decNumber/src','-Isrc/apps_local/calculator']
    objects=[]
    for unit in ['decnumber_impl','deccontext_impl']:
        dest=out/(unit+'.o'); run([cc,'-std=c99','-O2','-Ilib/decNumber/src','-c','lib/decNumber/src/'+unit+'.c','-o',str(dest)]); objects.append(str(dest))
    executable=out/('arithmetic.cjs' if windows else 'arithmetic')
    run([cpp,*common,*flags,'host-tests/calculator/test_calculator.cpp',*objects,'-o',str(executable)])
    run([*runtime,str(executable)])
    includes=['-Itest/language_pack/stubs','-Ihost-tests/reading-suite/stubs','-Ilib/I18n','-Ilib/EpdFont','-Ifreeink-sdk/libs/ui/FreeInkUI/include','-Ifreeink-sdk/libs/assets/Icons/include']
    sources=['host-tests/calculator/test_screens.cpp','src/apps_local/calculator/CalculatorScreens.cpp','src/apps_local/ui/PixelMenu.cpp','src/apps_local/pomodoro/TimerSettingsUi.cpp','lib/I18n/I18n.cpp','lib/I18n/I18nStrings.cpp','lib/I18n/LanguagePack.cpp','freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp']
    executable=out/('screens.cjs' if windows else 'screens')
    run([cpp,*common,*includes,*flags,*sources,*objects,'-o',str(executable)])
    if args.render: args.render.mkdir(parents=True,exist_ok=True)
    run([*runtime,str(executable),*([args.render.resolve().as_posix()] if args.render else [])])
if __name__=='__main__': main()
