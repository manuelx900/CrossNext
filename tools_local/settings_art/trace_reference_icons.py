"""Transcribe supplied reference silhouettes into reproducible native SVG masks."""
from pathlib import Path
import argparse,json,hashlib
from PIL import Image
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--input-dir',type=Path,required=True);a=p.parse_args()
dest=root/'assets_local/settings-icons';spec=json.loads((dest/'REFERENCE.json').read_text(encoding='utf-8'));images={}
for region in spec['regions']:
 key=region['image']
 if key not in images:images[key]=Image.open(a.input_dir/('codex-clipboard-'+key+'.png')).convert('L')
 image=images[key];size=region['size'];left=region['x'];top=region['centerY']-size//2;half=region['halfHeight'];paths=[]
 mask={(x,y) for y in range(size) if abs(top+y-region['centerY'])<=half for x in range(size) if image.getpixel((left+x,top+y))<175}
 # Table rules can cross a crop at a slight angle. Remove those connected
 # horizontal fragments while retaining detached dots and icon strokes.
 unseen=set(mask)
 while unseen:
  seed=unseen.pop();component={seed};pending=[seed]
  while pending:
   x,y=pending.pop()
   for neighbour in ((x-1,y),(x+1,y),(x,y-1),(x,y+1)):
    if neighbour in unseen:unseen.remove(neighbour);component.add(neighbour);pending.append(neighbour)
  xs=[v[0] for v in component];ys=[v[1] for v in component]
  if max(xs)-min(xs)>size*.75 and max(ys)-min(ys)<6:mask.difference_update(component)
 for y in range(size):
  if abs(top+y-region['centerY'])>half:continue
  values=[(x,y) in mask for x in range(size)]
  if sum(values)>size*.96:continue
  x=0
  while x<size:
   if not values[x]:x+=1;continue
   start=x
   while x<size and values[x]:x+=1
   paths.append(f'M{start} {y}h{x-start}v1H{start}z')
 path='<path fill="black" d="'+''.join(paths)+'"/>'
 if region.get('mirrorX'):path=f'<g transform="translate({size} 0) scale(-1 1)">'+path+'</g>'
 if region.get('slash'):
  path+=f'<path d="M18 18L{size-18} {size-18}" fill="none" stroke="white" stroke-width="15" stroke-linecap="round"/><path d="M18 18L{size-18} {size-18}" fill="none" stroke="black" stroke-width="7" stroke-linecap="round"/>'
 (dest/(region['name']+'.svg')).write_text(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {size} {size}">'+path+'</svg>\n',encoding='utf-8')

print(len(spec['regions']),'reference silhouettes traced')
