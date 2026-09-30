#!/usr/bin/env python3
"""Read-only extraction of selected base-game form identities. No assets copied."""
import argparse
import json
import struct
import zlib
from pathlib import Path

def records(path):
    with Path(path).open('rb') as source:
        while header := source.read(24):
            if len(header) != 24:
                raise ValueError('truncated record header')
            kind, size, flags, form = struct.unpack_from('<4sIII', header)
            if kind == b'GRUP':
                continue
            payload = source.read(size)
            if len(payload) != size:
                raise ValueError('truncated record')
            if kind not in (b'INGR', b'ALCH', b'SPEL', b'SLGM', b'NPC_', b'FURN'):
                continue
            if flags & 0x40000:
                expected = struct.unpack_from('<I', payload)[0]
                payload = zlib.decompress(payload[4:])
                if len(payload) != expected:
                    raise ValueError('incorrect decompressed size')
            offset = 0
            while offset + 6 <= len(payload):
                tag, length = struct.unpack_from('<4sH', payload, offset)
                offset += 6
                if tag == b'XXXX':
                    length = struct.unpack_from('<I', payload, offset)[0]
                    offset += 4
                    tag = payload[offset:offset+4]
                    offset += 6
                value = payload[offset:offset+length]
                offset += length
                if tag == b'EDID':
                    yield {'id': f'{form:08X}', 'type': kind.decode(), 'editor_id': value.rstrip(b'\0').decode('utf-8')}
                    break

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('esm')
    parser.add_argument('--match', default='')
    args = parser.parse_args()
    import re
    print(json.dumps([r for r in records(args.esm) if re.search(args.match, r['editor_id'], re.I)], indent=2))
