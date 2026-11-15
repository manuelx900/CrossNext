"""Validate the smooth grayscale resource pipeline and stable firmware geometry."""
import hashlib
import importlib.util
import json
import re
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("pet_gray", ROOT / "tools_local/pet/gray_assets.py")
gray = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gray)


class GrayResourcesTest(unittest.TestCase):
    def test_wall_environment_sources_and_geometry(self):
        rows = json.loads((gray.ORIGINAL / 'hogar/catalogo-integrado.json').read_text('utf-8'))
        dimensions = {int(id): (int(width), int(height)) for id, width, height in re.findall(
            r'StrId::STR_PET_ITEM_(\d{3}), (\d+), (\d+)',
            (ROOT / 'src/apps_local/pet/PetHabitatData.h').read_text('utf-8'))}
        self.assertEqual(len(dimensions), 240)
        pictures = gray.catalog()
        walls = 0
        for row, (picture, name) in zip(rows, pictures):
            self.assertEqual(name, row['id'])
            source = row.get('grayscale_source')
            self.assertEqual(bool(source), row['anclaje'] == 'entorno_pared')
            if source:
                walls += 1
                # Legacy monochrome and gray crops can round one edge a pixel apart.
                self.assertTrue(all(abs(a - b) <= 1 for a, b in zip(picture.size, dimensions[row['firmware_id']])))
                self.assertTrue((gray.MASTERS / source['file']).is_file())
                self.assertTrue(0 <= source['column'] < source['columns'])
                self.assertTrue(0 <= source['row'] < source['rows'])
                sprite = gray.cell(source['file'], source['column'], source['row'], source['columns'], source['rows'])
                self.assertGreater(sprite.width, 40)
                self.assertGreater(sprite.height, 40)
        self.assertEqual(walls, 26)

    def test_sources_and_corrected_silva(self):
        provenance = json.loads((gray.MASTERS / "provenance.json").read_text("utf-8"))
        for item in provenance["assets"]:
            self.assertEqual(hashlib.sha256((gray.MASTERS / item["file"]).read_bytes()).hexdigest(), item["sha256"])
        happy = gray.pet("silva", "newborn", "happy")
        corrected = gray.cell("retoques/silva-recien-nacido-feliz-contorno.png", 0, 0, 1, 1)
        self.assertEqual(happy.tobytes(), corrected.tobytes())
        self.assertEqual(happy.size, corrected.size)
        # Gutter detection retains ears that cross the atlas's nominal row divisions.
        for species in gray.SPECIES:
            for age in gray.AGES:
                for mood in (*gray.MOODS, "resting"):
                    sprite = gray.pet(species, age, mood)
                    self.assertTrue(sprite.width > 20 and sprite.height > 20)
        self.assertEqual(len(gray.catalog()), 240)
        for species in gray.SPECIES:
            for age in gray.AGES:
                hungry = gray.pet(species, age, "hungry")
                dedicated = gray.cell("../v5/hambre-cuenco.png", gray.SPECIES.index(species), gray.AGES.index(age), 3, 4)
                self.assertEqual((hungry.size, hungry.tobytes()), (dedicated.size, dedicated.tobytes()))
                self.assertNotEqual(hungry.tobytes(), gray.pet(species, age, "tired").tobytes())

    def test_six_deterministic_packs(self):
        counts = dict(graphics=64, home=52, habitat=560, breathing=1278, farewell=6, collection=1)
        with tempfile.TemporaryDirectory() as temp:
            for kind, expected in counts.items():
                path = Path(temp) / kind / (kind + ".bin")
                info = gray.build(kind, path)
                data = path.read_bytes()
                self.assertEqual(data, (ROOT / "resources/apps/pet" / (kind + ".bin")).read_bytes())
                magic, count, size, checksum, index_crc = struct.unpack_from("<8sIIII", data)
                self.assertEqual((magic, count, size), (b"CNART003", expected, len(data)))
                self.assertEqual(checksum, zlib.crc32(data[24:]))
                self.assertEqual(index_crc, zlib.crc32(data[24:24 + count * 16]))
                self.assertEqual(hashlib.sha256(data).hexdigest(), info["sha256"])
                end, records = 24 + count * 16, []
                levels = set()
                for n in range(count):
                    offset, width, height, length, crc = struct.unpack_from("<IHHII", data, 24 + n * 16)
                    self.assertEqual(offset, end)
                    self.assertTrue(1 < width <= 280 and 1 < height <= 512)
                    self.assertEqual(length, (width * 2 + 7) // 8 * height)
                    payload = data[offset:offset + length]
                    self.assertEqual(zlib.crc32(payload), crc)
                    self.assertEqual((info["images"][n]["width"], info["images"][n]["height"]), (width, height))
                    levels.update((byte >> shift) & 3 for byte in payload for shift in (0, 2, 4, 6))
                    if kind == "graphics" and n < 51:
                        self.assertTrue(any(((byte >> shift) & 3) == 3 for byte in payload for shift in (0, 2, 4, 6)),
                                        f"Character or egg {n} lost its black contour during export")
                    if info["images"][n]["mask"]:
                        self.assertTrue(all(((byte >> shift) & 3) in (0, 3) for byte in payload for shift in (0, 2, 4, 6)))
                    elif n and info["images"][n - 1]["mask"]:
                        pw, ph, mask = records[-1]
                        self.assertEqual((width, height), (pw, ph))
                        self.assertTrue(all((ink & ~opaque) == 0 for ink, opaque in zip(payload, mask)))
                    records.append((width, height, payload))
                    end = offset + length
                self.assertEqual(end, len(data))
                self.assertEqual(levels, {0, 1, 2, 3})
                if kind == "home":
                    self.assertEqual([row[:2] for row in records[-7:]],
                                     [(240, 400)] * 4 + [(266, 480), (267, 480), (267, 480)])
                if kind == "breathing":
                    self.assertLess(len(data), 8 * 1024 * 1024)
                    references, bodies = gray.body_references()
                    self.assertEqual((len(references), len(bodies)), (12, 72))
                    for character in range(12):
                        for pose in range(6):
                            slot = character * 6 if pose == 5 else 96 + (character * 5 + pose) * 6
                            metric = bodies[character * 6 + pose]
                            self.assertEqual(records[slot][:2], metric[:2])
                            self.assertTrue(0 <= metric[2] < metric[1] and metric[3] > 0)
                    prefix = hashlib.sha256()
                    for width, height, payload in records[:822]:
                        prefix.update(struct.pack('<HH', width, height))
                        prefix.update(payload)
                    self.assertEqual(prefix.hexdigest(), 'dbd401e59657dbe59775aa6b289fcd870d3da76b624d0ccba3161f3c9ea34943')
                    for start in range(1038, 1182, 4):
                        self.assertEqual(records[start][:2], records[start + 2][:2])
                        self.assertNotEqual(records[start + 1][2], records[start + 3][2])
                        self.assertLessEqual(len(records[start][2]) * 2, 64 * 1024)
                    for food in range(3):
                        states = [records[1014 + food * 8 + stage * 2 + 1] for stage in range(4)]
                        self.assertEqual(len({f[:2] for f in states}), 1)
                        self.assertEqual(len({f[2] for f in states}), 4)
                        def upper_ink(record):
                            width, height, payload = record
                            stride = (width * 2 + 7) // 8
                            return sum((payload[y * stride + x // 4] >> (6 - x % 4 * 2)) & 3
                                       for y in range(height // 2) for x in range(width))
                        self.assertGreater(upper_ink(states[0]), upper_ink(states[-1]) * 1.4)
                    for start in range(822, 1014, 4):
                        self.assertEqual(records[start][:2], records[start + 2][:2])
                        self.assertNotEqual(records[start + 1][2], records[start + 3][2])
                        self.assertLessEqual(len(records[start][2]) * 2, 64 * 1024)
                    for start in range(630, 822, 4):
                        first, second = records[start], records[start + 2]
                        self.assertEqual(first[:2], second[:2])
                        self.assertNotEqual(records[start + 1][2], records[start + 3][2])
                        for width, height, mask in (first, second):
                            stride = (width * 2 + 7) // 8
                            bottom = max(y for y in range(height) for x in range(width)
                                         if (mask[y * stride + x // 4] >> (6 - x % 4 * 2)) & 3)
                            self.assertEqual(bottom, height - 1)
                    for start in range(582, 630, 4):
                        self.assertEqual(records[start][:2], records[start + 2][:2])
                        self.assertNotEqual(records[start + 1][2], records[start + 3][2])
                        # Keep the feet baseline and lower-body mass stable through the blink.
                        def feet(record):
                            width, height, mask = record
                            stride = (width * 2 + 7) // 8
                            points = [(x, y) for y in range(height * 3 // 4, height) for x in range(width)
                                      if (mask[y * stride + x // 4] >> (6 - x % 4 * 2)) & 3]
                            return sum(x for x, _ in points) / len(points), max(y for _, y in points)
                        first, second = feet(records[start]), feet(records[start + 2])
                        self.assertLessEqual(abs(first[0] - second[0]), 2)
                        self.assertLessEqual(abs(first[1] - second[1]), 1)
                    for start in range(534, 582, 8):
                        frames = [records[start + n * 2] for n in range(4)]
                        self.assertEqual(len({f[:2] for f in frames}), 1)
                        areas = [sum(((byte >> shift) & 3) != 0 for byte in f[2] for shift in (0,2,4,6)) for f in frames]
                        self.assertTrue(all(a > b for a,b in zip(areas,areas[1:])), areas)
                    for start in range(474, 534, 4):
                        self.assertEqual(records[start][:2], records[start + 2][:2])
                        self.assertNotEqual(records[start + 1][2], records[start + 3][2])
                    for start in (*range(0, 72, 6), *range(96, 474, 6)):
                        frames = [records[start + phase * 2 + 1] for phase in range(3)]
                        self.assertEqual(len({row[:2] for row in frames}), 1)
                        self.assertEqual(len({row[2] for row in frames}), 3)

    def test_walk_registration_and_limit(self):
        pictures, entries = gray.registered_walk()
        self.assertEqual((len(pictures), len(entries)), (48, 24))
        header = ROOT / 'src/apps_local/pet/PetRestData.h'
        self.assertEqual(header.read_text('utf-8'), gray.rest_header(entries))
        for slot, reg in enumerate(entries):
            reference = gray.body_metrics(gray.fit(gray.pet(reg['species'], reg['age'], 'normal'),
                                                   (reg['reference_width'], reg['reference_height'])))
            for frame in range(2):
                im = pictures[slot * 2 + frame][0]
                self.assertLessEqual(im.width, 128)
                self.assertLessEqual(im.height, 160)
                body = gray.body_metrics(im)
                sx, sy = reg['width'] / im.width, reg['height'] / im.height
                self.assertTrue(.65 <= body['area'] * sx * sy / reference['area'] <= 1.35, (slot, frame))
                self.assertAlmostEqual(body['foot_x'] * sx + reg['left'], reference['foot_x'], delta=4)
                self.assertAlmostEqual(body['foot_y'] * sy + reg['top'], reference['foot_y'], delta=3)

    def test_companion_masters_and_registration(self):
        pictures, entries = gray.registered_companions()
        self.assertEqual((len(pictures), len(entries)), (72, 36))
        header = ROOT / 'src/apps_local/pet/PetCompanionData.h'
        self.assertEqual(header.read_text('utf-8'), gray.companion_header(entries))
        for slot, reg in enumerate(entries):
            reference = gray.body_metrics(gray.fit(gray.pet(reg['species'], reg['age'], 'normal'),
                                                   (reg['reference_width'], reg['reference_height'])))
            for frame in range(2):
                picture = pictures[slot * 2 + frame][0]
                body = gray.body_metrics(picture)
                sx, sy = reg['width'] / picture.width, reg['height'] / picture.height
                self.assertTrue(.70 <= body['area'] * sx * sy / reference['area'] <= 1.30, (slot, frame))
                self.assertAlmostEqual(body['foot_x'] * sx + reg['left'], reference['foot_x'], delta=3)
                self.assertAlmostEqual(body['foot_y'] * sy + reg['top'], reference['foot_y'], delta=2)

    def test_reaction_masters(self):
        directory = ROOT / 'REDISEÑOS/PET/assets/v8/acciones'
        info = json.loads((directory / 'provenance.json').read_text('utf-8'))
        self.assertEqual((info['columns'], info['rows'], info['frames']), (3, 4, 2))
        self.assertEqual(len(info['assets']), 8)
        for asset in info['assets']:
            path = directory / asset['file']
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), asset['sha256'])
            for species in range(3):
                for age in range(4):
                    picture = gray.cell('../v8/acciones/' + asset['file'], species, age, 3, 4)
                    self.assertGreater(picture.width, 100)
                    self.assertGreater(picture.height, 100)

    def test_registered_body_scale_and_feet(self):
        pictures, registrations = gray.registered_reactions()
        self.assertEqual((len(pictures), len(registrations)), (96, 48))
        header = ROOT / 'src/apps_local/pet/PetReactionData.h'
        self.assertEqual(header.read_text('utf-8'), gray.reaction_header(registrations))
        for slot, reg in enumerate(registrations):
            reference = gray.body_metrics(gray.fit(gray.pet(reg['species'], reg['age'], 'normal'),
                                                   (reg['reference_width'], reg['reference_height'])))
            for frame in range(2):
                picture = pictures[slot * 2 + frame][0]
                body = gray.body_metrics(picture)
                scale_x = reg['width'] / picture.width
                scale_y = reg['height'] / picture.height
                ratio = body['area'] * scale_x * scale_y / reference['area']
                self.assertTrue(.75 <= ratio <= 1.25, (reg, frame, ratio))
                self.assertAlmostEqual(body['foot_x'] * scale_x + reg['left'], reference['foot_x'], delta=3)
                self.assertAlmostEqual(body['foot_y'] * scale_y + reg['top'], reference['foot_y'], delta=2)


if __name__ == "__main__":
    unittest.main()
