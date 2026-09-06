#!/usr/bin/env python3
"""Re-derive the winning route through The Test for a given seed.

Four of the game's puzzles read out of the RNG, so a fixed command list only
holds for one RNG thread:

  * the door of the first room unlocks when the key colour matches the door
    colour twice running, and both recolour randomly every turn (the game's
    own hint: "typing unlock door 500 times usually works") -- the seed is
    chosen for a short run of these, so pick a new one rather than pad;
  * the phone reads the door's randomised serial back as a 7-digit code, which
    has to be typed into the panel;
  * the robot guard wanders, and only listens when it is in the room; the
    number to shout is the triangular number of its own counter, which climbs
    (and after five rounds jumps to a random 6-20) with every success;
  * the broken teleporter lands somewhere random, and the route needs the
    Morse Room.

Usage, from test/adrift4/:
    python3 harness/thetest_solve.py [seed]

Prints the whole solution file.  The seed must match the row's SCR_SEED in
harness/run_v4_walkthroughs.sh; try a few and keep one whose unlock run is
short (each attempt is four lines of golden).
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SCARE = os.path.join(HERE, "scare")
GAME = os.path.join(ROOT, "games", "thetest.taf")
WIN = "Well done!  You won!"
# fixed opening: jam the fluff machine, which coughs up the first key
OPENING = ["open clothes",
           "take fluff", "drop fluff", "z",
           "take fluff", "drop fluff", "z",
           "take fluff", "drop fluff", "z",
           "take key"]
ENDING = ["tiddlywink", "take tiddlywink", "put tiddlywink in slot",
          "east", "push south", "use key with keyhole"]


def play(seed, cmds):
    env = dict(os.environ, LC_ALL="C", SCR_SEED=str(seed), SCR_ECHO_INPUT="1",
               SCR_SKIP_WAITKEY="1", SCR_TRACE_VARS="robot2,guard2")
    proc = subprocess.run(
        [SCARE, GAME], input=("\n".join(cmds) + "\nquit\ny\n").encode("latin-1"),
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
    traces = [dict(re.findall(r"(?:\d+:)?([\w-]+)=(-?\d+)", line))
              for line in proc.stderr.decode("latin-1").splitlines()
              if line.startswith("VARTRACE")]
    if not traces:
        raise SystemExit("no VARTRACE: is `scare` built with -DSCARIER_DUMP_TOOLS?")
    return (proc.stdout.decode("latin-1").replace("\r", ""),
            {k: int(v) for k, v in traces[-1].items()})


def last_turn(transcript):
    return re.split(r"\n> ", transcript)[-2]


def main():
    seed = sys.argv[1] if len(sys.argv) > 1 else "8"
    route = list(OPENING)

    # the colour lock: brute force, the way the game intends
    for _ in range(500):
        transcript, _ = play(seed, route)
        if "Room of Eternal Dialing" in transcript:
            break
        route.append("unlock door")
    else:
        raise SystemExit("the door never opened -- try another seed")

    # the phone dictates the door code
    route.append("dial 987")
    transcript, _ = play(seed, route)
    code = re.search(r"Them: Yes its (\d+)", transcript)
    if not code:
        raise SystemExit("the phone did not read out a code")
    route += ["enter " + code.group(1), "east"]

    # six rounds of shouting the guard's triangular number at it
    transcript, variables = play(seed, route)
    while not variables["guard2"]:
        for _ in range(40):
            route.append("shout %d" % variables["robot2"])
            transcript, variables = play(seed, route)
            if "scurries off" in last_turn(transcript):
                break
        else:
            raise SystemExit("the guard never came back to be shouted at")
    route += ["take key", "take teleporter", "west"]

    # the teleporter is broken; keep trying until it drops us in the Morse Room
    for _ in range(30):
        transcript, _ = play(seed, route + ["teleport", "look"])
        if "Morse Room" in last_turn(transcript):
            route += ["teleport", "look"]
            break
        route.append("teleport")
    else:
        raise SystemExit("never landed in the Morse Room")

    route += ENDING
    transcript, _ = play(seed, route)
    if WIN not in transcript:
        raise SystemExit("route does not win at seed %s" % seed)
    print("\n".join(route))


if __name__ == "__main__":
    main()
