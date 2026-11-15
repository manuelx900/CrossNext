"""Real engine round trips, cancellation, changed sources and card preservation."""
from pathlib import Path
import hashlib, io, json, os, shutil, struct, subprocess, time, zipfile
from PIL import Image, ImageDraw
ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent
TEST = ROOT / 'build' / ('integration-' + str(time.time_ns()))
CARD = TEST / 'Tarjeta-日本語'
(CARD/'books').mkdir(parents=True)
(CARD/'.crosspoint/reading').mkdir(parents=True)
(CARD/'fonts').mkdir()
for font in ('Ubuntu','Bitter','Noto Serif'):
    shutil.copytree(REPO/'Resources/fonts'/font,CARD/'fonts'/font)
profile = {'fontSize':14,'readerFontStorageVersion':1,'sdFontFamilyName':'Bitter', 'lineHeight':2,
    'fontWeight':0,'embeddedStyle':True,'hyphenationEnabled':True,'orientation':0,'screenMargin':8,
    'wordSpacing':100,'characterSpacing':2,'epubEngine':0,'language':'es','xtcCoverRotation':1}
(CARD/'.crosspoint/settings.json').write_text(json.dumps(profile),encoding='utf-8')
(CARD/'.crosspoint/reading/stats.json').write_bytes(b'{"sessions":123,"minutes":3456}')
(CARD/'languages').mkdir(); (CARD/'languages/keep.txt').write_text('do not modify')

