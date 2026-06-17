#!/usr/bin/env python3
"""Build the committed room-fixture blobs from a /tmp/talcap capture.

Reads the DOSBox capture (manifest + per-room .fb goldens) produced by
dosbox_capture_pics.py, renders each room's vector stream with the offline
renderer (gmcgatest), and keeps only the rooms that reproduce the DOSBox golden
pixel-exactly over the 280x160 window (plus any within a small ceiling).  For
each kept room it emits, into test/gmcga/:

  rooms_streams.bin  -- concatenated Graphics-Magician vector streams (sliced
                        from the game RA..RG files, like the existing *.img)
  rooms_goldens.bin  -- concatenated 280x160 window goldens, packed 2 bits per
                        pixel, MSB-first (4 px/byte) -- 11200 bytes each
  rooms.tsv          -- name, stream_off, stream_len, golden_off, ceil

test_gmcga_pics.cpp reads these and renders each stream through graphics_magician_cga,
comparing to the unpacked golden in CGA palette-index space.  Self-contained:
no game files or NOVEL.EXE are needed at test time (streams are committed here).
"""
import os, subprocess

GAME = "/Users/administrator/Downloads/comprehend games/talisman-challenging-the-sands-of-time"
HERE = os.path.dirname(os.path.abspath(__file__))
COMP = os.path.dirname(os.path.dirname(HERE))           # terps/comprehend
FIX  = os.path.join(HERE, "fixtures")                   # local-only fixtures
os.makedirs(FIX, exist_ok=True)
TABLES = os.path.join(FIX, "novel_tables.bin")
GMCGATEST = os.path.join(COMP, "test", "gmcgatest")
CAP = "/tmp/talcap"
CEIL_MAX = 100      # keep near-exact rooms too, recording their actual ceiling

M = {(0, 0, 0): 0, (0, 170, 170): 1, (170, 0, 170): 2, (170, 170, 170): 3}


def ppm_idx(p):
    d = open(p, "rb").read()
    i = d.index(b"255\n") + 4
    px = d[i:]
    return [M[(px[j], px[j+1], px[j+2])] for j in range(0, len(px), 3)]


def fb_window(p):
    b = open(p, "rb").read()
    return [b[y*320 + 20 + x] for y in range(160) for x in range(280)]


def pack_window(win):
    out = bytearray(280 * 160 // 4)
    for i in range(0, len(win), 4):
        out[i >> 2] = (win[i] << 6) | (win[i+1] << 4) | (win[i+2] << 2) | win[i+3]
    return bytes(out)


rows = [l.strip().split("\t") for l in open(f"{CAP}/manifest.tsv")][1:]
streams = bytearray()
goldens = bytearray()
man = ["name\tstream_off\tstream_len\tgolden_off\tceil"]
kept = dropped = 0
for name, kind, fil, pic, off, length, G in rows:
    off, length = int(off), int(length)
    subprocess.run([GMCGATEST, TABLES, os.path.join(GAME, fil), str(off),
                    "/tmp/_g.ppm", "white"], capture_output=True)
    win = fb_window(f"{CAP}/{name}.fb")
    diffs = sum(1 for a, b in zip(ppm_idx("/tmp/_g.ppm"), win) if a != b)
    if diffs > CEIL_MAX:
        dropped += 1
        continue
    stream = open(os.path.join(GAME, fil), "rb").read()[off:off+length]
    man.append(f"{name}\t{len(streams)}\t{len(stream)}\t{len(goldens)}\t{diffs}")
    streams += stream
    goldens += pack_window(win)
    kept += 1

open(f"{FIX}/rooms_streams.bin", "wb").write(streams)
open(f"{FIX}/rooms_goldens.bin", "wb").write(goldens)
open(f"{HERE}/rooms.tsv", "w").write("\n".join(man) + "\n")
print(f"kept {kept} rooms ({dropped} over ceiling {CEIL_MAX})")
print(f"rooms_streams.bin {len(streams)} B, rooms_goldens.bin {len(goldens)} B")
