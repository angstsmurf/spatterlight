# Quest I (Quest for Flesh) — walkthrough

- **Engine:** ADRIFT 4. A one-room horror vignette: you are a ravenous
  creature in a dead-end alley, driven by a constantly rising **hunger** vital.
- **Result:** **UNWINNABLE — there is no victory ending.** Max score 10/?.
  Solution file: `harness/questi_solution.txt`.

## Structural verdict

13 tasks, **1 room**, 1 event. The two `ChangeScore` actions are `eat *cat`
(+5) and `eat *man` (+5); the only `EndGame` actions are **deaths** — `#Check
for Death from Hunger` (T3, `var1=2`) and `south` (T12, `var1=2`, leaving the
alley into traffic). **There is no `var1=0` win**, so the game cannot be won; it
is a survival-flavoured dead end where you feed (briefly dropping hunger) until
the hunger timer kills you.

## Play

```
eat man     <- +5; "you are having the most savage of lunches" (drops hunger)
eat cat     <- +5 when a stray cat is present
```

Each meal only delays the inevitable; the per-turn hunger event eventually fires
the death ending, and walking `south` is an instant death. The reachable maximum
is the two +5 feedings before starvation.
