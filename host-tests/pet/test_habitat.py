"""Check the SD packet contract, masks, and stable mapping of all supplied Home assets."""
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zlib
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('pet_habitat_builder', ROOT/'tools_local/pet/build_habitat.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class HabitatTest(unittest.TestCase):
    def test_reproducible_pack_and_masks(self):
        with tempfile.TemporaryDirectory() as temporary:
            binary_path = Path(temporary)/'habitat.bin'
            header_path = Path(temporary)/'data.h'
            info = builder.build(binary_path, header_path)
            data = binary_path.read_bytes()
            self.assertEqual(data, (ROOT/'resources/apps/pet/habitat.bin').read_bytes())
            self.assertEqual(header_path.read_bytes(), (ROOT/'src/apps_local/pet/PetHabitatData.h').read_bytes())
            magic,count,length,crc,index_crc = struct.unpack_from('<8sIIII',data)
            self.assertEqual((magic,count,length),(b'CNART002',560,len(data)))
            self.assertEqual(crc,zlib.crc32(data[24:]))
            self.assertEqual(index_crc,zlib.crc32(data[24:24+count*16]))
            for n in range(count):
                offset,width,height,size,checksum = struct.unpack_from('<IHHII',data,24+n*16)
                self.assertTrue(1 < width <= 280 and 1 < height <= 512)
                self.assertEqual(size,(width+7)//8*height)
                self.assertEqual(checksum,zlib.crc32(data[offset:offset+size]))
                if n >= 2 and n % 2 == 0:
                    ink_offset,ink_width,ink_height,ink_size,_ = struct.unpack_from('<IHHII',data,24+(n+1)*16)
                    self.assertEqual((width,height,size),(ink_width,ink_height,ink_size))
                    # Every ink pixel is included in the silhouette, including paws and tails.
                    self.assertTrue(all((ink & ~mask) == 0 for mask,ink in
                                        zip(data[offset:offset+size],data[ink_offset:ink_offset+size])))
            self.assertEqual(len(info['images']),560)

    def test_catalog_mapping_and_pngs(self):
        source=ROOT/'assets_local/pet/hogar'
        rows=json.loads((source/'catalogo-integrado.json').read_text(encoding='utf-8'))
        self.assertEqual(len({row['id'] for row in rows}),240)
        legacy=['descanso-001','descanso-004','descanso-005','adorno-001','adorno-002','adorno-003',
                'juego-001','juego-002','juego-003','entorno-001','entorno-002','entorno-003']
        self.assertEqual([row['id'] for row in rows[:12]],legacy)
        for row in rows:
            with Image.open(source/'firmware-png'/f"{row['id']}.png") as image:
                self.assertEqual(image.mode,'1')
                self.assertLessEqual(image.width,row['max_ancho_pantalla'])
                self.assertLessEqual(image.height,row['max_alto_pantalla'])
            self.assertTrue(row['nombre'] and row['english_name'])


if __name__=='__main__':
    unittest.main()
