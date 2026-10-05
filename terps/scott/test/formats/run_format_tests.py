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
"""

import argparse
import concurrent.futures
import difflib
import hashlib
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MANIFEST = os.path.join(HERE, "games.manifest.tsv")
EXPECTED = os.path.join(HERE, "expected")
PROBE = os.path.join(HERE, "..", "..", "scott_format_probe")
MARKER = "\n== fingerprint\n"
TIMEOUT = 60


class Row:
    def __init__(self, fields):
        self.id, self.dir, self.file, self.sha256, self.input, self.note = fields


def read_manifest():
    rows = []
    with open(MANIFEST, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            fields = line.split("\t")
            if len(fields) != 6:
                sys.exit(f"{MANIFEST}:{n}: expected 6 columns, got {len(fields)}")
            rows.append(Row(fields))
    ids = [r.id for r in rows]
    if len(ids) != len(set(ids)):
        sys.exit(f"{MANIFEST}: duplicate ids")
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
    """Hash every image once. Returns dir -> problem for the folders that
    cannot be tested. A two-disk game is looked up by file name next to the
    image being loaded, so one missing or wrong side spoils the whole folder."""
    missing, wrong = {}, {}
    for (d, f), want in sorted(files_of(rows).items()):
        path = os.path.join(games, d, f)
        if not os.path.exists(path):
            missing.setdefault(d, f)
        elif sha256(path) != want:
            wrong.setdefault(d, f)
    return missing, wrong


def fingerprint(row, games):
    path = os.path.join(games, row.dir, row.file)
    try:
        out = subprocess.run(
            [PROBE, path], input=(row.input + "\n").encode() if row.input else b"",
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=TIMEOUT).stdout
    except subprocess.TimeoutExpired:
        return "TIMEOUT\n"
    out = out.decode("utf-8", "replace")
    at = out.find(MARKER)
    if at < 0:
        # Fatal() inside a loader: keep its last words.
        lines = [l for l in out.splitlines() if l.strip()]
        return "NO FINGERPRINT: %s\n" % (lines[-1] if lines else "(no output)")
    return out[at + len(MARKER):]


def collect(rows, games, roots):
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
    print(f"collected {found} of {total} missing images")
    for targets in wanted.values():
        for d, f, _ in targets:
            print(f"  still missing: {d}/{f}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bless", action="store_true")
    ap.add_argument("--collect", action="store_true")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--games", default=os.path.join(HERE, "games"),
        help="where the images are (default: games/ next to this script)")
    ap.add_argument("args", nargs="*")
    opt = ap.parse_args()

    rows = read_manifest()
    if opt.collect:
        if not opt.args:
            ap.error("--collect needs at least one directory to search")
        collect(rows, opt.games, opt.args)
        return 0

    if opt.args:
        rows_to_run = [r for r in rows if any(r.id.startswith(a) for a in opt.args)]
        if not rows_to_run:
            sys.exit("no manifest row matches " + " ".join(opt.args))
    else:
        rows_to_run = rows

    # Siblings of the selected rows count too: they are the other disk sides.
    dirs = {r.dir for r in rows_to_run}
    missing, wrong = check_corpus([r for r in rows if r.dir in dirs], opt.games)

    if opt.list:
        for r in rows_to_run:
            status = "wrong" if r.dir in wrong else "absent" if r.dir in missing else "ok"
            print(f"{status:7}{r.id:44}{r.note}")
        return 0

    runnable = [r for r in rows_to_run if r.dir not in missing and r.dir not in wrong]
    if runnable and not os.access(PROBE, os.X_OK):
        sys.exit("scott_format_probe is not built: make -f Makefile.headless scott_format_probe")

    with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 4) as pool:
        results = list(pool.map(lambda r: fingerprint(r, opt.games), runnable))

    passed = failed = blessed = 0
    for row, got in zip(runnable, results):
        golden = os.path.join(EXPECTED, row.id + ".txt")
        if opt.bless:
            old = open(golden, encoding="utf-8").read() if os.path.exists(golden) else None
            if old != got:
                os.makedirs(EXPECTED, exist_ok=True)
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
    print(f"format tests: {passed} passed, {failed} failed, "
          f"{skipped - bad} skipped (image not in games/), {bad} with a wrong image")
    if skipped - bad == len(rows_to_run):
        print("  no images found; see test/formats/README.md")
    return 1 if failed or bad else 0


if __name__ == "__main__":
    sys.exit(main())
