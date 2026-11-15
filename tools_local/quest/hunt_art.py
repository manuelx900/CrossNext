"""Prepare the hunt sprite atlas for the existing rewards pack."""

from PIL import Image, ImageDraw, ImageChops
from bestiary_catalog import load


def trim_boundary_fragments(cell):
    """Exclude tiny pieces of a neighbouring atlas cell from sprite extraction."""
    remaining = cell.getchannel('A').point(lambda a: 255 if a >= 150 else 0)
    components = []
    while remaining.getbbox():
        index = remaining.tobytes().index(255)
        prior = remaining.copy()
        ImageDraw.floodfill(remaining, (index % cell.width, index // cell.width), 0)
        component = ImageChops.subtract(prior, remaining)
        components.append((component.histogram()[255], component.getbbox()))
    largest = max(area for area, _ in components)
    alpha = cell.getchannel('A')
    draw = ImageDraw.Draw(alpha)
    for area, (left, top, right, bottom) in components:
        boundary = left == 0 or top == 0 or right == cell.width or bottom == cell.height
        if boundary and area * 20 < largest:
            draw.rectangle((max(0, left - 1), max(0, top - 1), right, bottom), fill=0)
    cell.putalpha(alpha)
    return cell


def cells(path, columns, rows, trim_edges=False):
    with Image.open(path) as source:
        atlas = source.convert("RGBA")
    for row in range(rows):
        for column in range(columns):
            cell = atlas.crop((
                column * atlas.width // columns, row * atlas.height // rows,
                (column + 1) * atlas.width // columns, (row + 1) * atlas.height // rows,
            ))
            if trim_edges:
                cell = trim_boundary_fragments(cell)
            opaque = cell.getchannel("A").point(lambda value: 255 if value >= 150 else 0)
            bounds = opaque.getbbox()
            if not bounds:
                raise ValueError(f"{path.name}: empty sprite at {column}, {row}")
            yield cell.crop(bounds)


def centered(sprite, size, bottom):
    canvas = Image.new("RGBA", size)
    canvas.alpha_composite(sprite, ((size[0] - sprite.width) // 2, bottom - sprite.height))
    return canvas


def beast_frames(source):
    poses = list(cells(source, 3, 4))
    for row in range(4):
        frames = poses[row * 3 : row * 3 + 3]
        scale = min(236 / max(frame.width for frame in frames), 264 / max(frame.height for frame in frames))
        for frame in frames:
            fitted = frame.resize((round(frame.width * scale), round(frame.height * scale)), Image.Resampling.LANCZOS)
            yield centered(fitted, (248, 280), 272)


def bestiary_frames(directory):
    entries = load(directory)
    sheets = {}
    for entry in entries:
        key = entry['sheet']
        if key not in sheets:
            sheets[key] = list(cells(directory / f'{key}-v1.png', 6, 4, trim_edges=True))
        start = entry['row'] * 6 + entry['column']
        frames = sheets[key][start:start + 3]
        scale = min(236 / max(frame.width for frame in frames), 264 / max(frame.height for frame in frames))
        for frame in frames:
            fitted = frame.resize((round(frame.width * scale), round(frame.height * scale)), Image.Resampling.LANCZOS)
            yield centered(fitted, (248, 280), 272)


def relic():
    image = Image.new("RGBA", (112, 144))
    draw = ImageDraw.Draw(image)
    draw.arc((15, 6, 97, 83), 3, 177, "black", width=6)
    draw.ellipse((48, 72, 64, 94), fill="white", outline="black", width=4)
    diamond = [(56, 88), (91, 110), (56, 140), (21, 110)]
    draw.polygon(diamond, fill="white")
    draw.line(diamond + [diamond[0]], fill="black", width=6, joint="curve")
    drop = [(57, 97), (66, 109), (65, 118), (57, 125), (49, 119), (48, 111)]
    draw.polygon(drop, fill="black")
    draw.ellipse((55, 106, 59, 111), fill="white")
    return image


def records(directory):
    beasts = list(beast_frames(directory / "beasts-v2.png"))
    by_row = list(cells(directory / "outfits-v2.png", 4, 2))
    figures = []
    for index in range(4):
        for source in (by_row[index], by_row[index + 4]):
            source.thumbnail((155, 202), Image.Resampling.LANCZOS)
            figures.append(centered(source, (160, 208), 205))
    accessory = relic()
    return [
        *((image, False) for image in beasts),
        *((image, True) for image in beasts),
        *((image, False) for image in figures),
        *((image, True) for image in figures),
        (accessory, False),
        (accessory, True),
    ]
