#!/usr/bin/env python3
"""Glk protocol traces of the HTML TADS corpus, for regression checks.

Runs the Spatterlight build of tadsr on every game in htmltads/CENSUS.tsv
under the fake app of terps/bocfel/test/v6trace.py (itself built on
terps/scarier/test/glkdrive.py), types a few commands, and writes every
message the interpreter sends -- windows, style hints, colours, prints,
images, links, sounds -- to OUT/<game>.trace.  No goldens are stored: take a
baseline from the unchanged build, then compare.

    run_traces.py --out DIR [--compare DIR] [--build DIR] [-j N]
                  [--script FILE] [--walk DIR] [substring ...]

  --out DIR      where the traces go (created; must not exist yet)
  --compare DIR  a baseline from an earlier run: list the games whose trace
                 or stderr differs, and write unified diffs to OUT/diffs/
  --build DIR    the build products holding tadsr and libglkimp.dylib
                 (default: build/Debug at the top of the repository).  They
                 are copied into OUT/app first, so the next build can start
                 while a run is going
  -j N           games run in parallel (default 8)
  --script FILE  the commands to type, one per line (default: SCRIPT below)
  --walk DIR     for a game with a command script DIR/<stem>.txt (the
                 census path with "/" as "__" and without .gam/.t3), type
                 that instead; the other games get the default script
  --timeout S    seconds before a game is killed (default 60)
  substring      only the games whose census path contains one of these

Consecutive PRINTs to the same window in the same style are joined into one
trace line, so a change in how the interpreter splits its output into print
calls does not show as a difference.  Everything else is one line per
message.

The games in NOISY show the real time or wait on real-time timers, so their
traces differ from run to run; --compare lists them apart and does not count
them.

The games are not in the repository: fetch them with fetch_games.sh.
"""

import ast
import concurrent.futures
import difflib
import os
import re
import shutil
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
sys.path.insert(0, os.path.join(REPO, "terps", "bocfel", "test"))
import v6trace  # noqa: E402

CORPUS = os.path.join(HERE, "htmltads")

# Enough to get past most title screens and see the first room, the status
# line, and whatever the game prints for a few standard verbs.  The "press a
# key" pauses are answered with Return without using up a line (see
# v6trace.Tracer.feed_key).
SCRIPT = ["look", "inventory", "examine me", "about", "help", "wait",
          "score"]

NOISY = {
    "six/six.gam": "the status line shows the time of day",
    "space.t3": "real-time key timeouts during the intro",
    "landing.gam": "chooses some messages by the clock (\"take hen\")",
}


