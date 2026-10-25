"""Rebuild sample data ONLY in the browser simulator's preloaded SD directory.

The five short books and vector landscapes are original demonstration content.
No device code seeds or replaces a user's SD card.
"""
from pathlib import Path
import datetime as dt, hashlib, io, json, struct, zipfile, zlib
from xml.sax.saxutils import escape
import resvg_py
from PIL import Image
from seed_learning import LANGUAGE, VOCABULARY

ROOT=Path(__file__).resolve().parents[2]
SD=ROOT/'tools_local/wasm/sdcard'
assert SD.resolve().is_relative_to((ROOT/'tools_local/wasm').resolve())
DATA=SD/'.crosspoint'; (DATA/'demo-covers').mkdir(exist_ok=True)
books=[('EL BOSQUE DE TINTA','Clara Montes'),('THE QUIET HARBOR','Elena Reed'),('MAPAS DEL CIELO','Daniel Ríos'),('THE LAST GARDEN','Ada Wells'),('CIUDAD DE PAPEL','Lucía Prado')]
scenes=[
 '<circle cx="340" cy="180" r="58"/><path d="M0 560L170 300l100 150 100-220 110 330Z"/><path d="M70 650V360m-50 70 50-90 50 90m-95 60 45-90 45 90m-85 60 40-90 40 90" fill="none" stroke="black" stroke-width="14"/>',
 '<path d="M0 520q80-40 160 0t160 0 160 0v200H0Z"/><path d="M155 450h210l-45 55H190ZM230 250v190h115ZM220 280v160H135Z"/><circle cx="100" cy="190" r="40"/>',
 '<circle cx="250" cy="350" r="155" fill="none" stroke="black" stroke-width="8"/><path d="M90 350h320M250 190v320M140 240l220 220M140 460l220-220" stroke="black" stroke-width="3"/>'+''.join(f'<circle cx="{(n*73+43)%460+10}" cy="{(n*131+83)%500+120}" r="{4+n%4}"/>' for n in range(30)),
 '<path d="M0 600q120-80 240 0t240 0v120H0Z"/><path d="M220 580V270M220 350Q100 220 90 350q30 100 130 50M220 440q130-120 150-10-20 80-150 80" fill="none" stroke="black" stroke-width="16"/><circle cx="355" cy="175" r="45"/>',
 '<path d="M0 580V300h75v280h20V220h100v360h20V350h70v230h20V160h90v420h30V280h55v440H0Z"/>'+''.join(f'<rect x="{115+(n%3)*24}" y="{250+(n//3)*45}" width="10" height="22" fill="white"/>' for n in range(15)),
]
def svg(index,title='',author=''):
 return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 480 720"><rect width="480" height="720" fill="white"/><rect x="12" y="12" width="456" height="696" fill="none" stroke="black" stroke-width="8"/>{scenes[index]}<rect x="28" y="32" width="424" height="95" fill="white"/><text x="240" y="65" font-family="sans-serif" font-weight="bold" font-size="23" text-anchor="middle">{escape(title)}</text><text x="240" y="100" font-family="sans-serif" font-size="19" text-anchor="middle">{escape(author)}</text></svg>'
def picture(index,title='',author='',width=480,height=720):
 return Image.open(io.BytesIO(resvg_py.svg_to_bytes(svg_string=svg(index,title,author),width=width,height=height))).convert('RGB')
