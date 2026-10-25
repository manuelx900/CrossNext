"""Extract exact production methods; the harness supplies only HAL/Activity seams."""
from pathlib import Path
import sys
source=(Path(__file__).resolve().parents[2]/'src/apps_local/picross/PicrossActivity.cpp').read_text()
out=[]
for name in ('touchSave','flushSave','saveState','loadState'):
 start=source.index(('bool' if name in ('saveState','loadState') else 'void')+' PicrossActivity::'+name+'(')
 opening=source.index('{',start);depth=1;end=opening+1
 while depth:
  if source[end]=='{':depth+=1
  elif source[end]=='}':depth-=1
  end+=1
 out.append(source[start:end])
Path(sys.argv[1]).write_text('\n'.join(out))