def epub(filename, paragraphs=80, title='Preparador de prueba'):
    image = Image.new('RGB',(240,360),'white'); draw = ImageDraw.Draw(image)
    draw.rectangle((5,5,234,354),outline='black',width=4); draw.line((10,330,230,60),fill='black',width=8)
    out = io.BytesIO(); image.save(out,format='PNG')
    tiny = io.BytesIO(); image.resize((15,15)).save(tiny,format='JPEG')
    with zipfile.ZipFile(filename,'w',zipfile.ZIP_DEFLATED) as z:
        z.writestr('mimetype','application/epub+zip')
        z.writestr('META-INF/container.xml','<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
        z.writestr('OPS/content.opf',f'''<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">test</dc:identifier><dc:title>{title}</dc:title><dc:creator>CrossNext</dc:creator><dc:language>es</dc:language><meta name="cover" content="cover"/></metadata><manifest><item id="body" href="body.xhtml" media-type="application/xhtml+xml"/><item id="cover" href="cover.png" media-type="image/png" properties="cover-image"/></manifest><spine><itemref idref="body"/></spine></package>''')
        text = '<p>Una biblioteca preparada permite continuar la lectura. El árbol, la lluvia y la música acompañan este ejemplo de paginación.</p>' * paragraphs
        z.writestr('OPS/body.xhtml','<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Prueba</title><style>h1 {font-size:2em} p {text-indent:1em}</style></head><body><h1>Un capítulo grande</h1><img src="cover.png"/><img src="tiny.jpeg"/>'+text+'</body></html>')
        z.writestr('OPS/cover.png',out.getvalue())
        z.writestr('OPS/tiny.jpeg',tiny.getvalue())

def xtc(filename, gray=False):
    width,height=480,800
    payload=width*((height+7)//8)*2 if gray else ((width+7)//8)*height
    size=22+payload
    header=struct.pack('<IBBHBBBBIQQQQII',0x48435458 if gray else 0x00435458,1,0,1,0,0,0,0,0,0,56,72,0,0,0)
    table=struct.pack('<QIHH',72,size,width,height)
    page=struct.pack('<IHHBBIQ',0x00485458 if gray else 0x00475458,width,height,0,0,payload,0)
    filename.write_bytes(header+table+page+bytes([0x55])*payload)

epub(CARD/'books/Épreuve-日本語.epub')
xtc(CARD/'books/Rotado.xtc'); xtc(CARD/'books/Gris.xtch',True)

def snapshot(directory):
    return {str(p.relative_to(directory)):hashlib.sha256(p.read_bytes()).hexdigest() for p in directory.rglob('*') if p.is_file()}
protected={p:hashlib.sha256(p.read_bytes()).hexdigest() for p in CARD.rglob('*') if p.is_file()}
def intact():
    assert all(p.exists() and hashlib.sha256(p.read_bytes()).hexdigest()==value for p,value in protected.items())
def run(name, **options):
    config={'card':str(CARD),'verify':True,'fonts':['Bitter','Ubuntu','Noto Serif'],'sizes':[12,18],**options}
    filename=TEST/(name+'.json'); filename.write_text(json.dumps(config),encoding='utf-8')
    started=time.monotonic()
    result=subprocess.run(['node',str(ROOT/'dist/runtime/worker.cjs'),'run',str(filename)],capture_output=True,text=True,encoding='utf-8',timeout=120)
    (TEST/(name+'.jsonl')).write_text(result.stdout+result.stderr,encoding='utf-8')
    assert result.returncode==0,result.stdout+result.stderr
    events=[json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
    assert events[-1]['type']=='finished',events
    intact()
    return events,time.monotonic()-started

first,first_seconds=run('first')
assert first[-1]['completed']==3 and first[-1]['failed']==0,first[-1]
catalog = next(event for event in first if event['type'] == 'catalog')
assert catalog['device'] == 'xteink_x4_pro' and catalog['viewableInsets'] == [10, 1, 0, 1], catalog
assert catalog['viewportWidth'] == 462, catalog
verified=[e for e in first if e['type']=='verified']; assert len(verified)==6 and all(e['pages']>0 for e in verified)
thumbs=list((CARD/'.crosspoint').rglob('thumb_*.bmp')); assert len(thumbs)>=36
images=list((CARD/'.crosspoint').rglob('*.pxc')); assert images
before=snapshot(CARD/'.crosspoint')
times={p:p.stat().st_mtime_ns for p in (CARD/'.crosspoint').rglob('*') if p.is_file()}
second,second_seconds=run('warm')
assert second[-1]['total'] == 0 and second[-1]['alreadyPrepared'] == 3, second[-1]
assert not any(event['type'] == 'progress' for event in second), 'Prepared books were processed again'
assert snapshot(CARD/'.crosspoint')==before
assert all(p.stat().st_mtime_ns==value for p,value in times.items()),'Warm preparation rewrote existing cache files'
missing = next((CARD / '.crosspoint').glob('xtc_*/thumb_*.bmp'))
missing.unlink()
repaired, _ = run('missing-cover')
assert repaired[-1]['total'] == 1 and repaired[-1]['alreadyPrepared'] == 2 and missing.exists(), repaired[-1]
assert snapshot(CARD / '.crosspoint') == before, 'Missing cover was not restored from the prepared book'
classic,_=run('classic',profile={'epubEngine':1})
assert all(e['pages']>0 for e in classic if e['type']=='verified')
assert list((CARD/'.crosspoint').rglob('*.classic-p*.bin'))
landscape,_=run('landscape',profile={'orientation':1})
assert all(e['pages']>0 for e in landscape if e['type']=='verified')
all_sizes,_=run('all-sizes',formats=['epub'],fonts=['Bitter'],sizes='all')
assert [e['size'] for e in all_sizes if e['type']=='verified']==[12,14,16,18]

# Cancel during a real chapter build, then reuse the valid completed outputs.
epub(CARD/'books/Largo.epub',4000)
protected[CARD/'books/Largo.epub']=hashlib.sha256((CARD/'books/Largo.epub').read_bytes()).hexdigest()
cancel=TEST/'cancel'; config=TEST/'cancel-job.json'
config.write_text(json.dumps({'card':str(CARD),'fonts':['Bitter'],'sizes':[14],'cancelFile':str(cancel)}))
child=subprocess.Popen(['node',str(ROOT/'dist/runtime/worker.cjs'),'run',str(config)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding='utf-8')
lines=[]
for line in child.stdout:
    lines.append(line)
    event=json.loads(line)
    if event['type']=='progress' and event['book'].endswith('Largo.epub') and event.get('phase')==2:
        cancel.write_text('cancel')
child.wait(timeout=30); assert child.returncode==0
assert json.loads(lines[-1])['cancelled']; intact()
resumed,_=run('resumed',fonts=['Bitter'],sizes=[14]); assert resumed[-1]['completed'] + resumed[-1]['alreadyPrepared'] == 4
assert resumed[-1]['total'] < 4, resumed[-1]

# The archive changes at the same logical filename; old layouts cannot be reused.
target=CARD/'books/Épreuve-日本語.epub'; epub(target,200,'Contenido modificado')
os.utime(target,(time.time()+4,time.time()+4)); protected[target]=hashlib.sha256(target.read_bytes()).hexdigest()
changed,_=run('changed',fonts=['Bitter'],sizes=[14]); assert changed[-1]['failed']==0

# Corruption is reported and other books still finish.
(CARD/'books/Dañado.epub').write_bytes(b'not an epub')
protected[CARD/'books/Dañado.epub']=hashlib.sha256((CARD/'books/Dañado.epub').read_bytes()).hexdigest()
broken,_=run('corrupt',fonts=['Bitter'],sizes=[14]); assert broken[-1]['failed']==1 and broken[-1]['alreadyPrepared']==4 and broken[-1]['total']==1

report={'firstSeconds':round(first_seconds,3),'warmSeconds':round(second_seconds,3),'verifiedLayoutsFirst':len(verified),
 'coverVariants':len(thumbs),'imageCaches':len(images),'preservedFiles':len(protected),'cases':[
 'EPUB real page deserialization','XTC','XTCH','rotated thumbnails','multiple fonts and sizes','all four firmware vector sizes',
 'hardware bezel/cache geometry','classic engine isolation','landscape isolation','warm reuse without rewriting',
 'missing cover repair','pending-only resume counter','cancel/resume','changed EPUB invalidation',
 'corrupt EPUB isolation','book/settings/statistics preservation','Unicode paths']}
(TEST/'report.json').write_text(json.dumps(report,indent=2))
print(TEST); print(json.dumps(report,indent=2))
