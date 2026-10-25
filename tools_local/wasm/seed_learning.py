"""Deterministic sample content for the browser's disposable SD image only."""
from pathlib import Path
import datetime as dt,re,shutil,struct

LANGUAGE=[
 ('curious','curioso','Alice was curious about the tiny door.'),
 ('wonder','preguntarse','Sometimes I wonder where the river ends.'),
 ('whisper','susurrar','She began to whisper beside the sleeping child.'),
 ('journey','viaje','The long journey began at the quiet harbor.'),
 ('narrow','estrecho','A narrow bridge crossed the stream.'),
 ('brave','valiente','The brave traveler opened the old gate.'),
 ('gentle','suave','A gentle wind moved through the garden.'),
 ('bright','brillante','A bright star appeared above the harbor.'),
 ('wander','vagar','They like to wander along the shore.'),
 ('shelter','refugio','The old library offered shelter from the rain.'),
 ('distant','lejano','A distant bell rang across the water.'),
 ('remarkable','extraordinario','She recorded a remarkable discovery in her notebook.'),
]
VOCABULARY=[
 ('sendero','Camino estrecho que se forma al pasar personas o animales.','Un sendero cruzaba el jardín hasta llegar al río.'),
 ('tenue','De poca intensidad o fuerza.','Una luz tenue iluminaba el escritorio.'),
 ('anhelo','Deseo intenso de conseguir algo.','El anhelo de viajar la acompañaba desde niña.'),
 ('umbral','Entrada o comienzo de un lugar o de una etapa.','Se detuvo en el umbral de la biblioteca.'),
 ('sosiego','Tranquilidad, calma y ausencia de agitación.','Encontró sosiego junto a la ventana.'),
 ('recóndito','Muy escondido, apartado o difícil de encontrar.','En un recóndito rincón guardaba sus mapas.'),
]
def hash_bytes(data):
 h=2166136261
 for byte in data:h=((h^byte)*16777619)&0xffffffff
 return h
def field(value,width):
 value=value.encode();assert len(value)<width
 return value+bytes(width-len(value))
def record(kind,payload):
 data=struct.pack('<III',0x31564243,2 if kind==2 else 1,kind)+payload
 return data+struct.pack('<I',hash_bytes(data))
def seed(sd):
 root=Path(__file__).resolve().parents[2];sd=Path(sd).resolve()
 assert sd==root/'tools_local/wasm/sdcard'
 data=sd/'.crosspoint';base=data/'vocabulary'
 for name in ['words','contexts']:(base/name).mkdir(parents=True,exist_ok=True)
 today=(dt.date.today()-dt.date(1970,1,1)).days
 for purpose,rows in enumerate([VOCABULARY,LANGUAGE]):
  for n,(word,meaning,sentence) in enumerate(rows):
   dictionary='Demo ES definitions' if not purpose else 'Demo EN-ES'
   path='/books/Demo - EL BOSQUE DE TINTA.epub' if not purpose else '/books/Demo - THE QUIET HARBOR.epub'
   book='El bosque de tinta' if not purpose else 'The Quiet Harbor'
   language='ES' if not purpose else 'EN → ES'
   # Hash's field separators are bytes 0xff, rather than UTF-8 U+00FF.
   def identify_word():
    a,b=1469598103934665603,7809847782465536322
    for part in [dictionary.encode(),word.encode()]:
     for c in part+b'\xff':
      if 65<=c<=90:c+=32
      a=((a^c)*1099511628211)&0xffffffffffffffff;b=((b^c)*14029467366897019727)&0xffffffffffffffff
    return a,b
   a,b=identify_word();wid=f'{a:016x}{b:016x}'
   def add(value):
    nonlocal a,b
    for c in value.encode()+b'\xff':
     if 65<=c<=90:c+=32
     a=((a^c)*1099511628211)&0xffffffffffffffff;b=((b^c)*14029467366897019727)&0xffffffffffffffff
   a,b=1469598103934665603,7809847782465536322;add(wid)
   for c in path.encode():a=((a^c)*1099511628211)&0xffffffffffffffff;b=((b^c)*14029467366897019727)&0xffffffffffffffff
   add('');add('0')
   if purpose:add('translation')
   cid=f'{a:016x}{b:016x}'
   due=purpose==1 and n<5
   card=struct.pack('<qffiiIIIII',10000+purpose*100+n,0. if due else 12.,0. if due else 5.,0 if due else today+7,-1 if due else today-3,0 if due else 3,0,0,0 if due else 2,0)
   payload=field(wid,33)+field(word,192)*2+field(meaning,2048)+field(dictionary,128)+field(language,32)+field(cid,33)+card+struct.pack('<III',1,0,0)
   (base/'words'/f'{wid}.dat').write_bytes(record(1,payload))
   payload=field(cid,33)+field(wid,33)+field(path,512)+field(book,192)+field('Demo',128)+field('Capítulo 1',192)+field(word,192)+field(sentence,1536)+struct.pack('<IIIII',0xffffffff,0,0,0,purpose)
   (base/'contexts'/f'{cid}.dat').write_bytes(record(2,payload))
 quotes=[('El bosque de tinta','Clara Montes','Un libro también puede ser una forma de volver a casa.'),('The Quiet Harbor','Elena Reed','The smallest journeys sometimes leave the clearest memories.'),('Mapas del cielo','Daniel Ríos','Cada mapa empieza por una pregunta.'),('The Last Garden','Ada Wells','There is always another season waiting beneath the soil.'),('Ciudad de papel','Lucía Prado','Las historias encuentran espacio incluso en los márgenes.')]
 (sd/'My Clippings.txt').write_text(''.join(f'{title} ({author})\n- Highlight on page {n+1} | Location {n*42+1}\n\n{quote}\n==========\n' for n,(title,author,quote) in enumerate(quotes)),encoding='utf-8')
 shutil.copy2(root/'assets_local/simulator/murdle-v4.sav',data/'murdle.sav')
 for n,source in enumerate(sorted((root/'assets_local/wallpapers_reference').glob('*.png')),1):
  from PIL import Image
  Image.open(source).convert('1').save(sd/'WALLPAPER'/f'00-Reference-{n:02}.png')
 print('Learning samples: 18 words, 5 due; 5 quotes; Murdle case 1; 9 reference wallpapers.')
 from seed_real_learning import seed as seed_real_learning
 seed_real_learning(sd)

if __name__=='__main__':seed(Path(__file__).resolve().parents[2]/'tools_local/wasm/sdcard')
