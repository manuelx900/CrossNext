"""Pack active decorations, equipment and enemy poses without retired story scenes."""
import argparse
import hashlib
import json
import importlib.util
import sys
from pathlib import Path
import struct
import zlib
from PIL import Image,ImageOps

_spec=importlib.util.spec_from_file_location('quest_expansion_gray',Path(__file__).with_name('build_gray_art.py'))
_gray=importlib.util.module_from_spec(_spec)
sys.path.insert(0,str(Path(__file__).parent))
try:_spec.loader.exec_module(_gray)
finally:sys.path.pop(0)
fit,coverage=_gray.fit,_gray.coverage

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/'assets_local/quest/expansion'
MAX_PACK_BYTES=8*1024*1024

def master(job,preview):
    path=SOURCE/'masters'/job['master']
    provenance=SOURCE/'provenance'/(job['id']+'.json')
    if not path.exists() or not provenance.exists():
        if preview:return None
        raise ValueError('Missing original illustration: '+job['id'])
    record=json.loads(provenance.read_text('utf8'))
    assert record['sha256']==hashlib.sha256(path.read_bytes()).hexdigest(),job['id']
    assert record['prompt']==job['prompt'] and record['reference']==job['reference'],job['id']
    assert record.get('references',[record['reference']])==job.get('references',[job['reference']]),job['id']
    return path

def sprite(path,size,preview=False):
    if path is None:return Image.new('RGBA',size,(0,0,0,0))
    with Image.open(path) as image:
        picture=image.convert('RGBA')
    assert picture.getchannel('A').getextrema()[0]==0
    bounds=picture.getchannel('A').point(lambda a:255 if a>=160 else 0).getbbox();assert bounds
    return fit(picture.crop(bounds),size)

def scene(path):
    if path is None:return Image.new('RGBA',(480,320),'white')
    with Image.open(path) as image:return ImageOps.fit(image.convert('RGBA'),(480,320),method=Image.Resampling.LANCZOS)

def cells(path,columns):
    with Image.open(path) as source:image=source.convert('RGBA')
    mask=image.getchannel('A').point(lambda a:255 if a>=160 else 0)
    cuts=[0]
    for n in range(1,columns):
        ideal=n*image.width/columns; radius=image.width//columns//5
        empty=[x for x in range(round(ideal)-radius,round(ideal)+radius) if not mask.crop((x,0,x+1,image.height)).getbbox()]
        assert empty,(path.name,n,'no transparent gutter between poses')
        cuts.append(min(empty,key=lambda x:abs(x-ideal)))
    cuts.append(image.width)
    result=[]
    for n in range(columns):
        cell=image.crop((cuts[n],0,cuts[n+1],image.height))
        bounds=cell.getchannel('A').point(lambda a:255 if a>=160 else 0).getbbox()
        assert bounds,(path.name,n,'empty pose')
        result.append(cell.crop(bounds))
    return result

def write_pack(path,sprites,preview):
    index=bytearray();payload=bytearray();offset=24+16*len(sprites)
    for image,mask in sprites:
        assert image.width<=256 and image.height<=320
        data=coverage(image,mask)
        index.extend(struct.pack('<IHHII',offset,image.width,image.height,len(data),zlib.crc32(data)))
        payload.extend(data);offset+=len(data)
    content=index+payload
    binary=struct.pack('<8sIIII',b'CNART003',len(sprites),offset,zlib.crc32(content),zlib.crc32(index))+content
    assert len(binary)<=MAX_PACK_BYTES
    path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(binary)
    return dict(file=path.name,sprites=len(sprites),bytes=len(binary),sha256=hashlib.sha256(binary).hexdigest(),preview=preview)

