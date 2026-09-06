#!/usr/bin/env python3
"""Re-derive the House Of Horror route for one seed.

The Porch door is a trap for walkthroughs: T0 `* open * door *` ends with
twenty-one `ACT type=0 ... v2=1` actions, each moving one object to a *random
room of a room group*.  Every key, tool and treasure in the game is therefore
somewhere different in every RNG thread, and a hand-written route survives
exactly as long as the thread it was derived on.  Two more things are rolled
per turn on top of that: the ghost's walk (T48 `GHOST MOVING STUFF`, rep=0,
scatters everything you are carrying the first time you share a room with it)
and the zombie's (T95 `ZOMBIE ATTACK`, which costs one of your four health
points whenever it catches up with you and VAR 4 `luck` comes up 1).

So this is a closed-loop solver rather than a route: it replays the game from
the start after every command, reads the world out of the trace stream
(SCR_TRACE_OBJ for object positions, SCR_TRACE_JUDY for the ghost and the
zombie, SCR_TRACE_VARS for health) and picks the next command from what is
actually true, not from what was true when the route was written.  The puzzle
chain is a dependency list, not a sequence, so it re-orders itself around
wherever the objects landed.

Two fixed openings are forced and both are load-bearing:

  * `drop torch` on the Porch, then chase the ghost empty-handed until "A
    strange chill fills the air."  T48 is rep=0, so meeting the ghost with
    nothing in hand spends the robbery on an empty inventory.  It has to be
    the Porch (or another outdoor room): rooms 1-28 and 30 are dark without
    the torch, and a dark room hides its objects from `take` -- drop the torch
    indoors and you can never pick it up again.

  * shoot the zombie as early as the blunderbuss allows.  It does not stop the
    walk (NPC 6 has stopTask=0, so the corpse keeps stumbling into you and
    T95 keeps rolling), but it is +20 and it has to happen somewhere, and
    firing in room 11 -- or in 13 while the rats live, or 27 while the bats
    do -- burns the shot on T70/T76/T77/T78 instead.

Not every seed is solvable: the scatter can drop the whistle in the Attic
behind the bats it is needed to kill, or the last treasure in a room whose
guardian is already dead-locked, and roughly three seeds in four end in
`stuck with [...]` or a fourth zombie punch.  Sweep for one that works.

Usage, from test/adrift4/:
    python3 harness/hhorror_solve.py 1 > goldens/hhorror_solution.txt

Everything is resolved relative to this file and the dump is regenerated on
each run, so there is no scratch state to keep in sync.
"""
import collections
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SCARE = os.path.join(HERE, "scare")
GAME = os.path.join(ROOT, "games", "hhorror.taf")

KIT = 65                       # the first aid kit: T124 `use kit` resets health
GHOST, ZOMBIE = 4, 6           # NPC indices

# Exits the EXIT table itself gates, plus the four it does not: T35/T32/T33/T34
# answer *any* command in the Conservatory, Toy Room, Bathroom and Attic while
# their guardian lives, and the Wine Cellar door needs the iron key AND dead
# rats.  None = never opened by this route (the Study is empty, the Hole needs
# a shovel and is empty too).
GATE = {(0, "N"): "frontdoor", (8, "N"): None, (13, "IN"): None,
        (30, "OUT"): None, (13, "S"): "lab", (11, "N"): "plant",
        (21, "U"): "hatch", (26, "N"): "window", (34, "S"): "window",
        (27, "S"): "wall", (19, "E"): "monster", (23, "S"): "monster"}
# T32-T35 answer every `take` in these rooms with a mauling (-1 health) until
# their guardian is dead, so an object lying in one is out of reach until then.
BLOCKED = {11: "plant", 17: "molotov", 20: "monster", 27: "bats"}
TREASURE = {3: "diamond ring", 4: "candlestick", 5: "necklace", 6: "deed",
            7: "money", 8: "emerald ring", 9: "braclet", 10: "gold bar",
            11: "doubloons"}
WORD = {"N": "n", "E": "e", "S": "s", "W": "w", "U": "u", "D": "d",
        "IN": "in", "OUT": "out"}


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


GRAPH = exit_graph()


def path(src, dst, flags, avoid=()):
    """Shortest walk over the exits `flags` has opened, as typed directions."""
    if src == dst:
        return []
    queue, seen = collections.deque([(src, [])]), {src}
    while queue:
        room, walk = queue.popleft()
        for direction, dest in GRAPH[room].items():
            gate = GATE.get((room, direction), "")
            if gate is None or (gate and gate not in flags) or dest in seen:
                continue
            if dest in avoid and dest != dst:
                continue
            if dest == dst:
                return walk + [WORD[direction]]
            seen.add(dest)
            queue.append((dest, walk + [WORD[direction]]))
    return None


