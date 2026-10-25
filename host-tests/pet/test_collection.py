import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("pet_collection", ROOT / "tools_local/pet/build_collection.py")
pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)


class CollectionTest(unittest.TestCase):
    def test_sd_payload_and_determinism(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "collection.bin"
            info = pack.build(path)
            data = path.read_bytes()
            self.assertEqual(struct.unpack_from("<8sII", data), (b"CNART002", 1, 7390))
            self.assertEqual(struct.unpack_from("<II", data, 16), (zlib.crc32(data[24:]), zlib.crc32(data[24:40])))
            offset, width, height, size, crc = struct.unpack_from("<IHHII", data, 24)
            self.assertEqual((offset, width, height, size), (40, 280, 210, 7350))
            self.assertLessEqual(width, 280)  # sdart::MAX_WIDTH; no shared row-buffer expansion.
            self.assertEqual(zlib.crc32(data[offset:]), crc)
            self.assertTrue(any(data[offset:]) and any(value != 255 for value in data[offset:]))
            self.assertEqual(info["sprites"], 1)
            self.assertEqual(data, (ROOT / "resources/apps/pet/collection.bin").read_bytes())


if __name__ == "__main__":
    unittest.main()
