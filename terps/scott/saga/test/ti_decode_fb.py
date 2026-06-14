#!/usr/bin/env python3
# Decode a MAME ti99_4a framebuffer dump into the RTPI picture as a grid of
# TMS9918 palette indices (0..15), and optionally pixel-compare it against a
# titest .vdp render.  This is the *approach B* (final-displayed-pixels) golden
# path for the TI-99/4A RTPI renderer: it is invariant to the on-hardware VDP
# bank / raster-split tricks that make a raw VRAM pattern-table compare useless
# (see groundtruth_ti99/README.md).
#
# Capturing the framebuffer under MAME (mame-mcp), with the game paused on the
# target room:
#
#     local s; for _,scr in pairs(manager.machine.screens) do s=scr end
#     local f=io.open("/tmp/ti_fb.bin","wb"); f:write(s:pixels()); f:close()
#
# s:pixels() returns width*height little-endian 32-bit 0xXXRRGGBB pixels. The
# ti99_4a screen is 280x216 with a 12px border, so the 256x192 active area is at
# offset (12,12); the RTPI picture is the top 96 rows of that.
#
#   ti_decode_fb.py <fb.bin>                                -> write fb_pic.png
#   ti_decode_fb.py <fb.bin> grid <out.pgrid>               -> write golden grid
#   ti_decode_fb.py <fb.bin> cmp <render.vdp>               -> pixel match %
#   ti_decode_fb.py <fb.bin> best <dir-of-*.vdp>            -> rank renders

import struct, sys, glob, os

# TMS9918 palette as used by rtpi_graphics.c:SetupRTPIColors (index 0..15).
PAL = [(0,0,0),(0,0,0),(0x21,0xc8,0x42),(0x5e,0xdc,0x78),(0x50,0x51,0xe0),
       (0x7f,0x77,0xf8),(0xe1,0x5d,0x53),(0x42,0xeb,0xf5),(0xff,0x65,0x5c),
       (0xff,0x84,0x7e),(0xd4,0xc1,0x54),(0xea,0xd0,0x87),(0,0xae,0x3f),
       (0xc9,0x5b,0xba),(0xcd,0xcd,0xcd),(0xff,0xff,0xff)]

PIC_W, PIC_H = 256, 96   # RTPI picture: 32x12 tiles

def _nearest(c):
    best, bd = 0, 1 << 30
    for i, p in enumerate(PAL):
        d = sum((a-b)*(a-b) for a, b in zip(c, p))
        if d < bd:
            bd, best = d, i
    return best

def _norm(i):
    return 0 if i == 1 else i   # palette 0 and 1 are both black

def fb_grid(path, W=280, H=216):
    raw = open(path, "rb").read()
    ox, oy = (W-256)//2, (H-192)//2
    g = []
    for y in range(PIC_H):
        row = []
        for x in range(PIC_W):
            v = struct.unpack_from("<I", raw, ((oy+y)*W + ox+x)*4)[0]
            row.append(_norm(_nearest(((v>>16)&0xff, (v>>8)&0xff, v&0xff))))
        g.append(row)
    return g

def vdp_grid(path):
    d = open(path, "rb").read()
    px, co = d[:0xC00], d[0xC00:0x1800]
    T = [[0]*PIC_W for _ in range(PIC_H)]
    ptr = x = y = 0
    while y < PIC_H and ptr < 0xC00:
        for line in range(8):
            byte, attr = px[ptr], co[ptr]; ptr += 1
            fg, bg = attr >> 4, attr & 0xf
            for q in range(8):
                T[y+line][x+q] = _norm(fg if (byte & (0x80 >> q)) else bg)
        x += 8
        if x >= PIC_W:
            x, y = 0, y + 8
    return T

def match(a, b):
    same = sum(1 for y in range(PIC_H) for x in range(PIC_W) if a[y][x] == b[y][x])
    return same, PIC_W*PIC_H

def save_png(grid, path):
    from PIL import Image
    im = Image.new("RGB", (PIC_W, PIC_H))
    for y in range(PIC_H):
        for x in range(PIC_W):
            im.putpixel((x, y), PAL[grid[y][x]])
    im.save(path)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__); sys.exit(2)
    fb = sys.argv[1]
    if len(sys.argv) >= 4 and sys.argv[2] == "grid":
        g = fb_grid(fb)
        with open(sys.argv[3], "wb") as f:
            f.write(bytes(g[y][x] for y in range(PIC_H) for x in range(PIC_W)))
        print("wrote", sys.argv[3])
    elif len(sys.argv) >= 4 and sys.argv[2] == "cmp":
        g, t = fb_grid(fb), vdp_grid(sys.argv[3])
        s, tot = match(g, t)
        print("%d/%d pixels match (%.1f%%)" % (s, tot, 100*s/tot))
    elif len(sys.argv) >= 4 and sys.argv[2] == "best":
        g = fb_grid(fb)
        res = []
        for f in sorted(glob.glob(os.path.join(sys.argv[3], "*.vdp"))):
            s, tot = match(g, vdp_grid(f))
            res.append((s, f))
        res.sort(reverse=True)
        for s, f in res[:8]:
            print("%.1f%%  %s" % (100*s/(PIC_W*PIC_H), os.path.basename(f)))
    else:
        out = os.path.splitext(fb)[0] + "_pic.png"
        save_png(fb_grid(fb), out)
        print("wrote", out)
