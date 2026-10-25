"""Compile whole figures, independent garment icons, pets and auras to flash."""
from pathlib import Path
import hashlib
import json
from PIL import Image
import raster_codec

ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / 'assets_local/quest/catalog_v4'
MIN_PET_HEIGHT = 96
MAX_PET_WIDTH = 272


def components(mask):
    """Run-length components keep full silhouettes across uneven atlas rows."""
    width, height = mask.size
    pixels = mask.tobytes()
    runs, parents, previous = [], [], []
    def root(n):
        while parents[n] != n:
            parents[n] = parents[parents[n]]
            n = parents[n]
        return n
    for y in range(height):
        current = []
        x = 0
        while x < width:
            if not pixels[y*width+x]:
                x += 1
                continue
            x0 = x
            while x < width and pixels[y*width+x]:
                x += 1
            ident = len(runs)
            runs.append((x0,x,y)); parents.append(ident); current.append(ident)
        at = 0
        for ident in current:
            x0,x1,_ = runs[ident]
            while at < len(previous) and runs[previous[at]][1] < x0:
                at += 1
            other = at
            while other < len(previous) and runs[previous[other]][0] <= x1:
                left,right = root(ident),root(previous[other])
                if left != right:
                    parents[right] = left
                other += 1
        previous = current
    groups = {}
    for n, run in enumerate(runs):
        groups.setdefault(root(n),[]).append(run)
    return groups


def whole_cells(path, columns, rows):
    image = Image.open(path).convert('RGBA')
    alpha = image.getchannel('A').point(lambda a: 255 if a >= 192 else 0)
    groups = {key:runs for key,runs in components(alpha).items() if sum(x1-x0 for x0,x1,y in runs)>=5}
    ranked = sorted(groups,key=lambda g:sum(x1-x0 for x0,x1,y in groups[g]),reverse=True)
    count = columns*rows
    if len(ranked) < count:
        raise ValueError(f'{path.name}: overlapping figures; found {len(ranked)}, need {count}')
    def bounds(g):
        rs=groups[g]
        return min(r[0] for r in rs),min(r[2] for r in rs),max(r[1] for r in rs),max(r[2] for r in rs)+1
    boxes={g:bounds(g) for g in groups}
    mains=sorted(ranked[:count],key=lambda g:(boxes[g][1]+boxes[g][3])/2)
    ordered=[]
    for row in range(rows):
        ordered.extend(sorted(mains[row*columns:(row+1)*columns],key=lambda g:(boxes[g][0]+boxes[g][2])/2))
    owners={g:g for g in ordered}
    for g in ranked[count:]:
        b=boxes[g];cx,cy=(b[0]+b[2])/2,(b[1]+b[3])/2
        def distance(main):
            a=boxes[main];dx=max(a[0]-cx,0,cx-a[2]);dy=max(a[1]-cy,0,cy-a[3])
            return dx*dx+dy*dy
        owners[g]=min(ordered,key=distance)
    cells=[]
    for main in ordered:
        members=[g for g in groups if owners[g]==main]
        b=(min(boxes[g][0] for g in members),min(boxes[g][1] for g in members),max(boxes[g][2] for g in members),max(boxes[g][3] for g in members))
        if b[0]==0 or b[1]==0 or b[2]==image.width or b[3]==image.height:
            raise ValueError(f'{path.name}: subject touches image edge: {b}')
        selected=Image.new('L',image.size);pixels=selected.load()
        for g in members:
            for x0,x1,y in groups[g]:
                for x in range(x0,x1):
                    pixels[x,y]=255
        cell=image.crop(b);cell.putalpha(selected.crop(b));cells.append(cell)
    return cells


def grid_cells(path, columns, rows):
    image=Image.open(path).convert('RGBA')
    return [image.crop((round(col*image.width/columns),round(row*image.height/rows),round((col+1)*image.width/columns),round((row+1)*image.height/rows))) for row in range(rows) for col in range(columns)]


