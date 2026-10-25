#!/usr/bin/env python3
"""Build the masked PET room pack and its fixed layout metadata from the supplied ZIP artwork."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import zlib
from PIL import Image, ImageDraw, ImageOps

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'assets_local/pet/hogar'
ANCHORS = {'descanso': 0, 'adorno_suelo': 1, 'adorno_pared': 2, 'adorno_techo': 3,
           'juego': 4, 'entorno_suelo': 5, 'entorno_pared': 6}


def monochrome(image, size=None):
    gray = image.convert('L')
    if size:
        gray = gray.resize(size, Image.Resampling.BOX)
    return gray.point(lambda p: 0 if p < 160 else 255, mode='1')


def crop_ink(image):
    box = ImageOps.invert(image.convert('L')).getbbox()
    if not box:
        raise ValueError('Artwork has no visible ink')
    return image.crop(box), list(box)


def silhouette(image, voids=()):
    # Only white connected to the exterior is transparent. Enclosed white is opaque.
    ink = ImageOps.invert(image.convert('L'))
    padded = ImageOps.expand(ink, border=1, fill=0)
    ImageDraw.floodfill(padded, (0, 0), 128, thresh=0)
    for x, y in voids:
        point = (1 + image.width*x//100, 1 + image.height*y//100)
        if padded.getpixel(point) == 0:
            ImageDraw.floodfill(padded, point, 128, thresh=0)
    mask = padded.crop((1, 1, image.width + 1, image.height + 1)).point(lambda p: 0 if p == 128 else 255)
    return mask.convert('1')


def bits(image, mask=False):
    return (image if mask else ImageOps.invert(image.convert('L')).convert('1')).tobytes()


def build(output=ROOT / 'resources/apps/pet/habitat.bin', metadata=ROOT / 'src/apps_local/pet/PetHabitatData.h'):
    spec = importlib.util.spec_from_file_location('pet_original_resources', Path(__file__).with_name('build_resources.py'))
    original = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(original)
    rows = json.loads((SOURCE / 'catalogo-integrado.json').read_text(encoding='utf-8'))
    rest_poses = json.loads((SOURCE / 'rest-poses.json').read_text(encoding='utf-8'))
    rest_names = {row['id'] for row in rows if row['categoria'] == 'descanso'}
    if not set(rest_poses).issubset(rest_names) or any(
            value.get('pose') != 'head' or not 20 <= value.get('opening_height', 0) <= 70
            for value in rest_poses.values()):
        raise ValueError('Invalid shelter pose metadata')
    if len(rows) != 240 or [r['firmware_id'] for r in rows] != list(range(240)):
        raise ValueError('Catalog must contain exactly 240 stable IDs')
    png = SOURCE / 'firmware-png'
    png.mkdir(exist_ok=True)
    sprites, manifest, objects, pets = [], [], [], []

    def add(image, name, mask=False):
        if image.width > 280 or image.height > 512:
            raise ValueError(f'Oversized sprite: {name}')
        payload = bits(image, mask)
        sprites.append((image.width, image.height, payload))
        manifest.append({'id': len(sprites)-1, 'name': name, 'width': image.width, 'height': image.height})

    background = monochrome(Image.open(SOURCE/'hogar-base.png'), (432, 216))
    background.save(png/'hogar-base.png')
    for x in (0, 216):
        add(background.crop((x, 0, x+216, 216)), f'room-{x//216}')
    sheets = {}
    for row in rows:
        name = row['id']
        if row['lamina'] not in sheets:
            sheets[row['lamina']] = Image.open(SOURCE/row['lamina']).convert('L')
        x, y, w, h = (row[k] for k in ('real_x','real_y','real_width','real_height'))
        image, bounds = crop_ink(monochrome(sheets[row['lamina']].crop((x,y,x+w,y+h))))
        max_w, max_h = row['max_ancho_pantalla'], row['max_alto_pantalla']
        ratio = min(max_w/image.width, max_h/image.height, 1)
        image = monochrome(image, (max(2,round(image.width*ratio)), max(2,round(image.height*ratio))))
        image, _ = crop_ink(image)
        image.save(png/f'{name}.png')
        voids = row.get('transparent_points', [])
        add(silhouette(image, voids), name+'-mask', True)
        add(image, name)
        sx, sy, sw, front = row.get('sleep_support', [0,0,0,0])
        rest = rest_poses.get(name, {})
        pose = 'Head' if rest.get('pose') == 'head' else 'Body'
        head_height = rest.get('opening_height', 0)
        objects.append(f'  {{StrId::STR_PET_ITEM_{row["firmware_id"]:03}, {image.width}, {image.height}, '
                       f'{ANCHORS[row["anclaje"]]}, {sx}, {sy}, {sw}, {front}, RestPose::{pose}, {head_height}}},')
        manifest[-1]['source_cell']=[x,y,w,h]
        manifest[-1]['source_ink_bounds']=bounds
    for name in original.home_names():
        image, bounds = crop_ink(monochrome(Image.open(ROOT/'assets_local/pet/png'/f'{name}.png')))
        add(silhouette(image), name+'-mask', True)
        add(image, name)
        pets.append(f'  {{{image.width}, {image.height}}},')
    assert len(sprites) == 560
    index, data = bytearray(), bytearray()
    offset = 24 + len(sprites)*16
    for width,height,payload in sprites:
        index += struct.pack('<IHHII',offset,width,height,len(payload),zlib.crc32(payload))
        data += payload
        offset += len(payload)
    binary = struct.pack('<8sIIII',b'CNART002',len(sprites),offset,zlib.crc32(index+data),zlib.crc32(index))+index+data
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_bytes(binary)
    info={'format':'CNART002','sprites':len(sprites),'bytes':len(binary),
          'sha256':hashlib.sha256(binary).hexdigest(),'images':manifest}
    output.with_name('habitat-manifest.json').write_text(json.dumps(info,indent=2)+'\n',encoding='utf-8')
    header='''#pragma once

// Generated by tools_local/pet/build_habitat.py from the supplied artwork catalog.
#include <I18n.h>

#include <cstdint>

namespace petui {
enum class RestPose : uint8_t { Body, Head };
struct ObjectArt {
  StrId name;
  uint8_t width, height, anchor, sleepX, sleepY, sleepWidth, frontY;
  RestPose restPose;
  uint8_t openingHeight;
};
// clang-format off
inline constexpr ObjectArt OBJECT_ART[] = {
'''+ '\n'.join(objects)+'''
};
struct PetArt {
  uint16_t width, height;
};
inline constexpr PetArt PET_ART[] = {
'''+ '\n'.join(pets)+'''
};
// clang-format on
}  // namespace petui
'''
    metadata.write_text(header,encoding='utf-8',newline='\n')
    print(f'PET Home: {len(rows)} accessories, 39 masked poses, {len(binary)} bytes')
    return info


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'resources/apps/pet/habitat.bin')
    parser.add_argument('--metadata',type=Path,default=ROOT/'src/apps_local/pet/PetHabitatData.h')
    args=parser.parse_args()
    build(args.output,args.metadata)
