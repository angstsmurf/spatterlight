# Sophie's Adventure — walkthrough

- **Engine:** ADRIFT 4.0 (David Whyld, IFComp 2003; ScummVM gameid
  `if03_sophie`). A very large fairy-tale quest: Sophie's birthday "treat"
  turns into a march across Grimmghoul with five dwarves and Arliss the wizard
  to destroy Brask, the evil skeleton lord.
- **Two builds are wired, and they are not the same game** (see
  [Which .taf](#which-taf)):

| build | bytes | solution | commands | result |
|---|---|---|---|---|
| `sa.taf` (later, `040104`) | 536710 | `harness/sophie_solution.txt` | 255 | ★ **WON**, **193** points, ending 3 of 5 |
| `sophie.taf` (IFComp release) | 531015 | `harness/sophie_comp_solution.txt` | 255 | ★ **WON**, **183** points, ending 3 of 5 |

Both goldens are blessed and both rows are in `run_v4_walkthroughs.sh`. Win
marker for each: `You have won.` Row env for each:
**`SCR_SKIP_WAITKEY=1`** — the game has dozens of `...press a key...` pauses and
the transcript is unreadable without it.

Everything below describes **`sa.taf`** unless it says otherwise; the comp
build's divergences are collected in
[The comp build](#the-comp-build-sophietaf) at the end.

## Which .taf

Both load now. Until 2026-08-02 the IF Archive comp build distributed as
`sophie.taf` (531015 bytes) **would not load under SCARE**:

```
scarier: parse_get_taf_integer: invalid integer at line 225749
scarier: parse_stack_backtrace: version 4.00 schema parsed to depth 5
scarier:  0 - [s] "Tasks"   1 - [i] 4489   2 - [s] "Actions"   3 - [i] 1   4 - [s] "Type"
```

**FIXED 2026-08-02 — it was a SCARE parser bug.** It is not truncated (it
decompresses in full, 3,186,839 plain bytes / 254,549 lines, ending on the normal
`Trebuchet MS, 13` / `26 Sep 2003` trailer), and the real `run400.exe` under Wine
loads that exact file (md5 `b2ebc41262384db587533ed547a6220f`) and plays it — `1`
gets you into Sophie's Room with the normal description and `Exits lie: east`.

### The actual defect

Plain-text lines 225745–225752 are the tail of the `cast *summon*` /
`cast *divine*` / `cast *ally*` task, and the record is a **task restriction**,
not an action:

```
225744  ''          $Question
225745  1           V<TASK_RESTR> count
225746  12          Restr[0].Type   <- documented range is 0-4
225747  7667826     Restr[0].Var1
225748  7209070     Restr[0].Var2
225749  7471205     Restr[0].Var3
225750  ''          Restr[0].$FailMessage
225751  0           V<TASK_ACTION> count
225752  '#'         $RestrMask
```

The v4.0 `TASK_RESTR` descriptor in `sctafpar.cpp` enumerates only types 0–4, and
the schema language's `?` test supports **`=` comparisons only**
(`parse_test_expression`), so there is no `Case Else`. Type 12 therefore matched
nothing and consumed **no** Var fields. The parse slid three lines: `FailMessage`
became `'7667826'`, the actions vector count became `7209070`, and it died two
reads later on `''` — reported as `invalid integer` at
`Tasks/4489/Actions/1/Type`. The stream re-syncs on its own at the next `#`,
which is why the damage looked so localised.

### What the record actually contains

The three values are the four-byte little-endian halves of a UTF-16 string, and
the "Type" is its byte length:

```
12 bytes:  72 00 75 00  6E 00 6E 00  65 00 72 00   ->  "runner"
```

So a stray `runner` string blob was written over the restriction — real damage in
the 2003 release, but damage the Runner shrugs off because it consumes three Vars
for an unrecognised type. This is also why `sa.taf` (the later build) is clean:
it has no such record.

### The fix

`sctafpar.cpp` gains a v4.0 fixup, `|V400_TASK_RESTR:Type>4?#Var1,#Var2,#Var3|`,
plus a `parse_fixup_v400()` handler (the v4.0 arm of `parse_fixup()` used to be
`scr_fatal("unexpected call")`). The three-Var count is **inferred from this one
sample** — instrumenting the fixup showed it firing exactly once on the comp
build and **never** across all 99 `.taf` files in `games/`, so nothing else in
the corpus constrains it. Suite immediately after the change: **94/94 PASS**, no
golden changed; **96/96** once the comp build got its own row.

## Repairs to the archived walkthrough (for `sa.taf`)

The archived `walkthru.txt` is 237 commands and does not finish. Eight distinct
repairs were needed; the final route is 255 commands. (It was written against
the *comp* build, which is why so much of it misses — the comp build needs its
own, different set of repairs, listed
[at the end](#the-comp-build-sophietaf).)

| # | Where | Problem | Fix |
|---|-------|---------|-----|
| 1 | Shamuel's | `get all` | three explicit `get`s (`crucifix`, `holy handgrenade`) |
| 2 | Skull Clearing | `get all` | five explicit `get`s (`thimble`, `belt`, `raw meat`, `whistle`, `spike trap`) |
| 3 | Top Of Steps | `s` goes nowhere | `d` (to the Short Corridor) |
| 4 | the crypt | `get glowing crystal` | `get dark crystal` (it only glows once recharged in the lava) |
| 5 | before the devil | `talk to devil` refused | insert `open door 5` first |
| 6 | after the Gallery | `get all` | four explicit `get`s |
| 7 | the Gallery | a spurious second `n` | delete it (Benthem appears only after `nw`/`se`) |
| 8 | Chamber of Battle | "not enough spell energy" | insert `nw` / `sleep` / `se` to recharge |

**`get all` is a hard-blocked joke command.** The game answers three escalating
refusals and the third disables it permanently, so it can never appear in a
route. This is what silently broke the later `throw meat` and `throw spike trap`
steps: the items were never picked up.

## The endgame — what the archived walkthrough is missing entirely

The archived route reaches Brask and then loses. Beating him needs two things it
never does.

### 1. Give Dimm the orb

Dimm the skeleton — Brask's brother — stands in the **Shadowy Hall** and asks you
to fetch the orb Brask binds his subjects with. The orb is on the desk in the
**Small Study**, right where the archived route already goes (it picks the orb up
without ever noticing). You have to carry it **back** to the Shadowy Hall:

```
e / e / d / w / w        <- Small Study -> Crumbling Passage -> Empty Hall
                            -> Portraits Room -> Art Room -> Shadowy Hall
give orb                 <- +15 points, and Dimm teaches you the Words of Power
e / e / u                <- back to the Empty Hall
nw                       <- the invisible trap -> Skeleton Lord's Domain
```

`give orb` is task 5084, gated on the orb being visible to you; its reward sets
the variable `meteor strike` and is the **only** way to unlock the spell. Note
that `nw` is not listed by the `exits` command in the Empty Hall — it is the trap
that drops the whole party into Brask's throne room, and there is no way back.

### 2. Do more than 39 points of damage in one visit

The Domain has no exits. Every turn there, event 10 *check for brask damage*
runs task 4577 `+ brask dead!!!`, which fires when the variable `brask damage`
is **> 39**; it moves you to room 122 *End Game*, adds 20 points and picks one of
five ending tasks by score.

The route ends:

```
meteor strike            <- +20   (only available if you gave Dimm the orb)
cast implosion           <- +15
raise crucifix           <- +5    = 40, and Brask shatters
```

Damage values, in the order the game hands them out (each spell has a chain of
tasks; the first cast is the good one and later casts do less and cost niceness):

| action | dmg | note |
|---|---|---|
| `meteor strike` | 20 | needs the orb given to Dimm; once only |
| `cast implosion` | 15, then 5 | the second cast costs 5 niceness |
| `throw handgrenade` | 20 | **already spent** — it is the required kill for the evil skeleton in the Shadowy Glade |
| `cast fire blast` | 6, then 3 | first cast costs 2 niceness |
| `raise crucifix` | 5 | destroys the crucifix |
| `kill brask` | 1, 1, 1 | then the fourth attempt is fatal |

`cast summon ally`, `cast invisibility`, `cast shield` and `cast super strength`
all exist as combat tasks but answer *"You haven't learnt that spell! Stop
cheating!"* on this route — Sophie only ever learns implosion, fire blast and
(via Dimm) meteor strike.

**Spells are free inside the Domain.** Elsewhere each cast deducts from the
`energy` variable, but the room-109 combat tasks *add* the same amount back
(`cast implosion` there is `energy += 12`), so the fight is not energy-limited.
Sophie walks in with 18 energy and comes out of the first implosion with 30.

## Endings

Five ending tasks in room 122, selected on the `scor` variable: >300, >229,
>170, >120, else worst. This route finishes on **193** (173 before Brask, +20 for
killing him), which is **ending 3**. The score comes almost entirely from
optional side quests — talking to everyone, Snordy's quests, the items in
Sophie's house — so a fuller route would land a better ending. The game is
completable at any score; the endings only change the epilogue and (per the
epilogue itself) what is available in the sequel, *Sophie's Quest*.

## Notes

- **Niceness is a losing condition.** Task 4722 `- run out of niceness` fires in
  the Domain when `nice` hits 1. The route above spends none: the only
  niceness-costing options are repeat casts and `kill brask`.
- `crystaldone` (hands out all five crystals) and `giveall` (hands out every
  combat item plus 30 energy plus meteor strike) are **author debug commands**
  left in the released game. Neither is used here.
- **Don't try to read mid-run state with the debugger in the headless build.**
  `SCR_DEBUGGER_ENABLED=1` opens the debug dialog *before the first turn*
  (`debug_game_started()`), and there is no `#debug` game command in the ANSI
  front end — so a piped `{ head -N solution; echo '#debug'; echo 'variables *'; }`
  silently reports the game's **initial** variables (and every walkthrough line
  gets eaten as a debugger command; `e` becomes `Event`). Re-entry mid-game is
  only via `step` or a watchpoint. For live state use `SCR_TRACE_TASKS=1` and
  read the `Task: variable N (name) += x` lines.

## The comp build (`sophie.taf`)

- **Solution:** `harness/sophie_comp_solution.txt`, 255 commands.
- **Result:** ★ **WON**, **183 points** (163 walking into the Domain, +20 for
  Brask), ending 3 of 5 — the same `>170` band as `sa.taf`.

The archived `walkthru.txt` (237 commands) was written against **this** build,
not `sa.taf`, so it fits better here — but it still does not finish. Seven line
edits plus a rewritten endgame get it home:

| # | Where | Problem | Fix |
|---|-------|---------|-----|
| 1 | Shamuel's | `get all` | `get crucifix` / `get holy handgrenade` |
| 2 | Skull Clearing | `get all` | `get thimble` / `get belt` / `get raw meat` / `get whistle` / `get spike trap` |
| 3 | Top Of Steps | `s` goes nowhere | `d` (to the Short Corridor), then the `s` |
| 4 | the crypt | `get glowing crystal` | `get dark crystal` |
| 5 | the Chamber | `get all` | `get crucifix` / `get thimble` / `get belt` / `get whistle` |
| 6 | the Gallery | a spurious second `n` | delete it (Benthem appears only after `nw`/`se`) |
| 7 | before the Chamber of Battle | "not enough spell energy" | insert `nw` / `sleep` / `se` |
| 8 | the whole endgame | missing | see below (+14 commands) |

Repairs 1/2/5 are the same `get all` joke-command trap as in `sa.taf` — three
escalating refusals, the third disabling it for good — and it silently broke the
later `throw holy handgrenade at evil skeleton` and `get torazin crystal` steps.
Repair 3 alone cleared five downstream failures (`get chisel`, `open panel`,
`get snowball`, `get sword`, `get wine`).

### The endgame — Kridlor, not a desk orb

`sa.taf` leaves the orb lying on the study desk. Here you have to earn it, and
the archived route stops one whole quest short.

```
                            <- Gallery, after the Benthem fight
nw / sleep / se             <- the Corridor is the only place to recharge
n / n                       <- Chamber of Battle: the second `n` springs the trap
cast implosion              <- one cast kills all fifteen suits of armour;
                               Snitch finds a BELL in the rubble
s / s / e / e / u / w / w   <- Gallery -> Shadowy Hall -> Art Room ->
                               Portraits Room -> Empty Hall -> Crumbling
                               Passage -> Small Study
read scroll                 <- Kridlor's notes on resurrection by burning ashes
cast fire blast             <- torches the bowl of ashes; KRIDLOR'S GHOST rises
1                           <- "Is there anything I can do to help?" (optional)
e / w                       <- THE HAND-IN. Leaving and re-entering with all
                               three items is what triggers it
                            <- Kridlor takes them, welshes on the reward,
                               vanishes -- and leaves the ORB on the desk
e / e / d / w / w           <- back to the Shadowy Hall
give orb                    <- Dimm teaches you the Words of Power
e / e / u                   <- Empty Hall
nw                          <- the invisible trap -> Skeleton Lord's Domain
meteor strike               <- 20
cast implosion              <- 15
raise crucifix              <-  5   = 40 > 39, and Brask shatters
```

Three details cost the most time:

- **The three quest items are already yours** by the time Kridlor asks. The
  **bell** drops from the Chamber of Battle armour (task 4469 `- armour
  defeated`), the **compass** from killing Benthem in the Gallery (task 4438
  `- benthem gone`), and the comb earlier in the route. Kridlor's menu option
  `1` only narrates the quest — the hand-in fires whether or not you ask.
- **The hand-in is a movement, not a `give`.** `give bell to kridlor` (and comb,
  and compass) all answer *"Kridlor the ghost doesn't seem interested."* The
  real trigger is task 4470, `w` in the **Crumbling Passage** while holding all
  three; it strips the items, grants `obj800=[orb]`, adds 10 to `scor` and moves
  you into the Small Study. So you step `e` out and `w` straight back in.
- **`w` in the Shadowy Hall is a red herring.** The room advertises "west (to
  the dark alcove)" and the alcove looks exactly like the sort of place a task
  would hide, but it just drops you into the Wine Cellar. Room numbers, from
  `SCR_TRACE_PLAYER=1`: Shadowy Hall **92**, Art Room 94, Portraits Room 95,
  Empty Hall 96, Crumbling Passage **118**, Small Study **119**.

`sleep` ("you awaken, find your magical energies recharged") is the only
recharge in the fortress, and the Corridor northwest of the Gallery is the only
room you can reach with a spare turn to use it — once the Chamber of Battle's
grills come down there is no way out and no way to cast.
