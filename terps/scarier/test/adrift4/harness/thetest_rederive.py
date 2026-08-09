#!/usr/bin/env python3
"""Re-derive thetest_win_solution.txt under a changed RNG stream.

thetest's winning route is almost entirely made of "keep trying until the
random thing happens" blocks, so the command counts in the solution file are
not part of the puzzle -- they are the number of dice rolls the old stream
happened to need:

  - `unlock door` until the colour-changing key matches the door twice in a
    row ("the door click open");
  - `shout <triangle number>` until the Robot Guard is in the room to hear it
    (it storms in and out at random), six times over;
  - `teleport` until the random destination is the Morse Room.

Any engine change that consumes a different number of RNG draws re-times all
three, which is what the 2026-08-04 event-restart arbitration did (§8 of
RUNNER_TESTS_TODO.md: 3.9 immediate restarts are silent and keep their full
period, so events no longer roll a length every turn).  Rather than hand-tune
the pads again, this replays the prefix after every appended command and
grows each block until its marker appears.

Run from terps/scarier/test/adrift4/harness; writes the solution file.
"""
import subprocess, sys

SCARE = "./scare"
GAME = "../games/thetest.taf"
OUT = "../goldens/thetest_win_solution.txt"

# No `#` header goes into the solution file: thetest opens with keypress
# waits, and os_ansi only skips comment lines at a *line* prompt, so a comment
# gets eaten as a keypress and shifts the whole route by one command.
HEADER = []

# The 14-command prelude, up to the first `unlock door`: fixed puzzle steps.
PRE = [c for c in open(OUT, encoding="latin-1").read().split("\n")
       if not c.startswith("#")]
FIRST = next(i for i, c in enumerate(PRE) if c.strip() == "unlock door")
PRE = PRE[:FIRST]


def replay(cmds):
    inp = "\n".join(cmds + ["quit", "y"]) + "\n"
    p = subprocess.run([SCARE, GAME], input=inp.encode("latin-1"),
                       capture_output=True)
    return p.stdout.decode("latin-1", "replace")


def grow(cmds, cmd, done, limit=60, label=""):
    """Append `cmd` (a command or a list of them) until done(transcript)."""
    cmds = list(cmds)
    step = cmd if isinstance(cmd, list) else [cmd]
    for i in range(limit):
        cmds += step
        out = replay(cmds)
        if "afraid you are dead" in out:
            sys.exit("died during %s after %d x %r" % (label, i + 1, cmd))
        if done(out):
            print("  %-24s %3d x %r" % (label, i + 1, cmd))
            return cmds
    sys.exit("gave up on %s after %d x %r" % (label, limit, cmd))


def search(cmds, cmd, done, limit=400, label=""):
    """Same, but the marker is monotone in the count, so bisect instead of
    replaying once per command -- the unlock block runs to ~120."""
    lo, hi = 1, limit
    if not done(replay(cmds + [cmd] * hi)):
        sys.exit("gave up on %s within %d x %r" % (label, limit, cmd))
    while lo < hi:
        mid = (lo + hi) // 2
        if done(replay(cmds + [cmd] * mid)):
            hi = mid
        else:
            lo = mid + 1
    print("  %-24s %3d x %r" % (label, lo, cmd))
    return cmds + [cmd] * lo


cmds = list(PRE)

# 2. The colour-changing key: unlock until two colour matches in a row.
cmds = search(cmds, "unlock door", lambda o: "click open" in o,
              label="Room of Eternal Dialing")

# 3. The phone (fixed: 987 is the only number the dial task accepts, and the
#    code it reads back is deterministic once the RNG has got this far).
cmds += ["dial 987"]
out = replay(cmds)
code = out.rsplit("Them: Yes its ", 1)[1].split(".")[0]
print("  %-24s %s" % ("door code", code))
cmds += ["enter " + code, "east"]

# 4. Six triangle numbers, each shouted until the guard is in the room.
for n, want in enumerate(("1", "3", "6", "10", "15", "21"), start=1):
    cmds = grow(cmds, "shout " + want,
                lambda o, n=n: o.count("scurries off") >= n,
                label="shout " + want)

# The command after the sixth success is stolen by #findoutsecret.
cmds = grow(cmds, "take key", lambda o: "dropped his key" in o,
            limit=4, label="guard's key")
cmds += ["take key", "take teleporter", "west"]

# 5. Teleport (random destination) until it lands in the Morse Room.
# `teleport` itself does not say where you land, so each try is a pair.
cmds = grow(cmds, ["teleport", "look"], lambda o: "Morse Room" in o,
            label="Morse Room")

# 6/7. The endgame is deterministic.
cmds += ["tiddlywink", "take tiddlywink", "put tiddlywink in slot",
         "east", "push south", "use key with keyhole"]

out = replay(cmds)
if "Well done!  You won!" not in out:
    sys.exit("route does not win")
print("win in %d commands" % len(cmds))
open(OUT, "w", encoding="latin-1").write("\n".join(HEADER + cmds) + "\n")
