#!/usr/bin/env python3
"""What the interpreter does with a damaged game file.

  damaged.py TESTDIR HL [ID-PREFIX...]

Two checks, on damaged copies of the games in TESTDIR/games/ (the corpus of
the picture tests, see README.md), run through the text build HL (taylor_hl):

  text only  A file whose game data is whole but whose pictures cannot be
             read must say so and play on without them: the rows of FALLBACK
             below, each a game and what is done to it.

  cut short  Every game file, cut off at each eighth of its length, must end
             in an orderly way: a game that plays, or an error message and an
             exit. A crash or a hang is a failure.

Rows whose game is not in TESTDIR/games/ are skipped, as in the other suites.
"""

import concurrent.futures
import os
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "..", "..", "common_imagetest"))
import run_image_tests as images

TIMEOUT = 60
NOTICE = b"[The pictures in this file could not be read. Playing without graphics.]"
COMMAND = b"INVENTORY"
EIGHTHS = range(1, 8)


def truncate(length):
    return lambda data: data[:length]


def poke(offset, was, now):
    def damage(data):
        if data[offset] != was:
            sys.exit(f"damaged.py: byte {offset} is not {was:#x}: not the file this row was written for")
        return data[:offset] + bytes([now]) + data[offset + 1:]
    return damage


# A Spectrum snapshot is the memory from 0x4000 up after a short header, and
# these games keep some of their picture data above the game itself: cut off
# at 34000 bytes, the game is whole and the pictures are not. (Much shorter
# and the game's own tables are gone, which is a fatal error.) Questprobe 3
# finds its picture tables by a signature, which is spoiled here.
FALLBACK = [
    ("sna-rebel-planet", "cut off at 34000 bytes", truncate(34000)),
    ("sna-kayleth", "cut off at 34000 bytes", truncate(34000)),
    ("sna-heman", "cut off at 34000 bytes", truncate(34000)),
    ("sna-temple-of-terror", "cut off at 34000 bytes", truncate(34000)),
    ("sna-blizzard-pass", "cut off at 34000 bytes", truncate(34000)),
    ("sna-questprobe-3", "picture table signature spoiled", poke(26845, 0xa7, 0x58)),
]


def run(hl, row, games, damage):
    """Play a damaged copy of the game for one command. Returns the exit
    status (negative for a signal, None for a hang) and the transcript."""
    with open(os.path.join(games, row.dir, row.file), "rb") as f:
        data = damage(f.read())
    with tempfile.TemporaryDirectory() as cwd:
        # The name is kept: the loaders look at the extension.
        path = os.path.join(cwd, row.file)
        with open(path, "wb") as f:
            f.write(data)
        keys = "".join(k + "\n" for k in row.keys).encode()
        try:
            done = subprocess.run([os.path.abspath(hl), path],
                input=keys + COMMAND + b"\n", stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL, timeout=TIMEOUT, cwd=cwd)
        except subprocess.TimeoutExpired:
            return None, b""
        return done.returncode, done.stdout


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    testdir, hl, prefixes = sys.argv[1], sys.argv[2], sys.argv[3:]
    games = os.path.join(testdir, "games")
    manifest = [r for r in images.read_manifest(os.path.join(testdir, "games.manifest.tsv"))
                if r.id != images.COMPANION]
    missing, wrong = images.check_corpus(manifest, games)
    rows = {r.id: r for r in manifest}

    def wanted(id):
        return (not prefixes or any(id.startswith(p) for p in prefixes)) \
            and rows[id].dir not in missing and rows[id].dir not in wrong

    for id, _, _ in FALLBACK:
        if id not in rows:
            sys.exit(f"damaged.py: no manifest row {id}")
    if not os.access(hl, os.X_OK):
        sys.exit(f"{hl} is not built")

    failed = 0
    with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 4) as pool:
        fallback = [f for f in FALLBACK if wanted(f[0])]
        results = pool.map(lambda f: run(hl, rows[f[0]], games, f[2]), fallback)
        for (id, what, _), (status, out) in zip(fallback, results):
            problem = None
            if status != 0:
                problem = "hung" if status is None else f"exit status {status}"
            elif NOTICE not in out:
                problem = "did not say that it plays without graphics"
            elif COMMAND not in out:    # echoed when it is read
                problem = "did not get to a command"
            if problem:
                failed += 1
                print(f"FAIL    {id}, {what}: {problem}")

        # One file may have several rows (the versions of Temple of Terror);
        # they are all cut, since the keys decide what is loaded.
        cuts = [(r, n) for r in manifest if wanted(r.id) for n in EIGHTHS]

        def cut(job):
            row, n = job
            size = os.path.getsize(os.path.join(games, row.dir, row.file))
            return run(hl, row, games, truncate(size * n // 8))

        for (row, n), (status, _) in zip(cuts, pool.map(cut, cuts)):
            if status is None or status < 0:
                failed += 1
                how = "hung" if status is None else f"killed by signal {-status}"
                print(f"FAIL    {row.id}, cut off at {n}/8: {how}")

    skipped = len([r for r in manifest if rows[r.id].dir in missing or rows[r.id].dir in wrong])
    print(f"damaged files: {len(fallback)} play on without pictures, "
          f"{len(cuts)} cut short, {failed} failed, "
          f"{skipped} rows skipped (game not in games/)")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
