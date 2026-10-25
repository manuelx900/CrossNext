import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("pet_resources", ROOT / "tools_local/pet/build_resources.py")
pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)


class ResourceTest(unittest.TestCase):
    def test_empty_slots_and_crc(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "graphics.bin"
            info = pack.build(output)
            binary = output.read_bytes()
            magic, count, size, crc, index_crc = struct.unpack_from("<8sIIII", binary)
            self.assertEqual((magic, count, size), (b"CNART002", 64, len(binary)))
            self.assertEqual(zlib.crc32(binary[24:]), crc)
            self.assertEqual(zlib.crc32(binary[24:24 + count * 16]), index_crc)
            self.assertEqual(len({item["name"] for item in info["images"]}), 64)
            for n in range(count):
                offset, width, height, length, checksum = struct.unpack_from("<IHHII", binary, 24 + n * 16)
                self.assertEqual((width, height, length), (1, 1, 1))
                self.assertEqual(binary[offset:offset + length], b"\x00")
                self.assertEqual(zlib.crc32(binary[offset:offset + length]), checksum)

    def test_home_slots(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "home.bin"
            info = pack.build(output, home=True)
            self.assertEqual(info["sprites"],45)
            self.assertEqual(info["images"][3]["name"],"home-silva-newborn-awake")
            self.assertEqual(info["images"][38]["name"],"home-ember-adult-sick")
            self.assertEqual([i['name'] for i in info['images'][39:]],
                             [f'background-{s}-{h}' for s in ('silva', 'nox', 'ember') for h in (0, 1)])
            self.assertTrue(all((i['width'], i['height']) == (224, 144) for i in info['images'][39:]))
            self.assertTrue(output.with_name("home-manifest.json").is_file())

    def test_complete_supplied_collection(self):
        from PIL import Image
        source = ROOT / "assets_local/pet/png"
        for home in (False, True):
            expected = pack.home_names() if home else pack.names()
            filename = "home.bin" if home else "graphics.bin"
            with tempfile.TemporaryDirectory() as temporary:
                output = Path(temporary) / filename
                info = pack.build(output, source, home)
                binary = output.read_bytes()
                self.assertEqual(binary, (ROOT / "resources/apps/pet" / filename).read_bytes())
                self.assertTrue(all(item["supplied"] for item in info["images"]))
                self.assertEqual(zlib.crc32(binary[24:]), struct.unpack_from("<I", binary, 16)[0])
                self.assertEqual(zlib.crc32(binary[24:24 + info['sprites'] * 16]), struct.unpack_from("<I", binary, 20)[0])
                for n, name in enumerate(expected):
                    offset, width, height, length, checksum = struct.unpack_from("<IHHII", binary, 24 + n * 16)
                    self.assertGreater(width, 1)
                    self.assertGreater(height, 1)
                    self.assertLessEqual(width, 256)
                    self.assertLessEqual(height, 256)
                    self.assertEqual(length, (width + 7) // 8 * height)
                    pixels = binary[offset:offset + length]
                    self.assertEqual(zlib.crc32(pixels), checksum)
                    if name == "ui-petting-hand":
                        self.assertLessEqual(width, 144)
                        self.assertLessEqual(height, 100)
                        continue
                    with Image.open(source / (name + ".png")) as image:
                        self.assertEqual(image.mode, "1")
                        left, top, packed_width, packed_height = info["images"][n]["source_bounds"]
                        self.assertEqual((packed_width, packed_height), (width, height))
                        for y in range(height):
                            for x in range(width):
                                ink = bool(pixels[y * ((width + 7) // 8) + x // 8] & (0x80 >> (x % 8)))
                                self.assertEqual(ink, image.getpixel((left + x, top + y)) == 0)
                        # The export may discard white canvas, never ink or scale original pixels.
                        self.assertEqual(sum(byte.bit_count() for byte in pixels),
                                         sum(image.getpixel((x,y)) == 0 for y in range(256) for x in range(256)))
                if home:
                    for image in info['images'][39:]:
                        offset, width, height, length, checksum = struct.unpack_from('<IHHII', binary, 24 + image['id'] * 16)
                        self.assertEqual((width, height, length), (224, 144, 4032))
                        self.assertEqual(zlib.crc32(binary[offset:offset + length]), checksum)
                        self.assertTrue(any(binary[offset:offset + length]))
        for species in pack.SPECIES:
            self.assertEqual((source / (species + "-egg.png")).read_bytes(),
                             (source / ("home-" + species + "-egg.png")).read_bytes())
            for stage in pack.STAGES:
                self.assertEqual((source / f"{species}-{stage}-normal.png").read_bytes(),
                                 (source / f"home-{species}-{stage}-awake.png").read_bytes())

    def test_supplied_art_uses_ink_bits(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            image = Image.new("1", (9, 3), 1)
            black = {(0, 0), (8, 1), (3, 2)}
            for point in black:
                image.putpixel(point, 0)
            image.save(source / "silva-egg.png")
            output = source / "graphics.bin"
            info = pack.build(output, source)
            binary = output.read_bytes()
            offset, width, height, length, checksum = struct.unpack_from("<IHHII", binary, 24)
            self.assertTrue(info["images"][0]["supplied"])
            self.assertEqual((width, height, length), (9, 3, 6))
            self.assertEqual(zlib.crc32(binary[offset:offset + length]), checksum)
            for y in range(height):
                for x in range(width):
                    ink = bool(binary[offset + y * 2 + x // 8] & (0x80 >> (x % 8)))
                    self.assertEqual(ink, (x, y) in black)


if __name__ == "__main__":
    unittest.main()
