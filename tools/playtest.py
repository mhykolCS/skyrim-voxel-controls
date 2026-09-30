#!/usr/bin/env python3
"""Local driver for a VOXEL_PLAYTEST build; absent from release DLLs.

Sends only to the development plugin's command file, never desktop keyboard input.
"""
import argparse
import struct
import subprocess
import time
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('game', type=Path)
parser.add_argument('command', help='One development command, e.g. "action camera"')
parser.add_argument('--capture', type=Path, help='Write the game-only DDS capture as PNG')
args = parser.parse_args()
folder = args.game / 'Data/SKSE/Plugins'
path = folder / 'VoxelControls.playtest.txt'
if path.exists():
    parser.error('A command is still pending; it was left unchanged')
staged = path.with_suffix('.tmp')
staged.write_text(args.command + '\n')
staged.replace(path)
deadline = time.monotonic() + 10
while path.exists() and time.monotonic() < deadline:
    time.sleep(.1)
if path.exists():
    parser.error('Command was not consumed: the game needs a VOXEL_PLAYTEST build')
if args.capture:
    capture = folder / 'VoxelControls.capture.dds'
    time.sleep(.5)
    data = capture.read_bytes()
    height, width = struct.unpack_from('<II', data, 12)
    flags, fourcc, bits, red, green, blue, alpha = struct.unpack_from('<7I', data, 80)
    # ImageMagick's DDS reader ignores these RGBA channel masks. Feed the raw
    # channels explicitly so capture colours match the actual back buffer.
    if data[:4] != b'DDS ' or (fourcc, bits, red, green, blue, alpha) != (0, 32, 255, 65280, 16711680, 4278190080):
        parser.error('Unsupported capture pixel layout; DDS was preserved')
    subprocess.run(['magick', '-size', f'{width}x{height}', '-depth', '8', 'rgba:-', str(args.capture)],
                   input=data[128:], check=True)
print('Command consumed:', args.command)
