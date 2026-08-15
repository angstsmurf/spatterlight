#!/usr/bin/env python3
"""Print the FIRST divergence in each out/*.diff, with the turn that led to it.

A single early disagreement desynchronises the rest of a replay -- the two
engines are being fed the same script, so once one of them is a turn ahead
every later line differs and the diff stops meaning anything.  triage.py
measures the whole diff; this measures only its head, which is the part that
has a cause.  Read this first, fix what it shows, and re-run.

Usage: firstdiff.py [label ...]
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "out")


def first_hunk(path):
    """(preceding command, [changed lines]) for the diff's first real change."""
    lines = open(path, encoding="utf-8", errors="replace").read().split("\n")
    cmd, changed = "", []
    for line in lines:
        if line[:3] in ("---", "+++") or line.startswith("@@"):
            continue
        if line[:1] == " ":
            if changed:
                break
            # The "> ..." echo is the turn this output belongs to.
            if line[1:].startswith("> "):
                cmd = line[1:]
            continue
        if line[:1] in ("+", "-"):
            changed.append(line)
        elif changed:
            break
    return cmd, changed


def main():
    want = set(sys.argv[1:])
    for name in sorted(os.listdir(OUT)):
        if not name.endswith(".diff"):
            continue
        label = name[:-5]
        if want and label not in want:
            continue
        cmd, changed = first_hunk(os.path.join(OUT, name))
        if not changed:
            continue
        print("=== %s   %s" % (label, cmd))
        for l in changed[:12]:
            print("   " + l[:150])
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