def play(seed, cmds):
    """Play `cmds` from the start; return the world as of the last turn."""
    env = dict(os.environ, SCR_SEED=str(seed), LC_ALL="C", SCR_SKIP_WAITKEY="1",
               SCR_ECHO_INPUT="1", SCR_TRACE_PLAYER="1", SCR_TRACE_JUDY="1",
               SCR_TRACE_OBJ="all", SCR_TRACE_VARS="health")
    proc = subprocess.run(
        [SCARE, GAME], input=("\n".join(cmds) + "\nquit\ny\n").encode("latin-1"),
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
    room, health, npc, pos, health_log = -1, 0, {}, {}, []
    for line in proc.stderr.decode("latin-1").splitlines():
        m = re.match(r"PLAYERROOM room=(-?\d+)", line)
        if m:
            room, npc, pos = int(m.group(1)), {}, {}
            continue
        m = re.match(r"OBJTRACE obj=(\d+) pos=(-?\d+)", line)
        if m:
            pos[int(m.group(1))] = int(m.group(2))
            continue
        m = re.match(r"VARTRACE health=(-?\d+)", line)
        if m:
            health = int(m.group(1))
            health_log.append(health)
            continue
        m = re.match(r"JUDYTRACE npc=(\d+) room=(-?\d+)", line)
        if m:
            npc[int(m.group(1))] = int(m.group(2))
    if not pos:
        raise SystemExit("no trace: is `scare` built with -DSCARIER_DUMP_TOOLS?")
    text = proc.stdout.decode("latin-1").replace("\r", "")
    return {"room": room, "health": health, "log": health_log, "npc": npc,
            "pos": pos, "text": text, "turns": text.split("\n> ")}


SEED = int(sys.argv[1]) if len(sys.argv) > 1 else 1
route, flags, done, state = [], set(), set(), None
kit_used = False


def sync():
    global state
    state = play(SEED, route)
    return state


def die(message):
    log = state["log"]
    hits = [(i, route[i] if i < len(route) else "?")
            for i in range(1, len(log)) if log[i] < log[i - 1]]
    sys.stderr.write("health %d -> %d, hits %s\n" % (log[0], log[-1], hits))
    sys.stderr.write("\n".join(route[-12:]) + "\n")
    sys.stderr.write(state["turns"][-2][-600:] + "\n")
    raise SystemExit("hhorror: " + message)


def guard():
    """T124 resets health to 4, once.  Spend it before the next punch lands."""
    global kit_used
    if kit_used or state["health"] > 2 or state["pos"][KIT] != 0:
        return
    kit_used = True
    route.append("use kit")
    sync()


def emit(cmds):
    route.extend(cmds)
    sync()
    if "I'm afraid you are dead!" in state["text"]:
        die("killed on `%s`" % cmds[-1])
    guard()
    return state


def act(cmds, want=None):
    emit(cmds)
    if want and want not in state["turns"][-2]:
        die("%r did not print %r" % (cmds[-1], want))


def goto(room):
    """Walk to `room` a step at a time, stepping around the zombie if we can.

    The zombie's walk keeps running after it is shot, and T95 costs a health
    point every time it catches up, so the cheapest defence is simply not to
    walk into the room it is standing in.
    """
    for _ in range(60):
        if state["room"] == room:
            return
        here = state["room"]
        steps = (path(here, room, flags, avoid=(state["npc"][ZOMBIE],))
                 or path(here, room, flags))
        if steps is None:
            die("no path %d -> %d (flags %s)" % (here, room, sorted(flags)))
        emit(steps[:1])
    die("walk to %d never arrived (stuck in %d)" % (room, state["room"]))


def fetch(obj, phrase):
    """Walk to wherever `obj` is lying and pick it up.

    Re-read after arriving: the ghost and our own detours can both have moved
    it -- or spent it, in the first aid kit's case -- while we were on the way.
    """
    for _ in range(4):
        if state["pos"][obj] == 0 or (obj == KIT and kit_used):
            return
        if state["pos"][obj] < 1:
            die("object %d is not in a room (pos %d)" % (obj, state["pos"][obj]))
        goto(state["pos"][obj] - 1)
        if state["pos"][obj] == state["room"] + 1:
            emit(["take " + phrase])
    die("take %s left it at pos %d" % (phrase, state["pos"][obj]))


def grab(obj, phrase):
    """Take an object that is in or on something here, not lying on the floor."""
    emit(["take " + phrase])
    if state["pos"][obj] != 0:
        die("take %s left it at pos %d" % (phrase, state["pos"][obj]))


def takeable(obj):
    """Is `obj` lying in a room we can walk to and pick things up in?"""
    where = state["pos"][obj]
    if where == 0:
        return True
    if where < 1:
        return False
    room = where - 1
    if BLOCKED.get(room) is not None and BLOCKED[room] not in done:
        return False
    return path(state["room"], room, flags) is not None


def sweep(objects):
    """Fetch `objects` nearest-first, so the tour is not a random walk."""
    left = dict(objects)
    while left:
        reach = {o: path(state["room"], state["pos"][o] - 1, flags)
                 for o in left if state["pos"][o] >= 1}
        reach = {o: w for o, w in reach.items() if w is not None}
        if not reach:
            die("cannot reach %s" % sorted(left))
        best = min(reach, key=lambda o: len(reach[o]))
        fetch(best, left.pop(best))


def flush_ghost():
    """Spend T48's one robbery on an empty inventory."""
    act(["n"])
    act(["open door"], "You open the door.")
    flags.add("frontdoor")
    act(["drop torch"])
    for _ in range(120):
        if "A strange chill fills the air." in state["text"]:
            break
        ghost = state["npc"][GHOST]
        steps = path(state["room"], ghost, flags) if ghost >= 0 else None
        emit(steps[:1] if steps else ["wait"])
    else:
        die("the ghost never robbed us")
    goto(0)
    act(["take torch"])


def do_zombie():
    goto(3)
    act(["say live"])                   # the blunderbuss appears in the fireplace
    act(["take blunderbuss"])
    goto(16)                            # the snooker ball is on the table
    grab(46, "ball")
    act(["put ball in blunderbuss"])
    for _ in range(120):
        here, there = state["room"], state["npc"][ZOMBIE]
        if here == there and here not in (11, 13, 27):
            break
        steps = None if there in (11, 13, 27) else path(here, there, flags)
        emit(steps[:1] if steps else ["wait"])
    else:
        die("never met the zombie outside rooms 11/13/27")
    act(["fire blunderbuss"])
    act(["drop blunderbuss"])
    grab(11, "doubloons")


def do_cheese():
    fetch(20, "poison")
    fetch(19, "cheese")
    act(["poison cheese"])              # also yields the empty bottle


def do_water():
    goto(6)
    act(["turn tap"])
    act(["fill bottle"])


def do_bless():
    fetch(24, "bible")
    act(["bless water"])
    act(["drop bible"])


def do_rats():
    fetch(2, "iron key")
    goto(13)
    act(["give poisoned cheese to rats"])
    act(["unlock door"])
    flags.add("lab")
    act(["drop iron key"])


def do_plant():
    fetch(39, "weedkiller")
    goto(11)
    act(["pour weedkiller on plant"])
    flags.add("plant")


def do_hatch():
    fetch(12, "hook")
    goto(21)
    act(["open hatch"])
    flags.add("hatch")
    act(["drop hook"])


def do_bats():
    fetch(36, "whistle")
    goto(27)
    act(["blow whistle"])
    act(["drop whistle"])


def do_wall():
    fetch(13, "hammer")
    goto(27)
    act(["break wall"])
    flags.add("wall")
    act(["drop hammer"])


def do_skeleton():
    goto(28)
    act(["pour holy water on skeleton"])
    act(["drop holy water"])


def do_molotov():
    fetch(29, "dish cloth")
    fetch(28, "vodka")
    act(["use vodka with cloth"])
    goto(9)
    act(["light molotov"])              # EVENT 1: eight turns before it kills you
    goto(17)
    act(["throw molotov"])


def do_monster():
    fetch(31, "bleach")
    goto(19)
    act(["pour bleach on monster"])
    flags.add("monster")


def do_toilet():
    fetch(15, "plunger")
    goto(20)
    act(["unblock toilet"])
    act(["drop plunger"])
    grab(10, "gold bar")


# Each step fetches what it needs and drops it again the moment it is spent:
# the endgame inventory has to be nine treasures plus the torch, and capacity
# is exactly ten.  `ready` decides the order, so the chain re-shuffles itself
# around wherever this seed's scatter put things.
PLAN = [
    ("kit", lambda: state["pos"][KIT] < 1 or takeable(KIT),
     lambda: takeable(KIT) and fetch(KIT, "kit")),
    ("zombie", lambda: True, do_zombie),
    ("cheese", lambda: takeable(20) and takeable(19), do_cheese),
    ("water", lambda: "cheese" in done, do_water),
    ("bless", lambda: "water" in done and takeable(24), do_bless),
    ("rats", lambda: "cheese" in done and takeable(2), do_rats),
    ("plant", lambda: takeable(39), do_plant),
    ("hatch", lambda: takeable(12), do_hatch),
    ("bats", lambda: takeable(36) and "hatch" in flags, do_bats),
    ("wall", lambda: takeable(13) and "bats" in done, do_wall),
    ("skeleton", lambda: "bless" in done and "wall" in flags, do_skeleton),
    ("molotov", lambda: takeable(29) and takeable(28), do_molotov),
    ("monster", lambda: takeable(31), do_monster),
    ("toilet", lambda: "monster" in done and takeable(15), do_toilet),
]


def main():
    flush_ghost()
    while len(done) < len(PLAN):
        for name, ready, run in PLAN:
            if name not in done and ready():
                run()
                done.add(name)
                break
        else:
            die("stuck with %s left" % [n for n, _, _ in PLAN if n not in done])
    sweep({o: n for o, n in TREASURE.items() if state["pos"][o] != 0})
    goto(29)
    act(["drive"])
    act(["wait"])                       # the nine scoring tasks fire here
    print("\n".join(route))


if __name__ == "__main__":
    main()
