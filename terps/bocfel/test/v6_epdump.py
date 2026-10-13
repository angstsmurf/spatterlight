#!/usr/bin/env python3
"""Snapshot of what find_entrypoints() decides for every V6 story revision.

The headless build (make -f Makefile.headless) prints, with BOCFEL_EP_DUMP
set, the hooked addresses, the contents of the per-game aggregates the
finders fill in and a checksum of the patched story.  This runs it on every
revision in ~/Downloads/{ZorkZeroes,Arthurs,Journeys,Shoguns} and on the
tracked blorbs, one file of stderr per story.

    v6_epdump.py --out before
    ... change entrypoints.cpp, make -f Makefile.headless ...
    v6_epdump.py --out after --compare before
"""

import glob
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
TERP = os.path.join(HERE, "..", "headless_obj", "bocfel_hl")
SOURCES = [os.path.expanduser("~/Downloads/%s/*.z6" % d)
           for d in ("ZorkZeroes", "Arthurs", "Journeys", "Shoguns")]
SOURCES.append(os.path.join(REPO, "SpatterlightTests", "Supporting Files",
                            "Games", "*.zblorb"))


def main(argv):
    out = compare = None
    while argv:
        option = argv.pop(0)
        if option == "--out":
            out = argv.pop(0)
        elif option == "--compare":
            compare = argv.pop(0)
        else:
            sys.exit(__doc__)
    if out is None:
        sys.exit(__doc__)
    os.makedirs(out, exist_ok=True)
    stories = sorted(s for pattern in SOURCES for s in glob.glob(pattern))
    changed = 0
    for story in stories:
        env = dict(os.environ, BOCFEL_EP_DUMP="1")
        try:
            result = subprocess.run([TERP, story], env=env, timeout=4,
                                    stdin=subprocess.DEVNULL,
                                    stdout=subprocess.DEVNULL,
                                    stderr=subprocess.PIPE)
            text = result.stderr.decode("utf-8", "replace")
            text += "exit %d\n" % result.returncode
        except subprocess.TimeoutExpired as hung:
            text = (hung.stderr or b"").decode("utf-8", "replace") + "hung\n"
        name = os.path.basename(story) + ".txt"
        with open(os.path.join(out, name), "w") as handle:
            handle.write(text)
        if compare:
            try:
                with open(os.path.join(compare, name)) as handle:
                    before = handle.read()
            except OSError:
                before = None
            if before != text:
                changed += 1
                print("CHANGED", name)
                if before is not None:
                    old, new = before.split("\n"), text.split("\n")
                    for a, b in zip(old, new):
                        if a != b:
                            print("   -", a[:150])
                            print("   +", b[:150])
                            break
    print("%d stories%s" % (len(stories), ", %d changed" % changed
                            if compare else ""))
    return 1 if changed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
