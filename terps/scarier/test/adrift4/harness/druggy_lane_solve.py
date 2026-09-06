#!/usr/bin/env python3
"""
Re-derive the Druggy Lane trading route for whatever price stream the current
RNG produces.

Druggy Lane is Dope Wars: thirty days, six drugs whose prices are re-rolled
every night, a $25000 debt that compounds daily, and a win that only asks you
to clear the debt.  The route is therefore not a puzzle solution but a
*schedule*, and the schedule is worthless the moment the random stream moves --
which is exactly what the scr_congruential_rand first-draw fix did.

The prices depend on the number of turns taken, not on what those turns were,
so the route keeps a rigid three-commands-per-day shape:

    look | sell <qty> <drug>          (day 1 has nothing to sell)
    buy <qty> <drug>
    next day

Holding that shape fixed means a probe run that buys ONE unit sees exactly the
price stream the real route will see, so each day can be solved by replaying
the whole prefix, peeking at tomorrow, and then committing the buy:

    for each day:  run(prefix + [sell, "buy 1 acid", "next day"])
                   -> today's cash + today's prices + tomorrow's prices
                   pick the drug with the best tomorrow/today ratio
                   spend everything on it, capped below the 32-bit wrap

The cap matters: ADRIFT keeps a variable in a signed 32-bit Long, and a greedy
compounding run sails past 2^31 with days to spare -- which is how the old route
came to "finish with $-141777" and still be congratulated.  CASH_CAP bounds the
whole trajectory: each day's buy is sized so that tomorrow's sale lands under it,
since the cash left idle beside a position is carried along with it.

Usage:  python3 harness/druggy_lane_solve.py > goldens/druggy_lane_solution.txt
"""

import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCARE = os.path.join(ROOT, "harness", "scare")
GAME = os.path.join(ROOT, "games", "druggy_lane.taf")

DRUGS = ["acid", "coke", "crack", "hash", "weed", "speed"]
CASH_CAP = 2_000_000_000       # stay clear of the signed 32-bit wrap
DAYS = 30

PRICE_RE = re.compile(r"^([A-Z]+) - (\d+) - \$(-?\d+) - \$(-?\d+)$")
CASH_RE = re.compile(r"^CASH = \$(-?\d+)$")
DEBT_RE = re.compile(r"^DEBT = \$(-?\d+)$")


def run(cmds):
    """Replay `cmds` from a fresh start and return the per-command blocks."""
    script = "\n".join(cmds + ["quit", "y"]) + "\n"
    env = dict(os.environ, LC_ALL="C", SCR_ECHO_INPUT="1", SCR_SKIP_WAITKEY="1")
    out = subprocess.run([SCARE, GAME], input=script.encode(), env=env,
                         stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                         timeout=120).stdout.decode("latin-1")
    blocks, cur = [], []
    for line in out.replace("\r", "").split("\n"):
        if line.startswith("> "):
            blocks.append(cur)
            cur = []
        else:
            cur.append(line.rstrip())
    blocks.append(cur)
    # blocks[0] is the pre-first-command banner; drop it and the quit/y tail so
    # blocks[i] is the output of cmds[i].
    return blocks[1:1 + len(cmds)]


def state(block):
    """Prices, cash and debt as last printed inside one command's output."""
    prices, cash, debt = {}, None, None
    for line in block:
        m = PRICE_RE.match(line.strip())
        if m:
            prices[m.group(1).lower()] = int(m.group(3))
            continue
        m = CASH_RE.match(line.strip())
        if m:
            cash = int(m.group(1))
            continue
        m = DEBT_RE.match(line.strip())
        if m:
            debt = int(m.group(1))
    return prices, cash, debt


def die(msg):
    sys.stderr.write("druggy_lane_solve: %s\n" % msg)
    sys.exit(1)


def main():
    route, held = [], None       # held = (qty, drug)

    for day in range(DAYS, 1, -1):
        opening = "look" if held is None else "sell %d %s" % held
        blocks = run(route + [opening, "buy 1 acid", "next day"])
        # -3 = the sell/look, -2 = the probe buy, -1 = next day (tomorrow).  The
        # trailing quit/y blocks are already stripped by run().  Only `look` and
        # `next day` redraw the market; a `sell` just prints its receipt, so on
        # every day but the first, today's board is the previous night's block
        # (-4) and the sale has to be added to the cash it shows.
        if held is None:
            today, cash, _ = state(blocks[-3])
        else:
            today, cash, _ = state(blocks[-4])
            if today and cash is not None:
                cash += held[0] * today[held[1]]
        tomorrow, _, _ = state(blocks[-1])
        if cash is None or not today or not tomorrow:
            die("day %d: could not read the market" % day)

        drug = max(DRUGS, key=lambda d: tomorrow[d] / today[d])
        if tomorrow[drug] <= today[drug]:
            die("day %d: every drug falls tomorrow" % day)
        # Buy as much as the day's cash allows, but never so much that
        # tomorrow's sale would push the pile over CASH_CAP: the cap has to
        # bound the whole trajectory, not just one position, because the cash
        # left idle beside the position is carried along with it.
        qty = cash // today[drug]
        room = CASH_CAP - cash
        gain = tomorrow[drug] - today[drug]
        if gain > 0:
            qty = min(qty, max(1, room // gain))
        if qty < 1:
            die("day %d: cannot afford a single %s" % (day, drug))

        route += [opening, "buy %d %s" % (qty, drug), "next day"]
        held = (qty, drug)

    # One day left: liquidate, clear the debt, and let the clock run out.
    blocks = run(route)          # the board on the last trading day
    _, _, debt = state(blocks[-1])
    if debt is None:
        die("final day: could not read the debt")
    route += ["sell %d %s" % held, "pay back %d" % debt, "next day"]

    # The three intro "[Press any key]" pauses eat the first three lines of the
    # solution file, so the route has to be fed three sacrificial blanks.  The
    # probe runs above skip the pauses (SCR_SKIP_WAITKEY) and must not.
    print("\n".join(["", "", ""] + route))


if __name__ == "__main__":
    main()