recent=json.loads((DATA/'recent.json').read_text())
recent['books']=[b for b in recent['books'] if '/Demo - ' not in b['path']]
for index,(title,author) in enumerate(books):
 im=picture(index,title,author);jpg=io.BytesIO();im.save(jpg,'JPEG',quality=88)
 cover='/'+(DATA/'demo-covers'/f'cover-{index+1}.bmp').relative_to(SD).as_posix()
 im.convert('1').save(SD/cover.lstrip('/'))
 path=f'/books/Demo - {title}.epub'
 chapters=[]
 for chapter in range(1,6):
  paragraphs=[f'Este es un libro de muestra del simulador. Capítulo {chapter}. El texto permite probar el tamaño de letra, el paso de página, el progreso de lectura y el carrusel con una biblioteca poblada.']
  if chapter == 1 and index in (0, 1):
   paragraphs = [entry[2] for entry in (VOCABULARY if index == 0 else LANGUAGE)] + paragraphs
  for part in range(12):
   paragraphs.append('La viajera abrió su cuaderno junto a la ventana. Había dibujado un camino entre las montañas y el mar, pero aún le faltaba decidir dónde comenzaba la historia. Cada tarde leía unas páginas, observaba el paisaje y añadía una idea nueva. No buscaba llegar deprisa: quería recordar los pequeños detalles del viaje. Al otro lado del puente encontró una biblioteca, un jardín y una plaza en silencio. Allí comprendió que un libro también podía ser una forma de volver a casa.')
  chapters.append(f'<html xmlns="http://www.w3.org/1999/xhtml"><head><title>{escape(title)}</title></head><body><h1>Capítulo {chapter}</h1>'+''.join('<p>'+escape(p)+'</p>' for p in paragraphs)+'</body></html>')
 with zipfile.ZipFile(SD/path.lstrip('/'),'w') as archive:
  archive.writestr('mimetype','application/epub+zip',compress_type=zipfile.ZIP_STORED)
  archive.writestr('META-INF/container.xml','<?xml version="1.0"?><container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0"><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
  archive.writestr('OEBPS/cover.jpg',jpg.getvalue())
  manifest='<item id="cover" href="cover.jpg" media-type="image/jpeg"/><item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>'
  spine=''
  for n,chapter in enumerate(chapters):
   archive.writestr(f'OEBPS/chapter-{n+1}.xhtml',chapter);manifest+=f'<item id="c{n}" href="chapter-{n+1}.xhtml" media-type="application/xhtml+xml"/>';spine+=f'<itemref idref="c{n}"/>'
  uid=hashlib.md5(path.encode()).hexdigest()
  archive.writestr('OEBPS/content.opf',f'<?xml version="1.0"?><package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="book-id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>{escape(title)}</dc:title><dc:creator>{escape(author)}</dc:creator><dc:language>es</dc:language><dc:identifier id="book-id">{uid}</dc:identifier><meta name="cover" content="cover"/></metadata><manifest>{manifest}</manifest><spine toc="ncx">{spine}</spine></package>')
  nav=''.join(f'<navPoint id="c{n}" playOrder="{n}"><navLabel><text>Capítulo {n}</text></navLabel><content src="chapter-{n}.xhtml"/></navPoint>' for n in range(1,6))
  archive.writestr('OEBPS/toc.ncx',f'<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1"><head><meta name="dtb:uid" content="{uid}"/></head><docTitle><text>{escape(title)}</text></docTitle><navMap>{nav}</navMap></ncx>')
 recent['books'].append({'path':path,'title':title,'author':author,'coverBmpPath':''})
(DATA/'recent.json').write_text(json.dumps(recent,ensure_ascii=False,separators=(',',':')))
settings=json.loads((DATA/'settings.json').read_text());settings.update(language='ES',homeLayout=1)
(DATA/'settings.json').write_text(json.dumps(settings,separators=(',',':')))
state=json.loads((DATA/'state.json').read_text());state.update(openEpubPath='',showBootScreen=False)
(DATA/'state.json').write_text(json.dumps(state,separators=(',',':')))
for folder in ('sleep','WALLPAPER'):
 (SD/folder).mkdir(exist_ok=True)
 for n in range(3):
  im=picture((n+(0 if folder=='sleep' else 2))%5,width=(480 if n!=1 else 800),height=(800 if n!=1 else 480))
  im.save(SD/folder/f'Demo-{n+1}.png')
# The fixture uses the actual checksummed journal wire format.
def checksum(data):
 h=2166136261
 for b in data:h=((h^b)*16777619)&0xffffffff
 return h
def put(data,offset,value,width):data[offset:offset+width]=int(value).to_bytes(width,'little')
def text(data,offset,width,value):
 raw=value.encode()[:width-1];data[offset:offset+len(raw)]=raw
history=bytearray(b'CRS1\x80\x01'+bytes(10));running={}
today=dt.date(2026,10,17)
for ago in range(44,-1,-1):
 if ago in (17,24,33):continue
 day=today-dt.timedelta(days=ago)
 for session in range(1+(ago%4==0)):
  index=(ago+session)%len(books);title,author=books[index];path=f'/books/Demo - {title}.epub';uid=hashlib.md5(path.encode()).hexdigest()
  b=running.setdefault(uid,dict(seconds=0,sessions=0,pages=0,first=0,finished=0,days=set()))
  seconds=900+(ago*137+session*557)%2700;pages=8+ago%24
  hour=9 if session==0 else 20;epoch=int(dt.datetime.combine(day,dt.time(hour,15),dt.timezone.utc).timestamp());localday=(day-dt.date(1970,1,1)).days
  b['seconds']+=seconds;b['sessions']+=1;b['pages']+=pages;b['first']=b['first'] or epoch;b['days'].add(localday)
  first=index<3 and not b['finished'] and ago<12
  if first:b['finished']=epoch+seconds
  data=bytearray(384);data[:4]=b'RSE1';put(data,4,seconds,4);put(data,8,pages,4);data[12]=first;put(data,14,10000 if b['finished'] else min(9500,b['pages']*25),2)
  for offset,value in [(16,epoch),(24,b['seconds']),(40,b['first']),(48,epoch+seconds),(56,b['finished'])]:put(data,offset,value,8)
  for offset,value in [(32,b['sessions']),(36,b['pages']),(64,len(b['days'])),(68,localday),(330,localday),(334,hour*3600+15*60)]:put(data,offset,value,4)
  put(data,72,b['seconds']//3,8);put(data,80,b['seconds']-b['seconds']//3,8)
  text(data,104,33,uid);text(data,137,128,title);text(data,265,64,author);data[329]=bool(b['finished']);put(data,380,checksum(data[:380]),4);history+=data
directory=DATA/'reading-history';directory.mkdir(exist_ok=True);(directory/'sessions.bin').write_bytes(history)
(SD/'notes').mkdir(exist_ok=True)
for name,content in [('Viaje','- [x] Elegir el destino\n- [ ] Reservar el tren\n- [ ] Preparar la mochila\n'),('Lecturas','- [x] Leer veinte minutos\n- [ ] Anotar una idea\n'),('Casa','- [ ] Regar las plantas\n- [ ] Ordenar libros\n'),('Estudio','- [ ] Repasar notas\n- [ ] Resolver un ejercicio\n'),('Proyectos','- [ ] Preparar material\n- [ ] Revisar avances\n'),('Semana','- [ ] Planear lecturas\n- [ ] Visitar la biblioteca\n'),('.crossplay-free-tasks','- [ ] Regar las plantas\n- [x] Ordenar el escritorio\n')]:
 (SD/'notes'/f'{name}.md').write_text(content,encoding='utf-8')
(SD/'DEMO.md').write_text('Simulator sample library: five original short demonstration EPUBs, original vector covers and six landscapes; 45-day checksummed reading journal and sample task lists. Rebuild with tools_local/wasm/seed_demo.py. These files never enter device flash.\n')
# Actual Habits format, with twenty days of varied check-ins.
habit_bytes=bytearray(16+12*(33+16+50)+4)
habit_bytes[:4]=b'HBT1';put(habit_bytes,4,1,4);anchor=(today-dt.date(1970,1,1)).days;put(habit_bytes,8,anchor,4);habit_bytes[12]=3
for n,name in enumerate(('Leer','Ejercicio','Beber agua')):
 offset=16+n*99;text(habit_bytes,offset,33,name);put(habit_bytes,offset+33,anchor-20,4)
 for ago in range(21):
  if (ago+n)%5==1:continue
  day=anchor-ago;slot=day-(anchor-399)
  habit_bytes[offset+49+slot//8]|=1<<(slot%8)
put(habit_bytes,len(habit_bytes)-4,zlib.crc32(habit_bytes[:-4]),4)
(DATA/'habits-a.bin').write_bytes(habit_bytes)
# Empty cover paths exercise real EPUB thumbnail generation at each carousel height.
recent['books'][0]['coverBmpPath']=''
(DATA/'recent.json').write_text(json.dumps(recent,ensure_ascii=False,separators=(',',':')))
from seed_learning import seed as seed_learning
seed_learning(SD)
print('Simulator seeded:',len(recent['books']),'recent books,',len(history)//384,'reading records, six wallpapers, six named lists and free tasks')
