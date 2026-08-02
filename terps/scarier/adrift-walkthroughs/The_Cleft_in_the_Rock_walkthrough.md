# The Cleft in the Rock — walkthrough

- **Engine:** ADRIFT 4 (`cleft.taf`, Axemark, 2001). "After hearing many
  rumours of yet another Underground Empire, you have arrived at what appears
  to be a likely entrance." What you actually find is a show cave with a ticket
  kiosk, a topiary, a sleeping attendant and a loading bay.
- **Result:** ★ **WON, 100/100** — a genuine full score. The dump's eight
  awards (5 + 10 + 10 + 5 + 3 + 12 + 15 + 40) sum to exactly 100 and this route
  collects every one; the game's own `score` verb agrees the maximum is 100.
- **Solution:** `harness/cleft_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `You have successfully completed the Cleft in the Rock`.
- **Provenance:** no author walkthrough exists — plover.net lists the game
  without one. The route was derived from `SCR_DUMP_TASKS`.

## Route

```
n / e / e / d / d / wake attendant / u / u
w / w / w / w / w / get ticket / e / e
e / n / give ticket to merthyr / n / e / get compass / w / s
s / nw / w / drop compass / e / se / ne / n
out / get knife / in / n / sw / e / e / cut tape
drop knife / open loose box / get rope / get lamp / w / w / nw / w
tie rope to compass / drop all / get compass / eat compass / get lamp / d / get battery / put battery in lamp
u / e / se / e / n / n / n / ne
press red / press blue / pull lever / sw / s / s / s / d
d / z / z / z / z / z / z / z
z / z / open case / get coin / u / u / w / sw
score / put coin in slot
```

(one command per line in the solution file; no blank lines — the game has no
`<waitkey>` pauses, checked with `SCR_MARK_WAITKEY=1`.)

## Notes

- **The pedestal compass is the whole ordering problem.** `tie rope to compass`
  (+15) is gated on the rusty pedestal compass being *in the Pothole room*, and
  that compass weighs **81** of the player's **90** weight units. It can only be
  carried on an otherwise empty inventory, so it gets a trip of its own —
  fetched from the Scenic vista and dumped down the passage before the crate in
  the Storeroom is ever opened.
- **The carried-weight ledger double-counts container contents**, faithfully to
  the Runner: `get all` on the opened loose box reads **72** for 45 units of
  goods, and still reads **27** after `drop all`. That stale 27 is enough to put
  the 81-unit compass over the limit, which is why the rope and lamp are taken
  one at a time (`get rope`, `get lamp`) and the knife is dropped as soon as it
  has cut the tape. The `drop all` at the Pothole room does clear the ledger to
  0, which is what makes the second `get compass` possible.
- **The control-room combination is red + blue, and *not* green/puce.** Task 9's
  restrictions want `press green` and `Press puce` **undone**; pressing all four
  gets you "Nothing happens." The lever is worth 12 and sets a klaxon going.
- **The packing case arrives 15 turns after the lever**, at the Loading bay, and
  the nine `z`s are that wait counted from the walk down the ramp. It holds the
  gold coin the endgame needs (`put coin in slot`, +40, in the Tiny room).
- `eat compass` is a 3-point joke that destroys the compass, so it has to come
  *after* `tie rope to compass` has already opened the shaft down to the Cave
  depths. The shaft stays open because the exit is gated on the task, not on
  the compass still being there.
- The Deja vu / Vuja de maze is not a maze: every exit from the first room but
  `sw` leads to the second, `out` from the second reaches the Side chamber with
  the Stanley knife, and `sw` from the first goes home to the Foyer.
- The lamp is optional for navigation — the Control foyer and Control room read
  "It is far too dark to see." / "It is far too see to dark." without it, but
  the buttons and lever still work. It is only carried because
  `put battery in lamp` is worth 5.

## Harness fix this game forced

`SCR_DUMP_TASKS` died on this .taf with
`scarier: internal error: prop_get_string: can't retrieve property`, truncating
the dump after task 9 and printing no rooms, exits or events at all. Two
`prop_get_string` calls in `scdump.cpp` were fatal on a legitimately absent
property and are now `prop_get` with a checked result:

- the task **Command** — Cleft has two tasks (10 and 11) with an empty command
  list;
- a type-3 action's **Expr** — a plain "set a literal" action stores only
  Var1..Var3.

The `Short` name lookup in `scdump_object_name()` was hardened the same way.
