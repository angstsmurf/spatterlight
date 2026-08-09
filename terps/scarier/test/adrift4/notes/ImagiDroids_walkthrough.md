# ImagiDroids — walkthrough (**ending reached**, 20 commands)

- **Author:** Woodfish, "Release 1 / ADRIFT Runner 43". Credited testers are
  David Whyld and The Invisible Man, who saw it "as a section of *Active
  Fiction*". You are an android at a computer, and the fantasy game you play on
  it — *The Quest For Krandor*, "Written by >ANDROID001" — is a game an android
  wrote. The frame story is two professors in the lab below working out what
  that implies.
- **Engine:** **ADRIFT 4.00**
  (`xxd -p -l 12 games/imagi.taf | cut -c17-22` → `93453e`).
- **Result:** the game's **single ending** is reached. No score system (no
  `ACT type=4` anywhere), and only one `ACT type=6`, on TASK 42.
- **Source:** `downloaded/Imagidroids_walkthrough.txt`, from `walkthrus.txt`
  in the Woodfish compendium. It replays with **one** substitution.
- Row: `imagidroids_solution.txt|imagi.taf|You choose to put him out of his misery.|SCR_SKIP_WAITKEY=1`.

## The one authorial repair: `open it` → `open brick`

The list goes `x clean area` / `take brick` / `open it`. TASK 34 prises the
loose brick out of the wall and prints "revealing some chalky words written in
the space behind it", but the pronoun still refers to the **clean area** you
examined the turn before, so `open it` becomes `open the clean area` and is
refused. The task that matters is

```
TASK 36  cmd=[[open/pull]{a/the}{loose}[brick]]
    ACT type=0 v1=4 v2=0 v3=0 obj13=[brick]     # the brick crumbles to dust
    ACT type=0 v1=3 v2=4 v3=0 obj12=[key]       # the golden key -> your hand
```

Nothing else in the game produces the key, and without the key `unlock door`
(TASK 27, `RESTR type=0 v1=3 v2=1` — key held) fails and the cell is a dead
end. Naming the brick is the only repair the author's list needs.

## The other failure was ours

The list's `north` used to fail as well, and that one was the interpreter's
fault, not the author's. The exit task is

```
TASK 38  cmd=[{go/walk/move}[n/escape/out]{orth/out}]
```

— one word built out of two adjacent groups, so it should take `n`, `north`,
`escape` and `out`. SCARE interposed a **mandatory** whitespace node between
adjacent `[]` / `{}` groups, so only the bare `escape` and `out` forms worked
and the shipped walkthrough could not leave the prison cell.

This game is also where the intended behaviour is provable, because it contains
both halves of the argument:

* `[open/pull/push]{the}{wooden}[door]` has no spaces in it at all yet has to
  match "open door" — so a space between adjacent groups must be *allowed*.
* `[s]{outh}{ /-}[w]{est}` (TASK 6, southwest/northwest) spells the space in
  "south west" out as an explicit `{ /-}` alternative — so adjacency alone must
  not *imply* one.

Only "optional whitespace" satisfies both. Fixed 2026-08-04 in `scparser.cpp`
with a distinct `NODE_JOIN` node; explicit spaces written in a pattern keep the
strict matcher. The same change repaired *The Forum*'s TASK 15, whose noun is
`[clog]{s}` — its second blow against Ds490 had been silently failing, with the
parser refusal blessed into the golden. See `RUNNER_TESTS_TODO.md` §7. No other
row of the 198-row corpus moved.

## The rest of it

There is no third puzzle. `x walls` → `x clean area` is a two-step reveal chain
(TASK 30 gates TASK 34), `unlock door` / `open door` / `north` is the door, and
walking out of the cell is where the inner game takes over: the troll guard
walks in, the android's avatar beheads him unprompted, and *The Quest For
Krandor* is over.

The eleven `z`s are the professors' conversation, which is a pure event chain
with no input in it — EVENT 2, 3 and 4 are three turns each and EVENT 5 is two,
firing TASK 39 → 40 → 41 → 42. TASK 42 is `[# Android 013 jumps down]`, and its
`ACT type=6` is the end of the game. The upstream list says twelve `z`s;
eleven is exactly enough, and a twelfth would be typed after the game is over.

`SCR_SKIP_WAITKEY=1` is required for the row: four `[ press any key ]` pauses
(the game booting inside the game, and three between the frame story's scenes)
would otherwise eat four commands.

## Shape of it

`goldens/imagidroids_solution.txt`, 20 lines:

```
turn on computer
click on icon                     -> The Quest For Krandor
x walls / x clean area            the two-step reveal
take brick / open brick           <- the list says "open it"; the key
unlock door / open door / north   out of the cell, and the troll dies
z x11                             the professors talk themselves into it
                                  -- ENDING
```
