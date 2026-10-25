#!/usr/bin/env python3
"""Convert the user-supplied pixel atlas into seven immutable firmware portraits."""
import io
import sys
from pathlib import Path

from PIL import Image


def outputs(root):
    atlas = Image.open(root / "assets_local/carnivoro/animals-atlas.png").convert("L")
    lines = ["#pragma once", "", "#include <Icon.h>", "", "namespace carnivoroui {"]
    files = {}
    width, height = atlas.width / 4, atlas.height / 2
    for index in range(7):
        x, y = index % 4, index // 4
        image = atlas.crop((int(x * width + 6), int(y * height + 6),
                            int((x + 1) * width - 6), int((y + 1) * height - 6)))
        image = image.resize((160, 160), Image.Resampling.NEAREST).point(lambda p: 255 if p >= 155 else 0, "1")
        png = io.BytesIO()
        image.save(png, format="PNG")
        files[f"assets_local/carnivoro/animal-{index}.png"] = png.getvalue()
        data = image.tobytes()
        lines.append(f"inline constexpr uint8_t animal{index}Bits[] = {{")
        lines.extend("    " + ", ".join(f"0x{b:02x}" for b in data[n:n + 19]) + "," for n in range(0, len(data), 19))
        lines.extend(["};", f"inline constexpr freeink::Icon animal{index} = {{160, 160, 80, animal{index}Bits}};"])
    lines.extend(["inline constexpr const freeink::Icon* portraits[] = {&animal0, &animal1, &animal2, &animal3,", "                                                     &animal4, &animal5, &animal6};", "}  // namespace carnivoroui", ""])
    files["src/apps_local/carnivoro/CarnivoroArt.h"] = "\n".join(lines).encode()
    return files


def main():
    root = Path(__file__).resolve().parent.parent
    files = outputs(root)
    if "--check" in sys.argv:
        stale = [name for name, data in files.items() if not (root / name).is_file() or (root / name).read_bytes() != data]
        for name in stale:
            print("stale:", name, file=sys.stderr)
        return bool(stale)
    for name, data in files.items():
        (root / name).write_bytes(data)
    print("7 portraits, 22,400 immutable flash bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
