#!/usr/bin/env python3
"""Picture regression tests for an interpreter with an image probe.

  run_image_tests.py TESTDIR PROBE [ID-PREFIX...]     check (all rows, or the named ones)
  run_image_tests.py TESTDIR PROBE --bless [ID...]    rewrite the goldens
  run_image_tests.py TESTDIR PROBE --collect DIR...   find the games under DIR and
                                                      copy them into TESTDIR/games/
  run_image_tests.py TESTDIR PROBE --list             one line per row: status, id, note

Every row of TESTDIR/games.manifest.tsv names one game file and the key
presses the game wants before its first prompt. The probe (see image_glk.c)
starts the real interpreter on it, draws every picture and every room, and
prints one line per picture: how many pixels were painted, where, and a CRC32
of their colours. That is compared with TESTDIR/expected/<id>.txt.

The games are copyrighted and not in the repository. Rows whose game is not in
TESTDIR/games/ are skipped, so the suite passes (testing nothing) on a bare
checkout. See TESTDIR/README.md.

The manifest / golden / --bless / --collect machinery is shared with the Scott
loader format tests (scott/test/formats/run_format_tests.py), which import this
module and describe their differences with a Suite.
"""

import argparse
import concurrent.futures
import difflib
import hashlib
import os
import shutil
import subprocess
import sys

MARKER = "\n== pictures\n"
TIMEOUT = 120
COMPANION = "-"


class Suite:
    """Everything that differs between the suites sharing this runner: the
    paths, how the probe is run, and the words of the report."""

    def __init__(self, testdir, probe, run, companion=COMPANION,
                 label="image tests", item="game", thing="file", id_width=36,
                 readme=None, not_built=None):
        self.testdir = testdir
        self.manifest = os.path.join(testdir, "games.manifest.tsv")
        self.expected = os.path.join(testdir, "expected")
        self.probe = probe
        self.run = run              # run(probe, row, games) -> fingerprint text
        self.companion = companion  # id of rows that are files, not tests
        self.label = label          # "image tests: 1 passed, ..."
        self.item = item            # "skipped (game not in games/)", "no games found"
        self.thing = thing          # "with a wrong file", "missing files"
        self.id_width = id_width    # --list column width
        self.readme = readme or os.path.join(testdir, "README.md")
        self.not_built = not_built or f"{probe} is not built: make -f Makefile.headless"


class Row:
    def __init__(self, fields):
        # keys: the key presses (image probes) or the menu answer (format probe)
        self.id, self.dir, self.file, self.sha256, self.keys, self.note = fields


