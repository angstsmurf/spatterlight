#!/usr/bin/env python3
"""Derive a winning route through The Puzzle Box's ten combination locks.

Every lock is randomised at load (and again after a wrong answer), so no fixed
command list can survive an RNG change -- the route has to be recomputed per
seed.  Each lock is a set of dials with an ADRIFT variable pair: the target
`X` and the player's current setting `sX`, and the box's red button succeeds
when every `X - sX` is zero (tasks "check puzzle <Na>" in an SCR_DUMP_TASKS
dump).  So the route is: read the pair with SCR_TRACE_VARS, turn each dial
`(target - current) mod <positions>` times, press the button, repeat.

Three locks also want the clue looked at first -- the weathercock, the pigs
and the ceiling in the oil painting; those are hard restrictions on the button
task ("push *button*, not seen direction"), not flavour.

Usage, from test/adrift4/:
    python3 harness/puzzlebox_solve.py [seed]

It prints the whole command block for goldens/puzzlebox_solution.txt, and
verifies it by replaying it once more and looking for the win text.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SCARE = os.path.join(HERE, "scare")
GAME = os.path.join(ROOT, "games", "puzzlebox.taf")
WIN = 'shouts, "Get out and stay out!"'

# variables the run is traced with: every target/current pair plus the gates
TRACE = ("switch1,switch2,switch3,switch4,switch5,sw1,sw2,sw3,sw4,sw5,"
         "hour,minutes,shour,sminutes,"
         "bow1,bow2,bow3,bow4,bow5,bow6,bow7,"
         "sbow1,sbow2,sbow3,sbow4,sbow5,sbow6,sbow7,"
         "real-weathercock,compass,towna,townb,townc,stowna,stownb,stownc,"
         "rom1,rom2,rom3,rom4,rom5,srom1,srom2,srom3,srom4,srom5,"
         "pig,spig,sym,ssym,sym1,sym2,sym3,sym4,sym5,"
         "ssym1,ssym2,ssym3,ssym4,ssym5,bin1,bin2,bin3,sbin1,sbin2,sbin3")


def play(seed, cmds):
    """Play `cmds`; return (transcript, variables after the last turn)."""
    env = dict(os.environ, LC_ALL="C", SCR_SEED=str(seed), SCR_TRACE_VARS=TRACE)
    proc = subprocess.run(
        [SCARE, GAME], input=("\n".join(cmds) + "\nquit\ny\n").encode("latin-1"),
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
    traces = [dict(re.findall(r"(?:\d+:)?([\w-]+)=(-?\d+)", line))
              for line in proc.stderr.decode("latin-1").splitlines()
              if line.startswith("VARTRACE")]
    if not traces:
        raise SystemExit("no VARTRACE: is `scare` built with -DSCARIER_DUMP_TOOLS?")
    return proc.stdout.decode("latin-1").replace("\r", ""), {
        k: int(v) for k, v in traces[-1].items()}


def town_names():
    """{variable: {value: town}} for the three town buttons of puzzle 5."""
    env = dict(os.environ, LC_ALL="C", SCR_DUMP_TASKS="1")
    proc = subprocess.run([SCARE, GAME], input=b"1\nquit\ny\n",
                          stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                          env=env)
    names, town = {}, None
    for line in proc.stderr.decode("latin-1").splitlines():
        match = re.search(r"cmd=\[\[push\]\{the\}\[(\w+)\]\{button\}\]", line)
        if match:
            town = match.group(1)
        match = re.search(r"ACT type=3 v1=(5[567]) v2=0 v3=(\d+)", line)
        if match and town:
            names.setdefault({"55": "stowna", "56": "stownb",
                              "57": "stownc"}[match.group(1)],
                             {})[int(match.group(2))] = town
            town = None
    if len(names) != 3:
        raise SystemExit("could not read the town buttons from the dump")
    return names


def turns(target, current, positions):
    return (target - current) % positions


def dials(var, count, positions, numbered=True):
    """One lock's worth of `turn dial N` commands, as a function of the vars."""
    def stage(v):
        out = []
        for i in range(1, count + 1):
            suffix = str(i) if count > 1 else ""
            command = "turn dial %d" % i if numbered and count > 1 else "turn dial"
            out += [command] * turns(v[var + suffix], v["s" + var + suffix],
                                     positions)
        return out
    return stage


def switches(v):
    out = []
    for i in range(1, 6):
        current, target = v["switch%d" % i], v["sw%d" % i]
        # three positions: pulling moves the switch down, pushing moves it up
        word = "pull" if target > current else "push"
        out += ["%s switch %d" % (word, i)] * abs(target - current)
    return out


def clock(v):
    # the painting's church clock, plus five minutes -- the game's own
    # if(minutes=11 & hour=N, N+1, hour) rollover, quirk and all
    hour = v["hour"] + 1 if v["minutes"] == 11 else v["hour"]
    minute = v["minutes"] % 12 + 1
    return (["move hour hand"] * turns(hour, v["shour"], 12) +
            ["move minute hand"] * turns(minute, v["sminutes"], 12))


def compass(v):
    return ["x weathercock"] + ["turn pointer"] * turns(
        v["real-weathercock"], v["compass"], 8)


def towns(names):
    def stage(v):
        return ["push %s button" % names[key][v[key[1:]]]
                for key in ("stowna", "stownb", "stownc")]
    return stage


def main():
    seed = sys.argv[1] if len(sys.argv) > 1 else "1"
    stages = [
        switches,
        clock,
        dials("bow", 7, 7),
        compass,
        towns(town_names()),
        dials("rom", 5, 5),
        lambda v: ["x animal"] + ["turn dial"] * turns(v["pig"], v["spig"], 9),
        lambda v: ["x ceiling"] + ["turn dial"] * turns(v["sym"], v["ssym"], 7),
        dials("sym", 5, 7),
        dials("bin", 3, 10),
    ]

    # "1" picks Play the game off the title menu; the look is not optional --
    # the box is only randomised on the first real turn
    route = ["1", "look"]
    for number, stage in enumerate(stages, 1):
        transcript, variables = play(seed, route)
        route += stage(variables) + ["push button"]
        if transcript.count("reveal the next puzzle") != number - 1:
            raise SystemExit("puzzle %d did not open" % (number - 1))

    transcript, _ = play(seed, route)
    if WIN not in transcript:
        raise SystemExit("route does not win at seed %s" % seed)
    print("\n".join(route))


if __name__ == "__main__":
    main()
