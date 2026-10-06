#!/usr/bin/env python3
"""Replay command scripts through a headless text build of an interpreter.

  run_script_tests.py SCRIPTDIR IMAGEDIR HL [PREFIX...]   check (all rows, or the
                                                          scripts or games whose
                                                          name starts so)
  run_script_tests.py SCRIPTDIR IMAGEDIR HL --bless [PREFIX...]
                                                          rewrite the hashes
  run_script_tests.py SCRIPTDIR IMAGEDIR HL --save DIR    also write
                                                          DIR/<script>/<id>.txt
  run_script_tests.py SCRIPTDIR IMAGEDIR HL --list        one line per row with
                                                          how it ends

Every row of SCRIPTDIR/scripts.tsv names a command script in that folder and
one game from IMAGEDIR/games.manifest.tsv, the manifest of the picture tests
(run_image_tests.py). The game is started in HL (plus_hl, taylor_hl) with the
keys of its manifest row and then the script on standard input, and the
SHA-256 and line count of the transcript are compared with the row. The
transcripts themselves are game text and are not kept; --save writes them
out, so that two builds can be compared with diff -r.

The same script is run on every release of its game, on purpose: one script
exercises every loader and every variant of the game's text. A replay that
does not reach the end of the game (see SCRIPTDIR/README.md) is still
deterministic, and that is all a regression test needs.

The games are copyrighted and not in the repository. Rows whose game is not in
IMAGEDIR/games/ are skipped, so the suite passes (testing nothing) on a bare
checkout.
"""

import argparse
import concurrent.futures
import hashlib
import os
import re
import subprocess
import sys

import run_image_tests as images

TIMEOUT = 60
# CheapGlk reports the window calls it cannot honour on standard output.
NOISE = re.compile(rb"Glk library error:[^\n]*\n")
# A status window that CheapGlk does not have is cleared with empty lines.
BLANKS = re.compile(rb"\n{3,}")


def read_table(table):
    pairs = []
    with open(table, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            fields = line.split("\t")
            if len(fields) != 4:
                sys.exit(f"{table}:{n}: expected 4 columns, got {len(fields)}")
            pairs.append(fields)
    return pairs


def write_table(table, pairs):
    with open(table, encoding="utf-8") as f:
        head = [l for l in f if l.startswith("#")]
    with open(table, "w", encoding="utf-8") as f:
        f.writelines(head)
        for fields in pairs:
            f.write("\t".join(fields) + "\n")


def transcript(binary, script, row, games):
    with open(script, "rb") as f:
        commands = f.read()
    # CheapGlk reads a key press as a line and takes its first character.
    keys = "".join(k + "\n" for k in row.keys).encode()
    try:
        out = subprocess.run(
            [binary, os.path.join(games, row.dir, row.file)],
            input=keys + commands, stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL, timeout=TIMEOUT).stdout
    except subprocess.TimeoutExpired:
        return b"TIMEOUT\n"
    return BLANKS.sub(b"\n\n", NOISE.sub(b"", out))


def ending(text):
    text = re.sub(r"[_=*]{8,}", "\n", text.decode("latin-1"))
    lines = [l.strip() for l in text.splitlines()
             if l.strip() and "<end of input>" not in l]
    return " / ".join(lines[-3:])[-170:]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("scriptdir")
    ap.add_argument("imagedir")
    ap.add_argument("hl")
    ap.add_argument("--bless", action="store_true")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--save", metavar="DIR")
    ap.add_argument("--games",
        help="where the games are (default: IMAGEDIR/games)")
    ap.add_argument("args", nargs="*")
    opt = ap.parse_args()

    table = os.path.join(opt.scriptdir, "scripts.tsv")
    games = opt.games or os.path.join(opt.imagedir, "games")
    manifest = images.read_manifest(os.path.join(opt.imagedir, "games.manifest.tsv"))
    rows = {r.id: r for r in manifest if r.id != images.COMPANION}
    pairs = read_table(table)

    def path(script):
        return os.path.join(opt.scriptdir, script + ".txt")

    for script, id, _, _ in pairs:
        if id not in rows:
            sys.exit(f"{table}: no manifest row {id}")
        if not os.path.exists(path(script)):
            sys.exit(f"{table}: no script {script}.txt")

    selected = [p for p in pairs if not opt.args
                or any(p[0].startswith(a) or p[1].startswith(a) for a in opt.args)]
    if not selected:
        sys.exit("no row matches " + " ".join(opt.args))

    dirs = {rows[p[1]].dir for p in selected}
    missing, wrong = images.check_corpus([r for r in manifest if r.dir in dirs], games)
    runnable = [p for p in selected
                if rows[p[1]].dir not in missing and rows[p[1]].dir not in wrong]
    if runnable and not os.access(opt.hl, os.X_OK):
        sys.exit(f"{opt.hl} is not built")

    with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 4) as pool:
        results = list(pool.map(
            lambda p: transcript(opt.hl, path(p[0]), rows[p[1]], games), runnable))

    passed = failed = blessed = 0
    for pair, got in zip(runnable, results):
        script, id, lines, digest = pair
        if opt.save:
            os.makedirs(os.path.join(opt.save, script), exist_ok=True)
            with open(os.path.join(opt.save, script, id + ".txt"), "wb") as f:
                f.write(got)
        if opt.list:
            print(f"{script:24}{id:40}{ending(got)}")
            continue
        now = [str(got.count(b"\n")), hashlib.sha256(got).hexdigest()]
        if opt.bless:
            if pair[2:] != now:
                pair[2:] = now
                blessed += 1
                print(f"BLESSED {script} {id}")
        elif pair[2:] == now:
            passed += 1
        else:
            failed += 1
            print(f"FAIL    {script} on {id}: {now[0]} lines, expected {lines}; "
                  f"it now ends: {ending(got)}")
    if opt.list:
        return 0
    if opt.bless:
        write_table(table, pairs)
        print(f"blessed {blessed} of {len(runnable)} rows")
        return 0
    print(f"script tests: {passed} passed, {failed} failed, "
          f"{len(selected) - len(runnable)} skipped (game not in games/)")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
