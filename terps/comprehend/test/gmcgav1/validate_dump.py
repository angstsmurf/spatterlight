#!/usr/bin/env python3
"""Validate a dump produced by dosbox_dump_all_v1.py against the renderer.

For every row in <OUT>/manifest.tsv, render the picture with gmcgav1test (drawing
the recorded base room first for object overlays) and compare the 280x160 picture
window against the DOSBox .fb golden.  Reports per-picture mismatch counts.

  V1GAME=<game dir> V1OUT=<dump dir> python3 validate_dump.py [ceiling]

ceiling (default 0) = max mismatches still counted as a pass.
"""
import os, subprocess, sys

GAME = os.environ.get("V1GAME", "/Users/administrator/Downloads/comprehend games/crimson-crown")
OUT  = os.environ.get("V1OUT", "/tmp/v1cap/sweep")
TOOL = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "gmcgav1test")
CEIL = int(sys.argv[1]) if len(sys.argv) > 1 else 0


def base_file_pic(base_id):
    if not base_id:
        return None, None
    fi = (base_id - 1) >> 4
    return f"R{chr(ord('A') + fi)}.MS1", (base_id - 1) & 15


def main():
    ovr = os.path.join(GAME, "PC_GRAPH.OVR")
    exe = os.path.join(GAME, "NOVEL.EXE")
    rows = open(os.path.join(OUT, "manifest.tsv")).read().splitlines()[1:]
    charset = os.path.join(GAME, "CHARSET.GDA")   # in-picture font (op3/op5 text)
    npass = nfail = 0
    worst = []
    for line in rows:
        name, tag, fileTag, pic, pic_id, base_id = line.split("\t")
        ms1 = os.path.join(GAME, fileTag + ".MS1")
        fb = os.path.join(OUT, name + ".fb")
        if not os.path.exists(fb):
            print(f"{name:10} MISSING .fb"); nfail += 1; continue
        env = dict(os.environ)
        if os.path.exists(charset):
            env["GMCGA_CHARSET"] = charset
        bfile, bpic = base_file_pic(int(base_id))
        if bfile:
            env["GMCGA_BASE_MS1"] = os.path.join(GAME, bfile)
            env["GMCGA_BASE_PIC"] = str(bpic)
        else:
            env.pop("GMCGA_BASE_MS1", None)
        r = subprocess.run([TOOL, ovr, exe, ms1, pic, "/tmp/_v.ppm", fb],
                           capture_output=True, text=True, env=env)
        m = None
        for ln in r.stdout.splitlines():
            if ln.startswith("compare:"):
                m = int(ln.split()[1])
        if m is None:
            print(f"{name:10} NO COMPARE ({r.stdout.strip()[:60]})"); nfail += 1; continue
        ok = m <= CEIL
        npass += ok; nfail += (not ok)
        base_s = f" base={bfile}:{bpic}" if bfile else ""
        if not ok:
            worst.append((m, name, base_s))
        print(f"{name:10} {m:6d} mismatch{base_s}  {'ok' if ok else 'FAIL'}")
    print("=" * 50)
    print(f"PASS {npass}  FAIL {nfail}  (ceiling {CEIL})")
    for m, name, b in sorted(worst, reverse=True)[:15]:
        print(f"  worst: {name:10} {m:6d}{b}")


if __name__ == "__main__":
    main()
