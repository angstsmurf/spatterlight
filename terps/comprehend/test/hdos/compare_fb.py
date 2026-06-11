#!/usr/bin/env python3
"""compare_fb.py -- diff a hdostest render against a DOSBox ground-truth .fb.

The renderer (hdostest) emits a 280x160 PPM in the renderer's index->RGB
palette.  The ground-truth .fb is the full 320x200 DOS framebuffer (one index
0-3 per pixel); the picture window sits at screen origin (20, 0), 280x160.

We compare in *index* space, so high- vs low-intensity palette doesn't matter.

Usage: compare_fb.py <render.ppm> <ground.fb> [diff.png]
Exit 0 iff identical over the 280x160 window.
"""
import sys

W, H = 320, 200
PW, PH = 280, 160
OX, OY = 20, 0

# renderer PPM palette (hdos_talisman kHdosColor) -> index; both CGA palette-1
# intensities are accepted (the game uses low intensity: 00aaaa/aa00aa/aaaaaa)
RGB2IDX = {
    (0, 0, 0): 0,
    (0x55, 0xff, 0xff): 1, (0x00, 0xaa, 0xaa): 1,
    (0xff, 0x55, 0xff): 2, (0xaa, 0x00, 0xaa): 2,
    (0xff, 0xff, 0xff): 3, (0xaa, 0xaa, 0xaa): 3,
}


def read_ppm_idx(path):
    d = open(path, "rb").read()
    assert d[:2] == b"P6", "not a P6 ppm"
    # parse header: P6\n<w> <h>\n<max>\n
    i = 2
    fields = []
    while len(fields) < 3:
        while d[i] in b" \t\n\r":
            i += 1
        s = i
        while d[i] not in b" \t\n\r":
            i += 1
        fields.append(int(d[s:i]))
    i += 1  # single whitespace after maxval
    w, h, _ = fields
    px = d[i:]
    out = bytearray(w * h)
    for j in range(w * h):
        c = (px[3 * j], px[3 * j + 1], px[3 * j + 2])
        out[j] = RGB2IDX.get(c, 0)
    return w, h, out


def main():
    rppm, gfb = sys.argv[1], sys.argv[2]
    rw, rh, r = read_ppm_idx(rppm)
    assert (rw, rh) == (PW, PH), "render is %dx%d, expected %dx%d" % (rw, rh, PW, PH)
    g = open(gfb, "rb").read()
    assert len(g) == W * H, "ground fb wrong size"

    diffs = 0
    diffpix = []
    for y in range(PH):
        for x in range(PW):
            rv = r[y * PW + x]
            gv = g[(OY + y) * W + (OX + x)]
            if rv != gv:
                diffs += 1
                if len(diffpix) < 20:
                    diffpix.append((x, y, rv, gv))
    total = PW * PH
    print("diffs: %d / %d  (%.3f%%)" % (diffs, total, 100.0 * diffs / total))
    if diffpix:
        print("first mismatches (x,y,render,ground):")
        for p in diffpix:
            print("  ", p)

    if len(sys.argv) > 3:
        try:
            from PIL import Image
        except ImportError:
            return 1 if diffs else 0
        im = Image.new("RGB", (PW, PH))
        pal = [(0, 0, 0), (0, 0xaa, 0xaa), (0xaa, 0, 0xaa), (0xaa, 0xaa, 0xaa)]
        out = im.load()
        for y in range(PH):
            for x in range(PW):
                rv = r[y * PW + x]
                gv = g[(OY + y) * W + (OX + x)]
                out[x, y] = (255, 0, 0) if rv != gv else pal[gv]
        im.save(sys.argv[3])
        print("wrote diff image", sys.argv[3])
    return 1 if diffs else 0


if __name__ == "__main__":
    sys.exit(main())
