# Druggy Lane — walkthrough

- **Engine:** ADRIFT 3.9 (`druggy_lane.taf`, 12,028 bytes), Paul Boswell.
  Not a parser adventure at all: one room, no objects, no NPCs, 22 tasks and
  **23 variables** implementing *Dope Wars*. Repay a $25,000 debt in 30 days,
  starting with $5,000.
- **Result:** ★ **WON** — debt cleared, finishing with **$1,955,720,463**.
- **Solution:** `goldens/druggy_lane_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `You have managed to deal your way to freedom!`
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS`
  plus a measured price table (below).

## The machine

Everything happens in the one room; the room description *is* the trading
screen (six drugs with hold/price/paid-price, then DEBT, DAYS LEFT, CASH,
BANK). The commands are `buy N <drug>`, `sell N <drug>`, `take out N loan`,
`pay back N`, `deposit N`, `withdraw N`, `next day`.

`next day` (TASK 0) is the only thing that advances anything, and it does
exactly five things:

| Action | Effect |
|---|---|
| `%daysleft% - 1` | one day gone |
| `%debt% = (%debt%*36)/35` | **+2.857 %/day**, truncated |
| `%bank% = (%bank%*40)/33` | **+21.2 %/day**, truncated |
| six `type=3 v2=2` actions | re-randomise all six prices |

The end is checked every turn by a pair of `where=room` tasks fired from
one-turn repeating events:

- TASK 20 `#That's the end (win)` — `daysleft == 0` **and** `debt == 0`:
  sets `%total% = %cash% + %bank%` and ends the game as a win.
- TASK 21 `#That's the end (lose)` — `daysleft == 0` and `debt != 0`.

So **drugs still in your stash on the final day are worth nothing** — only
cash and bank count toward the total — and the debt has to be at exactly
zero when the clock runs out.

## The bank is the game's broken heart

21.2 %/day compounding against a 2.9 %/day debt means you never have to
trade at all. The simplest winning route is six commands long and needs no
knowledge of the price stream (and therefore no fixed seed):

```
deposit 5000
next day ×29
withdraw 60000 / pay back 56591 / deposit 60000
next day
```

$5,000 becomes $1,531,949 in the bank by day 1, the debt has only grown to
$56,591, and the game ends with **$1,535,358**. Verified; it is the route to
quote to a human player.

## The route in the golden: a measured trading plan

The committed solution does better by trading, because the daily price swings
routinely beat 21 %. It was derived like this:

1. **Fix a turn schedule.** Prices come from the engine RNG, and the stream
   advances per *turn*, so a plan is only reproducible if the number of turns
   per day is fixed. The golden uses exactly three turns a day —
   *liquidate / invest / `next day`* — with `look` as the filler when a slot
   is not needed.
2. **Measure the price table.** Run the same schedule with `look look next
   day` ×30 and scrape the 31 room descriptions. That gives every price on
   every day, and the debt series (25,000 → 56,591 at day 1 → 58,208 at day 0).
3. **Greedy is optimal here.** There is no stash capacity, no police, no
   random events, and all wealth is fungible, so each day you simply put
   everything into whichever asset has the best price ratio to tomorrow —
   `floor(cash/price_today) * price_tomorrow` for a drug, `(cash*40)/33` for
   the bank.
4. **Pay the debt on the last possible day.** Debt grows at 2.9 % while
   capital grows at 40 %+, so paying early is pure loss. The route pays
   $56,591 on day 1 and then plays the final `next day`.

The resulting allocation (asset held *overnight* on each day):

| Day | 30 | 29 | 28 | 27 | 26 | 25 | 24 | 23 | 22 | 21 | 20 | 19 | 18 | 17 | 16 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| | crack | speed | speed | bank | speed | hash | coke | speed | bank | crack | crack | speed | bank | weed | acid |

| Day | 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| | speed | acid | weed | crack | weed | crack | bank | weed | speed | crack | weed | crack | weed | cash | pay debt |

$5,000 → $1,955,777,052 by day 1; minus the $56,591 debt, the game prints
*"finished with $1,955,720,463"* (the odd $2 is truncation residue left in
the bank by a `deposit`/`withdraw` round trip on day 27).

## Why it stops at ~$1.96 billion

Unbounded greedy reaches **$7,061,712,858**, but ADRIFT 3.9's variables are
VB6 `Long`s — 32-bit — so anything past 2,147,483,647 would raise an Overflow
in the real Runner. Scarier's `scr_int` is a C `long` (64-bit here) and would
happily print the larger number, which is exactly the kind of result that is
*our* arithmetic rather than the game's. The plan is therefore capped: every
value the game computes, including the `%number% * %price%` intermediates,
stays under 2^31. Day 2 holds plain cash because every other move that day
would have crossed the ceiling.

## RNG footguns found here

- **Never use `wait` as filler.** `wait` runs `Globals.WaitTurns` turns, so it
  advances the RNG stream by more than one and every price after it shifts.
  The first version of this route used `wait` for the empty slots and ended
  the game $177,241 in the *red* while still "winning" — the engine does not
  check that cash is non-negative.
- **An unrecognised command is not a free turn either.** `xyzzy` consumes one
  unit of stream, and a *second* consecutive unrecognised command consumes
  none. `look`, `i`, and any successful task command each consume exactly
  one, which is why the schedule is built out of `look`.
- Every command in the golden succeeds; a *failed* buy or sell (restriction
  refused) would print its own message and is not interchangeable with a
  successful one for stream purposes.

## Other notes

- **`take out N loan`** (TASK 13) adds N to both cash and debt with no
  interest advantage — the debt multiplier applies to the whole balance, so
  borrowing is strictly bad unless you can out-earn 2.9 %/day, which you can.
  It is unnecessary: $5,000 is plenty of seed capital under this price stream.
- **You can lose by winning badly.** The final-day check only looks at the
  debt, so a player who sells nothing and holds a stash worth millions ends
  with whatever cash they happen to have.
- The game has no score — `score` answers *"Your score is 0 out of a maximum
  of 0"* — so `%total%`, computed by TASK 20 at the end, is the only measure
  of how well you did.
