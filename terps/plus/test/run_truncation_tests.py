#!/usr/bin/env python3
"""Truncated-file test for the binary database loader of the Plus interpreter.

  run_truncation_tests.py TESTDIR PROBE [ID-PREFIX...]
  run_truncation_tests.py --containers TESTDIR PROBE [ID-PREFIX...]

Runs the image probe with PLUS_PROBE_TRUNCATE set on every game of
TESTDIR/games.manifest.tsv (see ../../common_imagetest/run_image_tests.py).
The probe then loads the database of the game from copies cut short at a
thousand-odd lengths (see TruncationTest() in image_probe.c) and says what
came of it. A row passes if every cut copy was refused: none loaded, none
crashed the loader, none hung it.

There are no goldens. The MS-DOS games are plain text files read by another
loader and are skipped, as are rows whose game is not in games/.

With --containers, the files cut and damaged are the disk images themselves
(ATR, D64, DSK, DO, WOZ, MSA, ST and the .plus file), not the database in
them: each is cut short at a few dozen lengths, and copied with a byte
changed every few hundred, and the probe runs on every copy as it would on
the real one, other files of the game beside it. Whether a copy loads is not
the question; a row passes if no copy crashes the probe, sets off a
sanitizer or hangs it. (A damaged action table can loop for ever, as it
would on the original machine; MAX_SCAN_LINES in plusmain.c stops that.
With 40 damaged copies in place of 8, one copy of the ST Claymorgue did.)
That is only
worth much on a probe built with
AddressSanitizer (make -f Makefile.headless check-asan builds one):

  run_truncation_tests.py --containers test/images ./plus_image_probe_asan
"""

import concurrent.futures
import os
import random
import re
import shutil
import subprocess
import sys
import tempfile

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


CONTAINER_TIMEOUT = 120
CUTS = 24          # lengths each image is cut to, besides the short ones
DAMAGED = 8        # damaged copies of each image
DAMAGE_EVERY = 300  # about one byte in this many is changed
SANITIZER = re.compile(rb"Sanitizer|runtime error:")


def cut_lengths(size):
    lengths = {0, 1, 2, 16, 128, 256, 1024, size - 1}
    lengths.update(size * i // CUTS for i in range(1, CUTS))
    return sorted(n for n in lengths if 0 <= n < size)


def damage(data, seed):
    rng = random.Random(seed)
    data = bytearray(data)
    pos = rng.randrange(DAMAGE_EVERY)
    while pos < len(data):
        data[pos] ^= rng.randrange(1, 256)
        pos += rng.randrange(1, 2 * DAMAGE_EVERY)
    return bytes(data)


def run_copy(probe, row, games, name, data):
    """Runs the probe on a copy of the image of row holding data, in a
    folder of its own with the other files of the game linked beside it.
    Returns None if the probe came through, or what went wrong."""
    src = os.path.join(games, row.dir)
    with tempfile.TemporaryDirectory(prefix="plus-container-") as tmp:
        for other in os.listdir(src):
            if other != row.file:
                os.symlink(os.path.abspath(os.path.join(src, other)), os.path.join(tmp, other))
        path = os.path.join(tmp, row.file)
        with open(path, "wb") as f:
            f.write(data)
        try:
            done = subprocess.run(
                [probe, path] + ([row.keys] if row.keys else []), stdin=subprocess.DEVNULL,
                stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=CONTAINER_TIMEOUT,
                env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"))
        except subprocess.TimeoutExpired:
            return f"{name}: hung"
    found = SANITIZER.search(done.stderr)
    if found:
        line = done.stderr[done.stderr.rfind(b"\n", 0, found.start()) + 1:].split(b"\n")[0]
        return f"{name}: {line.decode('utf-8', 'replace').strip()}"
    if done.returncode < 0:
        return f"{name}: killed by signal {-done.returncode}"
    return None


def container_jobs(row, games):
    with open(os.path.join(games, row.dir, row.file), "rb") as f:
        data = f.read()
    for n in cut_lengths(len(data)):
        yield row, f"cut at {n}", data[:n]
    for seed in range(DAMAGED):
        yield row, f"damaged copy {seed}", damage(data, seed)


def containers(testdir, probe, prefixes):
    games = os.path.join(testdir, "games")
    rows = images.read_manifest(os.path.join(testdir, "games.manifest.tsv"))
    tests = [r for r in rows if r.id != images.COMPANION and not r.dir.startswith("dat-")
             and (not prefixes or any(r.id.startswith(p) for p in prefixes))]
    missing, wrong = images.check_corpus(rows, games)
    runnable = [r for r in tests if r.dir not in missing and r.dir not in wrong]
    if runnable and not os.access(probe, os.X_OK):
        sys.exit("no probe at %s; build it first" % probe)

    jobs = [job for row in runnable for job in container_jobs(row, games)]
    with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 4) as pool:
        results = list(pool.map(lambda j: run_copy(probe, j[0], games, j[1], j[2]), jobs))

    failures = {}
    for (row, _, _), got in zip(jobs, results):
        if got:
            failures.setdefault(row.id, []).append(got)
    for row in runnable:
        for got in failures.get(row.id, []):
            print(f"FAIL    {row.id}: {got}")
    print(f"damaged containers: {len(runnable) - len(failures)} passed, "
          f"{len(failures)} failed ({len(jobs)} copies), "
          f"{len(tests) - len(runnable)} skipped (game not in games/)")
    return 1 if failures else 0


def main():
    args = sys.argv[1:]
    if args[:1] == ["--containers"]:
        if len(args) < 3:
            sys.exit(__doc__)
        return containers(args[1], args[2], args[3:])
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
