# Menagerie! (`circus.taf`) — ★ WON 64/140 (deterministic, seed 2)

David Good's **"Menagerie!"** (v1.03, 2001; WON 1st place, ADRIFT Spring 2001
Minicomp). Native **ADRIFT 3.90.17**. You are **Willow Murphy**, a PETA spy sent
to the Waleri Bros. Menagerie & Circus to **film animal cruelty on a videocamera
without getting caught**. 18 rooms, 158 tasks, 16 NPCs, 18 events. Difficulty
(Easy/Medium/Hard, Medium default) gates timers + the stored max (Easy max = 140).

**Status: ★ SOLVED — deterministic full WIN banked** in
`harness/circus_solution.txt` (run with `SC_SEED=2`; ending *"...PETA plants a
Willow tree in your name"*). The economy that had this parked is closed by the
**toy-knife chain** (below); NPC timing is handled by *spam-until-present*.

## The winning route (annotated)

1. **`Easy`**, `open case`. Bootstrap the **ticket**: `n`→entrance, `buy 4 tokens`
   ($2→4 tok), `s`,`e`→duck pond, **`play duck pond` ×4** (the ticket is always the
   4th duck — a counter, not RNG; +10) — winning it triggers the **pickpocket** that
   scatters your kit (camera→trunk, tape→lion cage, battery→Platform).
2. **Toy-knife economy** (the key to affording everything): `w`,`n`→entrance,
   `sell 10 points` ($1), then **`buy peanut`** (repeat until vendor **Joe** wanders
   in; he's deterministic-but-mobile). `ne`→East Bleachers, **`look under bleachers`**
   (+5, opens the `down` exit), `down`→under-bleachers, **`give peanut to pringles`**
   (the monkey hands over the **toy knife**, +10), **`take knife`**, `up`, **`sw`**→
   entrance (compass is rotated — *prose says SW = main entrance*), **`sell knife`**
   (Marie pays **+$2 +5pts**). `sell 10 points` ×2, **`buy 6 tokens`** (now 6 tok).
3. **Mirror + reading**: `s`,`w`→funhouse (−1 tok; **Bill stays here** unless you ever
   give popcorn to Zap, so this is safe), **`show mirror to bill`** (+5, a hard
   prereq for the reading). `e`,`e`,`e`→fortune teller, **`ask reading`** (−4 tok,
   +6) — deals **three tarot cards** = the trunk combo.
4. **Combo is randomised**: the cards' Roman numerals are the lock numbers (re-read
   with `x first/second/third card`). **Under seed 2: XIII / IX / V → 13, 9, 5.**
5. **Trunk**: `w`,`w`,`n`,`n`,`n`,`n`,`e`→Clown Dressing Room, **`turn lock 13`**,
   `turn lock 9`, `turn lock 5` (*"click as the lock opens"*), `open trunk` (+10),
   **`take videocamera`**.
6. **Tape**: `w`,`n`→Animal Cages, **`ask barb about tape`** (repeat until the lion
   tamer **Barb** wanders back in; she reaches into the lion cage and hands it over,
   +10). **Never `open` a cage = instant death.**
7. **Battery**: `s`,`s`,`s`,`u`→Platform, **`take battery`** (**never `jump` = death**).
8. **Load + film + win**: `put battery in camera`, `put tape in camera` (the camera is
   a container — it must be *loaded*, holding the parts isn't enough), `d`,`n`,`n`,`n`→
   Animal Cages, **`use camera`** (+10, sets `videodone`), **`home`** → **WIN** (+20
   tier; Willow is hit by a circus truck on the way out but the footage reaches PETA —
   the author's intended bittersweet victory).

Everything below is the structural decode (verified against the SCARE dump/trace).

## Determinism caveat (IMPORTANT)
The funhouse is an **RNG death trap**: `fundeath` (var12) = `random(0,10)`, set once
at `#initialize` and re-rolled only on each *successful* funhouse `west`. The funhouse
`west` is **task 19** (safe, needs `fundeath != 1`) vs **task 20** (death, fires when
`fundeath == 1`: +10 then `ACT type=6 v1=2` = "Skippy the clown ... heart attack").
Under the harness's **default seed 1, `fundeath==1` → the game is UNWINNABLE** (every
first funhouse entry kills). `fundeath==1` happens for ~1/11 seeds (seeds 1, 9, 17,
25, 33...). **Use `SC_SEED=2`** (fundeath=8) or any non-1-mod-8-ish survivable seed.
The harness `seed.c` now reads `SC_SEED` (default 1); this is the analogue of
Shadow_Of_The_Past's beast gamble — a real player saves before `west` and retries.

## The win (fully decoded)
Win = type-6 `v1=0` (WIN) on `*home*`/`*car*` — **tasks 48/49/50**, three score tiers,
all requiring **`videodone` (var22) == 1**:
- **task 48**: `videodone==1` AND `thescore (var31) >= escore1 (var51=120)` → +20, best.
- **task 49**: `videodone==1` AND `escore2 (var52=90) <= thescore < escore1` → +20, mid.
- **task 50**: `videodone==1` AND `thescore < escore2 (90)` → WIN, lowest tier (no +20).

So the **core win = set `videodone`, then go `home`** — `videodone` only ever becomes 1
via **`use camera`** in room 4 (Animal Cages), tasks 31/32/33 (each +10 score, +10
`thescore`, sets `videodone=1`). `thescore` only changes the ending tier. Filming is
repeatable, so for the top tier film ~12×; for a bare win, film once.

> Restriction-index gotcha: in SCARE, **restriction** variable index = `Var1 - 2`
> (so a win restr printed `v1=24` = var22=`videodone`, `v1=33` = var31=`thescore`);
> **action** variable index = `Var1` directly. `v3` in a var-vs-var compare references
> variable index `v3-1`. (Decoded from `screstrs.c` / `sctasks.c`.)

## Filming prerequisites — the theft scatter (task 34 `$pickpocket`, fires on winning the ticket)
Winning the duck-pond ticket (EVENT 2 → pickpocket) **scatters the kit**:
- **videocamera (obj0) → into the trunk** (container idx9 = obj41, in room5 Clown Dressing Room).
- **videotape (obj10) → into the lion cage** (container idx2 = obj4, room4).
- **battery (obj9) → room12 (Platform)** — just lying there, plain `take battery`.
- **case (obj1) → worn by NPC15 (Zap)** — only needed for filming tasks 32/33; **task 31
  films with just camera+battery+tape, no case**, so the case is optional.

Filming **task 31** `use camera` (room4) needs: hold videocamera, battery present,
videotape present, plus a `swords` (var57) condition (set by play, normally satisfied).

## Recovery chain (each step verified)
1. **Funhouse (room10)**, `west` from Midway (room9), costs 1 token, survive (seed!):
   `show mirror to bill` (task66, +5) — uses the "wacky mirror" already in the funhouse;
   Bill (NPC8) is there. **This is mandatory**: it is a hard prerequisite of the reading.
   (`take money` = task67 needs **Bill NOT in the room**, and *nothing moves Bill* — so
   the funhouse "$5 tip" appears **unobtainable**; do not count on it.)
2. **Fortune Teller (room11)**, `ask reading` (task4, +6): costs **4 tokens**
   (`readingcosts` var63=4) and **requires task66 done**. Deals three tarot cards
   whose numerals ARE the trunk combo (randomised per game; re-read with
   `x first/second/third card`). The reading resets combo vars 27/28/29 to 0 — they
   are repopulated by the card values (NOT a fixed 13/10/5; tasks 122–130 each set a
   combo slot from the card's number).
3. **Clown Dressing Room (room5)**: `turn lock <c1>` → `turn lock <c2>` → `turn lock
   <c3>` for the three card numbers (**seed 2: XIII/IX/V → 13, 9, 5**; the lock tasks
   82-90 require the reading done), then `open trunk` (task79, +10) → **take videocamera**.
4. **Animal Cages (room4)**: `ask barb for tape` (task77, +10) — Barb (NPC5, lion tamer)
   fetches the videotape out of the lion cage (do NOT `open lion cage` — instant death,
   task10). 
5. **Platform (room12)**: `take battery`.
6. Back to **room4**: `use camera` (task31) → `videodone=1` (+10). Repeat for score tier.
7. `home` / `car` → **WIN**.

## Map (EXIT dump)
```
room9  Midway(start) : N->0  E->15  S->16  NW->13  W->10(funhouse, 1 token)
room0  Main Entrance : N->2(bigtop, gate task13=ticket)  NE->1  SW->7  S->9
room15 Midway East   : E->11(fortune)  S->13  W->9  SE->16   (duck pond here)
room16 Midway South  : N->9  E->13  W->17(arcade)  NE->15
room13 Outdoor Stage : N->15  W->16  SW->9
room11 Fortune Teller: W->15
room1  East Bleachers : W->2  D->14(gate task93)  SE->0  SW->3   (look under bleachers +5)
room2  Center Ring    : N->3  E->1  S->0  W->7  U->12(Platform)
room12 Platform       : D->2     (battery here; `jump`=DEATH task9)
room7  West Bleachers : E->2  NE->3  NW->0
room4  Animal Cages   : S->8     (reached via room8 Backyard; filming room)
room5  Clown Dressing : W->8     (trunk here)
room10 Funhouse       : E->9
```

## The economy (the remaining grind)
- **Vars**: dollars=var4, **tokens=var5(`gametick`)**, `thescore`=var31, `videodone`=var22.
- Tokens: **2 per $1** from Marie (room0). Start `$2 = 4 tokens`.
- **Tokens needed = 9**: duck pond 4 (ticket is ALWAYS won on the **4th** play — it's a
  counter `played`, not RNG, seed-independent) + funhouse 1 + reading 4. **Deficit ≈ 5
  tokens ≈ $2.50** beyond the starting $2.
- **`sell N points`** (room0, Marie): 10 pts → $1 (linear, no bulk bonus). `sell 10/20/30/
  40/50 points` = tasks 147/149/150/151/152.
- Sellable point sources (all one-time unless noted), and their NET point value after costs:
  - ticket +10; `look under bleachers` (room1) +5; `show mirror to bill` +5 (−1 token ≈ net 0).
  - **Food pump**: `buy peanut`/`buy popcorn` ($1 each, +3) then `eat` (+10, **once each**,
    tasks 16/17/18) = **+3 net pts per food**; cracker costs $2. Requires the **wandering
    peanut vendor** to be present at room0 (deterministic per seed; he visits every few turns).
  - **Ring toss** (room9, `play ring`, tasks 29/30 +6 per win) costs 1 token: ≈ +0.2 token
    net per win — slow but repeatable.
- **What actually closes the gap (the solved economy):** the **toy-knife chain**, not the
  food pump. `buy peanut` ($1) → `give peanut to pringles` (the monkey in room14, +10) →
  `take knife` → `sell knife` to Marie for **+$2 and +5 pts**. Net **+$1 and +18 pts** —
  comfortably more than the deficit. Token granularity (each $1 = 2 tokens, but 9 are
  spent) forces buying **10 tokens / $5**; start $2 + knife $1 + selling ~30 points = $5.
  The food pump and `play wheel`/`give tip cecily` are all net-negative or break-even, so
  they're avoided.
- **NPC timing solved by spam-until-present:** Joe (peanut, room0) and Barb (tape, room4)
  wander deterministically; repeat the action across turns until they walk in (robust to
  turn drift). Pringles (room14) and Bill (funhouse) stay put — Bill only leaves if you
  ever `give popcorn to zap`, so never do that.

## Death traps to avoid
Opening ANY cage (`open lion/tiger/gorilla/elephant`, tasks 10/11/36/38) = death;
`jump` on the Platform (task9) = death (+10 then die); funhouse `west` with fundeath==1;
plus timed mad-clown / tightrope(guido) / lion-tamer / magic-act / stampede / timed-die.

## Banked solution
`harness/circus_solution.txt` (run with **`SC_SEED=2`**) is a complete deterministic
**WIN, 64/140**, verified via `play.sh`. Marker: *"...PETA plants a Willow tree in your
name with a brass plaque..."* / *"Congratulations. You completed the game"*. The combo
inside it (13/9/5) is seed-2-specific — for any other seed, read the three tarot cards
after `ask reading` and turn the locks to those numerals.
