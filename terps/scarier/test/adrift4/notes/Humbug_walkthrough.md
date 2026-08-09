# Humbug — walkthrough (**WIN, 2000/2000 — full score**)

- **Game:** *Humbug* (Graham Cluley, 1990–1997), **converted to ADRIFT by
  Campbell Wild** from "Version 5.0 (r2)". You are Sidney Widdershins, spending
  Christmas at Grandad's crumbling Attervist Manor; the job is to find six
  gemstones, save Grandad, and see off Jasper Slake, the evil dentist next door.
- **Engine:** **ADRIFT 4.00** (`xxd -l 16 games/humbug.taf` →
  `… c2 cf 93 45 3e 61 …`).
- **Result:** **WIN**, ending on *"I give Grandad a friendly hug and he smiles at
  me: We've won!"* and
  `You scored 2000 out of a possible 2000 and managed to complete 100% of this
  adventure.  Grandad would probably describe you as a winner.. or a cheat.`
- **Score: 2000 out of 2000 — every point in the game**, banked in 125 scoring
  actions.
- **Harness row:**
  `humbug_solution.txt|humbug.taf|Grandad would probably describe you as a winner.. or a cheat.|SCR_SKIP_WAITKEY=1`,
  **1048 commands**, PASSing golden.
- **Source:** `downloaded/Humbug_walkthrough.sol` — a 1152-line step-by-step
  solution by **pjg**, written for the *original* v5.0 game, not for this
  conversion.

## The conversion is astonishingly faithful

The interesting result here is a negative one. pjg's solution predates the
ADRIFT port and describes a different engine's parser, yet **all 125 of its
annotated `(N/total)` awards fire, in the same order, with the same deltas and
the same running totals** — every one of them, from `Hug Horace (10/10)` to the
final `Push button (280/2000)`, with zero mismatches. Campbell Wild's "not
guaranteed to contain all the details of the original, but is as close as I can
make it" is, on this evidence, an understatement.

What the .sol does *not* give you is anything the original's players could see
on screen and a text file cannot carry: turn counts, four numbers that have to
be read out of the game, and one plural noun.

## The five things that had to be re-derived

### 1. Every "(keep looking until …)" is a real turn count

pjg writes prose where the game wants patience. Three of these are
all-or-nothing, and one of them kills you if you get it wrong:

| The .sol says | Actually |
| --- | --- |
| *"keep looking until Grandad shows up, when he enters the club, follow him"* | 11 `Look`s, then `S` — a **two-turn** window |
| *"you must wait about 5 turns … you will win a prize"* | 7 `Look`s; `Open package` a turn later than it reads |
| *"wait until you hear the hunting horn … another 35 moves or so"* | 12 `Wait`s |
| *"wait for Jasper to loose a tooth in another 7 moves or so"* | tooth on the 4th turn after the toffee |
| *"you may have to wait 25 moves or so for Horace to take his tin out again"* | Horace's snuff cycle is exactly **10 turns** |

The **Golden Gulp bouncer** is the sharp one. He refuses *"Can't you read son?
No unaccompanied juniors"* on every turn but the two on which Grandad is
standing in the tunnel beside you. Probed against the engine: `S` after nine
`Look`s is refused (Grandad only walks in at the end of *that* turn), after ten
or eleven it works, after twelve it is refused again. Miss the two-turn window
and the door stays shut, taking the +10, the raffle token and the mystery
package (wanted 150 commands later for `Give package to hacker`) with it.

The raffle then runs on the game's clock, not yours, and ADRIFT resolves your
command before the turn's events: the announcement *and* the package arrive
together at the end of a turn, so the `Open package` that reads as if it
follows the announcement has to be issued one turn further on, or it is
`Open what?`.

