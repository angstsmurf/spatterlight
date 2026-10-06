#!/usr/bin/env python3
"""Tests for how the two disks of a two-disk game find each other.

  run_companion_tests.py [ID-PREFIX...]     check (all sets, or the named ones)
  run_companion_tests.py --bless [ID...]    rewrite companions.tsv
  run_companion_tests.py --list             one line per set

The runner is common_imagetest/run_companion_tests.py, which says what
companions.tsv holds. This script adds what is particular to this suite: the
probe (as in run_format_tests.py), and the file name pairs that
saga/atari8detect.c and saga/apple2detect.c know by heart, read from the
source, so that a pair added there is tested too.
"""

import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "common_imagetest"))
import run_image_tests as images  # noqa: E402
import run_companion_tests as companions  # noqa: E402
import run_format_tests as formats  # noqa: E402

SAGA = os.path.join(HERE, "..", "..", "saga")
# source file, name of the table, the extensions its pairs are tried on
DATABASES = (
    ("atari8detect.c", "a8companionlist", "atr"),
    ("apple2detect.c", "a2companionlist", "dsk,do,woz"),
)


def known_pairs():
    pairs = []
    for source, table, exts in DATABASES:
        with open(os.path.join(SAGA, source), encoding="utf-8") as f:
            text = f.read()
        body = re.search(table + r"\[\]\[2\] = \{(.*?)\n\};", text, re.S)
        if not body:
            sys.exit(f"{source}: no table {table}")
        found = re.findall(r'\{ \{ "([^"]+)", \d+ \}, \{ "([^"]+)", \d+ \} \}', body.group(1))
        if not found:
            sys.exit(f"{source}: no pairs in {table}")
        pairs += [(exts, os.path.splitext(a)[0], os.path.splitext(b)[0]) for a, b in found]
    return pairs


def main():
    ap = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    companions.add_args(ap, os.path.join(HERE, "games"))
    opt = ap.parse_args()
    suite = images.Suite(
        HERE, formats.PROBE,
        lambda probe, row, games: images.run_with_stdin(
            probe, row, games, formats.MARKER, formats.TIMEOUT),
        companion=None, label="format tests", item="image", id_width=44,
        not_built="scott_format_probe is not built: make -f Makefile.headless scott_format_probe")
    return companions.run_companions(suite, opt, known_pairs())


if __name__ == "__main__":
    sys.exit(main())
