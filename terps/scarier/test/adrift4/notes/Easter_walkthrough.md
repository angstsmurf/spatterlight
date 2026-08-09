# It's Easter, Peeps! — walkthrough

- **Engine:** ADRIFT 4.0 (Generator 4.00). *It's Easter, Peeps!*, by
  **Sara Brookside**, One Room Game Competition 2006 (release 1.0, 13 May 2006).
- **Game file:** `easter.taf` (copied into `games/`).
- **Solution:** `goldens/easter_solution.txt` — 71 commands.
- **Result:** ★ **WON**, `***You have won***`. The game keeps no score.
- **Row:** `easter_solution.txt|easter.taf|***You have won***|`
- **Row env:** none. The game has exactly one `[Press any key to end]`, and it
  comes *after* the win text, so `SCR_SKIP_WAITKEY` is not needed.

The whole game is a single room (a candy store) and one goal: fill Max's
Easter basket with the eight items on his list. It is a One Room Game Comp
entry, so there is no map, no NPC to follow and no failure state — only the
eight-item collection puzzle and a pair of optional ciphers.

## Route

The committed solution *is* the author's own `EasterWalk.txt`, replayed
command for command, with only its two non-commands dropped: `about` (a
paratext dump) and `verbose` (an ADRIFT Runner menu item, not something you
type). Everything else, including the examines, is kept — the shipped
walkthrough records the real Runner's reply to each of them, which makes the
golden a genuine ground-truth oracle rather than just a win check. See
[Runner divergences](#runner-divergences) for what that oracle turned up.

Max's list, handed over by the shopkeeper the first time you `show basket to
shopkeeper`:

| item | how |
| --- | --- |
| a lollipop | on the newspaper rack — `take lollipop` |
| a creme egg | from the pinata |
| tiny chocolate eggs | from the pinata |
| yellow marshmallow chicks | from the pinata |
| jellybeans | from the pinata (`a sack of jelly beans`) |
| a macaroon | `ask shopkeeper about macaroon` — he writes "Max" on it in icing |
| a candy coin | in the pay phone's coin return — `take candy coin` |
| a gumball | `insert token in gumball machine`, token from under the goose |

Three things gate the rest:

1. **The pinata.** `x rabbit` → `x plate` → `read plate` spells out "Look up"
   in butterscotch. `look up` reveals the pinata; without it `hit pinata` gets
   "You're going to need some kind of tool…". The tool is the umbrella from
   the umbrella stand, and it takes **three** swings: sway, then a strip of
   candy dots, then JACKPOT and the four pinata candies on the floor.
2. **The goose.** `ask shopkeeper about goose` reveals she likes candy dots;
   `feed candy dots to goose` makes her stand up off the bronze token. The
   candy dots work straight out of the basket — no need to take them back.
3. **The macaroon** is behind the display-case glass. Asking the shopkeeper
   for it is the only way in.

`show basket to shopkeeper` with all eight items present ends the game.

### The two ciphers

Both are pure flavour — neither is needed to win, and neither has an in-game
use:

- `read serving tray` — "One code's as easy as A-B-C, / The other uses phone
  digits, 1-2-3."
- `read note` (in the pay phone) — `1.19.11.19.8.15.16.11.5.5.16.5.18.6.15.18.13.1.3.1.18.15.15.14`,
  A=1 → *ask shopkeeper for macaroon*.
- `read pixie dust` — `/3../3./3./3/__/2../2/6./3/9../__/3/6../8/7../__/8/6../__/4/6../6../7../3./.`,
  a phone keypad code.

### Minimal route

The game can be won in **26 commands** with every examine dropped:

```
take lollipop / put lollipop in basket / look up / take umbrella /
hit pinata with umbrella ×3 / take dots, egg, eggs, chicks, beans /
put each in basket / show basket to shopkeeper /
ask shopkeeper about macaroon / put macaroon in basket /
take candy coin / put candy coin in basket / feed candy dots to goose /
insert token in gumball machine / put gumball in basket /
show basket to shopkeeper
```

`read plate` is not a prerequisite for `look up`, and
`ask shopkeeper about goose` is not a prerequisite for feeding her. The
committed solution keeps both because they are in the author's walkthrough.

## Runner divergences

`EasterWalk.txt` is a real `run400.exe` transcript, so lining the golden up
against it command by command is a free fidelity audit. Every reply matches
verbatim except for the four items below.

### Container listings — FIXED (`sclibrar.cpp`)

The Runner has two styles for listing what is inside a container, and which
one it picks was recorded in `lib_list_in_object()` as "frankly, a mystery".
It is not a mystery: run400 selects purely on the **number of contained
objects**. In the listing helper at `0006A418` in `~/Desktop/run400.txt`,
`var_98` is the count of objects whose position is 246 (in object) and whose
parent is this container, and then

```
0006A49E   var_98 == 1 && var_9E == 0  ->  "<obj> is inside <cont>."
0006A607   var_98 == 2 && var_9E == 0  ->  "<a> and <b> are inside <cont>."
0006A786   otherwise                   ->  "Inside <cont> is <list>."
```

One or two objects get the postfixed form, three or more the prefixed one,
and **nothing anywhere in that chain tests whether the container is static or
dynamic**. Scarier had been using the postfixed form only for *dynamic*
containers holding exactly one object, so this game showed it up four
different ways in a single session:

| container | items | Runner | scarier (before) |
| --- | --- | --- | --- |
| umbrella stand (static) | 1 | An umbrella is inside the umbrella stand. | Inside the umbrella stand is an umbrella. |
| pay phone (static) | 2 | A crumpled note and a candy coin are inside the pay phone. | Inside the pay phone is … |
| wallet (dynamic) | 2 | A few bills and a couple of photographs are inside your wallet. | Inside your wallet is … |
| Easter basket (dynamic) | 6 | Inside the Easter basket is a strip of candy dots, … | *(same — already right)* |

`lib_list_in_object()` now counts and picks on `count == 1 || count == 2`.
(The old part-of-NPC test is kept as an extra alternative, so containers worn
by or attached to an NPC keep the format they had; it can now only matter at
three or more objects.)

This re-blessed **37 goldens**, and two of the rewrites are independent
confirmation that the new rule is the right one — they are places where the
*author's own ALR* only matches the postfixed phrasing, and so had never
fired before:

```
yak_shaving:  Inside the pile of snow is a pair of chopsticks.
           -> Sticking out of the pile of snow are a pair of chopsticks.

              You open the seat.  Inside the seat is a hairdryer.
           -> You lift the seat to reveal a concealed storage area. The only
              thing it contains, apart from a few dust-bunnies, is an
              electric hairdryer.
```

One more golden outside this corpus moved: `test/adrift4/harness/capacity_nest_expected.txt`,
whose `n22` holds two objects. Its Runner-verified content is the *capacity*
refusal wording, not the listing wording, so the rephrase is safe there too.
Full `make -f Makefile.headless test` after the change: v4 **129/129 PASS**,
both capacity probes PASS, a5 suite untouched.

### `take` from the floor prints "You pick up" — OPEN

For an object lying loose in the room, run400 answers `take egg` with
**"You take the creme egg."**; scarier answers "You pick up the creme egg."
Both agree on the container/surface case ("You take the lollipop from the
newspaper rack.").

run400 carries two separate take handlers with two separate message
templates:

- `0007B652` — matched on `get`/`take`/`pick` and *not* `from`, building
  `<person> pick up <list>` and "There is nothing to pick up here.";
- `00073402` — building `<person> take <list>[ from <parent>].`, with
  "Take what?" and "There is nothing worth taking here.".

Scarier's `lib_cmd_take_*` mixes the two: "You pick up …" with no parent,
"You take … from …" with one. Which handler run400 actually reaches for a
bare `take`, and what it does for `get`, cannot be settled from the listing
alone — it needs a live `get X` / `take X` pair in the Wine Runner. Note also
`RUNNER_TESTS_TODO.md` §"Single-object library success vs failing explicit-verb
task", where run390 was probed live and printed **"You pick up the rock."**
for `take rock`, so this may be a 3.9-vs-4.0 split rather than a get-vs-take
one. 37 goldens carry 128 "You pick up …" lines, so this is not a change to
make on inference. **Left as-is; documented only.**

### `g` does not echo the command it repeats

The Runner answers `g` with `(hit pinata with umbrella)` on its own line
before the response; scarier prints only the implicit-tool line
`(with umbrella)`. The *semantics* are identical and correct — see
`scare-g-means-get`, where the Runner's Auto complete once faked a `g`
divergence that was not there. Only the echo is missing.

### Ambient events land on different turns

The shopkeeper's unprompted lines ("You must be shopping for somebody pretty
special", the wind-up chick, the little girl outside, the "don't forget to
look up" nudge) appear in both transcripts but on different turns. They are
randomised repeating events, and the harness `scare` binary links a fixed-seed
PRNG shim, so they cannot line up with a 2006 Runner session. Not a
divergence to chase.
