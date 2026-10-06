#!/usr/bin/env python3
"""Replay command scripts through the headless interpreter.

Every row of scripts.tsv names a command script in this folder and one game
image from ../formats/games.manifest.tsv. The image is started in scott_hl
with the script on standard input, and the SHA-256 and line count of the
transcript are compared with the row. The transcripts themselves are game
text and are not kept; --save writes them out, so that two builds can be
compared with diff -r.

The same script is run on every image of its game, on purpose: one script
exercises every loader and every variant of the game's text. Many of these
replays do not reach the end of the game (see README.md). They are still
deterministic, and that is all a regression test needs.

  run_script_tests.py [PREFIX...]         check (all rows, or the scripts or
                                          images whose name starts so)
  run_script_tests.py --bless [PREFIX...] rewrite the hashes
  run_script_tests.py --save DIR          also write DIR/<script>/<id>.txt
  run_script_tests.py --list              one line per row with how it ends
"""

import argparse
import concurrent.futures
import hashlib
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "formats"))
import run_format_tests as formats

TABLE = os.path.join(HERE, "scripts.tsv")
TIMEOUT = 60
# CheapGlk reports the window calls it cannot honour on standard output.
NOISE = re.compile(rb"Glk library error:[^\n]*\n")


def read_table():
    pairs = []
    with open(TABLE, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            fields = line.split("\t")
            if len(fields) == 4:
                fields.append("")
            if len(fields) != 5:
                sys.exit(f"{TABLE}:{n}: expected 4 or 5 columns, got {len(fields)}")
            pairs.append(fields)
    return pairs


def write_table(pairs):
    with open(TABLE, encoding="utf-8") as f:
        head = [l for l in f if l.startswith("#")]
    with open(TABLE, "w", encoding="utf-8") as f:
        f.writelines(head)
        for fields in pairs:
            f.write("\t".join(fields).rstrip("\t") + "\n")


def transcript(binary, script, row, keys, games):
    with open(os.path.join(HERE, script + ".txt"), "rb") as f:
        commands = f.read()
    menu = (row.input + "\n").encode() if row.input else b""
    # What the release asks before the first command: a key a line, as the
    # script itself has them. "_" is Return.
    menu += "".join(k.replace("_", "") + "\n" for k in keys).encode()
    try:
        out = subprocess.run(
            [binary, os.path.join(games, row.dir, row.file)],
            input=menu + commands, stdout=subprocess.PIPE,
            env=dict(os.environ, SCOTT_SCRIPT_KEYS="1"),
            stderr=subprocess.DEVNULL, timeout=TIMEOUT).stdout
    except subprocess.TimeoutExpired:
        return b"TIMEOUT\n"
    return NOISE.sub(b"", out)


def ending(text):
    lines = [l.strip() for l in text.decode("latin-1").splitlines()
             if l.strip() and "<end of input>" not in l and set(l.strip()) != {"*"}]
    return " / ".join(lines[-2:])[:150]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bless", action="store_true")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--save", metavar="DIR")
    ap.add_argument("--hl", default=os.path.join(HERE, "..", "..", "scott_hl"),
        help="the interpreter to run (default: ../../scott_hl)")
    ap.add_argument("--games", default=os.path.join(HERE, "..", "formats", "games"),
        help="where the images are (default: ../formats/games)")
    ap.add_argument("args", nargs="*")
    opt = ap.parse_args()

    rows = {r.id: r for r in formats.read_manifest()}
    pairs = read_table()
    for script, id, _, _, _ in pairs:
        if id not in rows:
            sys.exit(f"{TABLE}: no manifest row {id}")
        if not os.path.exists(os.path.join(HERE, script + ".txt")):
            sys.exit(f"{TABLE}: no script {script}.txt")

    selected = [p for p in pairs if not opt.args
                or any(p[0].startswith(a) or p[1].startswith(a) for a in opt.args)]
    if not selected:
        sys.exit("no row matches " + " ".join(opt.args))

    dirs = {rows[p[1]].dir for p in selected}
    missing, wrong = formats.check_corpus(
        [r for r in rows.values() if r.dir in dirs], opt.games)
    runnable = [p for p in selected
                if rows[p[1]].dir not in missing and rows[p[1]].dir not in wrong]
    if runnable and not os.access(opt.hl, os.X_OK):
        sys.exit("scott_hl is not built: make -f Makefile.headless scott_hl")

    with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 4) as pool:
        results = list(pool.map(
            lambda p: transcript(opt.hl, p[0], rows[p[1]], p[4], opt.games), runnable))

    passed = failed = blessed = 0
    for pair, got in zip(runnable, results):
        script, id, lines, digest, _ = pair
        if opt.save:
            os.makedirs(os.path.join(opt.save, script), exist_ok=True)
            with open(os.path.join(opt.save, script, id + ".txt"), "wb") as f:
                f.write(got)
        if opt.list:
            print(f"{script:24}{id:44}{ending(got)}")
            continue
        now = [str(got.count(b"\n")), hashlib.sha256(got).hexdigest()]
        if opt.bless:
            if pair[2:4] != now:
                pair[2:4] = now
                blessed += 1
                print(f"BLESSED {script} {id}")
        elif pair[2:4] == now:
            passed += 1
        else:
            failed += 1
            print(f"FAIL    {script} on {id}: {now[0]} lines, expected {lines}; "
                  f"it now ends: {ending(got)}")
    if opt.list:
        return 0
    if opt.bless:
        write_table(pairs)
        print(f"blessed {blessed} of {len(runnable)} rows")
        return 0
    print(f"script tests: {passed} passed, {failed} failed, "
          f"{len(selected) - len(runnable)} skipped (image not in games/)")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
