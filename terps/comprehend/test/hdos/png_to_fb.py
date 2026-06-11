#!/usr/bin/env python3
"""png_to_fb.py -- turn a DOSBox NOVEL1.EXE screenshot into a ground-truth
framebuffer dump for the DOS Talisman CGA renderer regression tests.

DOSBox captures CGA mode 4 (320x200, 2bpp) as a 640x400 PNG: an exact 2x
nearest-neighbour blow-up, so only four distinct colours ever appear -- CGA
palette 1.  We collapse the 2x scale and map each colour to the renderer's own
palette index (see hdos_talisman.cpp kHdosColor / the "0=black 1=cyan 2=magenta
3=white" convention):

    000000 -> 0 (black)
    00aaaa -> 1 (cyan)       [low-intensity palette 1, as the real game uses]
    aa00aa -> 2 (magenta)
    aaaaaa -> 3 (white)

The high-intensity variants (55ffff / ff55ff / ffffff) map to the same indices,
so a machine=cga vs machine=svga capture both decode identically.

Output `.fb` is 320*200 = 64000 bytes, one index (0-3) per pixel, row-major.
This is the byte-exact golden; cropping to the renderer's 280x160 picture
window is done by the comparison harness, not here.

Usage: png_to_fb.py <in.png> <out.fb> [out.ppm]
"""
import sys
from PIL import Image

# CGA palette 1, both intensities, -> renderer index.
COLOR2IDX = {
    (0x00, 0x00, 0x00): 0,
    (0x00, 0xaa, 0xaa): 1, (0x55, 0xff, 0xff): 1,
    (0xaa, 0x00, 0xaa): 2, (0xff, 0x55, 0xff): 2,
    (0xaa, 0xaa, 0xaa): 3, (0xff, 0xff, 0xff): 3,
}
# index -> RGB for the PPM preview (use the low-intensity palette the game shows)
IDX2RGB = [(0, 0, 0), (0, 0xaa, 0xaa), (0xaa, 0, 0xaa), (0xaa, 0xaa, 0xaa)]

W, H = 320, 200


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    im = Image.open(sys.argv[1]).convert("RGB")
    sw, sh = im.size
    if (sw, sh) != (2 * W, 2 * H):
        sys.exit("expected a %dx%d screenshot, got %dx%d" % (2 * W, 2 * H, sw, sh))
    px = im.load()
    fb = bytearray(W * H)
    unknown = {}
    for y in range(H):
        for x in range(W):
            c = px[2 * x, 2 * y]
            idx = COLOR2IDX.get(c)
            if idx is None:
                unknown[c] = unknown.get(c, 0) + 1
                idx = 0
            fb[y * W + x] = idx
    if unknown:
        sys.stderr.write("warning: %d non-palette colours, e.g. %s\n" %
                         (len(unknown), list(unknown)[:4]))
    open(sys.argv[2], "wb").write(fb)
    if len(sys.argv) > 3:
        with open(sys.argv[3], "wb") as o:
            o.write(b"P6\n%d %d\n255\n" % (W, H))
            o.write(bytes(b for i in fb for b in IDX2RGB[i]))
    print("wrote %s (%d bytes)" % (sys.argv[2], len(fb)))


if __name__ == "__main__":
    main()
