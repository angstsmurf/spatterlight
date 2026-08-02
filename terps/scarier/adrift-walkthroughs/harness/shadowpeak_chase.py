#!/usr/bin/env python3
"""Re-derive the Shadowpeak Damastus (npc=35) maze chase for a fixed route.

Greedy pursuit from the stone button (room 157): attack when co-located, else
take one BFS step toward Damastus over the maze exit graph.  Ends by walking to
room 151 (the bronze button) once he is dead.

The chase is the first thing that breaks whenever an engine change re-threads
the walkers' RNG -- the 2026-07-02 tick-order fix, the 2026-08-01 walk-timing
fix, the 2026-08-02 load-time immediate-event start.  Each time the repair is
the same two steps: find a seed whose *upstream* route still runs clean (no new
command failures before the chase), then re-derive the chase under that seed
with this script.

Usage, from adrift-walkthroughs/:
    python3 harness/shadowpeak_chase.py shadowpeak_solution 102

It prints the command block to splice between `press stone button` and
`press bronze button` in harness/<solution>.txt.  Everything is resolved
relative to this file, and the dump is regenerated on each run, so there is no
scratch state to keep in sync.
"""
import collections
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SCARE = os.path.join(HERE, "scare")
GAME = os.path.join(ROOT, "games", "Shadowpeak.taf")
NPC = 35
BRONZE_ROOM = 151
# The maze is rooms 139-165, and its directions are NOT rotated (typed dir ==
# map slot), unlike the rest of the game -- so a plain BFS over EXIT lines is
# enough here and nowhere else.
MAZE = set(range(139, 166))
WORD = {"N": "n", "E": "e", "S": "s", "W": "w", "NE": "ne", "SE": "se",
        "NW": "nw", "SW": "sw", "U": "u", "D": "d", "IN": "in", "OUT": "out"}


def exit_graph():
    """room -> {DIR: dest}, from a fresh SCR_DUMP_TASKS dump."""
    env = dict(os.environ, SCR_DUMP_TASKS="1", LC_ALL="C")
    proc = subprocess.run([SCARE, GAME], input=b"quit\ny\n",
                          stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                          env=env)
    graph = collections.defaultdict(dict)
    for line in proc.stderr.decode("latin-1").splitlines():
        m = re.match(r"EXIT room=(\d+) (\w+) -> dest=(-?\d+)", line)
        if m:
            graph[int(m.group(1))][m.group(2)] = int(m.group(3))
    if not graph:
        raise SystemExit("no EXIT lines: is `scare` built with -DSCARIER_DUMP_TOOLS?")
    return graph


def bfs(graph, src, dst, first_only):
    """Shortest maze path src->dst: its first step, or the whole direction list."""
    if src == dst:
        return None if first_only else []
    queue = collections.deque([(src, [])])
    seen = {src}
    while queue:
        room, path = queue.popleft()
        for direction, dest in graph[room].items():
            if dest not in MAZE or dest in seen:
                continue
            if dest == dst:
                return (path + [direction])[0] if first_only else path + [direction]
            seen.add(dest)
            queue.append((dest, path + [direction]))
    return None


def play(seed, cmds):
    """Play `cmds`; return (player room, Damastus room, stamina) at the end."""
    env = dict(os.environ, SCR_LEGACY_RANDMAP="1", SCR_SEED=str(seed),
               LC_ALL="C", SCR_TRACE_PLAYER="1", SCR_TRACE_JUDY="1")
    proc = subprocess.run(
        [SCARE, GAME],
        input=("\n".join(cmds) + "\nquit\ny\n").encode("latin-1"),
        stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, env=env)
    err = proc.stderr.decode("latin-1")
    rooms = [int(m) for m in re.findall(r"PLAYERROOM room=(-?\d+)", err)]
    stamina = [int(m) for m in
               re.findall(r"PLAYERROOM room=-?\d+ stamina=(-?\d+)", err)]
    damastus = [int(m) for m in
                re.findall(r"JUDYTRACE npc=%d room=(-?\d+)" % NPC, err)]
    if not rooms or not damastus:
        raise SystemExit("no trace: is `scare` built with -DSCARIER_DUMP_TOOLS?")
    return rooms[-1], damastus[-1], stamina[-1]


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    solution, seed = sys.argv[1], sys.argv[2]
    src = open(os.path.join(HERE, solution + ".txt"), encoding="latin-1").read()
    live = [l for l in src.split("\n")
            if l.strip() and not l.lstrip().startswith("#")]
    prefix = live[:live.index("press stone button") + 1]

    graph = exit_graph()
    chase = []
    for _ in range(80):
        player, damastus, stamina = play(seed, prefix + chase)
        if damastus == -1:                      # hidden == dead
            tail = bfs(graph, player, BRONZE_ROOM, first_only=False)
            print("# Damastus dies in room %d, stamina %d" % (player, stamina))
            for command in chase + [WORD[d] for d in tail]:
                print(command)
            return
        if player == damastus:
            chase.append("attack damastus")
            continue
        step = bfs(graph, player, damastus, first_only=True)
        if step is None:
            raise SystemExit(
                "no maze path %d -> %d: the upstream route is off the rails at "
                "this seed, pick another" % (player, damastus))
        chase.append(WORD[step])
    raise SystemExit("gave up after 80 steps")


if __name__ == "__main__":
    main()
