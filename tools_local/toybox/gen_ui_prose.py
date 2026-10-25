"""Generate the small reading UI face from the repository's OFL Noto Sans source."""
from pathlib import Path
import subprocess,sys,tempfile
from fontTools import subset
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as temporary:
 font=Path(temporary)/'ui-prose.ttf'
 subset.main([str(root/'lib/EpdFont/builtinFonts/source/NotoSans/NotoSans-Regular.ttf'),'--unicodes=U+0020-007E,U+00A0-00FF,U+2013-2014,U+2018-2019,U+2026','--output-file='+str(font)])
 for size in (10,14):
  name=f'ui_prose_{size}'
  result=subprocess.run([sys.executable,str(root/'lib/EpdFont/scripts/fontconvert.py'),name,str(size),str(font),'--force-autohint'],capture_output=True,text=True,encoding='utf-8',check=True)
  source=result.stdout
  source=__import__('re').sub(r' \* Command used:.*', ' * Source: tools_local/toybox/gen_ui_prose.py (Noto Sans, SIL OFL)', source)
  source='\n'.join(line for line in source.splitlines() if not line.endswith('extracted'))+'\n'
  (root/f'src/apps_local/ui/fonts/{name}.h').write_text(source,encoding='utf-8')
