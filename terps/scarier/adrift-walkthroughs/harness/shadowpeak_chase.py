#!/usr/bin/env python3
"""Re-derive the Shadowpeak Damastus (npc=35) maze chase for the fixed route.

Greedy pursuit from the stone button (room 157): attack when co-located,
else take one BFS step toward Damastus over the maze exit graph. Ends by
walking to room 151 (bronze button) once he is dead.
Mirrors the original /tmp/chase.py referenced in the solution comments.
"""
import subprocess, re, collections, sys

SCARE = "/Users/administrator/spatterlight/terps/scarier/adrift-walkthroughs/harness/scare"
GAME = "/Users/administrator/adrift-battle/games/Shadowpeak.taf"
SCRATCH = "/private/tmp/claude-502/-Users-administrator/3776f75b-21b4-49b8-8ed8-1d457ccadfb5/scratchpad"
BASE = f"{SCRATCH}/ag2_m17.txt"
DUMP = f"{SCRATCH}/dump.txt"
NPC = 35
BRONZE_ROOM = 151

# --- exit graph ---
g = collections.defaultdict(dict)  # room -> dir -> dest
for line in open(DUMP, encoding="latin-1"):
    m = re.match(r"EXIT room=(\d+) (\w+) -> dest=(\d+) gateTask=(-?\d+)", line)
    if m:
        r, d, dest = int(m.group(1)), m.group(2), int(m.group(3))
        g[r][d.lower()] = dest

def bfs_step(src, dst):
    """First typed direction on a shortest path src->dst (maze not rotated)."""
    if src == dst:
        return None
    q = collections.deque([src])
    prev = {src: None}
    while q:
        cur = q.popleft()
        for d, nxt in g[cur].items():
            if nxt not in prev:
                prev[nxt] = (cur, d)
                if nxt == dst:
                    # walk back to first step
                    node = dst
                    while prev[node][0] != src:
                        node = prev[node][0]
                    return prev[node][1]
                q.append(nxt)
    return None

# --- prefix: base file up to and including "press stone button" ---
lines = open(BASE, encoding="latin-1").read().splitlines()
stone = lines.index("press stone button")
prefix = lines[: stone + 1]

def run(cmds):
    inp = "\n".join(cmds + ["quit", "y"]) + "\n"
    p = subprocess.run([SCARE, GAME], input=inp.encode("latin-1"),
                       capture_output=True,
                       env={"SCR_TRACE_JUDY": "1", "SCR_TRACE_PLAYER": "1"})
    return p.stdout.decode("latin-1"), p.stderr.decode("latin-1")

def last_state(err):
    """(player_room, damastus_room) after the final command."""
    pr = dr = None
    for line in err.splitlines():
        m = re.match(r"PLAYERROOM room=(-?\d+)", line)
        if m:
            pr = int(m.group(1))
        m = re.match(rf"JUDYTRACE npc={NPC} room=(-?\d+)", line)
        if m:
            dr = int(m.group(1))
    return pr, dr

chase = []
for step in range(40):
    out, err = run(prefix + chase)
    pr, dr = last_state(err)
    dead = dr is not None and dr < 0
    tail = [l for l in out.splitlines() if l.strip()][-3:]
    print(f"step {step}: player={pr} damastus={dr} dead={dead} chase={chase[-1] if chase else '-'}")
    if dead:
        print("Damastus dead. Kill text:")
        for l in tail:
            print("   ", l)
        break
    if pr == dr:
        chase.append("attack damastus")
    else:
        d = bfs_step(pr, dr)
        if d is None:
            print("no path!", pr, dr); sys.exit(1)
        chase.append(d)
else:
    print("FAILED: no kill in 40 steps"); sys.exit(1)

# walk to the bronze button room
out, err = run(prefix + chase)
pr, _ = last_state(err)
to_bronze = []
while pr != BRONZE_ROOM:
    d = bfs_step(pr, BRONZE_ROOM)
    to_bronze.append(d)
    # simulate movement over graph (no NPC threat once dead)
    pr = g[pr][d]
print("CHASE:", chase)
print("TO_BRONZE:", to_bronze)
open(f"{SCRATCH}/chase_result.txt", "w").write("\n".join(chase + to_bronze) + "\n")
