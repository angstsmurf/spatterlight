# Villains and Kings — Walkthrough

A short ADRIFT game with the Battle System enabled. Derived under SCARE
(deterministic seed) and cross-checked against the game's internal task, event,
exit and Battle-System data, and against the original ADRIFT Runner
(`run400.exe` / `jasea-0.2t.jar`, source in `decompiled/Battles.bas`).

**Result: 13 / 37 points.** There is no winning ending. The other **24 points
are unreachable in any faithful interpreter** — they are gated behind a combat
kill the game's own data makes impossible, a pair of mutually-exclusive scoring
tasks, and two window tasks that can never satisfy their state requirements.
Full analysis at the end. None of this is a SCARE divergence from the Runner
(one genuine SCARE crash *was* found and fixed; see the last section).

You are "Assface the Third", a not-really-a-detective summoned by King Harry.

---

## Town map

```
  Royal Shower Room (3) -E-> Kings throne room (0) [Harry, Guido, doughnuts]
                                 |  S
                              Hall (1)  [broken window w/ soap; jackass spawns here]
                                 |  S
                            Waiting room (2) [start; Small man has the note]
                                 |  S
                            Courtyard (4) -E- Armory (5) -in- Forge (6)
                                 |  W
                            Giant fountain (7) -N- Outside a BIG house (9) -W- Sculpto's House (8) [radio, dead "guy"]
```

---

## Full command list (13 points)

```
                           (the game starts straight in the Waiting room)
take note                  (sneak the note from the Small man's back pocket)
n                          (-> Hall)
n                          (-> Kings throne room)
give note to king          (+1)
w                          (-> Royal Shower Room)
push tile                  (+2; a loose tile - "click" opens the Hall window AND
                            makes the assassin appear in the Hall)
e                          (-> throne room)
s                          (-> Hall)
take soap on a rope        (from the now-open broken window)
x window                   (bind the parser's "referenced object" to the broken
                            window, so the next line targets it and not the
                            decorative stained-glass "windows")
close window               (+1)
n                          (-> throne room)
give soap on a rope to king (+5; the king joyfully runs off to shower, tossing
                            you a key)
take doughnut              (+3; you can only snatch it WHILE the king is in the
                            shower - if you try earlier, "the king is watching")
s
s
s                          (Hall -> Waiting room -> Courtyard)
w                          (-> Giant fountain)
n                          (-> Outside a BIG house)
w                          (-> Sculpto's House)
use radio                  (+1)
```

That is every reachable point: note **+1**, soap **+5**, close window **+1**,
doughnut **+3**, push tile **+2**, radio **+1** = **13 / 37**.

---

## Walkthrough notes

- **The note** is in the Small man's back pocket in the Waiting room; `take note`
  sneaks it out. Give it to the king for **+1**.
- **The window puzzle.** The Hall's broken window can't be opened normally. In
  the Royal Shower Room (west of the throne), `push tile` (**+2**) triggers a
  "click" that opens the window — and also makes the assassin appear in the
  Hall. Grab the `soap on a rope` from the window. To score `close window`
  (**+1**) you must first `x window`: "window" is ambiguous between the broken
  window and the decorative stained-glass "windows", and examining it first
  points the parser at the right one.
- **The soap.** Give the `soap on a rope` to the king for **+5**. He runs to the
  shower, which is the only moment the doughnuts are unguarded.
- **The doughnut.** `take doughnut` scores **+3**, but *only while the king
  showers* — otherwise "the king is watching you hover around his doughnuts".
- **The radio** is in Sculpto's House (far southwest). `use radio` for **+1**.

---

## The 24 unreachable points (all game-data, not SCARE)

### Combat is unwinnable — blocks 17 points

Pushing the tile makes "Jackass Trying to Kill You" (the assassin) appear in the
Hall and turn hostile (a type-7 *Change Battle Attribute* action sets its
attitude to Enemy). The Battle System then has it attack you each turn. But it
can never be killed, and it can never hurt you, because of the shipped stats:

| Character | Strength | **Accuracy** | Defense | **Agility** | Stamina |
|-----------|----------|--------------|---------|-------------|---------|
| Player | 2 | **0** | 1 | **0** | 5 |
| Jackass | 1 | **0** | 1 | **0** | 3 |
| *(all other NPCs)* | … | **0** | … | **0** | … |
| *(all weapons, incl. grenade)* | Hit +2…+10 | **Acc +0** | | | |

