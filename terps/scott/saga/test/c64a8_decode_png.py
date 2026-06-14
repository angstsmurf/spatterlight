#!/usr/bin/env python3
# Decode a VICE screenshot of a C64 / Atari 8-bit *full* bitmap SAGA room (the
# SagaPlus titles — Spider-Man, Buckaroo Banzai, …) into a golden grid for
# c64a8test, and match/compare it against the C renderer's output
# (c64a8test grid <spec>). Same C64R1 (render grid) / C64C2 (golden) formats as
# the mini-C64 c64_decode_png.py, so c64a8test cmp stays a pure RGB-equality
# check; this variant differs only in how it aligns the screenshot.
#
# Alignment differs from the mini-C64 decoder in two ways:
#   * Wider X search. The original C64 game draws these rooms with a ~+24px left
#     margin, so the display origin sits well past the 320x200 bitmap's nominal
#     left border (Spider-Man lands at dx=56, beyond c64_decode_png.py's range).
#   * Edge-weighted scoring. The rooms are line art over a periodic 2px-checker
#     floor; that floor matches at *every* even X offset and easily fools a
#     flat pixel-count search into a wrong-but-plausible peak. We score the
#     alignment only on render pixels that sit on a real colour transition
#     (different from the pixel two columns left — which cancels the period-2
#     floor), so the true offset wins decisively.
#
#   c64a8_decode_png.py grid    <png> <spec> <bin> <out.c64> [dx dy]
#   c64a8_decode_png.py cmp     <png> <spec> <bin>           [dx dy]  align+score
#   c64a8_decode_png.py capture <png> <spec> <bin> <outdir>  [dx dy]  copy spec+dats+golden

import sys, os, struct, subprocess, shutil

from PIL import Image

UNSET = 0xffffffff

def load_png(path):
    im = Image.open(path).convert('RGB')
    return im.load(), im.size[0], im.size[1]

def render_grid(binpath, spec):
    out = '/tmp/_c64a8_cmp.grid'
    subprocess.run([binpath, 'grid', spec, out], check=True, stdout=subprocess.DEVNULL)
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

def edge_pixels(g, w, h):
    # Render pixels on a real colour transition (differ from the pixel 2 cols
    # left): cancels the period-2 checker floor, leaving line art / borders.
    e = []
    for y in range(min(h, 200)):
        for x in range(2, min(w, 320)):
            v = g[y][x]
            if v == UNSET:
                continue
            if g[y][x - 2] != UNSET and g[y][x - 2] != v:
                e.append((x, y, v))
    return e

def score(px, pw, ph, cols, pts, ox, oy):
    m = t = 0
    for x, y, v in pts:
        sx, sy = ox + x, oy + y
        if 0 <= sx < pw and 0 <= sy < ph:
            t += 1
            if nearest(px[sx, sy], cols) == v:
                m += 1
    return m, t

def best_offset(px, pw, ph, cols, g, w, h, xrange=72, yrange=56):
    edges = edge_pixels(g, w, h)
    if not edges:                       # featureless image: fall back to all px
        edges = [(x, y, g[y][x]) for y in range(min(h, 200))
                 for x in range(min(w, 320)) if g[y][x] != UNSET]
    best = (-1.0, 0, 0)
    for oy in range(yrange):
        for ox in range(xrange):
            m, t = score(px, pw, ph, cols, edges, ox, oy)
            if t and m / t > best[0]:
                best = (m / t, ox, oy)
    return best[1], best[2]

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
    png, spec, binpath = sys.argv[2], sys.argv[3], sys.argv[4]
    rest = sys.argv[5:]
    # optional trailing "dx dy" explicit offset (the last two args)
    explicit = None
    if len(rest) >= 2 and rest[-1].lstrip('-').isdigit() and rest[-2].lstrip('-').isdigit():
        explicit = (int(rest[-2]), int(rest[-1]))
        rest = rest[:-2]

    px, pw, ph = load_png(png)
    g, w, h = render_grid(binpath, spec)
    cols = renderer_colours(g, w, h)
    if explicit:
        ox, oy = explicit
    else:
        ox, oy = best_offset(px, pw, ph, cols, g, w, h)
    drawn = [(x, y, g[y][x]) for y in range(min(h, 200))
             for x in range(min(w, 320)) if g[y][x] != UNSET]
    m, t = score(px, pw, ph, cols, drawn, ox, oy)
    pct = 100 * m / max(t, 1)
    print(f"offset dx={ox} dy={oy}: {m}/{t} = {pct:.2f}%  ({len(cols)} colours)")

    if mode == 'cmp':
        return
    golden = build_golden(px, pw, ph, cols, g, w, h, ox, oy)
    if mode == 'grid':
        write_golden(rest[0], golden, w, h)
        print("wrote", rest[0])
    elif mode == 'capture':
        outdir = rest[0]
        if pct < 99.95:
            print(f"NO clean match ({pct:.2f}%) - composite incomplete or offset wrong")
            return
        os.makedirs(outdir, exist_ok=True)
        specdir = os.path.dirname(spec)
        shutil.copy(spec, os.path.join(outdir, os.path.basename(spec)))
        with open(spec) as f:
            for line in f:
                line = line.split('#')[0].strip()
                if not line or line.startswith('!'):
                    continue
                fn = line.split()[-1]
                shutil.copy(os.path.join(specdir, fn), os.path.join(outdir, fn))
        name = os.path.splitext(os.path.basename(spec))[0]
        write_golden(os.path.join(outdir, name + '.c64'), golden, w, h)
        print(f"captured {name}: {pct:.2f}% (dx={ox} dy={oy}) -> {outdir}")

if __name__ == '__main__':
    main()
