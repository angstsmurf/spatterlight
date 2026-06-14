#!/usr/bin/env python3
# Decode a DOSBox CGA screenshot of a SAGA DOS room into a 320x200 slot grid
# (2-bit CGA colour index per pixel), and optionally match/compare it against the
# C renderer's output (dostest grid) for one or all .PAK files.
#
# CGA mode 4 palette 1 (low intensity):
#   black (0,0,0)=0  cyan (0,170,170)=1  magenta (170,0,170)=2  gray (170,170,170)=3
# The .PAK stores these indices directly and both the DOS game and the C renderer
# (pcdraw.c) emit them unchanged, so the comparison is a direct index match.
#
#   dos_decode_png.py grid   <shot.png> <out.cga>            decode PNG -> slot grid
#   dos_decode_png.py match  <shot.png> <dostest> <pakdir>   find best-matching .PAK
#   dos_decode_png.py cmp    <shot.png> <dostest> <pak>      compare one .PAK

import sys, os, struct, subprocess
from PIL import Image

CGA = {(0,0,0):0, (0,170,170):1, (170,0,170):2, (170,170,170):3}

def nearest(rgb):
    best, bd = 0, 1<<30
    for c,i in CGA.items():
        d = sum((a-b)**2 for a,b in zip(rgb,c))
        if d < bd: bd, best = d, i
    return best

def decode_png(path):
    im = Image.open(path).convert('RGB')
    w,h = im.size
    sx, sy = w//320, h//200          # DOSBox usually doubles to 640x400
    if sx < 1: sx = 1
    if sy < 1: sy = 1
    px = im.load()
    grid = [[0]*320 for _ in range(200)]
    for y in range(200):
        for x in range(320):
            grid[y][x] = nearest(px[x*sx, y*sy])
    return grid

def load_dostest_grid(path):
    with open(path,'rb') as f:
        data = f.read()
    assert data[:5] == b'DGRID', "bad grid magic"
    w,h = struct.unpack_from('<ii', data, 5)
    body = data[13:]
    g = [[0]*w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            b = body[y*w+x]
            g[y][x] = -1 if b == 0xff else b
    return g, w, h

def render_pak(dostest, pak):
    out = '/tmp/_dos_cmp.grid'
    subprocess.run([dostest, 'grid', pak, out], check=True,
                   stdout=subprocess.DEVNULL)
    return load_dostest_grid(out)

def compare(cga, cg):
    # Compare only where the C renderer drew (its bbox of set pixels).
    g, w, h = cg
    match = total = 0
    for y in range(min(h,200)):
        for x in range(min(w,320)):
            v = g[y][x]
            if v < 0:
                continue
            total += 1
            if v == cga[y][x]:
                match += 1
    return match, total

def main():
    mode = sys.argv[1]
    if mode == 'grid':
        cga = decode_png(sys.argv[2])
        with open(sys.argv[3],'wb') as f:
            f.write(b'CGA00'); f.write(struct.pack('<ii',320,200))
            for row in cga: f.write(bytes(row))
        print("wrote", sys.argv[3]); return
    if mode == 'cmp':
        cga = decode_png(sys.argv[2])
        m,t = compare(cga, render_pak(sys.argv[3], sys.argv[4]))
        print(f"{os.path.basename(sys.argv[4])}: {m}/{t} = {100*m/max(t,1):.2f}%")
        return
    if mode == 'capture':
        # capture <shot.png> <dostest> <pakdir> <outdir>
        # Identify the .PAK the screenshot shows (room images only, fixed dx=24),
        # and if it matches exactly, copy the .PAK + write its golden .cga.
        import shutil
        cga = decode_png(sys.argv[2])
        dostest, pakdir, outdir = sys.argv[3], sys.argv[4], sys.argv[5]
        os.makedirs(outdir, exist_ok=True)
        DX = 24
        best = (0.0, None)
        for fn in sorted(os.listdir(pakdir)):
            if not fn.upper().endswith('.PAK') or not fn.upper().startswith('R'):
                continue
            g, w, h = render_pak(dostest, os.path.join(pakdir, fn))
            m = t = 0
            for y in range(min(h, 200)):
                for x in range(min(w, 320 - DX)):
                    v = g[y][x]
                    if v < 0: continue
                    t += 1; m += (v == cga[y][x + DX])
            if t and m / t > best[0]:
                best = (m / t, fn, m, t)
        if best[1] and best[0] >= 0.999:
            fn = best[1]
            shutil.copy(os.path.join(pakdir, fn), os.path.join(outdir, fn))
            with open(os.path.join(outdir, fn[:-4] + '.cga'), 'wb') as f:
                f.write(b'CGA00'); f.write(struct.pack('<ii', 320, 200))
                for row in cga: f.write(bytes(row))
            print(f"captured {fn}: {best[2]}/{best[3]} = 100% -> {outdir}")
        else:
            print(f"NO clean match (best {best[1]} {best[0]*100:.1f}%) - composite or unseen room")
        return
    if mode == 'match':
        cga = decode_png(sys.argv[2])
        dostest, pakdir = sys.argv[3], sys.argv[4]
        res = []
        for fn in sorted(os.listdir(pakdir)):
            if not fn.upper().endswith('.PAK'): continue
            try:
                m,t = compare(cga, render_pak(dostest, os.path.join(pakdir,fn)))
            except Exception as e:
                continue
            if t: res.append((100*m/t, m, t, fn))
        res.sort(reverse=True)
        for pct,m,t,fn in res[:8]:
            print(f"  {fn:12s} {m:5d}/{t:5d} = {pct:6.2f}%")
        return
    print("unknown mode", mode); sys.exit(2)

if __name__ == '__main__':
    main()
