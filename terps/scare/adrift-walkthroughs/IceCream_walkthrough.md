# Ice Cream! — walkthrough

- **Engine:** ADRIFT 4. A one-room job-sim joke: you are a new employee in the
  Ice Cream Shoppe whose job is to scoop ice cream, put it on a cone and serve
  the customer.
- **Result:** **0/0 sandbox — no score, no ending.** Solution file:
  `harness/icecream_solution.txt` (a representative serve).

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
put ice cream in cone
give cone to customer
```

(Drop a scoop and you can `mop up the ice cream`; that's the extent of the
"game".)
