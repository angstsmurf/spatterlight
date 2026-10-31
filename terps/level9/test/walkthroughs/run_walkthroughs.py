#!/usr/bin/env python3
"""Play the full-game walkthroughs in level9_hl and check that each one ends.

  run_walkthroughs.py [NAME...]

Each row of walkthroughs.tsv names a script, the image it plays, the last score
it reports ("none" for a game without one) and a phrase from its ending. Images
are found under $L9_GAMES, default "~/Downloads/Level 9 stuff"; multi-part games
need their other files beside them. A row whose image is missing or differs is
skipped.
"""

import hashlib
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
HL = os.path.join(HERE, "..", "..", "level9_hl")
GAMES = os.environ.get("L9_GAMES",
                       os.path.expanduser("~/Downloads/Level 9 stuff"))
SCORE = re.compile(rb"(?i)(?:scored?|score is)\s+(\d+)\s*(?:out of(?: about)?|/)\s*(\d+)")


def play(name, image, cwd):
    with open(os.path.join(HERE, name + ".txt"), "rb") as script:
        return subprocess.run([HL, image], stdin=script, cwd=cwd, timeout=120,
                              capture_output=True).stdout


def main():
    wanted = sys.argv[1:]
    passed = failed = skipped = 0
    with open(os.path.join(HERE, "walkthroughs.tsv"), encoding="utf-8") as rows:
        rows = [l.rstrip("\n").split("\t") for l in rows if not l.startswith("#")]
    for name, score, ending, sha, rel in rows:
        if wanted and name not in wanted:
            continue
        image = os.path.join(GAMES, rel)
        if not os.path.exists(image) or \
           hashlib.sha256(open(image, "rb").read()).hexdigest() != sha:
            print(f"SKIP {name}: no matching {rel}")
            skipped += 1
            continue
        # A temporary folder, because a script that goes wrong can save a game.
        with tempfile.TemporaryDirectory() as cwd:
            out = play(name, os.path.abspath(image), cwd)
        scores = SCORE.findall(out)
        got = (scores[-1][0] + b"/" + scores[-1][1]).decode() if scores else "none"
        text = re.sub(rb"\s+", b" ", out)
        if got == score and ending.encode() in text:
            print(f"ok   {name}: {got}")
            passed += 1
        else:
            print(f"FAIL {name}: score {got}, expected {score}; "
                  f"ending {'found' if ending.encode() in text else 'missing'}")
            failed += 1
    print(f"walkthroughs: {passed} passed, {failed} failed, {skipped} skipped")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
