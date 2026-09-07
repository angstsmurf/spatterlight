# Ice Cream! — walkthrough

- **Engine:** ADRIFT 4. A one-room job-sim joke: you are a new employee in the
  Ice Cream Shoppe whose job is to scoop ice cream, put it on a cone and serve
  the customer.
- **Result:** **0/0 sandbox — no score, no ending.** Solution file:
  `goldens/icecream_solution.txt` (a representative serve).

## Structural verdict

15 tasks, **1 room**, 4 events, 1 NPC (the customer). A full dump shows **zero
`ChangeScore` actions and zero `EndGame` actions** — there is no score and no
win/lose state to reach. The tasks just model the serving loop (`scoop ice
cream`, `put ice cream in cone`, `give cone to customer`), a couple of random
"the ice cream falls" events (`#RandFall` / `#RandomFall`) and the cleanup
(`mop up the ice cream`). It is a pure activity sandbox — you can serve cones
indefinitely, but nothing is tallied and the game never ends.

## Play

```
take cone
scoop ice cream
place ice cream in cone
give cone to customer
```

(Drop a scoop and you can `mop up the ice cream`; that's the extent of the
"game".)

## Parser traps

**`put ice cream in cone` does not work — `place` or `set` is required.** The
three serving tasks share one pattern,
`[put/place/set]{the/some/all}{of}{the}[ice cream]{in/in the/on/on the}[cone]`,
so the wording ought to be free. But 4.0 hands any line holding the whole word
`put` or `drop` to its put/drop list parser *before* the task dispatch; that
splits at `" in "` and resolves `"put ice cream "` against the objects present.
Nothing scores (every object here has Prefix "the", and the aliases are "ice
cream scoop" and "ice cream cone"), and the one no-match exit leaves the
command line rewritten to that fragment. The tasks then match nothing and the
catch-all speaks for the noun resolved from the original line: "I don't
understand what you want to do with the cone." Measured in run400.
`put ice cream on cone` escapes it — the `on` branch has an escape of its own —
and `place`/`set` never enter the parser.

**`take cone` at the start is a task refusal, not the library's.** The cone is
already in hand, and task 14 (`[take/get] cone`, restricted on not holding it)
prints its FailMessage `  You already have an empty cone.` — 4.0 gives the
tasks their look at a held object before the " already carrying " refusal.
