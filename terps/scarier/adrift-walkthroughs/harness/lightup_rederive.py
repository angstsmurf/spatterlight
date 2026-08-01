#!/usr/bin/env python3
"""Re-derive light_up_solution.txt under the ported throw-drop mechanics
(2026-08-01: a landed method-5 throw drops the weapon in the room and, in
4.0, deals base-Strength-only damage).

Route facts this leans on (see Light_Up_walkthrough.md):
  - `attack <dead/absent enemy>` and a failed `take` are FREE no-turn parse
    errors, so fixed generous fight blocks are safe to bank.
  - `rest` is not understood by the game; survival is Defence + crew tanking.
  - The game re-gives the lighter at the Chapter 4 transition.
  - Death repair = insert `look` pads at a segment boundary to reshuffle the
    per-turn RNG (enemy target picks); localized by prefix simulation.

Run from terps/scarier/adrift-walkthroughs.  Deterministic under SCR_SEED=2.
"""
import subprocess, sys

SCARE = "harness/scare"
GAME = "games/light_up_4summer_comp.taf"

base = open("harness/light_up_solution.txt").read().splitlines()
# Chapters 1-3 up to the Cell 6 `take lighter`, then the six-attack Chip
# fight, the chapter-transition waitkeys, and `hard` (+15).
PRE = base[:206] + ["attack chip", "take lighter"] * 5 + ["attack chip"] \
      + ["", "", ""] + ["hard"]

def replay(cmds):
    inp = "\n".join(cmds + ["quit", "y"]) + "\n"
    p = subprocess.run([SCARE, GAME], input=inp.encode("latin-1"),
                       capture_output=True, env={"SCR_SEED": "2"})
    return p.stdout.decode("latin-1", "replace")

def fight(enemies, rounds=8, takes=("take lighter",)):
    block = []
    for _ in range(rounds):
        for e in enemies:
            block += ["attack " + e] + list(takes)
    return block

T2 = ("take double-lighter", "take lighter")

def seglist(pads, final_looks):
    segs = [
        ["north"] + fight(["ozgat"]),
        ["east"] + fight(["riven"]),
        ["north"] + fight(["ozgat", "riven"]),
        ["east", "take double-lighter"],
        ["west"] + fight(["ozgat", "riven"], takes=T2),
        ["west"] + fight(["riven"], takes=T2),
        ["north"],
        ["north"] + fight(["higher"], takes=T2),
        ["northeast"] + fight(["ozgat", "higher", "riven"], rounds=12,
                              takes=T2) + ["look"] * final_looks,
    ]
    return [["look"] * pads[i] + segs[i] for i in range(len(segs))]

def status(out):
    if "afraid you are dead" in out: return "dead"
    if "Waste Land" in out: return "teleported"
    return "alive"

pads = [0] * 9
final_looks = 60

for attempt in range(80):
    segs = seglist(pads, final_looks)
    out = replay(PRE + sum(segs, []))
    st = status(out)
    if st == "teleported":
        print(f"teleported (attempt {attempt}, pads={pads})")
        break
    if st == "dead":
        # Localize the fatal segment by prefix simulation, then pad before it.
        acc, idx_dead = [], None
        for i, seg in enumerate(segs):
            acc += seg
            st2 = status(replay(PRE + acc))
            if st2 != "alive":
                idx_dead = i if st2 == "dead" else None
                break
        if idx_dead is None:
            print(f"attempt {attempt}: death vanished in prefix scan; retrying")
            continue
        pads[idx_dead] += 1
        if pads[idx_dead] > 6:
            print(f"segment {idx_dead} unfixable with padding"); sys.exit(1)
        print(f"attempt {attempt}: died in segment {idx_dead}; pads={pads}")
        continue
    final_looks += 60
    print(f"attempt {attempt}: alive, no teleport; final_looks={final_looks}")
    if final_looks > 400:
        print("teleport never fires"); sys.exit(1)
else:
    print("out of attempts"); sys.exit(1)

# Chapter 5: pad the teleport-cutscene waitkeys, then north/east + looks.
cmds = PRE + sum(seglist(pads, final_looks), [])
for pads5 in range(0, 10):
    tail = [""] * pads5 + ["north", "east"] + ["look"] * 20
    out = replay(cmds + tail)
    if "THE END" in out:
        cmds += tail
        print(f"WIN with {pads5} ch5 pad(s); total {len(cmds)} commands")
        open("harness/light_up_solution_new.txt", "w") \
            .write("\n".join(cmds) + "\n")
        print("wrote harness/light_up_solution_new.txt")
        sys.exit(0)
print("no ch5 win; tail of last attempt:")
print(out[-2500:])
sys.exit(1)
