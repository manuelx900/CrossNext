from pathlib import Path
import sqlite3,sys,json
out=Path(sys.argv[1]);out.mkdir(exist_ok=True)
for size in (512,1024,4096,65536):
 path=out/f'anki-{size}.sqlite'
 if path.exists():path.unlink()
 db=sqlite3.connect(path);db.execute(f'pragma page_size={size}')
 db.execute('create table notes(id integer primary key,guid text,mid integer,mod integer,usn integer,tags text,flds text,sfld text,csum integer,flags integer,data text)')
 db.execute('create table cards(id integer primary key,nid integer,did integer,ord integer,mod integer,usn integer,type integer,queue integer,due integer,ivl integer,factor integer,reps integer,lapses integer,left integer,odue integer,odid integer,flags integer,data text)')
 for n in range(1,301):
  fields=f'Front {n}\x1fBack {n}'
  if n==173:fields='漢字 '+'Overflow '*1400+'\x1fAnswer'
  db.execute('insert into notes values(?,?,?,?,?,?,?,?,?,?,?)',(n,'id',2,100,-1,'',fields,'',0,0,''))
  db.execute('insert into cards values(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)',(1000000000000+n,n,1,n%2,100,-1,0,0,0,0,0,0,0,0,0,0,0,''))
 db.commit();db.close()
print('Generated real SQLite fixtures at four page sizes, including B-tree branches and overflow chains')

try:
 from compression import zstd
except ImportError:
 import zstandard as zstd
raw=(out/'anki-4096.sqlite').read_bytes()
compressed=zstd.compress(raw) if hasattr(zstd,'compress') else zstd.ZstdCompressor().compress(raw)
(out/'anki-zstd.bin').write_bytes(compressed)

# Cloze math must hide the answer before adapting grouping braces. A large
# note must not abort the valid cards after it.
path=out/'anki-regressions.sqlite'
if path.exists():path.unlink()
db=sqlite3.connect(path)
db.execute('create table notes(id integer primary key,guid text,mid integer,mod integer,usn integer,tags text,flds text,sfld text,csum integer,flags integer,data text)')
db.execute('create table cards(id integer primary key,nid integer,did integer,ord integer,mod integer,usn integer,type integer,queue integer,due integer,ivl integer,factor integer,reps integer,lapses integer,left integer,odue integer,odid integer,flags integer,data text)')
rows=['Energy: ${{c1::mc^2}}$'+chr(31)+'Extra explanation', 'a'*70000+chr(31)+'b'*70000, 'after'+chr(31)+'still imported']
for n,fields in enumerate(rows,1):
 db.execute('insert into notes values(?,?,?,?,?,?,?,?,?,?,?)',(n,'id',2,100,-1,'',fields,'',0,0,''))
 db.execute('insert into cards values(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)',(100+n,n,1,0,100,-1,0,0,0,0,0,0,0,0,0,0,0,''))
db.commit();db.close()

# Native long faces must survive SQLite overflow pages without truncation.
path=out/'anki-long.sqlite'
if path.exists():path.unlink()
db=sqlite3.connect(path)
db.execute('create table notes(id integer primary key,guid text,mid integer,mod integer,usn integer,tags text,flds text,sfld text,csum integer,flags integer,data text)')
db.execute('create table cards(id integer primary key,nid integer,did integer,ord integer,mod integer,usn integer,type integer,queue integer,due integer,ivl integer,factor integer,reps integer,lapses integer,left integer,odue integer,odid integer,flags integer,data text)')
long_rows=[('Q'*1000,'A'*5000),('Q'*20000,'A'*50000),('W'*2000,'\n'*100+'TAIL'),('','valid answer'),('valid question',''),('after','survives')]
for n,(front,back) in enumerate(long_rows,1):
 db.execute('insert into notes values(?,?,?,?,?,?,?,?,?,?,?)',(n,'id',2,100,-1,'',front+'\x1f'+back,'',0,0,''))
 db.execute('insert into cards values(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)',(100+n,n,1,0,100,-1,0,0,0,0,0,0,0,0,0,0,0,''))
db.commit();db.close()
