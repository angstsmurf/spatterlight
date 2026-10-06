#!/usr/bin/env python3
# Decode a VICE screenshot of a C64 / Atari 8-bit SAGA room into a golden grid
# for c64test (mini-C64 "tiny" rooms) or c64a8test (the SagaPlus full bitmaps —
# Spider-Man, Buckaroo Banzai, …), and match/compare it against the C
# renderer's composited output (<bin> grid <spec>).
#
# A room as displayed is a composite: a background bitmap + object overlays (+
# monochrome white item sprites on the mini-C64; see groundtruth_c64/README.md).
# The C renderer replays that composite and emits each pixel's *displayed
# colour* (C64R1 RGB grid). We align the screenshot to that render, then
# classify every VICE pixel to the nearest renderer colour and write the golden
# in the renderer's own colour space (C64C2) — so `<bin> cmp` is a pure
# RGB-equality check and the test stays self-contained. Pixels are 1:1.
#
# The VICE frame carries a border, so the C64 display origin is found by an
# offset search (reported), with one of two alignments:
#
#   --align flat (default; c64test) — try every offset in 48x48 and keep the one
#     where most drawn render pixels match.
#   --align edge (c64a8test; c64a8_decode_png.py) — differs in two ways:
#     * Wider X search (72x56). The original C64 game draws these rooms with a
#       ~+24px left margin, so the display origin sits well past the 320x200
#       bitmap's nominal left border (Spider-Man lands at dx=56).
#     * Edge-weighted scoring. The rooms are line art over a periodic 2px-checker
#       floor; that floor matches at *every* even X offset and easily fools a
#       flat pixel-count search into a wrong-but-plausible peak. We score the
#       alignment only on render pixels that sit on a real colour transition
#       (different from the pixel two columns left — which cancels the period-2
#       floor), so the true offset wins decisively.
#
#   c64_decode_png.py [--align flat|edge] grid    <png> <spec> <bin> <out.c64> [dx dy]
#   c64_decode_png.py [--align flat|edge] cmp     <png> <spec> <bin>           [dx dy]
#   c64_decode_png.py [--align flat|edge] capture <png> <spec> <bin> <outdir>  [dx dy]
#
# A trailing "dx dy" skips the search and uses that offset. `capture` copies
# the spec, every .dat it references (spec `!directives` are skipped) and the
# golden into <outdir>.

import sys, os, struct, subprocess, shutil

from PIL import Image

UNSET = 0xffffffff

def load_png(path):
    im = Image.open(path).convert('RGB')
    return im.load(), im.size[0], im.size[1]

def render_grid(binpath, spec, out):
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

def drawn_pixels(g, w, h):
    return [(x, y, g[y][x]) for y in range(min(h, 200)) for x in range(min(w, 320))
            if g[y][x] != UNSET]

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

def best_offset_flat(px, pw, ph, cols, g, w, h, search=48):
    """Most matching drawn pixels wins. Returns (m, t, ox, oy)."""
    drawn = drawn_pixels(g, w, h)
    best = (-1, 0, 0, 0)
    for oy in range(search):
        for ox in range(search):
            m, t = score(px, pw, ph, cols, drawn, ox, oy)
            if t and m > best[0]:
                best = (m, t, ox, oy)
    return best

def best_offset_edge(px, pw, ph, cols, g, w, h, xrange=72, yrange=56):
    """Best match ratio over the edge pixels wins. Returns (ox, oy)."""
    edges = edge_pixels(g, w, h)
    if not edges:                       # featureless image: fall back to all px
        edges = drawn_pixels(g, w, h)
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
    # Compact palette+index format: "C64C2", w, h, npal (int32 LE), npal*3 RGB
    # bytes, then w*h index bytes (0xff = UNSET). Colours are the renderer's own,
    # so `cmp` is an exact RGB match.
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

# Per-alignment scratch file for the render grid (kept as the old scripts had
# them, so a flat and an edge run can go side by side).
GRID_TMP = {'flat': '/tmp/_c64_cmp.grid', 'edge': '/tmp/_c64a8_cmp.grid'}

def main(argv=None, align='flat'):
    args = list(sys.argv[1:] if argv is None else argv)
    if args and args[0].startswith('--align'):
        if '=' in args[0]:
            align = args.pop(0).split('=', 1)[1]
        else:
            args.pop(0)
            align = args.pop(0)
    if align not in GRID_TMP:
        sys.exit(f"unknown --align {align} (flat or edge)")
    mode = args[0]
    png, spec, binpath = args[1], args[2], args[3]
    rest = args[4:]
    # optional trailing "dx dy" explicit offset (the last two args)
    explicit = None
    if len(rest) >= 2 and rest[-1].lstrip('-').isdigit() and rest[-2].lstrip('-').isdigit():
        explicit = (int(rest[-2]), int(rest[-1]))
        rest = rest[:-2]

    px, pw, ph = load_png(png)
    g, w, h = render_grid(binpath, spec, GRID_TMP[align])
    cols = renderer_colours(g, w, h)
    if align == 'flat' and not explicit:
        m, t, ox, oy = best_offset_flat(px, pw, ph, cols, g, w, h)
        label = "best offset"
    else:
        if explicit:
            ox, oy = explicit
        else:
            ox, oy = best_offset_edge(px, pw, ph, cols, g, w, h)
        m, t = score(px, pw, ph, cols, drawn_pixels(g, w, h), ox, oy)
        label = "offset"
    pct = 100 * m / max(t, 1)
    print(f"{label} dx={ox} dy={oy}: {m}/{t} = {pct:.2f}%  ({len(cols)} colours)")

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
        # Copy the spec and every .dat it references into the corpus dir.
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
