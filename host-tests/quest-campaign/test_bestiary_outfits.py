"""Validate original outfit identities, gender variants and the streamed pack."""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import zlib


def check(root):
    folder = root / 'resources/apps/reading-adventure'
    path = folder / 'bestiary-outfits.bin'
    data = path.read_bytes()
    manifest = json.loads((folder / 'bestiary-outfits-manifest.json').read_text('utf8'))
    assert hashlib.sha256(data).hexdigest() == manifest['sha256']
    magic, count, size, crc, index_crc = struct.unpack_from('<8sIIII', data)
    assert magic == b'CNART003' and count == manifest['sprites'] == 224
    assert size == manifest['bytes'] == len(data) < 8 * 1024 * 1024
    assert zlib.crc32(data[24:]) == crc and zlib.crc32(data[24:24 + count * 16]) == index_crc
    pictures = []
    end = 24 + count * 16
    for n in range(count):
        offset, width, height, length, checksum = struct.unpack_from('<IHHII', data, 24 + n * 16)
        assert offset == end and (width,height,length) == (160,208,8320)
        pixels = data[offset:offset + length]
        assert zlib.crc32(pixels) == checksum and any(pixels)
        pictures.append(pixels)
        end += length
    assert end == len(data)
    assert len(set(pictures[:112])) == 112
    for ink, mask in zip(pictures[:112], pictures[112:]):
        assert all((a >> shift & 3) == 0 or (b >> shift & 3) > 0
                   for a,b in zip(ink,mask) for shift in (0,2,4,6))
    directory = root / 'assets_local/quest/bestiary-outfits'
    provenance = json.loads((directory / 'provenance.json').read_text('utf8'))
    assert provenance['tool'] == 'built-in image_gen' and len(provenance['masters']) == 7
    for master in provenance['masters']:
        assert hashlib.sha256((directory / master['master']).read_bytes()).hexdigest() == master['sha256']
    spec = importlib.util.spec_from_file_location('outfit_builder', root / 'tools_local/quest/build_bestiary_outfits.py')
    builder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(builder)
    builder.validate_masters(directory)
    with tempfile.TemporaryDirectory() as temporary:
        invalid = Path(temporary)
        recorded = (directory / 'provenance.json').read_bytes()
        (invalid / 'provenance.json').write_bytes(recorded)
        (invalid / provenance['masters'][0]['master']).write_bytes(b'unapproved replacement')
        try:
            builder.validate_masters(invalid)
        except ValueError as error:
            assert 'checksum mismatch' in str(error)
        else:
            raise AssertionError('Replaced master was accepted')
        assert (invalid / 'provenance.json').read_bytes() == recorded
    entries = json.loads((root / 'assets_local/quest/bestiary/catalog.json').read_text('utf8'))['monsters']
    assert len(entries) == 56 and not any('body' in entry for entry in entries)
    print('Bestiary outfits: 56 original designs,112 distinct gender sprites,224 records,all CRC/masks and provenance passed')


if __name__ == '__main__':
    check(Path(__file__).resolve().parents[2])
