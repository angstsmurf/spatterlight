# Salutations — walkthrough

- **Engine:** ADRIFT 4.00 (`salutations.taf`, 5,591 bytes) by **Lumin**,
  Ectocomp 2008. **One room, 17 tasks, 2 events, 9 objects, 3 NPC records**
  (all three called "The Spider"; only NPC 0 starts in play).
- **Result:** ★ **WON** — T12 `burn sack (soaked)`, `ACT type=6 v1=0`, then
  WINTEXT. There is **no score**: not one `ACT type=4` in the file, so every
  win is equally maximal.
- **Solution:** `goldens/salutations_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh` with `SCR_SKIP_WAITKEY=1`). Win marker:
  `you'll decline to answer.`
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS`
  plus play.

## Route

```
remove jacket            T0   frees your right hand
x leaves                 T13  reveals the branch caught in the web
get stick                T2   needs the jacket AND the leaves
get pack                 T3   needs the stick: you hook a strap and lift
get knife                T4   needs the pack
kill spider              T7   ungated — starts the six-turn clock
get lighter              T9   needs the pack (flavour only, see below)
get whiskey              T10  needs the spider dead
pour whiskey on sack     T11
burn sack                T12  EndGame win
```

Ten commands, no movement — the game never leaves *Stuck in a Spider's Web*.

## The shape: a gated chain, then a race

The first five commands are one linear `RESTR type=2` chain (jacket → leaves →
stick → pack → knife) and nothing is timed while you work through it. Killing
the spider flips the game: **EVENT 1 [spider dead]** starts on T7 with
`time1=time2=6` and finishes in T14 `#death`, the egg sack splitting open and
the hatchlings eating you.

The clock is tight on purpose, because T10 `get whiskey` is restricted on T7 —
the whiskey **cannot** be taken before the spider is dead, so none of the
fire-setting can be prepared in advance. Six turns, four spent.

**Watch `z`.** The game sets the ADRIFT global `WaitTurns` to 3, so one `wait`
costs half the budget. Measured from the kill: five ordinary commands survive,
six die; a single `z` survives, two `z` die; three ordinary commands plus one
`z` die.

The one real trap is the obvious move. T5 `cut sack` — knife in hand, sack in
front of you — is `ACT type=6 v1=2`, a loss: you slit it open yourself and the
swarm comes out. T6 `cut web` is safe but leads nowhere.

## Three things worth keeping

**1. The `<waitkey>` shift is silent, not fatal.** The intro ends in a
`<waitkey>`, so without `SCR_SKIP_WAITKEY=1` the *first* command of the script
is swallowed as the keypress. Because of slip (3) below the game still wins
that way — dropping `remove jacket` costs nothing — so the row would have
blessed a quietly shifted, one-command-shorter transcript rather than failing.
That is the argument for setting the variable on every row whose game contains
a `<waitkey>`, not just the ones that visibly stall.

**2. A messageless restriction falls through to the library.** T2 `get stick`
carries two restrictions and only one of them has failure text:

| state | `get stick` answers |
|---|---|
| nothing done | `You can't really move your arms right now...` (T2's text) |
| jacket off, leaves not examined | `Take what?` (library — the stick isn't in scope yet) |
| leaves examined, jacket on | `You take the long stick.` (library take succeeds) |

So the message belongs to the *leaves* restriction; failing only the
messageless jacket restriction drops straight through to the library, and once
T13 has put the stick in the room the library take just works. This is the v4
restriction model in miniature — see `adrift4-vs-5-restriction-eval`.

**3. Two author slips make a six-command win possible**, which the committed
route deliberately does not take:

- T4 `get knife*` is gated on holding the backpack, but that restriction has
  no failure message either, and the library take reaches into the pack lying
  on the ground quite happily (`You take your hunting knife.`). The whole
  jacket/leaves/stick/pack chain is skippable. Contrast T3 `get pack` and T9
  `get lighter`, which *do* have failure messages — "You stretch as hard as
  you can, but with the pack on the ground and you hanging above it it just
  doesn't seem possible to reach." and "The lighter is in your backpack and
  you can't reach it." — and so refuse properly.
- T12 `burn sack (soaked)` restricts on T11 (the whiskey) only, never on
  holding the lighter; T15 `burn web` does check for it. The winning text
  describes flicking the lighter on even in a run where `get lighter` was
  refused.

Put together: `get knife / kill spider / get lighter / get whiskey /
pour whiskey on sack / burn sack` wins in six, and `get lighter` there is pure
decoration. The regression takes the intended path so that it exercises the
object chain and both events instead of the holes in them.
