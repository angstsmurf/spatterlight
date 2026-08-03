# Wrecked — walkthrough (**WIN, 250/250 — maximum**)

- **Author:** Campbell Wild (ADRIFT's own author), 2000.
- **Engine:** **ADRIFT 3.80** (`xxd -p -l 12 wrecked.taf | cut -c17-22` → `944536`),
  the third and last 3.80 game in this corpus after `marooned.taf` and
  `Crime_Adventure.taf`.
- **Result:** **WON, 250 of 250**, verified in seeded Scarier
  (`harness/wrecked_solution.txt`, PASSing golden, win marker
  "Hope you enjoyed playing Wrecked.").
- **Source:** this *is* a replay — `downloaded/Wrecked_walkthrough.txt`, "By
  Campbell Wild 12/09/00", is the author's own solution for this build, and
  every one of its `[+ N points]` annotations is reproduced. What it does not
  give is anything you can pipe into an interpreter; four classes of gap had to
  be filled in, and those gaps are the interesting part.

## Gap 1 — the bracketed waits are real turns

The published file says `[wait for train to arrive]`, `[wait for train to
leave]`, `[wait for train]`, `[wait for train to go to Redstown]`,
`[wait for train to go to Ambersville]` and `[wait for Porkie to arrive]`.
Under the fixed seed those cash out as:

| moment | waits |
| --- | --- |
| first arrival at Ambersville station → train pulls in | 2 |
| in the train toilet → doors close and the train moves off | 3 |
| second visit to Ambersville station → train pulls in | 10 |
| aboard at Ambersville → "pulls in to a stop at Redstown" | 7 |
| aboard at Redstown → "pulls in to a stop at Ambersville" | 5 |

The first ride is a *free* ride: you have no ticket, so hide in the toilet
until the train is moving, step `out`, and one turn later Boris "yanks open the
doors, and kicks me out of the moving train" — which is the `[should be thrown
to wasteland]` note, and the only way into the scrapyard from the inside.
`east`, `push button` (+5) opens the wire gate for good.

## Gap 2 — Porkie wanders

`wave wand` in the village centre (+10) turns the statue of Porkie into a live
pig, who then walks a random circuit of his own. The second wave — the one that
turns him back to stone *outside the Post Office*, where you need a step up
(+5) — only works on a turn when he is actually in the room. Three `wait`s put
you on his cycle; the first `wave wand` still misses him ("nothing appears to
happen") and the announcement "Porkie walks towards you from the south" lands
on that same turn, so a second `wave wand` immediately after is what scores.
The `wait` count matters more than it looks: `wave wand` and `wait` draw
different amounts from the RNG, so a fourth `wait` instead of the wasted wave
sends Porkie straight through the room and out again in one turn.

## Gap 3 — two blocker tasks whose FailMessage is the placeholder `x`

Twice the game refuses a bare direction word and prints a lone `x`:

* **The pub, wearing the scuba outfit.** Task 96 (`where=1 room=10`, commands
  `in pub with scuba` and `in`, restriction "scuba outfit worn by player",
  CompleteText *"Arthur sticks his head out the door. \"You can't come in
  looking like that!\""*) exists to stop you drinking in a wetsuit. Its
  FailMessage — shown once you have taken the outfit off — is the author's
  placeholder `x`, and it swallows the plain command `in`.
* **The Post Office roof.** Task 84 (`climb *roof*`, alt commands `up`, `u`,
  `get *roof*`, `go *roof`) is the "I cannot reach the roof of the Post Office
  from here" blocker, restricted on task 83 (`climb *statue*`) **not** being
  done. Climb the statue (+10) and the restriction inverts, so `up` now prints
  `x` instead of taking the room's newly-opened up exit.

Both are faithful, not Scarier bugs. The pub case was checked live: `run390.exe`
under Wine, playing a gen390 conversion of this very file, prints `x` and
refuses entry exactly as we do. The roof case has the identical shape, and
gen390 re-encodes its restriction byte-identically to our parse
(`RESTR type=2 v1=84 v2=1` → task 83 must not be done).

The workaround is the same both times, and it is a parser fact rather than a
fix: **`go in` and `go up` are not in either task's command list**, so no task
matches, and the movement falls through to the room exit. `go in` was confirmed
to work in run390 as well; `go up` was only exercised in Scarier.

## Gap 4 — `turn it` binds to the wrong noun

The published line after `put key in ignition` is `turn it`. The pronoun binds
to the ignition, and `turn ignition` reaches task 37 (`turn *key*`), the
blocker restricted on the key *not* being in the ignition, which prints its
FailMessage "I can't turn the ignition." The scoring task is 55, same command,
restricted on task 35 done **and** petrol in the engine. Say **`turn key`** and
"The motor roars to life." (+5).

Order matters in the boat, too: `throw anchor overboard` fails flat ("I don't
understand what you want me to do with the anchor") until `push lever` has
carried you out to the wreck.

## The route in brief

Full command list: `harness/wrecked_solution.txt`.

1. **Bus shelter → swimming baths.** Read the graffiti for the phone number,
   take the tweed jacket and the ignition key from the bench, `close` the red
   locker (+5) and take the coin.
2. **The pint chain.** Buy a pint with the coin (+5), carry it out, `give pint
   to boff` (+10) — he hands over the marker pen. Explicit nouns are required
   here: the pronoun does not bind to the pint Arthur puts on the bar.
3. **The Gold card.** Get the application form from the Post Office rack, `fill
   it out` (+10, needs the pen), `post it` (+5). Then `write on bus shelter`
   (+5) and drop the pen.
4. **The train, take one.** Ride without a ticket, get thrown out onto the
   wasteland, `push button` (+5). Collect the Gold card from the unclaimed-mail
   tray at the Post Office.
5. **Shopping.** `pick flowers` (+5); `give card to harold` (+5) buys the scuba
   outfit. `push plaque` (+10) in the village centre yields the pork chop;
   feeding it to Angus (+5) frees the shark hook, which opens the drain (+5).
6. **The pool.** Wear the outfit, `swim` (+5), `pull plug` (+5) — that drains
   the pool and, with it, the sewer.
7. **Getting drunk.** Take the outfit off, `go in` the pub, `buy another drink`
   (+5), pocket the ticket off the tables, `drink lager` (+5) outside; while
   drunk, `put glass under car` in the scrapyard (+5) fills it with petrol.
8. **The motel and Narnia.** `give card to helga` (+5) for a room; `flush it`
   (+5) in the bathroom; `go in wardrobe` (+5) reaches Narnia, where the jacket
   (+5) buys a flute and the flowers (+5) buy the witch's wand. `phone 4388`
   (+10) from the lobby sets up Redstown.
9. **Porkie and the cat.** See Gap 2. `go up` onto the roof, take the cat,
   `give it to madge` (+5) for the hexagonal amulet.
10. **The wreck.** Anchor from the drained sewer; boat from the pier; petrol
    (+5), key (+5), `turn key` (+5), `push lever` (+5), tie the anchor (+5) and
    throw it over (+5). Dive: `put amulet in indentation` (+10) opens the
    cabinet — log book and bronze key. Back up, `get anchor`, `push lever`, and
    the log book names the sewer as the hiding place.
11. **Redstown.** Ticket in hand this time. `knock three times` (+10);
    `give her the flute` (+10) buys night-vision goggles.
12. **The treasure.** Chewed stick from the kennel; `put key in keyhole` (+5)
    in the sewer opens the passage; `wear goggles` for the dark stretch;
    `put stick in slot` (+5) makes a lever, `pull lever` (+5) raises the
    portcullis; `open chest` (+10) — and the scuba outfit you are still wearing
    is what stops the poison gas killing you.

**250 of 250.**
