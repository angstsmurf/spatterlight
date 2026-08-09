# I Was a Teenage Headless Experiment — walkthrough (**WIN**, 10 commands)

- **Author:** Duncan Bowsman, EctoComp 2010 (fourth place). Dr. Gerchis has
  just sawn your head off; you play the body, blind, and have to get the head
  back on before you bleed out.
- **Engine:** **ADRIFT 4.00**
  (`xxd -p -l 12 games/headless.taf | cut -c17-22` → `93453e`).
- **Result:** **WON.** No score system (`score` reports 0 of 0), so the
  epilogue is the only measure.
- **Source:** `downloaded/TeenageHeadlessExperiment_clubfloyd.html`, the
  7 July 2013 ClubFloyd session. It does eventually win, but it spends most of
  its length stuck on the syringe (see below) and cannot be replayed. Route
  derived from the task dump.
- Row: `headless_solution.txt|headless.taf|as a teenage headless experiment|SCR_SKIP_WAITKEY=1`.

## The waitkey flag is not optional here

Most rows use `SCR_SKIP_WAITKEY=1` for cosmetic pagination. This one needs it
for a correctness reason that is easy to misdiagnose: the game **opens with a
fake death**. Before you get a single prompt it prints

```
... but he has the element of surprise and a bonesaw, and you do not.

I'm afraid you are dead!
You scored 0 out of the maximum 0!
That is 100% of the game!

[Press any key to continue]
```

and only then starts the real game. Without the flag that `[Press any key]`
eats the first command of the script, everything shifts by one, and the route
silently plays a different game that also never errors.

## The one real puzzle

`put head on body` is TASK 57/58, and its restriction is

```
RESTR type=0 obj3=[Formula X] v2=1        # v2=1 = HELD by the player
```

not merely *present*. Killing Gerchis leaves the glowing syringe on the lab
floor, and the obvious reading — the syringe is right there, so use it — gets
you `I don't understand what you want me to do with your head.` The fix is one
command, `get syringe`, between `kill gerchis` and `put head on body`. (The
`v2` place codes are decoded in `screstrs.cpp`'s `restr_object_in_place()`:
0/6 = in room, 1/7 = held, 2/8 = worn, 3/9 = visible.)

## Shape of it

`goldens/headless_solution.txt`, 10 lines:

```
get shovel            you are blind; you find it by feel
s                     HALLWAY JUNCTION
w                     you walk into the lab door
break window          through the frosted glass, door flies open
kill gerchis          one swing of the shovel
get syringe           Formula X must be HELD, not just present
put head on body      -- you are whole again
e
n                     ENTRYWAY
n                     out into Escanaba -- WIN
```