def raster(cell, width, height, name, grounded=False, trimmed=False, aura=False, filtered=False):
    smooth=aura or filtered
    cutoff=128 if smooth else 192
    bbox=cell.getchannel('A').point(lambda a:255 if a>=192 else 0).getbbox()
    if not bbox:
        raise ValueError('Empty art: '+name)
    if not aura:cell=cell.crop(bbox)
    scale=min((width-8)/cell.width,(height-8)/cell.height)
    size=max(1,round(cell.width*scale)),max(1,round(cell.height*scale))
    cell=cell.resize(size,Image.Resampling.LANCZOS if smooth else Image.Resampling.NEAREST)
    if not aura:cell=cell.crop(cell.getchannel('A').point(lambda a:255 if a>=cutoff else 0).getbbox())
    canvas=Image.new('RGBA',(width,height))
    top=(height-cell.height)//2 if aura else height-cell.height-(0 if grounded else 4)
    canvas.paste(cell,((width-cell.width)//2,top))
    ink,mask=bytearray(width//8*height),bytearray(width//8*height)
    clean=Image.new('RGBA',(width,height))
    for y in range(height):
        for x in range(width):
            r,g,b,a=canvas.getpixel((x,y))
            if a<cutoff:continue
            at,bit=y*(width//8)+x//8,128>>(x%8);mask[at]|=bit
            dark=r*299+g*587+b*114<160000
            if dark:ink[at]|=bit
            clean.putpixel((x,y),(0,0,0,255) if dark else (255,255,255,255))
    if not any(ink):raise ValueError('No ink: '+name)
    if grounded and clean.getchannel('A').getbbox()[3] != height:
        raise ValueError('Ground baseline missing: '+name)
    if grounded or trimmed:
        left,top,right,bottom=clean.getchannel('A').getbbox()
        first,last=left//8,(right+7)//8
        stride=width//8
        ink=b''.join(ink[y*stride+first:y*stride+last] for y in range(top,bottom))
        mask=b''.join(mask[y*stride+first:y*stride+last] for y in range(top,bottom))
        clean=clean.crop((first*8,top,last*8,bottom))
        width,height=clean.size
    (ASSETS/'native').mkdir(exist_ok=True);clean.save(ASSETS/'native'/f'{name}.png')
    return bytes(ink),bytes(mask),width,height


def compile_group(namespace, header, cells, width, height, group, grounded=False, trimmed=False, aura=False, filtered=False):
    sprites,references,masks,checks=[],[],[],[]
    for n,cell in enumerate(cells):
        name=f'{group}{n}';ink,mask,sprite_width,sprite_height=raster(cell,width,height,name,grounded,trimmed,aura,filtered)
        sprites.extend(((name,ink,sprite_width,sprite_height),(name+'Mask',mask,sprite_width,sprite_height)))
        references.append(name);masks.append(name+'Mask');checks.append(hashlib.sha256(ink+mask).hexdigest())
    output,size=raster_codec.compile_sheet(namespace,sprites,{group:references,group+'Masks':masks},'tools_local/quest/build_whole_art.py',ASSETS/f'{namespace}-codec-check.json')
    (ROOT/'assets_local/quest/encoded'/header).write_text(output,newline='\n')
    return {'count':len(cells),'bytes':size,'unique':len(set(checks)),'sha256':checks}


def compile_pets(cells, catalog):
    sprites,refs,masks,icons,icon_masks,checks,dimensions=[],[],[],[],[],[],[]
    for index,(cell,item) in enumerate(zip(cells,catalog)):
        if cell.getchannel('A').getextrema()[0] == 255:
            raise ValueError(f'Pet {index}: transparent exterior required')
        box=cell.getchannel('A').point(lambda a:255 if a>=192 else 0).getbbox()
        if not box:raise ValueError(f'Pet {index}: empty art')
        ratio=(box[2]-box[0])/(box[3]-box[1])
        height=min(max(MIN_PET_HEIGHT,int(item['height']))+2,int(MAX_PET_WIDTH/ratio))
        if height<MIN_PET_HEIGHT:raise ValueError(f'Pet {index}: silhouette too wide for minimum height')
        width=min(MAX_PET_WIDTH+8,((round(height*ratio)+7)//8)*8+8)
        name=f'pets{index}'
        ink,mask,w,h=raster(cell,width,height+8,name,grounded=True,filtered=True)
        if h<MIN_PET_HEIGHT or w>MAX_PET_WIDTH+8:raise ValueError(f'Pet {index}: invalid native size {w}x{h}')
        sprites.extend(((name,ink,w,h),(name+'Mask',mask,w,h)))
        refs.append(name);masks.append(name+'Mask');dimensions.append([w,h]);checks.append(hashlib.sha256(ink+mask).hexdigest())
        icon=f'petIcons{index}'
        ink,mask,w,h=raster(cell,64,80,icon,filtered=True)
        sprites.extend(((icon,ink,w,h),(icon+'Mask',mask,w,h)))
        icons.append(icon);icon_masks.append(icon+'Mask')
    output,size=raster_codec.compile_sheet('questart',sprites,{'pets':refs,'petsMasks':masks,'petIcons':icons,'petIconMasks':icon_masks},'tools_local/quest/build_whole_art.py',ASSETS/'questart-codec-check.json')
    (ROOT/'assets_local/quest/encoded/QuestArt.h').write_text(output,newline='\n')
    assert len(set(checks))==50
    return {'count':len(cells),'bytes':size,'unique':len(set(checks)),'sha256':checks,'dimensions':dimensions,'icons':len(icons)}


def build():
    (ROOT/'assets_local/quest/encoded').mkdir(exist_ok=True)
    catalog=json.loads((ASSETS/'catalog.json').read_text(encoding='utf-8'))
    figures=[]
    for page in range(10):
        cells=whole_cells(ASSETS/f'outfits-{page}.png',5,4)
        print('Extracted whole figures:',page,flush=True)
        for outfit in range(10):
            row=0 if outfit<5 else 2;col=outfit%5
            figures.extend((cells[row*5+col],cells[(row+1)*5+col]))
    for index in (183,185):
        figures[index]=whole_cells(ASSETS/'overrides'/f'figures{index}.png',1,1)[0]
    icons,auras=[],[]
    for page in range(5):
        icons.extend(whole_cells(ASSETS/f'icons-{page}.png',5,4))
    pets=[Image.open(ASSETS/'pets_v5'/f'pet-{index:02d}.png').convert('RGBA') for index in range(50)]
    auras=[Image.open(ASSETS/'auras_v5'/f'aura-{index:02d}.png').convert('RGBA') for index in range(50)]
    assert len(figures)==200 and len(icons)==100 and len(pets)==len(auras)==50
    report={}
    report['figures']=compile_group('questfigures','QuestFigures.h',figures,128,208,'figures',grounded=True,filtered=True)
    report['icons']=compile_group('questoutfits','QuestOutfitIcons.h',icons,64,80,'icons')
    report['pets']=compile_pets(pets,catalog['pets'])
    report['auras']=compile_group('questauras','QuestAuraArt.h',auras,192,224,'motifs',aura=True)
    assert report['figures']['unique']==200 and report['icons']['unique']==100
    metadata='#pragma once\n#include <cstdint>\nnamespace questartmeta {\n'
    frame_width=0
    for index in range(200):
        with Image.open(ASSETS/'native'/f'figures{index}.png') as figure:
            frame_width=max(frame_width,(figure.width*256+figure.height-1)//figure.height)
    metadata+='inline constexpr uint16_t figureFrameWidth='+str(frame_width)+';\n'
    metadata+='inline constexpr uint16_t minimumPetHeight='+str(MIN_PET_HEIGHT)+';\n'
    metadata+='inline constexpr uint16_t auraScale[]={'+','.join(str(x['scale']) for x in catalog['auras'])+'};\n}\n'
    (ROOT/'src/apps_local/quest/QuestArtMeta.h').write_text(metadata,newline='\n')
    (ASSETS/'art-check.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({key:{k:v for k,v in value.items() if k!='sha256'} for key,value in report.items()},indent=2))


if __name__=='__main__':build()
