#!/usr/bin/env python3
"""Tests for how the two disks of a two-disk game find each other.

  run_companion_tests.py TESTDIR PROBE [ID-PREFIX...]    check (all sets, or the named ones)
  run_companion_tests.py TESTDIR PROBE --bless [ID...]   rewrite TESTDIR/companions.tsv
  run_companion_tests.py TESTDIR PROBE --list            one line per set

A two-disk set is a folder of TESTDIR/games.manifest.tsv (see
run_image_tests.py) with the rows <id> and <id>-disk2. The picture and format
tests already load every set from both disks, under the names the files came
with. Here the files are put in a scratch folder under other names, or one of
them is left out, and the probe is run on each disk again. TESTDIR/companions.tsv
says what is to come of that:

  alone  ID    FIRST   SECOND
      what the probe makes of each disk when the other one is not there,
      as a summary of how its output differs from the golden of the row

  names  EXTS  FIRST   SECOND   VERDICT
      the two disks of every set with one of the file extensions EXTS, named
      FIRST and SECOND (plus the extension they had): "found" if both disks
      load as in the golden, "not found" if both load as they do alone,
      "only from first" or "only from second" otherwise

A names row is run on every set, so 'the other disk was found' is known to
hold for every disk layout and not only for the file name code. Sets with a
missing file are skipped, so the suite passes (testing nothing) on a bare
checkout.

The interpreters also know some file name pairs by heart. A suite can hand
them over (see scott/test/formats/run_companion_tests.py); each has to be a
names row then, and --bless adds the ones that are not.
"""

import argparse
import concurrent.futures
import os
import shutil
import sys
import tempfile

import run_image_tests as images

SECOND = "-disk2"
VERDICTS = ("found", "not found", "only from first", "only from second")


class Trial:
    """One file, as the run functions of run_image_tests.py want it."""

    def __init__(self, dir, file, keys):
        self.dir, self.file, self.keys = dir, file, keys


def disk_sets(rows, companion):
    """[(first row, second row)] for the folders that are two-disk sets."""
    by_id = {r.id: r for r in rows if r.id != companion}
    return [(r, by_id[r.id + SECOND]) for r in rows
            if r.id != companion and r.id + SECOND in by_id]


def extension(row):
    return os.path.splitext(row.file)[1]


def summarize(want, got):
    """How one probe output differs from the golden, in a few words."""
    if got == want:
        return "complete"
    if not got.strip():
        return "nothing"
    a, b = want.splitlines(), got.splitlines()
    if len(b) == 1 and len(a) > 1:
        return b[0].strip()
    names = lambda lines: {(l.split() or [""])[0]: l for l in lines}
    wa, gb = names(a), names(b)
    changed = [k for k in wa if k in gb and wa[k] != gb[k]]
    lost = [k for k in wa if k not in gb]
    new = [k for k in gb if k not in wa]
    if len(changed) + len(lost) + len(new) > 4:
        return f"{len(wa) - len(changed) - len(lost)} of {len(wa)} lines as in the golden"
    parts = [word + ": " + " ".join(keys)
             for word, keys in (("differs", changed), ("lacks", lost), ("adds", new)) if keys]
    return "; ".join(parts) or "differs"