def games(filters):
    """Census paths (relative to htmltads/games), in census order."""
    out = []
    with open(os.path.join(CORPUS, "CENSUS.tsv"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            path = line.split("\t", 1)[0]
            if not filters or any(s in path for s in filters):
                out.append(path)
    return out


def trace_name(path):
    return path.replace("/", "__") + ".trace"


def stage(out, build):
    """tadsr where it expects to find libglkimp: ../Frameworks."""
    app = os.path.join(out, "app")
    os.makedirs(os.path.join(app, "MacOS"))
    os.makedirs(os.path.join(app, "Frameworks"))
    terp = os.path.join(app, "MacOS", "tadsr")
    shutil.copy2(os.path.join(build, "tadsr"), terp)
    shutil.copy2(os.path.join(build, "libglkimp.dylib"),
                 os.path.join(app, "Frameworks", "libglkimp.dylib"))
    return terp


def join_prints(trace):
    """Join runs of PRINT lines with the same window and style."""
    joined = []
    head = None                 # "PRINT a1 a2 ..." of the run being joined
    text = ""
    for line in trace:
        if line.startswith("PRINT "):
            # "PRINT a1 a2 a3 a4 a5 'text'", the text as repr() wrote it
            fields = line.split(" ", 6)
            args = " ".join(fields[:6])
            this = ast.literal_eval(fields[6])
            if args == head:
                text += this
                continue
            if head is not None:
                joined.append("%s %r" % (head, text))
            head, text = args, this
            continue
        if head is not None:
            joined.append("%s %r" % (head, text))
            head = None
        joined.append(line)
    if head is not None:
        joined.append("%s %r" % (head, text))
    return joined


def script_for(path, walk, default):
    if walk:
        stem = re.sub(r"\.(gam|t3)$", "", path.replace("/", "__"),
                      flags=re.I)
        try:
            with open(os.path.join(walk, stem + ".txt"),
                      encoding="utf-8") as f:
                return v6trace.load_script(f.read())
        except FileNotFoundError:
            pass
    return default


def run_one(terp, path, out, script, timeout):
    story = os.path.join(CORPUS, "games", path)
    home = os.path.join(out, "home", trace_name(path)[:-len(".trace")])
    os.makedirs(home)
    started = time.time()
    trace, rc, err, left = v6trace.run(terp, story, home, list(script),
                                       timeout=timeout)
    lines = join_prints(trace)
    lines.append("=== exit %s, script left: %s" % (rc, left))
    name = trace_name(path)
    with open(os.path.join(out, name), "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    with open(os.path.join(out, name[:-len(".trace")] + ".err"), "w",
              encoding="utf-8") as f:
        f.write(err)
    shutil.rmtree(home, ignore_errors=True)
    return path, rc, time.time() - started


def read(path):
    try:
        with open(path, encoding="utf-8") as f:
            return f.read().splitlines(keepends=True)
    except FileNotFoundError:
        return None


def compare(out, baseline, paths):
    diffs = os.path.join(out, "diffs")
    os.makedirs(diffs, exist_ok=True)
    changed = []
    for path in paths:
        stem = trace_name(path)[:-len(".trace")]
        what = []
        for ext in (".trace", ".err"):
            old = read(os.path.join(baseline, stem + ext))
            new = read(os.path.join(out, stem + ext))
            if old == new:
                continue
            what.append(ext[1:])
            if old is None:
                old = ["(missing in baseline)\n"]
            with open(os.path.join(diffs, stem + ext + ".diff"), "w",
                      encoding="utf-8") as f:
                f.writelines(difflib.unified_diff(
                    old, new, "old/" + stem + ext, "new/" + stem + ext))
        if what:
            changed.append((path, what))
    noisy = [c for c in changed if c[0] in NOISY]
    changed = [c for c in changed if c[0] not in NOISY]
    for path, what in noisy:
        print("noisy   %s (%s): %s" % (path, ", ".join(what), NOISY[path]))
    for path, what in changed:
        print("CHANGED %s (%s)" % (path, ", ".join(what)))
    print("%d of %d games changed" % (len(changed), len(paths)))
    return changed


def main():
    argv = sys.argv[1:]
    out = baseline = None
    build = os.path.join(REPO, "build", "Debug")
    jobs, timeout, script, walk = 8, 60.0, SCRIPT, None
    filters = []
    while argv:
        opt = argv.pop(0)
        if opt == "--out":
            out = os.path.abspath(argv.pop(0))
        elif opt == "--compare":
            baseline = os.path.abspath(argv.pop(0))
        elif opt == "--build":
            build = os.path.abspath(argv.pop(0))
        elif opt == "-j":
            jobs = int(argv.pop(0))
        elif opt == "--timeout":
            timeout = float(argv.pop(0))
        elif opt == "--script":
            with open(argv.pop(0), encoding="utf-8") as f:
                script = v6trace.load_script(f.read())
        elif opt == "--walk":
            walk = os.path.abspath(argv.pop(0))
        elif opt.startswith("-"):
            sys.exit("unknown option " + opt)
        else:
            filters.append(opt)
    if not out:
        sys.exit(__doc__)
    paths = games(filters)
    missing = [p for p in paths
               if not os.path.exists(os.path.join(CORPUS, "games", p))]
    if missing:
        sys.exit("%d games missing (run fetch_games.sh), e.g. %s"
                 % (len(missing), missing[0]))
    os.makedirs(out)
    terp = stage(out, build)
    started = time.time()
    trouble = []
    with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
        futures = [pool.submit(run_one, terp, p, out,
                               script_for(p, walk, script), timeout)
                   for p in paths]
        for fut in concurrent.futures.as_completed(futures):
            path, rc, secs = fut.result()
            if rc != 0:
                trouble.append((path, rc))
    shutil.rmtree(os.path.join(out, "home"), ignore_errors=True)
    print("%d games in %.0f s" % (len(paths), time.time() - started))
    for path, rc in sorted(trouble):
        print("EXIT %s: %s" % (rc, path))
    if baseline:
        sys.exit(1 if compare(out, baseline, paths) else 0)


if __name__ == "__main__":
    main()
