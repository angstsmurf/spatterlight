#!/usr/bin/env python3
"""Fill-by-fill diff of the renderer vs the native op14 capture.

Pairs dosbox_capture_op14_v1.py's native VRAM dumps (state BEFORE each op14
fill) with graphics_magician_cga.cpp's GMCGA_TRACE_DIR snapshots (also state
BEFORE each fill) and reports the FIRST fill at which the two diverge -- i.e.
the fill whose output differs, the one to study in the span-traversal block.

Both sides dump the canvas *before* fill N executes, so if fills 1..k agree and
fill k+1's before-state diverges, fill k is the culprit.  We compare only the
280x160 picture window (native window origin = screen (20,0)).

  V1GAME=<dir> TARGET=RB:3 [BASE=RA:0] V1OUT=/tmp/op14cap python3 compare_op14.py

Run dosbox_capture_op14_v1.py first to populate V1OUT with fill_NNN.vram.
"""
import os, subprocess, sys

GAME   = os.environ.get("V1GAME", "/Users/administrator/Downloads/comprehend games/crimson-crown")
OUT    = os.environ.get("V1OUT", "/tmp/op14cap")
TARGET = os.environ.get("TARGET", "RB:3")
BASE   = os.environ.get("BASE", "")
HERE   = os.path.dirname(os.path.abspath(__file__))
TOOL   = os.path.join(HERE, "..", "gmcgav1test")
RTRACE = os.path.join(OUT, "render")

PIC_W, PIC_H = 280, 160


def vram_window(vram):
    """16 KB raw CGA -> 280x160 palette-index window at screen origin (20,0)."""
    win = bytearray(PIC_W * PIC_H)
    for y in range(PIC_H):
        base = (y & 1) * 0x2000 + (y >> 1) * 80
        for x in range(PIC_W):
            sx = x + 20
            b = vram[base + (sx >> 2)]
            win[y * PIC_W + x] = (b >> ((3 - (sx & 3)) * 2)) & 3   # MSB-first
    return win


def page_window(raw):
    """11200-byte s_screenmem (160x70, LSB-first) -> 280x160 index window."""
    win = bytearray(PIC_W * PIC_H)
    for y in range(PIC_H):
        for x in range(PIC_W):
            b = raw[y * 70 + (x >> 2)]
            win[y * PIC_W + x] = (b >> ((x & 3) * 2)) & 3           # LSB-first
    return win


def file_for_letter(prefix, letter):
    for cand in (f"{prefix}{letter}.MS1", f"{prefix.lower()}{letter.lower()}.ms1"):
        p = os.path.join(GAME, cand)
        if os.path.exists(p):
            return p
    return None


def parse(spec):
    tag, pic = spec.split(":")
    return tag[0].upper(), tag[1].upper(), int(pic)


def render_trace():
    """Run gmcgav1test with GMCGA_TRACE_DIR; return sorted fill_*.raw paths."""
    os.makedirs(RTRACE, exist_ok=True)
    for f in os.listdir(RTRACE):
        if f.endswith(".raw"):
            os.remove(os.path.join(RTRACE, f))
    kind, letter, pic = parse(TARGET)
    ms1 = file_for_letter("R" if kind == "R" else "O", letter)
    assert ms1, f"no MS1 for {TARGET}"
    env = dict(os.environ)
    env["GMCGA_TRACE_DIR"] = RTRACE
    cs = os.path.join(GAME, "CHARSET.GDA")
    if os.path.exists(cs):
        env["GMCGA_CHARSET"] = cs
    if BASE:
        bk, bl, bp = parse("R" + BASE if not BASE[0].isalpha() else BASE)
        env["GMCGA_BASE_MS1"] = file_for_letter("R", bl)
        env["GMCGA_BASE_PIC"] = str(bp)
    else:
        env.pop("GMCGA_BASE_MS1", None)
    r = subprocess.run([TOOL, os.path.join(GAME, "PC_GRAPH.OVR"),
                        os.path.join(GAME, "NOVEL.EXE"), ms1, str(pic),
                        "/tmp/_op14.ppm"], capture_output=True, text=True, env=env)
    if r.returncode != 0:
        print(r.stdout, r.stderr)
        sys.exit("gmcgav1test failed")
    befores = sorted(os.path.join(RTRACE, f) for f in os.listdir(RTRACE)
                     if f.endswith(".raw") and "_after" not in f)
    afters = sorted(os.path.join(RTRACE, f) for f in os.listdir(RTRACE)
                    if f.endswith("_after.raw"))
    return befores, afters


def diff_pair(nv_path, r_path):
    nwin = vram_window(open(nv_path, "rb").read())
    rwin = page_window(open(r_path, "rb").read())
    return [(p % PIC_W, p // PIC_W, rwin[p], nwin[p])
            for p in range(PIC_W * PIC_H) if rwin[p] != nwin[p]]


def report(label, diffs):
    if not diffs:
        print(f"{label}: match")
        return
    xs = [d[0] for d in diffs]
    ys = [d[1] for d in diffs]
    print(f"{label}: {len(diffs)} px differ  "
          f"bbox x={min(xs)}..{max(xs)} y={min(ys)}..{max(ys)}")
    for d in diffs[:24]:
        print(f"    x={d[0]:3d} y={d[1]:3d} render={d[2]} native={d[3]}")
    if len(diffs) > 24:
        print(f"    ... +{len(diffs)-24} more")


def main():
    nbefore = sorted(os.path.join(OUT, f) for f in os.listdir(OUT)
                     if f.startswith("fill_") and f.endswith(".vram")
                     and "_after" not in f)
    nafter = sorted(os.path.join(OUT, f) for f in os.listdir(OUT)
                    if f.endswith("_after.vram"))
    if not nbefore:
        sys.exit(f"no native fill_*.vram in {OUT}; run dosbox_capture_op14_v1.py")
    rbefore, rafter = render_trace()
    print(f"native fills: {len(nbefore)}   renderer fills: {len(rbefore)}")

    # Align the tail: native traces only the target's fills; the renderer trace
    # may be preceded by the base picture's fills.  Pair the last len(nbefore).
    if len(rbefore) < len(nbefore):
        sys.exit("renderer produced fewer fills than native -- stream mismatch")
    off = len(rbefore) - len(nbefore)
    if off:
        print(f"(skipping {off} leading renderer fills = base picture)")

    print("\n== BEFORE each fill (input canvas) ==")
    for i in range(len(nbefore)):
        report(f"fill #{i+1:3d} before", diff_pair(nbefore[i], rbefore[off + i]))

    if nafter and rafter:
        print("\n== AFTER each fill (the fill's own output) ==")
        for i in range(len(nafter)):
            d = diff_pair(nafter[i], rafter[off + i])
            report(f"fill #{i+1:3d} after ", d)
            if d:
                print(f"    ^ fill #{i+1} ITSELF diverges (before matched, "
                      f"after differs => the flood fill is the culprit)")


if __name__ == "__main__":
    main()
