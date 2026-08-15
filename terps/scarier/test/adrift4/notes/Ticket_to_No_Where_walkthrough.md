# Ticket to No Where — walkthrough

- **Engine:** ADRIFT 4 (`ticket.taf`, Richard Otter, InsideADRIFT Summer Comp
  2004). You have missed your train and have until 4.00pm to get to a meeting
  at Grantby. The whole game is a clock: one minute per turn across six and a
  half game-hours, so the route is turn-sensitive from end to end and most
  puzzles are conversations held while waiting.
- **Result:** ★ **WON, 110/110** — a genuine full score, and the same total the
  author reports. The dump's twenty-two awards sum to exactly 110 and this
  route collects every one.
- **Solution:** `goldens/ticket_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `You won and managed to score 110 out of a possible 110`. The ending line in
  full is `… and ended the game with £45.` / `Great score!`.

## Route

The author's own walkthrough, from the comp package as
`summercomp04.zip → competition/wthroughs/ticketwalkthru.txt`, replayed
verbatim — 324 commands, one per line in the solution file. Eight per line
here:

```
(blank line — intro waitkey)
x bin / get wire / north / ask ticket seller about grantby / x counter / x leaflet / read leaflet / inventory
read scrap of paper / read card / ask ticket seller about leaflet / west / x counter / open tea urn / get pipe / open till
get iou / open display unit / get stale donut / x table / look under table / get rattle / x notice / east
ask ticket seller about pipe / north / ask porter about pipe / ask porter about iou / ask porter about grantby / east / say hello to me / say hello to mrs peters
ask mrs peters about grantby / ask mrs peters about sister / ask mrs peters about husband / ask mrs peters about bill peters / x mrs peters / ask mrs peters about baby / ask mrs peters about rattle / give rattle to mrs peters
x vending machine / x slot / poke pound with wire / insert pound in slot / get chocolate / look under vending / look behind vending / get all
ask mrs peters about gazette / read gazette / ask mrs peters about horse racing / ask mrs peters about gamble / ask mrs peters about money / west / nw / x trainspotter
ask trainspotter about grantby / ne / east / ask station master about beard / ask station master about wallet / ask station master about theft / ask station master about business people / x desk
get notepad / ask station master about pipe / ask station master about iou / ask station master about grantby / give pipe to station master / get notepad / get note / get pen
open desk / read book / ask station master about book / get flowers / drop vase of flowers / get flowers / west / x payphone
read sticker / call 768555 / call 768555 / call 767123 / call 798654 / call 858585 / east / ask station master about passing through
west / sw / ask trainspotter about passing through / ask trainspotter about stopping train / ask trainspotter about note / ask trainspotter about notepad / ask trainspotter about pen / read note
ask trainspotter about beard / se / x mess / clean mess / south / south / get crumbled newspaper / read crumbled newspaper
ask bearded chap about wallet / ask bearded chap about meeting / ask bearded chap about bill peters / ask bearded chap about stationary / ask bearded chap about catalog / give catalog to bearded chap / ask bearded chap about catalog / give card to bearded chap
ask bearded chap about newspaper / 2. / n / west / get bag of shopping / x bag of shopping / east / north
x bench / look under bench / get all / read pamphlet / ask porter about pamphlet / south / south / drop crumbled newspaper in bin
time / north / get banana skin / south / put banana skin in bin / north / north / ask porter about young girl
ask porter about german / ask young girl about grantby / nw / give notepad to trainspotter / ask trainspotter about tights / get tights / give tights to trainspotter / ask trainspotter about gloves
get gloves / give gloves to trainspotter / ask trainspotter about young girl / ask trainspotter about german / where is young girl / se / ask young girl about flowers / give flowers to young girl
ask young girl about flowers / say donningby bahnhof umsteigen grantby to young girl / nw / ne / north / get manual / south / throw donut at pigeon
x train / east / ask station master about note / west / sw / se / get apple core / south
south / drop apple core in bin / north / north / nw / give manual to trainspotter / ne / east
give iou to station master / west / wait / wait / east / ask station master about iou / west / sw
se / ask porter about iou / time / ask porter about bill peters / ask porter about horse racing / ask porter about gambling / bored / nw
ask trainspotter about bill peters / ask trainspotter about john tailer / ne / north / get suitcase / south / sw / se
ask porter about suitcase / time / west / sw / se / x drunk / time / east
ask business man about cats / ask business man about john tailer / ask business man about beard / ask business man about theft / x business man / x left pocket / x right pocket / ask business man about pocket
ask business man about 768555 / open pet food / x betting slip / get bell / put food in pocket / get all / x credit card / read credit card
west / nw / ne / north / get parcel / x parcel / open parcel / read delivery receipt
get pencil / x pencil / snap pencil / drop pencil (broken) in parcel / close parcel / south / east / ask station master about parcel
give parcel to station master / ask station master about parcel / ask station master about pencil / time / inventory / west / sw / se
wait / wait / wait / wait / ask business woman about breakfast / ask business woman about cafe / south / west
get sandwich / east / north / give sandwich to business woman / ask business woman about chocolate / give chocolate to business woman / x coat / cover drunk with coat
get all / x bottle / use bottle on note / write colesworth on note / nw / ne / east / give note to station master
ask station master about note / give pen to station master / west / get wrapper / sw / ask trainspotter about wrapper / se / south
south / drop wrapper in bin / inventory / north / north / ask business woman about coat / bored / nw
ne / north / south / sw / (blank) / se / south / ask ticket seller about leaflet
north / get sock / south / south / drop sock in bin / north / north / nw
wait / ne / time / wait / wait / wait / say hello to john tailer / drop credit card
sw / get carton / se / south / south / drop carton in bin / north / north
nw / ne / get credit card / give credit card to john tailer
```

## Repairs to the author's transcript

`ticketwalkthru.txt` marks its commands with *nothing at all* — no prompt, no
prefix, no indent — so they had to be recovered from the surrounding prose by
heuristic (a short lowercase line after a blank one, not ending in punctuation).
Four escaped it and were put back by hand:

- `2.` — a stray line the author typed, which the Runner's echo then merged
  with the following `n` onto one transcript line, hiding both.
- `n` — the command merged onto that same line.
- `say donningby bahnhof umsteigen grantby to young girl` — too long for the
  length filter.
- one empty line the author entered by accident.

All four cost a turn, and on a one-minute-per-turn clock the game notices, so
all four are kept. The route wins only with them in place.

## Two engine fixes came out of this game

Both were verified live against `run400.exe` under Wine before being made:

- **`drop <thing> in <container>` is Adrift's put-in handler wearing another
  verb.** Five bits of litter go in the rubbish bin this way and are worth two
  points each. Before the fix the priority `drop %text%` pattern swallowed the
  `in bin` tail and the game answered `Drop what?`. Six new patterns now sit
  ahead of it in `PRIORITY_COMMANDS[]` covering `drop`/`put down` × `in`/`on` ×
  plain/`all`/`all except`. (100 → 108 points.)
- **`all` does not range over what the player already carries.** Holding the
  open bag of shopping and typing `get all` takes only the pamphlet on the
  bench and leaves the tights, pet food, deodorant and gloves in the bag — but
  a *named* take still reaches into a carried open container (`get paper` lifts
  the scrap out of the wallet). `lib_take_all_filter()` in `sclibrar.cpp` now
  excludes `obj_indirectly_held_by_player`. (108 → 110.) The visible symptom
  before the fix was `The banana skin is too heavy for you to carry at the
  moment.` — an over-full inventory, four items downstream of the real bug.

Two ALEXIS goldens moved with the second fix and were re-blessed after proving
the change correct there too: its leather bag is player-carried, so the Runner
would never have pulled the Forecarn stone out of it either.

## Scoring

Twenty-two awards, summing to exactly 110:

| Points | For |
| ---: | --- |
| 10 | `give credit card to john tailer` (the ending) |
| 10 | `say donningby bahnhof umsteigen grantby to young girl` |
| 10 | `put food in pocket` |
| 5 × 14 | the giving/using puzzles — rattle to Mrs Peters, chocolate to the business woman, pipe and IOU and pen and note to the station master, notepad and tights to the trainspotter, card to the bearded chap, flowers to the young girl, donut at the pigeon, coat over the drunk, `use bottle on note`, `write colesworth on note` |
| 2 × 5 | "extra for tidyness" — the five bits of litter dropped in the rubbish bin |

The five 2-pointers are the ones the `drop … in bin` fix unlocked; they are
what separates 100 from a full score.

## Notes

- **The clock is the puzzle.** One minute per turn from 9.30am; several tasks
  are gated on the time rather than on state, which is why the route contains
  bare `wait`s and `bored`s in clumps — they are load-bearing, not padding.
  An early attempt at debugging this route chased a "clock runs slow"
  hypothesis; tracing showed the clock is exact (task 28 fires at turn 105 =
  11:15 as designed) and the fault was in the command extraction above.
- `time` appears seven times in the route. It is the author checking the
  deadline, and each one is a real turn.
- Only one `<waitkey>` pause, at the intro; the leading blank line answers it.
  The later blank line in the file is the author's stray empty command, not a
  pause.
- One wording divergence noticed here — the Runner says `There is nothing worth
  taking here.` where SCARE said `There is nothing to pick up here.` **Settled
  and ported 2026-08-15**: it is a 4.0 rewording, not a cosmetic difference.
  run370/380/390 all say "nothing to pick up here" and only run400 says "worth
  taking", so the message is now version-gated (`lib_is_version_400()` in
  `sclibrar.cpp`), along with the take template it shares a handler with. See
  §4 of `RUNNER_TESTS_TODO.md`.
