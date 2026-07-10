# Adaptive Spectre-banish solver for The Spectre of Castle Coris.
#
# This is the tool that converged test/SpectreOfCastleCoris_walkthrough.txt to
# its full maximum-score win (700/700).  The Spectre is a repeating turn-based
# killer: when it materialises, MOVEMENT IS SILENTLY EATEN ("your feet are
# rooted to the spot") until it is banished by "read prayer" WITH THE PRAYER
# BOOK HELD IN HAND (book-in-worn-bag fails; plain "say prayer" is hijacked by
# "say <x> to <NPC>" near characters).  Un-banished encounters kill a few turns
# later.  Because every command edit shifts the (deterministic) timer, the
# cadence cannot be hand-authored -- it must be re-converged after any change.
#
# Policy per iteration (first problem found, then re-run):
#   * The materialisation block is located; if the next command does not banish,
#     a banish sequence is inserted right after it, chosen by the book's state
#     (tracked from the transcript text): held -> [read prayer]; in the bag ->
#     [get book, read prayer, put book in bag]; deliberately dropped (the game
#     force-drops it for every two-handed action: get wood, climb rope, fish...)
#     -> [get book, read prayer, drop book].  Restoring the prior state is what
#     keeps downstream two-handed actions working.
#   * A spectre death with no catchable materialisation is fixed by inserting
#     the banish just before the death.
#   * If the book was left in a DIFFERENT room when the Spectre appears, that
#     cannot be auto-fixed (report and stop): re-order the script so the book
#     always travels, or add "z" padding so the timer fires before the bookless
#     window (that is how the fishpond bait sequence is protected).
#
# Usage:  python3 test/spectre_prayer_solver.py <command-list.txt> [budget-secs]
#   (run from terps/scarier; needs test/a5run_dump and the game blorb; writes
#    the converged list to solved_<status>.txt next to the input.)
#
import subprocess, sys, re, os, time

SCARIER = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = f"{SCARIER}/test/adrift5-games/TheSpectreOfCastleCoris.blorb"
BIN = f"{SCARIER}/test/a5run_dump"

SPECTRE = re.compile(r"The Spectre is materialising in front of you")
BANISH = re.compile(r"apparition wails|mist fades from view|mist swirls around you then dissipates|You recite the words|You say out loud the words")
DEATH = re.compile(r"sibilant laughter|You have died|YOU HAVE DIED", re.I)
WON = re.compile(r"YOU HAVE WON|CONGRATULATIONS", re.I)
BOOK_HELD = re.compile(r"Ok, you pick up the prayer book|Ok, you take the prayer book from the bag|already carrying the prayer book|find a small book, which you pick up")
BOOK_DOWN = re.compile(r"you put down [^.\n]*prayer book|Ok, you put down the prayer book")
BOOK_BAG = re.compile(r"OK, you put the prayer book inside the bag", re.I)

# Room-name set for left-behind detection (best effort: skipped if a5dump or
# python XML parsing is unavailable).
def room_names():
    try:
        import xml.etree.ElementTree as ET
        dump = subprocess.run([f"{SCARIER}/test/a5dump", GAME], capture_output=True, text=True, timeout=60)
        root = ET.fromstring(dump.stdout)
        names = set()
        for loc in root.findall("Location"):
            n = (loc.findtext("ShortDescription/Description/Text") or "").strip()
            if n and not n.startswith("<"):
                names.add(n)
        return names
    except Exception:
        return set()

def run(cmds):
    inp = "o\nb\n" + "\n".join(cmds) + "\n"
    p = subprocess.run([BIN, GAME, "/dev/stdin"], input=inp, capture_output=True, text=True, timeout=300)
    return p.stdout

def blocks_of(out):
    parts = re.split(r"(?m)^> (.*)$", out)
    return [(parts[i].strip(), parts[i + 1] if i + 1 < len(parts) else "") for i in range(1, len(parts), 2)]

