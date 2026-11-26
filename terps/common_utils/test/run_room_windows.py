#!/usr/bin/env python3
"""Check that the room description windows of Scott, Plus, TaylorMade and
Comprehend hold what is printed in them.

    run_room_windows.py [--build DIR] [--jobs N] [--list] [NAME ...]

Each command script of the interpreters' own script tests (the first game of
every row of scott/test/scripts, plus/test/scripts and taylor/test/scripts,
and the Comprehend walkthroughs) is played through the Spatterlight build of
its interpreter, under the fake app of bocfel/test/v6trace.py, at several
window sizes and cell sizes, and once more with the window resized every ten
lines of the script.  Every print into a text grid is checked against the
grid's size as the app counts it (gli_cells_in in glkimp/protocol.h), from the
rectangle the library gave the window: a line that runs past the right edge,
which the app would wrap, or a row below the last one, which the app drops,
is a GRID OVERFLOW.  The app's cells are seldom whole pixels, which is why the
sizes include 7.2x15.6 and 6.6x13.2.

The windows are the room description (layouttext.c's ShowTextInTopWindow in
Scott, Plus and Taylor, comprehend's own in Comprehend), the delimiter line
under it, the status lines and the Seas of Blood battle panels.  Not checked:
a window too short for the description.  ShowTextInTopWindow does not take the
bottom window below three rows, so in a very small window the description
loses its last lines; the sizes here leave room for it.

--build DIR is where Xcode put the interpreters and their libraries (default
build/Debug of the repository; build them with
`xcodebuild -target scott -target plus -target taylor -target comprehend
ARCHS=arm64 CODE_SIGNING_ALLOWED=NO`).  NAME limits the run to the scripts
whose name, or interpreter, starts so.  Games that have not been fetched are
skipped.  Exit status 1 if any overflow is found.
"""

import collections
import concurrent.futures
import os
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TERPS = os.path.dirname(os.path.dirname(HERE))
REPO = os.path.dirname(TERPS)
sys.path.insert(0, os.path.join(TERPS, "bocfel", "test"))
import v6trace  # noqa: E402

INTERPRETERS = ("scott", "plus", "taylor", "comprehend")

# The fixed sizes, and the cell sizes each is played with
SIZES = [(800, 600), (420, 400)]
CELLS = [(7.2, 15.6), (8.0, 16.0), (6.6, 13.2)]
# The resizing run goes round these, starting with the first
RESIZES = [(800, 600), (512, 560), (1024, 700), (611, 480)]
RESIZE_CELLS = [(7.2, 15.6), (9.6, 19.2)]
RESIZE_EVERY = 10

COMPREHEND = os.path.join(TERPS, "comprehend", "test", "walkthrough")
COMPREHEND_GAMES = [
    ("ootopos-dos", "oo-topos/NOVEL.EXE", "ootopos"),
    ("crimsoncrown-dos", "crimson-crown/NOVEL.EXE", "crimsoncrown"),
    ("talisman-dos", "talisman-challenging-the-sands-of-time/NOVEL.EXE",
     "talisman"),
    ("transylvania-dos", "transylvania/NOVEL.EXE", "transylvania"),
    ("covetedmirror", "The Coveted Mirror 2.0 (woz-a-day collection)/"
     "The Coveted Mirror 2.0 side B (boot).woz", "covetedmirror"),
]


def tsv_rows(path):
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.strip() and not line.startswith("#"):
                yield line.rstrip("\n").split("\t")


def field(row, i):
    return row[i] if len(row) > i else ""


def cases():
    """(interpreter, name, game, script, start-up keys) for each script."""
    out = []
    scott = os.path.join(TERPS, "scott", "test")
    games = {r[0]: r for r in tsv_rows(
        os.path.join(scott, "formats", "games.manifest.tsv"))}
    seen = set()
    for row in tsv_rows(os.path.join(scott, "scripts", "scripts.tsv")):
        if row[0] in seen or row[1] not in games:
            continue
        seen.add(row[0])
        game = games[row[1]]
        # The answer to a compilation's menu, then the script row's keys
        keys = [field(game, 4)] if field(game, 4) else []
        keys += [k.replace("_", "") for k in field(row, 4).split()]
        out.append(("scott", row[0],
                    os.path.join(scott, "formats", "games", game[1], game[2]),
                    os.path.join(scott, "scripts", row[0] + ".txt"), keys))
    for terp in ("plus", "taylor"):
        test = os.path.join(TERPS, terp, "test")
        games = {r[0]: r for r in tsv_rows(
            os.path.join(test, "images", "games.manifest.tsv"))}
        seen = set()
        for row in tsv_rows(os.path.join(test, "scripts", "scripts.tsv")):
            if row[0] in seen or row[1] not in games:
                continue
            seen.add(row[0])
            game = games[row[1]]
            out.append((terp, row[0],
                        os.path.join(test, "images", "games", game[1],
                                     game[2]),
                        os.path.join(test, "scripts", row[0] + ".txt"),
                        field(game, 4).split()))
    for name, game, script in COMPREHEND_GAMES:
        out.append(("comprehend", name, os.path.join(COMPREHEND, "games", game),
                    os.path.join(COMPREHEND, "scripts", script + ".txt"), []))
    return out