def read_table(path):
    alone, names, head = {}, [], []
    if not os.path.exists(path):
        return alone, names, head
    with open(path, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            if line.startswith("#") or not line.strip():
                head.append(line)
                continue
            fields = line.rstrip("\n").split("\t")
            if fields[0] == "alone" and len(fields) == 4:
                alone[fields[1]] = fields[2:]
            elif fields[0] == "names" and len(fields) == 5:
                names.append(fields[1:])
            else:
                sys.exit(f"{path}:{n}: neither an alone row nor a names row")
    return alone, names, head


def write_table(path, alone, names, head):
    with open(path, "w", encoding="utf-8") as f:
        f.writelines(head)
        for id, outcome in alone.items():
            f.write("\t".join(["alone", id] + outcome) + "\n")
        for fields in names:
            f.write("\t".join(["names"] + fields) + "\n")


def place(src, dst):
    try:
        os.symlink(src, dst)
    except OSError:
        shutil.copyfile(src, dst)


def run_trial(suite, games, scratch, number, pair, names):
    """Put the disks of pair that have a name in a folder of their own and
    run the probe on each. Returns the outputs, None for a disk left out."""
    folder = f"{number:05}"
    os.mkdir(os.path.join(scratch, folder))
    for row, name in zip(pair, names):
        if name:
            place(os.path.abspath(os.path.join(games, row.dir, row.file)),
                  os.path.join(scratch, folder, name))
    return [suite.run(suite.probe, Trial(folder, name, row.keys), scratch) if name else None
            for row, name in zip(pair, names)]


def verdict(goldens, alone, got):
    found = [g == w for g, w in zip(got, goldens)]
    if all(found):
        return "found"
    if all(g == a for g, a in zip(got, alone)):
        return "not found"
    if found[0] and got[1] == alone[1]:
        return "only from first"
    if found[1] and got[0] == alone[0]:
        return "only from second"
    return "neither as in the golden nor as alone"


def run_companions(suite, opt, database=()):
    """Check, --bless or --list. database: [(exts, first, second)], the name
    pairs the interpreter knows by heart. Returns the exit code."""
    games = opt.games or os.path.join(suite.testdir, "games")
    table = os.path.join(suite.testdir, "companions.tsv")
    rows = images.read_manifest(suite.manifest, suite.companion)
    sets = disk_sets(rows, suite.companion)
    alone_want, names, head = read_table(table)

    failures = []
    for exts, first, second in database:
        if not any(n[:3] == [exts, first, second] for n in names):
            if opt.bless:
                names.append([exts, first, second, "?"])
            else:
                failures.append(f"the interpreter knows the pair {first} / {second}, "
                                f"which is not a names row (run with --bless)")

    selected = [p for p in sets if not opt.args or any(p[0].id.startswith(a) for a in opt.args)]
    if not selected:
        sys.exit("no two-disk set matches " + " ".join(opt.args))
    dirs = {p[0].dir for p in selected}
    missing, wrong = images.check_corpus([r for r in rows if r.dir in dirs], games)
    runnable = [p for p in selected if p[0].dir not in missing and p[0].dir not in wrong]

    if opt.list:
        for first, second in selected:
            status = "wrong" if first.dir in wrong else "absent" if first.dir in missing else "ok"
            print(f"{status:7}{first.id:{suite.id_width}}{first.file} / {second.file}")
        return 0
    if runnable and not os.access(suite.probe, os.X_OK):
        sys.exit(suite.not_built)

    # (set, None) is the two disks alone, (set, n) the names row n.
    jobs = [(p, None) for p in runnable]
    for n, (exts, first, second, _) in enumerate(names):
        jobs += [(p, n) for p in runnable if extension(p[0])[1:].lower() in exts.split(",")]

    def run(job):
        number, (pair, n) = job
        if n is None:
            a = run_trial(suite, games, scratch, 2 * number, pair, (pair[0].file, None))
            b = run_trial(suite, games, scratch, 2 * number + 1, pair, (None, pair[1].file))
            return [a[0], b[1]]
        return run_trial(suite, games, scratch, 2 * number, pair,
                         [names[n][1 + i] + extension(pair[i]) for i in (0, 1)])

    with tempfile.TemporaryDirectory() as scratch:
        with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 4) as pool:
            results = list(pool.map(run, enumerate(jobs)))

    goldens = {}
    for pair in runnable:
        paths = [os.path.join(suite.expected, r.id + ".txt") for r in pair]
        if not all(os.path.exists(p) for p in paths):
            sys.exit(f"{pair[0].id}: no golden; bless the {suite.label} first")
        goldens[pair[0].id] = [open(p, encoding="utf-8").read() for p in paths]

    alone_got = {pair[0].id: got for (pair, n), got in zip(jobs, results) if n is None}
    passed = changed = 0
    for pair in runnable:
        id = pair[0].id
        now = [summarize(w, g) for w, g in zip(goldens[id], alone_got[id])]
        if alone_want.get(id) == now:
            passed += 1
        elif opt.bless:
            alone_want[id] = now
            changed += 1
        else:
            failures.append(f"{id} alone: {' / '.join(now)}, expected "
                            f"{' / '.join(alone_want.get(id) or ['no row (run with --bless)'])}")

    by_row = {}
    for (pair, n), got in zip(jobs, results):
        if n is not None:
            id = pair[0].id
            by_row.setdefault(n, []).append((id, verdict(goldens[id], alone_got[id], got)))
    for n, fields in enumerate(names):
        got = by_row.get(n)
        if not got:
            continue    # no set with this extension is here
        said = sorted({v for _, v in got})
        if said == [fields[3]]:
            passed += 1
        elif opt.bless and len(said) == 1 and said[0] in VERDICTS:
            fields[3] = said[0]
            changed += 1
        else:
            failures.append(f"names {fields[1]} / {fields[2]}: expected {fields[3]}, but "
                            + "; ".join(f"{v}: {' '.join(i for i, w in got if w == v)}"
                                        for v in said if v != fields[3]))

    for d, f in sorted(wrong.items()):
        print(f"WRONG   games/{d}/{f} is not the file the manifest pins (sha256 differs)")
    for line in failures:
        print("FAIL    " + line)
    skipped = len(selected) - len(runnable)
    if opt.bless:
        write_table(table, alone_want, names, head)
        print(f"{changed} rows rewritten, {passed} unchanged, {skipped} sets skipped"
              + (f", {len(failures)} rows that cannot be blessed" if failures else ""))
        return 1 if failures else 0
    print(f"companion tests: {passed} passed, {len(failures)} failed, "
          f"{skipped} sets skipped ({suite.item} not in games/)")
    return 1 if failures or wrong else 0


def add_args(ap, games_default=None):
    ap.add_argument("--bless", action="store_true")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--games", default=games_default,
        help="where the games are (default: games/ in the test folder)")
    ap.add_argument("args", nargs="*")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("testdir", help="folder with games.manifest.tsv and expected/")
    ap.add_argument("probe", help="the interpreter's image probe binary")
    add_args(ap)
    opt = ap.parse_args()
    return run_companions(images.Suite(opt.testdir, opt.probe, images.run_with_keys), opt)


if __name__ == "__main__":
    sys.exit(main())
