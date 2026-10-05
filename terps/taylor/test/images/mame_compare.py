#!/usr/bin/env python3
"""Compare the interpreter's room pictures with the screens captured from MAME.

    mame_compare.py [--probe PATH] [--diff DIR] [-v] [GAME...]

Runs the picture probe on each game with -d, drawing every room from the state
the original was in when its screen was captured (mame/<game>/room<N>.state,
see mame_capture.py), and compares room<N>.png with room<N>.scr, pixel by
pixel, in the top 12 character rows where the pictures are. A room the
interpreter does not draw, leaving the previous picture on the screen, is not
compared. Nor is a room the original did not draw: one that kills the player
before the picture is drawn leaves the picture of the first room, as captured
in room0.scr, on the screen. Colours are compared as ZX Spectrum colour
numbers, so the palette the interpreter paints with does not matter.
Differences listed in mame_known.tsv (game, room, reason) are counted but do
not fail the run; -v lists them.

--diff DIR writes, for every room that differs, a PNG with the original, the
interpreter's picture and the differing pixels side by side.
"""

import argparse
import os
import struct
import subprocess
import sys
import tempfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from mame_capture import GAMES, manifest_file  # noqa: E402

WIDTH, HEIGHT = 256, 96
RGB = [(0, 0, 0), (0, 0, 215), (215, 0, 0), (215, 0, 215), (0, 215, 0), (0, 215, 215), (215, 215, 0), (215, 215, 215),
       (0, 0, 0), (0, 0, 255), (255, 0, 0), (255, 0, 255), (0, 255, 0), (0, 255, 255), (255, 255, 0), (255, 255, 255)]


def scr_colours(scr):
    """The picture area of a display file as rows of colour numbers 0-15."""
    rows = []
    for y in range(HEIGHT):
        at = ((y & 0xC0) << 5) | ((y & 7) << 8) | ((y & 0x38) << 2)
        row = bytearray(WIDTH)
        for cx in range(32):
            bits = scr[at + cx]
            attr = scr[0x1800 + (y // 8) * 32 + cx]
            bright = 8 if attr & 0x40 else 0
            ink, paper = (attr & 7) | bright, ((attr >> 3) & 7) | bright
            for i in range(8):
                row[cx * 8 + i] = ink if bits & (0x80 >> i) else paper
        rows.append(bytes(c & 7 if c == 8 else c for c in row))  # bright black is black
    return rows


def png_colours(path):
    """A probe PNG (8-bit RGB, unfiltered rows) as rows of colour numbers."""
    data = open(path, "rb").read()
    at, idat = 8, b""
    while at < len(data):
        length, tag = struct.unpack(">I4s", data[at:at + 8])
        body = data[at + 8:at + 8 + length]
        if tag == b"IHDR":
            width, height, depth, kind = struct.unpack(">IIBB", body[:10])
            assert (depth, kind) == (8, 2), path
        elif tag == b"IDAT":
            idat += body
        at += 12 + length
    raw = zlib.decompress(idat)
    scale = width // WIDTH
    rows = []
    for y in range(HEIGHT):
        start = y * scale * (1 + width * 3)
        line = raw[start:start + 1 + width * 3]
        assert line[0] == 0, path
        row = bytearray(WIDTH)
        for x in range(WIDTH):
            r, g, b = line[1 + x * scale * 3:4 + x * scale * 3]
            c = (1 if b > 127 else 0) | (2 if r > 127 else 0) | (4 if g > 127 else 0)  # bright red is ff0014
            row[x] = c | 8 if c and 255 in (r, g, b) else c
        rows.append(bytes(row))
    return rows


def write_png(path, rows):
    raw = b"".join(b"\0" + b"".join(bytes(p) for p in row) for row in rows)

    def chunk(tag, body):
        return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body))

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", len(rows[0]), len(rows), 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def write_diff(path, original, ours):
    rows = []
    for a, b in zip(original, ours):
        rows.append([RGB[c] for c in a] + [(128, 128, 128)] * 4 + [RGB[c] for c in b] + [(128, 128, 128)] * 4
                    + [(255, 255, 255) if p != q else (0, 0, 0) for p, q in zip(a, b)])
    write_png(path, rows)


def known_differences(name):
    known = {}
    for line in open(os.path.join(HERE, "mame_known.tsv"), encoding="utf-8"):
        f = line.rstrip("\n").split("\t")
        if len(f) == 3 and f[0] == name:
            known[f[1]] = f[2]
    return known


def compare(name, probe, diff_dir, verbose):
    game = GAMES[name]
    golden = os.path.join(HERE, "mame", name)
    image = manifest_file(game["row"])
    if not os.path.isdir(golden) or not os.path.exists(image):
        print("%s: skipped (no %s)" % (name, "captures in mame/" if os.path.exists(image) else "game image"))
        return 0, 0, 0, 0
    known = known_differences(name)
    same = differ = expected = undrawn = 0
    start, start_room = None, None
    if os.path.exists(os.path.join(golden, "room0.state")):
        start = scr_colours(open(os.path.join(golden, "room0.scr"), "rb").read())
        start_room = "room%d" % open(os.path.join(golden, "room0.state"), "rb").read()[0]
    with tempfile.TemporaryDirectory() as tmp:
        report = subprocess.run([probe, "-d", tmp, image, "n"], stdin=subprocess.DEVNULL,
                                env=dict(os.environ, TAYLOR_PROBE_STATE=golden),
                                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL).stdout.decode("latin-1")
        not_drawn = {line.split()[0] for line in report.splitlines() if " not drawn " in line}
        for scr in sorted(os.listdir(golden), key=lambda n: (len(n), n)):
            room = scr[:-4]
            if not scr.endswith(".scr") or not os.path.exists(os.path.join(golden, room + ".state")):
                continue
            if room in not_drawn:
                undrawn += 1
                continue
            original = scr_colours(open(os.path.join(golden, scr), "rb").read())
            if original == start and room != start_room:
                undrawn += 1
                continue
            png = os.path.join(tmp, room + ".png")
            ours = png_colours(png) if os.path.exists(png) else [bytes(WIDTH)] * HEIGHT
            pixels = sum(p != q for a, b in zip(original, ours) for p, q in zip(a, b))
            if not pixels:
                same += 1
                continue
            if diff_dir:
                os.makedirs(diff_dir, exist_ok=True)
                write_diff(os.path.join(diff_dir, "%s-%s.png" % (name, room)), original, ours)
            if room in known:
                expected += 1
                if verbose:
                    print("%s %s: %d pixels differ (known: %s)" % (name, room, pixels, known[room]))
            else:
                differ += 1
                print("%s %s: %d pixels differ" % (name, room, pixels))
    return same, differ, expected, undrawn


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--probe", default=os.path.join(HERE, "..", "..", "taylor_image_probe"))
    ap.add_argument("--diff", metavar="DIR")
    ap.add_argument("-v", "--verbose", action="store_true")
    ap.add_argument("games", nargs="*")
    args = ap.parse_args()
    total = [0, 0, 0, 0]
    for name in args.games or GAMES:
        if name not in GAMES:
            sys.exit("unknown game " + name)
        total = [t + n for t, n in zip(total, compare(name, args.probe, args.diff, args.verbose))]
    print("mame compare: %d rooms identical, %d differ, %d known differences, %d not drawn" % tuple(total))
    return 1 if total[1] else 0


if __name__ == "__main__":
    sys.exit(main())