ADRIFT resolves a hit with **`accuracy > agility`** (strict — confirmed in the
Runner source, `decompiled/Battles.bas`). With every accuracy and agility at 0,
the test is forever `0 > 0` = false, so **no blow ever lands** (you see only
"manages to avoid"). The assassin has 3 stamina but cannot be drained.

Because the assassin can't die, its death task **`jackassdies` (+2)** never
fires; the corpse needed for **`search guy` (+10)** never appears; and that
search is the *only* source of the **golden soap**, so **`give golden soap to
king` (+5)** is impossible too. That is 17 points gated behind one impossible
kill. The original Runner behaves identically — it's the author's incomplete
combat data, the same pattern as *The Town of Azra*.

> The battle verbs themselves work fine in SCARE (verified): `attack guy` /
> `attack jackass trying to kill you` produce real strikes; only `attack jackass`
> fails, because "jackass" is not one of the NPC's handles (its handles are the
> full name and the alias "guy") — exactly as the Runner behaves. The fight is
> just unwinnable by data.

### A mutually-exclusive duplicate — 5 points

There are two **+5** tasks for handing over the soap: `give soap to king`
(task 2) and `yes` (task 17, the king's "is this for me?" prompt). Both transfer
the single `soap on a rope`, so once you've done one you no longer hold the soap
and the other can't fire. Only **+5** of the nominal **+10** is ever obtainable.

### Two window tasks that can't meet their state — 2 points

- **`open window` (+1)** requires the window in state CLOSED(6). The broken
  window only ever goes broken → OPEN(5) (via the tile) → LOCKED(7) (via
  `close window`), never passing through CLOSED, so the task can't fire.
- **`take soap` (+1)** (the scored take, task 5) never triggers: the library
  "take" verb resolves and removes the `soap on a rope` before the scoring task
  matches, for every phrasing tried. (You still get the soap — just without the
  point.)

### Tally

13 reachable + 17 (combat chain) + 5 (duplicate soap) + 2 (window) = **37**.

---

## With combat-assist enabled: 30 / 37

SCARE now has an opt-in **combat-assist** mode (`sc_set_combat_assist`; the
headless harness enables it via `SC_ASSUME_COMBAT=1`) that gives an automatic
hit roll in games whose author left all Accuracy/Agility at 0 — making this
game's intended strength-vs-defence combat actually work. With it on, the
assassin is killable and the combat-gated content opens up, raising the maximum
from 13 to **30 / 37**. Solution: `harness/villains_and_kings_assisted_solution.txt`.

The extra steps (after pushing the tile spawns the assassin in the Hall): grab
the **sword** from the Armory first, then in the Hall `attack guy` ~3× to kill
the assassin (`jackassdies`, **+2**), `search guy` (**+10**) which yields the
**golden soap**, `take golden soap`, and — *before* picking up the ordinary soap
on a rope — go to the throne and `give the golden soap to king` (**+5**; the king
proclaims "I HAVE THE GOLDEN SOAP!"). Then collect the rope soap and give it too
(**+5**). (Order matters: while holding the rope soap, "give … golden soap …"
matches the *ordinary*-soap task first, so hand over the golden soap while it's
the only soap you carry.)

The remaining 7 points are still out: the duplicate rope-soap task (`yes` vs
`give soap`, mutually exclusive, −5) and the two finicky window tasks (`open
window` −1, scored `take soap` −1). The assist is **non-faithful** (the real
Runner also can't hit here); the faithful maximum remains 13/37.

## A genuine SCARE bug found and fixed: `close window` crash

While deriving this, `close window` typed before any object had been referenced
**crashed SCARE** with `prop_find_child: integer key cannot be negative` and
aborted the interpreter. Cause: an object-state restriction that refers to "the
referenced object" was evaluated when no object was bound (`referenced_object`
is its initial **-1**), and that -1 was passed straight to `prop_get_integer`,
which fatals on a negative key.

The original Runner does **not** crash here, so this was a SCARE robustness
defect. Fixed in `terps/scare/screstrs.c` (`restr_pass_task_object_state`) by
returning FALSE when the referenced object is < 0 — the restriction simply can't
be satisfied — matching the `referenced_object == -1` guards already used
elsewhere in the codebase. After the fix, `close window` gives a graceful
response, and the intended `x window` → `close window` path scores its +1.
