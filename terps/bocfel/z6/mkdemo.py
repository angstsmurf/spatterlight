#!/usr/bin/env python3
"""Turn an Infocom V6 story file into its demo version.

Infocom's own Zork Zero demos differ from the full releases only in the
initial value of the DEMO-VERSION? global (0 -> 1) and the header checksum.
The global is located from the INPUT-DEMO / READ-DEMO routines, which do
    SET 'DEMO-VERSION?,-1 ; INPUT/READ ... ; EQUAL? DEMO-VERSION?,1

Releases with the demo code: Zork Zero r296 and later (not r242 or the .z5
builds), Arthur r40 and later, Shogun r311 and later. Journey has none.

usage: mkdemo.py scan STORY...     report the demo global of each file
       mkdemo.py STORY DEMO        write a demo version of STORY to DEMO
"""
import os, re, sys, struct
from collections import namedtuple

# Z-machine header fields.
HEADER_RELEASE = 0x02
HEADER_GLOBALS = 0x0c
HEADER_SERIAL = 0x12
HEADER_FILE_LENGTH = 0x1a
HEADER_CHECKSUM = 0x1c
HEADER_SIZE = 0x40

# Variable numbers below this are the stack and the locals.
FIRST_GLOBAL = 0x10

# How far past the SET to look for the EQUAL? test.
EQUAL_SEARCH_BYTES = 16

# The file length in the header is in units of this many bytes.
LENGTH_SCALE = {1: 2, 2: 2, 3: 2, 4: 4, 5: 4, 6: 8, 7: 8, 8: 8}

Story = namedtuple('Story', 'data release serial demo_globals checksum stored_checksum length')

def word(d, offset):
    return struct.unpack('>H', d[offset:offset + 2])[0]

def find_demo_global(d):
    """Map each candidate variable number to the offsets where it is set."""
    hits = {}
    for m in re.finditer(rb'\xcd\x4f(.)\xff\xff[\xf6\xe4]', d, re.S):
        g = m.group(1)[0]
        tail = d[m.end():m.end() + EQUAL_SEARCH_BYTES]
        if bytes([0x41, g, 0x01]) in tail and g >= FIRST_GLOBAL:
            hits.setdefault(g, []).append(m.start())
    return hits

def checksum(d):
    """The checksum of the story file and the file length its header gives."""
    scale = LENGTH_SCALE.get(d[0])
    if scale is None:
        sys.exit(f"not a Z-machine story file (version byte {d[0]})")
    length = word(d, HEADER_FILE_LENGTH) * scale
    return sum(d[HEADER_SIZE:length]) & 0xffff, length

def global_address(d, g):
    """Where the value of variable number g is stored."""
    return word(d, HEADER_GLOBALS) + 2 * (g - FIRST_GLOBAL)

def info(path):
    with open(path, 'rb') as f:
        d = f.read()
    cs, length = checksum(d)
    return Story(data=d, release=word(d, HEADER_RELEASE),
                 serial=d[HEADER_SERIAL:HEADER_SERIAL + 6].decode('latin1'),
                 demo_globals=find_demo_global(d), checksum=cs,
                 stored_checksum=word(d, HEADER_CHECKSUM), length=length)

def scan(paths):
    for p in paths:
        s = info(p)
        d = s.data
        desc = []
        for g, offs in s.demo_globals.items():
            a = global_address(d, g)
            desc.append(f"G{g-FIRST_GLOBAL:02x} @0x{a:x}={d[a]:02x}{d[a+1]:02x} ({len(offs)} sites)")
        if s.checksum == s.stored_checksum:
            cksum = 'ok'
        else:
            cksum = f'BAD {s.stored_checksum:04x}!={s.checksum:04x}'
        print(f"{os.path.basename(p):38s} v{d[0]} r{s.release} s{s.serial} len {len(d)}/{s.length} "
              f"cksum {cksum}  " + ('; '.join(desc) or 'NO DEMO GLOBAL'))

def make_demo(src, dst):
    s = info(src)
    if len(s.demo_globals) != 1:
        sys.exit(f"{src}: expected one demo global, found {s.demo_globals}")
    g = next(iter(s.demo_globals))
    d = bytearray(s.data)
    a = global_address(d, g)
    if d[a:a+2] != b'\0\0':
        sys.exit(f"{src}: demo global is already {d[a:a+2].hex()}")
    d[a+1] = 1
    new_checksum = checksum(d)[0]
    d[HEADER_CHECKSUM:HEADER_CHECKSUM + 2] = struct.pack('>H', new_checksum)
    with open(dst, 'wb') as f:
        f.write(d)
    print(f"{dst}: G{g-FIRST_GLOBAL:02x} @0x{a:x} set, checksum {s.stored_checksum:04x} -> {new_checksum:04x}")

if __name__ == '__main__':
    if len(sys.argv) < 3 or (sys.argv[1] != 'scan' and len(sys.argv) != 3):
        sys.exit(__doc__)
    if sys.argv[1] == 'scan':
        scan(sys.argv[2:])
    else:
        make_demo(*sys.argv[1:3])
