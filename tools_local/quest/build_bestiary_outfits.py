"""Pack the 56 original bestiary outfits, male/female, at native avatar size."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
from PIL import Image, ImageDraw, ImageChops, ImageFilter

sys.path.insert(0, str(Path(__file__).parent))
try:
    from bestiary_catalog import load
    from hunt_art import centered
    from build_expansion_art import write_pack
finally:
    sys.path.pop(0)

ROOT = Path(__file__).resolve().parents[2]


def avatar_cells(path):
    """Extract complete figures even when a generated row drifts across a grid line."""
    atlas = Image.open(path).convert('RGBA')
    alpha = atlas.getchannel('A')
    remaining = alpha.point(lambda a: 255 if a >= 150 else 0)
    components = []
    while remaining.getbbox():
        index = remaining.tobytes().index(255)
        prior = remaining.copy()
        ImageDraw.floodfill(remaining, (index % atlas.width, index // atlas.width), 0)
        mask = ImageChops.subtract(prior, remaining)
        bounds = mask.getbbox()
        components.append((mask.histogram()[255], bounds, mask.crop(bounds)))
    components.sort(key=lambda component: component[0], reverse=True)
    main, extras = components[:16], components[16:]
    assert len(main) == 16 and main[-1][0] > atlas.width * atlas.height // 1000, path
    ordered = {}
    for component in main:
        _, (left, top, right, bottom), _ = component
        cell = min(3, (top + bottom) * 2 // atlas.height) * 4 + min(3, (left + right) * 2 // atlas.width)
        assert cell not in ordered, (path, cell)
        ordered[cell] = [component]
    for component in extras:
        _, bounds, _ = component
        left, top, right, bottom = bounds
        def distance(cell):
            _, (a,b,c,d), _ = ordered[cell][0]
            dx, dy = max(a-right, left-c, 0), max(b-bottom, top-d, 0)
            return dx*dx + dy*dy
        cell = min(ordered, key=distance)
        if distance(cell) <= (min(atlas.width, atlas.height) // 20) ** 2:
            ordered[cell].append(component)
    for cell in range(16):
        selection = Image.new('L', atlas.size)
        for _, bounds, mask in ordered[cell]:
            selection.paste(mask, bounds)
        selection = ImageChops.multiply(selection.filter(ImageFilter.MaxFilter(3)), alpha)
        bounds = selection.point(lambda a:255 if a>=150 else 0).getbbox()
        sprite = atlas.crop(bounds)
        sprite.putalpha(selection.crop(bounds))
        yield sprite


def validate_masters(directory):
    provenance = json.loads((directory / 'provenance.json').read_text('utf8'))
    expected = {f'{entry["sheet"]}-v1.png' for entry in load(ROOT / 'assets_local/quest/bestiary')}
    if {master['master'] for master in provenance['masters']} != expected:
        raise ValueError('Outfit master provenance does not match the bestiary')
    for master in provenance['masters']:
        if hashlib.sha256((directory / master['master']).read_bytes()).hexdigest() != master['sha256']:
            raise ValueError(f'Outfit master checksum mismatch: {master["master"]}')


def figures():
    entries = load(ROOT / 'assets_local/quest/bestiary')
    directory = ROOT / 'assets_local/quest/bestiary-outfits'
    validate_masters(directory)
    sheets = {}
    for index, entry in enumerate(entries):
        key = entry['sheet']
        if key not in sheets:
            sheets[key] = list(avatar_cells(directory / f'{key}-v1.png'))
        offset = index % 8 * 2
        for source in sheets[key][offset:offset + 2]:
            source.thumbnail((152, 198), Image.Resampling.LANCZOS)
            yield centered(source, (160, 208), 204)


def build(output):
    output.mkdir(parents=True, exist_ok=True)
    avatars = list(figures())
    assert len(avatars) == 112
    records = [(avatar, False) for avatar in avatars] + [(avatar, True) for avatar in avatars]
    manifest = write_pack(output / 'bestiary-outfits.bin', records, False)
    (output / 'bestiary-outfits-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', 'utf8')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'resources/apps/reading-adventure-v2')
    build(parser.parse_args().output)
