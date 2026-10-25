"""Extract the reviewed Murdle reference crops into uniform one-bit flash inputs."""
from pathlib import Path
import hashlib
import json
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / "assets_local/murdle"


def main():
    manifest = json.loads((ART / "sprites.json").read_text(encoding="utf-8"))
    width, height = manifest["canvas"]
    inset = manifest["inset"]
    outputs = []
    for kind, expected in [("weapons", 20), ("places", 16), ("motives", 12)]:
        entries = manifest[kind]
        assert len(entries) == expected
        assert [entry["index"] for entry in entries] == list(range(expected))
        source = ART / manifest["sources"][kind]
        image = Image.open(source).convert("RGBA")
        for entry in entries:
            x0, y0, x1, y1 = entry["crop"]
            assert 0 <= x0 < x1 <= image.width and 0 <= y0 < y1 <= image.height
            cropped = image.crop((x0, y0, x1, y1))
            cropped.putalpha(cropped.getchannel("A").point(
                lambda value: 255 if value >= manifest["alpha_threshold"] else 0))
            # Transparent exterior becomes white paper; opaque white interiors stay white.
            paper = Image.new("RGBA", cropped.size, "white")
            paper.alpha_composite(cropped)
            mono = paper.convert("L").point(lambda value: 255 if value >= manifest["ink_threshold"] else 0, mode="1")
            scale = min((width - 2 * inset) / mono.width, (height - 2 * inset) / mono.height)
            size = (max(1, round(mono.width * scale)), max(1, round(mono.height * scale)))
            sprite = mono.resize(size, Image.Resampling.NEAREST)
            canvas = Image.new("1", (width, height), 1)
            canvas.paste(sprite, ((width - size[0]) // 2, (height - size[1]) // 2))
            target = ART / (entry["symbol"] + ".png")
            canvas.save(target)
            outputs.append({"category": kind, "id": entry["id"], "symbol": entry["symbol"],
                            "source_crop": entry["crop"], "scaled_size": size,
                            "sha256": hashlib.sha256(target.read_bytes()).hexdigest()})
    menu = manifest["menu"]
    image = Image.open(ART / menu["source"]).convert("RGBA")
    paper = Image.new("RGBA", image.size, "white")
    paper.alpha_composite(image)
    mono = paper.convert("L").point(lambda value: 255 if value >= manifest["ink_threshold"] else 0, mode="1")
    menu_width, menu_height = menu["size"]
    scale = min(menu_width / mono.width, menu_height / mono.height)
    size = (max(1, round(mono.width * scale)), max(1, round(mono.height * scale)))
    canvas = Image.new("1", (menu_width, menu_height), 1)
    canvas.paste(mono.resize(size, Image.Resampling.NEAREST),
                 ((menu_width - size[0]) // 2, (menu_height - size[1]) // 2))
    canvas.save(ART / "Menu.png")
    (ART / "sprites-export.json").write_text(json.dumps(outputs, indent=2) + "\n", encoding="utf-8")
    print("Exported 20 weapon, 16 place and 12 motive sprites, 96x96 one-bit canvases, nearest-neighbor")


if __name__ == "__main__":
    main()
