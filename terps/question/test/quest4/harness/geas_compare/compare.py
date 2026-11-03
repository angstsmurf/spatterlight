#!/usr/bin/env python3
"""Replay the Quest 4 corpus on Question and on the legacy Geas 0.5 engine.

    ./compare.py                 # all rows: group table, timings, Geas's wins
    ./compare.py Adventure Beam  # just these labels

This produces the figures in notes/QUESTION_vs_GEAS.md.  The rows come from
../run_walkthroughs.sh, through the oracle's parser.  A row passes if its win
marker appears in the output, which is all this measures: the routes were
derived against Question, so a route that needs something Geas lacks stops there.

The engine is taken from git at BASELINE and its one $rand line is patched to
draw from erkyrath_random, so both engines see the same stream.  Rows run one
at a time so that the timings mean something.  out/ holds the extracted engine,
the binary, each engine's transcripts and results.tsv.
"""

import importlib.util
import io
import os
import re
import subprocess
import sys
import tarfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
HARNESS = os.path.dirname(HERE)
TEST = os.path.dirname(os.path.dirname(HARNESS))
COMMON = os.path.abspath(os.path.join(TEST, "..", "..", "common_utils"))
OUT = os.path.join(HERE, "out")
SRC = os.path.join(OUT, "terps", "geas")
LEGACY = os.path.join(OUT, "legacy_runner")
QUESTION = os.path.join(HARNESS, "question_walkthrough_runner")
CASDUMP = os.path.join(HARNESS, "casdump")

BASELINE = "513b0f4ff"
TIMEOUT = 300
RAND_OLD = "lower + (rand() % (upper + 1 - lower))"
RAND_NEW = "lower + (erkyrath_random() % (upper + 1 - lower))"

# This directory's own name would shadow the oracle's compare module.
_spec = importlib.util.spec_from_file_location(
    "oracle_compare", os.path.join(HARNESS, "oracle", "compare.py"))
oracle = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(oracle)


def build():
    """Build Question's tools, extract the legacy engine and compile the adapter."""
    subprocess.run(["make", "-s", "-C", TEST, "quest4/harness/question_walkthrough_runner",
                    "quest4/harness/casdump", "quest4/harness/randomness.o"], check=True)
    os.makedirs(OUT, exist_ok=True)
    top = subprocess.run(["git", "rev-parse", "--show-toplevel"], cwd=HERE, check=True,
                         stdout=subprocess.PIPE, text=True).stdout.strip()
    tar = subprocess.run(["git", "archive", BASELINE, "terps/geas"], cwd=top, check=True,
                         stdout=subprocess.PIPE).stdout
    with tarfile.open(fileobj=io.BytesIO(tar)) as t:
        t.extractall(OUT, filter="data")

    path = os.path.join(SRC, "geas-runner.cc")
    with open(path, encoding="latin-1", newline="") as f:
        text = f.read()
    if text.count(RAND_OLD) != 1:
        sys.exit("the legacy $rand line is not where it was expected")
    with open(path, "w", encoding="latin-1", newline="") as f:
        f.write(text.replace(RAND_OLD, RAND_NEW))

    subprocess.run(["c++", "-std=c++17", "-O2", "-w", "-I" + SRC, "-I" + COMMON,
                    os.path.join(HERE, "legacy_runner.cc"),
                    os.path.join(HARNESS, "randomness.o"), "-o", LEGACY], check=True)


def group(path):
    """The row's group in the notes' table, from its magic and its ASL version."""
    with open(path, "rb") as f:
        magic = f.read(7)
    fmt = magic.decode("latin-1") if magic.startswith(b"QCGF") else None
    if fmt in ("QCGF001", "QCGF003"):
        return fmt
    dump = subprocess.run([CASDUMP, path], stdout=subprocess.PIPE,
                          stderr=subprocess.DEVNULL).stdout
    m = re.search(rb"asl-version\s*<\s*(\d+)", dump)
    version = int(m.group(1)) if m else 0
    if fmt:
        return fmt + (", asl 311-353" if version < 400 else ", asl 400-410")
    if version < 311:
        return "text, asl < 311"
    return "text, asl 311-353" if version < 400 else "text, asl 400-410"


def play(binary, row, name):
    """(seconds, exit status) of one row; a signal is negative, a hang is None."""
    game = os.path.join(oracle.GAMES, row["game"])
    script = os.path.join(oracle.GOLDENS, row["script"])
    cmd = [binary, *row["flags"], "--echo", "--win", row["marker"], game, script]
    env = dict(os.environ, QUESTION_SEED="1")
    with open(os.path.join(OUT, f'{row["label"]}.{name}.txt'), "wb") as out:
        start = time.perf_counter()
        try:
            status = subprocess.run(cmd, stdout=out, stderr=subprocess.DEVNULL,
                                    env=env, timeout=TIMEOUT).returncode
        except subprocess.TimeoutExpired:
            status = None
        return time.perf_counter() - start, status


def main():
    want = set(sys.argv[1:])
    rows = [r for r in oracle.read_table() if not want or r["label"] in want]
    if not rows:
        sys.exit("no matching rows")
    build()
    oracle.ensure_movecont_stub()

    results = []
    for row in rows:
        path = os.path.join(oracle.GAMES, row["game"])
        if not os.path.exists(path):
            print(f'{row["label"]}: game file missing, skipped')
            continue
        qt, qs = play(QUESTION, row, "question")
        gt, gs = play(LEGACY, row, "geas")
        results.append(dict(label=row["label"], group=group(path),
                            qt=qt, qs=qs, gt=gt, gs=gs))

    with open(os.path.join(OUT, "results.tsv"), "w") as f:
        f.write("label\tgroup\tquestion_s\tquestion_exit\tgeas_s\tgeas_exit\n")
        for r in results:
            f.write(f'{r["label"]}\t{r["group"]}\t{r["qt"]:.3f}\t{r["qs"]}\t'
                    f'{r["gt"]:.3f}\t{r["gs"]}\n')

    print(f'{"Group":<26}{"rows":>5}{"Question":>10}{"Geas":>6}')
    for name in sorted({r["group"] for r in results}):
        g = [r for r in results if r["group"] == name]
        print(f'{name:<26}{len(g):>5}{sum(r["qs"] == 0 for r in g):>10}'
              f'{sum(r["gs"] == 0 for r in g):>6}')
    wins = [r for r in results if r["gs"] == 0]
    print(f'{"All":<26}{len(results):>5}{sum(r["qs"] == 0 for r in results):>10}{len(wins):>6}')

    ended = sum(r["gs"] == 1 for r in results)
    stopped = [r for r in results if r["gs"] not in (0, 1)]
    print(f"\nGeas misses: {ended} ran to the end of the script without the marker, "
          f"{len(stopped)} never got going")
    for r in stopped:
        print(f'  {r["label"]}: exit {r["gs"]}')

    both = [r for r in results if r["qs"] == 0 and r["gs"] == 0]
    slowest = max(results, key=lambda r: r["gt"])
    print("\nTime (s)                  Question    Geas")
    print(f'{"All rows":<26}{sum(r["qt"] for r in results):>8.2f}'
          f'{sum(r["gt"] for r in results):>8.2f}')
    print(f'{"Rows both complete":<26}{sum(r["qt"] for r in both):>8.2f}'
          f'{sum(r["gt"] for r in both):>8.2f}')
    print(f'{"Slowest: " + slowest["label"]:<26}{slowest["qt"]:>8.2f}{slowest["gt"]:>8.2f}')
    print("\nGeas completes: " + ", ".join(r["label"] for r in wins))
    return 0


if __name__ == "__main__":
    sys.exit(main())
