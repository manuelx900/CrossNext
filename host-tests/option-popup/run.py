"""Compile the production popup class with a host draw target and real SDK routing."""
from pathlib import Path
import os,shutil,subprocess,sys
r=Path(__file__).resolve().parents[2];d=r/'datos.local/settings-pagination/native';d.mkdir(parents=True,exist_ok=True)
s=(r/'src/components/OptionPopup.h').read_text(encoding='utf-8');s=s[s.index('class OptionPopup {'):]
(d/'production.inc').write_text(s,encoding='utf-8')
em=r/'.venv/emsdk/upstream/emscripten';env=os.environ.copy();env['PATH']=str(em)+os.pathsep+env.get('PATH','')
command=[str(em/('em++.exe' if os.name=='nt' else 'em++')),'-std=c++20','-O1','-Wall','-Wextra','-Werror','-Wno-comment','-Wno-missing-field-initializers','-I'+str(d),'-Ifreeink-sdk/libs/ui/FreeInkUI/include','-Ifreeink-sdk/libs/assets/Icons/include','host-tests/option-popup/test.cpp','src/apps_local/ui/PixelMenu.cpp','freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp','-sENVIRONMENT=node','-sEXIT_RUNTIME=1','-o',str(d/'test.cjs')]
subprocess.run(command,cwd=r,env=env,check=True)
node=shutil.which('node') or str(r/'.venv/emsdk/node/24.19.0_64bit/node.exe')
subprocess.run([node,str(d/'test.cjs')],cwd=r,check=True)
