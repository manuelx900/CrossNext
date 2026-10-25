"""Pack the supplied Care reference icons into the existing one-bit Icon format."""
from pathlib import Path
import json

from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "assets_local/pet/ui/care-reference.png"
METADATA = ROOT / "assets_local/pet/ui/care-icons.json"
OUTPUT = ROOT / "src/apps_local/pet/PetCareIcons.h"


def main():
    source = Image.open(SOURCE).convert("L")
    metadata = json.loads(METADATA.read_text("utf-8"))
    lines = ["#pragma once", "", "#include <Icon.h>", "",
             "// Generated from the supplied reference by tools_local/pet/build_care_icons.py.",
             "namespace petui::careicons {", "// clang-format off"]
    for name, box in metadata["crops"].items():
        crop = source.crop(box).point(lambda value: 255 if value >= 128 else 0, mode="1")
        bounds = ImageOps.invert(crop.convert("L")).getbbox()
        assert bounds, name
        crop = crop.crop(bounds)
        crop.thumbnail((62, 62), Image.Resampling.NEAREST)
        canvas = Image.new("1", (64, 64), 1)
        canvas.paste(crop, ((64 - crop.width) // 2, (64 - crop.height) // 2))
        ink = [(y, sum(canvas.getpixel((x, y)) == 0 for x in range(64))) for y in range(64)]
        center = round(sum(y * count for y, count in ink) / sum(count for _, count in ink))
        data = canvas.tobytes()
        assert len(data) == 512
        lines.append(f"inline constexpr uint8_t {name}_BITS[] = {{")
        for at in range(0, len(data), 16):
            lines.append("    " + ", ".join(f"0x{byte:02x}" for byte in data[at:at + 16]) + ",")
        lines.extend(["};", f"inline constexpr freeink::Icon {name} = {{64, 64, {center}, {name}_BITS}};", ""])
    lines.extend(["// clang-format on", "}  // namespace petui::careicons", ""])
    OUTPUT.write_text("\n".join(lines), "utf-8")
    print(f"Packed {len(metadata['crops'])} reference icons: {len(metadata['crops']) * 512} flash bitmap bytes")


if __name__ == "__main__":
    main()
