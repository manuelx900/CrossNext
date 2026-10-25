"""Check the generated pack contract, paired masks, sources and deterministic rebuild."""
from pathlib import Path
import hashlib
import importlib.util
import json
import struct
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[2]
path = ROOT / "resources/apps/pet/farewell.bin"
data = path.read_bytes()
magic, count, size, crc, index_crc = struct.unpack_from("<8sIIII", data)
assert magic == b"CNART002" and count == 6 and size == len(data)
assert crc == zlib.crc32(data[24:]) and index_crc == zlib.crc32(data[24:120])
manifest = json.loads(path.with_name('farewell-manifest.json').read_text('utf-8'))
assert manifest['sha256'] == hashlib.sha256(data).hexdigest()
end = 120
records=[]
for i in range(count):
    offset,w,h,length,crc = struct.unpack_from('<IHHII',data,24+i*16)
    assert offset == end and 0 < w <= 208 and 0 < h <= 240 and length == (w+7)//8*h
    payload=data[offset:offset+length]
    assert zlib.crc32(payload) == crc
    records.append((w,h,payload))
    end=offset+length
assert end == len(data)
for i in range(0,count,2):
    w,h,mask=records[i]
    iw,ih,ink=records[i+1]
    assert (w,h)==(iw,ih) and all((a&b)==b for a,b in zip(mask,ink))
for row in manifest['sources']:
    original=ROOT/'assets_local/pet/farewell'/row['file']
    assert hashlib.sha256(original.read_bytes()).hexdigest() == row['sha256']
spec=importlib.util.spec_from_file_location('farewell_builder',ROOT/'tools_local/pet/build_farewell.py')
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
with tempfile.TemporaryDirectory() as temp:
    copy=Path(temp)/'farewell.bin'
    module.build(copy)
    assert copy.read_bytes()==data
print('PET farewell: checksum, masks, source provenance and deterministic rebuild passed')
