#!/usr/bin/env python3
"""Generate reproducible synthetic APKG stress cases; never downloads or touches a device SD."""
from pathlib import Path
import argparse,copy,hashlib,io,json,random,sqlite3,struct,tempfile,zipfile
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[2]
BASIC={"id":1000,"name":"Stress Basic","type":0,"mod":0,"usn":-1,"sortf":0,"did":1,"latexPre":"","latexPost":"","latexsvg":False,"css":".card {font-family: Arial; font-size: 24px; text-align:center; color: red;} .code{white-space:pre;font-family:monospace}","flds":[{"name":n,"ord":i,"sticky":False,"rtl":False,"font":"Arial","size":20,"media":[]} for i,n in enumerate(("Front","Back"))],"tmpls":[{"name":"Card 1","ord":0,"qfmt":"{{Front}}","afmt":"{{FrontSide}}<hr id=answer>{{Back}}","bqfmt":"","bafmt":"","did":None}],"req":[[0,"any",[0]]]}
SCHEMA="""create table col(id integer primary key,crt integer,mod integer,scm integer,ver integer,dty integer,usn integer,ls integer,conf text,models text,decks text,dconf text,tags text);
create table notes(id integer primary key,guid text,mid integer,mod integer,usn integer,tags text,flds text,sfld text,csum integer,flags integer,data text);
create table cards(id integer primary key,nid integer,did integer,ord integer,mod integer,usn integer,type integer,queue integer,due integer,ivl integer,factor integer,reps integer,lapses integer,left integer,odue integer,odid integer,flags integer,data text);
create table revlog(id integer primary key,cid integer,usn integer,ease integer,ivl integer,lastIvl integer,factor integer,time integer,type integer);
create table graves(usn integer,oid integer,type integer);"""

