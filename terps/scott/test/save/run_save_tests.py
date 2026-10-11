#!/usr/bin/env python3
"""Saving and restoring to a file, in the headless interpreter.

Every case plays test/regression.dat in scott_hl, in a scratch directory.
CheapGlk asks for a file name on standard input and adds ".glksave", so a
script can save and restore as often as it likes.

The state of the game is read back by saving it: a save file holds every
counter, flag and item location, so two states are the same when their save
files are. A restore that should work is followed by a save, which has to
give the expected file. A restore of a damaged file is bracketed by two
saves, which have to be identical, and has to print the BAD DATA message.

  run_save_tests.py [--hl PATH] [-v] [NAME...]
"""

import argparse
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SCOTT = os.path.join(HERE, "..", "..")
GAME = os.path.join(HERE, "..", "regression.dat")
TIMEOUT = 20

BAD_DATA = "BAD DATA! Invalid save file."
SAVED = "Saved."

# regression.dat: 2 rooms, items 0..10, 16 counter lines in a save file
NUM_COUNTERS = 16
NUM_ROOMS = 2
CARRIED = 255
STATE_LINE = NUM_COUNTERS  # index of the "flags dark room ..." line
# Fields of that line
BITFLAGS, DARK, ROOM, COUNTER, SAVEDROOM, LIGHT, AUTOINV = range(7)

# Played before anything is saved, so that the live state is not the
# initial one: some items carried, one moved by the game, the player in
# the second room.
PREAMBLE = ["take all", "n", "drop coin"]


def play(hl, workdir, commands):
    """Run the commands, return the transcript."""
    result = subprocess.run(
        [hl, GAME], input=("\n".join(commands) + "\n").encode(),
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, cwd=workdir,
        timeout=TIMEOUT)
    return result.stdout.decode("latin-1")


def save(name):
    return ["save game", name]


def restore(name):
    return ["restore game", name]


def read_save(workdir, name):
    path = os.path.join(workdir, name + ".glksave")
    if not os.path.exists(path):
        return None
    with open(path) as f:
        return f.read().split("\n")[:-1]


def write_save(workdir, name, lines):
    with open(os.path.join(workdir, name + ".glksave"), "w") as f:
        f.write("".join(line + "\n" for line in lines))


def with_field(lines, index, value):
    """A copy with one field of the state line replaced."""
    fields = lines[STATE_LINE].split()
    fields[index] = str(value)
    return lines[:STATE_LINE] + [" ".join(fields)] + lines[STATE_LINE + 1:]


def with_line(lines, index, text):
    return lines[:index] + [text] + lines[index + 1:]


# A valid save file that differs from the live state in every field, built
# from a real one so the line counts fit the game.
def other_state(base):
    lines = [f"{i + 1} {i % (NUM_ROOMS + 1)}" for i in range(NUM_COUNTERS)]
    # Bit 40 is beyond 32 bits, bit 15 is the darkness flag
    flags = (1 << 40) | (1 << 15) | 5
    lines.append(f"{flags} 1 1 77 2 42 1")
    items = base[STATE_LINE + 1:]
    places = [0, 1, 2, CARRIED]
    lines += [str(places[i % len(places)]) for i in range(len(items))]
    return lines


# ---- Restores that have to work: (file to restore, file a save then gives)

def good_cases(base):
    other = other_state(base)
    cases = {}

    cases["every-field"] = (other, other)

    # Saves from before AutoInventory was added have six fields on the state
    # line. The setting keeps its current value, which is off.
    old = other[:]
    old[STATE_LINE] = " ".join(other[STATE_LINE].split()[:AUTOINV])
    cases["no-autoinventory-field"] = (old, with_field(other, AUTOINV, 0))

    # The oldest saves had darkness as a field of its own and not as a flag
    dark = with_field(with_field(other, BITFLAGS, 5), DARK, 1)
    cases["darkness-field-only"] = (dark, with_field(other, BITFLAGS, (1 << 15) | 5))

    # The limits of what is accepted
    cases["first-room"] = (with_field(other, ROOM, 1),) * 2
    cases["last-room"] = (with_field(other, ROOM, NUM_ROOMS),) * 2
    cases["saved-room-zero"] = (with_field(other, SAVEDROOM, 0),) * 2
    cases["negative-counter"] = (with_line(other, 3, "-12 1"),) * 2
    cases["light-never-runs-out"] = (with_field(other, LIGHT, -1),) * 2
    return cases


# ---- Files that have to be refused

def bad_cases(base):
    other = other_state(base)
    first_item = STATE_LINE + 1
    last = len(other) - 1
    return {
        "empty": [],
        "garbage": ["This is not", "a saved game"],
        "binary": ["\x00\x01\x02\xff\xfe"],
        "counters-only": other[:NUM_COUNTERS],
        "no-items": other[:first_item],
        "one-item-short": other[:-1],
        "one-line-too-many": other + ["0"],
        "trailing-text": other + ["x"],
        "counter-not-a-number": with_line(other, 5, "x 1"),
        "counter-line-one-field": with_line(other, 5, "7"),
        "room-flag-negative": with_line(other, 5, "7 -1"),
        "room-flag-past-last-room": with_line(other, 5, f"7 {NUM_ROOMS + 1}"),
        "state-line-five-fields": with_line(
            other, STATE_LINE, " ".join(other[STATE_LINE].split()[:5])),
        "state-line-not-numbers": with_line(other, STATE_LINE, "a b c d e f g"),
        "room-zero": with_field(other, ROOM, 0),
        "room-negative": with_field(other, ROOM, -1),
        "room-past-last-room": with_field(other, ROOM, NUM_ROOMS + 1),
        "saved-room-negative": with_field(other, SAVEDROOM, -1),
        "saved-room-past-last-room": with_field(other, SAVEDROOM, NUM_ROOMS + 1),
        "item-not-a-number": with_line(other, first_item + 2, "x"),
        "item-negative": with_line(other, first_item + 2, "-1"),
        "item-past-last-room": with_line(other, first_item + 2, str(NUM_ROOMS + 1)),
        "item-just-below-carried": with_line(other, first_item + 2, str(CARRIED - 1)),
        "last-item-past-last-room": with_line(other, last, str(NUM_ROOMS + 1)),
    }