def script_lines(path, terp):
    with open(path, encoding="utf-8", errors="replace") as f:
        lines = [l.rstrip("\r\n") for l in f if not l.startswith("#")]
    if terp == "comprehend":
        # Its runner drops blank lines; the other scripts need them, for
        # the <HIT ENTER> pauses
        lines = [l for l in lines if l.strip()]
    return lines


def stage(work, build):
    """The interpreters where they expect their libraries."""
    app = os.path.join(work, "app")
    os.makedirs(os.path.join(app, "MacOS"))
    os.makedirs(os.path.join(app, "Frameworks"))
    for terp in INTERPRETERS:
        shutil.copy2(os.path.join(build, terp), os.path.join(app, "MacOS"))
    for name in os.listdir(build):
        if name.endswith(".dylib"):
            shutil.copy2(os.path.join(build, name),
                         os.path.join(app, "Frameworks"))
    return os.path.join(app, "MacOS")


def play(binaries, case, size, cell):
    """(case, what the run was, its overflows, how it ended)."""
    terp, name, game, script, keys = case
    lines = keys + script_lines(script, terp)
    if size is None:
        resized = []
        for i, line in enumerate(lines):
            if i and i % RESIZE_EVERY == 0:
                resized.append("arrange:%dx%d" % RESIZES[
                    (i // RESIZE_EVERY) % len(RESIZES)])
            resized.append(line)
        lines, size = resized, RESIZES[0]
        what = "resized"
    else:
        what = "%dx%d" % size
    what += " cells %gx%g" % cell
    with tempfile.TemporaryDirectory() as home:
        trace, rc, err, left = v6trace.run(
            os.path.join(binaries, terp), game, home, lines, timeout=300,
            width=size[0], height=size[1], cellw=cell[0], cellh=cell[1])
    overflows, at = [], "%dx%d" % size
    for line in trace:
        if line.startswith("--- arrange "):
            at = line[len("--- arrange "):]
        elif "GRID OVERFLOW" in line:
            overflows.append("at %s: %s" % (at, line[4:]))
    ending = "" if rc == 0 else " (exit %s)" % rc
    return case, what, overflows, ending


def main(argv):
    build = os.path.join(REPO, "build", "Debug")
    jobs, listing, names = 6, False, []
    while argv:
        arg = argv.pop(0)
        if arg == "--build":
            build = argv.pop(0)
        elif arg == "--jobs":
            jobs = int(argv.pop(0))
        elif arg == "--list":
            listing = True
        elif arg.startswith("-"):
            sys.exit(__doc__)
        else:
            names.append(arg)

    chosen = [c for c in cases()
              if not names or any(c[0].startswith(n) or c[1].startswith(n)
                                  for n in names)]
    missing = [c for c in chosen if not os.path.exists(c[2])]
    chosen = [c for c in chosen if os.path.exists(c[2])]
    if listing:
        for c in chosen:
            print("%-10s %-32s %s" % (c[0], c[1], os.path.relpath(c[2], TERPS)))
        for c in missing:
            print("%-10s %-32s missing %s" % (c[0], c[1],
                                              os.path.relpath(c[2], TERPS)))
        return 0

    runs = [(c, s, k) for c in chosen for s in SIZES for k in CELLS]
    runs += [(c, None, k) for c in chosen for k in RESIZE_CELLS]
    failed = 0
    with tempfile.TemporaryDirectory() as work:
        binaries = stage(work, build)
        with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
            for case, what, overflows, ending in pool.map(
                    lambda run: play(binaries, *run), runs):
                if not overflows and not ending:
                    continue
                failed += 1
                print("%s/%s %s: %d overflows%s"
                      % (case[0], case[1], what, len(overflows), ending))
                for line, n in collections.Counter(overflows).most_common(5):
                    print("    %dx %s" % (n, line[:200]))
                sys.stdout.flush()
    for c in missing:
        print("skipped %s/%s: no %s" % (c[0], c[1],
                                       os.path.relpath(c[2], TERPS)))
    print("%d scripts, %d runs, %d with overflows or a bad exit"
          % (len(chosen), len(runs), failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
