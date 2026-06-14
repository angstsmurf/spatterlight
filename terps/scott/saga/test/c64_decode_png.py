#!/usr/bin/env python3
# Decode a VICE screenshot of a C64 "tiny" (mini-C64) SAGA room into a golden
# grid for c64test, and match/compare it against the C renderer's composited
# output (c64test grid <spec>).
#
# A room as displayed is a composite: a background bitmap + object overlays +
# monochrome white item sprites (see groundtruth_c64/README.md). The C renderer
# (c64test) replays that composite and emits each pixel's *displayed colour*
# (C64R1 RGB grid). We align the screenshot to that render, then classify every
# VICE pixel to the nearest renderer colour and write the golden in the
# renderer's own colour space (C64C1) — so c64test cmp is a pure RGB-equality
# check and the test stays self-contained.
#
# The VICE frame carries a border, so the C64 display origin is found by a small
# offset search that maximises the match (reported). Pixels are 1:1 (no scaling).
#
#   c64_decode_png.py grid    <png> <spec> <c64test> <out.c64>
#   c64_decode_png.py cmp     <png> <spec> <c64test>            render+compare
#   c64_decode_png.py capture <png> <spec> <c64test> <outdir>   copy spec+dats+golden

import sys, os, struct, subprocess, shutil

from PIL import Image

UNSET = 0xffffffff

def load_png(path):
    im = Image.open(path).convert('RGB')
    return im.load(), im.size[0], im.size[1]

def render_grid(c64test, spec):
    out = '/tmp/_c64_cmp.grid'
    subprocess.run([c64test, 'grid', spec, out], check=True, stdout=subprocess.DEVNULL)
    with open(out, 'rb') as f:
        data = f.read()
    assert data[:5] == b'C64R1', "bad grid magic"
    w, h = struct.unpack_from('<ii', data, 5)
    vals = struct.unpack_from('<%dI' % (w * h), data, 13)
    g = [vals[y * w:(y + 1) * w] for y in range(h)]
    return g, w, h

def renderer_colours(g, w, h):
    s = set()
    for y in range(h):
        for x in range(w):
            if g[y][x] != UNSET:
                s.add(g[y][x])
    return sorted(s)

def nearest(px, cols):
    pr, pg, pb = px
    best, bd = cols[0], 1 << 30
    for c in cols:
        d = (pr - (c >> 16 & 255)) ** 2 + (pg - (c >> 8 & 255)) ** 2 + (pb - (c & 255)) ** 2
        if d < bd:
            bd, best = d, c
    return best

def best_offset(px, pw, ph, cols, g, w, h, search=48):
    drawn = [(x, y, g[y][x]) for y in range(min(h, 200)) for x in range(min(w, 320))
             if g[y][x] != UNSET]
    best = (-1, 0, 0, 0)
    for oy in range(search):
        for ox in range(search):
            m = t = 0
            for x, y, v in drawn:
                sx, sy = ox + x, oy + y
                if 0 <= sx < pw and 0 <= sy < ph:
                    t += 1
                    if nearest(px[sx, sy], cols) == v:
                        m += 1
            if t and m > best[0]:
                best = (m, t, ox, oy)
    return best

def build_golden(px, pw, ph, cols, g, w, h, ox, oy):
    out = [[UNSET] * w for _ in range(h)]
    for y in range(min(h, 200)):
        for x in range(min(w, 320)):
            if g[y][x] == UNSET:
                continue
            sx, sy = ox + x, oy + y
            out[y][x] = nearest(px[sx, sy], cols) if (0 <= sx < pw and 0 <= sy < ph) else UNSET
    return out

def write_golden(path, golden, w, h):
    # Compact palette+index format: "C64C2", w, h, npal (int32 LE), npal*3 RGB
    # bytes, then w*h index bytes (0xff = UNSET). Colours are the renderer's own,
    # so c64test cmp is an exact RGB match.
    pal = sorted({golden[y][x] for y in range(h) for x in range(w)
                  if golden[y][x] != UNSET})
    idx = {c: i for i, c in enumerate(pal)}
    assert len(pal) < 255, "too many colours for byte index"
    with open(path, 'wb') as f:
        f.write(b'C64C2'); f.write(struct.pack('<iii', w, h, len(pal)))
        for c in pal:
            f.write(bytes((c >> 16 & 255, c >> 8 & 255, c & 255)))
        for row in golden:
            f.write(bytes(idx.get(v, 0xff) for v in row))

def main():
    mode = sys.argv[1]
    png, spec, c64test = sys.argv[2], sys.argv[3], sys.argv[4]
    px, pw, ph = load_png(png)
    g, w, h = render_grid(c64test, spec)
    cols = renderer_colours(g, w, h)
    m, t, ox, oy = best_offset(px, pw, ph, cols, g, w, h)
    pct = 100 * m / max(t, 1)
    print(f"best offset dx={ox} dy={oy}: {m}/{t} = {pct:.2f}%  ({len(cols)} colours)")

    if mode == 'cmp':
        return
    golden = build_golden(px, pw, ph, cols, g, w, h, ox, oy)
    if mode == 'grid':
        write_golden(sys.argv[5], golden, w, h)
        print("wrote", sys.argv[5])
    elif mode == 'capture':
        outdir = sys.argv[5]
        if pct < 99.95:
            print(f"NO clean match ({pct:.2f}%) - composite incomplete or offset wrong")
            return
        os.makedirs(outdir, exist_ok=True)
        # Copy the spec and every .dat it references into the corpus dir.
        specdir = os.path.dirname(spec)
        shutil.copy(spec, os.path.join(outdir, os.path.basename(spec)))
        with open(spec) as f:
            for line in f:
                line = line.split('#')[0].strip()
                if not line:
                    continue
                fn = line.split()[-1]
                shutil.copy(os.path.join(specdir, fn), os.path.join(outdir, fn))
        name = os.path.splitext(os.path.basename(spec))[0]
        write_golden(os.path.join(outdir, name + '.c64'), golden, w, h)
        print(f"captured {name}: {pct:.2f}% (dx={ox} dy={oy}) -> {outdir}")

if __name__ == '__main__':
    main()