**Horace's snuff tin** is the same shape of puzzle. He takes the tin out, snorts,
and pockets it again on a 10-turn loop; the paper aeroplane has to be thrown on
the turn the tin is *out*, because that is the only turn he will drop it and
bound off after the plane. Two footnotes the .sol gets wrong here: `Get tin`
is refused even while he is chasing the plane (*"Horace refuses to give me the
tin!"*) — but `Open it` / `Put pepper in tin` / `Close tin` all work on the tin
where it lies — and `Put tin on bench` is a no-op, because Horace picks the tin
up himself on his way back.

### 2. `Get sheet` → `Get sheets`

One letter, and the most expensive mistake in the file. The bed linen only
answers to its plural noun, so the .sol's `Get sheet` silently takes nothing;
five rooms later `Tie Dennis with sheets` fails with *"But I am not carrying any
sheets"*, which is easy to shrug off — and then, three rooms after that,
Dennis wakes up:

> Dennis the fireman runs towards me. He doesn't look very happy, "You!". He
> takes a swipe at me. With a groan I fall to the ground. … **Sorry.. you appear
> to be dead.**

(The other typo in the file, `Drop troch`, is harmless but is fixed to
`Drop torch` for a clean transcript.)

### 3. The combination door is a seven-segment display

The buttons in the neon tunnel are not a numeric keypad. `X buttons`:

```
There are eight buttons arranged in a strange pattern:
    4 3 5 2 7 1 6 0
Below the buttons there is a small liquid crystal display.
```

which is a seven-segment digit with the confirm key off to the side:

```
      4          top
    3   5        top-left, top-right
      2    7     middle          (7 = confirm)
    1   6        bottom-left, bottom-right
      0          bottom
```

Each button toggles its segment; `7` commits the shape as a digit. **The
segments are not cleared when a digit is committed** — the display flickers,
the lamps stay lit. So each digit is entered by pressing the *symmetric
difference* between the segment set already lit and the one you want. Reset it
every time and the second digit comes out as garbage.

The code itself is the aardvark's. Wake it, feed it termites, and hand it the
feather it has been miming for — it dips the feather in its ear wax and scrawls
`HEL3761` on the wall. `HEL` is already on the display: the door wants `3761`.

The route also keeps the .sol's `Read display` after every commit. It is not
just narration — it is a turn, and the three turns it costs are what keep
**Schrodinger the cat** on the schedule the mouse puzzle needs a hundred
commands later. Dropping them shortens the route by three turns and the cat is
in the wrong room when the sandwich goes down.

### 4. Four numbers, and a word, that only exist inside the game

| Placeholder in the .sol | Value | Where it comes from |
| --- | --- | --- |
| `<first…fourth decimal digit from slate>` | `3` `4` `4` `6` | the slate reads **MMMCDXLVI** |
| `<number from filofax>` | `010473736401` | *"Viking Contact Society"*, in green ink |
| `<insurance number>` | `60318897` | Olaf recites it — but only after `Pop balloon` cures his hiccups |
| `<telephone number>` | `010473470651` | the computer displays it once the first two numbers are typed |
| `<magic word>` | `Jisanajen` | the runes, read **through Grandad's monocle** |

The three computer lines and the `Say` are worth **70 points** between them, and
they are the only points in the game a walkthrough reader cannot simply copy.
Skipping all four still *wins* — the endgame does not depend on them — but it
ends at **1930/2000**, *"a partially-nibbled tortilla"*.

### 5. Two small ordering repairs

* `Get sandwich` needs one `z` after it: the cat has to be in the room before it
  will follow the sandwich down the stairs.
* `Get mouse` needs a third `Look`: two are not enough for the cat to catch it.

## The route, section by section

The command file follows the .sol's own 23 sections. Running score at the end of
each:

| # | Section | Score |
| --- | --- | --- |
| 1 | The beginning — Horace, the petrol, the letterbox, the chimp | 170 |
| 2 | The Time Chair | 200 |
| 3 | The Gravedigger | 230 |
| 4 | The Time Case | 270 |
| 5 | The Octopus — the runes, `Say moccasin beehive` | 340 |
| 6 | The Hacker | 370 |
| 7 | The Maze and Rabbit — `Dig` = **+100**, the biggest award before the finale | 520 |
| 8 | The Aardvark and Combination Door | 580 |
| 9 | The Fish in the Wine Barrel | 640 |
| 10 | The Viking Ship, Sealion, and Ticket | 680 |
| 11 | The Caddy — Schrodinger, the sandwich, the mouse | 750 |
| 12 | The Owl, Skylight, and Slug | 870 |
| 13 | Grandad and the Nightclub | 910 |
| 14 | The Mist Robot | 940 |
| 15 | The Coffin | 1010 |
| 16 | The Wumpus — and Jasper's tooth and polaroid | 1170 |
| 17 | The Rucksack — Olaf, the hacker, the computer | 1270 |
| 18 | The Powder and the Fairy | 1410 |
| 19 | The Fire Alarm — Dennis | 1500 |
| 20 | The Gems and the Chutes — six levers, then `Say Jisanajen` (+50) | 1610 |
| 21 | The Bear Cub | 1640 |
| 22 | The Petrol Can — Horace's snuff | 1680 |
| 23 | Grandad and the Bath — `Push button` (**+280**) | **2000** |

## Footguns worth remembering

* **`SCR_SKIP_WAITKEY=1` is mandatory.** The ASCII-art title screen ends in
  `[Press any key]`, which eats the first two commands of the script and
  desyncs everything after it; the run still *finishes*, at 1930, which is
  exactly the sort of near-miss the win marker exists to catch. The marker on
  this row quotes the full-score rank line, so it guards the score too.
* **You cannot leave the mist room with white hair** — the hair has to be washed
  out at the manor first (`Wash hair`, +10), which is also why the whole Wumpus
  section detours back up to the bathroom.
* **Horace has to be "bothered"** into taking you to his hut: two items dropped
  in the maze, no fewer.
* The polaroid must be **carried** at the final `Push button` — it is what
  routs Jasper — and it is dropped and re-taken twice on the way, most easily
  missed at the `Drop all` after the gem chutes.

## Reproducing

```sh
cd terps/scarier/test/adrift4/harness
sh run_v4_walkthroughs.sh humbug          # PASS against the committed golden
```

The derivation harness used to build the list from the .sol lives in the
session scratchpad, not in the repo: a converter that strips the .sol's
parenthetical asides with a depth counter carried across line ends, plus ten
anchored repairs (the ones described above). Only the finished
`humbug_solution.txt` is committed.