def build(output,preview=False):
    catalog=json.loads((SOURCE/'catalog.json').read_text('utf8'))
    jobs={j['id']:j for j in catalog['jobs'] if j['kind'] not in ('scene','requestScene')}
    paths={key:master(job,preview) for key,job in jobs.items()}
    found=[p for p in paths.values() if p is not None]
    hashes=[hashlib.sha256(p.read_bytes()).hexdigest() for p in found]
    assert len(set(hashes))==len(hashes),'Duplicate expansion master'
    if not preview:assert len(found)==len(jobs)
    decorations=[sprite(paths[f'D{n:03d}'],(112,144)) for n in range(101)]
    rewards=[(p,False) for p in decorations]+[(p,True) for p in decorations]
    crown=paths['C000']
    figures=[fit(c,(160,208)) for c in cells(crown,2)] if crown else [Image.new('RGBA',(160,208))]*2
    rewards.extend((p,False) for p in figures);rewards.extend((p,True) for p in figures)
    pet=sprite(paths['C002'],(160,176));rewards.extend(((pet,False),(pet,True)))
    backgrounds=[scene(paths['C001']),scene(paths['V000']),scene(paths['V001'])]
    for picture in backgrounds:
        for x in (0,240):rewards.append((picture.crop((x,0,x+240,320)),False))
    icon=backgrounds[0].copy();icon.thumbnail((108,76),Image.Resampling.LANCZOS)
    thumb=Image.new('RGBA',(112,80),'white');thumb.alpha_composite(icon,((112-icon.width)//2,(80-icon.height)//2));rewards.append((thumb,False))
    assert len(rewards)==215
    from hunt_art import records, bestiary_frames
    hunt_records=records(ROOT/'assets_local/quest/hunts')
    rewards.extend(hunt_records)
    bestiary=list(bestiary_frames(ROOT/'assets_local/quest/bestiary'))
    assert len(bestiary)==168
    rewards.extend((image,False) for image in bestiary)
    rewards.extend((image,True) for image in bestiary)
    bounds=[]
    for picture,_ in hunt_records[:12]:
        bounds.append(picture.getchannel('A').point(lambda a:255 if a>=150 else 0).getbbox())
    for picture in bestiary:
        bounds.append(picture.getchannel('A').point(lambda a:255 if a>=150 else 0).getbbox())
    header='#pragma once\n#include <cstdint>\n// Generated by tools_local/quest/build_expansion_art.py.\nnamespace questhuntart {\n'
    header+='struct Bounds { int16_t x, y, width, height; };\ninline constexpr Bounds INK[] = {\n'
    header+=''.join(f'    {{{x}, {y}, {right-x}, {bottom-y}}},\n' for x,y,right,bottom in bounds)
    header+='};\n}\n'
    (ROOT/'src/apps_local/quest/QuestExclusiveHuntBounds.h').write_text(header,'utf8')
    assert len(rewards)==593
    enemies=[]
    for n in range(60):
        path=paths[f'E{n:03d}']
        poses=cells(path,3) if path else [Image.new('RGBA',(248,280))]*3
        if path:
            factor=min(236/max(p.width for p in poses),268/max(p.height for p in poses))
            for cell in poses:
                cell=cell.resize((round(cell.width*factor),round(cell.height*factor)),Image.Resampling.LANCZOS)
                canvas=Image.new('RGBA',(248,280));canvas.alpha_composite(cell,((248-cell.width)//2,274-cell.height));enemies.append(canvas)
        else:enemies.extend(poses)
    guardians=[(p,False) for p in enemies]+[(p,True) for p in enemies]
    assert len(guardians)==360
    manifests=[write_pack(output/'rewards.bin',rewards,preview),write_pack(output/'guardians.bin',guardians,preview)]
    retired=output/'moments.bin'
    if retired.exists():retired.unlink()
    record=dict(version=2,originals=len(found),expectedOriginals=len(jobs),preview=preview,packs=manifests)
    (output/'expansion-manifest.json').write_text(json.dumps(record,indent=2)+'\n','utf8')
    print(json.dumps(record,indent=2))
    return record

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'resources/apps/reading-adventure-v2')
    parser.add_argument('--preview',action='store_true',help='Layout fixture only: missing assets remain empty; never deliver this pack')
    args=parser.parse_args();build(args.output,args.preview)
