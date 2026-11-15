"""Import the user's menu silhouettes; firmware generation uses the saved SVGs."""
from pathlib import Path
import argparse, hashlib, json
from PIL import Image, ImageOps

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--input-dir', type=Path, required=True)
args = parser.parse_args()
dest = root / 'assets_local/navigation-icons'
spec = json.loads((dest / 'REFERENCE.json').read_text('utf8'))
for region in spec['regions']:
    path = args.input_dir / spec['images'][str(region['image'])]['file']
    image = Image.open(path).convert('L').crop(region['rect'])
    if region.get('invert'):
        image = ImageOps.invert(image)
    mask = image.point(lambda v: 255 if v < 175 else 0)
    box = mask.getbbox()
    if not box:
        raise ValueError('Empty silhouette: ' + region['name'])
    image = ImageOps.invert(mask.crop(box))
    paths = []
    for y in range(image.height):
        x = 0
        while x < image.width:
            if image.getpixel((x, y)):
                x += 1
                continue
            start = x
            while x < image.width and not image.getpixel((x, y)):
                x += 1
            paths.append(f'M{start} {y}h{x-start}v1H{start}z')
    # Preserve the reference aspect ratio; the generator supplies shared padding.
    svg = f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {image.width} {image.height}"><path fill="black" d="'+''.join(paths)+'"/></svg>\n'
    (dest / (region['name'] + '.svg')).write_text(svg, 'utf8')
for source in spec['images'].values():
    source['sha256'] = hashlib.sha256((args.input_dir / source['file']).read_bytes()).hexdigest()
(dest / 'REFERENCE.json').write_text(json.dumps(spec, ensure_ascii=False, indent=2)+'\n', 'utf8')
print('Imported', len(spec['regions']), 'reference silhouettes')
