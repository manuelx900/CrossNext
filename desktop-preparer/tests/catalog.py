"""Saved-reader selection, dynamic card discovery and protected settings."""
from pathlib import Path
import hashlib,json,subprocess,tempfile,shutil
ROOT=Path(__file__).resolve().parents[1]
REPO=ROOT.parent
WORKER=ROOT/'dist/runtime/worker.cjs'
with tempfile.TemporaryDirectory(prefix='CrossNext-catalog-test-') as folder:
 card=Path(folder)/'Tarjeta'; card.mkdir(); (card/'.crosspoint').mkdir()
 for index in range(40):
  target=card/('fonts' if index%2 else '.fonts')/('Custom %02d'%index)
  target.mkdir(parents=True)
  shutil.copy2(REPO/'Resources/fonts/Ubuntu/Ubuntu-Regular.ttf',target/'Regular.ttf')
 def inspect(profile, missing=False):
  settings=card/'.crosspoint/settings.json'
  if missing:
   settings.unlink(missing_ok=True)
  else: settings.write_text(json.dumps(profile),encoding='utf-8')
  before=None if missing else hashlib.sha256(settings.read_bytes()).digest()
  config=Path(folder)/'job.json'; config.write_text(json.dumps({'card':str(card)}))
  result=subprocess.run(['node',str(WORKER),'inspect',str(config)],capture_output=True,text=True,encoding='utf-8',timeout=30)
  assert result.returncode==0,result.stdout+result.stderr
  assert not settings.exists() if missing else hashlib.sha256(settings.read_bytes()).digest()==before
  return json.loads(result.stdout.splitlines()[0])
 result=inspect({},missing=True)
 assert result['paragraphIndentSpaces']==2
 result=inspect({'sdFontFamilyName':'Custom 37','fontSize':19,'readerFontStorageVersion':1})
 assert result['selectedFont']=='Custom 37' and result['selectedSize']==18 and not result['fontMissing']
 assert result['fonts'][38]['sizes']==[12,14,16,18]
 assert len(result['fonts'])==41
 result=inspect({'sdFontFamilyName':'Custom 37','fontSize':2,'readerFontStorageVersion':1})
 assert result['selectedSize']==16
 result=inspect({'sdFontFamilyName':'Custom 37','fontSize':19,'fontPointSize':17,'readerFontStorageVersion':1})
 assert result['selectedSize']==16
 result=inspect({'sdFontFamilyName':'Custom 37','fontSize':14,'readerFontStorageVersion':1,'extraParagraphSpacing':True})
 assert result['paragraphIndentSpaces']==0
 result=inspect({'sdFontFamilyName':'Custom 37','fontSize':14,'readerFontStorageVersion':1,'extraParagraphSpacing':False})
 assert result['paragraphIndentSpaces']==2
 result=inspect({'sdFontFamilyName':'Custom 37','fontSize':14,'readerFontStorageVersion':1,'extraParagraphSpacing':True,'paragraphIndentSpaces':7})
 assert result['paragraphIndentSpaces']==5
 result=inspect({'sdFontFamilyName':'Missing font','fontSize':14,'readerFontStorageVersion':1})
 assert result['fontMissing'] and result['requestedFont']=='Missing font' and result['selectedFont']=='Ubuntu'
 result=inspect({'fontFamily':0,'fontSize':1})
 assert result['requestedFont']=='Noto Serif' and result['profile']['fontPointSize']==14
 # The worker rejects system roots before mounting or preparing.
 script="const w=require(process.argv[1]); let rejected=false; try {w.safeDirectory(process.env.SystemRoot);} catch(e){rejected=true;} if(!rejected) process.exit(1); if(w.cacheFile('../settings.json') || w.cacheFile('settings.json') || w.cacheFile('progress.bin')) process.exit(2);"
 subprocess.run(['node','-e',script,str(WORKER)],check=True)
 print('PASS: 40 custom families, both font roots, saved family/size, legacy sizes, missing-family warning, settings preserved, system/cache-path guards')