def load(f):
    cmds = [l.rstrip("\n") for l in open(f)]
    cmds = [c for c in cmds if c.strip() and not c.startswith("#")]
    if cmds[:2] == ["o", "b"]: cmds = cmds[2:]
    return cmds

def insertion(state):
    if state == "held": return ["read prayer"]
    if state == "down": return ["get book", "read prayer", "drop book"]
    if state == "bag":  return ["get book", "read prayer", "put book in bag"]
    return ["get book", "read prayer"]

def solve(cmds, budget=300, rooms=None):
    rooms = rooms if rooms is not None else room_names()
    t0 = time.time(); hist = {}
    for it in range(600):
        if time.time() - t0 > budget:
            print(f"TIME BUDGET after {it} iters"); return cmds, "budget"
        out = run(cmds)
        if WON.search(out):
            print(f"*** WON *** after {it} edits, {len(cmds)} cmds"); return cmds, "won"
        bl = blocks_of(out)
        state = "none"; room = "?"; bookroom = None; problem = None
        for bi in range(2, len(bl)):
            cmd, txt = bl[bi]
            ci = bi - 2
            for line in txt.splitlines():
                if line.strip() in rooms: room = line.strip()
            if BOOK_HELD.search(txt): state = "held"; bookroom = None
            if BOOK_DOWN.search(txt): state = "down"; bookroom = room
            if BOOK_BAG.search(txt): state = "bag"; bookroom = None
            if SPECTRE.search(txt):
                nxt = bl[bi + 1][1] if bi + 1 < len(bl) else ""
                nxt2 = bl[bi + 2][1] if bi + 2 < len(bl) else ""
                nxt3 = bl[bi + 3][1] if bi + 3 < len(bl) else ""
                if BANISH.search(txt): continue
                if state == "held" and BANISH.search(nxt): continue
                if state != "held" and (BANISH.search(nxt) or BANISH.search(nxt2) or BANISH.search(nxt3)): continue
                key = ("S", ci); hist[key] = hist.get(key, 0) + 1
                if hist[key] > 8:
                    print(f"STUCK spectre at cmd#{ci} '{cmds[ci] if ci < len(cmds) else '?'}'")
                    print("ctx:", cmds[max(0, ci - 4):ci + 5]); print(txt[-300:])
                    return cmds, "stuck"
                if state == "down" and bookroom is not None and bookroom != room:
                    print(f"BOOK LEFT BEHIND: spectre@{ci} in {room!r} but book in {bookroom!r}")
                    print("ctx:", cmds[max(0, ci - 6):ci + 3])
                    return cmds, "bookaway"
                ins = insertion(state)
                cmds[ci + 1:ci + 1] = ins
                problem = f"iter{it}: spectre@{ci} ({cmds[ci]}) state={state} -> insert {ins}"
                break
            if DEATH.search(txt):
                key = ("D", ci); hist[key] = hist.get(key, 0) + 1
                if hist[key] > 8:
                    print(f"STUCK death at cmd#{ci}"); print("ctx:", cmds[max(0, ci - 6):ci + 2]); print(txt[:300])
                    return cmds, "stuck"
                ins = insertion(state)
                cmds[ci:ci] = ins
                problem = f"iter{it}: death@{ci} -> insert {ins} before"
                break
        if problem:
            print(problem); continue
        print(f"clean after {it} iters, {len(cmds)} cmds (no unbanished spectre, no death)")
        return cmds, "clean"
    return cmds, "loop"

if __name__ == "__main__":
    cmds = load(sys.argv[1])
    budget = int(sys.argv[2]) if len(sys.argv) > 2 else 300
    cmds, status = solve(cmds, budget)
    outf = os.path.join(os.path.dirname(os.path.abspath(sys.argv[1])) or ".", f"solved_{status}.txt")
    open(outf, "w").write("\n".join(cmds) + "\n")
    print("wrote", outf)