def picture(size=(320,240),kind="RGB",name="PNG",transparent=False,noise=False):
 if noise:
  raw=random.Random(1549).randbytes(size[0]*size[1]*3);im=Image.frombytes("RGB",size,raw)
 else:
  im=Image.new(kind,size,(255,255,255,0) if transparent else "white");d=ImageDraw.Draw(im);d.rectangle((10,10,size[0]-11,size[1]-11),outline="black",width=max(2,size[0]//200));d.line((10,size[1]//2,size[0]-11,size[1]//2),fill="black",width=max(1,size[0]//1000));d.text((20,25),f"{size[0]} x {size[1]} TEST",fill="black")
 out=io.BytesIO();im.save(out,name,quality=95);return out.getvalue()

def package(path,rows,media=None):
 media=media or {};path.parent.mkdir(parents=True,exist_ok=True)
 models={str(BASIC['id']):copy.deepcopy(BASIC)};cloze=copy.deepcopy(BASIC);cloze.update(id=1001,name='Stress Cloze',type=1);cloze['flds'][0]['name']='Text';cloze['flds'][1]['name']='Extra';cloze['tmpls'][0].update(qfmt='{{cloze:Text}}',afmt='{{cloze:Text}}<br>{{Extra}}');models['1001']=cloze
 custom=copy.deepcopy(BASIC);custom.update(id=1002,name='Stress Custom');custom['tmpls'][0].update(qfmt='{{#Front}}{{Front}}{{/Front}}<script>document.body.textContent="SCRIPT_EXECUTED"</script>',afmt='{{Back}}');models['1002']=custom
 reversed_model=copy.deepcopy(BASIC);reversed_model.update(id=1003,name='Stress Reversed');reversed_model['tmpls'].append(dict(reversed_model['tmpls'][0],name='Card 2',ord=1,qfmt='{{Back}}',afmt='{{Front}}'));models['1003']=reversed_model
 with tempfile.TemporaryDirectory() as tmp:
  dbpath=Path(tmp)/'collection.anki2';db=sqlite3.connect(dbpath);db.executescript(SCHEMA)
  db.execute('insert into col values(?,?,?,?,?,?,?,?,?,?,?,?,?)',(1,1700000000,0,0,11,0,0,0,'{}',json.dumps(models),json.dumps({'1':{'id':1,'name':path.stem,'mod':0,'usn':-1,'desc':'Synthetic firmware stress tests','dyn':0,'conf':1}}),json.dumps({'1':{'id':1,'name':'Default','new':{'perDay':20,'delays':[1,10]},'rev':{'perDay':200}}}),'{}'))
  card_count=0
  for n,row in enumerate(rows,1):
   front,back=row['front'],row['back'];mid=row.get('model',1000);nid=1800000000000+n
   db.execute('insert into notes values(?,?,?,?,?,?,?,?,?,?,?)',(nid,'stress'+str(n),mid,0,-1,row.get('tags','stress'),front+'\x1f'+back,front[:64],0,0,''))
   for ordinal in row.get('ordinals',[0]):
    card_count+=1;db.execute('insert into cards values(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)',(1900000000000+card_count,nid,1,ordinal,0,-1,0,0,card_count,0,2500,0,0,0,0,0,0,''))
  db.commit();db.close()
  with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED) as z:
   z.write(dbpath,'collection.anki2');mapping={}
   for n,(name,data) in enumerate(media.items()):mapping[str(n)]=name;z.writestr(str(n),data)
   z.writestr('media',json.dumps(mapping,ensure_ascii=False))
 return {'file':path.name,'notes':len(rows),'cards':card_count,'media':len(media),'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'cases':[{k:v for k,v in x.items() if k not in ('front','back')}|{'front_bytes':len(x['front'].encode()),'back_bytes':len(x['back'].encode())} for x in rows[:100]],'case_count':len(rows),'cases_sampled':len(rows)>100}

def row(case,front,back,**kw):return dict(case=case,front=front,back=back,**kw)
def generate(out):
 out=Path(out);out.mkdir(parents=True,exist_ok=True);manifest={'synthetic':True,'decks':[],'hardware_measurements':'pending'}
 text=[row('basic','Hello','World'),row('special','¿Qué? áéíóú ñ & < >','Sí. € £ ™ ©')]
 for n in (1000,5000,20000):text.append(row(f'text-{n}',f'Pregunta {n}',(f'Paragraph {n} complete. '*((n//len(f'Paragraph {n} complete. '))+1))[:n]+' END'))
 text += [row('20k-question-50k-answer','Q'*20000,'A'*50000+' END'),row('word-2000','W'*2000,'Long token complete'),row('100-breaks','Before<br>'*100,'AFTER'),row('over-limit','TOO-LARGE','A'*70000),row('empty-front','','valid other field'),row('empty-answer','valid question',''),row('html-malformed','<b>Unclosed bold','<div>one<ul><li>two<li>three'),row('code','C++ code','<pre><code>if (a &lt; b) {\n    x = a &amp; b;\n}\n'+('long_identifier_'*100)+'</code></pre>')]
 math=[r'\(\frac{1}{2}+\sqrt{x}\)',r'\[\int_0^1 x^2\,dx=\frac{1}{3}\]',r'\(\sum_{n=1}^{\infty}\frac{1}{n^2}\)',r'\(\begin{pmatrix}1&2\\3&4\end{pmatrix}\)',r'\[\begin{aligned}x+y&=2\\x-y&=0\end{aligned}\]',r'\(\ce{2H2 + O2 -> 2H2O}\)',r'\(\ce{^{14}_{6}C -> ^{14}_{7}N + e^-}\)',r'\(\ce{SO4^{2-} + Ba^{2+} -> BaSO4 v}\)']
 for n,formula in enumerate(math):text.append(row('math-chem-'+str(n),'Read the formula',formula))
 for name,value in [('es','Español: pingüino, acción, ¿sí?'),('cjk','中文 漢字 日本語 한국어'),('rtl','العربية עברית'),('unicode','e\u0301 α β γ ∑ ∞ → 😀 👩‍🔬')]:text.append(row('unicode-'+name,name,value))
 for cols in (2,5,10):text.append(row('table-'+str(cols),'Table '+str(cols),'<table>'+''.join('<tr>'+''.join(f'<td>r{r}c{c} content</td>' for c in range(cols))+'</tr>' for r in range(3))+'</table>'))
 text += [row('table-span','Merged cells','<table><tr><td colspan="2">Merged</td><td>Other</td></tr><tr><td>A</td><td>B</td><td>C</td></tr></table>'),row('html-css','<b>Bold</b> <i>Italic</i> <a href="https://example.com">Link</a>','H<sub>2</sub>O and x<sup>2</sup><style>.card{font-size:100px}</style>'),row('script','SAFE<script>throw new Error("SCRIPT_EXECUTED")</script>','Safe answer',model=1002)]
 manifest['decks'].append(package(out/'01_Textos_y_formatos.apkg',text))
 png=picture();jpg=picture(name='JPEG');alpha=picture(kind='RGBA',transparent=True)
 media={'normal.png':png,'normal.jpg':jpg,'alpha.png':alpha,'static.gif':picture(name='GIF'),'sample.webp':picture(name='WEBP'),'wide.png':picture((8192,64)),'tall.png':picture((64,8192)),'4k.jpg':picture((3840,2160),name='JPEG',noise=True),'8k.png':picture((7680,4320)),'10mb.jpg':jpg+b'\0'*(10*1024*1024-len(jpg)),'broken.png':png[:48],'space & ñ.png':png,'../outside.png':png,'audio.ogg':b'OggS'+b'\0'*64,'video.mp4':b'\0\0\0\x18ftypmp42'}
 image_rows=[row('image-'+name,'Image '+name,'<img src="'+name.replace('&','&amp;')+'">') for name in media if name not in ('audio.ogg','video.mp4')]
 image_rows += [row('missing','Missing image','<img src="absent.png">'),row('audio','Audio reference','[sound:audio.ogg]'),row('video','Video reference','<video src="video.mp4">Video alternative text</video>'),row('external-url','Remote image','<img src="https://example.com/private.png">')]
 manifest['decks'].append(package(out/'02_Imagenes_limites.apkg',image_rows,media))
 clozes=[row('cloze-simple','The capital is {{c1::Madrid}}','Extra',model=1001),row('cloze-multiple','{{c1::one}} {{c2::two}} {{c3::three}}','',model=1001,ordinals=[0,1,2]),row('cloze-hint','A {{c1::secret::hint}} remains hidden','',model=1001),row('cloze-nested','{{c1::outer {{c2::inner}}}}','',model=1001,ordinals=[0,1]),row('cloze-image','Before {{c1::<img src="normal.png">}}','Back extra',model=1001),row('reversed','FRONT','BACK',model=1003,ordinals=[0,1]),row('type-in','{{type:Front}}','Typed answer'),row('custom','Conditional template','Static fallback',model=1002)]
 manifest['decks'].append(package(out/'03_Cloze_y_plantillas.apkg',clozes,{'normal.png':png}))
 scale=out/'Escala';scale.mkdir(exist_ok=True)
 for n in (100,1000,10000,30000):
  item=package(scale/f'{n:05d}_tarjetas.apkg',[row(f'card-{i}',f'Question {i:05d}',f'Answer {i:05d}') for i in range(n)]);item['file']='Escala/'+item['file'];manifest['decks'].append(item)
 pictures={f'image-{n:03d}.png':picture((128,128)) for n in range(100)}
 item=package(scale/'100_imagenes.apkg',[row(f'image-{n}',f'Image {n}','<img src="image-'+f'{n:03d}'+'.png">') for n in range(100)],pictures);item['file']='Escala/'+item['file'];manifest['decks'].append(item)
 integrity=out/'Integridad';integrity.mkdir(exist_ok=True)
 good=out/'01_Textos_y_formatos.apkg';(integrity/'Truncado.apkg').write_bytes(good.read_bytes()[:100]);(integrity/'No-es-SQLite.apkg').write_bytes(b'not a zip')
 bomb=bytearray(good.read_bytes());pos=bomb.index(b'PK\x01\x02');struct.pack_into('<I',bomb,pos+24,300*1024*1024);(integrity/'Tamano-excesivo.apkg').write_bytes(bomb)
 # This extra member is never selected for extraction. Media names are lookup keys,
 # not filesystem destinations; the production importer must stay inside its cache.
 path=integrity/'Rutas-extranas.apkg';package(path,[row('path-media','Path stays in archive','<img src="../outside.png">')],{'../outside.png':png})
 with zipfile.ZipFile(path,'a',zipfile.ZIP_DEFLATED) as z:z.writestr('../escape.txt',b'MUST_NOT_BE_WRITTEN')
 manifest['integrity']=[{'file':'Integridad/'+p.name,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(integrity.glob('*.apkg'))]
 (out/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf8')
 print(json.dumps({'decks':len(manifest['decks']),'cards':sum(x['cards'] for x in manifest['decks']),'directory':str(out)},ensure_ascii=False))
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--out',type=Path,default=ROOT/'assets_local/simulator/stress-corpus');generate(parser.parse_args().out)
