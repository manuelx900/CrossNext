"""Import Home icon silhouettes from the user-supplied rounded reference."""
from pathlib import Path
import argparse
import hashlib
import json

from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[2]
DEST = ROOT / 'assets_local/home-icons'
REGIONS = {
    'Folder': ([42, 580, 122, 650], True),
    'Library': ([42, 680, 122, 750], False),
    'Send': ([42, 780, 122, 850], False),
    'Stats': ([42, 880, 122, 950], False),
    'Games': ([38, 980, 125, 1050], False),
    'Apps': ([42, 1080, 122, 1150], False),
    'Settings': ([42, 1180, 122, 1250], False),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reference', type=Path)
    args = parser.parse_args()
    source = Image.open(args.reference).convert('L')
    if source.size != (767, 1280):
        raise ValueError('Expected the supplied 767 x 1280 reference')
    for name, (rect, invert) in REGIONS.items():
        image = source.crop(rect)
        if invert:
            image = ImageOps.invert(image)
        ink = image.point(lambda v: 255 if v < 175 else 0)
        box = ink.getbbox()
        if box is None:
            raise ValueError('Empty silhouette: ' + name)
        ink = ink.crop(box)
        paths = []
        for y in range(ink.height):
            x = 0
            while x < ink.width:
                if not ink.getpixel((x, y)):
                    x += 1
                    continue
                begin = x
                while x < ink.width and ink.getpixel((x, y)):
                    x += 1
                paths.append(f'M{begin} {y}h{x-begin}v1H{begin}z')
        (DEST / (name + '.svg')).write_text(
            f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {ink.width} {ink.height}">'
            '<path fill="black" d="' + ''.join(paths) + '"/></svg>\n', 'utf8')
    # Carousel2 retains its existing gear silhouette.
    (DEST / 'Gear.svg').write_bytes((ROOT / 'assets_local/navigation-icons/Gear.svg').read_bytes())
    (DEST / 'REFERENCE.json').write_text(json.dumps({
        'file': '2-Foto-2.jpg', 'sha256': hashlib.sha256(args.reference.read_bytes()).hexdigest(),
        'size': list(source.size), 'regions': REGIONS,
        'gear': 'assets_local/navigation-icons/Gear.svg',
    }, indent=2) + '\n', 'utf8')


if __name__ == '__main__':
    main()
