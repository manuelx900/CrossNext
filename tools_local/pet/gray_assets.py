#!/usr/bin/env python3
"""Register PET's grayscale masters into stable, two-bit CNART003 SD packs."""
from functools import lru_cache
from collections import deque
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import zlib

from PIL import Image, ImageFilter, ImageOps

ROOT = Path(__file__).resolve().parents[2]
MASTERS = Path(os.environ.get("CROSSNEXT_PET_MASTERS", ROOT / "REDISEÑOS/PET/assets/v2"))
ORIGINAL = ROOT / "assets_local/pet"
SPECIES = ("silva", "nox", "ember")
AGES = ("newborn", "baby", "juvenile", "adult")
MOODS = ("normal", "happy", "hungry", "tired", "sick")
OBJECTS = ("bed", "hammock", "shelter", "plant", "lamp", "leaf-mobile", "ball", "tunnel", "ring",
           "leaves", "stars", "stones")


def available():
    return (MASTERS / "provenance.json").is_file()


def require_current():
    if not available():
        raise FileNotFoundError(
            "Current PET masters are missing: " + str(MASTERS) +
            ". Set CROSSNEXT_PET_MASTERS to the current v2 master folder. "
            "Use --legacy only to build the old compatibility fixtures.")


@lru_cache(maxsize=24)
def image(name):
    with Image.open(MASTERS / name) as source:
        return source.convert("RGBA")


