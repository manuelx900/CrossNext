"""Rebuild Quest catalog artwork and the Intercept microSD illustration pack."""
from pathlib import Path
import hashlib,json,shutil
from collections import deque
from PIL import Image,ImageOps,ImageFilter
ROOT=Path(__file__).resolve().parents[2]
ASSETS=ROOT/'assets_local/quest/expansion'
def packed(im):
    im=im.convert('1',dither=Image.Dither.FLOYDSTEINBERG)
    return bytes(v^255 for v in im.tobytes())
def silhouette(im):
    im=im.convert('L').point(lambda v:255 if v>220 else 0)
    im=ImageOps.expand(im,1,fill=255)
    from PIL import ImageDraw
    ImageDraw.floodfill(im,(0,0),128,thresh=0)
    return im.crop((1,1,im.width-1,im.height-1)).point(lambda v:255 if v==128 else 0)
def cells(im,cols,rows,row_cuts=None):
    cuts=row_cuts or [round(n*im.height/rows) for n in range(rows+1)]
    for n in range(cols*rows):
        row=n//cols;col=n%cols
        yield im.crop((round(col*im.width/cols),cuts[row],round((col+1)*im.width/cols),cuts[row+1]))
def pet_cells(im):
    # Segment complete connected silhouettes across nominal atlas boundaries.
    gray=im.convert('L');w,h=gray.size
    mask=gray.point(lambda v:255 if v<200 else 0).filter(ImageFilter.MaxFilter(3))
    px=mask.load();seen=bytearray(w*h);parts={}
    for y in range(h):
        for x in range(w):
            if seen[y*w+x] or not px[x,y]:continue
            queue=deque([(x,y)]);seen[y*w+x]=1;part=[]
            while queue:
                cx,cy=queue.popleft();part.append((cx,cy))
                for nx,ny in ((cx-1,cy),(cx+1,cy),(cx,cy-1),(cx,cy+1)):
                    if 0<=nx<w and 0<=ny<h and not seen[ny*w+nx] and px[nx,ny]:
                        seen[ny*w+nx]=1;queue.append((nx,ny))
            if len(part)<1000:continue
            xs=[v[0] for v in part];ys=[v[1] for v in part]
            left,top,right,bottom=min(xs),min(ys),max(xs)+1,max(ys)+1
            col=min(4,int((left+right)*5/(2*w)));row=min(4,int((top+bottom)*5/(2*h)))
            key=row*5+col
            if key in parts:raise ValueError('Pet silhouettes merged or out of order')
            kept=Image.new('L',gray.size);kp=kept.load()
            for cx,cy in part:kp[cx,cy]=255
            cell=Image.composite(gray,Image.new('L',gray.size,255),kept)
            parts[key]=cell.crop((left,top,right,bottom))
    if set(parts)!=set(range(25)):raise ValueError('Expected twenty-five complete pet silhouettes')
    return [parts[n] for n in range(25)]


def fitted(im,width,height):
    gray=im.convert('L');bounds=gray.point(lambda v:255 if v<210 else 0).getbbox()
    if not bounds:raise ValueError('Empty atlas cell')
    crop=gray.crop(bounds);crop.thumbnail((width-4,height-4),Image.Resampling.LANCZOS)
    out=Image.new('L',(width,height),255);out.paste(crop,((width-crop.width)//2,(height-crop.height)//2));return out
def array(name,data):
    return 'inline constexpr uint8_t '+name+'[]={\n'+''.join('  '+','.join(f'0x{x:02x}' for x in data[n:n+24])+',\n' for n in range(0,len(data),24))+'};\n'
def build():
    from build_whole_art import build as build_complete_catalog
    build_complete_catalog()
    import importlib.util
    spec = importlib.util.spec_from_file_location('intercept_pack', ROOT / 'tools_local/intercept/build_resource_pack.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.build(ROOT / 'resources/apps/intercept/graphics.bin')
if __name__=='__main__':build()
