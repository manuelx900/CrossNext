#!/usr/bin/env python3
"""Flatten Image Occlusion Enhanced masks into ordinary PNG card faces.

The original APKG is never modified. Card IDs and non-occlusion notes are kept.
Run on a computer with Pillow and resvg-py; the reader itself needs neither.
    python prepare_occlusion.py source.apkg prepared.apkg
"""
import argparse
import copy
import hashlib
import html
import io
import json
from pathlib import Path
import re
import sqlite3
import tempfile
import zipfile
import xml.etree.ElementTree as ET

from PIL import Image
import resvg_py

IMG = re.compile(r'''<img\b[^>]*\bsrc\s*=\s*(?:"([^"]+)"|'([^']+)'|([^\s>]+))''', re.I)
MAX_W, MAX_H = 448, 620


def image_name(field):
    match = IMG.search(field)
    if not match:
        raise ValueError("Occlusion card has no image reference")
    return html.unescape(next(part for part in match.groups() if part is not None))


def composite(original, mask):
    base = Image.open(io.BytesIO(original)).convert("RGBA")
    base.thumbnail((MAX_W, MAX_H), Image.Resampling.LANCZOS)
    root = ET.fromstring(mask)
    # Enhanced masks use a square canvas even for a portrait source. Keep that
    # coordinate system: resizing it to the photograph's aspect distorts masks.
    width = float(root.attrib["width"].removesuffix("px"))
    height = float(root.attrib["height"].removesuffix("px"))
    if width <= 0 or height <= 0 or base.width <= 0:
        raise ValueError("Invalid occlusion canvas")
    scale = base.width / Image.open(io.BytesIO(original)).width
    render_w, render_h = max(1, round(width * scale)), max(1, round(height * scale))
    # Opaque black marks the target; opaque white hides other labels. This
    # retains both kinds of mask on a monochrome display.
    for shape in root.iter():
        if shape.attrib.get("fill", "").lower() == "#ff7e7e":
            shape.set("fill", "#000000")
        elif shape.attrib.get("fill", "").lower() == "#ffeba2":
            shape.set("fill", "#ffffff")
    svg = ET.tostring(root, encoding="unicode")
    # No filesystem/network references are allowed in a downloaded SVG.
    for element in root.iter():
        for key, value in element.attrib.items():
            if key.rsplit("}", 1)[-1] == "href" and not value.startswith("#"):
                raise ValueError("External SVG resources are unsupported")
    overlay = Image.open(io.BytesIO(resvg_py.svg_to_bytes(
        svg_string=svg, width=render_w, height=render_h, skip_system_fonts=True))).convert("RGBA")
    base.alpha_composite(overlay, (0, 0))
    flattened = Image.new("RGB", base.size, "white")
    flattened.paste(base, mask=base.getchannel("A"))
    result = io.BytesIO()
    flattened.save(result, "PNG")
    return result.getvalue()


def prepare(source, destination):
    source, destination = Path(source), Path(destination)
    if source.resolve() == destination.resolve():
        raise ValueError("Choose a different output path; the original must be kept")
    with zipfile.ZipFile(source) as archive, tempfile.TemporaryDirectory() as scratch:
        key = next(k for k in ("collection.anki21b", "collection.anki21", "collection.anki2") if k in archive.namelist())
        data = archive.read(key)
        if data[:4] == bytes.fromhex("28b52ffd"):
            import zstandard
            data = zstandard.ZstdDecompressor().decompress(data, max_output_size=256*1024*1024)
        collection = Path(scratch)/"collection.anki2"
        collection.write_bytes(data)
        db = sqlite3.connect(collection)
        models = json.loads(db.execute("select models from col").fetchone()[0])
        media = json.loads(archive.read("media"))
        lookup = {name: key for key, name in media.items()}
        pictures = {}
        # A derived package is valid Basic Anki as well as valid firmware input.
        original_model = next(iter(models.values()))
        flattened_id = max(int(k) for k in models) + 1
        basic = copy.deepcopy(original_model)
        basic.update(id=flattened_id, name="CrossNext Image Occlusion PNG", type=0,
                     req=[[0,"any",[0]]])
        basic["flds"] = [dict(original_model["flds"][0], name="Front", ord=0),
                         dict(original_model["flds"][1], name="Back", ord=1)]
        basic["tmpls"] = [dict(original_model["tmpls"][0], name="Card 1", ord=0,
                              qfmt="{{Front}}", afmt='{{FrontSide}}<hr id="answer">{{Back}}')]
        models[str(flattened_id)] = basic
        converted = 0
        for nid, mid, fields in db.execute("select id,mid,flds from notes").fetchall():
            names = [f["name"].casefold() for f in models[str(mid)]["flds"]]
            if not {"question mask", "answer mask", "image"}.issubset(names):
                continue
            parts = fields.split("\x1f")
            values = dict(zip(names, parts))
            original = archive.read(lookup[image_name(values["image"])])
            faces = []
            for side in ("question", "answer"):
                mask = archive.read(lookup[image_name(values[side+" mask"])])
                name = f"crossnext-{nid}-{side}.png"
                pictures[name] = composite(original, mask)
                caption = values.get("header", "") if side == "question" else values.get("remarks", "")
                if side == "answer":
                    for extra in ("extra 1", "extra 2", "sources"):
                        if values.get(extra, "").strip():
                            caption += "<div>"+values[extra]+"</div>"
                # Keep optional question/footer and answer explanation as text.
                if side == "question" and values.get("footer", "").strip():
                    caption += "<div>"+values.get("footer", "")+"</div>"
                faces.append(f'<img src="{name}">' + ("<div>"+caption+"</div>" if caption.strip() else ""))
            db.execute("update notes set flds=?,mid=? where id=?", ("\x1f".join(faces),flattened_id,nid))
            converted += 1
        db.execute("update col set models=?", (json.dumps(models),))
        db.commit()
        db.close()
        destination.parent.mkdir(parents=True, exist_ok=True)
        temporary = destination.with_suffix(destination.suffix+".part")
        with zipfile.ZipFile(temporary,"w",zipfile.ZIP_DEFLATED) as output:
            output.write(collection,"collection.anki2")
            # Non-occlusion cards retain their original media as well.
            for entry, name in media.items():
                output.writestr(entry,archive.read(entry))
            next_id = max((int(n) for n in media), default=-1)+1
            for name, data in pictures.items():
                entry = str(next_id); next_id += 1
                media[entry]=name
                output.writestr(entry,data)
            output.writestr("media",json.dumps(media,ensure_ascii=False))
        temporary.replace(destination)
    return {"occlusion_notes":converted,"faces":len(pictures),
            "source_sha256":hashlib.sha256(source.read_bytes()).hexdigest(),
            "prepared_sha256":hashlib.sha256(destination.read_bytes()).hexdigest()}


if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source",type=Path)
    parser.add_argument("destination",type=Path)
    args=parser.parse_args()
    print(json.dumps(prepare(args.source,args.destination),indent=2))