class Failure(Exception):
    pass


def check(condition, message):
    if not condition:
        raise Failure(message)


def diff(expected, got):
    if got is None:
        return "no file was written"
    out = []
    for n in range(max(len(expected), len(got))):
        e = expected[n] if n < len(expected) else "(nothing)"
        g = got[n] if n < len(got) else "(nothing)"
        if e != g:
            out.append(f"line {n + 1}: expected {e!r}, got {g!r}")
    return "; ".join(out[:4])


def test_round_trip(hl, workdir):
    """Save, change everything a turn can change, restore, save again."""
    transcript = play(hl, workdir, PREAMBLE + save("first") + [
        "s", "drop all", "n"] + save("changed") + restore("first")
        + save("second") + ["inv"])
    first = read_save(workdir, "first")
    changed = read_save(workdir, "changed")
    second = read_save(workdir, "second")
    check(first is not None, "SAVE GAME wrote no file")
    check(transcript.count(SAVED) == 3, "expected three 'Saved.' messages")
    check(BAD_DATA not in transcript, "a good file was refused")
    check(changed != first, "the moves in between changed nothing")
    check(second == first, "state after restore differs: " + diff(first, second))
    check(len(first) == STATE_LINE + 1 + 11, f"{len(first)} lines in the save file")
    # The restored inventory, not the empty one from before the restore
    start = transcript.rindex("I'm carrying")
    carried = transcript[start:transcript.index("I'm in", start)]
    check("A book" in carried and "A gem" in carried and "A coin" not in carried,
          "inventory after restore is wrong")


def test_restore_twice(hl, workdir):
    """A file can be restored more than once in a session."""
    play(hl, workdir, PREAMBLE + save("first") + ["s", "drop all"]
         + restore("first") + ["s", "drop all"] + restore("first")
         + save("second"))
    first = read_save(workdir, "first")
    second = read_save(workdir, "second")
    check(second == first, "state after second restore differs: " + diff(first, second))


def test_restore_at_start(hl, workdir):
    """A file from one session restores in a new one, as the first command."""
    play(hl, workdir, PREAMBLE + save("first"))
    play(hl, workdir, restore("first") + save("second"))
    first = read_save(workdir, "first")
    second = read_save(workdir, "second")
    check(second == first, "state in the new session differs: " + diff(first, second))


def test_missing_file(hl, workdir):
    """Restoring a file that is not there changes nothing."""
    play(hl, workdir, PREAMBLE + save("before") + restore("nosuchfile")
         + save("after"))
    before = read_save(workdir, "before")
    after = read_save(workdir, "after")
    check(after == before, "state changed: " + diff(before, after))


def baseline(hl, workdir):
    play(hl, workdir, PREAMBLE + save("base"))
    base = read_save(workdir, "base")
    if base is None:
        sys.exit("run_save_tests: SAVE GAME wrote no file, cannot build the cases")
    return base


def make_good(lines, expected):
    def test(hl, workdir):
        write_save(workdir, "given", lines)
        transcript = play(hl, workdir, PREAMBLE + restore("given") + save("after"))
        check(BAD_DATA not in transcript, "the file was refused")
        after = read_save(workdir, "after")
        check(after == expected, diff(expected, after))
    return test


def make_bad(lines):
    def test(hl, workdir):
        write_save(workdir, "given", lines)
        transcript = play(hl, workdir, PREAMBLE + save("before")
                          + restore("given") + save("after") + ["inv"])
        check(BAD_DATA in transcript, "the file was accepted")
        before = read_save(workdir, "before")
        after = read_save(workdir, "after")
        check(after == before, "state changed: " + diff(before, after))
        # The game goes on: the command after the failed restore is obeyed
        check("I'm carrying" in transcript[transcript.index(BAD_DATA):],
              "no reply to the command after the failed restore")
    return test


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--hl", default=os.path.join(SCOTT, "scott_hl"))
    parser.add_argument("-v", "--verbose", action="store_true")
    parser.add_argument("names", nargs="*", metavar="NAME")
    args = parser.parse_args()
    hl = os.path.abspath(args.hl)
    if not os.path.exists(hl):
        sys.exit(f"run_save_tests: {hl} not found (make -f Makefile.headless)")

    with tempfile.TemporaryDirectory() as workdir:
        base = baseline(hl, workdir)

    tests = {
        "round-trip": test_round_trip,
        "restore-twice": test_restore_twice,
        "restore-at-start": test_restore_at_start,
        "missing-file": test_missing_file,
    }
    for name, (lines, expected) in good_cases(base).items():
        tests["good-" + name] = make_good(lines, expected)
    for name, lines in bad_cases(base).items():
        tests["bad-" + name] = make_bad(lines)

    passed = failed = 0
    for name, test in tests.items():
        if args.names and not any(name.startswith(n) for n in args.names):
            continue
        with tempfile.TemporaryDirectory() as workdir:
            try:
                test(hl, workdir)
            except Failure as e:
                failed += 1
                print(f"FAIL {name}: {e}")
            except subprocess.TimeoutExpired:
                failed += 1
                print(f"FAIL {name}: timed out")
            else:
                passed += 1
                if args.verbose:
                    print(f"ok   {name}")
    print(f"save tests: {passed} passed, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
