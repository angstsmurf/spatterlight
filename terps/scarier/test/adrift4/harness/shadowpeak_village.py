"""Re-derive the Shadowpeak village zombie phase (Holga/Jarris/Boris/Arthur).

Greedy, one turn at a time from the Village Square: attack whichever named
zombie is standing in the player's room, otherwise wait.  Stops as soon as all
four death messages have been printed (+40 score).  Same shape as
shadowpeak_chase.py, and needed for the same reason: the four zombies wander,
so any fixed block of attack-pairs is only valid for one RNG thread.

Usage, from test/adrift4/:
    python3 harness/shadowpeak_village.py shadowpeak_killwraith_solution 180

It prints the block to splice in where the `attack holga` run starts.  Pad it
back out to the original block length with `z` so the downstream event timers
(EVENT 92, the Morac death timer) keep landing where the rest of the route
expects them.
"""
import os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SCARE = os.path.join(HERE, "scare")
GAME = os.path.join(ROOT, "games", "Shadowpeak.taf")
# npc index -> (command noun, death message)
ZOMBIES = {3: ("holga", "Holga keels over"),
           4: ("jarris", "Jarris slumps to the floor"),
           5: ("boris", "Boris falls down, quite dead"),
           6: ("arthur", "Arthur falls face down")}

def live_lines(solution):
    src = open(os.path.join(ROOT, "goldens", solution + ".txt"), encoding="latin-1").read()
    return [l for l in src.split("\n") if l.strip() and not l.lstrip().startswith("#")]

def play(seed, cmds):
    env = dict(os.environ, SCR_SEED=str(seed), LC_ALL="C",
               SCR_TRACE_PLAYER="1", SCR_TRACE_JUDY="1")
    p = subprocess.run([SCARE, GAME],
                       input=("\n".join(cmds) + "\nquit\ny\n").encode("latin-1"),
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env, timeout=600)
    out, err = p.stdout.decode("latin-1"), p.stderr.decode("latin-1")
    room = int(re.findall(r"PLAYERROOM room=(-?\d+)", err)[-1])
    # last turn's block of JUDYTRACE lines
    here = {}
    for m in re.finditer(r"JUDYTRACE npc=(\d+) room=(-?\d+)", err):
        here[int(m.group(1))] = int(m.group(2))
    dead = {n for n, (_, msg) in ZOMBIES.items() if msg in out}
    return room, here, dead, "I'm afraid you are dead!" in out

def derive(seed, prefix, limit=160):
    block = []
    for _ in range(limit):
        room, here, dead, killed = play(seed, prefix + block)
        if killed:
            return None, None
        if len(dead) == 4:
            return block, len(block)
        target = next((n for n in ZOMBIES if n not in dead and here.get(n) == room), None)
        block.append("attack " + ZOMBIES[target][0] if target else "z")
    return None, None

def block_start(live):
    """First turn of the village phase: the first wait-or-villager-attack after
    the `fly` that drops the player into the village."""
    fly = max(i for i, l in enumerate(live) if l == "fly")
    names = tuple("attack " + z[0] for z in ZOMBIES.values())
    return next(i for i, l in enumerate(live[fly:], fly)
                if l == "z" or l.startswith(names))


def main():
    solution, seed = sys.argv[1], sys.argv[2]
    live = live_lines(solution)
    block, n = derive(seed, live[:block_start(live)])
    if block is None:
        raise SystemExit("village phase did not finish at seed %s" % seed)
    print("# %d turns, all four zombies down" % n)
    print("\n".join(block))

if __name__ == "__main__":
    main()
