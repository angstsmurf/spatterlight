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
import re, sys, struct

def find_demo_global(d):
    hits = {}
    for m in re.finditer(rb'\xcd\x4f(.)\xff\xff[\xf6\xe4]', d, re.S):
        g = m.group(1)[0]
        tail = d[m.end():m.end() + 16]
        if bytes([0x41, g, 0x01]) in tail and g >= 0x10:
            hits.setdefault(g, []).append(m.start())
    return hits

def checksum(d):
    ver = d[0]
    scale = {1:2,2:2,3:2,4:4,5:4,6:8,7:8,8:8}[ver]
    length = struct.unpack('>H', d[0x1a:0x1c])[0] * scale
    return sum(d[0x40:length]) & 0xffff, length

def info(path):
    d = open(path, 'rb').read()
    rel = struct.unpack('>H', d[2:4])[0]
    serial = d[0x12:0x18].decode('latin1')
    hits = find_demo_global(d)
    cs, length = checksum(d)
    stored = struct.unpack('>H', d[0x1c:0x1e])[0]
    return d, rel, serial, hits, cs, stored, length

if __name__ == '__main__':
    if len(sys.argv) < 3 or (sys.argv[1] != 'scan' and len(sys.argv) != 3):
        sys.exit(__doc__)
    if sys.argv[1] == 'scan':
        for p in sys.argv[2:]:
            d, rel, serial, hits, cs, stored, length = info(p)
            gt = struct.unpack('>H', d[0x0c:0x0e])[0]
            desc = []
            for g, offs in hits.items():
                a = gt + 2 * (g - 0x10)
                desc.append(f"G{g-0x10:02x} @0x{a:x}={d[a]:02x}{d[a+1]:02x} ({len(offs)} sites)")
            print(f"{p.split('/')[-1]:38s} v{d[0]} r{rel} s{serial} len {len(d)}/{length} "
                  f"cksum {'ok' if cs == stored else f'BAD {stored:04x}!={cs:04x}'}  "
                  + ('; '.join(desc) or 'NO DEMO GLOBAL'))
    else:
        src, dst = sys.argv[1:3]
        d, rel, serial, hits, cs, stored, length = info(src)
        if len(hits) != 1:
            sys.exit(f"{src}: expected one demo global, found {hits}")
        g = next(iter(hits))
        gt = struct.unpack('>H', d[0x0c:0x0e])[0]
        a = gt + 2 * (g - 0x10)
        if d[a:a+2] != b'\0\0':
            sys.exit(f"{src}: demo global is already {d[a:a+2].hex()}")
        d = bytearray(d)
        d[a+1] = 1
        d[0x1c:0x1e] = struct.pack('>H', checksum(d)[0])
        open(dst, 'wb').write(d)
        print(f"{dst}: G{g-0x10:02x} @0x{a:x} set, checksum {stored:04x} -> {d[0x1c]:02x}{d[0x1d]:02x}")
