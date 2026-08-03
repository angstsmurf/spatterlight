# Inverness Castle — walkthrough & analysis

- **Game:** *Inverness Castle* (file `inverness.taf`), version **0.3c**, February 5, 2001
- **Author:** David Good (david@dreamnetstudios.com)
- **Engine:** ADRIFT 3.90.12 (Campbell Wild)
- **Result:** **UNWINNABLE — no ending exists.** Max reachable **75 / 205 (36%)**,
  deterministic. Solution: `harness/inverness_solution.txt`.
  (Verdict re-checked from scratch 2026-08-02 — it stands; see *Provenance*.)

A faithful, abridged dramatisation of *Macbeth*, Act 1–2: you are the Thane of
Ross, arriving at Inverness Castle the night Duncan is murdered. The game is an
**unfinished beta** — it has a scoring system but **no win/lose state at all**,
and its climax dead-ends with the player knocked unconscious and tied up in the
cellar with no implemented escape.

## Why it can't be won / why 130 points are unreachable

Two structural facts, both proven from the `.taf` data (task/event dump) and
confirmed in play:

1. **No ending.** Across all 57 tasks there is **not a single EndGame action
   (ADRIFT action type 6)** — no win, no lose, no death-stop. The game can never
   declare itself over. (Same situation as `lifesimulation`, `Town of Azra`,
   `Nonsense Machine`: a scored sandbox/fragment, not a completable game.)
   The whole action histogram of the file is
   `type0 ×16, type1 ×19, type2 ×15, type3 ×18, type4 ×23, type7 ×4` — no type 5
   or 6 anywhere. Positive control for the method: `circus.taf`, *also* a 3.9
   file dumped through the same code path, shows **24** type-6 actions, so an
   EndGame in a V390 task would have been seen. (In the 3.9 on-disk format
   EndGame is raw action type **5**; `sctafpar.cpp`'s `V390_TASK_ACTION:Type>4?
   #Type++` fixup shifts it to internal 6.) The header's WinText field is also
   **empty** — the author never even wrote a victory message.

2. **130 of the 205 points are mutually exclusive.** The headline scorer is the
   witches' **riddle box**: tasks T29–T42 are **fourteen** different riddle
   answers worth **+10 each (140 pts)**, but they are gated on one variable
   (`var8` = "current riddle id", values 10–23). The box poses **exactly one**
   randomly-chosen riddle, and the **first correct answer opens the box**, after
   which the same riddle simply re-poses and never re-scores. So **only one of
   the fourteen riddle tasks is reachable in any single playthrough** — worth
   **+10**, not +140. `var8` is written exactly once, by `$initriddle` (T43),
   which is run by event 6 *"randomize riddle"* at game start with `restart=0`
   (fire-once); nothing else ever touches it, so the riddle can never be
   re-rolled. And SCARE — like the 3.9 Runner — scores each task at most once
   (`task_run_change_score_action()`: `gs_task_scored`; only 3.8 files may
   re-score), so re-answering the same riddle cannot pump the score either.
   The fourteen answers, in `var8` order 10…23, are: *fire, mushroom, shadow,
   bookmark, nothing, step, board, cold, m, r, coffin, darkness, e, mirror*.
   **Under the current deterministic harness seed the posed riddle is the
   `var8 = 13` one** — *"A hundred brothers lie next to each other… I am the
   tongue that lies between two"* → answer **bookmark** (T32). In the real
   Runner you get a random single riddle, but still only one solvable per game.

That caps the real maximum at **205 − 130 (the 13 unanswerable riddles) = 75**,
and every one of those 75 points is collected by the solution below.

## The 75 reachable points

| Pts | Task | How |
|----:|------|-----|
| +5  | knock (T25) | Knock 5× at the locked front door; the Porter's "hell-gate" speech plays, then he opens up |
| +10 | get torch (T3) | Distract the door Porter with bread, then take the lit wall torch |
| +5  | push statue (T15) | In the Sitting Room, push the statue aside to reveal a hole |
| +10 | look behind painting (T7) | Put the lit torch in the hole (a click sounds), then look behind the painting → a wooden box |
| +5  | ask witch about box (T44) | Carry the box to the heath; the witches identify it as a riddle box |
| +10 | answer riddle (T32, *bookmark* under the harness seed) | Solve the posed riddle; the box opens, revealing a scroll and an old key |
| +5  | unlock desk (T48) | Old key unlocks Macbeth's desk in the Master Bedroom |
| +5  | open desk (T47) | Open the drawer → Macbeth's letter to his wife |
| +20 | search bedroom (T8 + T9) | In the Dressing Room, overhear the murder plot ("search bedroom"); the discovery completes one turn later — and Macbeth catches you |

After that final +20 you are knocked out, dragged to the **cellar and tied to a
pillar**. This is the unfinished edge of the game: the only "untie" task
(`drop belt`, T54 — 15 alt commands covering belt/kilt/shirt/armour/helmet) is an
**empty stub with no actions**, and the cellar's exit is gated on a flag nothing
ever clears, so you are stuck at 75/205 forever. Precisely: `$getcaught` (T49)
sets variables 11 and 12 to 1, and **T50** — a blocker in the Cellar with alt
commands for *every* direction (`up/u/down/d/n/s/e/w/ne/nw/se/sw/go*/out`) —
fires whenever variable 12 is 1. No action anywhere in the file ever writes
either variable again, so the answer stays *"You can't go anywhere, you are tied
to a pillar."* forever.

The game does keep running: event 0 *"Duncan dies"* starts on `$getcaught` and
finishes 10 turns later (confirmed with `SCR_TRACE_EVENTS=1`), completing
`$duncandies`, which arms **T1** (`west` in the Receiving Hall, needing the
daggers) — clearly the intended Act-2 continuation where you are framed for the
murder. It prints nothing, and the Receiving Hall is unreachable from the
cellar, so it changes nothing for the player. That is where the beta stops.

## Full command list (deterministic, harness seed 1234)

```
knock
knock
knock
knock
knock              (Porter opens the door, +5; enter Entrance Hall)
n
n                  (-> Kitchen)
open pantry
take bread
se
s                  (-> back to Entrance Hall, Porter present)
give bread to porter
get torch          (+10)
e                  (-> Sitting Room)
push statue        (+5, reveals hole)
put torch in hole
look behind painting   (+10, reveals wooden box)
take box
open box           ("can't open it — perhaps someone can help")
w
s
ne
s                  (-> Blasted Heath, the three witches)
ask witches about box  (+5)
answer bookmark    (+10, the box opens — see the riddle note above)
take key
take scroll
n
w                  (Behind Inverness — the front door has re-locked)
s                  (-> Kitchen via the back door)
se
up                 (-> upstairs)
w                  (-> Master Bedroom)
unlock desk        (+5)
open desk          (+5)
take letter
s                  (-> Dressing Room; T8 fires on arrival, +10, and T9 -- the
                    wildcard "you listen to the voices" -- takes the other +10)
z                  (the Macbeth / Lady Macbeth plot scene plays out)
look
z
z                  (Macbeth bursts in, knocks you out -> Blackness -> Cellar)
score              (75/205)
```

Final score **75/205**. (The caught-and-cellar animation runs over the couple of
turns after the eavesdrop; the game then sits idle in the cellar.)

## Notes on the route

- **Bread → Porter → torch.** The lit torch is on the Entrance-Hall wall, but the
  Porter blocks you taking it. Get the bread from the Kitchen pantry and
  `give bread to porter`; he wanders off to find a drink, leaving the torch free
  (T3 literally checks "bread held by Porter").
- **Torch / statue / painting.** `push statue` exposes a hole; `put torch in hole`
  satisfies the hidden-light requirement so `look behind painting` works and drops
  the box into the Sitting Room.
- **You must leave the castle to open the box.** The old key (for the desk) is
  *inside* the box, and the box only opens after a witch riddle on the heath. The
  front door **re-locks** once you step outside, so re-enter through the **back
  door**: Road → W (Behind Inverness) → S (Kitchen).
- **The catch is mandatory for the last +20.** Entering the Dressing Room scores
  T8 (`search*`/`examin*`, restricted to rooms 22–23); T9 — the `*` wildcard
  "you listen to the voices" task — takes the companion +10 *and* is the start
  task of event 7 *"get caught"*, which is unconditional and runs `$getcaught`
  one turn later. There is no way to get those points without being caught, and
  being caught is a dead end — so 75 is simultaneously the maximum and the
  terminus.
- **The desk does not actually need the box opened.** T48's restriction is
  "old key held by player", and the key counts as held while it still sits
  inside the (held, unopened) riddle box — so `unlock desk`/`open desk` score
  their +10 even on a run that flubs the riddle. That is why the pre-2026-08-02
  desynced route still looked plausible: it reached the ending with only the
  riddle's +10 missing. **Verified against the real 3.9 Runner 2026-08-02**
  (probe `test/make_39_heldprobe.py`, run in `run390.exe` under Wine): a key
  inside a *closed* box the player carries answers "held by the player", and
  only stops doing so when the box is put down. So this is the original
  engine's behaviour, not a SCARE liberty.

## Provenance

Derived with the headless deterministic SCARE harness (`harness/`, seed 1234).
The reachability was proven from the structural task/event/where dump, now a
permanent tool: `SCR_DUMP_TASKS=1 ./scare ../games/inverness.taf` on a
`build.sh` binary (`scdump.cpp`, `-DSCARIER_DUMP_TOOLS`). 57 tasks, 26 rooms,
43 objects, 16 NPCs, 9 events. This is a faithful reading of the published 3.90
`.taf`, not a SCARE limitation — the missing ending and the single-riddle box
behaviour exist in the original ADRIFT Runner too.

**Re-checked 2026-08-02** (independent pass, "is it *really* unwinnable?"):
verdict unchanged — no EndGame action, empty WinText, cellar block permanent,
Duncan-dies event fires into an empty room. But the *route* had silently
desynced: the `scr_randomint` low-bit fix (see the corpus notes) changed which
riddle `$initriddle` rolls, so the committed `answer step` stopped matching and
the solution scored **65/205**, not 75, while still passing its regression row —
the row's win marker was the "A murderer thou shalt be" catch text, which the
desynced run still printed. Fixed here: `answer step` → `answer bookmark`, a
trailing `score` turn added to the solution, golden re-blessed, and the marker
in `run_v4_walkthroughs.sh` tightened to
`Your score is 75 out of a maximum of 205.` so the same class of desync fails
loudly next time. Full v4 corpus after the change: 127 rows, all PASS.

The one behaviour the re-check leaned on that had never been probed — "held by
the player" reaching into a **closed** carried container — was then checked
directly against `run390.exe` (2026-08-02). It holds; see *Notes on the route*
and `RUNNER_TESTS_TODO.md`.