def read_manifest(manifest, companion=COMPANION):
    rows = []
    with open(manifest, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            fields = line.split("\t")
            if len(fields) != 6:
                sys.exit(f"{manifest}:{n}: expected 6 columns, got {len(fields)}")
            rows.append(Row(fields))
    ids = [r.id for r in rows if r.id != companion]
    if len(ids) != len(set(ids)):
        sys.exit(f"{manifest}: duplicate ids")
    return rows


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def files_of(rows):
    """The distinct images: (dir, file) -> sha256."""
    return {(r.dir, r.file): r.sha256 for r in rows}


def check_corpus(rows, games):
    """Hash every file once. Returns dir -> problem for the folders that
    cannot be tested. The other disk of a two-disk game and the picture files
    of an MS-DOS one are looked up by file name next to the file being loaded,
    so one missing or wrong file spoils the whole folder."""
    missing, wrong = {}, {}
    for (d, f), want in sorted(files_of(rows).items()):
        path = os.path.join(games, d, f)
        if not os.path.exists(path):
            missing.setdefault(d, f)
        elif sha256(path) != want:
            wrong.setdefault(d, f)
    return missing, wrong


def run_with_keys(probe, row, games, marker=MARKER, timeout=TIMEOUT):
    """The image probes: the key presses are one command-line argument, and a
    non-zero exit appends the last line of stderr."""
    path = os.path.join(games, row.dir, row.file)
    try:
        run = subprocess.run(
            [probe, path] + ([row.keys] if row.keys else []), stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout)
    except subprocess.TimeoutExpired:
        return "TIMEOUT\n"
    out = run.stdout.decode("utf-8", "replace")
    at = out.find(marker)
    got = out[at + len(marker):] if at >= 0 else ""
    if run.returncode != 0:
        # The interpreter gave up, or the probe did: keep the last words.
        lines = [l for l in run.stderr.decode("utf-8", "replace").splitlines() if l.strip()]
        got += "PROBE FAILED: %s\n" % (lines[-1] if lines else "exit %d" % run.returncode)
    return got


def run_with_stdin(probe, row, games, marker, timeout):
    """A probe that reads its answer (e.g. a menu choice) from stdin and
    reports failure on stdout only."""
    path = os.path.join(games, row.dir, row.file)
    try:
        out = subprocess.run(
            [probe, path], input=(row.keys + "\n").encode() if row.keys else b"",
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=timeout).stdout
    except subprocess.TimeoutExpired:
        return "TIMEOUT\n"
    out = out.decode("utf-8", "replace")
    at = out.find(marker)
    if at < 0:
        # Fatal() inside a loader: keep its last words.
        lines = [l for l in out.splitlines() if l.strip()]
        return "NO FINGERPRINT: %s\n" % (lines[-1] if lines else "(no output)")
    return out[at + len(marker):]


def collect(rows, games, roots, thing="file"):
    wanted = {}
    for (d, f), want in files_of(rows).items():
        if not os.path.exists(os.path.join(games, d, f)):
            wanted.setdefault(f.lower(), []).append((d, f, want))
    found = 0
    total = sum(len(v) for v in wanted.values())
    for root in roots:
        for dirpath, dirnames, filenames in os.walk(root):
            if os.path.abspath(dirpath).startswith(os.path.abspath(games)):
                dirnames[:] = []
                continue
            for name in filenames:
                targets = wanted.get(name.lower())
                if not targets:
                    continue
                digest = sha256(os.path.join(dirpath, name))
                for t in [t for t in targets if t[2] == digest]:
                    d, f, _ = t
                    os.makedirs(os.path.join(games, d), exist_ok=True)
                    shutil.copyfile(os.path.join(dirpath, name), os.path.join(games, d, f))
                    targets.remove(t)
                    found += 1
    print(f"collected {found} of {total} missing {thing}s")
    for targets in wanted.values():
        for d, f, _ in targets:
            print(f"  still missing: {d}/{f}")


def add_common_args(ap, games_help, games_default=None):
    ap.add_argument("--bless", action="store_true")
    ap.add_argument("--collect", action="store_true")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--games", default=games_default, help=games_help)
    ap.add_argument("args", nargs="*")


def run_suite(suite, opt, ap):
    """Check, --bless, --collect or --list one suite. Returns the exit code."""
    expected = suite.expected
    games = opt.games or os.path.join(suite.testdir, "games")

    rows = read_manifest(suite.manifest, suite.companion)
    tests = [r for r in rows if r.id != suite.companion]
    if opt.collect:
        if not opt.args:
            ap.error("--collect needs at least one directory to search")
        collect(rows, games, opt.args, suite.thing)
        return 0

    if opt.args:
        rows_to_run = [r for r in tests if any(r.id.startswith(a) for a in opt.args)]
        if not rows_to_run:
            sys.exit("no manifest row matches " + " ".join(opt.args))
    else:
        rows_to_run = tests

    # The other files in the folders of the selected rows count too.
    dirs = {r.dir for r in rows_to_run}
    missing, wrong = check_corpus([r for r in rows if r.dir in dirs], games)

    if opt.list:
        for r in rows_to_run:
            status = "wrong" if r.dir in wrong else "absent" if r.dir in missing else "ok"
            print(f"{status:7}{r.id:{suite.id_width}}{r.note}")
        return 0

    runnable = [r for r in rows_to_run if r.dir not in missing and r.dir not in wrong]
    if runnable and not os.access(suite.probe, os.X_OK):
        sys.exit(suite.not_built)

    with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 4) as pool:
        results = list(pool.map(lambda r: suite.run(suite.probe, r, games), runnable))

    passed = failed = blessed = 0
    for row, got in zip(runnable, results):
        golden = os.path.join(expected, row.id + ".txt")
        if opt.bless:
            old = open(golden, encoding="utf-8").read() if os.path.exists(golden) else None
            if old != got:
                os.makedirs(expected, exist_ok=True)
                with open(golden, "w", encoding="utf-8") as f:
                    f.write(got)
                blessed += 1
                print(f"BLESSED {row.id}")
            continue
        if not os.path.exists(golden):
            failed += 1
            print(f"FAIL    {row.id}: no golden (run with --bless)")
            continue
        want = open(golden, encoding="utf-8").read()
        if want == got:
            passed += 1
            continue
        failed += 1
        print(f"FAIL    {row.id}  ({row.dir}/{row.file})")
        for line in difflib.unified_diff(want.splitlines(), got.splitlines(),
                "expected", "got", lineterm="", n=0):
            print("    " + line)

    for d, f in sorted(wrong.items()):
        print(f"WRONG   games/{d}/{f} is not the file the manifest pins (sha256 differs)")
    skipped = len(rows_to_run) - len(runnable)
    bad = sum(1 for r in rows_to_run if r.dir in wrong)

    if opt.bless:
        print(f"{blessed} goldens rewritten, {len(runnable) - blessed} unchanged, "
              f"{skipped} rows skipped")
        return 0
    print(f"{suite.label}: {passed} passed, {failed} failed, "
          f"{skipped - bad} skipped ({suite.item} not in games/), "
          f"{bad} with a wrong {suite.thing}")
    if skipped - bad == len(rows_to_run):
        print(f"  no {suite.item}s found; see {suite.readme}")
    return 1 if failed or bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("testdir", help="folder with games.manifest.tsv and expected/")
    ap.add_argument("probe", help="the interpreter's image probe binary")
    add_common_args(ap, "where the games are (default: games/ in the test folder)")
    opt = ap.parse_args()
    return run_suite(Suite(opt.testdir, opt.probe, run_with_keys), opt, ap)


if __name__ == "__main__":
    sys.exit(main())
