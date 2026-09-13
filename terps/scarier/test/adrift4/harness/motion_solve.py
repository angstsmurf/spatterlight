#!/usr/bin/env python3
"""Adaptively re-derive a winning command sequence for Motion.taf.

Reruns the game from scratch each turn (SCARE reads all of stdin up front,
so there is no way to negotiate one command at a time within a single
process) with a growing command list, inspects the trailing VARTRACE
snapshot (SCR_TRACE_VARS=1) and the visible text to decide the next
command, and appends it. Prints the full winning command list to stdout.

Usage, from test/adrift4/:
    python3 harness/motion_solve.py <seed> [max_turns]
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SCARE = os.path.join(HERE, "scare")
GAME = os.path.join(ROOT, "games", "Motion.taf")
WIN = "You scored 100 out of the maximum 100!"
RESTART_STAGE = "Hit Enter to simply restart this stage"
NEXT_PROMPT = 'type in "next"'

VT_RE = re.compile(
    r"VARTRACE 0:fuel=(-?\d+) 1:rocketpicture=(-?\d+) 2:rocketfuel=(-?\d+) "
    r"4:screens=(-?\d+) 5:closeness=(-?\d+) 6:stage=(-?\d+) 7:restart=(-?\d+) "
    r"8:floatingpicture=(-?\d+) 9:balloonish=(-?\d+) 10:ballooned=(-?\d+) "
    r"11:rate=(-?\d+) 12:direction facing=(-?\d+) "
    r"13:drivingpicturehorizon=(-?\d+) 14:drivingpicturevertic=(-?\d+)")

FIELDS = ["fuel", "rocketpicture", "rocketfuel", "screens", "closeness",
          "stage", "restart", "floatingpicture", "balloonish", "ballooned",
          "rate", "facing", "horizon", "vertic"]


def run(seed, cmds):
    env = dict(os.environ, SCR_ECHO_INPUT="1", SCR_TRACE_VARS="1",
               SCR_SKIP_WAITKEY="1", SCR_RNG="xoshiro",
               SCR_SEED=str(seed), LC_ALL="C")
    proc = subprocess.run(
        [SCARE, GAME],
        input=("\n".join(cmds) + "\n").encode("latin-1"),
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env)
    return proc.stdout.decode("latin-1")


def last_vars(out):
    m = None
    for m in VT_RE.finditer(out):
        pass
    if m is None:
        return None
    return dict(zip(FIELDS, (int(x) for x in m.groups())))


# Stage 3 ("The Drive") is a maze: the player has a facing (var 12, 1-4,
# cycled clockwise by "r" and counter-clockwise by "l") and a position
# (drivingpicturehorizon/drivingpicturevertic). A blank command attempts to
# move one step in the current facing; decoded via SCR_TRACE_TASKS, task 56
# runs every turn and tries 10 numbered cell/direction rules (tasks 57-66,
# each gated on facing plus position thresholds); if none match, task 67 (a
# true no-op fallback) absorbs the turn -- i.e. the move is silently
# blocked (no wall message, no error, just no position change). BUT even
# when a rule's restrictions DO pass, its action is a *random* increment
# (e.g. "drivingpicturehorizon += random(0,1)") -- so a single blank
# command that doesn't move you is NOT proof the direction is walled; it
# might just be an unlucky draw of 0 on an open path. A single failed
# attempt is therefore not enough to tell "wall" from "bad luck", so each
# candidate direction is retried several times before being written off as
# blocked (with random(0,1) odds that's a <0.01% false-negative rate).
# This implements a right-hand-wall-following maze solver: at each cell,
# relative to the heading we arrived with, try turning right (+1), straight
# (+0), left (-1), then behind (+2, the way we came) in that order, each
# retried up to _MAX_RETRIES times, moving on as soon as one succeeds.
_MAX_RETRIES = 15
_nav = {"arrived_facing": None, "oi": 0, "target_facing": None,
        "awaiting_result": False, "pre_pos": None, "stage_seen": False,
        "retries_left": _MAX_RETRIES}


def _turn_cmd(cur, target):
    """One command that reduces the gap from cur facing to target facing."""
    delta = (target - cur) % 4
    if delta == 0:
        return None
    if delta <= 2:
        return "r"
    return "l"


def _stage3_move(v):
    facing = v["facing"]
    pos = (v["horizon"], v["vertic"])
    offsets = [1, 0, -1, 2]

    if not _nav["stage_seen"]:
        _nav.update(stage_seen=True, arrived_facing=facing, oi=0,
                    target_facing=None, awaiting_result=False, pre_pos=None,
                    retries_left=_MAX_RETRIES)

    if _nav["awaiting_result"]:
        _nav["awaiting_result"] = False
        if pos != _nav["pre_pos"]:
            # Move succeeded -- new cell, restart the search from here.
            _nav.update(arrived_facing=_nav["target_facing"], oi=0,
                        target_facing=None, pre_pos=None,
                        retries_left=_MAX_RETRIES)
        else:
            _nav["retries_left"] -= 1
            if _nav["retries_left"] > 0:
                # Still not confirmed blocked -- retry the same facing.
                _nav["awaiting_result"] = True
                _nav["pre_pos"] = pos
                return ""
            # Exhausted retries -- genuinely blocked, try the next offset.
            _nav["oi"] += 1
            _nav["retries_left"] = _MAX_RETRIES
            if _nav["oi"] >= len(offsets):
                # Shouldn't happen (offset +2 is always the way we came,
                # except at the very start) -- bail rather than loop.
                return ""
            _nav["target_facing"] = None

    if _nav["target_facing"] is None:
        off = offsets[_nav["oi"]]
        _nav["target_facing"] = ((_nav["arrived_facing"] - 1 + off) % 4) + 1

    cmd = _turn_cmd(facing, _nav["target_facing"])
    if cmd is not None:
        return cmd
    _nav["awaiting_result"] = True
    _nav["pre_pos"] = pos
    return ""


def next_move(out, turn, last_f_turn):
    if WIN in out:
        return "WIN"
    tail = out[-4000:]
    if RESTART_STAGE in tail:
        return "CRASH"
    if NEXT_PROMPT in tail:
        return "next"
    v = last_vars(out)
    if v is None:
        return ""
    if v["stage"] == 1:
        # "fuel" (var 0) is a hard-capped resource (9 successful add-fuel
        # actions for the *whole* ascent -- the last charges are needed
        # late, to survive the final climb from rocketpicture=22 (where
        # the 3rd/last closeness checkpoint fires) up to rocketpicture=29
        # (the wrap that actually triggers "Congratulations", regardless
        # of closeness). Spending charges on a fixed early schedule burns
        # out the budget well before that final climb. So only press "f"
        # when the gauge is genuinely in trouble (deeply negative), with
        # just enough cooldown to let one press's delayed effect land
        # before considering another -- this conserves charges for
        # whenever they're actually needed, including near the end.
        if (v["fuel"] < 9 and v["rocketfuel"] <= 0
                and turn - last_f_turn >= 8):
            return "f"
        return ""
    if v["stage"] == 2:
        # The real win/loss check (decoded via SCR_TRACE_TASKS) runs every
        # turn once floatingpicture (var 8, the descent-progress counter)
        # approaches its terminal zone: task 38 (win) fires once
        # floatingpicture>=34 AND rate(var 11)<=2; task 39 (loss) fires once
        # floatingpicture>=35 AND rate>=2. So device count (balloonish) is
        # not the outcome by itself -- what matters is rate<=2 at the exact
        # moment floatingpicture crosses 34. "f" (activate a device) resets
        # rate to 1, but only succeeds while ballooned (var 10) is 0 -- i.e.
        # no previously-activated device is still active/pending -- so
        # pressing it every single turn (the old rule) mostly fails, and on
        # the presses that *do* succeed it freezes floatingpicture (rate=1
        # => next = old-1+1 = old) for no benefit early on, while still
        # burning through the 3-device budget long before it's actually
        # needed near the checkpoint. Instead: only press once rate has
        # crept up to >=2 (no point resetting an already-low rate), and
        # press unconditionally once floatingpicture is nearing 34 to try to
        # walk through it with rate pinned low.
        if (v["balloonish"] > 0 and v["ballooned"] == 0
                and (v["rate"] >= 5
                     or (29 <= v["floatingpicture"] < 34))):
            return "f"
        return ""
    if v["stage"] == 3:
        return _stage3_move(v)
    return ""


def main():
    seed = sys.argv[1]
    max_turns = int(sys.argv[2]) if len(sys.argv) > 2 else 300
    cmds = []
    last_f_turn = -1000
    for turn in range(max_turns):
        out = run(seed, cmds)
        mv = next_move(out, turn, last_f_turn)
        if mv == "WIN":
            sys.stderr.write("# WIN after %d turns\n" % turn)
            for c in cmds:
                print(c)
            return
        if mv == "CRASH":
            sys.stderr.write(
                "# stage crash/restart at turn %d -- dumping tail\n" % turn)
            sys.stderr.write("\n".join(out.split("\n")[-60:]) + "\n")
            raise SystemExit(1)
        if mv == "f":
            last_f_turn = turn
        v = last_vars(out)
        sys.stderr.write("# turn %d: cmd=%r vars=%s\n" % (turn, mv, v))
        cmds.append(mv)
    sys.stderr.write("# gave up after %d turns; dumping tail\n" % max_turns)
    sys.stderr.write("\n".join(out.split("\n")[-80:]) + "\n")
    raise SystemExit(1)


if __name__ == "__main__":
    main()
