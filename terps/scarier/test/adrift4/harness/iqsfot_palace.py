#!/usr/bin/env python3
"""Re-derive the Irvine Quik chapter-5 palace fight for the harness row's seed.

The palace (rooms 42-53) is patrolled by four mooks, NPCs 14-17, which EVENTs
15-18 respawn into the player's room on ranged timers -- so any change to the
RNG stream or the tick order re-phases them and the fixed attack weave desyncs.
The rules, from the row's comment block in run_v4_walkthroughs.sh:

  * a correct-verb attack always KOs (sentry punch, guard kick, patrol throw,
    soldier punch);
  * leaving a room is refused while any mook is in it;
  * the elite (NPC 18) never blocks and is walked past.

The solver is greedy: after every command it asks the scarier debugger where
the player and the mooks are (a debugger query spends no turn), fights whoever
shares the room, and otherwise takes the next step of the fixed route.

Usage, from test/adrift4/:
    python3 harness/iqsfot_palace.py [seed]      # default: the row's seed, 31

It prints the whole solution with the palace block re-derived; redirect it over
goldens/iqsfot_solution.txt and bless.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SCARE = os.path.join(HERE, "scare")
GAME = os.path.join(ROOT, "games", "iqsfot.taf")
SOLUTION = os.path.join(ROOT, "goldens", "iqsfot_solution.txt")

MOOKS = {14: "punch sentry", 15: "kick guard", 16: "throw patrol",
         17: "punch soldier"}
PALACE = range(42, 54)
# The route with every attack stripped out; `punch sentinel` is the scripted
# lesson at the back door, not a fight.  It ends by entering the throne room.
ROUTE = ["punch sentinel", "n", "n", "e", "s", "s", "s", "e", "x crates",
         "deploy", "get key", "get key", "deploy", "w", "n", "n", "n", "n",
         "w", "n", "w", "unlock door", "w"]
MOVES = {"n", "s", "e", "w"}
THRONE_ROOM = 54


def run(seed, commands):
    """Play `commands`, then query the debugger; return (room, mooks, damage, text)."""
    feed = ["continue"] + commands + [
        "debug", "npcs 14 17", "variables 41", "player", "continue",
        "quit", "y"]
    env = dict(os.environ, SCR_DEBUGGER_ENABLED="1", SCR_ECHO_INPUT="1",
               SCR_SEED=str(seed), SCR_SKIP_WAITKEY="1", LC_ALL="C")
    out = subprocess.run([SCARE, GAME], input="\n".join(feed).encode() + b"\n",
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                         env=env).stdout.decode("latin-1")
    game, _, debug = out.rpartition("\n> debug\n")
    mooks = {}
    for m in re.finditer(r'NPC (\d+) .*\n(?:    .*\n)*?    In Room (\d+)', debug):
        mooks[int(m.group(1))] = int(m.group(2))
    room = int(re.search(r'Player "Irvine"\n    In Room (\d+)', debug).group(1))
    damage = int(re.search(r'"Irvine_Health"\n    Value = (-?\d+)', debug).group(1))
    return room, mooks, damage, game


def main():
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 31
    lines = open(SOLUTION).read().splitlines()
    header = [l for l in lines if l.startswith("#")]
    body = [l for l in lines if not l.startswith("#")]
    ins = [i for i, l in enumerate(body) if l == "in"]
    prefix = body[:ins[1] + 1]              # through the palace back door
    tail = body[body.index("teach fan karate"):]

    block, step, last_room = [], 0, None
    while step < len(ROUTE):
        room, mooks, damage, text = run(seed, prefix + block)
        if "IMPRISONED!" in text:
            raise SystemExit("imprisoned after: %s" % " / ".join(block))
        if room == THRONE_ROOM:
            break
        # A refused move goes round again once the room is clear.
        if block and block[-1] in MOVES and room == last_room:
            step -= 1
        last_room = room
        here = sorted(n for n, r in mooks.items() if r == room)
        if room in PALACE and here:
            cmd = MOOKS[here[0]]
        else:
            cmd = ROUTE[step]
            step += 1
        block.append(cmd)
        print("%-14s room=%d damage=%d here=%s" % (cmd, room, damage, here),
              file=sys.stderr)
    room, _, damage, _ = run(seed, prefix + block)
    if room != THRONE_ROOM:
        raise SystemExit("route ended in room %d, not the throne room" % room)
    print("palace block: %d commands, damage %d" % (len(block), damage),
          file=sys.stderr)
    print("\n".join(header + prefix + block + tail))


if __name__ == "__main__":
    main()
