#!/usr/bin/env python3
"""Truncated-file test for the binary database loader of the Plus interpreter.

  run_truncation_tests.py TESTDIR PROBE [ID-PREFIX...]

Runs the image probe with PLUS_PROBE_TRUNCATE set on every game of
TESTDIR/games.manifest.tsv (see ../../common_imagetest/run_image_tests.py).
The probe then loads the database of the game from copies cut short at a
thousand-odd lengths (see TruncationTest() in image_probe.c) and says what
came of it. A row passes if every cut copy was refused: none loaded, none
crashed the loader, none hung it.

There are no goldens. The MS-DOS games are plain text files read by another
loader and are skipped, as are rows whose game is not in games/.
"""

import concurrent.futures
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "..", "common_imagetest"))
import run_image_tests as images

TIMEOUT = 600
RESULT = re.compile(r"^truncate: (.*)$", re.M)
CLEAN = re.compile(r"database \d+ bytes, \d+ cuts refused, 0 loaded, 0 crashed, 0 hung$")


def run(probe, row, games):
    path = os.path.join(games, row.dir, row.file)
    try:
        done = subprocess.run(
            [probe, path] + ([row.keys] if row.keys else []), stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=TIMEOUT,
            env=dict(os.environ, PLUS_PROBE_TRUNCATE="1"))
    except subprocess.TimeoutExpired:
        return "timed out"
    found = RESULT.search(done.stdout.decode("utf-8", "replace"))
    return found.group(1) if found else "the probe said nothing (exit %d)" % done.returncode


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    testdir, probe, prefixes = sys.argv[1], sys.argv[2], sys.argv[3:]
    games = os.path.join(testdir, "games")
    rows = images.read_manifest(os.path.join(testdir, "games.manifest.tsv"))
    tests = [r for r in rows if r.id != images.COMPANION
             and (not prefixes or any(r.id.startswith(p) for p in prefixes))]
    missing, wrong = images.check_corpus(rows, games)
    runnable = [r for r in tests if r.dir not in missing and r.dir not in wrong]
    if runnable and not os.access(probe, os.X_OK):
        sys.exit("no probe at %s; build it first" % probe)

    with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 4) as pool:
        results = list(pool.map(lambda r: run(probe, r, games), runnable))

    passed = failed = text = 0
    for row, got in zip(runnable, results):
        if got == "not a binary database":
            text += 1
        elif CLEAN.match(got):
            passed += 1
        else:
            failed += 1
            print(f"FAIL    {row.id}: {got}")
    print(f"truncated databases: {passed} passed, {failed} failed, "
          f"{text} plain text games not tried, {len(tests) - len(runnable)} skipped "
          f"(game not in games/)")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
