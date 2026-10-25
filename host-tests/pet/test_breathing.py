"""Check every SD animation frame, its registration, mask, CRC and deterministic rebuild."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("pet_breathing", ROOT / "tools_local/pet/build_breathing.py")
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class BreathingTest(unittest.TestCase):
    def test_deterministic_registered_frames(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)/"breathing.bin"
            builder.build(path)
            data = path.read_bytes()
        self.assertEqual(data, (ROOT/"resources/apps/pet/breathing.bin").read_bytes())
        magic, count, length, checksum, index_crc = struct.unpack_from("<8sIIII", data)
        self.assertEqual((magic, count, length), (b"CNART002", 474, len(data)))
        self.assertEqual(checksum, zlib.crc32(data[24:]))
        self.assertEqual(index_crc, zlib.crc32(data[24:24+count*16]))
        frames = []
        for index in range(count):
            offset, width, height, size, crc = struct.unpack_from("<IHHII", data, 24+index*16)
            payload = data[offset:offset+size]
            self.assertEqual(len(payload), size)
            self.assertEqual(size, ((width+7)//8)*height)
            self.assertEqual(crc, zlib.crc32(payload))
            self.assertTrue(1 < width <= 280 and 1 < height <= 512)
            frames.append((width, height, payload))
            if index % 2:
                mask = frames[index-1]
                self.assertEqual(mask[:2], (width, height))
                self.assertTrue(all((ink & ~opaque) == 0 for opaque, ink in zip(mask[2], payload)))
        for start in [*range(0, 72, 6), *range(96, 474, 6)]:
            phases = [frames[start+phase*2+1] for phase in range(3)]
            self.assertEqual(len({f[:2] for f in phases}), 1)
            self.assertEqual(len({f[2] for f in phases}), 3, f"No visible breathing at index {start}")


if __name__ == "__main__":
    unittest.main()
