# Forum — walkthrough

- **Engine:** ADRIFT 4 (`forum.taf`, Woodfish, **2nd ADRIFT One-Hour Game
  Competition**, 2003). An in-joke about the ADRIFT forum: you are Bob the
  newbie and the Dark Lord is the author. Sequel: `forum2.taf`.
- **Result:** **WON**, 0/0 — no scoring system anywhere.
- **Solution:** `goldens/forum_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `You Won!`

## Structural verdict

37 tasks, 10 rooms, two NPCs (The Mad Monk, Ds490). Task 36 (`# Player wins`)
is the only `EndGame(win)`, and it is never typed — it is *executed* by task 33
or 34, i.e. by whichever option you pick in the last of the four Final
Challenge rooms. Three tasks kill you: task 11 (the monk, via `EVENT 1`), task
17 (Ds490, via `EVENT 2`) and task 22 (Mystery, on the third wrong riddle
answer).

## Route

Line 1 is **blank on purpose** — the intro ends in `... press any key ...`,
which swallows one line of input.

```
        ← blank
x hand
n
x altar
take bowl
z
z
throw water at monk
wear clogs
n
e
smack ds with the giant yellow rubber hand
remove clogs
smack ds with the wooden clogs
n
a coin
pay a coin
1
1
1
1
```

## Notes

- **Both timed fights are the whole game.** `n` (task 0) teleports you to the
  Temple; `EVENT 0 [Monk walks in]` then fires task 7 after 2–6 turns and
  `EVENT 1 [Monk kills]` fires task 11 at turn 8.
- `x altar` reveals the glass bowl of holy water. **The monk's arrival text
  prints at the end of the second `z`** — throwing earlier only gets *"The Mad
  Monk is not here."* `throw water at monk` (task 9) cures him; he thanks you,
  leaves the wooden clogs, and is also the gate on the Temple's north exit.
- `wear clogs` before going north: task 12 requires the clogs **worn** to cross
  the coals into the Circular Chamber.
- **Ds490's fight is two smacks and both phrasings are fussy.**
  - The noun alternatives are `[ds/monster/man/figure]` with `{490}` optional,
    so `smack ds490` does **not** match — it falls through to the library's
    *"I don't think … would be a very effective weapon."*
  - Task 15 (the clogs blow) requires the clogs **held**, so you must take them
    off again first; while worn it answers *"I don't understand what you want me
    to do with the pair of wooden clogs."*
  - Each blow bumps a counter; at 2 the engine runs task 16 (`# Ds defeated`).
    `EVENT 3` would run task 18 nine turns in, so there is no time to dawdle.
- Mystery's riddle — *"I have a head and a tail, yet no body"* — is answered
  with `a coin` (task 19), which also puts the coin in your hand. `pay a coin`
  buys admission to the Evil Lair. The four Final Challenge rooms accept only
  `1` or `2` and **any** answer advances, so four `1`s finish it.
