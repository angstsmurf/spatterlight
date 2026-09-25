# A Day In Toronto — walkthrough (**no ending exists; full tour**)

- **Game:** `toronto.taf`, ADRIFT 3.90. A beginner's sightseeing sandbox: CN
  Tower, a restaurant, Wallmart, Medieval Times, the science centre and an
  apartment block. 20 rooms, 3 tasks.
- **Result:** no win, no score and no ending of any kind. There is no
  `ACT type=4`, `type=6` or terminal room, and there are no events. The only tasks
  are TASK 0–2 (`ask waiter about fries/salad/burger`, each of which puts the
  dish in the restaurant). The walkthrough is a tour that visits all 20 rooms and
  uses every interaction. 35 commands after the name and gender answers. Empty
  win marker.
- **Env:** none. Lines 1–2 (`Bob`, `male`) answer the name and gender prompts.

## Route

`u u d d` (CN Tower: watch deck, space deck), `n` walkway (`take pass`), `w`
gift shop, `s` bank, `w` restaurant (`ask waiter about burger`, `eat burger`),
`w` guest information (`take restaurant pamplet`, `read restaurant pamplet`),
`s` Wallmart entrance, `in` (`take toy truck`), `nw` Medieval Times (with the
pass you see the whole show), `ne` science centre doors, `e` apartment building,
`in u d out`, `w n` lobby, `d` space section, `in` shuttle (`take`/`wear
astronaught uniform`), `out u u` arcade, `w` nature section (`x big fake bird`).

## Observed parser behaviour (believed Runner-faithful)

- `ask waiter about burger` prints "huh?" but still puts the burger in the room.
  A 3.9 task that has no completion text answers DontUnderstand (see the
  run390 silent-task note).
- Objects match only by their full raw name: `x bird` and `take uniform` fail,
  while `x big fake bird` and `take astronaught uniform` work.
- The tower pamphlet is authored as `tower pamplet ` (with a trailing space) and
  the route never refers to it. The restaurant pamphlet is the one you can take.