@lru_cache(maxsize=64)
def _bands(name, axis, count):
    source = image(name)
    alpha = source.getchannel("A").point(lambda value: 255 if value >= 160 else 0)
    projection = alpha.filter(ImageFilter.MedianFilter(3)).getprojection()[axis]
    occupied = [i for i, value in enumerate(projection) if value]
    gaps = sorted(((b - a - 1, a + 1, b) for a, b in zip(occupied, occupied[1:]) if b - a > 1), reverse=True)
    if len(gaps) < count - 1 or any(gap[0] < 4 for gap in gaps[:count - 1]):
        raise ValueError("A grayscale master has no distinct sprite gutters")
    cuts = sorted((a + b) // 2 for _, a, b in gaps[:count - 1])
    limits = [0, *cuts, source.width if axis == 0 else source.height]
    return list(zip(limits, limits[1:]))


@lru_cache(maxsize=400)
def cell(name, column, row, columns, rows):
    source = image(name)
    left, right = _bands(name, 0, columns)[column] if columns > 1 else (0, source.width)
    top, bottom = _bands(name, 1, rows)[row] if rows > 1 else (0, source.height)
    result = source.crop((left, top, right, bottom))
    bounds = result.getchannel("A").point(lambda value: 255 if value >= 160 else 0).getbbox()
    if not bounds:
        raise ValueError(f"Empty gray sprite: {name}, {column}, {row}")
    return result.crop(bounds)


def _old_size(name, padding=0):
    with Image.open(ORIGINAL / "png" / (name + ".png")) as source:
        mask = source.convert("L").point(lambda value: 255 if value < 160 else 0)
        bounds = mask.getbbox()
        if not bounds:
            raise ValueError(f"Empty dimension reference: {name}")
        left, top, right, bottom = bounds
        return (min(source.width, right + padding) - max(0, left - padding),
                min(source.height, bottom + padding) - max(0, top - padding))


def fit(source, size, phase=0, padding=0):
    width, height = size
    target = Image.new("RGBA", size, (255, 255, 255, 0))
    picture = source.copy()
    picture.thumbnail((max(2, width - padding * 2), max(2, height - padding * 2)), Image.Resampling.LANCZOS)
    if phase:
        picture = picture.resize((max(2, picture.width - int(phase == 2)), max(2, picture.height - phase)),
                                  Image.Resampling.LANCZOS)
    target.alpha_composite(picture, ((width - picture.width) // 2, height - padding - picture.height))
    return target


def feeding_frames(name, column, row, columns, rows, size, by_row=False):
    """Keep both poses on one scale and one plate/feet baseline."""
    sources = [cell('../v3/feeding/' + name, column if by_row else phase,
                    phase if by_row else row, columns, rows) for phase in range(2)]
    factor = min(size[0] / max(s.width for s in sources), size[1] / max(s.height for s in sources))
    result = []
    for source in sources:
        picture = source.resize((max(2, round(source.width * factor)), max(2, round(source.height * factor))),
                                Image.Resampling.LANCZOS)
        target = Image.new('RGBA', size, (255, 255, 255, 0))
        target.alpha_composite(picture, ((size[0] - picture.width) // 2, size[1] - picture.height))
        result.append(target)
    return result


def body_metrics(source):
    """Measure the connected character, excluding detached droplets and motion marks."""
    width, height = source.size
    opaque = bytearray(source.getchannel('A').point(lambda a: 255 if a >= 160 else 0).tobytes())
    largest = []
    for at in range(len(opaque)):
        if not opaque[at]:
            continue
        opaque[at] = 0
        pending, points = deque([at]), []
        while pending:
            i = pending.popleft()
            points.append(i)
            x, y = i % width, i // width
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    xx, yy = x + dx, y + dy
                    if (dx or dy) and 0 <= xx < width and 0 <= yy < height:
                        j = yy * width + xx
                        if opaque[j]:
                            opaque[j] = 0
                            pending.append(j)
        if len(points) > len(largest):
            largest = points
    if len(largest) < 64:
        raise ValueError('Character alpha component is missing')
    bottom = max(i // width for i in largest)
    top = min(i // width for i in largest)
    feet = [i for i in largest if i // width >= bottom - max(2, (bottom - top) * 8 // 100)]
    return dict(area=len(largest), foot_x=sum(i % width for i in feet) / len(feet), foot_y=bottom)


def _registered_actions(folder, actions, max_size=(280, 512), mirrored=False):
    pictures, registrations = [], []
    for action in actions:
        for si, species in enumerate(SPECIES):
            for ai, age in enumerate(AGES):
                size = _old_size(f'home-{species}-{age}-awake')
                reference = body_metrics(fit(pet(species, age, 'normal'), size))
                sources = [cell(f'../{folder}/{action}-{frame}.png', si, ai, 3, 4) for frame in range(2)]
                if mirrored:
                    sources = [ImageOps.mirror(source) for source in sources]
                parts = [body_metrics(source) for source in sources]
                factor = math.sqrt(reference['area'] / (sum(p['area'] for p in parts) / 2))
                scaled, offsets = [], []
                for source, part in zip(sources, parts):
                    scaled.append(source.resize((max(2, round(source.width * factor)), max(2, round(source.height * factor))),
                                                Image.Resampling.LANCZOS))
                    offsets.append((round(reference['foot_x'] - part['foot_x'] * factor),
                                    round(reference['foot_y'] - part['foot_y'] * factor)))
                left = min(0, *(p[0] for p in offsets))
                top = min(0, *(p[1] for p in offsets))
                right = max(size[0], *(p[0] + im.width for p, im in zip(offsets, scaled)))
                bottom = max(size[1], *(p[1] + im.height for p, im in zip(offsets, scaled)))
                bounds = (right - left, bottom - top)
                ratio = min(1, max_size[0] / bounds[0], max_size[1] / bounds[1])
                stored = (max(2, round(bounds[0] * ratio)), max(2, round(bounds[1] * ratio)))
                registrations.append(dict(action=action, species=species, age=age, left=left, top=top,
                                          width=bounds[0], height=bounds[1], reference_width=size[0],
                                          reference_height=size[1], stored=stored, factor=factor))
                for frame, (source, offset) in enumerate(zip(scaled, offsets)):
                    target = Image.new('RGBA', bounds, (255, 255, 255, 0))
                    target.alpha_composite(source, (offset[0] - left, offset[1] - top))
                    if stored != bounds:
                        target = target.resize(stored, Image.Resampling.LANCZOS)
                    pictures.append((target, f'{species}-{age}-{action}-registered-{frame}'))
    return pictures, registrations


@lru_cache(maxsize=1)
def registered_reactions():
    return _registered_actions('v8/acciones', ('acariciar', 'limpiar', 'curar', 'jugar'))


@lru_cache(maxsize=1)
def registered_companions():
    return _registered_actions('v10/personality', ('sit', 'stretch', 'greet'))


@lru_cache(maxsize=1)
def registered_walk():
    pictures, registrations = [], []
    for right in (False, True):
        frames, entries = _registered_actions('v11/rest', ('walk',), (128, 160), right)
        pictures.extend((picture, name + ('-right' if right else '-left')) for picture, name in frames)
        registrations.extend(entries)
    return pictures, registrations


def rest_header(registrations):
    return companion_header(registrations).replace('namespace petcompanion', 'namespace petrest')


def companion_header(registrations):
    rows = ['#pragma once', '', '#include "PetReactionData.h"', '',
            '// Generated by tools_local/pet/gray_assets.py --update-registration.',
            'namespace petcompanion {', '// clang-format off',
            'inline constexpr petreact::Registration REGISTRATIONS[] = {']
    for entry in registrations:
        rows.append('    {' + ', '.join(str(entry[k]) for k in ('left', 'top', 'width', 'height', 'reference_width', 'reference_height')) + '},')
    rows += ['};', '// clang-format on', '}  // namespace petcompanion', '']
    return '\n'.join(rows)


@lru_cache(maxsize=1)
def body_references():
    references, poses = [], []

    def measure(source):
        metric = body_metrics(source)
        return (source.width, source.height, metric['foot_y'],
                round(math.sqrt(metric['area']) * 256), round(metric['foot_x'] * 256))

    for species in SPECIES:
        for age in AGES:
            references.append(measure(fit(pet(species, age, 'normal'), _old_size(f'home-{species}-{age}-awake'))))
            for mood in (*MOODS, 'resting'):
                name = f'home-{species}-{age}-{mood}' if mood in ('sick', 'resting') else f'{species}-{age}-{mood}'
                poses.append(measure(fit(pet(species, age, mood), _old_size(name))))
    return references, poses


def reaction_header(registrations):
    rows = ['#pragma once', '', '#include <cstdint>', '',
            '// Generated by tools_local/pet/gray_assets.py --update-registration.',
            '// Bounds use reference-pose pixels, independent of bitmap downsampling.',
            'namespace petreact {', 'struct Registration {',
            '  int16_t left, top;', '  uint16_t width, height, referenceWidth, referenceHeight;', '};',
            '// clang-format off', 'inline constexpr Registration REGISTRATIONS[] = {']
    for entry in registrations:
        values = ', '.join(str(entry[k]) for k in ('left', 'top', 'width', 'height', 'reference_width', 'reference_height'))
        rows.append('    {' + values + '},')
    rows += ['};', '// clang-format on', 'struct BodyReference {',
             '  uint16_t width, height, footY;',
             '  uint32_t bodySizeQ8, footXQ8;', '};', '// clang-format off']
    for name, entries in zip(('REFERENCES', 'BODIES'), body_references()):
        rows.append('inline constexpr BodyReference ' + name + '[] = {')
        for entry in entries:
            rows.append('    {' + ', '.join(str(value) for value in entry) + '},')
        rows.append('};')
    rows += ['// clang-format on', '}  // namespace petreact', '']
    return '\n'.join(rows)


def update_registration():
    _, registrations = registered_reactions()
    path = ROOT / 'src/apps_local/pet/PetReactionData.h'
    text = reaction_header(registrations)
    if not path.exists() or path.read_text(encoding='utf-8') != text:
        path.write_text(text, encoding='utf-8')
    path = ROOT / 'REDISEÑOS/PET/assets/v8/acciones/registration.json'
    path.write_text(json.dumps(dict(method='connected alpha area and shared feet anchor',
                                   legacy_records=822, records=1014, entries=registrations,
                                   references=body_references()[0], poses=body_references()[1]), indent=2) + '\n', encoding='utf-8')
    _, companions = registered_companions()
    header = ROOT / 'src/apps_local/pet/PetCompanionData.h'
    header.write_text(companion_header(companions), encoding='utf-8')
    destination = ROOT / 'REDISEÑOS/PET/assets/v10/personality/registration.json'
    destination.write_text(json.dumps(dict(method='connected alpha area and shared feet anchor', entries=companions), indent=2) + '\n', encoding='utf-8')
    _, walking = registered_walk()
    (ROOT / 'src/apps_local/pet/PetRestData.h').write_text(rest_header(walking), encoding='utf-8')
    (ROOT / 'REDISE\u00d1OS/PET/assets/v11/rest/registration.json').write_text(json.dumps(dict(method='shared body scale and feet; return sprites mirrored before registration', entries=walking), indent=2) + '\n', encoding='utf-8')
    print('Registered reactions, companion poses and 24 directional walk pairs')


def pet(species, age, mood):
    if mood == "hungry":
        return cell("../v5/hambre-cuenco.png", SPECIES.index(species), AGES.index(age), 3, 4)
    if species == "silva" and age == "adult" and mood == "tired":
        return cell("retoques/silva-adulto-cansado-corregido.png", 0, 0, 1, 1)
    if species == "silva" and age == "newborn" and mood == "happy":
        return cell("retoques/silva-recien-nacido-feliz-contorno.png", 0, 0, 1, 1)
    column = 5 if mood == "resting" else MOODS.index(mood)
    return cell(species + "-etapas-final.png", column, AGES.index(age), 6, 4)


def egg(species, phase=0):
    return cell("../v12/huevos-contorno.png", (1, 0, 2)[phase], SPECIES.index(species), 3, 3)


@lru_cache(maxsize=1)
def catalog():
    rows = json.loads((ORIGINAL / "hogar/catalogo-integrado.json").read_text(encoding="utf-8"))
    if len(rows) != 240 or [row["firmware_id"] for row in rows] != list(range(240)):
        raise ValueError("The furnishing catalog must retain its stable IDs")
    results = []
    for row in rows:
        with Image.open(ORIGINAL / "hogar" / row["lamina"]) as reference:
            column = min(4, int((row["real_x"] + row["real_width"] / 2) * 5 / reference.width))
            line = min(3, int((row["real_y"] + row["real_height"] / 2) * 4 / reference.height))
            x, y, width, height = (row[key] for key in ("real_x", "real_y", "real_width", "real_height"))
            old = reference.crop((x, y, x + width, y + height)).convert("L").point(
                lambda value: 0 if value < 160 else 255, "1")
            old = old.crop(ImageOps.invert(old.convert("L")).getbbox())
            ratio = min(row["max_ancho_pantalla"] / old.width, row["max_alto_pantalla"] / old.height, 1)
            old = old.resize((max(2, round(old.width * ratio)), max(2, round(old.height * ratio))),
                             Image.Resampling.BOX).convert("L").point(lambda value: 0 if value < 160 else 255, "1")
            bounds = ImageOps.invert(old.convert("L")).getbbox()
            size = bounds[2] - bounds[0], bounds[3] - bounds[1]
        # New atlases retain the catalogue's established geometry and stable IDs.
        source = row.get("grayscale_source")
        picture = (cell(source["file"], source["column"], source["row"], source["columns"], source["rows"])
                   if source else cell(row["lamina"], column, line, 5, 4))
        results.append((fit(picture, size), row["id"]))
    return results


def home_sprites():
    result = [(fit(egg(species), _old_size("home-" + species + "-egg")), "home-" + species + "-egg")
              for species in SPECIES]
    for species in SPECIES:
        for age in AGES:
            for pose in ("awake", "resting", "sick"):
                name = f"home-{species}-{age}-{pose}"
                result.append((fit(pet(species, age, "normal" if pose == "awake" else pose), _old_size(name)), name))
    return result


def room():
    source = image("habitacion-gris-final.png")
    canvas = Image.new("RGBA", (432, 216), (255, 255, 255, 255))
    # Register the illustrated window to the established room's architectural anchor.
    window = source.crop((round(source.width * .67), round(source.height * .35),
                          round(source.width * .94), round(source.height * .50)))
    window.thumbnail((80, 68), Image.Resampling.LANCZOS)
    canvas.alpha_composite(window, (338 + (80 - window.width) // 2, 10 + (68 - window.height) // 2))
    return canvas


def _coverage(source, mask=False):
    alpha = source.getchannel("A")
    gray = Image.alpha_composite(Image.new("RGBA", source.size, "white"), source).convert("L")
    values = gray.tobytes()
    opacity = alpha.tobytes()
    stride = (source.width * 2 + 7) // 8
    data = bytearray(stride * source.height)
    for y in range(source.height):
        for x in range(source.width):
            at = y * source.width + x
            value = 0 if opacity[at] < 160 else 3 if mask else min(3, (255 - values[at] + 42) // 85)
            data[y * stride + x // 4] |= value << (6 - (x % 4) * 2)
    return bytes(data)


def _write(output, sprites, kind):
    offset = 24 + len(sprites) * 16
    index, data, records = bytearray(), bytearray(), []
    for source, name, mask in sprites:
        if not 1 < source.width <= 280 or not 1 < source.height <= 512:
            raise ValueError(f"Oversized gray sprite: {name} {source.size}")
        payload = _coverage(source, mask)
        index.extend(struct.pack("<IHHII", offset, source.width, source.height, len(payload), zlib.crc32(payload)))
        data.extend(payload)
        records.append(dict(id=len(records), name=name, width=source.width, height=source.height,
                            supplied=True, pixel_format="ink2", mask=mask))
        offset += len(payload)
    payload = index + data
    binary = struct.pack("<8sIIII", b"CNART003", len(sprites), offset, zlib.crc32(payload), zlib.crc32(index)) + payload
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(binary)
    manifest = dict(format="CNART003", sprites=len(sprites), bytes=len(binary),
                    sha256=hashlib.sha256(binary).hexdigest(), images=records,
                    provenance="REDISEÑOS/PET/assets/v2/provenance.json")
    if kind in ("graphics", "home", "habitat", "breathing"):
        manifest["egg_provenance"] = "REDISE\u00d1OS/PET/assets/v12/provenance.json"
    if kind == "breathing":
        manifest["rest_provenance"] = "REDISE\u00d1OS/PET/assets/v11/rest/provenance.json"
        manifest["feeding_provenance"] = "REDISEÑOS/PET/assets/v3/feeding/provenance.json"
        manifest["food_bites_provenance"] = "REDISEÑOS/PET/assets/v4/provenance.json"
        manifest["curiosity_provenance"] = "REDISEÑOS/PET/assets/v7/curiosidad/provenance.json"
        manifest["reactions_provenance"] = "REDISEÑOS/PET/assets/v8/acciones/provenance.json"
        manifest["personality_provenance"] = "REDISEÑOS/PET/assets/v10/personality/provenance.json"
        manifest["personality_registration"] = "REDISEÑOS/PET/assets/v10/personality/registration.json"
        manifest["reactions_registration"] = "REDISEÑOS/PET/assets/v8/acciones/registration.json"
    if kind in ("graphics", "breathing"):
        manifest["hungry_provenance"] = "REDISEÑOS/PET/assets/v5/provenance.json"
    filename = "manifest.json" if kind == "graphics" else kind + "-manifest.json"
    output.with_name(filename).write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"PET gray {kind}: {len(sprites)} stable sprite slots, {len(binary)} bytes")
    return manifest


def build(kind, output):
    output = Path(output)
    sprites = []

    def add(source, name, mask=False):
        sprites.append((source, name, mask))

    def pair(source, name):
        add(source, name + "-mask", True)
        add(source, name)

    if kind == "graphics":
        for species in SPECIES:
            name = species + "-egg"
            add(fit(egg(species), _old_size(name, 8), padding=8), name)
        for species in SPECIES:
            for age in AGES:
                for mood in MOODS[:4]:
                    name = f"{species}-{age}-{mood}"
                    add(fit(pet(species, age, mood), _old_size(name, 8), padding=8), name)
        for index, name in enumerate(OBJECTS):
            add(fit(catalog()[index][0], _old_size("object-" + name, 8), padding=8), "object-" + name)
        add(fit(cell("mano.png", 0, 0, 1, 1), (144, 100)), "ui-petting-hand")
        assert len(sprites) == 64
    elif kind == "home":
        for source, name in home_sprites():
            add(source, name)
        panorama = ImageOps.fit(image("habitacion-horizontal-final.png"), (448, 144),
                                Image.Resampling.LANCZOS, centering=(.5, 1.0))
        for species in SPECIES:
            for half in range(2):
                add(panorama.crop((half * 224, 0, (half + 1) * 224, 144)), species + "-background-" + str(half))
        # Tile at panel resolution; enlarging a small sprite loses the window and plank seams.
        for name, width, height, columns, rows in (("gris", 480, 800, 2, 2), ("horizontal", 800, 480, 3, 1)):
            backdrop = image(f"habitacion-{name}-final.png").resize((width, height), Image.Resampling.LANCZOS)
            for row in range(rows):
                for column in range(columns):
                    add(backdrop.crop((column * width // columns, row * height // rows,
                                       (column + 1) * width // columns, (row + 1) * height // rows)),
                        f"room-{name}-{row}-{column}")
        assert len(sprites) == 52
    elif kind == "habitat":
        backdrop = room()
        for half in range(2):
            add(backdrop.crop((half * 216, 0, (half + 1) * 216, 216)), "room-" + str(half))
        for source, name in catalog():
            pair(source, name)
        for source, name in home_sprites():
            pair(source, name)
        assert len(sprites) == 560
    elif kind == "breathing":
        heads = []
        for species in SPECIES:
            for age in AGES:
                size = _old_size(f"home-{species}-{age}-resting")
                for phase in range(3):
                    source = fit(pet(species, age, "resting"), size, phase)
                    name = f"{species}-{age}-sleep-{phase}"
                    pair(source, name)
                    if phase == 0:
                        head_width = size[0] * {"silva": 56, "nox": 60, "ember": 58}[species] // 100
                        heads.append((source.crop((0, 0, max(2, head_width), max(2, size[1] * 78 // 100))), name))
        for source, name in heads:
            pair(source, name + "-head")
        for species in SPECIES:
            for age in AGES:
                for mood in MOODS:
                    name = f"home-{species}-{age}-sick" if mood == "sick" else f"{species}-{age}-{mood}"
                    for phase in range(3):
                        pair(fit(pet(species, age, mood), _old_size(name), phase), f"{species}-{age}-{mood}-{phase}")
        for species in SPECIES:
            for phase in range(3):
                pair(fit(egg(species, phase), _old_size(species + "-egg")), f"{species}-egg-wobble-{phase}")
        assert len(sprites) == 474
        for food in range(3):
            for phase, source in enumerate(feeding_frames('alimentos.png', food, 0, 3, 2, (96, 72), True)):
                pair(source, f'food-{food}-{phase}')
        for species in SPECIES:
            for age_index, age in enumerate(AGES):
                for phase, source in enumerate(feeding_frames(species + '-comer.png', 0, age_index, 2, 4,
                                                              _old_size(f'home-{species}-{age}-awake'))):
                    pair(source, f'{species}-{age}-eat-{phase}')
        assert len(sprites) == 534
        for food in range(6):
            sources = [cell('../v4/alimentos-mordidas.png', food, eaten, 6, 4) for eaten in range(4)]
            size = (96, 72)
            factor = min(size[0] / max(s.width for s in sources), size[1] / max(s.height for s in sources))
            for eaten, source in enumerate(sources):
                picture = source.resize((max(2, round(source.width * factor)), max(2, round(source.height * factor))),
                                        Image.Resampling.LANCZOS)
                target = Image.new('RGBA', size, (255, 255, 255, 0))
                target.alpha_composite(picture, ((size[0] - picture.width) // 2, size[1] - picture.height))
                pair(target, f'food-progressive-{food}-{eaten}')
        assert len(sprites) == 582
        for species_index, species in enumerate(SPECIES):
            for age_index, age in enumerate(AGES):
                sources = [cell('../v7/curiosidad/' + filename, species_index, age_index, 3, 4)
                           for filename in ('mirada.png', 'parpadeo.png')]
                size = _old_size(f'home-{species}-{age}-awake')
                factor = min(size[0] / max(s.width for s in sources), size[1] / max(s.height for s in sources))
                for frame, source in enumerate(sources):
                    picture = source.resize((max(2, round(source.width * factor)), max(2, round(source.height * factor))),
                                            Image.Resampling.LANCZOS)
                    target = Image.new('RGBA', size, (255, 255, 255, 0))
                    target.alpha_composite(picture, ((size[0] - picture.width) // 2, size[1] - picture.height))
                    pair(target, f'{species}-{age}-curious-{frame}')
        assert len(sprites) == 630
        for action in ('acariciar', 'limpiar', 'curar', 'jugar'):
            for species_index, species in enumerate(SPECIES):
                for age_index, age in enumerate(AGES):
                    sources = [cell(f'../v8/acciones/{action}-{frame}.png', species_index, age_index, 3, 4)
                               for frame in range(2)]
                    size = _old_size(f'home-{species}-{age}-awake')
                    factor = min(size[0] / max(s.width for s in sources), size[1] / max(s.height for s in sources))
                    for frame, source in enumerate(sources):
                        picture = source.resize((max(2, round(source.width * factor)), max(2, round(source.height * factor))),
                                                Image.Resampling.LANCZOS)
                        target = Image.new('RGBA', size, (255, 255, 255, 0))
                        target.alpha_composite(picture, ((size[0] - picture.width) // 2, size[1] - picture.height))
                        pair(target, f'{species}-{age}-{action}-{frame}')
        assert len(sprites) == 822
        pictures, registrations = registered_reactions()
        header = ROOT / 'src/apps_local/pet/PetReactionData.h'
        if not header.exists() or header.read_text(encoding='utf-8') != reaction_header(registrations):
            raise ValueError('Run python tools_local/pet/gray_assets.py --update-registration before building')
        for picture, name in pictures:
            pair(picture, name)
        assert len(sprites) == 1014
        for food in range(3):
            sources = [cell('../v10/personality/pet-foods.png', food, eaten, 3, 4) for eaten in range(4)]
            size = (96, 72)
            factor = min(size[0] / max(im.width for im in sources), size[1] / max(im.height for im in sources))
            for eaten, source in enumerate(sources):
                picture = source.resize((max(2, round(source.width * factor)), max(2, round(source.height * factor))), Image.Resampling.LANCZOS)
                target = Image.new('RGBA', size, (255, 255, 255, 0))
                target.alpha_composite(picture, ((size[0] - picture.width) // 2, size[1] - picture.height))
                pair(target, f'pet-food-{food}-{eaten}')
        assert len(sprites) == 1038
        companions, registrations = registered_companions()
        header = ROOT / 'src/apps_local/pet/PetCompanionData.h'
        if not header.exists() or header.read_text(encoding='utf-8') != companion_header(registrations):
            raise ValueError('Run gray_assets.py --update-registration for companion poses')
        for picture, name in companions:
            pair(picture, name)
        assert len(sprites) == 1182
        walking, registrations = registered_walk()
        header = ROOT / 'src/apps_local/pet/PetRestData.h'
        if not header.exists() or header.read_text(encoding='utf-8') != rest_header(registrations):
            raise ValueError('Run gray_assets.py --update-registration for rest transitions')
        for picture, name in walking:
            pair(picture, name)
        assert len(sprites) == 1278
    elif kind == "farewell":
        for index, species in enumerate(SPECIES):
            source = cell("retoques/silva-despedida-contorno.png", 0, 0, 1, 1) if index == 0 else cell(
                "despedida-contorno.png", index, 0, 3, 1)
            size = source.copy()
            size.thumbnail((208, 240), Image.Resampling.LANCZOS)
            pair(size, species)
        assert len(sprites) == 6
    elif kind == "collection":
        add(fit(cell("trofeo.png", 0, 0, 1, 1), (280, 210)), "trophy-laurels")
    else:
        raise ValueError("Unknown PET gray pack: " + kind)
    return _write(output, sprites, kind)


if __name__ == "__main__":
    import sys
    require_current()
    if sys.argv[1:] == ['--update-registration']:
        update_registration()
        sys.exit(0)
    for resource in ("graphics", "home", "habitat", "breathing", "farewell", "collection"):
        build(resource, ROOT / "resources/apps/pet-v2" / (resource + ".bin"))
