"""Append Spanish pixel glyphs while preserving the reviewed ASCII cuts verbatim.
Accents use the same integer pixel grid. No FreeType version can change the
existing letters, advances, line heights or baselines. Safe to run repeatedly.
"""
from pathlib import Path
import re
ROOT = Path(__file__).resolve().parents[2]
CHARS = "¡¿ÁÉÍÓÚÜÑáéíóúüñ"
BASE = dict(zip("ÁÉÍÓÚÜÑáéíóúüñ", "AEIOUUNaeiouun"))

def extend(path):
    source=path.read_text(); name=path.stem
    bm=re.search(r"static const uint8_t "+name+r"Bitmaps\[\d+\] = \{(.*?)\};",source,re.S)
    gs=re.search(r"static const EpdGlyph "+name+r"Glyphs\[\] = \{(.*?)\};",source,re.S)
    glyphs=[list(map(int,x.split(','))) for x in re.findall(r"^\s*\{([-\d, \t]+)\},",gs[1],re.M)][:95]
    end=glyphs[-1][-1]+glyphs[-1][-2]
    data=bytearray(int(x,16) for x in re.findall(r"0x([0-9a-fA-F]+)",bm[1]))[:end]
    def pixels(g):
        w,h,advance,left,top,count,offset=g
        return [[bool(data[offset+(y*w+x)//8] & (128 >> ((y*w+x)%8))) for x in range(w)] for y in range(h)]
    def append(rows,g):
        w=len(rows[0]);h=len(rows);bits=[v for row in rows for v in row];packed=bytearray((len(bits)+7)//8)
        for n,v in enumerate(bits):
            if v:packed[n//8]|=128>>(n%8)
        result=[w,h,g[2],g[3],g[4],len(packed),len(data)];data.extend(packed);return result
    intervals=[(32,126,0)];extra=[]
    for ch in sorted(CHARS):
        base=BASE.get(ch,'!' if ch=='¡' else '?');g=glyphs[ord(base)-32].copy();rows=pixels(g)
        if ch in '¡¿':
            rows=[list(reversed(r)) for r in reversed(rows)];g[4]=g[1]-max(0,g[1]//6)
        else:
            cap=glyphs[ord('H')-32][1];unit=max(1,cap//12);mh=3*unit;gap=unit;w=g[0]
            mark=[[False]*w for _ in range(mh)]
            if ch in 'Üü':
                for y in range(unit):
                    for x in (w//3,2*w//3):
                        for dx in range(unit):mark[y+unit][min(w-1,x+dx)]=True
            elif ch in 'Ññ':
                start=max(0,(w-6*unit)//2)
                for x in range(6*unit):
                    y=unit if x<2*unit or x>=4*unit else 0
                    for dy in range(unit):mark[y+dy][min(w-1,start+x)]=True
            else:
                for y in range(mh):
                    for dx in range(unit):mark[y][min(w-1,w//2+unit-y//2+dx)]=True
            if ch=='í':
                dot_end=0
                while dot_end<len(rows) and any(rows[dot_end]):dot_end+=1
                for y in range(dot_end):rows[y]=[False]*w
                for y,line in enumerate(mark):rows[y]=line
            else:
                rows=mark+[[False]*w for _ in range(gap)]+rows;g[4]+=mh+gap
        idx=len(glyphs)+len(extra);extra.append((ch,append(rows,g)));intervals.append((ord(ch),ord(ch),idx))
    bitmap='static const uint8_t '+name+'Bitmaps['+str(len(data))+'] = {\n'+''.join('    '+', '.join(f'0x{x:02X}' for x in data[i:i+18])+',\n' for i in range(0,len(data),18))+'};'
    source=source[:bm.start()]+bitmap+source[bm.end():]
    gs=re.search(r"static const EpdGlyph "+name+r"Glyphs\[\] = \{(.*?)\};",source,re.S)
    ascii_lines=gs[1].splitlines()[:96]
    assert len(glyphs)==95
    block='static const EpdGlyph '+name+'Glyphs[] = {\n'+''.join('    {'+', '.join(map(str,g))+'},  // '+(chr(32+i) if i!=60 else 'backslash')+'\n' for i,g in enumerate(glyphs))+''.join('    {'+', '.join(map(str,g))+'},  // '+ch+'\n' for ch,g in extra)+'};'
    source=source[:gs.start()]+block+source[gs.end():]
    source=re.sub(r"static const EpdUnicodeInterval "+name+r"Intervals\[\] = \{.*?\};",'static const EpdUnicodeInterval '+name+'Intervals[] = {\n'+''.join(f'    {{0x{a:X}, 0x{b:X}, {i}}},\n' for a,b,i in intervals)+'};',source,flags=re.S)
    source=re.sub(r'('+name+r'Intervals,\s*)\d+,',lambda m:m[1]+str(len(intervals))+',',source)
    source=source.replace(' * Command used:', ' * ASCII source:') if 'Spanish glyphs:' not in source else source
    if 'Spanish glyphs:' not in source:source=source.replace(' */',' * Spanish glyphs: tools_local/toybox/extend_spanish_fonts.py\n */',1)
    path.write_text(source,newline='\n');print(name,len(data)-end,'added bitmap bytes')
if __name__=='__main__':
    for size in (14,20,30,44,64):extend(ROOT/f'src/apps_local/ui/fonts/toybox_{size}.h')
