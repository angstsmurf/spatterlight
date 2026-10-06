#!/usr/bin/env python3
"""Regression tests for the disk and tape image loaders.

Every row of games.manifest.tsv names one game image and, for the images that
hold several games, the menu answer. The image is run through
scott_format_probe, which prints a fingerprint of everything DetectGame()
loaded from it, and the fingerprint is compared with expected/<id>.txt.

The images are copyrighted and not in the repository. Rows whose image is not
in games/ are skipped, so the suite passes (testing nothing) on a bare
checkout. See README.md.

  run_format_tests.py [ID-PREFIX...]      check (all rows, or the named ones)
  run_format_tests.py --bless [ID...]     rewrite the goldens
  run_format_tests.py --collect DIR...    find the images under DIR and copy
                                          them into games/
  run_format_tests.py --list              one line per row: status, id, note

The runner itself is common_imagetest/run_image_tests.py; this script only
describes how this suite differs (probe, marker, menu answer on stdin).
"""

import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "common_imagetest"))
import run_image_tests as runner  # noqa: E402

PROBE = os.path.join(HERE, "..", "..", "scott_format_probe")
MARKER = "\n== fingerprint\n"
TIMEOUT = 60


def main():
    ap = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    runner.add_common_args(ap, "where the images are (default: games/ next to this script)",
                           os.path.join(HERE, "games"))
    opt = ap.parse_args()
    suite = runner.Suite(
        HERE, PROBE,
        lambda probe, row, games: runner.run_with_stdin(probe, row, games, MARKER, TIMEOUT),
        companion=None, label="format tests", item="image", thing="image", id_width=44,
        readme="test/formats/README.md",
        not_built="scott_format_probe is not built: make -f Makefile.headless scott_format_probe")
    return runner.run_suite(suite, opt, ap)


if __name__ == "__main__":
    sys.exit(main())
