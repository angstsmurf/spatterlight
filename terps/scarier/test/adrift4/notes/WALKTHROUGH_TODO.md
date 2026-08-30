# ADRIFT 4 walkthroughs — derivation COMPLETE

Original goal: produce a verified, reproducible, near-maximum-score walkthrough
for every `.taf` in `test/adrift4/games/`, in the style of
`Sun_Empire_walkthrough.md` — full command list, annotated phases, and an honest
note on any unreachable points and *why*. These are obscure 2000–2005 ADRIFT
comp games with no published walkthroughs (checked Key & Compass, IF Archive,
CASA); they were derived by driving each game through a headless, deterministic
Scarier build and reading its internals.

**Status 2026-08-04: done**, re-verified 2026-08-10. The suite was **203 rows,
203 PASS** — 0 FAIL, 0 SKIP, 0 NEEDGOLD, 0 NOSCRIPT, exit 0. Every game has a
route or a documented verdict, and every walkthrough in `downloaded/` has a game
and a row. This file is now the method, the cautions, and an index.

**Second wave 2026-08-10 → 2026-08-11: 22 more games wired, suite 225 rows,
225 PASS — then PARKED.** See *Second wave* below for what was done and what is
left.

**Third wave 2026-08-11: 6 more, suite now 231 rows, 231 PASS — PARKED again.**
Unparked to work the remaining v3.90 files smallest-first, and parked at the
user's request after *Where Is Richard?*. See *Third wave* below.

**2026-08-12: *Camp Windy Lake : Part 2* wired out of order (the user asked for
it by name), suite 232 rows, 232 PASS.** The first of the eight AIF holdouts to
go in, on the *Diary of a Stripper* terms — row committed, solution and golden
gitignored. See *Camp Windy Lake : Part 2* below.

**2026-08-12: *Salutations* wired, suite 233 rows, 233 PASS.** Back to the
smallest-first order the third wave was following — this is the 5,591-byte
4.00 file the sweep had skipped, a one-room Ectocomp 2008 speed-IF, WON in ten
commands with no score in the file at all. See *Salutations* below; its
`<waitkey>` lesson applies to every future row.

**2026-08-12: *A Day at the Iachini House* wired, suite 234 rows, 234 PASS.**
Next in the smallest-first order — 19,083 bytes, 4.00, a 27-room chore game.
**WON 115/115** in 170 commands, and the maximum is exactly reachable although
the file contains 140 points' worth of awards. See *A Day at the Iachini
House* below.

**2026-08-14: *La hija del relojero* wired, suite 235 rows, 235 PASS.** Next in
the smallest-first order — 21,775 bytes, 4.00, Spanish, one room, eight tasks,
no score anywhere in the file. **WON in eleven commands**, and three of the
eight tasks are provably dead: two are `Where` Type 0 and the third is
unmatchable because the game's own synonym table rewrites its verb. See
*La hija del relojero* below — and §4 of `../../../RUNNER_TESTS_TODO.md`, which
that game opened a new row in.

**2026-08-14: *Veteran Knowledge* wired twice, suite 237 rows, 237 PASS.** Next
in the smallest-first order — 52,248 bytes, 4.00, 43 rooms and 359 tasks, the
full-length rewrite of the already-wired *Veteran Experience*. **WON 50/50** in
120 commands, and 50 is provably the ceiling (eight `ACT type=4` awards summing
to 50, all eight fired). No published walkthrough exists, and it did not matter:
**the game ships its own hint system, and `SCR_DUMP_TASKS` prints it** as the
per-task `HINTQ=`/`HINT1=`/`HINT2=` fields, one entry per puzzle with the
literal command in capitals. That is the transferable lesson — grep for `HINT2=`
first on any game whose author built a hint menu. `vetknow2.taf` went in on the
same route: it differs from `vetknow.taf` in **three strings only** (the author
byte-field `Robert Rafgon` → `Robert Street`, one added ABOUT sentence saying
so, and the build date), the transcripts are byte-identical, and the second row
exists to keep them that way. See *Veteran Knowledge* below.

**2026-08-14: *The Lost Tomb* wired, suite 238 rows, 238 PASS.** Next in the
smallest-first order — 56,336 bytes, **3.90**, 19 rooms / 99 tasks / 86 objects
/ 1 NPC / 13 events. **WON 175/175** in 105 commands, and 175 is provably the
ceiling (23 `ACT type=4` awards summing to exactly 175, all 23 fired). No
published walkthrough, and again it did not matter: **this game ships an author
hint menu too**, so the `HINTQ=`/`HINT1=`/`HINT2=` trick from *Veteran
Knowledge* worked a second time — that is now a habit, not a coincidence. The
row's real value is an engine-semantics find: object restriction `type=0 v2=2`
is **worn by the player, not held**, and the game's biggest single award (+20,
T47 "read the wall through the death mask") hangs on it while the `v2=8`
counterpart prints a plausible wall description instead of a refusal. Carrying
the finished mask in reads as success and silently costs 20 points. See
*The Lost Tomb* below.

**2026-08-14: *The Long Journey Home* wired, suite 239 rows, 239 PASS.** Next
in the smallest-first order — `Journ2.taf`, 59,124 bytes, **3.90**, Danny
Chabino, 20 June 2001. **UNFINISHABLE, 30/90** in 46 commands, so the row is
anchored on the score line like *The Hangover*'s. Three independent walls, all
provable from the dump: the ten `ACT type=4` awards include **three
male/female twins**, so 90 is two mutually exclusive careers and 60 is anyone's
arithmetic ceiling; **T22 steals the valve from T25** (identical four patterns,
T22 unrestricted and at the lower index) in a room whose only exit is
`gateTask=25`, making Rage a **one-way trap**; and **T76 `#6 start card game`
has no dispatcher at all** — internal-label Command[0], no ALTCMDs, no event
targets it, and the file has zero `ACT type=5` — which seals T77/T78/T79 and
the whole endgame. The verdict does not even rest on those: **T86 `#17 the
end`, the file's only `ACT type=6 v1=0`, is itself `where=0`**, the second file
in the corpus with *The Hangover*'s exact shape. Two transferable findings came
out of it — see *The Long Journey Home* below.

**2026-08-14: *Murder in Great Falls* wired, suite 240 rows, 240 PASS.** Next
in the smallest-first order — `mudergreatfalls.taf`, 59,896 bytes, **3.90**, no
author recorded anywhere, released 24 Nov 2001. **WON 200/200** in 98 moves,
and 200 is provably the ceiling (exactly 32 `ACT type=4` awards, no variables
and no events in the file, summing to exactly the declared 200; all 32 fired).
Its headline is a `<waitkey>` placement worse than *Far From Home*'s: this game
has **two start-up prompts** — a name prompt and, because `PlayerGender` is
Unknown, the Runner's gender dialog — and a `<waitkey>` sits **between them**,
so without `SCR_SKIP_WAITKEY=1` line 2 is swallowed, every later line is
offered as a gender and rejected, and the run never starts. The day structure
is pure task plumbing (no events, no variables): T35/T61/T63 close Days 1/2/3
and all three are `where=3`, so they fire from the player's own living room
with the NPC never met. Two engine-fidelity witnesses came out of it — **T47
`turn on tv` is `where=0`**, a third corpus case after *The Hangover* and
*La hija del relojero*, invisible in play only because the library answers
"You can't turn that." first; and the 3.90 room refusal **"You can't do that
here!"** fires live on `knock on door` typed in the Office, the first English
witness for the 2026-08-10 port. The author hint menu is here too, but this is
the first of the three where **`HINT2=` did not hand over the walkthrough** —
all five entries are prose and none names a command. See *Murder in Great
Falls* below.

**Closed 2026-08-16: the 18 SUSPECT rows, and the comment-eating bug behind a
further 24.** `harness/waitkey_audit.py` now reports **SUSPECT 0**. See
"The waitkey audit" below for the full account; in short, every row whose game
reaches a `<waitkey>` on its route now sets `SCR_SKIP_WAITKEY=1` (IMMUNE 97),
the surplus filler lines are gone, and `os_ansi.cpp`'s pause read honours the
same `#` comment convention `os_read_line` has always applied. All 242 rows
PASS.

**2026-08-16: *The Woods Are Dark* wired, suite 243 rows, 243 PASS.** Back to
the smallest-first order the third wave was following — `thewoods.taf`, 71,216
bytes, **3.90**, Cannibal 2003, 23 rooms / 82 tasks / 18 objects / **10
variables, 0 events**. **WON 100/100** in 73 commands, and 100 is provably the
ceiling: 21 `ACT type=4` awards summing to exactly the declared maximum, all 21
on the critical path and all 21 fired. With no events and no NPC clocks the
whole game is one dependency chain, so there is no pacing to get wrong — only
two self-gating steps to get in order (`lift trunk` needs `trunk == 0`, the
dolls house needs `melissa == 0`), and both are the shape that reads as "already
dealt with" if you come back to them. Two things worth carrying: **T16
`unlock window` only scores in Drew's Bedroom** because the game-wide
`* unlock * window *` task, T51, has that one room cut out of its `WHERE_ROOMS`
list so the command falls through to the scoring twin; and **T10 `bounce ball`
teleports the player to the Back Yard** mid-message, so the second half of the
route starts downstairs rather than where the ball was found. See
*The Woods Are Dark* below.

**2026-08-16: *Captive Universe* wired, suite 244 rows, 244 PASS.** Next in the
smallest-first order — `Captive.taf`, 74,568 bytes, **3.90**, no author recorded
anywhere, after the Harry Harrison novel. 62 rooms / 61 tasks / 49 objects /
2 NPCs / 19 events / **no variables**. **WON 100/100** in 57 commands, and 100
is provably the ceiling (nine `ACT type=4` awards, 8×10 + 20, all nine fired;
the only other one is T3 `* hint *` at **−356**, the author's joke penalty for
asking for a hint). With no variables at all, every gate in the game is a
task-done test or an event, and the first half is a **one-shot clock**: leaving
the courtyard gate (T11) starts four events at once that fire at exactly turns
8, 18, 18 and 20 and never again (`restart=0`), so the route only has to be
somewhere safe at those three instants — rooms 19/40/41/44 and the swamp appear
in no arrest task's `WHERE_ROOMS`. The village and the inner temple are the
mirror image, `starter=1 restart=1` arrests that run *every* turn until
nightfall, which is why everything the second half needs is unreachable before
turn 20. Two things worth carrying: **`Globals.WaitTurns` is 3 here too**, so
the four `z`s that pace to nightfall are twelve turns (third witness after
Cursed and *The Vampire With A Conscience*); and **EVENT 18 [Timedoor] is an
`affTask` pointing at its own `startTask` with `fin=1`**, i.e. a task that
un-completes itself one turn later — the ledge's steel door slides shut unless
`w` is the very next command. A corpus-wide sweep puts that idiom in **nine**
files (`Captive`, `Mangiasaur`, `To_Hell_And_Beyond`, `Vendetta`, `humbug`,
`losttombv2`, `the_pk_girl`, `tra`, `wrecked`), so it is a shape to recognise,
not a novelty. See *Captive Universe* below.

**2026-08-16: *Adventures of Thumper — Wonder Wombat* wired, suite 245 rows,
245 PASS.** Next non-AIF file in the smallest-first order — `wonderwombat.taf`,
107,200 bytes, **3.90**, Chris Tyson 2001–2002. 51 rooms / 131 tasks / 76
objects / 39 NPCs / 39 events / 15 variables / **441 `<waitkey>` tags**.
**WON in 217 commands.** The file contains **not one `ACT type=4`**, so the
game has no score at all and the summary reads *"You scored 0 out of the
maximum 0! … 100% of the game!"* after **any** ending, win, loss or death — a
score marker would pass on a corpse, so the row matches the winning cutscene's
closing line `THUMPER KICKS ASS!!!` instead. That is now the rule for
no-score games: **never anchor on the summary when MaxScore is 0.**

Four transferable findings:

* **Two different string matchers, one object.** The jar is *"a jar of chronic
  fooluffultitus syndrom pills"*. `take fooluffultitus pills` is refused with
  "Take what?" while `take syndrom pills` works — the built-in take/drop parser
  matches the object's own name plus its **prefix adjective** words only. But
  `give syndrom pills to fry` is refused with "Give … to who?" while
  `give fooluffultitus pills to fry` works — task command matching runs against
  the **raw input string** (`*fooluffultitus*fry*`) and never consults the noun
  parser. A refused `take` therefore says nothing about whether a task command
  will match, and vice versa.
* **A maze that cannot be mapped.** Rooms 32–41 all catch `n`/`s`/`e`/`w` with
  one `where=2` task whose action is `type=1 v1=0 v2=1 v3=0` — *move player to
  a **random** room*. The only real edges in the block are `32 S -> 31` and
  `41 N -> 42`. Under the seeded harness the wander is reproducible, so the
  route's twelve norths are a measured constant with no meaning; they would be
  wrong under any other seed. Worth recognising the shape before trying to map
  one.
* **Turn-parity-sensitive routes exist.** The only copy of the titus component
  sits in room 30, *Fantasy Land*, entered at `alcohol >= 100` and left at
  `<= 99`, with alcohol decaying 1 per 5 turns from `EVENT 0 -> TASK 1`. Which
  `drink beer` tips over — and how many turns the hangover lasts — depends on
  the total turn count of everything before it, so deleting even a failed
  command higher up desyncs the second half of the route. Three failed probe
  commands were trimmed during derivation and every count below them had to be
  re-measured. If a route has a meter threshold in it, treat the whole prefix
  as frozen.
* **Losing on purpose is a gate.** The swear-off pays $5,000 for the word from
  under the shack doormat, but only in round two: losing round one is what
  sends Percy the Possum to the bar, and the arena will not re-open while he
  is in it.

Nine unrestricted author cheats survive in the shipped file (`win`, `chunka`,
`oozle`, `thoof`, `skam`, `joxxx`, `glenn`, `rottencop`, `joepoe`); `win` is a
bare `ACT type=6 v1=0` usable from anywhere. None is used. See
*WonderWombat_walkthrough.md*.

**2026-08-17: the 3.90 *Town of Azra* wired, suite 246 rows, 246 PASS — and
the "half this game's goals are unreachable" verdict was a property of the
*file*, not the game.** The corpus carries two Azra builds and the table below
had them down as "a second file, not a second game". That is true of the data
and false of the play. `The_Town_Of_Azra.taf` (IF Archive) is a **4.00**
upconversion of the author's **3.90** original, `The Town Of Azra.taf`
(adrift.co); the ADRIFT 4 editor left every battle attribute degenerate, so the
4.0 `accuracy > agility` gate is `0 > 0` and no blow ever lands. Combat is
Azra's only income, so the 4.00 file is walled off at the shops and the inn —
which is exactly where the existing 27-turn row stops. The 3.90 file is
recognised by `battle_is_legacy_version()`, gets `battle_legacy` (no hit gate;
`damage = strength − defence`), and plays through **all six goals the author
lists in his intro**: kill a bandit, sell a deer carcass to Drako, buy from all
three shops, stay at Gralle's Inn, learn Stealth Tactics, and buy the $7,500
house. 414 commands / **505 turns**, `SCR_SKIP_WAITKEY=1`. Both rows stay: each
is faithful to its own file, and together they are the corpus's cleanest
demonstration that **an editor upconversion can silently destroy a game**.

Four transferable findings:

* **Check for a sibling build before writing an "unreachable" verdict.** The
  old note here had a careful, correct, four-paragraph proof that four of six
  goals were dead — against the wrong file. The tell was already recorded: the
  *upgraded-3.9 fingerprint* (every attribute range `Lo == Hi`, acc/agi/recovery
  all zero) means the shipped 4.00 file is a conversion, which means an original
  may exist. When a game's blocker is the acc-vs-agi stalemate, look for a 3.9x
  release before concluding anything.
* **A rejected command costs no turn.** "That is not an option or command." is a
  parser rejection and the clock does not advance. So a route derived by an
  adaptive driver can have all its failed probes **deleted** afterwards with no
  effect on anything downstream — 49 of them came out of this route and the turn
  count stayed at 505. Substituting `z` for them instead moved it to 756 and
  desynced every NPC. This is the exact opposite of the *Wonder Wombat* lesson
  above (where trimming a failed command desynced the second half): there the
  meter was ticking on a real turn count, here the trimmed commands were never
  turns at all. Establish which case you are in before editing a derived route.
* **Armour can delete a grind.** Damage is `strength − defence` applied only
  when positive, so pushing defence past the enemy's strength makes a fight
  free rather than merely cheaper. Azra's bandit has strength 5; rawhide (2) +
  bronze helmet (1) + copper armour (4) = 7, and the remaining ~470 turns need
  no healing at all. Worth a `status` check before budgeting a long fight.
* **Read the intro's goal list.** Azra prints its six goals, the `search for
  money` hint and Drako's exact `sell … for 500 dollars` phrasing on the title
  screens. For a game with no score and no ending, that list *is* the
  completion criterion, and the win marker becomes the final `stats` turn count.

Also settled: rooms 16/17/18 (Town Council Hall, its Main Door, the Law
Building with Pahlidro) have no inbound exit from rooms 0–15 and no task that
moves the player there — task 58 `enter *** town council hall` is itself
`where = room 17`. Orphaned in both builds. See *The_Town_Of_Azra_walkthrough.md*.

**2026-08-17: *Vardock Bates* wired, suite 247 rows, 247 PASS — and with it
every non-AIF file in the corpus is done.** 2,928,980 bytes, 4.00, Spanish, by
far the largest `.taf` here and the last 4.00 holdout. A five-chapter vampire
story: buried alive in chapter 1, a scavenger hunt through Barcelona in
chapter 2, a flashback to Ramsés II's Egypt in chapter 3, a museum robbery in
chapter 4, and a two-ending choice on top of Christ the Redeemer in chapter 5.
**WON in 103 commands.** No score at all (not one `ACT type=4`) and an empty
`WINTEXT`, so the row matches a line of the winning cutscene, exactly as
`relojero` does — and, as with `relojero`, the engine prints no end-of-game
score summary either. `SCR_SKIP_WAITKEY=1` for the five chapter banners.

Three transferable findings:

* **A Spanish ADRIFT game can be entirely English underneath.** The author
  writes his task patterns with **English verbs and Spanish nouns**
  (`[take]{el}[mechero/encendedor]`, `[light]{el}[mechero]`) and ships a
  `SYNONYM` table that rewrites the player's Spanish input before matching. So
  the dump reads like an English game while the play is Spanish, and *both*
  languages parse at the prompt. `relojero` is built the same way. When a dump
  looks bilingual, check the synonym table before assuming the patterns are
  unreachable.
* **The library take handler does not accept a Spanish article.**
  `coger el revolver` → *"¿Qué quieres coger?"*, `coger revolver` works. The
  built-in noun parser knows only the object's own name and prefix words;
  author tasks accept the article only because they spell it out in a `{el}`
  optional group. This is the *Wonder Wombat* "two different string matchers,
  one object" lesson again, in a second language — and it is worth checking
  first whenever a plainly-correct `take` is refused.
* **Two endings can both be `ACT type=6 v1=0`, with one gating the other.** In
  room 38 `poner el brazalete` (TASK 36) and `lanzar el brazalete` (TASK 35)
  are both wins, but TASK 35 requires TASK 36 **UNdone** plus the briefcase
  opened and the Committee's document taken and read. So one ending silently
  locks the other out and the asymmetry is the only thing that says which is
  the "full" one. Only the fuller ending is wired.

Also: the revolver behind the bathroom mirror is a pure trap — TASK 56 (touch
or attack the wolf), TASK 57 (shoot it) and TASK 31 (`matar a jason`) all
`exec task 23`, an EndGame **lose**. The wolf is beaten with the cobblestone
that broke the mirror. Two hard timers: EVENT 0 gives ten turns from mounting
the horse in Egypt to `decir museo` against a shortest path of eight, and
EVENT 3 gives exactly one turn from `hablar con jason` to `esquivar el baston`.
See *Vardock_Bates_walkthrough.md*.

**2026-08-17: the eight AIF holdouts were triaged on content, and
*Lara Croft: The Sun Obelisk* wired, suite 248 rows, 248 PASS** (250 after
*Doctor Who* and *The Gamma Gals* below)**.** See
*The AIF holdouts* below for the triage — four of the eight are declined and
four are the ordinary *Diary of a Stripper* case. `croft.taf` is the first of
the four, 148,447 bytes, **3.90**, Christopher Cole, Fall 2002: a Tomb Raider
pastiche, cast adults throughout, 35 rooms / 231 tasks / 40 objects / 30
statics / 4 NPCs / 4 variables / **one event**. **WON 150/150** in 101
commands, and 150 is provably the ceiling (48 `ACT type=4` awards, 27×2 + 7×3 +
13×5 + 1×10 = exactly the declared maximum, all 48 fired). Eleven `ACT type=6`
— ten `v1=2` deaths and one `v1=0`. No `<waitkey>` in the file at all, so the
row carries no env (NO-WAITKEY 115). Solution and golden gitignored, row
committed, no notes file — the *Camp Windy Lake 2* treatment, third time.

The row's reason to exist is that **the author's own shipped walkthrough is
three points short of the total it prints.** `croftwlk.txt` (by John
<not_jwc@hotmail.com>, in the game's own zip, now
`downloaded/LaraCroft_SunObelisk_walkthrough.txt`) is annotated with running
scores all the way to 150, and replaying it verbatim ends at **147**. The
culprit is one line. The game is adult AIF, so the strings are written here in
schematic form — `V` is the verb the walkthrough types, `V'` the verb the tasks
are written with, `N` the shared noun; the literal commands are in the
gitignored `goldens/croft_solution.txt`. The offending line is `V N with jade`:

* The file carries `SYNONYM [V] -> [V']`, and substitution runs **before**
  task matching — *La hija del relojero*'s finding, now in an English game. The
  matcher sees `V' N with jade`.
* **TASK 117 `V' N*` then claims it.** 117 is unrestricted, sits six
  indices in front of the task the author meant, and its trailing `*` swallows
  the rest of the line. It is the *solo* statue action and has already fired for
  its +2 earlier in the scene, so the second hit reprints its message and
  awards nothing — a silent loss with plausible output, the shape *The Lost
  Tomb*'s death mask has.
* The +3 is **TASK 123**, whose 25 patterns include `V' * N* with jade`.
  Reaching it needs a phrasing 117 cannot claim; the route uses 123's own
  `me and jade V' * N*` (typed with `V`) — which also settles
  that **a medial `*` matches zero words**, not just one or more.

This is first-match precedence rather than an engine divergence — **and that
was measured in the real Runner, not inferred from the task indices.** See
*Verified against run390* below. The transferable rule is the pairing — **when
a game ships a `SYNONYM` table, read it before trusting any published route**,
because the command the author documents is not the string the matcher sees,
and a lower-indexed unrestricted task with a trailing `*` will quietly eat the
rewritten form.

**Verified against run390, 2026-08-17.** The parity claim above was first
written from the dump alone, which is not evidence about the Runner, so it was
measured. Replaying all 101 croft commands in `run390.exe` proved too fragile
(the game's picture window takes focus and the scripted keystrokes desync
within a few turns — the first attempt sat in the Ante Chamber at score 0),
so the shape was reduced to a purpose-built probe instead:
`harness/make_39_synprobe.py`. It is built from croft's own vocabulary so that
it is a faithful reduction of the file, which is why it is **gitignored** along
with the AIF solutions and goldens; it lives in `harness/` on this machine.
One room, no objects, `SYNONYM [V] -> [V']`, `MaxScore 3`, `bNoAutoComplete 1`
— the game turns the Runner's input mangling off for itself — and exactly two
unrestricted, repeatable, all-rooms tasks:

    TASK 1  `V' N*`                                      -> "TASK1 FIRED."  +0
    TASK 2  `me and jade V' * N*` / `V' * N* with jade`
                                                         -> "TASK2 FIRED."  +3

Run in `run390.exe` under Wine (`pfx/drive_c/adrift/pSYN.taf`, screenshot
`synB.png`) and in `harness/scare`, the two agree line for line:

| command | run390 | Scarier |
|---|---|---|
| `V' N with jade` | TASK1 FIRED, score 0 | TASK1 FIRED, score 0 |
| `V N with jade` | TASK1 FIRED, **score 0** | TASK1 FIRED, **score 0** |
| `me and jade V N` | TASK2 FIRED, score 3 | TASK2 FIRED, score 3 |

So all three of the sub-claims hold in the real Runner: the `SYNONYM` rewrite
happens **before** task matching (row 2 behaves exactly like row 1), the
lower-indexed unrestricted task with the trailing `*` **does** steal the line
from the higher-indexed one that spells the command out, and a **medial `*`
matches zero words** (row 3 matches `me and jade V' * N*` with nothing
between `jade` and `N`). `croftwlk.txt` really does top out at 147 in the
Runner it was written for, and Scarier is right to score it that way.

Two harness notes from this, both already in the Wine memory but worth the
repetition: **the first scripted command after launch is routinely lost** —
`synA.png` shows a blank echo and "I don't understand." where the first
command should be, and padding the script with two `look`s fixed it — and **the
echo is the only trustworthy record of what the Runner actually received**, so
a probe worth running is one whose every command echoes visibly.

Also worth carrying: `shoot goon` at the Waterfall is the **scoring branch**
(it needs the twin Magnums, lost at the cave's dead end), and the alternative —
sliding down the slope in the Thick Jungle — skips Strathairn's Camp and 21 of
the 150 points, which the author's own FAQ puts at 67% of the total. The Hall
of Spheres riddle answer is typed **bare** as `tomorrow`, a task command rather
than a `say`/`answer` verb. The altar must be **pulled**, not opened; opening
it is one of the ten deaths. And the shirt button, the chunk of quartz, the
Aztec coin and the Lost Cave's hollowed-out rock altar are author-confirmed red
herrings — every carried item is taken away before the Temple regardless of
route, so none of them can matter.

**2026-08-17: *Doctor Who and the Vortex of Lust* wired, suite 249 rows, 249
PASS** (250 after *The Gamma Gals* below)**.** `dr-who-vortex-lust.taf`, 166,913 bytes, **3.90**, Christopher Cole
again — the fourth Cole game in the corpus after `diarystrip`, `windy2` and
`croft`. 25 rooms, 209 tasks, 9 NPCs. **WON 150/150** in 113 commands, and
150 is provably the ceiling: 50 `ACT type=4` awards summing to exactly the
declared maximum, all fired by this route. Only **two** `ACT type=6` in the
whole file — `shoot dalek` (death) and `replace staff` (the win). No
`<waitkey>` anywhere, but the game *does* prompt for a name, so line 1 of the
solution is `Sam` and the row still carries no env. Solution and golden
gitignored, row committed, no notes file — the *Camp Windy Lake 2* treatment,
fourth time.

**The author ships no walkthrough at all**, only `drwho-score.txt` (kept as
`downloaded/DrWho_VortexOfLust_scoresheet.txt`), which lists *what* scores but
never what unlocks it. So the route was derived from the dump against that
score sheet, and **the order is the whole puzzle**. It is one forced chain:
three of the six girls have `startRoom=-1` and are placed only by finishing the
one before.

(Each girl's chain ends in a *finisher* task; the literal commands are in the
gitignored `goldens/dr-who-vortex-lust_solution.txt`.)

* Ace's finisher moves Nyssa into Your Room.
* Nyssa's finisher moves Sarah into the Lab and Adric into the Library, and
  gates `tell tegan about adric`.
* Tegan's finisher hands over the picture, the only way to score Adric.
* `give picture to adric` pays out the dispenser code `12553m`.
* the wine that code dispenses is what starts Sarah.

**Leela is the trap in that chain.** She is on the map from the start and has
no gate, so she is the natural one to leave for later — but Sarah's finisher
(TASK 159) moves characters 2..7 and 9 to room 0 and empties the TARDIS, so the
Solarium has to be visited **before** the Lab. Getting it wrong costs 15 points
and prints **no refusal at all**: the same silent-loss shape as croft's TASK
117 and *The Lost Tomb*'s death mask, and the third time in this corpus that a
route is wrong without the game ever saying so.

Per girl an `X sex` variable gates every body task, and in every case it is set
by something **non-sexual**: `tell peri about temporal breach` (which itself
needs `ask k9 about temporal energy`, and K9 is in the first corridor), `round
3` of the strip darts, `tell tegan about adric`, `give wine to sarah`. Reading
the body tasks alone therefore tells you nothing about how to reach them.

Two more traps the route avoids: seven of Ace's nine scoring tasks require the
7th Doctor's hat **not** to be worn (restriction type 0 `Var2=8`), so wearing
the hat quietly removes 12 of her 15 points; and `shoot dalek` with the blaster
rifle is the game's only death — the Dalek is disabled with the sonic
screwdriver from the Doctor's jacket in the very first room. The Kitchen's
other code, `122-663a`, is a task with no actions: a red herring.

**2026-08-17: *The Gamma Gals* wired, suite 250 rows, 250 PASS — and that is
the last unwired `.taf` in the ADRIFT 4 corpus.** `gamma.taf`, 277,834 bytes,
**3.90**, Christopher Cole for the **fifth** time after `diarystrip`, `windy2`,
`croft` and `dr-who-vortex-lust`. 44 rooms (ten of them closets), 304 tasks, 10
NPCs, 32 variables — the largest v4 file in the corpus. **WON 150/150** in 182
commands, and 150 is provably the ceiling: 68 `ACT type=4` awards summing to
exactly the declared maximum, all fired by this route. **Not one refused
command in the whole transcript** — no "You can't go in that direction", no
"You can't do that here", no parser complaint. No `<waitkey>` in the
deobfuscated body, but the game *does* prompt for a name, so line 1 is `Sam`
and the row carries no env. Solution and golden gitignored, row committed, no
notes file — the *Camp Windy Lake 2* treatment, fifth time.

**Again the author ships no walkthrough**, only `gamma-score.txt` (kept as
`downloaded/GammaGals_scoresheet.txt`) and `gamma.txt`
(`downloaded/GammaGals_readme.txt`). The score sheet is grouped per girl, and
**those group totals are what pin the route down**: Sharron 14, Sharron &
Shannon 29, Shannon 9, Heather 10, Kelly 17, Christine 11, Laurie 2, Krista 3,
Krista & Laurie 10, Stacey 35, Other 10. The route hits every group exactly, so
the 150 is not just a total that happens to arrive — each scene is closed out.

The game is adult AIF, so the scene commands are referred to below by task
number and by role — a girl's *finisher* is the last scoring task in her chain.
The literal strings are in the gitignored `goldens/gamma_solution.txt`.

**Stacey is a counter, not a place.** The win is TASK 292, Stacey's finisher
(+10, the file's only `ACT type=6 v1=0`), gated on `stacey sex == 7`
**exactly**, not `>=`. Six non-repeatable tasks bump it, and they are the other
five girls' finishers plus the twins: T91 (the twins' finisher) +1, T125
(Shannon) +1, T182 (Heather) +1, T232 (Kelly) +1, T250 (Christine) +1, T252
`tell krista about laurie` +2. All six must fire, so
Stacey is *necessarily* the last scene — and an `== 7` gate means overshooting
is as fatal as undershooting, except that there is nothing else to overshoot
with.

**The ordering trap is Sharron.** All six solo-Sharron scoring tasks (44, 45,
54, 58, 62, 64) carry `CHAR Shannon NOT in room with player`, and T63, the
last of Sharron's solo chain (+2), **moves Shannon into Sharron's Room**. Do it
early and 14 points
disappear with **no refusal printed at all** — the fourth silent-loss shape in
this corpus after croft's TASK 117, *The Lost Tomb*'s death mask and *Doctor
Who*'s Leela.

Per girl an `X sex` variable gates every body task and, exactly as in *Doctor
Who*, is always set by something **non-sexual**: `give bracelet sharron`
(bracelet behind the Downstairs Bathroom toilet), `show bottle to heather`,
`light joint` (joint under the Party Room couch, lighter on the Front Porch
table one room off the start), `tell christine about erin`, `tell laurie about
krista`, `tell krista about laurie`. And the girls place each other — T93 sends
Heather to the Kitchen, T95 drops the player in Shannon's Room, T52 sends
Laurie to the Front Room, T76 sends Krista to her own room, T252 puts Krista in
Laurie's room (the only way to get the pair together), T168 moves Heather to
the Party Room, T182 moves Christine to the Dining Room, T234 moves Kelly to
her room and hides Erin, T237 moves Christine to her room. The walking order is
forced by that graph, not chosen.

**`wendy` is the hidden keystone.** TASK 126, a bare word with no verb, +5,
Heather's Room, player must be **alone** — and it is the prerequisite for `tell
kelly about zeke`, which gates *all 17* Kelly points. Heather only leaves her
room once the bracelet is handed over, so the bracelet has to come first or
`wendy` cannot be typed. Two other details worth keeping: `mix rum and coke`
consumes the coke and the glass but **not** the bottle, and TASK 211 (+2) is
the one scoring task that needs the bottle still in Heather's hands at the end
of her scene; and
`play kelly's video` needs the player **alone** in the TV Room, Erin being the
one character with no scoring tasks of her own and the video being what removes
her from the map. The red herrings are real and skipped: the quarter under the
Front Room sofa, `ride bike`, `workout`, `play stereo`, the Shower's shampoo,
and `put rum in glass` / `pour coke in glass`, which are accepted and do
nothing.

**The AIF holdouts, triaged 2026-08-17.** Eight files were left when every
non-AIF game was done. They are not one category, and the split is on the age
of the cast, not on how explicit the text is. **Four are declined**; four are
the ordinary *Diary of a Stripper* / *Camp Windy Lake 2* case — adult content
between adults, wire the row and gitignore the solution, golden and notes.

| File | Title | Verdict | Evidence |
|---|---|---|---|
| `enc1.taf` | Encounter 1: Tim's Mom | **declined** | the author's shipped `enc1.txt` states the PC is a 15-year-old boy and the whole 50-point table is explicit sex with an adult |
| `windy.taf` | Camp Windy Lake | **declined** | the *game's own opening screen* reads "This game's hero is a fourteen year old boy and involves some sexual scenes with very young girls", and `score.txt` scores each of them; the readme adds incest and non-consensual sex |
| `enc2.taf` | Encounter 2: The Study Group | **declined** | `enc2.txt` sets it in a 12th-grade English class study group and the scoring table is explicit sex driven by getting the three classmates drunk to the point of passing out |
| `Buffy Before the Date.taf` | Buffy: Before the Date | **declined** | Sunnydale-High-era fan fiction: Xander is "a poor student", Cordelia is his girlfriend, the `dawn` conversation topic answers "She doesn't exist yet, Xand", Buffy's maths homework is an object, and the wine is "not really legal for your age" |
| `croft.taf` | Lara Croft: The Sun Obelisk | **WIRED 2026-08-17** | Lara Croft, Damian, Jade, Tapper — adults |
| `dr-who-vortex-lust.taf` | Doctor Who and the Vortex of Lust | **WIRED 2026-08-17** | the file's own companion bios give ages, the youngest being Peri at twenty |
| `gamma.taf` | The Gamma Gals | **WIRED 2026-08-17** | a sorority house; the PC's girlfriend is a sister there |
| `windy2.taf` | Camp Windy Lake: Part 2 | **WIRED 2026-08-12** | the readme's cast list is nineteen- and twenty-year-old counsellors plus the thirty-something owner |

The four declines are not a judgement about the corpus or about wiring AIF —
`windy2` and `diarystrip` are wired and their rows are worth having. They are
about deriving and committing a *walkthrough*, i.e. a scored, step-by-step
route through explicit sexual content whose participants the author has put at
school age. Each verdict above is quoted from the game's own shipped text, so
it does not need re-deriving; if it is ever revisited, revisit it on that
evidence rather than on file size. **None remain to wire**: `gamma.taf`
(277,834 bytes) went in on 2026-08-17, and with it every `.taf` in the ADRIFT 4
corpus is either wired or declined on the evidence above.

Otherwise nothing here is open work. The one exception this file used to carry — the
Runner's **"You can't do that here!"** refusal for a task typed outside its
`Where` rooms — was probed and **implemented 2026-08-10**, along with its
sibling **"You have already done that."**; see the follow-up under
*The Hangover*. Everything else that once read as open below has since
closed — where a later finding overturned a dated entry it carries a superseding
note in place; the entry itself is left as written.

```
test/adrift4/harness/run_v4_walkthroughs.sh          # the whole suite
test/adrift4/harness/run_v4_walkthroughs.sh <regex>  # one row
test/adrift4/harness/run_v4_walkthroughs.sh --bless  # re-record goldens
```

## Second wave (2026-08-10 → 2026-08-11) — 22 games, then PARKED

The first wave covered every `.taf` that was in `games/` when it ran. The
manifest has grown since (issue #119's corpus fetch), so a second pass worked
**smallest unwired file first** through the ADRIFT 3.8/3.9 leftovers. Twenty-two
went in; the suite went 203 → **225 rows, 225 PASS**. Same four artefacts per
game as before: commented `goldens/<name>_solution.txt`, blessed
`.expected.txt`, a commented row in `harness/run_v4_walkthroughs.sh`, and
`notes/<Game>_walkthrough.md`.

| Bytes | Game | Result |
|---|---|---|
| 5,164 | I | ending reached; no score in the file |
| 6,508 | Dreamland | **WON 50/50** |
| 8,292 | Forest On The Norm | completed; no scoring system |
| 10,310 | The Adventures of Bob Bobsly | **WON 155/155** |
| 12,028 | Druggy Lane | **WON** — debt cleared at $1,955,720,463 |
| 12,660 | Escape from Insanity | **WON 1000/1000** |
| 16,695 | Lost Souls | **WON**; no scoring system |
| 19,755 | Chicago | **WON 75** |
| 20,065 | Everything Emanuelle | fullest of four endings; no score |
| 20,652 | Textident Evil | **WON 100/100** |
| 22,214 | Impulso | **WON**; no score |
| 23,225 | Montahue Scott and the Mobius Belt | **WON 3/3** |
| 26,973 | A Morning With A Headache | **WON 115/115** |
| 29,811 | Sleaze City | **WON 100/100** |
| 31,353 | Albridge Manor | **WON 50/50** |
| 37,088 | The Lost Mines | **WON 100/100** |
| 39,485 | The Dark Tower | **WON** (T8 `turn on power`) |
| 41,801 | Report Espionage | **WON 100/100**, all 23 tasks on the route |
| 42,118 | Far From Home | **WON 50/50** |
| 42,463 | S Tar Dus T | **WON**, richest of four endings; no score anywhere |
| 43,334 | Diary of a Stripper | **WON 13/13**, best of fourteen ALR endings — **AIF, solution/golden/notes gitignored** |
| 71,345 | Silk Noil | **WON**; no score. Added after the park, from the author's own walkthrough (see below) |

**AIF in the corpus.** *Diary of a Stripper* is sexually explicit, so its
solution, golden and notes are gitignored and only its `run_v4_walkthroughs.sh`
row is committed — the same call made for *Archie's Birthday* on 2026-07-13 (see
`.gitignore`). Its row is still worth having: the ALR-table score, the two
chained events and the rejected-command-does-not-tick rule are all pinned by it.
Everything else wired in this wave is clean, and so is the rest of the corpus: a
scan of every committed golden for explicit vocabulary turns up nothing but
profanity (*Mortality*, *The Plague*, *Second Chance* and *Light Up the
Darkness* swear, and *Second Chance* adds two anatomical words — none of it
pornographic).

Three findings from this wave are worth carrying forward, and each is written
up in full in its own note:

- **A `<waitkey>` can sit in front of the name prompt.** *Far From Home* pauses
  inside the introduction, *before* `Please enter your name:`, so a scripted run
  must spend a line on the keypress and give the name on line two — otherwise
  the player is called `look` for the rest of the game. Always run
  `SCR_MARK_WAITKEY=1` before writing the first line of a solution file.
- **`ACT type=0` "into object" indexes the container list directly** — no `-1`,
  unlike the room case. *S Tar Dus T* drops its missing page in the lake, not
  in the outhouse hole, and reading the destination as 1-based sends you to the
  wrong room for the game's last puzzle. (`sctasks.cpp task_move_object`,
  `case 2` / `case 3`.)
- **A command the parser rejects does not advance the turn counter**, so
  gibberish padding will never make a timed event fire. Found while pacing
  *Diary of a Stripper*'s two chained events; `z` works, `wait1` does not.

## Third wave (2026-08-11) — 6 games, then PARKED again

Unparked to continue smallest-unwired-3.90-first. Six went in and the suite
went 225 → **231 rows, 231 PASS**.

| Bytes | Game | Result |
|---|---|---|
| 44,503 | Asylum | ending reached; no score in the file |
| 44,666 | The Wheels Must Turn | **WON** |
| 45,737 | Life | ✗ **UNFINISHABLE** — no `ACT type=6` and no `ACT type=4` at all; demonstration row |
| 48,764 | Renuntio | **WON** — first Spanish game in the v4 corpus |
| 51,820 | House Of Horror | **WON 145/155**, and 145 is a *proven ceiling*: T109 scores the doubloons on `v3=0`, which tests OBJ_HIDDEN rather than a room, so the +10 is reachable only by not shooting the zombie (worth +20) |
| 55,039 | Where Is Richard? | **WON 1000/1000** in 68 commands |

Two things came out of this wave beyond the rows:

- **`SCR_DUMP_TASKS` was dying on `Richard.taf`.** The LOCKKEY and OPENABLE
  loops in `scdump.cpp` called the *fatal* `prop_get_integer()` on
  `Objects[i].Openable`, and that game has an object without one, so the dump
  aborted right after the OBJNAME block — before ROOM, TASK, EVENT or NPC were
  printed. Fixed with the tolerant `prop_get()`. The engine itself was never
  affected; this was dev-only instrumentation.
- **A second corpus witness for one-level container nesting in "held by the
  player".** *Where Is Richard?* kills its spider with a cupcake that never
  leaves the backpack the player is carrying — see `probe p39held` and the
  comment above `restr_object_in_place` in `screstrs.cpp`.

**Parked again 2026-08-11 at the user's request, unparked 2026-08-16 for
*The Woods Are Dark* and *Captive Universe*.** Nothing is broken and nothing is
half-finished — the suite is green at 250/250 and every wired game has all four
artefacts. What remains is **4 unwired `.taf` files, all of them
v3.90 and all four declined on content grounds** (23 when this was written;
*Camp Windy Lake : Part 2*,
*Salutations* and *A Day at the Iachini House* went in on 2026-08-12,
*La hija del relojero*, both *Veteran Knowledge* files, *The Lost Tomb*,
*The Long Journey Home*, *Murder in Great Falls*, *The Vampire With A
Conscience* and *The Merry Murders* on 2026-08-14, *The Woods Are Dark* and
*Captive Universe* and *Wonder Wombat* on 2026-08-16, and the 3.90 *Town of
Azra*, *Vardock Bates*, *Lara Croft: The Sun Obelisk*, *Doctor Who and the
Vortex of Lust* and *The Gamma Gals* on 2026-08-17); the table below is the
original 29, with the six done in the third wave struck through and windy2,
salutations, iachini, relojero, vetknow, vetknow2, losttombv2, Journ2,
mudergreatfalls, Vampire, Merry_Murders, thewoods, Captive, wonderwombat,
Vardock Bates, croft, dr-who-vortex-lust and gamma struck through after them.
**Every file left is AIF**, and every 4.00 file in the corpus is now
wired. **Read *The AIF holdouts, triaged 2026-08-17* at the top of this file
before touching any of them**: all four that are left (`enc1`, `windy`, `enc2`,
`Buffy Before the Date`) are declined on content grounds, with the evidence
quoted from each game's own shipped text. **There is no next file to take.**
Do not pick one by size without reading that section.

**Two cautions about that list.** *Byte size does not compare across versions*:
a 4.00 `.taf` is zlib-compressed and a 3.90 one is only XOR-obfuscated, so the
13,868-byte 4.00 file and the 44,145-byte 3.90 file in the Azra row below are
the *same game* — though, as it turned out, not the same game to *play*; see
the 2026-08-17 entry. And *the smallest-first ordering is by file size, not by game
size* — read the Ver column before picking. Version comes from the 14-byte
header signature (`sctaffil.cpp`, `V400_SIGNATURE`/`V390_SIGNATURE`/…), which
is also what tells 3.80 and 3.70 apart from 3.90; every 3.80 and 3.70 file in
the manifest is already wired.

| Bytes | Ver | File | Title | Note |
|---|---|---|---|---|
| 5,591 | 4.00 | ~~`salutations.taf`~~ | ~~Salutations~~ | **WIRED 2026-08-12 — WON, no score in the file** |
| 19,083 | 4.00 | ~~`iachini.taf`~~ | ~~A Day at the Iachini House~~ | **WIRED 2026-08-12 — WON 115/115** |
| 21,775 | 4.00 | ~~`relojero.taf`~~ | ~~La hija del relojero~~ | **WIRED 2026-08-14 — WON, no score in the file** |
| 44,145 | 3.90 | ~~`The Town Of Azra.taf`~~ | ~~The Town of Azra~~ | **WIRED 2026-08-17 — all six of the author's goals; no score in the file.** Same *game* as `The_Town_Of_Azra.taf` (4.00, 13,868 bytes) but **not the same game to play**: only the 3.90 file gets `battle_legacy`, and combat is the game's only income |
| 44,503 | 3.90 | ~~`as.taf`~~ | ~~Asylum~~ | **WIRED, third wave** |
| 44,666 | 3.90 | ~~`Wheel105.taf`~~ | ~~The Wheels Must Turn~~ | **WIRED, third wave** |
| 45,737 | 3.90 | ~~`life.taf`~~ | ~~Life~~ | **WIRED, third wave** |
| 48,764 | 3.90 | ~~`Renuntio.taf`~~ | ~~Renuntio~~ | **WIRED, third wave** |
| 51,820 | 3.90 | ~~`hhorror.taf`~~ | ~~House Of Horror~~ | **WIRED, third wave** |
| 52,248 | 4.00 | ~~`vetknow.taf`~~ | ~~Veteran Knowledge~~ | **WIRED 2026-08-14 — WON 50/50** |
| 52,290 | 4.00 | ~~`vetknow2.taf`~~ | ~~Veteran Knowledge [Version 2]~~ | **WIRED 2026-08-14** — three changed strings vs the above, byte-identical transcript |
| 55,039 | 3.90 | ~~`Richard.taf`~~ | ~~Where Is Richard?~~ | **WIRED, third wave** |
| 56,336 | 3.90 | ~~`losttombv2.taf`~~ | ~~The Lost Tomb~~ | **WIRED 2026-08-14 — WON 175/175** |
| 59,124 | 3.90 | ~~`Journ2.taf`~~ | ~~The Long Journey Home~~ | **WIRED 2026-08-14 — UNFINISHABLE, 30/90** |
| 59,896 | 3.90 | ~~`mudergreatfalls.taf`~~ | ~~Murder In Great Falls~~ | **WIRED 2026-08-14 — WON 200/200** |
| 63,183 | 3.90 | ~~`Vampire.taf`~~ | ~~The Vampire With A Conscience~~ | **WIRED 2026-08-14 — WON 100/100** |
| 69,489 | 3.90 | ~~`Merry_Murders.taf`~~ | ~~Merry Murders~~ | **WIRED 2026-08-14 — WON 135/135** |
| 71,216 | 3.90 | ~~`thewoods.taf`~~ | ~~The Woods Are Dark~~ | **WIRED 2026-08-16 — WON 100/100** |
| 74,568 | 3.90 | ~~`Captive.taf`~~ | ~~Captive Universe~~ | **WIRED 2026-08-16 — WON 100/100** |
| 101,668 | 3.90 | `enc1.taf` | Encounter 1: Tim's Mom | **AIF — DECLINED on content, 2026-08-17** |
| 107,200 | 3.90 | ~~`wonderwombat.taf`~~ | ~~Adventures of Thumper – Wonder Wombat~~ | **WIRED 2026-08-16 — WON; no score in the file** |
| 114,698 | 3.90 | `windy.taf` | Camp Windy Lake | **AIF — DECLINED on content, 2026-08-17** |
| 120,335 | 3.90 | `enc2.taf` | Encounter 2: The Study Group | **AIF — DECLINED on content, 2026-08-17** |
| 125,581 | 3.90 | `Buffy Before the Date.taf` | Buffy: Before the Date | **AIF — DECLINED on content, 2026-08-17** |
| 148,447 | 3.90 | ~~`croft.taf`~~ | ~~Lara Croft: The Sun Obelisk~~ | **WIRED 2026-08-17 — WON 150/150; AIF, solution/golden gitignored.** The author's own shipped walkthrough tops out at 147 |
| 166,913 | 3.90 | ~~`dr-who-vortex-lust.taf`~~ | ~~Doctor Who and the Vortex of Lust~~ | **WIRED 2026-08-17 — WON 150/150; AIF, solution/golden gitignored.** No walkthrough exists; derived from the dump against the author's score sheet |
| 191,548 | 3.90 | ~~`windy2.taf`~~ | ~~Camp Windy Lake: Part 2~~ | **WIRED 2026-08-12 — AIF, solution/golden gitignored** |
| 277,834 | 3.90 | ~~`gamma.taf`~~ | ~~The Gamma Gals~~ | **WIRED 2026-08-17 — WON 150/150; AIF, solution/golden gitignored.** No walkthrough exists; derived from the dump against the author's score sheet |
| 2,928,980 | 4.00 | ~~`Vardock Bates.taf`~~ | ~~Vardock Bates~~ | **WIRED 2026-08-17 — WON; no score in the file.** Spanish, and by far the largest file in the corpus |

**Author material already on this machine, for whoever picks this up next.**
Several of the remaining games shipped documentation in their IF Archive /
adrift.co packages, unpacked under `~/Downloads`: `windy2walk.txt` +
`cw2faq.txt` (Camp Windy Lake 2 — used 2026-08-12, now kept as
`downloaded/CampWindyLake2_walkthrough.txt` / `_faq.txt`),
`croftwlk.txt` + `lcsofaq.txt` (Lara Croft — used 2026-08-17, now kept as
`downloaded/LaraCroft_SunObelisk_walkthrough.txt` / `_faq.txt`),
`score.txt` (Camp Windy Lake), `enc1.txt` / `enc2.txt` (the Encounters), and
`readme.txt` (The Long Journey Home). *Where Is Richard?* ships `Map.jpg` — a
hand-drawn sketch of the swamp, readable and accurate — but no walkthrough,
and that was the pattern: a map or a FAQ is common, a solution is not.

**Eight of the twenty-three that remain are AIF** and will need the *Diary of a Stripper*
treatment when they are wired — gitignore the solution, the golden and the
notes, commit the row alone. Measured, not guessed: de-obfuscating each `.taf`
and counting explicit vocabulary gives `gamma.taf` 1416, `dr-who-vortex-lust`
705, `windy2` 670, `windy` 630, `croft` 582, `enc1` 550, `enc2` 369 and
`Buffy Before the Date` 339, against 0 for every other unwired file (bar three
incidental hits in `wonderwombat.taf`). They are marked **AIF** in the table
above. *(2026-08-12: `windy2` is now wired on exactly those terms, so **seven
of twenty-two** remain — the treatment below is no longer hypothetical, it is
what that row did.)*

**Check the game's own zip before deriving anything.** *Silk Noil* was wired
after the park because its author shipped a ten-command walkthrough inside
`sn_zip.zip`; it is now `downloaded/SilkNoil_walkthrough.txt`. The
`downloaded/` harvest only covered walkthroughs IFDB links to, so a bundled
`*walk*.txt`, `*.sol` or `readme` in the game's own archive is a free oracle
that the harvest will have missed. `~/Downloads/More Adrift games/` holds the
unpacked archives.

Regenerate the unwired list at any time with a set difference between the second
field of every non-comment row in `harness/run_v4_walkthroughs.sh` and the
first column of `games.manifest.tsv`; re-run the vocabulary scan with
`harness/taf_pattern_scan.py`'s `plaintext()`, which de-obfuscates both TAF
generations.

## Fourth wave, 2026-08-23- — the manifest outgrew the corpus again

**"There is no next file to take" (above) was true on 2026-08-17, but the
manifest kept growing under issue #119's ongoing IFDB `adrift 4` tag sweep.**
Regenerating the unwired list (the recipe just above) on 2026-08-23 found the
manifest at 431 rows against 252 wired — **179 unwired `.taf` files**, all
physically present in `games/`. A quick heuristic triage before touching any
of them: a small regex vocabulary scan (`aif_scan.py`, scratch script, not
committed — same `plaintext()` de-obfuscation as `taf_pattern_scan.py`, counts
hits for a list of explicit-content terms) ranks all 179 by hit count. The top
of that ranking reproduces the same handful of titles the third-wave AIF
triage already flagged by name (`windy.taf`, `enc1.taf`, `enc2.taf`, `Buffy
Before the Date.taf`) plus new ones worth the same content-decline scrutiny
before deriving (`ss whore.taf`, `British.Fox.and.the.Celebrity.Abductions.taf`,
`BSG TWENTY TWO Final.taf`, `A Dream Come True.taf`, `goblin.taf`,
`sibling seduction.taf`, `plains.taf`, `Riding_Home.taf`, `awakening.taf`,
`Choices.taf`, `delight.taf`, `cldone.taf`, `amy.taf` — none decided yet, the
scan only tells you where to look first). The 0-hit remainder is ordinary
smallest-first territory.

**First batch, 2026-08-23: 8 more wired, suite 252 → 260 rows, all PASS
(counted after later batches landed too — see the running total in the repo
history; check `sh run_v4_walkthroughs.sh` for the live count).** All eight
are tiny (1.3–2.6 KB) 4.00 files with no `EndGame`/score structure of any real
puzzle weight — this size band is mostly speed-IF/joke/demo material, which
matches the "smallest-first" pattern from every earlier wave. Derived in
parallel (one background agent per game, each doing its own `SCR_DUMP_TASKS`
structural dump before playing) and merged by hand, re-blessing every golden
through the real harness rather than trusting a worker's own transcript pipe
— one (`picture_solution.txt`) had a stray trailing blank line from a
slightly different pipeline, and one win marker (`smote_solution.txt`) turned
out to straddle a word-wrapped line break and had to be shortened to a
substring that stays on one line. Both are now general cautions for future
batches, not just this one.

- **Sandy's Lost Doll** (`Sandy.taf`, 1286 bytes) — **UNWINNABLE, 0/0.** Six
  flavour-only "look in/under X" tasks, plus a three-stage toilet gag gated on
  a `mom` counter. The win task's `RESTR type=4 Var1=0` tests the *command's
  referenced number* (offset scheme: `Var1=0` = numeric wildcard capture, not
  a variable compare — that needs `Var1>=2`), which `look in toilet` never
  supplies, so it fails forever. An authoring slip in the `.taf` itself,
  reproduced faithfully.
- **Newton** (`Newton.taf`, 1291 bytes) — **WON, 0/0**, 4 commands. Joke
  micro-game: wait for the apple to fall (turn 4), then `get apple` on the
  very next command is the *only* winning move (a bare wildcard or `examine
  apple` both lose).
- **Pick up the phone booth and Cry** (`Phoneb.taf`, 1372 bytes) — **WON,
  0/0**, 2 commands. `x me` silently completes a hidden gate, then `take
  phone booth` wins (grim flavour text, but a real `type=6 v1=0` EndGame).
  The game's own hinted `cry` first step is a red herring.
- **PTGOOD 8\*10^23** (`competition2006__adrift__ptgood__PTGOOD.taf`, 1971
  bytes, 2006 minicomp) — **WON, 0/0**, 6 commands. Open a window to unlock a
  room-exit shortcut, reach Slan's Bench, `open vial` to win.
- **Smote** (`smote.taf`, 1987 bytes) — **WON, 0/0**, 9 commands. Play the god
  Jimmy across three linked worlds (Water/Volcano/Desert); `get ice`, carry it
  + a pyramid to pop the volcano, melt and carry the ice to flood the desert —
  each step smites one world, the third ends the game.
- **JINXTRON** (`JINXTRON.taf`, 2179 bytes) — **no win/loss/score at all** —
  a pure dialogue toy (the childhood "jinx" game) driven by an 11-state
  `VAR1` conversation machine plus a recurring random-word interjection
  event. Banked as a demonstrative 7-command playthrough, same class as
  `Toxically_Earth`/`lifesimulation`. Re-checked 2026-08-23 and the whole
  loop is now banked too (`jinxtron_full_solution.txt`): the `Jinx` variable
  runs 1 → 2 (anything but `jinx` gets you jinxed) → 7 (five more turns;
  "Player, Player, Player — you're unjinxed") → 8 (at 7 the `Random_say`
  event announces "Oh, EDAM, by the way." each turn, and typing that same
  word back matches tasks 23–33, which compare the *referenced text* against
  the string variable `random_say`) → 9 (`jinx`, and now JINXTRON is the
  jinxed one) → 10 (say `jinxtron` three times, counted in `jinxtron_said`,
  and it is freed: "I'm free! Bwa hahaha!… Isn't jinx fun?"). Answering that
  question puts `Jinx` back to 2 and jinxes you again, so the machine is a
  closed loop — there are **zero** type-4 (score) and **zero** type-6
  (EndGame) actions in the file, and no reachable ending. The word to echo
  back is RNG-dependent, so the row is deterministic only under the
  harness's fixed seed.
- **Conversation With A Picture** (`Picture.taf`, 2257 bytes) — **WON, 0/0**,
  3 commands. Sit on a bench, `ask picture about bird` unlocks `ask picture
  about parrot`, which wins.
- **Rift** (`rift.taf`, 2606 bytes) — **WON, 0/0**, 3 commands. Unfinished
  intro/demo (the author's own WINTEXT says so): `move`, `x the floorboards`
  (article required), `x the machine`.

None of the eight needed an engine change; all are faithful joke/demo/
speed-IF material with no score to maximize. **171 unwired files remain**
after this batch — continue smallest-first through the 0-vocab-hit games,
and give the flagged AIF-vocabulary titles the same quoted-evidence
content-decline review the third wave used before deriving any of them.

**Second batch, 2026-08-23: 8 more wired, suite 269 → 277 rows, all PASS.**
Continuing smallest-first (2745–4401 bytes, all 4.00, all 0-vocab-hit).
Three of the eight needed `SCR_SKIP_WAITKEY=1` — a fork's own manual
transcript can look correct while actually desyncing on an unhandled intro
"press a key" prompt, so every incoming row now gets a real
`sh run_v4_walkthroughs.sh --bless` pass rather than trusting the worker's
self-reported transcript, same rule as the picture/smote fixes above.

- **The Foggy Banana Adventure** — **WON, 0/0**, 8 commands. One room, a
  strict TALK/INSPECT/USE chain; `use hoover`/`use phone` each consume a
  generic no-restriction task ahead of the real gated one in task order, so
  both must be issued twice.
- **Just Another Day** (2886 bytes) — **WON, 0/0**, 135 lines (mostly blank
  padding for "press any key" pauses; needs `SCR_SKIP_WAITKEY=1`). A
  Groundhog-Day time loop: a task21 AND gate requires four side-quest flags
  (pet the wolf, take the leaf, visit the old man, an undressed talk with
  the boss) before `Jump` is accepted from Outside; jumping resets into a
  parallel "empty" map whose real exit is `w` from Cubicles (`e` there is a
  dead-end joke room reprint).
- **Blast** (3447 bytes, Ectocomp 2008) — **WON, 0/0**, 25 commands. A
  100 HP demon roams 7 rooms on a deterministic patrol; four weapons sit on
  surfaces that must be examined before `get` works. Killing the demon (100
  damage total) sets a hidden "frag" flag that is the real unlock for an
  exit — a `Var1`-offset-by-2 restriction quirk made it look like a check on
  an unrelated "body" variable instead.
- **Pilfers** (3727 bytes) — **WON, 107/107** (max), 16 commands; needs
  `SCR_SKIP_WAITKEY=1`. Two-room escape/logic-puzzle skin: the Blue Room
  quiz answers are flavour-only, `DOOR:2` not `DOOR:1` (an unconditional
  death trap); the Red Room's window-break task has an AND `RestrMask`, so
  both `throw rock at window` and `push bed to window` are required.
- **The_Stowaway** — **WON, 10000/10000** (max), 16 commands. Climb to the
  crow's nest, dialogue a ghostly Strange Kid three times (catching him
  early is instant death), then during a lightning-storm timed event
  `use kid as a shield` — counter-intuitively the winning, max-score move is
  sacrificing the kid as a lightning rod.
- **Witness_Demon_vs_Vampire** (3849 bytes) — **WON, 0/0**, 13 commands;
  needs `SCR_SKIP_WAITKEY=1`. The game ships its own hint system giving away
  the solution outright: matches + oil + holy water from a cache, draw a
  pentagram (traps the demon), kill the vampire with holy water, lure the
  demon into the pentagram, light a match — order matters, holy water before
  the pentagram is a death trap.
- **The Vault** (4258 bytes) — **WON, 0/0**, 1 command. The intended
  item-fetch quest (cross to the Street, get a key off a dying man, unlock a
  drawer for a bible, return) is entirely bypassable: the win task
  (`read bible` in the starting room) has zero restrictions, so it fires
  immediately. Authoring bug, reproduced faithfully.
- **hiker** ("Conversation with a Hitchhiker", 4401 bytes, Ectocomp 2008) —
  **WON, 0/0**, 3 lines (2 blank + 1 command). The game itself labels three
  "Ending N of Three" branches; `kill the hitchhiker` is an explicit win
  task (Ending Three) — far shorter than surviving the doom-timer to Ending
  Two, and equally a clean win since there's no score to maximize.

**163 unwired files remain** after this batch.

**Third batch, 2026-08-23: 8 more wired, suite 277 → 285 rows, all PASS.**
Same discipline as the second batch: every fork was pre-briefed to self-check
`SCR_SKIP_WAITKEY=1` before reporting, but one (asteroid_after) still slipped
through with a solution that looked plausible without the flag and only
failed the win-marker check at real `--bless` time — reconfirming that a
fork's self-piped transcript is never a substitute for the real harness pass.

- **raccoon** (6 rooms, 0/0) — needs `SCR_SKIP_WAITKEY=1`. Trip the yard's
  traps with a thrown pebble, chew through the garbage-can lid's tie-cord and
  re-tie it to a heavy rock, splice the sleeping dog's leash onto that same
  cord, then wake the dog — an automatic chase sequence yanks the rock away
  and hauls the dog off, leaving the lid unguarded to open and descend into.
- **Way Out** (5 rooms, 0/0), 5 commands. A straight corridor north to the
  exit; optional look-left/-right side commands drain a "sanity" variable
  toward a stop-game threshold but are entirely avoidable.
- **The Fly Human** (9-room linear corridor, 21 tasks) — **unwinnable by
  design**: zero `ACT type=4`/`type=6` actions exist anywhere in the task
  dump, confirmed by the `score` command reading `0 out of a maximum of 0`.
  The final two rooms fire via automatic post-completion events, not player
  input; the closing line is authorial flavor text with no real EndGame.
- **zombiecow** (5-room comedy) — needs `SCR_SKIP_WAITKEY=1`. **WON,
  100/130** — the declared max double-counts two mutually-exclusive +30
  winning endings (`eat the clover` / `refuse clover`), only one of which is
  ever reachable in a single playthrough; 130 is not achievable.
- **outline** (3-room detective puzzle) — **WON, 5/5** (max), 16 commands.
  Objects on a surface (ruler/mug on the desk) are out of scope for
  `take`/`x` until the surface itself has been examined once; the win task
  only matches the literal pattern `x*outline`/`read*outline`, not a generic
  `examine`.
- **hungry** ("Ectocomp 2011", 9-room escape, 0/0), 7 commands. Grab the pot
  off the reception desk (invisible until the desk is examined), then smash
  the north office window with it before the randomly-arriving soldier shows
  up (he never actually enters within this short route).
- **The Long Barrow** (8-room dig/tunnel puzzle, 0/0), 19 commands. A
  countdown "dig" variable triggers a fall into the tunnels on the 3rd dig;
  tools must be fetched by climbing back up once the down exit unlocks. A
  lit torch is required before tunnel movement works at all, and a 5-turn
  suffocation timer starts on tunnel entry — digging a "dark patch" defuses
  it before pulling the final chamber's slab loose.
- **Asteroid Aftermath** (single-hub satellite realignment puzzle, 0/0) —
  needs `SCR_SKIP_WAITKEY=1` (the fork's self-check missed this: two
  "press any key" intro prompts silently ate the first two valve commands,
  producing a plausible-looking but wrong "Valve action not available"
  transcript that only surfaced as a real `--bless` REFUSED). Valve toggles
  silently relocate NPC satellites between camera rooms; a specific
  open/close sequence lands every required satellite group together, and a
  "*** ERROR ***" block on the close-valve step is authorial flavor text, not
  a real failure.

**155 unwired files remain** after this batch.

**Fourth batch, 2026-08-23: 8 more wired, suite 287 → 295 rows, all PASS.**
All eight were low/zero content-vocabulary-scanner hits (1-5 matches on
incidental words like "naked"/"strip"/"penis" in non-sexual horror or comedy
prose, confirmed as false positives before deriving); genuine AIF-flagged
titles remain deferred, untouched.

- **rollingthedough** (drunk sneak-into-bed comedy, sudden-death heavy) —
  needs `SCR_SKIP_WAITKEY=1`. **WON, 50/50** (max). Remove shoes before the
  creaky stairs, stash them specifically in the Bathroom, throw the rolling
  pin out the bedroom window, then lie on the bed — nearly every other move
  (lights, the dog, talking to the sleeping wife, keeping the pin/shoes) is
  an instant death branch.
- **The_Shuffling_Room** (horror vignette, 0/0), 10 commands. `release
  shoulders` must complete before `release hands` will succeed; feeling
  along the dark wall after falling free of the circle finds a hidden
  lightswitch that needs `use switch` typed twice (the first "resists" as a
  no-op flavor task). The "naked men"/"naked women" horror-atmosphere prose
  that triggered the vocabulary scanner is incidental, not sexual.
- **herrdoktor** (3-room comedy puzzle, 0/0), 15 commands. Bait a fishing
  pole with an acorn to lure a squirrel down from its tree, strap a jetpack
  cylinder to its back, fuel it with a de-linted sweetroll, then launch it
  down the well to rescue the trapped girl. The flagged "strip" hit was just
  the game's own `strip`/`rem*coat` parser verb.
- **The Angel the Devil and the Human** (river-crossing puzzle, 0/0), 24
  commands (leading "1" answers a mode prompt). Classic fox/goose/corn
  logic: never leave the Devil unsupervised with the Angel or the Human
  (Angel+Human alone is always safe); ferry all three to Heaven and the win
  fires automatically once they're all present. The flagged "penis" hit was
  one incidental comedic aside, not explicit content.
- **Existence** (IntroComp 2009, 3-room ghost vignette, 0/0) — needs
  `SCR_SKIP_WAITKEY=1`. 5 commands (1 blank to dismiss the intro pause).
  `use fan` sucks the player through and empowers them, unlocking `use
  pencil` as the win.
- **zacksmackfoot** (3-room teaser demo, 0/0) — needs `SCR_SKIP_WAITKEY=1`.
  5 commands, single unconditional "stop" ending reached the instant the
  player exits the wreck. Take the briefcase first for the better epilogue
  text (cosmetic only, since the game is unscored); open the penknife and
  jam it in the cargo door's emergency slot to free the jam.
- **boiled eggs** (no scoring, single win ending), 19 commands. Pump an
  NPC's dialogue tree (in strict topic order) for the spare-key location and
  a neighbor's hidden box, unlock the front door, hide under the bed until
  the neighbor falls asleep, then take the box and climb out the window.
- **P2P** ("Point 2 Point" steeplechase reflex race) — needs
  `SCR_SKIP_WAITKEY=1`. **WON, 30/30** (max), 4 commands. Jump the first
  obstacle, talk to a rival rider to spook the horse blocking a turn (no
  actual steering needed), then jump and turn past the final two hazards.

**147 unwired files remain** after this batch.

**Fifth batch, 2026-08-23: 8 more wired, suite 295 → 303 rows, all PASS.**
All eight were the smallest remaining zero-content-vocabulary-scanner-hit
titles by file size; genuine AIF-flagged titles remain deferred, untouched.

- **MurderMansionntro** (3-room promo teaser, 0/0) — needs
  `SCR_SKIP_WAITKEY=1` (a "(Please press any key)" pause after the Credits
  screen otherwise eats the next scripted menu choice). Not the full "Murder
  Mansion" game, just a walk-up-and-knock demo with no win/end condition at
  all (no `ACT type=6` anywhere): work the intro menu, examine the stoop
  objects, then bang the door knocker to reach the fixed closing-credit
  screen, the fullest reachable content.
- **whitterscap** ("Whitterscap's Key", Q-key running-gag comedy) — **WON,
  2/2** (max). Give a shiny button to Charles to reach the Forest, decode
  Brelgan's runes with a decoder, pick the Zenes spell over the alternative,
  wait for Whitterscap to appear in the Secret Passage and steal his key with
  `zenes`, then type any Q-word (but not `quit` itself) for the score bonus
  before quitting to win. A second, mutually-exclusive +1 "blezwif" ending
  requires still holding the button while in the Forest — unreachable, since
  the only route into the Forest consumes the button.
- **The Dangers of Driving at Night** (unscored horror vignette, 0/0) — needs
  `SCR_SKIP_WAITKEY=1` (an unlogged "(Press a key)" pause after a scripted
  roadside near-miss otherwise eats the next `n`). Drive north through the
  accident event, pay the gas station clerk, spare teenager Chris some
  change, refuse trucker Harold's ride (accepting is presumably a dead
  branch, not explored), then talk to Chris again so he reveals the back
  exit and the ending fires.
- **All Hallows Eve** (3-room Halloween vignette, 23/26 true single-playthrough
  max — the nominal 26-point sum is unreachable in one game since the last
  two endings are mutually exclusive) — needs `SCR_SKIP_WAITKEY=1`. Brew a
  love potion from toad eggs (sing to the toads), purple beetles (shake the
  bush), and bird-bath water poured into a cauldron hidden under the grass,
  then trap the witch's wandering cat and ransom it back to her for the
  higher-value ending (+15) over just throwing the potion at her (+3).
- **gorxungula** ("Gorxungula's Curse", unscored surreal fantasy, 0/0) —
  needs `SCR_SKIP_WAITKEY=1` (the very first "(Press any key)" intro pause
  otherwise eats the gender answer and desyncs into an infinite reprompt
  loop). No gold coin exists anywhere in the world at start; it can only be
  obtained by deliberately walking into the death room and typing `restart`
  from the death screen (a live in-game `restart` does not grant it). Trade
  the tome sitting in Elder Moose's tub to Clathering for a beverage
  ("spirits"), then offer both the coin and the spirits in the tub to win.
- **lobster** ("Attack of Doc Lobster's Mutant Menagerie of Horror",
  unscored monster-factory sim, 0/0). Each round names a new species, then
  hands the Servant a fixed instrument sequence; repeating the
  scalpel+sprinkles+envenomator+serum combo across 6 named species
  deterministically drives a hidden `death` counter (with non-random,
  flat deltas for this specific combo) past the 6000 threshold the win task
  checks. `SCR_SKIP_WAITKEY=1` is not needed here — the script's own blank
  lines correctly satisfy every in-game pause, and adding the flag instead
  desyncs the placeholder blanks into extra harmless empty-command turns.
- **Business As Usual** (unscored museum tidy-up puzzle, 0/0), 27 commands.
  Nine background NPCs steal and relocate three of four "Grand Items" on a
  fixed schedule that finishes by turn 16; grabbing an item early doesn't
  protect it, since the scripted thefts strip it from the player's inventory
  regardless of who holds it. Wait out the disruption, then shuttle the
  displaced items home one at a time (carry capacity is exactly one) before
  the boss's turn-~26 inspection. Bare noun words like "book"/"lamp" get
  globally synonym-rewritten to "go to X room", so use `take all`/`drop all`
  instead of naming objects directly.
- **Oh_Human** (escape-room dead-end trap, 60/200) — the apparent 100-point
  "climb the ladder" branch requires the crate to be sitting on top of the
  box, a state provably unreachable from the task/action graph (the crate's
  only two possible locations never overlap with the box's static room), so
  it and the 40-point fourth "hole" jump that gates it are dead ends/red
  herrings. The real ending: drop the electrical device once past the first
  hole to free its orbiting light, pick the light up, and while still in the
  `theroom==4` window (i.e. before taking the fourth hole-jump, which
  permanently exits that window) use it to cut through the walls and escape
  through the door — the only text reachable at all, since the game has no
  `ACT type=6` EndGame action anywhere in its 11 authored tasks.

**139 unwired files remain** after this batch.

**Sixth batch, 2026-08-29: 8 more wired, suite 303 → 311 rows, all PASS.**
Continuing smallest-first (7,631–8,458 bytes, all 4.00), skipping
`Sex is Mental.taf` (8,373 bytes, 82 vocabulary hits — genuinely flagged,
left for a separate content triage rather than derived). Derived in parallel,
one background agent per game as in the fourth-wave batches, then merged and
re-blessed centrally through the real harness (`sh run_v4_walkthroughs.sh
--bless <name>` per row, then a full clean run — no NEEDGOLD, no FAIL).

- **The Skydiver** (7,631 bytes) — **WON 1000/1000**, the true maximum: eight
  `ACT type=4` awards on the taken path sum to exactly 1000, dominated by a
  single +900 for `inflate parachute`. TASK13/TASK14 are two identically-
  scored (+15) mutually exclusive quilt-fix branches on the same state; the
  lower-indexed one always wins, so the route must go through the shoelace
  branch to keep the yarn a later step needs. TASK16 (unscrew bottle) is a
  dead author bug — its restriction wants an object nothing in the game ever
  creates — invisible only because the later task's `RestrMask` ORs the
  working branch in without it. No score summary prints at all (`MaxScore`
  is 0 in the authored file even though the engine score reaches 1000), so
  the row's win marker is the game's own darkly-comic truncated closing
  line, not a score string. 23 commands, no env.
- **the_road** ("The Road Leads to Nowhere", Hourglass comp, 7,903 bytes) —
  no score anywhere in the file (all 32 tasks score=0), a single linear
  story reaching its one non-death ending. Two object-*seen*-model gates
  (the backpack, the fireplace sticks) and a four-painting digit cipher for
  the trapdoor code. Two fixed turn-timers pace the endgame — an 11-turn
  cabin collapse and an exact 8-turn walk-to-realization once on the Road.
  42 commands, `SCR_SKIP_WAITKEY=1`.
- **The Perfect Spy** (7,988 bytes) — **WON 10/10**, the true maximum (four
  `ACT type=4` awards, all fired). Transformation "done" flags are
  *transient*: an exit gated on "change into mouse" done really means
  "currently in mouse form", because turning human again unsets the flag
  with its own `ACT type=5` — so the route must re-transform immediately
  before each mouse-only transit rather than relying on an earlier
  transformation. The keycard and the guard's-leg climb are form-gated in
  opposite directions on purpose. 19 commands, no env.
- **seciden_oddcomp** ("Return to the Forest House", Seciden Mencarde, Odd
  Comp 2008, 8,019 bytes) — **WON 102/102**, the true maximum (six
  `ACT type=4` awards of 17 each, the GOOD ending). A silent, unconditional
  17-turn "Beast Kills Susie" timer runs from turn 0 (the Beast is already
  in the room; its "appears" text is misleading flavour), and draining its
  fang after the kill is literally the timing event's own `pauseTask`,
  cancelling the clock. 21 commands, `SCR_SKIP_WAITKEY=1`.
- **Perspectives** (Justahack, 8,043 bytes) — no score anywhere in the file
  (zero `ACT type=4` across 14 tasks), a four-ending no-score game; this
  route reaches the richest, the "Negotiation Style Ending" (same convention
  as *Everything Emanuelle*/*S Tar Dus T*). **TASK 10, the "Heroic Actions
  Ending", is provably dead**: TASK 9 has the byte-identical attack pattern
  with no restriction and a lower index, so the first-match scan always
  intercepts it regardless of game state — a whole ending lost to task
  ordering, not just points. 17 commands, `SCR_SKIP_WAITKEY=1`.
- **Big City Laundry** (8,088 bytes) — **WON**, no score system at all (zero
  `ACT type=4` across 30 tasks), the game's one good ending. Real
  engine-fidelity witness: the robbery event is a real-time window keyed on
  the back door's **open/closed** state, not its lock state — leaving it
  unlocked-but-open across a timeskip fires a silent loss with no parser
  warning, the fourth silent-loss shape catalogued in this corpus. 78
  commands, no env.
- **Over the Edge** (Ren, Hourglass comp, 6 Aug 2006, 8,128 bytes) — a WWI
  shell-shock vignette with no score and no formal `ACT type=6` anywhere;
  ends by reaching a literal credits-screen task rather than a win/lose
  call, so there is nothing to lose, only a route to find. The literal
  command `open your eyes` (not the passive Groundhog-Day loop TASK 0
  otherwise runs) is the true awakening. 23 commands, `SCR_SKIP_WAITKEY=1`.
- **Drinks** (8,458 bytes) — **WON**, no score system at all (zero
  `ACT type=4`, one `ACT type=6` win on "open casket"), a Victorian
  post-dinner ghost story, one puzzle, one ending. The casket is a dynamic
  object seated in the room from the start but tagged unseen until entry —
  another object-*seen*-model instance, so `go south` must be the very first
  command. 18 commands, `SCR_SKIP_WAITKEY=1`.

Two vocabulary-scanner false positives worth recording since they came up
twice this batch: "sex" as the idiom "the fairer sex" (seciden_oddcomp), and
"strip"/"striped" describing tiger fur after a transformation (The Perfect
Spy) — both read in full context before deriving, per the standing content
policy.

**134 unwired files remain** after this batch (recomputed via the set-
difference recipe below, not carried forward by hand).

**Seventh batch, 2026-08-29: 8 more wired, suite 311 → 319 rows, all PASS.**
Continuing smallest-first (8,861–9,909 bytes, all 4.00). Vocabulary-scanned
first as always; seven were clean false-positives (cockpit/strip
mall/thrust-into-celebrity, click-not-lick, Abstract-Algebra-not-bra, etc.)
and one, *The Worst Game In The World... Ever!!!*, was genuinely flagged —
read in full before deriving, confirmed comedic/parody with no underage or
non-consent indicators, and wired on the *Diary of a Stripper* terms (row
committed, solution/golden gitignored). Derived in parallel, one background
agent per game, merged and re-blessed centrally as usual. **Caught a real
merge-time bug this round**: two of the eight agents (foresthouse2, takeone)
wrote a `#`-commented documentation header into their `_solution.txt` files
despite being told to write a plain command list — harmless-looking, but
`transcript()` does a bare `cat "$2"` into the engine's stdin, so every
comment line would have been typed as a literal (nonsense) command and
silently desynced the whole transcript. Caught by inspecting file sizes
(3.8–4.2 KB for 22–34 short commands, obviously too large) before blessing,
not by the bless step itself, which would have "passed" against its own
wrong golden. Stripped to bare command lists and re-verified against the
agents' own reported command counts before proceeding.

- **R2DC** ("Return to Dracula's Castle II: Revenge of Dracula's Castle",
  comedy by "Arthur Winslow", 8,861 bytes) — **WON 1,000,000/1,000,000**,
  the true maximum (a single `ACT type=4` in the whole file). TASK 24
  (`climb ladder`) has no `COMPLETE=` text at all, so a task that actually
  fires and moves the player still prints the library's generic
  verb-failure line — a task that *works* while *looking* like a failure,
  the inverse of the usual silent-loss shape; score-neutral, sidestepped
  by using the plain `u` exit. 11 commands, `SCR_SKIP_WAITKEY=1`.
- **The Forest House [Mini-Game, v.2]** (Seciden Mencarde, 2007 Ectocomp,
  9,476 bytes) — **WON 13/13**, the true maximum; the game's own
  `Globals.MaxScore` is 12, one short of what its own tasks actually pay
  out, an authoring bug the engine faithfully reproduces ("You scored 13
  out of the maximum 12!"). Both endings gate on a single `injured`
  variable set only by the inferior thorn-crossing branch, so the
  sweater+stick combo is the only route that both scores and survives.
  34 commands, `SCR_SKIP_WAITKEY=1`.
- **The Shetland Enigma** (9,485 bytes) — **WON, score 210** — provably the
  ceiling (18 non-exclusive awards summing to 210, all fired); the game's
  own declared "maximum of 100" undercounts its own scoring by 110, another
  authoring quirk reproduced verbatim. A startup-screen variant of the
  object-*seen* model: the boot room description lists the ice chunk but
  doesn't mark it seen, so `take ice` as the literal first command fails
  until an explicit `look` re-seeds it. 66 commands, no env.
- **Take One** (Robert Street/"Rafgon", finish-the-game-comp-2005, 9,547
  bytes) — **WON**, no score system, the game's only ending. A demon-arrival
  turn-counter must land on exactly 16 while the jewel sits caged and the
  switch is off, routing the demon to eat the jewel rather than catch the
  player — 8 explicit waits hit the threshold. 22 commands,
  `SCR_SKIP_WAITKEY=1`.
- **Tenebrae Semper** (Seciden Mencarde, EctoComp 2010 "3 Hours", 9,757
  bytes) — **confirmed unwinnable**, not merely unreached: all three
  authored endings are dead by construction (one task has zero `ALTCMD`
  entries and can never match input; the sole door to the other two
  endings' shared prerequisite room is gated by a task with no `ACT`
  entries at all, so its `CompleteText` alone marks the command handled
  and the engine never falls through to real movement) — verified against
  `run_all_commands()`/`task_run_task_unrestricted()` as faithful Runner
  behaviour, a genuine author defect. Row demonstrates the fullest
  reachable content instead. 34 commands, `SCR_SKIP_WAITKEY=1`.
- **Helsing** ("Steve Van Helsing: Process Server", 9,776 bytes) — **WON**,
  no score system, the game's only ending. A flavor-only jukebox command
  (`play [track] #4`) turns out to be a silent hard prerequisite for two
  unrelated-looking later tasks, with no in-game hint connecting them
  except a callback in the win text. 8 commands, `SCR_SKIP_WAITKEY=1`.
- **The Worst Game In The World... Ever!!!** (9,858 bytes) — **AIF,
  solution/golden gitignored**; comedic, deliberately misspelled,
  non-explicit content between consenting adults, confirmed clean on a
  full-transcript read. No score, no formal `EndGame` action anywhere — a
  branching scene-selector tree, so the row takes the richest single
  reachable path (the *S Tar Dus T* convention) rather than enumerating
  every branch. Two real engine-faithful author bugs: the game's own
  `SYNONYM` table rewrites "shoot" to "fire" before task matching, but the
  intended win task is authored on the literal pre-synonym verb and can
  now never match anything — permanently dead code that also seals off
  four otherwise-unreachable rooms; and a copy/paste room-mismatch bug
  turns one of three sibling sub-branches into a genuine soft-lock. 17
  commands, `SCR_SKIP_WAITKEY=1`.
- **Spooked! The Wonders of Science** (T.D.S., 9,909 bytes) — **WON 8/8**,
  the true maximum, matching the game's own printed maximum exactly. One
  room is genuinely unreachable by design (its only entrance and its exits
  gate on the same task's done-state in opposite directions) — the ending
  text explicitly lists it as an unanswered episode-2 hook, so this isn't
  a missed puzzle. 34 commands, `SCR_SKIP_WAITKEY=1`.

**126 unwired files remain** after this batch.

**Eighth batch, 2026-08-29: 8 more wired, suite 319 → 327 rows, all PASS.**
Continuing smallest-first (2,860–9,909 bytes, all 4.00). Vocabulary-scanned
first as always; one hit worth a closer look — *Video Tape Decay*'s
"incest" — read in context and confirmed to be abstract, non-explicit
theological backstory dialogue with no minors, proceeding under normal
wiring. *thelasthour* ships its own in-game content-warning screen for a
scripted hate-crime execution narrative (real historical KKK/MLK excerpts);
serious and dark, but no sexual content, so it likewise proceeds under
normal wiring rather than the AIF treatment. Derived in parallel, one
background agent per game, merged and re-blessed centrally as usual.

**Caught two real derivation failures this round, both by re-running the
literal harness invocation before blessing rather than trusting the
agents' self-reports** — the project's standing discipline paid off twice
in one batch:
- *Choose Your Own Three Hour Adventure*'s first-pass solution was
  reported "WON 14/14 (true max)" but the game has **no score at all** in
  its `SCR_DUMP_TASKS` field (that field is always 0 there — the real
  running score lives in a separate variable, only visible in the ending
  text). Worse, replaying the file verbatim showed it didn't even finish:
  it stalled mid-tree at a fresh menu, and one of the 14 commands had
  actually been an out-of-range menu number that wasted a turn on an error
  message. Re-derived from the task graph's path-gating variables into a
  genuine ending with a real score line.
- *thelasthour*'s first-pass solution (119 commands, mostly `wait`) was
  reported as reaching "the game's only ending" but replaying it verbatim
  showed it stuck forever on trailing `wait`s, never ending. The sole win
  action turns out to be gated by nothing but a pure turn-count `EVENT`
  (fires automatically at turn 120) — the file was exactly 2 turns short
  of that threshold. Fixed by extending the trailing wait block; confirmed
  1 extra `wait` still isn't enough and 2 is the tight minimum.

- **video.tape** (*Video_Tape_Decay.taf*, T.D.S., EctoComp three-hour
  speed-IF) — **WON**, the game's only ending, no score system. `RESTR type=0 v2=4` on the ten sacred-relic prayer
  restrictions means "inside container idx `v3-1`", i.e. every relic must
  be placed *inside* the church bowl, not merely carried — the failure
  text gives no hint of this. The unlock combination "1850" is on the
  cemetery tombstone, not the more obvious Cinema note (a decoy). Weight
  cap is exactly tight (10 relics × 9 = 90 = maxwt), so every one-time tool
  must be dropped immediately after use. 139 commands, `SCR_SKIP_WAITKEY=1`.
- **Regrets** (2,860 bytes) — **WON**, the game's only ending, no score
  system (every task's score field reads 0 — a first-pass agent
  misreported this as "9/9"). `SCR_SKIP_WAITKEY=1` is required: without it
  a late `...press a key...` prompt silently swallows the harness's
  terminating `quit`, desyncing the transcript instead of reaching "The
  game has ended." 12 commands.
- **Terrified** (Eric T. Dorrath, NaAdWriMo 2007) — **WON, 60/65 (92%)**,
  the true reachable maximum. TASK89 (a "crossed the fence" +5 bonus) is
  structurally unreachable: it's invoked via `ACT type=5` from the
  west-crossing tasks, but by the time it runs, that same task's own `ACT
  type=1` has already changed the player's room, so TASK89's `where=1
  room=19` check always fails against the *new* room — a genuine
  authoring bug in the original game, faithfully reproduced. Wearing the
  starting boots on the Gravel Path triggers a fatal noise-capture in one
  room; `remove boots` before that leg. 57 commands, no env.
- **Bringing the Rain** (*rain.taf*, 3,157 bytes) — **WON**, the game's
  only ending, no score system. A newly-seen inconsistency: `ACT type=0`
  (move-object) uses a 1-based/raw−1 room index while `ACT type=1`
  (move-character) uses a plain 0-based index, in the same file. Also: a
  task that only matches its pattern a full turn after the triggering
  event, not immediately; an `ALTCMD` list where only one of three
  candidate strings actually matches at runtime; and a strict, no-slack
  7-turn "about to be caught" countdown. 33 commands, `SCR_SKIP_WAITKEY=1`.
- **howitstarted** (2,860 bytes) — **WON 6/6**, the true maximum (100%). A
  short, linear, fourth-wall-breaking prequel vignette ("And...that's it.
  Sorry, I mean, I know this isn't the end of the story...") that still
  formally wins via its own `EndGame`/score summary. 29 commands, no env.
- **Station XIII** (sequel to *The Shetland Enigma*) — **WON 200/200
  raw**, every one of 14 `ACT type=4` awards fires, no mutually-exclusive
  or unreachable points. The declared `Globals.MaxScore` field is a stale
  9, so the engine reports "You scored 200 out of the maximum 9! That is
  2222% of the game!" — another genuine authoring bug faithfully
  reproduced. The stepladder is dropped in-room by each of three separate
  `climb ladder` tasks, so it must be re-fetched and hauled back for two
  of the three climbs rather than carried once; seven distinct
  object-*seen* surfaces gate key items; weight cap `wt<=108` is hit
  exactly at the route's peak. 93 commands, no env.
- **Choose Your Own Three Hour Adventure** (100-task branching CYOA) —
  **WON, score 9/14** (see re-derivation note above for why the first pass
  was wrong on both the score mechanism and the ending). `SCR_SKIP_WAITKEY=1`
  is required — a `[MORE]`/wait pause otherwise eats the next menu choice
  and desyncs into a death. 13 commands.
- **thelasthour** (Roberto Grassi, 2004) — **WON**, the game's only
  ending, no score system; win is gated purely by a turn-count `EVENT`
  (fires at turn 120, no other restrictions) — see re-derivation note
  above. 121 commands, no env.

**116 unwired files remain** after this batch.

**Ninth batch, 2026-08-29: 8 more wired, suite 327 → 335 rows, all PASS.**
Continuing smallest-first (8,373–11,380 bytes, all 4.00). *Sex is Mental* is
the smallest remaining file and the long-deferred, heavily-flagged title
noted in earlier project notes; read in full and confirmed comedic explicit
content between two apparent adults (a psychiatric-ward patient and an
on-site nurse), no minors, a third character's rape threat used only as a
narrative danger to avoid, never depicted — wired under the AIF treatment
(solution/golden gitignored), matching the *Diary of a Stripper* precedent.
A vocabulary scan of the next 7 candidates flagged four more hits, all
resolved as false positives on close reading: "teen" in *Pete's Punkin
Junkinator* ("teeny tiny", "group of teenagers" buying junk food), "cock" in
*The Crooked Estate* (three "peacock feathers" hits), "rape" in *Alias
Undercover Agent* (a substring of "scraped"), and "minor" in *reactor_1*
("minor bombardments", a shield-status readout). *A View to a Home* has a
"young girl...suicidal" NPC vignette (resolved via a religious book/hope
gesture) — dark subject matter, no sexual content, proceeds under normal
wiring per the *thelasthour* precedent. Derived in parallel, one background
agent per game, merged and re-blessed centrally as usual; all 8 self-reports
were independently re-verified against the literal harness invocation
before blessing (no fabricated claims caught this round).

One tooling footgun hit during the merge: `Edit` on
`harness/run_v4_walkthroughs.sh` silently dropped the script's executable
bit both times it was written this batch, breaking the very next `--bless`
invocation with a plain shell "permission denied" — fixed with `chmod +x`
before each bless pass. Also, two of this batch's win markers
(*The Crooked Estate*, *A View to a Home*) were drafted as the full closing
sentence, but the game's own line-wrap split each one across two lines,
so the harness's `grep -F` (line-oriented) never matched — both were
re-blessed after trimming the marker down to only the text that stays on
one line, per the project's existing "closing line wraps" convention.

- **Sex is Mental** (AIF, 8,373 bytes) — **no score system** (`MaxScore=0`,
  zero `ACT type=4`). The sole `ACT type=6` win is a comedic twist ending
  (the "nurse" the next morning turns out to be someone else entirely). 33
  commands, no env. Solution/golden gitignored.
- **Pete's Punkin Junkinator** — **WON 505/575** — six one-time production
  tasks sum to a declared max of 575, but an internal auto-task ends the
  game the instant a 4th punkin is produced, so only the best 4-of-6 subset
  is ever reachable in one playthrough; 505 is the true, provably
  unreachable-beyond ceiling, not a missed-content gap. 27 commands, no env.
- **The Crooked Estate** (8,745 bytes, Duncan Bowsman) — **unfinishable by
  design**: a one-room literary/atmospheric horror piece with zero `ACT
  type=4` and zero `ACT type=6` anywhere in its 58 tasks, and an empty
  `WINTEXT`. Screaming triggers a cascading sequence that resets the game's
  completed-task flags back to the opening state — the mechanical
  embodiment of the game's inescapable-loop theme, not an ending. `quit` is
  overridden by a custom in-fiction refusal rather than the engine's real
  meta-quit. Golden is a demonstration route exercising every implemented
  verb/scenery noun once. 45 commands, `SCR_SKIP_WAITKEY=1`.
- **Alias Undercover Agent** — **WON 35/35**, the true and declared
  maximum. Object-*seen* gate on the kitchen napkin; two distinct,
  separately-locked grate objects needing different verbs (`unscrew` vs.
  `unlock`+`open`); a safe combination that only registers via `examine
  dial` after each `turn dial to N`. 41 lines (name-prompt response + 40
  commands), no env.
- **A View to a Home** — **completed** (all three medals collected) — no
  scoring system at all, so completion is defined by the collection goal.
  Puzzle chain: bird's-nest key opens a locked closet for the bronze medal;
  a water-logged kitchen note (readable only once a background random
  event leaves the sink non-full) gives a safe combination for the silver
  medal and a text-maze route for a Rubik's cube; solving the cube against
  a second randomly-cycling jacuzzi state yields the gold medal. 122
  commands, no env.
- **briefcase** (Julius the master-thief) — **WON**, the game's only
  ending, no score system. A tight two-hidden-event timing puzzle: taking
  the briefcase only sets a flag, with a 1-turn-delayed event actually
  moving it into inventory and a 2-turn-delayed event locking the study
  door (which blocks the win task once fired) — `open case` must land in
  the exact 2-filler-turn window between the two. 19 commands, no env.
- **The_Seance** — **WON 100/100**, the true maximum — the game's own
  declared max is a stale 0. A hard real-time trap requires `open door` as
  literally the first command (dawdling 3 turns loses the game). The game's
  own SYNONYM table maps bare `n` to the yes/no verb before movement, so
  full direction words are required. Two alternate win endings (`yes`/`no`
  to join a ghost) both fire `ACT type=6`; `yes` scores higher. 18
  commands, `SCR_SKIP_WAITKEY=1`.
- **reactor_1** (ESS Chance: Reactor 1, Justahack) — **WON**, one of two
  endings (a heroic-sacrifice death ending also exists), no score system.
  Each of three mutually-exclusive repair attempts rolls a random outcome;
  this seed's first attempt locks out, the second succeeds. Closing out an
  NPC's radio conversation with a plain reply is required before `access
  computer` — leaving it open makes a bare menu choice resolve to the
  conversation instead. 11 commands, `SCR_SKIP_WAITKEY=1`.

**112 unwired files remain** after this batch.

**Tenth batch, 2026-08-29: 8 more wired, suite 335 → 343 rows, all PASS.**
Continuing smallest-first (11,579–14,070 bytes, all 4.00). A vocabulary scan
of the 8 candidates found no genuine hits requiring the AIF treatment this
round — all 8 proceed under normal wiring. *A View to a Home*'s "young
girl...suicidal" precedent recurs once more: *ForestHouse3* has a
childhood-death backstory revealed through dialogue in a time-travel/coma
narrative, dark theme with no sexual content, wired normally per the
*thelasthour* precedent. Derived in parallel, one background agent per game,
merged and re-blessed centrally as usual; all 8 self-reports were
independently re-verified against the literal harness invocation (byte-
identical md5 across 3 runs each) before blessing — no fabricated claims
caught this round.

One win marker (*ForestHouse3*) was first drafted as a full closing sentence
that the game's own line-wrap split across two lines, so the harness's
line-oriented `grep -F` never matched (`--bless` reported REFUSED); fixed by
trimming the marker down to the portion that stays on one line, same fix
pattern as the ninth batch's two wrapped markers. The executable-bit-
stripping `Edit` footgun from the ninth batch did not recur this time — the
script kept its `+x` bit after both edits, so the bit was only defensively
re-checked (`chmod +x`) rather than needed.

Re-running the unwired-file count via `comm -23` after this batch (comparing
all `games/*.taf` basenames against every distinct `.taf` named in the
manifest) gives **98 unwired files remain**, not the expected 112−8=104 —
the corpus itself is 427 `.taf` files, not the 433 implied by an earlier,
less rigorous count in these notes; trusting the freshly recomputed figure
over the stale one, as usual.

- **Motion** — **WON 100/100**, the true maximum (three `ACT type=4` awards
  of 25+25+50 across 68 tasks). A three-stage rocket minigame (launch, land,
  drive-to-recover) driven almost entirely by bare-Enter "wait" moves plus a
  handful of `f`(orward)/`next`/`r`/`l` commands. Win-check tasks run one
  turn behind each stage's own state-update task, so one extra confirming
  turn is needed once a threshold is first reached, and Stages 1-2 (not the
  final Stage 3) need a second `next` to advance past the shared "Won!"
  room. 137 commands (115 blank Enter presses + 10 `f` + 8 `next` + 2 `r` +
  2 `l`), `SCR_SKIP_WAITKEY=1`.
- **tophat** — the game's only ending, reached in three commands — no
  scoring system. A one-room vignette narrated from inside a magician's top
  hat; the assistant pops up (`up`) three times in a row, each with
  different flavor text, before being sent back down for good.
- **3 minutes1.0** ("Three Minutes to Live" by Ren, Hourglass Competition) —
  reaches the best of four possible endings (one survival, three death) —
  no scoring system. Free arms via `pull rope real hard right` (an
  RNG-fixed variable deterministically resolves to 1, making "right"
  correct); steer a rotating saw to cut both ankle ropes; saw off a
  coroner's hand to open a scanner-locked locker; solve a combination
  entirely via direct-placement commands (roulette ball, dice, cards), with
  zero reliance on RNG. Reconfirms the object-*seen* model: `take
  jack`/`take ace` fail until `x table` first makes them referenceable. 28
  commands, `SCR_SKIP_WAITKEY=1`.
- **neighbours** — **WON 100/100** via a custom evidence variable (no
  built-in ADRIFT score/EndGame actions) — six score-band `call police`
  tasks dispatch on the final tally. An old-bones dig task (+3) is
  permanently shadowed by an earlier wildcard `*dig*` task sharing the same
  restriction, so it never fires (an authoring bug, confirmed live) —
  skipped in the golden. All guaranteed one-time evidence sums to 94, so the
  golden repeats `x boxes` in the Cellar (an uncapped, likely-unintended +3
  each time past the first) twice more to land on exactly 100; a fifth
  evidence source (Crumm's-Garden dig, +5) is deliberately left untouched
  since taking it would overshoot 100 and soft-lock the ending. 64 commands,
  `SCR_SKIP_WAITKEY=1`.
- **The First To Arise Alone With A Pug** — **WON 100/100**, the true
  maximum. First chapter of a larger series — unlocking and opening the
  front door (the latter requires summoning an in-fiction power, `open
  front door with danthil`) ends the chapter. 40 commands, no env.
- **ForestHouse3** — reaches the game's only ending — no scoring system at
  all. A time-travel/coma narrative resolving a family tragedy; contains a
  childhood-death backstory revealed through dialogue — dark theme, no
  sexual content, proceeds under normal wiring per the *thelasthour*
  precedent. 72 commands, no env.
- **DayAtTheOffice** — **WON**, an intentional overachievement ending — the
  in-game `score` command tops out at 60, but the closing narrative
  separately tracks a 1-7 "performance" scale and this playthrough reaches
  8, one better than that scale's own maximum. 38 commands,
  `SCR_SKIP_WAITKEY=1`.
- **beer** — **WON 50/50**, the true maximum, one of at least two possible
  endings. Digging in the outback Bush comes up empty; the win path is
  `search dirt` there instead, finding a pouch that wins the game outright.
  80 commands, no env.

**98 unwired files remain** after this batch.

**Eleventh batch, 2026-08-29: 8 more wired, suite 343 → 351 rows, all PASS.**
Continuing smallest-first (14,541–18,973 bytes). A vocabulary scan of the 12
smallest unwired candidates surfaced one genuine decline and one AIF title:

- **`delight.taf` — declined, not wired.** Context-read of its 4
  "underage"/"underaged" hits confirms it depicts a sexualized minor NPC ("an
  underaged girl with pointed ears"), combined with an extreme overall
  explicit-vocabulary hit count (262). This is the same category already
  declined for `enc1.taf`, `windy.taf`, `enc2.taf`, and `Buffy Before the
  Date.taf` — content depicting sexualized minors is refused outright, never
  derived or wired, regardless of corpus-completion pressure.
- **`Temple_Of_The_Sun.taf` — AIF treatment.** Confirmed adult-only jungle-
  temple "maiden" seduction content between adult characters, no minors.
  Wired normally (row + comment block committed) but its solution/golden are
  gitignored per the `gamma`/`croft`/`Doctor Who and the Vortex of Lust`
  precedent — engine coverage without committing the explicit text.

Every other flagged hit in the remaining 10 smallest candidates was a
confirmed false positive on context read (an ice-cream-eating bystander's
"child", a witch's "Hello, child" greeting, "cucumber" substring-matching
"cum", "parapets" substring-matching "rape", an incidental unrelated "minor",
a joke line, and — in *The_Final_Question* specifically — an in-game
"credits reel" easter egg quoting *other*, unrelated David Whyld games'
own blurbs, one of which is self-labeled adult comedy; none of that is
gameplay content of the game being wired). *Patient7*'s dying-child-in-
hospital horror premise is dark/serious but not sexual, proceeding under
normal wiring per the *ForestHouse3*/*thelasthour* precedent for non-sexual
dark themes.

Derived in parallel, one background agent per game, merged and re-blessed
centrally as usual; all 8 self-reports were independently re-verified against
the literal harness invocation before blessing — no fabricated claims caught
this round. One win marker (*Temple_Of_The_Sun*) was first drafted as a full
closing sentence the game's own line-wrap split across two lines ("...this!
" / "Congratulations!"), so `--bless` reported REFUSED; fixed by trimming the
marker down to the portion that stays on one line, the same recurring fix
pattern as the ninth and tenth batches' wrapped markers. The executable-bit-
stripping `Edit` footgun did not recur — the script kept its `+x` bit through
both edits this batch, defensively re-checked (`chmod +x`) regardless.

Re-running the unwired-file count via `comm -23` after this batch gives **90
unwired files remain** (98 − 8, exactly as expected this time).

- **Mr_Fluffykins_Most_Harrowing_Misadventure** — reaches the game's only
  happy ending — no scoring system at all (zero `ACT type=4`). A
  Choose-Your-Own-Adventure gamebook wearing a parser: one nominal room,
  three variables, 25 tasks that just jump the story via `turn to page N`.
  The only formal `EndGame` in the whole file is the punitive "impatience"
  death; the genuine win is terminal prose with no EndGame action of its
  own. 5 commands, `SCR_SKIP_WAITKEY=1` (an embedded mid-passage waitkey
  otherwise eats the final scripted command).
- **A Witch Tale** — win-only, no scoring system. The single ending is a
  deliberately anticlimactic gag (forgets the magic words, turns into a
  tree) that the narration lampshades as fake before printing THE END — no
  actual "say magic words" task exists. Confirms a second, distinct
  event-object-move decode convention (`room = destination - 2`, on top of
  the ordinary `Obj2Dest(raw)-1`) separate from ordinary TASK move-object
  actions — worth flagging for future event-driven object placement work,
  no engine change made. 44 commands, `SCR_SKIP_WAITKEY=1`.
- **Door to Utopia, The** — no engine score (the `score` command is
  overridden with a joke refusal); progress is an author variable
  `success` 0-6 gating which of two doors — Heaven or Hell — the closing
  scene sends the player through. All 6 success points earned for the true
  Heaven ending; the game ships its own complete walkthrough as an in-game
  cheat (`i'm a cheat`), used only to cross-check the independently derived
  route against the scoring map. 59 commands, `SCR_SKIP_WAITKEY=1`.
- **Patient7** — reaches the game's only win ending — no scoring system. A
  three-day branching vignette (asylum patient vs. demonic-possession
  framing); the sole win requires betraying both secrets to the doctor on
  Day 2, then accepting the demonic pact (`demon`) on Day 3. A fragile spot
  in the game's own task table (Day-1 and Day-2 doctor-menu tasks share the
  same literal digit commands and room, distinguished only by a variable
  restriction) can misfire if a digit is typed before the correct on-screen
  prompt appears — not a walkthrough-breaking issue for a normally-paced
  player. 59 commands, `SCR_SKIP_WAITKEY=1`.
- **The_Final_Question** (David Whyld) — reaches the game's only ending —
  no scoring system. Two rooms plus a death room; the core puzzle is a
  timed window (stall with `z` until a spoken countdown clears, then step
  through the gateway) followed by reading four books to unlock the closing
  narration. 17 commands, `SCR_SKIP_WAITKEY=1`.
- **mustescape** — reaches the game's only win ending — no scoring system.
  A three-stage stealth/combat escape; the two hand-to-hand brawls share a
  single health pool, so the mid-route medical kit should be used before
  the second fight, and the vault door auto-opens once all three switches
  are thrown (no manual "open vault" command). The closing gunfight's
  hit/miss outcomes are fully deterministic, not RNG, making the winning
  sequence a fixed, discoverable solution; an undocumented `cheat`
  instant-win shortcut exists in all three combats but was deliberately
  avoided in favour of a genuinely solved route. 83 commands,
  `SCR_SKIP_WAITKEY=1`.
- **Caida libre** (Spanish, "Free Fall") — reaches the game's only win
  ending — no scoring system. A tiny 7-room linear physical-action puzzle
  (anchor, somersault onto a satellite, grab its antenna, walk two steps
  toward it, strike it to trigger an SOS beacon). The game's own ALR table
  rewrites the engine's built-in "You scored..." end text via a raw
  substring rule (`score`→`puntos`), faithfully mangling it into "You
  puntosd 0 fuera of the maximum 0!" — an author/Runner quirk, not a
  Scarier bug. 8 commands, no env vars.
- **Temple_Of_The_Sun** (AIF, solution/golden gitignored) — reaches the
  game's only win ending — no scoring system. A ritual-disguise puzzle
  (robe + headdress) gates two otherwise-unconditional instant-death exits;
  the win task itself has no typable command, firing automatically once a
  background event sees all four prerequisites satisfied on the same turn.
  31 commands, `SCR_SKIP_WAITKEY=1`.

**90 unwired files remain** after this batch.

**Twelfth batch, 2026-08-29: 8 more wired, suite 351 → 359 rows, all PASS.**
Continuing smallest-first (19,126–23,957 bytes). A vocabulary scan of the 10
smallest unwired candidates surfaced one AIF title and nine clean games:

- **`amy.taf` ("Amy And The Raging Hormones") — AIF treatment.** Confirmed
  adult-only content: the game opens with an explicit content warning and a
  "must be of legal age" gate, and the NPC is established throughout as "the
  girl from work" — an adult coworker, not a minor. The flagged "teen" hits
  refer only to a background concert crowd; the flagged "rape" hit is
  actually the game's own consent-enforcement mechanic (a non-consensual
  attempt instantly kills the player character, rather than depicting
  assault approvingly). Wired normally (row + comment block committed) but
  its solution/golden are gitignored per the `gamma`/`croft`/
  `Temple_Of_The_Sun` precedent.

Every other flagged hit in the remaining 9 candidates was a confirmed false
positive on context read: "weathercock" as a repeated puzzle-mechanism
variable name; "Rothchild" surname and "child's play" idiom; "eighteen"/
"fifteen"/"thirteen"/"seventeen"/"fourteen minutes" all substring-matching
"teen"; "draped" and "parapets" substring-matching "rape"; "succumbs" and
"circumstances" substring-matching "cum"; "cocks his head" as a gesture;
"stripes"/"became a stripper" as non-sexual backstory mentions; nostalgic
"as a child"/"your childhood" narration (8 hits, all in one game); "shy
teenagers" in a non-sexual matchmaking-comedy context; "children playing" as
a bystander mention; and an in-game profanity blocklist (`fuck` among other
swear words the game itself refuses to process as a verb) recurring in two
different games.

Derived in parallel, one background agent per game, merged and re-blessed
centrally as usual; all 8 self-reports were independently re-verified against
the literal harness invocation before blessing. `Will.taf`'s bless call
(`--bless will`) incidentally matched and re-blessed the pre-existing
`microbe_willie_solution.txt` row too (substring filter match) — confirmed a
byte-identical no-op diff, not a regression. No wrapped-marker or executable-
bit footguns recurred this batch.

Re-running the unwired-file count via `comm -23` after this batch gives **82
unwired files remain** (90 − 8, exactly as expected).

- **Wolves_at_the_Door** — no formal score (the `score` command is a fixed
  flavor line; a debug easter egg lists 28 "points" for five flavor actions,
  never wired to the engine) and no `ACT type=6` EndGame at all — the sole
  ending is a scripted event/task pair keyed on a turn counter. A real-time
  survival puzzle: every parsed command advances the counter, and landing
  exactly on turn 25 triggers a deliberate black-comedy "rescue" that always
  ends in death regardless of prior actions — there is no surviving branch.
  The game's own "- walk" easter-egg transcript omits a required `x small
  area` step and would fail if followed verbatim. 26 commands,
  `SCR_SKIP_WAITKEY=1`.
- **apokalupsis** (2009 Intro Comp taster) — win-only, no scoring system. A
  3-room linear detective scene gated by a hidden `evidence` counter (≥5 of
  7 examine-clue tasks). Uses ALR-table string substitution for variable-
  driven dialogue branching (`[regjim=N]` placeholders rewritten post-hoc by
  value) — looked like dead content from the static task dump alone but all
  branches are genuinely reachable in sequence. 46 commands,
  `SCR_SKIP_WAITKEY=1`.
- **dusk** ("A Walk At Dusk", Eric Mayer, 2005) — a puzzleless atmospheric
  walk, author-tracked score out of 10 (no engine `ACT type=4` at all) for
  10 optional observation vignettes; full 10/10 reached. The `stuff`
  command is a built-in walkthrough. The win (tree frog) needs `x sapling`
  TWICE after `x evergreen` — the first hit is a dead "nothing special"
  flavor task. Two "embarrassing" flavor flags (muddy shoes, walked into a
  web) are structurally unavoidable on any full-score route but cost no
  points. 33 commands, no env vars.
- **The_Hunter** — reaches the true structural maximum, 50/50 (sum of all 19
  `ACT type=4` actions, all on one reachable critical path). An amnesiac
  apprentice-mage escape from a ruined castle via two-word rune spells, then
  a village/travel act ending in a real-time ballista race against a
  pursuing warship. Notable authoring bug: a `say * name *` task pattern
  never populates `%text%` for ADRIFT's bare `*` wildcard, so the "obvious"
  phrasing silently fails every restriction; only phrasing that misses that
  literal pattern actually scores. 62 commands, no env vars.
- **Will** ("Selma's Will") — reaches the true structural maximum, 200/200. A
  heirloom-trading puzzle across ~12 relatives at a will reading; no
  double-counting exploits. Notable trap: giving the marbles to one NPC
  silently turns the previously-safe "go down" into an instant-death ending
  for the rest of the game — "slide down the banister" must be used
  instead. 124 commands, no env vars.
- **COBL** — the real win (`ACT type=6`) is permanently unreachable: it
  needs 4 NPC recruitments, but the 4th is blocked by a genuine `.taf` data
  bug (the newspapers are placed onto a STATIC object with an empty room
  list, so it can never become seen/reachable by any command — confirmed
  via `obj_directly_in_room()` and `SCR_TRACE_MATCH`, not a Scarier
  divergence). True max is 160/230 (69%), the best achievable outcome.
  107 commands, `SCR_SKIP_WAITKEY=1`.
- **puzzlebox** ("The Puzzle Box", Richard Otter, ORGComp 2007) — win-only,
  no scoring system. A sequential 10-stage combination-lock puzzle box, all
  target values fixed `.taf` constants (no RNG), though the variable-naming
  convention for "current state" vs. "target value" is inconsistently
  applied across puzzles. Puzzle 2 (clock) has a genuine off-by-one bug —
  completion needs the church clock's minutes PLUS 5, confirmed by both the
  expression chain and the game's own hint text. 85 commands, no env vars.
- **amy** (AIF, solution/golden gitignored) — reaches the game's only win
  ending — no scoring system. Two-scene structure (public gig, then private
  scene); the natural scene transition is gated by a counter no task ever
  increments (dead path, apparent authoring bug), so the game's own
  in-fiction shortcut code is the only working route. The private scene
  layers undress/arousal/intimacy-threshold gates before the finishing
  task; one escalation task the game's own readme warns against repeating
  arms a punitive kill-player ending on reuse, deliberately not invoked.
  19 commands, `SCR_SKIP_WAITKEY=1`.

**82 unwired files remain** after this batch.

**Thirteenth batch, 2026-08-29: 8 more wired, suite 359 → 367 rows, all PASS.**
Continuing smallest-first (24,494–35,681 bytes). A vocabulary scan of 8
candidates surfaced two AIF titles, one decline, and five clean games. Three
new false-positive patterns were confirmed this batch: "cockles"/"cockroach"
substring-matching "cock", "grapefruit" substring-matching "rape", and
"Documents" (a Windows filesystem path embedded in game asset metadata,
`C:\Documents and Settings\...`) substring-matching "cum".

- **`awakening.taf` ("Sexual Awakening") — DECLINED, not wired.** The intro
  states outright "This is a game in which you have sex and it should not be
  played by kids that are too young," frames an incest premise (player +
  sister, parents away), and the NPC description states verbatim: "Tammy is
  your sister... She is 16 years old and has brown hair..." — an explicit
  statement that the sexualized NPC is a minor. Declined per the
  `enc1`/`windy`/`delight` precedent; permanently excluded, not counted
  toward the unwired total's decrement.
- **`The_Strange_Tale_of_Dr_Wilkins.taf` ("A Victorian Transformation
  Melodrama") — AIF treatment.** All bodies/characters depicted are
  explicitly adult women (no minors — "child bearing hips" describes adult
  anatomy, "children of the night" is an unrelated placeholder-text list
  item). Wired normally, solution/golden gitignored per the
  `gamma`/`croft`/`amy` precedent.
- **`BSG TWENTY TWO Final.taf` — AIF treatment, borderline case resolved.**
  Ships a supported non-consent verb-set ("rape tricia", "rape ass", etc.)
  alongside its consensual content, plus the game's own disclaimer framing
  it as fictional adult content with an up-front warning. The NPC is
  explicitly confirmed adult in-game ("You're not a child..."), satisfying
  the established "no minors" bar — this is the established decline
  criterion in this project, not "no non-consensual content," so standard
  AIF treatment applies. As it happens, the derived walkthrough's sole win
  route is a conversational/fetish-escalation chain that never touches the
  rape-verb family at all — that branch is a complete red herring with
  respect to progress, confirmed structurally (it never sets the
  win-condition variable). Wired normally, solution/golden gitignored.

Every other flagged hit in the remaining 5 candidates was a confirmed false
positive on context read: "fifteen seconds" and "fifteen"/"sixteen"
substring-matching "teen"; "cucumbers" and "circumstances" substring-matching
"cum"; "drapes"/"scraped"/"childhood town" substring-matching "rape"/"child";
"cockles" (a stew idiom) and "cockroach" substring-matching "cock"; "my
child"/"poor child went missing" as non-sexual affectionate address and
missing-person subplot narration; a joke movie title ("Attack Of The Naked
Bimbos") quoted in passing dialogue; comedic non-graphic "I've had sex with
nothing but whores" self-deprecating dialogue; and — recurring for the third
time in this project — the "credits reel" easter-egg pattern quoting other
David Whyld games' own self-labeled blurbs, one titled "(An Adult Interactive
Fiction Game)"/"Adult comedy" for a DIFFERENT, unrelated game.

Derived in parallel, one background agent per game, merged and re-blessed
centrally as usual; all 8 self-reports were independently re-verified against
the literal harness invocation before blessing. No wrapped-marker or
executable-bit footguns recurred this batch.

Re-running the unwired-file count via `comm -23` after this batch gives **74
unwired files remain** (82 − 8, exactly as expected; `awakening.taf` stays
counted forever, per the `delight.taf` precedent).

- **YNKaboom** ("The Ascot") — pure yes/no CYOA, no formal score (0
  ChangeScore actions; 5 EndGame endings differentiated only by an
  in-fiction dollar figure). Off-topic (non-yes/no) input increments a
  persistent counter with escalating warnings; a 4th off-topic input at
  count 3 is an immediate death. The climax loop's final offer (an
  interpreter) only wins if that counter is already at 3 — i.e. the player
  must have deliberately misbehaved 3 times earlier — otherwise the
  identical choice kills the player instead: a genuine silent-loss trap with
  no in-turn warning. Solo route (skipping an optional NPC subplot) reaches
  the richest ending, $96,300,000. 25 commands, no env vars.
- **hub** — black-comedy domestic-chores sim with a hidden murder-mystery
  twist. A hungover "house husband" tidies the house before "Marta" gets
  home; completing every chore resurfaces his memory — he murdered his wife
  and hid her in the garden shed, then drives off. Three distinct trap
  endings exist (rewinding a video tape conjures a ghost and ends in death;
  overcooking soup triggers a fire-brigade arrest; calling a cleaning
  company then napping lets hired maids find the body first). The game's
  declared Maximum-Score field is 0, so its own `score` command always
  prints ".../0 (0%)" even though the internal counter genuinely reaches
  80/80 (all 26 ChangeScore actions banked) — an authoring quirk, not a
  scoring bug. Ships an in-fiction `walkthrough` command with ~6 real bugs
  that had to be corrected before it would replay clean. 112 commands, no
  env vars.
- **darkness** — single-location (lighthouse) exploration/repair game.
  Score from four sources: 7 of 8 "mystery notes" (the 8th, the keeper's
  hat, is a genuine 0-point decoy), repairing the generator, pulling the
  light-control lever, and firing a flare gun at a passing ship, plus a +10
  completion bonus for ≥7 notes found. The flare gun is gated on an internal
  "ship passing" state that only holds briefly after the lever is pulled;
  firing early just forfeits 5 points harmlessly, no dead end. 50/50, fully
  reachable. 111 commands, `SCR_SKIP_WAITKEY=1`.
- **Dream Quest** — linear fantasy fetch-quest (68 rooms, 58 tasks, 20 scored
  objectives at 5 points each) built from item-for-item exchange chains,
  culminating in a castle-crypt vampire hunt delivered back to a wizard for
  the win. Leans hard on the object-*seen* model: a sparrow in a nest and a
  nail in a pile of ashes are both present but unreferenceable until the
  container/scenery is explicitly examined first. A one-way bridge collapse
  locks the player into the castle after crossing; an unprotected Freezing
  Passage crossing silently kills unless prepared beforehand. 100/100, fully
  reachable, no dead points. 187 commands, no env vars.
- **The_Strange_Tale_of_Dr_Wilkins** (AIF, solution/golden gitignored) — no
  formal EndGame/win-ending exists anywhere in the task table; the declared
  MaxScore=95 is purely nominal, not an enforced cap — diligent play banks
  117/95 (123%). Authoring bugs: several NPC-specific "repeat" tasks corrupt
  an internal identity-state string, making some transformations unsafe to
  repeat; one specific transformation is uniquely safe for indefinite reuse
  and is the walkthrough's workaround. A genuine permanent softlock exists
  in one late-game room (a movement-blocking check and the room's real exit
  gate impose mutually exclusive preconditions) — deliberately never
  entered, forfeiting 2 low-value points rather than risking the trap. 178
  commands, no env vars.
- **jailbreakbob** — WINNABLE (verdict corrected 2026-08-30; the earlier
  "UNWINNABLE as authored" entry was a .taf-reading error). Task 35's
  NPC-move action uses a 1-based room index: going west from the dining hall
  with the yard pass drops Hoggins into YOUR CELL, not the dining hall, and
  the two cell-scoped events then produce his "You seen me comb, Bob?"
  request within a few turns. Comb → coin (second `talk hoggins`) → meeting
  room phone, option 4 (prank-call Terry's wife) → she disarms him on `ne`
  → `get gun`, `n` = task 72 win ("woo-hoo!"). Proved in the real run400.exe
  under Wine (Adrift_2_jailbreakbob_win.txt, 38/38 commands echoed) as well
  as Scarier. No formal score. 31 commands, `SCR_SKIP_WAITKEY=1`.
- **In_the_Claws_of_Clueless_Bob** — comedy frame story: the player is
  forced to "play" a series of deliberately terrible mini-games authored by
  in-fiction hack "Clueless Bob Newbie," escaping via absurd puzzles. Score
  is entirely author-simulated via a plain SetVar, not the engine's native
  score system — the real `score` command reports 0/0 (harmless quirk,
  never surfaced to the player). Ships its own built-in walkthrough menu as
  static text, confirmed to match actual play 1:1. 12/12 of the author's own
  tracked max, no hints used. 40 commands, `SCR_SKIP_WAITKEY=1`.
- **BSG TWENTY TWO Final** (AIF, solution/golden gitignored) — no formal
  score (its own 0/0 summary is text-replacement-suppressed); win/lose only
  on a ~23-turn countdown. The win route is a conversational/fetish-
  escalation chain gated behind 8 dialogue topics; the game's separate
  violence/torture/rape verb family never touches the win-condition
  variable — the derived route uses ZERO forced/non-consensual verbs. A
  duplicate-task/lower-index-wins quirk means one command must be issued
  twice (the first call is a no-op flavor task). 14 commands,
  `SCR_SKIP_WAITKEY=1`.

**74 unwired files remain** after this batch.

**Fourteenth batch, 2026-08-29 → 2026-08-30: 8 more wired, suite 367 → 375
rows, all PASS.** Continuing smallest-first (36,350–45,755 bytes). A
vocabulary scan of the 8 smallest-first candidates surfaced one decline, one
wrong-corpus duplicate, one AIF title, and six clean games. A new
false-positive pattern was confirmed this batch: "sexton" (a church-caretaker
role) substring-matching "sex". A second batch discovery clarified an
existing pattern rather than adding a new one: a game's own protective
refusal text guarding a child NPC from a player-attempted `strip`/flirt
command is itself a clean, protective pattern — not a violation — provided
(unlike `aparty.taf` below) the game's own narration never separately depicts
that NPC in a sexual scene outside the player's control.

- **`aparty.taf` — DECLINED, not wired.** The game's own walkthrough/design
  text instructs the player to "Flirt with the underage Beth"; Beth is
  repeatedly confirmed "teenage"/"Tony's daughter." Critically, this extends
  the project's decline criterion beyond player-initiated acts: even though
  the player's own attempts to flirt with Beth are explicitly refused ("Beth
  says 'Pervert.'"), the game's own narration separately depicts Beth having
  sex with her boyfriend Dan in a fade-to-black scene ("Beth and Dan are
  doing it in the bed... well, you get the picture"). A sexualized minor
  depicted in the game's own narration is declinable independent of whether
  the player can participate. Permanently excluded, not counted toward the
  unwired total's decrement.
- **`BeThere.taf` — excluded, not a content decision.** Not actually an
  ADRIFT 4 game: its IFiction header declares `<compiler>ADRIFT
  5</compiler>`, it opens with a plaintext `<ifindex>` XML block (the v5
  signature, not the XOR/zlib v3.x/4.0 format), and the v4 headless binary
  rejects it outright ("Not a loadable Adrift game"). MD5-identical to
  `test/adrift5/games/BeThere.taf`, which already has a complete ADRIFT 5
  golden pair (`goldens/BeThere_walkthrough.txt` / `_expected.txt`, 130/max,
  "*** You have won ***"). A duplicate file mistakenly present in the v4
  games/ tree; deleted from `test/adrift4/games/` (untracked/gitignored, so
  this touches no git history) rather than counted as a permanent decline
  like `aparty.taf`, `delight.taf`, and `awakening.taf` — it was never an
  ADRIFT 4 game to begin with. Replaced in this batch's 8-game count by
  `Aegis.taf` (the next-smallest unwired candidate).
- **`warlock.taf` — AIF treatment.** An 1841-dated in-fiction necromancer's
  diary names the sole sexualized character only as a "wench"/"young
  female" village adult; no minor-indicator terms (child/teen/minor/
  underage/rape/molest) appear anywhere in the file, and a targeted
  age-indicator sweep ("young", "years old", "college", "school", "student")
  confirmed every hit describes that same adult woman or an unrelated
  document-aging detail. Matches the `gamma`/`croft`/`Wilkins`/`BSG22`
  precedent. Wired normally, solution/golden gitignored.

Every other flagged hit across the remaining 6 candidates was a confirmed
false positive on context read: "minor wounds"/"minor stone slabs" idiom;
"sexton" (`The Old Church.taf`, a legitimate church-caretaker noun)
substring-matching "sex"; "Documents" (a Windows filesystem path embedded in
game asset metadata) substring-matching "cum"; "draped"/"scraped" substring-
matching "rape"; "eighteen"/"fifteen"/"fourteen"/"sixteen"/"nineteen" as
duration/backstory substrings matching "teen"; "cocktail parties"/
"cockroach" substring-matching "cock"; "stripped" (paint) and a mentioned-
but-never-depicted downloaded "strip poker" computer game substring-matching
"strip"; "molest" as one generic synonym in an ATTACK-verb list; a joke
insult about a door ("breast fed from falsies") substring-matching "breast";
"scum" substring-matching "cum"; in-game profanity-detection easter eggs
("You discovered a swear word") accounting for most raw `fuck`/`cocksucker`
hit counts; and, in `competition2011...suzy`'s case specifically, the game's
own **protective refusal mechanic** guarding its child NPC — attempting
`strip *kid*` returns "The kid is distressed enough as it is without you
trying to strip him/her off..." and nearby swearing triggers an in-fiction
scolding ("There are children present") — confirmed clean per the new
clarification above.

Derived in parallel, one background agent per game (`Aegis.taf` dispatched
separately once `BeThere.taf`'s wrong-corpus status was discovered mid-
batch); all 8 self-reports were independently re-verified against the
literal harness invocation before blessing, including one win-marker
adjustment (`Aegis.taf`'s reported marker had unreliable trailing
whitespace; re-picked a clean unique substring, " END", from the same
closing screen). No wrapped-marker or executable-bit footguns recurred this
batch.

Re-running the unwired-file count via `comm -23` after this batch gives **65
unwired files remain** (74 − 8 wired − 1 deleted `BeThere.taf`, exactly as
expected; `aparty.taf` joins `delight.taf`/`awakening.taf` as a permanently
counted, never-decrementing exclusion).

- **Back Home** — short linear horror/mystery vignette, no score system, one
  unavoidable ending (a coal-bunker key hunt via magnet+string fishing from
  a garden drain) revealing the protagonist accidentally caused an infant
  sibling's death. The author remaps the engine's default lose-message ALR
  to "GAME OVER" for this ending, repurposing the "loss" action type as the
  story's sole intended conclusion rather than an authoring bug. 54
  commands, `SCR_SKIP_WAITKEY=1`.
- **zelda** — UNWINNABLE as authored. "get/take wooden key" (Tree Room)
  prints "Taken." but has no ACT statement that actually grants the key
  object — every phrasing tried, confirmed via post-attempt inventory
  check. That key gates the only door into the Wizrobe Room, whose Dodongo-
  fight/raft chain is the sole route to the eastern continent (rooms
  35–61), holding the only EndGame(win) task and 138 of 197 scoring points.
  Verified maximum reachable score: 59/197 (29%). A second, moot authoring
  bug: entering the Graveyard with the shield equipped destroys it via a
  Like-Like task with no compensating relocate action. 79 commands,
  `SCR_SKIP_WAITKEY=1`.
- **Showtime_at_the_Gallows** — babysitting horror-comedy. No score system;
  every outcome (death or true ending) is a plain room-move into "The End",
  not an EndGame action. Several puzzles are deterministic NPC-patrol
  timing games (escape Zero on turn 9-10 of an 11-turn window; slip past
  twin hell-hounds by waiting exactly one turn on their period-4/period-5
  cycle). The climax is a false-choice trap: answering Zero's yes/no
  survival question kills the player either way — survival requires instead
  repeatedly asking Zero his own name before grabbing a brick. Katie's death
  outside the house is scripted/unavoidable regardless of phrasing (two
  redundant catch-all tasks). 165 commands, `SCR_SKIP_WAITKEY=1`.
- **The Old Church** — 10-room ghost-story puzzle, no score system, two
  EndGame endings. Giving the sword straight to the sexton (task 3) is a
  genuine silent-early-ending trap that forecloses a full NPC subplot;
  examining the organ in the Gallery is an unstated prerequisite for the
  tombstone/ghost-summoning chapel scene, and taking the sword also
  silently transfers a piece of cheese (a task-4 side effect) that is the
  unstated key to feeding the church mouse and reaching the full ending.
  Win-only. 17 commands, `SCR_SKIP_WAITKEY=1`.
- **competition2011...suzy ("How Suzy Got Her Powers")** — 2011 ADRIFT comp
  entry, superhero-origin comedy. Custom author-scripted score variable (no
  engine-level ChangeScore/EndGame exists at all); 22/22, a hidden `score`
  debug command and hidden `b walk` command (dumping the author's own
  tested walkthrough) are both present but unused here. Key trap: the vase
  must be filled with water and carried intact through the crawlspace
  (needed later to hydrate a trapped NPC), while the window must be smashed
  with the fire extinguisher instead — using the wrong tool on either
  forecloses a later scoring action. A parking-lot dawdle timer (≥10 turns
  before entering) triggers a soft "Part One - bad" ending if ignored. 31
  commands, `SCR_SKIP_WAITKEY=1`.
- **Rock Band** — comedy about a slacker roommate's Rock Band-obsessed
  housemate. No score system (the minigame's internal note-score variable
  is not the engine's real score); win-only, 3 EndGame endings (win/lose
  via a 50-turn countdown/death via moldy chow mein). Win route: play the
  in-fiction Rock Band minigame to a perfect 1000-point run (triggering
  "Gigantor"), retrieve a mechanical finger hidden in a closed laundry
  closet, `use finger on xbox` to eject the disc, then unplug the power
  cord. 24 commands, `SCR_SKIP_WAITKEY=1`.
- **Aegis** — fantasy pirate/naval political-intrigue adventure (Aegis
  Knight Celise vs. a treasonous Elder). No score system, no formal EndGame
  action — the sole ending is a plain player-move to the "End" room. Two
  authoring quirks worked around, not blocking: `attack the man/guard with
  the sword` fails on the literal words "with the sword" (bracket-match
  breaks, falls through to a generic library-verb refusal) — omit them;
  "tie the hook to the rope" has no matching task at all (dead flavor text)
  since "throw the rope at the ship" already succeeds unconditionally. 74
  commands, `SCR_SKIP_WAITKEY=1`.
- **warlock** (AIF, solution/golden gitignored) — no formal scoring system
  (the game's own ALR table overrides the default score summary with "There
  is no scoring system in this game."); the only reachable ending from
  beyond the summoning scene is a single mandatory dark-twist EndGame (no
  exit from that room otherwise). A lethal wrong-reagent fireball trap and a
  flavor-only scrying-vision incantation exist as red herrings among three
  total incantations. One flavor-only NPC-directed task is permanently
  unreachable: the game's own SYNONYM table unconditionally rewrites
  "lick"→"kiss" pre-parse, but no "kiss" phrasing was ever authored for that
  specific task — a self-inflicted authoring dead task, not an interpreter
  divergence; it contributes no unique content. 58 commands, no env vars.

**65 unwired files remain** after this batch.

## Camp Windy Lake : Part 2 (2026-08-12) — the AIF treatment, done once

Wired out of the smallest-first order because the user asked for it by name.
**WON 150/150** in 146 commands; suite 231 → **232 rows, 232 PASS**. Christopher
Cole, 22 Feb 2003, ADRIFT 3.90 — the same author as *Diary of a Stripper*. 23
rooms, 22 named objects, 400 tasks, 8 NPCs, 17 variables, one event.

**What the AIF treatment actually is**, now that it has been applied twice:
`goldens/windy2_solution.txt` and `goldens/windy2_solution.expected.txt` are in
`.gitignore`; the commented row in `harness/run_v4_walkthroughs.sh` is the only
committed artefact, and it carries the mechanics that would otherwise live in a
`notes/<Game>_walkthrough.md`. No notes file was written — the solution file's
own header holds the derivation, and it is local-only. Where the files don't
exist the row NOSCRIPTs, which is not a failure. The source walkthrough is
`downloaded/CampWindyLake2_walkthrough.txt` (also local-only).

**The maximum is provable, not assumed.** 54 `ACT type=4` score actions and no
other award: 3×1 + 26×2 + 17×3 + 1×4 + 6×5 + 1×10 = 150, the game's own declared
maximum, and the route fires all 54. Two `ACT type=6` EndGames — T196 `attack
tim` (v1=2, the death) and T198 `attack tim * machete` (v1=0, the win).

**Three repairs turned the published route into a script**, and the middle one
is the interesting one:

- A **name prompt** before the first move, so line 1 is the name. Same class of
  trap as *Far From Home*, without the `<waitkey>`.
- **A fixed-clock event used as a gate.** The walkthrough writes the beach party
  as a heading and then the literal instruction "(hang around until skinny
  dipping begins)". `SCR_DUMP_TASKS` names the clock: EVENT 0 [skinny dip],
  `starter=3 startTask=117 time1=time2=5` — five turns, no randomness. The event
  is what sends Laura back to her office, so `in` at the main cabin is refused
  until it fires, and the whole Laura scene (24 points) hangs off spending
  exactly those five turns. The route pays them with the two `ask laura
  about …` lines the walkthrough does give, plus three `z`.
- **Two tasks behind one door.** The shed's unlock (`unlock * shed` /
  `unlock * lock` / `use * key`, the +5) and its enter (`open * shed *` /
  `enter` / `in` / `open * door *`) are separate tasks, and "It's locked!" is
  the *enter* task's fail text. The walkthrough's single `open door` can only
  ever reach the second one, so the solution spells both out. Not an engine
  divergence — the de-obfuscated task table has no `open`-shaped command on the
  unlock, so the real Runner cannot score it that way either.

Nothing else in the 146 commands draws a parser refusal: no "What?", no "I don't
understand", no "isn't here" anywhere in the transcript. Worth remembering when
the other seven AIF files are wired — a published AIF walkthrough is a *route*
written for a human, and the gap between it and a script is usually a prompt, a
timer, or a verb the author paraphrased.

## Salutations (2026-08-12) — the smallest file in the corpus

Back to smallest-first: 5,591 bytes, ADRIFT 4.00, Lumin, Ectocomp 2008. **WON**
in ten commands; suite 232 → **233 rows, 233 PASS**. One room, 17 tasks, 2
events, 9 objects, and **no score at all** (not one `ACT type=4`), so the win
marker is WINTEXT prose. Full write-up in `notes/Salutations_walkthrough.md`.

Small game, three findings worth carrying forward:

- **A `<waitkey>` can shift a whole script without ever failing.** The intro
  ends in one, so without `SCR_SKIP_WAITKEY=1` the *first* command is eaten as
  the keypress — and this game still wins that way, because the command it
  eats (`remove jacket`) turns out to be skippable. A blessed golden would
  then have recorded a silently shifted run. **Set `SCR_SKIP_WAITKEY=1` on any
  row whose game contains a `<waitkey>`, not just the ones that visibly
  stall**; `plaintext()` will find the tag in the de-obfuscated file. This is
  now checkable across the whole suite — see the audit below.
- **`WaitTurns` is per game, and `z` is not one turn.** This game sets it to 3,
  so one `wait` spends half of EVENT 1's six-turn deadline. Measured from the
  kill: five ordinary commands live, six die; one `z` lives, two `z` die.
  Pacing a timed event by counting `z` lines is wrong unless the global has
  been read — compare the second-wave note that a rejected command does *not*
  advance the counter.
- **A restriction with no failure message falls through to the library.** T2
  `get stick` and T4 `get knife*` are both gated, neither gate carries text,
  and the library take then reaches into the backpack lying on the ground — a
  six-command win exists that skips the entire designed chain. T3 `get pack`
  and T9 `get lighter` do have failure text and refuse properly, so the same
  game shows both halves of the model side by side (`adrift4-vs-5-restriction-eval`).
  The committed route takes the intended path so the regression covers the
  object chain and both events rather than the holes.

### The waitkey audit it prompted — `harness/waitkey_audit.py`

Salutations made the shift visible, so the whole suite was checked for it. For
every row the script asks three questions: does the row set
`SCR_SKIP_WAITKEY=1`, does the de-obfuscated `.taf` contain a `<waitkey>` at
all, and does adding the variable change the number of `>` prompts the run
gets through. 2026-08-12, 233 rows:

| Bucket | Rows | Meaning |
|---|---|---|
| IMMUNE | 50 | row already sets `SCR_SKIP_WAITKEY=1` |
| NO-WAITKEY | 109 | no `<waitkey>` in the file |
| ABSORBED | 19 | tag present but never reached on this route |
| FILLED | 37 | lines are swallowed, and the solution has at least that many blank lines to spare — the filler convention working as intended |
| **SUSPECT** | **18** | more lines swallowed than blanks available |

**The filler convention is the majority answer and it is fine.** A route
written against a waitkey-heavy game feeds each pause a throwaway line —
`wes_ghn` carries 77 blank lines, `shardsofmemory` 45, `chooseyourown` 19 —
and those rows only look dramatic in the audit (`+33`, `+43`, `+17`) because
skipping the waitkeys turns every filler into a command.

**The 18 SUSPECT rows are a flag, not a verdict.** `icecream` is the proven
loss: `take cone` never runs, and the committed golden shows three commands
where the solution has four (it survives because the player starts holding a
cone). `farfromhome` is the opposite case, a false positive — its filler is
the leading `x`, not a blank. The rest are unexamined:

```
icecream  the_cat_in_the_tree  man_overboard  yak_shaving  confession
togetyou  zombies  topaz  circus  les_feux  thetest  thetest_win
to_hell_and_beyond_assisted  to_hell_and_beyond_assisted_max
lost  lost_down  dancing_even_him  farfromhome
```

**Why this was not fixed in the same pass.** Adding `SCR_SKIP_WAITKEY=1` to a
row is not a one-line change: it rewrites the golden, it makes every existing
filler line execute as a command, and several of these routes were derived
*under* the shift or transcribed verbatim from a published walkthrough whose
score was checked against the shifted run (`thetest_win`, the two
`to_hell_and_beyond_assisted` rows). Each one needs its route re-checked and
its win marker re-proved, which is a wave of work, not a sweep. Re-run
`python3 harness/waitkey_audit.py` after any of them is done.

### Worked through, 2026-08-16 — SUSPECT 0

The wave got done, and it turned up a second, larger case underneath it.

**The 18.** One repair rule, applied uniformly: put `SCR_SKIP_WAITKEY=1` on
the row so a solution line is a command again, then delete the lines that were
only ever filler and keep the lines that are route steps. Five rows came out
**byte-identical** — `the_cat_in_the_tree` (a leading `z`), `thetest` and
`thetest_win` (three leading `z` each), and both `to_hell_and_beyond_assisted`
rows (a `look` at four points) — which is the proof that those deletions hit
exactly the filler, and it retires the worry above about the routes that were
transcribed under the shift. The other 13 were re-blessed. **No score moved
anywhere in the suite**, and every win marker was re-proved before blessing.

Two rows were more than bookkeeping:

- **`man_overboard`** was losing seven real commands to the cabin's pause —
  `i`, `x hat`, `wear hat`, `x bed`, `x cupboard`, `open it`,
  `take all from cupboard`. The committed golden recorded the damage in plain
  sight: `x beard` → "You see no such thing.", `wear beard` → "Wear what?",
  `x log` and `read log` likewise, because the cupboard they come out of was
  never opened. All four answer properly now.
- **`circus`** (*Menagerie!*) was losing its first two commands, `Easy` and
  `open case`. Restoring them shifts every NPC's wander by two turns, and
  Pringles is then absent when the single `give peanut to pringles` fires —
  no knife, no knife sale, and the score falls 64 → 58. The route's own
  spam-until-present idiom is the fix: **ten** tries is the measured minimum
  under `SCR_SEED=12` (nine still misses him), the committed route uses twelve
  for margin, and 64/140 is back.

**The bug underneath: the pause did not honour `#`.** `os_read_line` has
skipped comment lines unconditionally for a long time — a `#` is never a valid
ADRIFT command, and the commented solution files are the documented corpus.
The `<waitkey>` read did not, so a pause could eat a comment. That is worse
than it sounds in both directions: it hid the swallow (the route still ran in
full, because the header was free filler, so the audit saw nothing wrong), and
it made a solution's behaviour depend on how many lines of prose it happened to
carry. `SCR_MARK_WAITKEY=1` — which now also names the line each pause ate —
put the count at **25 rows** with a pause eating a comment; `farfromhome` was
the one where a real command was lost as well, and it was already in the 18.

The pause read now skips comments the same way (`os_ansi.cpp`), and the other
**24 rows** were converted with the same rule as the 18. Eleven came out
byte-identical. Of the thirteen that did not:

- **`cbn` / `cbn2`** are the counter-example to "delete every blank". Their
  five (resp. three) *leading* blanks are real empty-command turns — one of
  `cbn`'s is what TASK 38 turns into the move out of room 0 — and deleting
  them loses the game. Only the blanks feeding the `x desk` mid-message pauses
  come out: two in `cbn`, one in `cbn2`. Both then pass byte-identical.
- **`TheADRIFTProject`** had been blessing the broken run. Its filler blanks
  were answering the *name* prompt, so the player was "Anonymous" and the
  gender prompt was asked twice and refused twice; the solution's own `Drifter`
  and `male` were arriving a prompt late. The route as written finally runs.
- **`griswold`** gets `x cassette` back, which shifts the doorbell event a turn.
- The remaining nine differ only by bare `>` prompts disappearing — empty turns
  the old golden recorded, which produced no game output at all.

2026-08-16, 242 rows:

| Bucket | Rows | Δ |
|---|---|---|
| IMMUNE | 97 | +47 (the 18, the 24, and rows wired since) |
| NO-WAITKEY | 113 | +4 (corpus growth) |
| ABSORBED | 8 | −11 |
| FILLED | 24 | −13 |
| **SUSPECT** | **0** | **−18** |

The 24 rows still in FILLED are the ones the convention genuinely suits and
they are left alone: their fillers are blank lines, never comments, so the
`#` fix does not touch them and the audit confirms they still balance. Suite:
**242 PASS, exit 0.**

*The Woods Are Dark* went in later the same day and moved IMMUNE to 98, which
is the rule above applied to a new row rather than a repair: its one
`<waitkey>` sits in the title text, so it takes `SCR_SKIP_WAITKEY=1` and its
header runs straight into the first command with no blank line for a pause to
eat. Suite **243 PASS, exit 0**. *Captive Universe* followed and landed in
NO-WAITKEY (114): `plaintext()` finds no `<waitkey>` in the file at all, so its
row carries no env. Suite **244 PASS, exit 0**.

## A Day at the Iachini House (2026-08-12) — a declared maximum that is exactly reachable

19,083 bytes, ADRIFT 4.00, Michael Iachini, August 2001. **WON 115 out of a
maximum of 115** in 170 commands; suite 233 → **234 rows, 234 PASS**. 27 rooms,
68 tasks, 5 events, 4 variables, **no NPCs**. No `<waitkey>` anywhere in the
file, so the row needs no env. Full write-up in
`notes/A_Day_at_the_Iachini_House_walkthrough.md`.

A chore game: `read list` enumerates six jobs (the broken stair, the hot tub's
pH, wash the afghan, lay a fire, take a shower, find the remote), and the
single ending — T30 `turn on * television *` — restricts on four of them plus
`RESTR type=3 v1=0 v2=5 v3=5`, the player **sitting on the couch**. `sit on
couch` is a required command, not flavour.

Three things worth carrying forward:

- **21 `ACT type=4` awards total 140, but only 115 can ever be scored, and the
  author's declared maximum already says so.** T51/T52/T53 are three copies of
  `take * shower *` at 10 each, and every one of them moves the single fluffy
  bath towel *nowhere* (`ACT type=0 … v2=0 v3=0`) and replaces it with a wet
  one; nothing dries or restores it, so exactly one shower is possible (−20).
  T45 (a base tablet at pH 6, +5) is unreachable (−5). 140 − 25 = 115. When a
  game's maximum looks smaller than the sum of its awards, check for duplicate
  tasks over a consumable before assuming a scoring bug.
- **The first-match rule can lock a variable band out permanently.** The hot
  tub bands `ph` across T44/T45/T46 (`add * bas*`) and T47/T48/T49
  (`add * acid*`). Starting at 10, T47 walks you to 8 and T48 scores landing on
  7. After that `add acid` still matches **T47 first**, whose restriction now
  fails *with a failure message*, so the v4 scan stops and T49 is never
  reached — and T44 does the same in front of T45. You cannot overshoot on
  purpose to collect the other award. This is *Salutations*' lesson from the
  other side: messageless gates fall through, gates with text end the scan
  (`adrift4-vs-5-restriction-eval`).
- **`put sheets in dryer` is absorbed by the library and looks like a puzzle.**
  T18's command is `put * sheet * dryer` and ADRIFT wildcards match **whole
  words**, so the plural never matches; the library answers "You put the box of
  dryer sheets inside the clothes dryer" and T19 then refuses, because its
  third restriction is on *task* 18 having run. The singular works. Same shape
  as the T53 room typo above — when a task refuses while its precondition
  visibly happened, diff the typed words against the command pattern before
  suspecting the engine.

Also: T53 is a second copy of T51 carrying the same `room=16` (the upstairs
bathroom) while hanging its wet towel on the *basement* bathroom's bar, so the
author meant `room=24` and mistyped it. And the game enforces a carry limit
whose messages read like puzzle refusals ("Your hands are full at the moment",
"The armload of firewood is too heavy for you to carry at the moment") — an
early route silently missed three objects to it.

## La hija del relojero (2026-08-14) — three of eight tasks dead, and a new §4 row

Next in the smallest-first order. **WON in 11 commands**; suite 234 →
**235 rows, 235 PASS**. "Nano", 31 March 2008, ADRIFT 4.00, **Spanish** — the
second Spanish game in the v4 corpus after *Renuntio* and the third one-room
game after *Salutations* and *I*. One room, 8 tasks, 12 objects, no NPCs, 2
events. A clockmaker sits at his dying daughter's bedside while five roses grow
out of her back; the win is to wind up the brass Phoenix he built for her and
let it sing her to sleep. Full write-up in
`notes/La_hija_del_relojero_walkthrough.md`.

**No score at all** — not one `ACT type=4` in the file, and `score` answers
*"Your puntos is 0 fuera of a maximum of 0"*. No losing ending either: T5's
`ACT type=6 v1=0` is the file's only type-6. The chain is three gated steps
(`abrir cajon` → `coger fenix` → `tirar cuerda`, which snaps the winding cord
into your hand → `arreglar fenix`), and the game hints every one of them in its
own object descriptions.

**Three of the eight tasks are dead, each for its own reason, and all three in
the file rather than in us.**

- **T6 `*vaso*` and T7 `*Tamborilero*` are `Where` Type 0** — runnable in *no*
  room, the rule settled against run400 for *The Hangover* (§5 of
  `RUNNER_TESTS_TODO.md`). Both were written to answer scenery the room
  description advertises. The glass is lost twice over: `STATIC obj=10 [Vaso]
  rooms=` places it in no room either, so `x vaso` is "You see no such thing"
  from both directions.
- **T4 `Abrir ventana` is killed by the game's own synonym table.** The file
  defines `SYNONYM [abrir] -> [Open]`, and substitution runs **before** task
  matching, so the command reaches the matcher as `Open ventana`, the pattern
  cannot match, and the library answers "Open what?". `SCR_TRACE_MATCH=1` is
  what pins this: it echoes the *post*-substitution input, printing
  `input=[hablar Maria]` for a typed `hablar hija` — the same machinery working
  as intended one task away from the one it breaks.

**The new finding, and it generalises: a localised game's message-replacement
table is a Runner oracle.** This `.taf` carries 49 output-message replacements
(`You open the` → `Extendiendo mi mano abrì el`, `from the` → `de el`,
`score` → `puntos`, and — usefully — `You can't do that here!` → a Spanish
line, so the refusal ported on 2026-08-10 is exercised by a second game). The
author wrote each pair by reading what the real Runner printed, which makes the
left-hand column a transcript of Runner output in miniature.

One pair does not fire against Scarier: **`You take a` → `Con sumo cuidado cogi
el`**. Scarier prints `You take the …`, so the replacement misses and the line
comes out half-translated — *"You take the Fenix de laton de el cajon."*, the
`from the` half fired and the `You take a` half did not.

The Phoenix has an **empty `Prefix`**, and Scarier's two object printers guess
differently about that: `lib_print_object()` defaults an empty prefix to `"a "`,
`lib_print_object_np()` defaults it to `"the "`, and both defaults carried a
comment saying they were empirical — the np one had said "what it's really
supposed to do is a mystery" since the original SCARE. The take-from message
uses the np printer, the container-reveal listing uses the other, which is why
the same object is `A Fenix de laton is inside the cajon` on one line and `the
Fenix de laton` on the next. The author wrote `A` → `Encontrè un` for the first
and `You take a` for the second, which read like a witness that the np default
was wrong.

**It isn't — arbitrated 2026-08-14 and the answer is that Scarier is right.**
This very game was run in `run400.exe` under Wine, and it answers `coger fenix`
with **`You take the Fenix de laton de el cajon.`** and `abrir cajon` with
**`Encontrè un Fenix de laton dentro del cajon.`** — both byte-identical to our
golden's lines 66-67 and 72. So the `"the "` np default and the `"a "` default
are *both* faithful, and the author's `You take a` pair simply never fired in
the real Runner either: he wrote it speculatively. **The caveat in the §4 row
was the right one to have written.** A localised replacement table is a Runner
oracle for the pairs that *do* fire, and no evidence at all for the ones that
do not — the same run shows the table is a blind string replace, since the room
description's `A mi lado hay…` comes out as `Encontrè un mi lado hay…` in
run400 too. The §4 row is closed and the `sclibrar.cpp` TODO retired; the 235
goldens riding on that default were never at risk.

**Method note for the next Spanish or otherwise localised game:** dump the
`.taf` plaintext tail (`zlib.decompress(data[22:])` for a 4.00 file) and read
the replacement table before deriving anything. It tells you the game's verbs,
its direction words, and — free of charge — what the Runner's library messages
look like.

## Veteran Knowledge ×2 (2026-08-14) — the author's hint system *is* the walkthrough

Next in the smallest-first order. **WON 50/50** in 120 commands; suite 235 →
**237 rows, 237 PASS**, because `vetknow2.taf` went in on the same route. Robert
Street (credited "Robert Rafgon" in the first release), 12 February 2005 in 3.90
and 6 May 2005 in 4.00 R46 — 43 rooms, 359 tasks, 83 objects, 15 NPCs, 38
events, 2 variables. The full-length rewrite of *Veteran Experience*
(`veteran.taf`, 12,043 bytes), which has been wired since the first wave: same
author, same washed-up wrestler crowbarring his way back to the World title, but
with a whole town in front of the arena. Full write-up in
`notes/Veteran_Knowledge_walkthrough.md`.

**50/50 is provable, not assumed.** The file has exactly eight `ACT type=4`
awards — 2 + 8 + 10 + 4×3 + 8 + 10 = 50 — and the route fires every one. It also
has exactly **one `ACT type=6`**, T260's win, so the game cannot be lost: the
Star's eight scheduled attacks and the steel chain he produces on turn 36 are
texture. WINTEXT is empty, so the marker is prose out of T260's own text.

**The transferable finding: `SCR_DUMP_TASKS` prints an author's hint menu.**
There is no published walkthrough for this game — David Welbourn covered the
earlier one only — and none was needed, because ADRIFT stores per-task hints and
the dump prints them as `HINTQ=` / `HINT1=` / `HINT2=`, with `HINT2` almost
always the literal command in capitals:

```
TASK 122 where=1 room=27 ... cmd=[give*teddy*monster]
    HINTQ=[The Monster's locker room 2]
    HINT1=[You firstly need to distract the Monster. ...]
    HINT2=[You need to GIVE TEDDY TO MONSTER]
```

The Ring's hint is a sentence-by-sentence walkthrough of the last seven moves.
**Grep the dump for `HINT2=` before deriving anything, on any game whose author
built a hint menu.** Typing `hint` in play is no substitute inside a golden: it
prompts `[Y/N]` *and echoes its own prompt twice*.

**Four timing shapes worth remembering**, each of which cost a replay: an event
gating an object's arrival (`EVENT 1 [Flyer arrives]`, `start=4..4`, so the
three `z`s at the top of the solution are the wait); NPCs that do not exist in
the world until a task elsewhere moves them (the brats are `startRoom=-1` and
only T57 `east`-out-of-the-bar, gated on drugging the beer, puts them in the
park — so the route crosses town twice by necessity); **one command that is two
tasks with inverted gates** (`look under ring` is T126 for the tacks and ladder
*before* the acid and T127 for the crowbar and fire extinguisher *after* it, so
the order is forced and getting it backwards makes the High Flyer unbeatable);
and a **variable with a four-turn expiry** (`spray star` sets VAR 0 `blinded`,
`EVENT 14..20 [unblind star]` clear it four turns later, and T260 wants
`blinded == 1`, so the crowbar has to swing on the very next turn).

**And one dead end that reads like a loss and is not.** Levering the fallen
crate (T86) gets you mugged by the Evil Twins and dumped unconscious in the
Mysterious room. That *is* the way onto the bottom floor — `touch east wall` for
a hairpin, then `west`.

**`vetknow2.taf` is the same game.** A zlib-decompress (offset 22) plus
`strings` diff of the two files finds **three changed strings and nothing
else**: the author byte-field, one added ABOUT sentence recording the rename,
and the build date. Not one room, task, object, NPC or event differs, and the
same 120 commands produce a byte-identical transcript. The second row exists to
keep that true — if those two goldens ever diverge, something is reading the
header when it should not be. Worth doing whenever the corpus holds two files of
one game; it is a free consistency check, unlike the *Town of Azra* pair, whose
two files really are different builds.

## The Lost Tomb (2026-08-14) — `v2=2` is *worn*, and it is worth 20 points

Next in the smallest-first order, and the first **3.90** file since the third
wave. **WON 175/175** in 105 commands; suite 237 → **238 rows, 238 PASS**.
`losttombv2.taf`, 56,336 bytes — 19 rooms, 99 tasks, 86 objects, 1 NPC, 13
events, 6 variables. No author is recorded anywhere: no author byte-field in
the file, and `games.manifest.tsv` line 123 carries only the title. An
Egyptology farce — you have found pharaoh Erick's tomb, and your funder Lord
Rupert Mongoose, monocle and pith helmet and an alarm clock set for tiffin, has
invited himself along. Full write-up in `notes/The_Lost_Tomb_walkthrough.md`.

**175/175 is provable.** Exactly 23 `ACT type=4` awards, summing to exactly the
declared maximum, and the route fires all 23. Nothing is left on the table:
the only unfired tasks with endings on them are deaths and near-misses.

**The transferable finding: object restriction `type=0 v2=2` is WORN BY THE
PLAYER, not held** (`screstrs.cpp`, `restr_object_in_place` case 2/8) — and
this game hangs its single largest award on that distinction in a way that is
invisible in play. `x wall` in the Riddle Room is four competing tasks: T45,
T46 and T47 want the ruby / sapphire / complete death mask **worn** (`v2=2`),
and T44 is the `v2=8` "not worn" counterpart, which prints a perfectly
plausible description of the wall. Walk in *carrying* the finished mask and you
get a sensible answer, no refusal, no failure message — and lose 20 points.
The riddle still answers, the escape still wins, and the run lands on 155/175
with nothing anywhere to say why. `wear mask` is the whole difference.
**Whenever a v4 route comes up short with no visible failure, check the losing
task's `v2` for 2/8 before assuming a routing mistake.**

**The hint-menu trick worked a second time.** No published walkthrough exists
for this game either, and again `SCR_DUMP_TASKS` printed the author's own
`HINTQ=`/`HINT1=`/`HINT2=` fields — one entry per puzzle, including the
oblique one that gives away the worn-mask rule ("*...maybe the wall will become
clear when looked at with the right attitude... ...or eyes...*"). Two games
running: treat the `HINT2=` grep as step one of the per-game workflow, not as a
lucky break. The two exceptions here are instructive — the riddle's hints are
deliberately a taunt ("*...and for this one, you're on your own... Mu ha ha ha
ha ha ha!...*"), and the numeral-floor hint points at a numeric "relationship
between the rows" that nothing in the game text states; the four safe panels
are simply hard-coded as the `ALTCMD` patterns of T86–T89 (X, VIII, XIV, XVII)
and every other panel is an `ACT type=6 v1=2` death.

**Four timing shapes and two losing endings**, each of which cost a replay:

- **A random event start that reads as a wrong answer.** `EVENT 8 [TIFFIN
  TIME!]` is `start=50..80`; when Rupert's alarm goes off it zeroes `VAR 0
  [Rupert]` for three turns, and every `ask rupert to ...` task is gated on it.
  A request swallowed by tiffin does not get refused — it falls through to a
  *flavour* message ("the lid is too heavy for you"), which reads exactly like
  a wrong solution. The route asks for the second sarcophagus twice.
- **A trap on a two-turn clock with a late-opening window.** `EVENT 2 [CLOSING
  WALLS]` steps 12 → 10 → 8 → 6 feet, and `jam spear in walls` (+10) is refused
  until 6 feet — turn 7 after the hand-in-the-hole ask, hence nine `z`s, then
  two more to bend the spear and open the door.
- **An end-of-turn event whose revealed object is not there yet.** `EVENT 6
  [PILLAR CHECK]` runs T30 (+10) at the *end* of the turn the fourth statue
  lands, so `take ruby` on the next line **does nothing and says nothing** — and
  since the mask needs the ruby, the run silently strands at 155/175. A bare
  `z` fixes it. Same failure signature as the worn-mask trap: a route that is
  20 or 10 points short with a clean transcript.
- **Lighting something you are still holding.** T59 (dynamite lit in hand)
  starts `EVENT 9` → T61 "BOOM! YOURE DEAD!" two turns later; T60 (+5) is the
  in-the-wall version. Plus a parser note: the crack is hidden until
  `x walls`, and the object it becomes is aliased **hole**, so
  `put dynamite in crack` is not understood.
- **Two losing endings on the obvious move.** Climbing out of the well while
  holding the death mask is T33, `ACT type=6 v1=1`; reaching into the crocodile
  statue before jamming its jaws with the pencil is T95, likewise. Both have a
  polite alternative task sitting next to them that does the right thing.

## The Long Journey Home (2026-08-14) — three walls, and a task-list off-by-one

`Journ2.taf`, 59,124 bytes, **3.90**, Danny Chabino, released 20 June 2001 —
the next file in the smallest-first order. 27 rooms, 89 tasks, 37 objects,
2 NPCs, 5 events, 6 variables. **UNFINISHABLE, 30 of a declared 90**, in 46
commands; suite 238 → **239 rows, 239 PASS**. The row is anchored on the score
line, the same convention *The Hangover* established. Full write-up in
`notes/The_Long_Journey_Home_walkthrough.md`; the commented route with per-line
task numbers is `goldens/journ2_solution.txt`.

You wake in your own bed, walk through the bathroom mirror and descend a well
into a stone underworld of rooms named Sorrow, Despaire, Anger, Rage, Fear and
Terror. There is a card game with a creature down there, and past it the way
home. Neither is reachable.

**The declared 90 is two careers, not one.** Ten `ACT type=4` awards sum to 90,
but T10/T11, T24/T25 and T74/T75 are **male/female twins** and move one is a
gender prompt. 60 is anyone's arithmetic ceiling — and the female half is
broken twice over: T24 is `where=0`, and T75 (unlike T74) has no `ACT type=0`
to drop the King of Spades, while `EXIT room=20 N -> dest=19 gateTask=74
wantDone=1` gates the Gnarled Woods' only way back on **T74 specifically**. A
woman who lights her torch in Terror scores her ten points and never leaves.

**Rage is a one-way trap because of a task-list off-by-one.** `T22 #12 turn
valve debris here` (room 9, `restr=0`, index 22) and `T25 #12 release pressure`
(+10, index 25) carry the *identical* four patterns —
`* turn/open/release/use * valve *`. Forward first-match in
`run_game_commands_common()` hands every phrasing to the unrestricted T22
forever, debris cleared or not, which contradicts T25's own hint text. And
Rage's only exit is `gateTask=25 wantDone=1`, with T17 additionally
intercepting `n` while `VAR 5 [taskstate]` is unset — so `get on raft` in the
Reservoir (T13) is a soft-lock.

The transferable part is **how that was settled without a Wine session**. The
same room holds two other success/failure sibling pairs and the author ordered
*both* correctly: T18 (`remove debris` holding the mast) sits before T19
(`remove debris` bare-handed, `ACT type=6 v1=2` — **instant death**), and T20
(`mount pipe`, restricted on T23) before T21 (`#12 pipe fitting too hot`,
unrestricted). Under any order but forward-first-match the Reservoir would kill
every player on sight and the author's own hint menu would be lying in two
places. Forward order is the only reading under which the file's hints are
self-consistent — and under forward order the valve pair alone is backwards.
**When a divergence question is really an ordering question, look for the
sibling pairs elsewhere in the same file: an author who relied on the order
three times and got it right twice has told you what the order is.** (Left
unverified against `run390.exe` on purpose: it is 10 points inside a room with
no exit, so it cannot move the verdict.)

**The card game has no starter, and the ending is sealed anyway.** `T76 #6
start card game` has three satisfiable restrictions and **nothing that can
fire it**: Command[0] is the author's internal `#N` label with no ALTCMDs, no
event targets it (raw `affTask`s are 33/85/82/83/1), and the file contains
**zero `ACT type=5`** actions. T77/T78/T79 (+10) are all `RESTR type=2 v1=77`,
so rooms 23–25, the NPC Joy and T85's +10 are unreachable. Then the closer:
the file has exactly two `ACT type=6` actions — T19's death and `T86 #17 the
end`, `ACT type=6 v1=0`, the one win — **and T86 is itself `where=0`**. The
`WINTEXT` (which is where the author credit and release date come from) is dead
data. This is the second file in the corpus with *The Hangover*'s exact shape;
see `adrift4-where-norooms.md`.

**Second transferable finding: literal task labels really are typeable.**
`!goto lair` (T87) and `!random` (T88) are debug tasks the author shipped in
the release, and typing them fires them — ADRIFT literal patterns match
literally. So a real Runner player *could* type `#6 start card game` and start
the card game by hand; only our headless front end refuses, because
`os_ansi.cpp:286` skips script lines beginning with `#`. Worth knowing before
declaring any `#`-labelled task "untypeable": here it changes nothing (the
backdoor is worth 20 points and still stops at T86), but on another file it
might.

Parser notes, one replay each: the Creature eats your **first** move in the
Lair (T5, one-shot, hands over the King of Hearts and does not move you); the
King and Queen of Clubs are the only two of the eight cards with no `card`
alias; the Gnarled Woods is an RNG maze (T68 needs `VAR 2 [direction] == 2`,
re-rolled every turn by `EVENT 4` through T0) that takes three `left`s under
the harness seed; T71 there claims `* *e *`, which matches **any word ending in
"e" followed by another word**, so it is `get king of spades`, not `take`; and
Terror is two near-identically described rooms with the shaft in one and the
stones in the other. `T6 #8 slipnslide` has the ALTCMD `[*]` — *whatever* you
type on the slippery slope slides you into Despaire.

**2026-08-30 — run390 CONFIRMS unwinnable, and the real Runner bricks far
earlier than we do.** Two live `run390.exe` sessions (`Adrift_2_journ2_end.txt`:
the 46-command golden plus a typed `#6 start card game` endgame probe;
`Adrift_3_journ2_t5.txt`: 21 commands then a mixed bag). Every command echoed.
In both, the moment you step into the Lair, `T3 #6 creature looks` fires on the
arrival command (its patterns are `* s *`, `* n *`, `* e *`, `* w *`, `* nw *`
and a bare `*`; `rep=0`, `where=1 room=1`, no restrictions) — and from then on
**every command typed in the Lair answers "You have already done that."**:
`look`, `score`, `e`, `w`, `north`, `take shovel`, `dig`, `fly`, `x card`,
`x king`, `z`, `give cards`, `out`, `leave`, `exit`. Only `i` and `x creature`
still work (library paths the 3.9 Runner takes before task matching). That is
the pre-4.0 rule recorded in `adrift4-spent-task-vs-restrictions.md` — a spent
non-repeatable task whose pattern matches *claims* the command and prints the
"already done" refusal — applied to a spent catch-all. Scarier deliberately
does not import that rule (it costs 15 goldens), which is why our golden walks
out of the Lair and banks 30; under the original Runner the ceiling is the
well's **5/90** and the player never sees Sorrow, Terror or the cards. The
card-game backdoor and the Rage valve question are therefore moot in run390 —
neither room is reachable — and the `where=0` T86 wall was never even
approached. Verdict unchanged, and now Runner-proved: UNFINISHABLE. (Under a
gen400 upconversion the 4.0 restrictions-first rule would let the spent T3 fall
through, so a 4.0 Runner would play like Scarier; not measured.) Driver note:
`drive_ckpt_safe.sh` line 128 skips any cmdfile line beginning with `#`, so a
`#`-labelled task can't be typed through the harness either — it needs a
one-off keystroke, which this session never got to because the Lair brick
came first.

## Murder in Great Falls (2026-08-14) — a `<waitkey>` between the two start-up prompts

`mudergreatfalls.taf`, 59,896 bytes, **3.90**, 28 rooms / 68 tasks / 61 objects
/ 10 NPCs, **no events and no variables at all**. No author is recorded — the
file has no author byte-field and `games.manifest.tsv` line 142 lists only the
title; the last line of the "Wild" trailer dates it 24 Nov 2001. A three-day
police procedural: Chief Branis phones about a body in a car by the college,
you collect evidence, hand it to Jake at the evidence dropoff, and on Day 4
name the killer out of three suspects. **WON 200/200** in 98 moves;
`notes/Murder_in_Great_Falls_walkthrough.md` has the route and the award table.

**200 is provably the ceiling and it is exactly reachable.** Exactly 32
`ACT type=4` awards (22 fives + 10 tens), nothing else scoring in the file, and
no variables for an ALR to be counting; they sum to the declared 200 and the
route fires all 32. The `score` checks read 110 / 175 / 190, with T66's +10 for
the correct accusation making 200.

**The `<waitkey>` finding, which is the row's real value.** 15 tags in the file,
13 on the route, and the first sits **between the game's two start-up prompts**:

```
Please enter your name:
>                            <- line 1, the name
                             <- the <waitkey> eats line 2
Please choose the player's gender (male or female):
>                            <- line 3 is offered as the gender
Please answer "male" or "female".
```

So an unset row does not shift by one, it never starts: every subsequent line
is offered as a gender and rejected forever. *Far From Home*'s trap was a
`<waitkey>` **in front of** the name prompt; this is one notch worse. Nothing
in the game tests the gender (not one gender restriction in the file), but the
dialog is unconditional because `PlayerGender` is Unknown, so line 2 has to
answer it. `SCR_SKIP_WAITKEY=1` on the row; `waitkey_audit.py` calls it IMMUNE.

**The days are task boundaries, not a clock**, which is how a game with a day
structure gets away with having no events and no variables. T35 `ask ross about
club`, T61 `ask ross about will` and T63 `ask ken about trey` end Days 1, 2 and
3; each carries an `ACT type=1` sending the player home, and every "which day
is it" test in the file is a task restriction or a room `ALT` on one of those
three. **All three are `where=3`** — probed, `s` / `d` / `ask ross about club`
from a fresh start ends Day 1 in three moves from the player's own living room,
with Ross absent and never met. The committed route walks to the NPCs anyway.

**Two rooms have deadlines and one of them costs points.** `EXIT room=9 E ->
dest=10 gateTask=35 wantDone=0` shuts the Photography Classroom the instant Day
1 ends (nothing in it scores). Ross's Living Room is reachable only through T10
`knock on door`, whose restrictions are "T35 done AND T61 not done" — a window
exactly one day wide — and the cigarette in its ashtray is worth 10 across T44
and T58. Miss it and the run lands on 190/200 with a clean transcript and
nothing anywhere to say why.

**Two engine-fidelity witnesses, neither of which changes anything.** T47 `turn
on tv` is **`where=0`**, runnable nowhere — a third corpus case after *The
Hangover* and *La hija del relojero* (`adrift4-where-norooms.md`) — and it is
invisible in play because the tv is static and the library answers "You can't
turn that." first, which under the Runner's `OUT = "" And FLAG = 1` guard
suppresses the room refusal. And that refusal **does** fire here: `knock on
door` typed in the Office (T10 is `where=1 room=5`) answers **"You can't do
that here!"**, the first English-string witness for the 2026-08-10 port —
*La hija del relojero*'s evidence was a Spanish message replacement.

**The author hint menu is here, and for once it was not the walkthrough.**
Third game running with `HINTQ=`/`HINT1=`/`HINT2=` in the dump, first where
none of the five entries names a command — they are prose ("Try looking on the
couch...", "Dr. Ross might know something, but you'll have to know what to ask
him."). They identified the five gates; the phrasings still came from `cmd=` /
`ALTCMD=`. Grep `HINT2=` first anyway — it saved the search, just not the
derivation. Watch for the author's typos while reading those lines: `use baggie
on gun` for *gum*, `open des`, `use camersa`, `turn off the famn`, and a hint
that reads "You musk speak to Chief Branis".

Losing endings: T64 `accuse rick` and T65 `accuse ross` are `ACT type=6 v1=1`,
T66 `accuse ken` is the file's only `v1=0`. All three read as a confident
arrest for a paragraph before the trial goes wrong, so the row is anchored on
**"Ken is found guilty of triple homicide."** and not on anything earlier.
`Globals/DispFirstRoom` is false in this file, so the transcript opens with no
Office description — the author's setting, honoured by `run_main_loop()`.

## The Vampire With A Conscience (2026-08-14) — a doubled award, and `wait` is worth three turns

`Vampire.taf`, 63,183 bytes, ADRIFT **3.90**, Ole Olsen, version 1.0. 17
rooms, 137 tasks, 49 objects (33 static), 11 NPCs, 11 events, 8 variables.
**WIN, 100/100 — the file's declared maximum — in 57 input lines.** Row:

```
vampire_solution.txt|Vampire.taf|Now you are the most powerful vampire alive.|SCR_SKIP_WAITKEY=1
```

Full write-up in `notes/The_Vampire_With_A_Conscience_walkthrough.md`. Four
things are worth carrying forward.

**1. The award total overshoots the maximum because one award is authored
twice.** 18 `ACT type=4` awards summing to **110** against a declared MaxScore
of **100** — the first corpus file where the arithmetic does *not* close on
its own. T94 and T95 are the same `push * %number% *` in the elevator with the
same four actions and the same +10, differing only in what they demand first
(T94 wants `ask portiere about igor van der linden`; T95 wants both duct
`listen`s). Forward first-match dispatch runs at most one task per command, so
exactly one of the two can ever pay. 110 − 10 = 100, and the ceiling is
exactly reachable. **Before reporting an award total that exceeds MaxScore,
check the list for two tasks with the same command pattern and the same
award** — an author writing alternative prerequisites for one puzzle is the
likely cause, and the duplicate is unreachable by construction.

**2. `wait` is not one turn.** `Globals/WaitTurns` is 3 in this file, so every
`wait` in the script burns three turns of event clock and three minutes of the
game's own minute counter, while printing a single "Time passes...". This cost
an hour of confusion: `SCR_TRACE_EVENTS` showed EVENT 3 `[MistForm]` ticking a
full 8 turns while the transcript showed only three prompts, and EVENT 8
`[RaiseJon]` (Time1=Time2=20) clearing after twelve commands. Both are correct
once the multiplier is in. **When event timings look ~3× too fast, read
`Globals/WaitTurns` before suspecting the engine** — `sclibrar.cpp:2347` sets
`game->waitcounter = game->waitturns`, and `scrunner.cpp:2415` runs a full
turn per decrement.

**3. The event index conventions are not uniform, and the dump resolves them
for you.** Confirmed against `scevents.cpp` on this file: `StartTask` and
`TaskAffected` are *one*-based (`task - 1`), but **`PauseTask` and
`ResumeTask` are two-based** (`pausetask - 2`, with 1 meaning "any task"), in
`evt_pauser_task_is_complete()` / `evt_resumer_task_is_complete()`. Reading
EVENT 2 `[Nutriton]`'s `pauseTask=56` as T55 rather than T54 sends you looking
for a reason the *hotel* guard should stop a starvation clock; the right
answer is T54 `drain jon simonsen`, i.e. the plot's murder is also the meal.

**4. Alternate-solution machinery that the winning route never touches.** The
bathroom's shaving foam (T130/T131, with a `foamLeft` counter) exists to
smear the two mirrors in front of the conference-room door, and the coffin's
earth (T50) reads like a vampire staple — neither is named by a single
restriction on the way to the win, because the mirrors only ever matter to a
*living* companion and yours is undead by then. Worth logging rather than
deleting: a `.taf` can carry a complete second puzzle chain that the maximum
score does not require.

The file's hint menu is the corpus's most honest: six entries covering the
hotel room, ending at **"OK, I got downtown. What now?" → "Sorry, you're on
your own from now on."**

## The Merry Murders (2026-08-14) — a lower-indexed ALTCMD that silently steals the verb

`Merry_Murders.taf`, ADRIFT 3.90, 69,489 bytes, 15 rooms, 76 tasks, 8 NPCs —
and **2 events, 0 variables**, the least machinery of any wired game in the v4
corpus. It is a seven-act locked-floor whodunit at a company Christmas party,
and it is the cleanest scoring file yet seen: **20 `ACT type=4` awards summing
to exactly the declared MaxScore of 135, every one of them on the single
critical path.** Full details in `notes/Merry_Murders_walkthrough.md`; three
things are worth carrying forward.

**1. A lower-indexed task's ALTCMD can swallow a later task's command, with no
diagnostic.** `read paper` is `ALTCMD[1]` of **T37 `read list`**; the note you
actually need to read is **T39 `read piece of paper`**, whose own `ALTCMD[1]`
is `read paper ` — *with a trailing space*, so it never matches. Forward
first-match dispatch re-reads the employee list, prints a perfectly plausible
response, and T39 stays incomplete — which leaves the janitor's closet locked
(room 1's N exit is `gateTask=39 wantDone=1`) for the rest of the game. When a
gated exit refuses to open even though you "did the thing", **grep the dump
for every task whose `cmd`/`ALTCMD` matches what you typed and take the lowest
index**, not the one you meant.

**2. An award-bearing task does not have to move you.** T46 `n` in the Computer
Lab pays +5 and prints "The lock opened, allowing me access into the
archives" — and leaves you in the Computer Lab. The second `n` is the exit.
Scoring text that reads like a transition is not evidence of one; check
whether the task has an `ACT type=1` at all.

**3. An `ACT type=1` NPC move is not the last word — a `WALK` can override
it.** T27 `research alex` is authored `v1=7 v2=0 v3=3`, "move NPC 5 (Trey) to
room 2", but Trey owns a `WALK` with `startTask=10` (task 9) that has already
fired, and he is in the **Plaza**. `show list to trey` (T38) is `where=2` over
`WHERE_ROOMS=[0 2]`, so both are legal venues and only one of them has him in
it. Where a mobile NPC *is* has to be read off the transcript, not off the
task that last moved them.

Also a reminder about instrumentation: `grep -c waitkey` on a
`SCR_DUMP_TASKS=1` capture returns 0 for this file, because the dump goes to
stderr and the tags live in the *transcript*. `SCR_MARK_WAITKEY=1` finds six,
one per act transition.

## The Woods Are Dark (2026-08-16) — a whole game with no clock in it

71,216 bytes, ADRIFT 3.90, **Cannibal**, 2003.
`https://ifarchive.org/if-archive/games/adrift/WoodsAreDark.zip`. 23 rooms, 82
tasks, 18 objects, **10 variables and 0 events**. **WON 100/100** in 73
commands through T48 `take head` in the Graves; 21 `ACT type=4` awards sum to
exactly the declared maximum and all 21 are on the critical path, so the
ceiling is the maximum and nothing is missable. Full write-up in
`The_Woods_Are_Dark_walkthrough.md`.

You are looking for two friends who walked into the woods above Black Hill and
did not come out, and the game is the night you spend in the cottage where the
Doherty family were murdered five years earlier. Each ghost you satisfy pays
out a piece of what happened, until the picture you hang in the master bedroom
puts you at the graves with the man who did it.

**No events, no NPC walks, no timers of any kind.** That is unusual enough in
this corpus to be the headline: with nothing on a clock there is no pacing to
get wrong and no `z` to spend, and the entire game is a dependency graph held
in seven of its ten variables (`cat`, `hearth`, `trunk`, `melissa`, `drew`,
`hook`, `attic`). Derivation was correspondingly cheap — read the variable
writes out of `SCR_DUMP_TASKS`, topologically sort the awards, walk it once.

Three findings worth carrying:

**1. A game-wide task can have a room *cut out* of it to make room for a
scoring twin.** `unlock window` matches two tasks. T51 `* lock * window *` is
`where=2` and carries `* unlock * window *` as ALTCMD[2] — but its
`WHERE_ROOMS` list is `[0 5 6 7 8 9 10 11 13 14 15 19]`, and room **12**,
Drew's Bedroom, is conspicuously not in it. So in the one room where the cat is
sitting outside the glass, the command falls past T51 and reaches T16, whose
own ALTCMD[1] is the same pattern and which pays +5 and sets `cat = 1`. When
two tasks share a verb, check the `WHERE_ROOMS` list before assuming the
lower-indexed one always wins — the author may have punched a hole in it.

**2. A task can relocate the player mid-message, and the prose is the only
notice.** T10 `bounce ball` has an `ACT type=1` to the Back Yard buried in a
long passage that reads as happening where you stand; the giveaway is one
clause, "against the wall in the backyard". Every plan for the second half of
the route that starts from Melissa's Bedroom is wrong. The general rule: read
`ACT type=1` out of the dump for *every* task on the route, not just the ones
that look like movement.

**3. Self-gating tasks are the ones that get skipped.** `lift trunk` (T21)
requires `trunk == 0` and sets it to 1; the dolls house (T31) requires
`melissa == 0` and sets it to 1. Both are one-shot, both sit in a room you have
other business in, and both pay out something you do not need for another forty
moves — T21's is the smudge of tiny writing that `look at writing` reads with
the looking glass at the very end of the chain. A restriction of the form
"variable X is still 0" combined with an action that sets X is a reliable marker
for "do this on the first visit or not at all".

The one place the route spends a turn on nothing is the Clearing: T52
`hang picture` drops you in room 20, and T45 there is a bare `[*]` with no
restrictions, so *any* command is consumed by the forwarding into the Graves.
The solution spends a `look` on it rather than losing `take head`.

## Captive Universe (2026-08-16) — a one-shot clock, and a task that undoes itself

Next in the smallest-first order. `Captive.taf`, 74,568 bytes, **3.90**, no
author recorded anywhere — the endgame signs off *"Based on Captive Universe by
Harry Harrison"*, after the 1969 novel. 62 rooms, 61 tasks, 49 objects, 2 NPCs,
19 events, **no variables at all**. **WON 100/100** in 57 commands; suite
243 → **244 rows, 244 PASS**. Full write-up in
`notes/Captive_Universe_walkthrough.md`.

You are the next sacrifice, locked in the temple cells and due to die at dawn.
The valley you have lived in all your life is sealed by a boulder the gods
dropped in the only exit; it is in fact the cargo deck of a colony ship, and the
diamond in the temple throne is its navigation key.

**100/100 is provable.** Nine `ACT type=4` awards, 8×10 + 20, summing to exactly
the declared maximum, and the route fires all nine. The tenth scoring action
goes the other way and is a joke: **T3 `* hint *` is `ACT type=4 v1=-356`**, the
author's penalty for asking for a hint, and it is the only thing in the file
that can move the score off the ceiling. No published walkthrough exists and no
author hint menu either — the `HINT2=` grep that carried *Veteran Knowledge* and
*The Lost Tomb* returns nothing here — so this one came off the dump.

**The transferable finding: with no variables, all the gating is events, and
one-shot events are easier than they look.** Leaving the courtyard gate (T11)
starts *four* events in the same turn — EVENT 13 at turn 8 (arrest in rooms
11–18), EVENT 10 and EVENT 11 at turn 18 (arrest in the trees / in the open
fields) and EVENT 9 at turn 20 (nightfall). All four are **`restart=0`**, so
each fires once, at exactly that turn, and never again. The route therefore does
not need to hurry or to plan a path — it only has to be somewhere safe on turns
8, 18 and 20, and rooms 19/40/41/44 (up trees) and the swamp appear in no arrest
task's `WHERE_ROOMS`. The mirror image is the pair that gate the second half:
EVENT 14 (village) and EVENT 6 (inner temple) are `starter=1 restart=1`, running
*every* turn while `-nighttime` is undone, so the Smith, the grainhouse, your
mother and the throne are all simply unreachable before turn 20. Read `restart`
before counting anything: a `restart=0` arrest is an instant to dodge, a
`restart=1` one is a wall to wait out.

**And a shape to recognise: an event that un-completes its own starter.**
EVENT 18 [Timedoor] is `starter=3 startTask=40 affTask=40(fin=1) time1=1` — T39
levers the ledge's steel door open, and one turn later the same event marks T39
*undone*. T40 `west` is restricted on T39 being done, so `w` must be the very
next command; spend a turn on anything else and the door "beeps, flashes a
little green light, and slides shut again". Not a soft-lock — the +10 EVENT 4
already paid is kept and T39 is `rep=1` — but a route that pauses on the ledge
reads as a wrong solution. A corpus-wide `SCR_DUMP_TASKS` sweep finds
`affTask …(fin=1)` in **nine** files (`Captive`, `Mangiasaur`,
`To_Hell_And_Beyond`, `Vendetta`, `humbug` ×2, `losttombv2`, `the_pk_girl`,
`tra` ×6, `wrecked` ×17), nearly all of them pointing back at their own
`startTask`. **Symptom to watch for: a step that visibly succeeded stops
counting as done a turn or two later.**

**Third witness for `Globals.WaitTurns` = 3** (after Cursed and *The Vampire
With A Conscience*), and here it is load-bearing in the other direction: the
four `z`s that pace the route to nightfall are *twelve* turns, so counting them
as four would put the wait five turns short.

**Two entrances, same 20 points, and the route deliberately takes the long
one.** The ship can be entered by the ledge (rope → `tie rope to ledge` → `u` →
`use crowbar` → `w`) or through the swamp (`use crowbar` then `swim` in room 35,
crowbar only). EVENTS 2/3 both pay T17 and EVENTS 4/5 both pay T19, so the score
is identical and the swamp is eight commands shorter — verified, it wins
100/100 too. The committed route takes the author's designed path so the
regression covers both NPCs, the three chained Smith events (crowbar → rob the
grainhouse → grain-for-rope) and the timed door rather than routing around them.
Same call as *Salutations*.

Two small author notes worth having: the four `open * door` tasks T4–T7 are
distinguished **only by how many `*`s the pattern carries**, one per pair of
rooms, so the player types `open door` everywhere and never sees the seam; and
T41–T44 each chain **three `ACT type=1`s in one task**, walking the ship's four
passageway rooms in a single turn, which is why rooms 56–59 exist on the map and
are never seen.

## Where everything is

| What | Where |
|---|---|
| The manifest — one line per row, `solution\|game\|win-marker\|env` | the table at the top of `harness/run_v4_walkthroughs.sh` |
| Routes and their recorded transcripts | `goldens/<name>_solution.txt` + `<name>_solution.expected.txt` |
| **Per-game analysis, route prose and score accounting** | `notes/<Game>_walkthrough.md` — 194 of them (192 tracked; the two AIF ones, *Archie's Birthday* and *Diary of a Stripper*, are gitignored) |
| Engine fidelity questions raised along the way | `../../../RUNNER_TESTS_TODO.md` |
| Which rows a game's `<waitkey>` is eating commands from | `python3 harness/waitkey_audit.py` |
| The full session-by-session derivation log (2026-06-24 → 2026-08-04) | git history of this file; it was pruned in the commit that added this line, so `git log --follow -p -- test/adrift4/notes/WALKTHROUGH_TODO.md` has all 4134 lines of it |

## Standing cautions

**Per-game verdicts were revised more than once — trust the per-game doc, not a
remembered summary.** Four "unwinnable" calls were overturned by later work, and
each reversal is written into its own note: WesGHN (30/100 → **WON 100/100**,
the gold ring was never orphaned), Mr Smith and Villains & Kings (both won once
the 3.9 battle path was version-gated), and The Plague – Redux (won via the pole
bypass, after `Where`/Type 0 was settled against run400). If a game's ceiling
matters, read `notes/<Game>_walkthrough.md` — it carries the current verdict and
the evidence for it.

**A published *session transcript* is Runner ground truth; a hand-written
command list is not.** Two transcripts caught real engine bugs. But a transcript
produced by stock SCARE (ClubFloyd's Floyd, recognisable by "Welcome to the
Cheap Glk Implementation" in the log) is not an oracle — it is our own engine,
one version back.

**Before declaring an object orphaned, decode every event's `o2`/`o3` with the
raw−1 rule** — especially when an event is named after the object. That is what
made WesGHN look unwinnable for two months.

**Author "to-do"/objective lists, hint menus and flavour text can all lie.** A
task can print "you can't…" and still score, if its restriction is an OR with a
passing branch. Trust the trace and `score`, not the prose.

## Per-game workflow

1. **Boot & look.** Run with an empty solution; read the intro, first room,
   objects, NPCs. Keep a running `solution.txt` (one command per line) of the
   *confirmed* path; replay it every iteration (`harness/play.sh`), appending
   probes.

2. **Dump the structure up front** (this is what makes it fast — don't brute the
   parser). With `SCR_DEBUGGER_ENABLED=1`, type `debug` then:
   - `tasks 0 N` — every task's **exact command pattern** (the verbs the author
     expects, e.g. `perform dna analysis`, `get sample from limb`). Find N from
     the "valid values are 0 to N" error.
   - `rooms 0 N` / `objects 0 N` / `npcs 0 N` — names, **locations**, container
     contents, hidden/locked flags. This locates keys, weapons, the win item,
     without searching blindly.
   - `events 0 N` — timed/triggered plot (alarms, attacks).

   Kill the process fast (it EOF-loops after the dump): wrap with
   `perl -e 'alarm 8; exec @ARGV' env SCR_DEBUGGER_ENABLED=1 ./scare GAME`, and
   read the output with `grep -a` (the stream contains NUL bytes).

3. **Get the exact scoring map.** `SCR_DUMP_TASKS=1` prints every task's actions,
   including `ACT type=4` (ChangeScore) with its points, and `expr=[...]` for
   type-3 variable actions. The points should sum to the game's stated maximum;
   that tells you exactly which tasks to complete. Where the score is an author
   *variable* rather than the engine score (The PK Girl, Three Monkeys), count
   the type-3 actions on that variable instead.

3b. **Grep that same dump for `HINT2=` before deriving anything.** ADRIFT
   stores per-task author hints, and `SCR_DUMP_TASKS` prints them as
   `HINTQ=`/`HINT1=`/`HINT2=` — one entry per puzzle, often with the literal
   command in capitals. On the two games that shipped a hint menu (*Veteran
   Knowledge*, *The Lost Tomb*) that was the whole walkthrough, for free. Do
   **not** type `hint` in play instead: it prompts `[Y/N]` and echoes its own
   prompt twice, which no golden survives.

4. **Play to a win**, banking confirmed steps into `solution.txt`. Use the
   `tasks` patterns for verbs and the `objects`/`npcs` dumps for "where is X".
   Wandering NPCs: find the deterministic turn they are present (trace with
   several `look`s) rather than guessing.

5. **Push toward max score.** Compare the scoring map to what you have; for each
   missing point, find the task and satisfy it.

6. **Diagnose anything that won't score** before calling it unreachable.
   `SCR_TRACE_TASKS=1` (or `SCR_TRACE_FLAGS=256`, `+8` for the parser) prints,
   per task, the restriction bracket expression (`#A#A(#O#)` = R0 AND R1 AND
   (R2 OR R3)) and each restriction PASS/FAIL with its operands — so you learn
   *why* a task fails. `SCR_TRACE_MATCH=1` shows which task claimed a command.

7. **Attribute unreachable points honestly.** Decide whether it is the **game
   data** or **Scarier**. The restriction operators live in the `.taf`, and both
   Scarier (`screstrs.cpp`) and the real Runner (`evaluaterestrictions` =
   `Sub_20_57`, in `~/Desktop/run400.txt`) evaluate the same expression. If we
   match the Runner, the bug is the author's and exists in the Runner too — say
   so. Only call it a divergence when our evaluation actually differs, and
   prefer settling it by *playing* run400/run390 under Wine over reading P-code.
   For "is it really unwinnable?", dump the action histogram and check it
   against a known-winnable file **of the same TAF version** as a positive
   control — 3.9 `circus.taf` has 24 type-6 EndGame actions, inverness has none.

8. **Verify & write up.** Re-run the final `solution.txt` three times (identical
   score and win marker — determinism guarantees this). Write
   `notes/<Game>_walkthrough.md`: header (author/comp/result), full command
   list, phase-by-phase prose, and a closing note on unreachable points with the
   evidence. Save the route as `goldens/<game>_solution.txt` and add the row to
   the manifest.

## Footguns / lessons learned

- **Rebuild before trusting anything.** A stale `harness/scare` cost real time
  on a seed sweep. `./build.sh` first. (The built binaries in `harness/` are
  untracked; only the sources and scripts beside them are committed.)
- **Always `git checkout` temporary instrumentation** (`scbattle.cpp`,
  `sctasks.cpp`, …) — leave the tree clean.
- **`Globals.WaitTurns` is per game**, and in Cursed it is **3**: one `z` runs
  three turns of events, so any cut-scene stepped beat-by-beat desyncs if you
  count `z`s as turns. Measure it before counting:
  ```
  printf 'z\nquit\ny\n' | SCR_TRACE_EVENTS=1 ./scare GAME 2>&1 >/dev/null \
    | grep -ac '^Event: ticking event 0:'
  ```
  Second witness 2026-08-14: *The Vampire With A Conscience* is also 3, and
  there the symptom was the opposite reading — every event looked like it was
  running ~3× too fast against a script full of `wait`s. Same cause, same
  check.
- **Check `OBJNAME … prefix=[…]` before believing "I see no such thing"** — the
  author's prefix is often part of the only accepted phrasing.
- **A route that comes up short with a clean transcript is usually a
  restriction you read as "held".** In `RESTR type=0`, `v2=1`/`7` is *held by*
  (which does include worn, and one level of container nesting) but **`v2=2`/`8`
  is *worn by*, and nothing else**. Authors pair the two into a task that
  succeeds either way but only scores when the object is worn — *The Lost Tomb*
  loses 20 of 175 points that way, with no failure message anywhere. The same
  signature comes from an end-of-turn event whose revealed object is not
  takeable until the next turn (its `take` fails silently too). When points go
  missing, diff the `SCR_DUMP_TASKS` restrictions of the task that *did* fire
  against the one that should have.
- **Two tasks with the same patterns: the lower index wins, always.**
  `run_game_commands_common()` scans forward and the first match whose `Where`
  and restrictions pass runs. Authors use this deliberately — the restricted
  "success" task above the unrestricted "you can't do that yet" one — and when
  they get it backwards the success task is *dead*, with a plausible failure
  message covering for it (*The Long Journey Home*, T22 vs T25, −10). **Before
  suspecting an engine divergence, look for the other sibling pairs in the same
  file**: an author who relied on the order three times and got it right twice
  has told you what the order is, and one of the pairs is usually a death task
  whose reversal would make the game unplayable for everyone. That is cheaper
  and more conclusive than a Wine session.
- **A `#`-labelled task with no ALTCMDs is not automatically dead.** ADRIFT
  literal command patterns match literally, so a player *can* type the author's
  internal label — `!goto lair` / `!random` (debug tasks shipped in *The Long
  Journey Home*) both fire. Only our headless front end refuses, because
  `os_ansi.cpp:286` treats a script line beginning with `#` as a comment. To
  prove a task genuinely unreachable you need all three: no matching pattern,
  no event `affTask`, and no `ACT type=5` anywhere in the file.
- **Determinism = combat reproducibility.** The Battle System is RNG-driven and
  the seed shim is what makes scores stable. "Doesn't seem to do any damage" is
  the faithful `damage = strength − defence ≤ 0` branch, not a bug.
- **The debugger EOF-loops** after a dump and floods MB of output — cap it with
  `perl -e 'alarm S; exec @ARGV'` (there is no `timeout(1)` on this Mac) and
  read with `grep -a`.
- **`tasks` with no range lists only *currently-runnable* tasks**; use a range.
- **zsh: `r=$(… | grep -c pat)` can yield an empty string**, so `[ "$r" != "0" ]`
  is true every iteration — a seed sweep once reported 40 consecutive false
  wins. Write the run to a file and use `grep -q`.
- **Goldens are extended-ASCII with NEL terminators**: `export LC_ALL=C` and
  `grep -a`, always.
- **Fetching games:** `Range: bytes=0-13` classifies a remote `.taf` cheaply,
  but adrift.co sometimes answers a ranged GET with an empty body — retry a
  short reply unranged or the file is silently misfiled. Percent-encode
  filenames with spaces or curl fails outright. The corpus is pinned by sha256
  in `test/adrift4/games.manifest.tsv`, fetched by `test/fetch_games.sh`, and
  the whole arrangement is explained in `test/GAMES.md`.
- Use the scratchpad for throwaway files; keep only the solution, the
  walkthrough and the harness here.


## Appendix — derivation entries kept because nothing else records them

The session-by-session log this file used to carry was pruned once the corpus
was complete; the whole of it is in git (`git log --follow -p -- this file`).
These fourteen entries are kept **verbatim** because they are the only write-up
for one or more games — those games never got a `notes/<Game>_walkthrough.md`,
so pruning them would have lost the route reasoning outright. Newest first, as
in the original log.

The games that depend on these entries: **The Hangover** (unwinnable, max 5/7),
**Troll!**, **Locked door with water trap**, **A Spot Of Bother**, **The
Amulet**, **Monsters (r2)**, **Shadrick's Travels**, **Beanstalk**, **Dancing
Even Him**, **Doomed Xycanthus**, **Black Sheep's Gold**, **akron**,
**twilight**, **Duck McCloud**, **Fistandantalus**, **James Bond 2000**,
**Microwave Man**, **Life of Mike**, **Super Liam**, **Where Are My Keys** and
the two French games (**Qui a tué Dana?**, **Enquête à hauts risques**).

Rows banked from the external adrift-battle corpus in 2026-06 — ptbad, vague,
Escape To New York, unauthorized termination, To Hell In A Hamper, marika,
Vendetta, Unraveling God, mishmash — were never written up here either; for
those, the manifest row (win marker + env) and the golden transcript are the
record.

---
## 2026-08-04 (latest) — the five missing tafs, wired — **203 PASS** — and an engine bug in every pre-4.0 game

The last five `downloaded/` walkthroughs whose `.taf` was not in `games/`. The
files arrived as `/Users/administrator/Downloads/missing` and are now
`games/imagi.taf`, `games/CD.taf`, `games/Chosen.taf`, `games/TheCellar.taf`
and `games/panic.taf`. All five are wired, blessed and green, and
**`downloaded/` is now fully wired — every walkthrough in it has a game and a
row.** Per-game docs: `ImagiDroids_walkthrough.md`,
`CrimsonDetritus_walkthrough.md`, `Chosen_walkthrough.md`,
`TheCellar_walkthrough.md`, `Panic_walkthrough.md`.

* **ImagiDroids** (Woodfish, 4.00) — the single ending, 20 commands, one
  authorial repair: `open it` after `x clean area` / `take brick` resolves the
  pronoun to the clean area, so it has to be `open brick`. No score system.
* **Crimson Detritus** (Mystery, 2003, 4.00) — ★ **WON 100/100**, all eight
  `ACT type=4`, 16 commands. One repair: the upstream transcript pasted two
  commands onto one line (`take uniform and wear it`), split into
  `take uniform` / `wear uniform`.
* **Chosen** (Ryan J. Bury, ADRIFT MiniComp 2001, 3.90) — ★ **WON 300/300**,
  the game's own stated maximum, 51 commands. The upstream file is **prose
  hints, not a command list**, so the route is derived from the dump. Three
  traps: `pull lever` in room 8 must come before room 7 or unguarded TASK 9
  kills you; every phrasing of `take block` in room 14 is TASK 14, a death; and
  the six sockets have to be spelled **A-D-R-I-F-T** in order. Two repairs the
  route needs: `take belt` first (it starts inside the trousers and TASK 0
  wants it *held*), and full block names only — `take block` is "Take what?".
* **The Cellar** (David Whyld, 2007, 4.00) — the ending, 132 commands replayed
  **verbatim** from the ClubFloyd log of 12 June 2022, nothing repaired. No
  score and no `ACT type=6`; it ends by setting `VAR 12 [game over]`. The game
  also ships **its own 24-command walkthrough** on TASK 1 (`walkthrough`), which
  replays verbatim too — so the path is confirmed from two independent sources.
  The ClubFloyd route is the one wired because it reaches far more of the 141
  tasks. Its two divergences from the log are both **ours to be proud of**:
  ClubFloyd's Floyd plays through stock SCARE ("Welcome to the Cheap Glk
  Implementation" is in the log), so the log is not an oracle, and both places
  we differ are places scarier has since been made more faithful — the
  in-container description and the run400 postfixed one-object format recorded
  in `lib_list_in_object()`, and a real U+2026 that cheapglk folded to `...`.
  Word-level agreement 98.3%.
* **Panic!** (Stewart J. McAbney, 3.90) — the ending, "Your rating is Messiah.",
  69 commands from the author's own full-session walkthrough, replayed
  verbatim. It found a real bug — below.

**The engine bug: the 3.9/3.8 immediate-restart fixup was eating StartText.**
`evt_fixup_v390_v380_immediate_restart()` (`scevents.cpp`) restarted a pre-4.0
`RestartType=1` event by hand — state to `ES_RUNNING`, clock to one less than a
fresh roll — instead of calling `evt_start_event()`. The event *cycled*
correctly, and its TaskAffected kept firing (which is the half §2 of
`RUNNER_TESTS_TODO.md` probed live, and it was never wrong), but its start
actions, StartText included, ran only on the very first start. So an
always-restarting one-turn event printed its line once per game and was mute
after that.

Panic! is built out of these, and two of them carry a StartText and **no**
LookText (`texts=S--`), so nothing else could be printing them. Against the
author's transcript: the priest's cough 66 published / 1 before / **66** after,
the stigmata 9 / 1 / **9**, the shaking 13 / 1 / **13**, the wraith 21 / 1 / 24.

Fixed 2026-08-04: call `evt_start_event()`, then take the one silent turn off
the clock. The length roll must come from `evt_start_event()` **alone** — the
first attempt kept the fixup's own `scr_randomint()` as well, and the extra
roll per restart churned the RNG stream enough to break three previously
winning routes (circus, thetest_win, wrecked). Two rolls where the Runner has
one is its own bug.

**Corpus effect:** 13 of 203 rows gained text, every diff a *pure addition* of
a line that had been silently swallowed. Mostly ambience — troll's "Sid sips
from his mug.", haunt's grandfather clock, wrecked's seagulls, spirits_flight's
wind, tq3's rattlesnakes, colony's lightning, twilight's apparition,
timmy_reid's Billy on his bike, cybercow_win's robot — but four are **plot**:
secret_of_lost_world's "It starts to rain." and "The volcano is erupting.",
marooned's rescue ship coming over the horizon, and the entire paragraph in
alices_restaurant where Obie takes your wallet and your belt at the station.
No route broke, no win marker moved, and `thetest`, `gateway` and `inverness` —
the games probed live in run390 for §2 — are byte-identical. Written up as
`RUNNER_TESTS_TODO.md` **§8**, which left two open probes: the wraith's
visibility while you are up the rope (Panic!'s one residual divergence, turns
44–46), and whether a restarted period with `Time1=5` is 5 turns or 4.
**Both were closed the same day** — the period keeps its full authored length
and the restart is silent, and `make_39_evseeprobe.py` refuted the visibility
theory outright (run390 prints event text while the player is sitting on a
surface or inside a container, exactly as we do; the 24-vs-21 count that raised
it was an RNG divergence, the player is never up the statue on those turns).
See §8 for both.

## 2026-08-04 — the three non-plain-text documents, wired — 169/169 PASS

The last three entries in the `downloaded/` queue were the ones that are not
plain text: two PDFs and a Word document. All three are now wired; the
"not plain text" bullet in the PARKED section below is struck through.

**Second Chance** (David Whyld, 2005) — ★ **WIN, the good ending, VERBATIM.**
`SecondChance_walkthrough.pdf` is not prose but a **full session log**: every
command is on its own `>`-prefixed line, so the 49-command script falls out of
the document mechanically and replays without one repair. The only thing that
needed knowing was a harness detail — the title sequence embeds two
`<waitkey>` pauses, which eat the first two commands and desync the whole run,
so the row carries `SCR_SKIP_WAITKEY=1`. The game keeps **no score**
(`score` → "No one's keeping score."); its endings are ranked by whether the
three vignettes went well (Dolores → `push button` to call the police; Jenny →
talk, never the "ask about sex" branch; Doug → three `talk to doug`; Antonia →
actually search her room). The marker is therefore the closing line of the
good ending, `congratulating me on a job well done.` See
`Second_Chance_walkthrough.md`.

**Private Eye** (David Whyld, 2006) — ★ **WIN, score 4, best ending,
VERBATIM.** A pure numbered-choice game: no parser verb appears anywhere in
the 73-choice route, so the solution file is a column of digits. The
walkthrough section of the shipped 116-page `PrivateEye_guide.pdf` replays
exactly. Two harness facts closed it: the PDF silently omits the title menu
(the script carries a leading `3` = "Play Private Eye"), and
`SCR_SKIP_WAITKEY=1` is mandatory or the run never leaves the menu. One
12-word sentence in the PDF — "No sooner have I put the phone down than Jim
ambles in." — has no counterpart in the transcript; it is the author bridging
two scenes in the write-up, **not** output we drop: the game's own wording is
longer ("…and plonks himself down in the other chair"), it lives on the
ex-girlfriend phone-call tasks rather than Layla's, and the short form appears
nowhere in the inflated `.taf`. Word-diffed, 19293/19371 words identical
(0.9975). See `Private_Eye_walkthrough.md`.

**The Plague – Redux** — ~~**UNFINISHABLE AS SHIPPED**~~ **[VERDICT REVERSED
2026-08-04 — the game is ★ WINNABLE via the where=1 pole-bypass tasks; see
the entry at the top of this file. The where=0 analysis below stands; the
dead-end conclusion does not.]** Original entry, kept for the probe record: The game's whole `[F] Fight / [E] Escape` system is seven
identical task blocks (`ZOMBIE 1`…`ZOMBIE 7`) in which **every** task sits at
Where/Type = 0 (`ROOMLIST_NO_ROOMS`) — 243 of the game's 696 tasks are parked
there. Nothing `ExecTask`s the `[f]`/`[e]` pair, so once "[F] Fight or [E]
Escape?" prints there is no input that answers it (`f` → "That didn't make any
sense!", `fight` → "That wasn't the answer.", `e` is eaten by the library as
*east*). The first mandatory fight is the Women's Toilet cubicle, whose coins
are the last 10p of the £1.20 the water vending machine wants — so the route
dead-ends at £1.10 and everything downstream (Kate, the office vent, the
camera batteries for the torch, Ray, the staff-area keys, the tunnels,
Candice, the ending) is unreachable.

Proved against the real Runner **twice**, which is what makes this a verdict
rather than a suspicion:

1. **Isolated probe** — new `test/adrift4/harness/make_400_whereprobe.py` builds a minimal 4.0
   game with `alpha` at where=0, `beta` at where=3, `gamma` at where=1 scoped
   to the other room, repacked with `taftool.py`. `run400.exe` fires beta,
   refuses gamma, and refuses alpha — identical to SCARE. So SCARE's
   `ROOMLIST_NO_ROOMS` handling is *correct*, and a where=0 task genuinely
   cannot be typed.
2. **Game-level probe** — a copy of this very game with `#StartRoom` patched
   `0` → `15` (Women's Toilets; line 80 of the unpacked plain body, right
   after the `bd d0` separator at line 79) loaded in `run400.exe` under Wine
   reaches the byte-identical cubicle scene and answers `f` with "That didn't
   make any sense!".

This is the **second** game in the corpus killed by this exact mistake — The
Hangover (2026-08-03, below) loses its two endgame tasks the same way, checked
against run390. Worth remembering as a diagnosis: when an ADRIFT 4 walkthrough
asks for a command the game flatly does not understand, dump the tasks and look
at `where=` before assuming a parser bug.

The row is blessed as a documented dead end — a maximal-reachable run that
replays the shipped `.doc` as far as the game allows, types `f`/`fight`/`kill
zombies` at the prompt to record the three refusals, collects all five
reachable coin caches, and ends at the vending machine. Marker: `I pressed the
vend button but nothing happened.` See `The_Plague_Redux_walkthrough.md`.

Two derivation footguns from this game, both costly: **the discovery verb is
`SEARCH`, not `EXAMINE`** — the `.doc` says "Examine the till" throughout, but
`x till` answers "nothing interesting" and only `search till` works (same for
rides, body, counter, windows, highchairs, bench, condom machine, Nick, bin,
footwear, desk) — and **`x coins` is the money counter**, `count coins` is not
a verb.

## 2026-08-04 — six more 3.80 games, and the only two surviving 3.70 games

The 2026-08-03 survey below concluded "exactly 11" ADRIFT 3.80 games exist
online. That was too low, for one reason worth writing down: **adrift.co serves
files its adventure database never lists**, so enumerating the database (634
entries) misses them, and the IF Archive never had them either.

The name list has to come from somewhere else — the Wayback copies of Campbell
Wild's own *Adventures* pages: `tardis.ed.ac.uk/~jcw/adventure/adventure.html`
and `~jcw/adrift/adventure.html` (Jul 2000 – May 2001) and
`jcwild.pwp.blueyonder.co.uk/adrift/adventure.html` (Jul 2001 – Mar 2002),
11 captures, **130 distinct filenames**. Probing each against
`www.adrift.co/files/games/<f>` found six more 3.80 games, now in `games/` with
MAP rows (all six load and run; suite **168 PASS / 14 NOSCRIPT**, exit 0):

| file | title | size |
|---|---|---|
| `duck.taf` | Duck McCloud — The Fight Begins (Dale Trigg & Ashley Canning) | 7.8 K |
| `first.taf` | The book of Fistandantalus | 12 K |
| `jb2000.taf` | James Bond — Happy Landings | 11 K |
| `microwaveman.taf` | Microwave Man! | 5.2 K |
| `mikes.taf` | The life of Mike | 19 K |
| `superliam.taf` | Super Liam 1. A hero is born | 31 K |

So **17 ADRIFT 3.80 games survive**, and `microwaveman.taf` (5.2 K) is now the
smallest 3.8 game — a better first walkthrough target than `secret.taf`.

**Two ADRIFT 3.70 games also survive**, both unlisted on adrift.co and on no
other host: `arlo.taf` (*Alice's Restaurant Anti-Massacree Adventure*, Laura
Lee, 18-03-2000, 70 K) and `castle.taf` (*Castle Quest*, Andrew Cornish,
10-06-2000, 14 K). Header `3c423fc96a87c2cf94453961 39fa` — note byte 10 is
`0x39`, **not** the `0x35` a naive "one less than 3.80" guess predicts, which
is why an earlier sweep with a guessed signature found nothing. The container is
identical (same XOR'd CRLF plaintext, same `taf38schema.xor`).

*(Same day, later: **both are now playable and in `games/` with MAP rows.**
`V370_PARSE_SCHEMA` in `sctafpar.cpp` covers the four layout differences — the
extra header integer is the **0-based winning-task index**, movements are pairs
over one flat destination list, tasks have no `BWinGame`, and a fixed block of
17 renameable built-in command words replaces the 3.80 synonyms table — and
every one of those, plus the pooled burden model and the object initial-position
list, was then **measured against the genuine `run370.exe`** rather than left as
inference. Two real bugs fell out of the probing, one of them the 3.80-wide
`Parent = -1` bug that had been hiding worn objects in `tra.taf`. See
`ADRIFT_370.md` and `../../../RUNNER_TESTS_TODO.md` §6.)*

*(Same day, later still: **both 3.70 games are now solved**, at full score —
`castle_quest_solution.txt` 17 moves / **50 of 50**, and
`alices_restaurant_solution.txt` 85 moves / **190 of 190**, every one of Arlo's
30 scoring tasks. Both PASS against blessed goldens; see
`Castle_Quest_walkthrough.md` and `Alices_Restaurant_walkthrough.md`. Castle
Quest is a pure trap-avoidance map with five scoring and five killing tasks and
no objects at all; Arlo's one hard puzzle is that `play guitar` starts the
horse's *follow-the-player* walk, so you lead the horse out of the stable and
then `stop playing` — reversing the start task cancels the walk and strands it
in the garden, freeing the cabinet.)*

*(Unrelated, same day: 38 of the `games/` entries were symlinks into
`~/Downloads/plover-adrift-2026-08/`, which briefly vanished and SKIPped 38 rows.
**Every symlink in `games/` has since been replaced by a real file** — all 72 of
them, not just the 38 — so the corpus no longer depends on anything under
`~/Downloads`. 191 files, 25 MB. Suite: **173 PASS / 0 FAIL / 0 SKIP /
14 NOSCRIPT**, exit 0 — every game in the MAP is present and passing.)*

Nothing **3.60 or older** survives anywhere: the IF Archive holds zero (its
Jan-2001 index listed the same seven 3.80 files it has today, and re-decoding
its 9 unclassified `.taf` headers shows all nine are 5.00), the adrift.co store
holds zero across 619 DB entries + 540 further Wayback-known filenames, and the
oldest game Campbell ever listed is `haunted.taf` (13-06-1999) — which survives
only as a 3.80 re-save. `jacjim.taf` (*Jacaranda Jim*) is gone from every
reachable host. Note that ADRIFT 3.23 (13 Jun 1999) was the first version to
encrypt TAFs at all, so anything older would be **plain ASCII** starting
`Version 3.xx` — no such file turned up either.

Method gotcha worth keeping: `Range: bytes=0-13` classifies a remote `.taf`
cheaply, but adrift.co sometimes answers a ranged GET with an **empty body**;
a short reply must be retried unranged or the file is silently misfiled as
unknown. Filenames with spaces must be percent-encoded or curl fails outright.

Aside, same sweep: `www.adrift.co/files/games/tagv2.zip` is Campbell's **TAG**
(1994–97 DOS Pascal ancestor of ADRIFT) — `gen2.exe`, `genrun2.exe`,
`space.tag`, `BAR.TAG`. Its `genrun2.exe` is MD5-identical to the one shipped
with Tony Ash's *The Edge of the Abyss*, i.e. this is the `tag.zip` that
"Abyss"'s ABOUT.TXT says vanished with Campbell's old URL. Not a taf; nothing
for Scarier to do with it, but it is the whole surviving TAG corpus (3 games).

## 2026-08-03 — the complete TAF 3.80 corpus is in `games/` — 165/165 PASS, 8 NOSCRIPT

**There are exactly 11 ADRIFT 3.80 games online, and we now have all of them.**
Surveyed by reading the 14-byte header of every candidate on the only two hosts
that still carry ADRIFT games: all 240 files under
`ifarchive.org/if-archive/games/adrift/` (`/`, `competitions/`, `italian/`,
`old/`, `spanish/`, opening every zip and blorb) and every pre-2005 download in
the adrift.co adventure DB (634 entries; `POST /cgi/adrift.cgi` with
`page=adventures&offset=N&compid=-1&compilation=0&sort=1&asc=0&category=0&perpage=50`,
which also tags each row with its own version icon `/img/380.gif` — that tagging
agreed with the bytes on all 9 of its 3.80 rows). The Archive holds 7, adrift.co
9, overlapping in 5. For scale, the same sweep counted ~49 v3.90 and ~349 v4.00
taf instances on the Archive alone: **3.80 is ~1.5% of the ADRIFT corpus.**

Catalogues cannot answer this question. IFDB's oldest ADRIFT format is
`adrift39` (id 44) and IFWiki's oldest category is "ADRIFT 3.9 works", so every
3.8 game is filed as 3.9 in both. The only reliable test is the header: a 3.x/4.x
TAF opens with `"Version X.YZ\r\n"` XOR the fixed VB6 keystream, so the first 14
bytes are a constant per version (`V380_SIGNATURE` &c., `sctaffil.cpp:53`), and a
`Range: bytes=0-13` request classifies a remote file without downloading it.
`3c423fc96a87c2cf94453661 39fa` is 3.80; 5.00 files share the first 8 bytes and
decode to `"Version 5.00"`.

| file | title | source |
|---|---|---|
| `marooned.taf` | Marooned | IF Archive **(had, WIN 80/140)** |
| `wrecked.taf` | Wrecked | both **(had, WIN 250/250)** |
| `Crime_Adventure.taf` | Crime Adventure | IF Archive **(had, WIN 95/95)** |
| `akron.taf` | Akron | IF Archive |
| `cave.taf` | Cave of Wonders | both |
| `haunt.taf` | House of the Damned | both |
| `twilight.taf` | The Twilight | both |
| `haunted.taf` | The Haunted House (Campbell, Jun 1999 — the oldest) | adrift.co only |
| `great.taf` | The Great Escape | adrift.co only |
| `secret.taf` | Tom Ceader: Escape from the south | adrift.co only |
| `tra.taf` | The Timmy Reid Adventure | adrift.co only |

The eight new ones are in `games/` and have MAP rows in
`run_v4_walkthroughs.sh`, so they report **NOSCRIPT** until routes are derived
(suite unchanged at 165 PASS, exit 0). Their win markers are deliberately blank
— filling one in before the route exists would bless a marker nobody has seen
the game print. All eight were smoke-run in `scare` (`look` / `score` / `quit`):
every one loads, prints its title and intro, and takes commands.

**Why this matters beyond completeness:** the 3.80 burden and container model
settled against the genuine `run380.exe` (pooled burden, class costs
`0→1 1→3 2→7 3→3 4→7`, capacity `= #MaxCarried`; contents free, `Capacity` a plain
object count, dynamic containers must be *held*) was derived on three games and
can now be exercised on eleven.

**One lead fell straight out of the smoke run.** `tra.taf` prints
`gs_create: object held by nonexistent NPC, -2` three times at load — i.e.
`initialparent == -1` on the `InitialPosition == 1` ("held by") path in
`scgamest.cpp:977`, so those objects are silently **hidden** instead of starting
in someone's inventory. `-1` is 3.80's generic "no parent" filler (48 of
`tra.taf`'s 248 objects carry it), which is the tell: 4.00's convention is
`Parent 0 == held by the player`, and 3.80 looks like it uses `-1` there
instead. Worse, the same file has objects with `InitialPosition 1` and `Parent`
21 / 2 / 28 (`garage key`, `baseball glove`, `baseball`, `guitar`), which the
4.00 rule reads as NPC `parent-1` — if 3.80's index is 0-based those are all off
by one, and being off by one is *silent*. Nothing in `sctafpar.cpp`'s
`|V380_OBJECT:_InitialPositions_|` fixup touches `Parent`; it only handles
positions `2` and `> 2`. Settle it the way the size/weight model was settled —
a `make38probe.py` file with one object held by the player and one held by NPC 0,
run under `run380.exe` in the adrift-battle Wine prefix. No other 3.80 game in
the corpus trips the warning, so `tra.taf` is the test case.

> **CLOSED 2026-08-04.** Measured exactly that way, and the guess above was
> right: pre-4.0 has no way to start an object *on an NPC* at all, so `Parent`
> is meaningless on the held and worn entries. `run380` gives `tra.taf`'s red
> sox hat (`Parent 0`) and its loose change (`Parent -1`) both **to the
> player**, and `run370` gives `castle.taf`'s sweatshirt to the player whatever
> `Parent` says. The fixup — now shared by 3.8 and 3.7 as
> `|V380_OBJECT:_InitialPositions_|` / `|V370_OBJECT:_InitialPositions_|`,
> `sctafpar.cpp` — forces the holder to the player on both entries, so the
> hidden objects come back. Separately, `gs_create()` now *validates* an
> out-of-range parent instead of storing it and assert-crashing on the first
> turn update (`scgamest.cpp`; regression `harness/badparent_test.cpp` +
> `make_badparent_taf.py`, which is where The Timmy Reid Adventure and Blood
> Relatives are pinned).

## PARKED 2026-08-03 — the `downloaded/` wiring run stops here, at 161/161 PASS

> **Superseded 2026-08-04 — this park is over and every item below is struck.**
> The run resumed and finished: Ba'Roo!, Lair of the Vampire, The Fugitive and
> The Dead Man were all derived and won, and the five undownloaded games
> (Chosen, Crimson Detritus, ImagiDroids, Panic!, The Cellar) arrived and were
> wired. **`downloaded/` is fully wired — every walkthrough in it has a game
> and a row** — and the suite is at 203 PASS / 0 NOSCRIPT. Kept for the
> resume-order notes and the two habits at the end.

Clean stopping point: suite green (no FAIL / SKIP / NEEDGOLD / NOSCRIPT /
REGRESSIONS), every golden blessed against the current binary, **nothing
committed**. The two `scparser.cpp` fixes, the `sctasks.cpp` negative-object
guard, the `scdump.cpp` additions below and the `sctafpar.cpp` 3.8 size/weight
fixup are in the working tree only.

To resume, the remaining `downloaded/` candidates that already have a staged
`.taf` are, roughly easiest first:

* transcripts (the format that has replayed near-verbatim seven times running)
  — ~~`LockedDoorWithWaterTrap_transcript.txt`~~ (wired),
  ~~`Marooned_walkthrough.txt`~~ (wired, 80/140 — see below),
  ~~`Wrecked_walkthrough.txt`~~ (wired, **250/250** — see below),
  ~~`Mortality_walkthrough.txt`~~ (wired, verbatim — see above)
* command lists / sectioned prose — ~~`ThreeMonkeysOneCage_solution.txt`~~ →
  `3monkeys.taf` (wired, **98/100 = ceiling** — see above),
  ~~`LargoWinch_solution.txt`~~ (wired, **97/97** — see
  above), ~~`Humbug_walkthrough.sol`~~ → `humbug.taf` (wired,
  **2000/2000 = full score** — see above),
  ~~`CrimeAdventure_walkthrough.sol`~~ → `Crime_Adventure.taf` (wired,
  **95/95 = full score** — see above)
* prose only, needs real derivation — ~~`TheSisters_walkthrough.txt`~~ →
  `TheSisters.taf` (wired, **109/109 = full score** — see above),
  ~~`ThePKGirl_walkthrough.txt`~~ → `the_pk_girl.taf` (wired, **Katryn 55/60,
  Katryn's ending** — see above)
* not plain text — ~~`SecondChance_walkthrough.pdf`~~ → `second chance.taf`
  (wired, **WIN, good ending, verbatim** — see below),
  ~~`PrivateEye_guide.pdf`~~ → `Private Eye.taf` (wired, **WIN, score 4 =
  best ending, verbatim** — see below),
  ~~`ThePlagueRedux_walkthrough.doc`~~ → `The Plague - Redux.taf` (wired,
  **UNFINISHABLE as shipped** — see below)

~~Still parked from earlier: Ba'Roo! (needs real derivation), Lair of the
Vampire (desyncs badly), The Fugitive (prose only). `TheDeadMan_walkthrough.html`
has no `.taf`. Five games are still undownloaded: Chosen, Crimson Detritus,
Imagidroids, Panic, The Cellar.~~ *(All nine wired 2026-08-04 — see the banner
at the top of this section.)*

Two habits from this batch worth keeping: check `OBJNAME ... prefix=[...]`
before believing "I see no such thing", and treat a published *session
transcript* (as opposed to a hand-written command list) as Runner ground truth
— two of them caught real engine bugs today.

## 2026-08-03 — Marooned — **WIN, 80/140** — and the TAF 3.8 size/weight bug behind it — 158/158 PASS

`marooned.taf` is the first **TAF version 3.80** game in the corpus (`xxd -p -l
12 f.taf | cut -c17-22` → `944536`; `934536` = 4.0, `944537` = 3.9), and it did
not merely desync — it was *unfinishable*, because two objects the game hands
you could not be picked up:

```
>get tires
Your hands are full.
>get seal
The dead seal is too heavy for you to carry.
```

**The cause is a parser bug, not the game.** Version 3.8 stores a single
"Size/weight" **class index**, 0..4, per object. Every 3.8 grammar in
`sctafpar.cpp` read `#SizeWeight` raw and handed it to the 4.0 model, which
packs *size* in the tens digit and *weight* in the units and scales each as
`base^digit`. So class 4 ("very large") arrived as weight `3^4 = 81` against
Marooned's limit of `8 * 3^2 = 72`, and class 2 as `3^2`… wrong axis, wrong
magnitude, silently. New fixup `|V380_OBJECT:_SizeWeight_|` normalises every
3.8 object to 4.0 "normal" (22).

**Why normalise instead of converting the class.** ADRIFT Generator 3.90 does
convert it, and we know its exact table, because gen390 is available under Wine
and its conversion of `marooned.taf` diffs cleanly against our parse (new `sw=`
field on `SCR_DUMP_OBJLOC`, plus a new `PLAYERLIMITS` line):

```
3.8 class  ->  3.9/4.0 SizeWeight
0 normal   ->  22        3 large      -> 32
1 heavy    ->  23        4 very large -> 42
2 very heavy-> 24
```

That table is what ADRIFT itself does, and it is **wrong for playing the 3.8
file**: it breaks the games. On gen390's own conversion, the genuine
`run390.exe` refuses the tires with the same "Your hands are full." we used to
print. Crime Adventure (`Crime_Adventure.taf`, also 3.80, limit 45) goes the
same way — its kettle is class 2 → weight 81 — yet its published solution says
"Get all the stuff in Fenwick kitchen". Two independent 3.8 games, both
unwinnable for their own authors. The 3.8 header carries only **MaxCarried**, a
plain object count, and the two neighbouring 3.8 fixups already assume counts
(`Capacity*10+2`, `MaxCarried*10+2`) — those only mean "N objects" if every
object is normal-sized. Normalising makes the model self-consistent and
reproduces the author's own walkthrough line for line ("You pick up the couple
of tires."). Recorded as an ADRIFT-conversion bug, not ours.

~~Not provable to the last inch: **run380.exe cannot be obtained.**~~ adrift.co
serves `/files/run380.zip` and `/files/gen380.zip` with HTTP 200, but both
archives contain the **3.90** binaries; IF Archive and its mirrors, the Wayback
Machine (ftp.gmd.de, ftp.tardis.ed.ac.uk, the egroups files areas) and
archive.org have no copy. run390 refuses 3.80 files outright ("You will need to
convert it with ADRIFT Generator 3.90"), ~~so gen390 conversion is the only
ground-truth path available for 3.8 games~~.

> **Superseded the same day — `run380.exe` is not lost.** David Whyld's dead
> `delron.org.uk` still serves `adrift38.zip` through the Wayback Machine (and
> `adrift37.zip` beside it); both are installed in the adrift-battle Wine
> prefix, so 3.8 and 3.7 now have direct ground truth like 3.9 and 4.0 and
> nothing about them depends on a gen390 conversion any more. Measured against
> it, the conclusion above survives but the mechanism grew: version 3.8 has
> **one pooled burden**, per-class costs `1/3/7/3/7`, limit exactly
> `#MaxCarried` — a cost of 7 that 4.0's packed `base^digit` cannot express,
> which is precisely why gen390's conversion breaks the games. So the class is
> no longer merely discarded: the fixup keeps it verbatim in `SizeWeightClass`
> and `obj_get_burden()` (`scobjcts.cpp`) spends it, while `SizeWeight` stays
> normalised to `22` so container capacities remain the plain object counts 3.8
> means them to be. Marooned's route was re-derived under the real model — it
> can carry only one heavy object at a time and now **ferries**, 119 commands,
> same **80/140** win. Full record: the "TAF 3.8 object Size/weight class" row
> of `../../../RUNNER_TESTS_TODO.md` §4; game-level write-up:
> `Marooned_walkthrough.md`.

**The route: 80/140, and 80 is the ceiling.** The published walkthrough is for a
*different build* (its boat description differs from ours; our title bar says
"Marooned v1"), so this one is freshly derived from the task dump + the
`EXIT`/`ROOM` map. The remaining 60 points are structurally unreachable:

* **task 24 (get trash, 10)** — its two commands, `get *trash` / `take *trash`,
  are both already ALTCMDs of the repeatable task 14 in the only room the trash
  exists in, so 14 always wins the match.
* **task 27 (swallow pill, 10)** — requires the berries eaten, but the berries
  are the monkey's price for the flint, and the flint is mandatory to light the
  fire.
* **task 35 (shoot flare gun at shark, 10)** — requires holding object 20, the
  *unloaded* gun; loading it (task 19) destroys object 20 and creates object 37,
  and firing object 37 runs task 18, which kills you. Unreachable by
  construction.
* **tasks 9 + 28 (scratched can pour/light, 20)** — only the *dented* can's
  lighting task (10) starts event 5 "Rescue" (`startTask=11 affTask=30` → the
  `#win` task 29). Pour the scratched can too and task 9 wins the `light tires`
  match, the tires burn up, and the ship never comes. So the scratched can goes
  to the shark (task 33) instead — same 10 points, and the win survives.

One flourish worth keeping: at the lagoon, `throw <anything>` is stolen by the
non-repeatable, score-0 task 15 (`throw %object% *`). Burn it on the map you
picked up off the bridge, and the next throw reaches task 33 for its 10 points.

## 2026-08-03 (earlier) — The Amulet — **WIN**, verbatim, and the double-"Congratulations!" settled — 156/156 PASS

*The Amulet* (3-hour comp, Daniel Hiebert again) is a **verbatim** replay: all
12 commands from the author's transcript, no repairs, flavour lines (`notes`,
`spells`) included. The game has **no scoring whatsoever** — `score` answers
"0 out of a maximum of 0" and `SCR_DUMP_TASKS` finds zero `ACT type=4` — so
reaching the ending is the only measure of the route.

The one difference from the published transcript is that we print
**"Congratulations!" twice**, and it is worth writing down how that was
settled, because "our transcript has an extra line" normally means a bug.
`SCR_DUMP_TASKS` now prints a `WINTEXT [...]` line; all three of the games
wired today have an **empty** `WinText`, which is exactly the case where
`task_run_end_game_action()` falls back to its hard-coded "Congratulations!".
Shadrick's Travels also has an empty `WINTEXT`, its winning task text does not
contain the word, and its published transcript still shows one
"Congratulations!" — which is only possible if the real Runner prints the
default too. So in The Amulet the *first* one belongs to the winning task's own
CompleteText and the second is the engine's; the Runner would show both, and
the author trimmed the duplicate when writing the walkthrough up. Our
transcript is the faithful one.

## 2026-08-03 (earlier) — Monsters (Release 2) — **WIN, 40/40**, and two real parser bugs — 155/155 PASS

*Monsters (Release 2)* by Daniel Hiebert wins at **40/40**, which is the whole
game: `SCR_DUMP_TASKS` counts eight `ACT type=4` actions worth 5 each, and this
route fires all eight. The upstream file is a real-Runner session transcript
with no prompt glyph at all — commands and responses simply alternate — so the
38 commands were lifted by hand.

Exactly **one repair**: `open the bedroom door` → `open door`. The game has two
door objects, obj13 (the bedroom's own door, room 2) and obj48 (Mommy's, rooms
3 and 9); "bedroom door" collides and the parser answers "Open what?", leaving
`s` blocked on `RESTR type=1 v1=7 obj48`. The bare noun reaches the right one.

**The interesting part: this game's transcript is ground truth, and it caught
two genuine SCARE parser bugs.** Both are fixed, and neither moved any other
golden in the 154-row suite.

1. **`uip_match_optional()` did not rewind on a failed look-ahead.** It rewinds
   when the look-ahead *succeeds* and consumed text, but when the look-ahead
   fails it fell straight into `uip_match_alternatives()` at whatever position
   the failed attempt had already advanced to — and `uip_match_list()` has no
   backtracking of its own. Task 2's pattern is

   ```
   [defeat/shine/turn/put] {the} [flashlight/light] {on} {the} {brainsucker} {brain}  {monster}
   ```

   Against `shine flashlight on the brainsucker`, the look-ahead from
   `{brainsucker}` let `{brain}` eat the first five letters of "brainsucker"
   (`uip_match_word()` is a prefix compare with no word-boundary check), then
   died on the trailing "sucker"; the alternatives were then tried from
   "sucker", `{brainsucker}` matched nothing, and the pattern failed. Net
   effect: the command the author's transcript shows working was rejected and
   the game lost 5 points. `shine flashlight` (no object words) worked fine,
   which is what makes this kind of bug so easy to miss.

2. **`%object%` never answered to a partial prefix.** `uip_build_candidate()`
   composed exactly two strings, `"Prefix Short"` and the bare `Short`, so
   `examine the four poster bed` — Prefix `Sissy's four poster`, Short `bed` —
   was "I see no such thing", while the author's transcript prints the object's
   description. The candidate now also carries the prefix with its leading
   words dropped one at a time (`four poster bed`, `poster bed`, `bed`); the
   *name* is never cut down, so a two-word Short still has to be given whole.

   This one has **independent confirmation**: re-running the suite changed
   exactly one line in one other golden, Shadrick's Travels, where
   `climb oak tree` went from "You can't climb that." to "You can't climb the
   old oak tree." — and line 80 of *that* game's upstream transcript reads
   "You can't climb the old oak tree." Two unrelated real-Runner transcripts,
   same verdict.

`SCR_DUMP_TASKS`'s `OBJNAME` line now also prints `prefix=[...]` and every
`alias=[...]`, which is what turned "why doesn't this noun match?" from a
guess into a lookup. Use it first whenever a walkthrough line comes back "I see
no such thing".

## 2026-08-03 (earlier) — Shadrick's Travels — **WIN, 100/100**, verbatim — 154/154 PASS

A fifth verbatim replay, and the cheapest one yet: *Shadrick's Travels* by
Mystery replays **exactly as published**, 22 commands, no repairs, ending on
`Congratulations!` with the full 100 points.

The only trick is extraction. The upstream file is a session transcript whose
prompt glyph is a **CP1252 `Ø` (0xD8)**, not `>`; the commands are the lines
that start with that byte:

```python
d = open('downloaded/ShadricksTravels_walkthrough.txt','rb').read().decode('cp1252').replace('\r','')
cmds = [l[1:].strip() for l in d.split('\n') if l.startswith('Ø')]
```

`SCR_DUMP_TASKS` reports `TOTAL 100 4` — the game contains exactly four
`ACT type=4` scoring actions (`tie rope to wood` +20, `tie swing to tree` +20,
`throw rock at hive` +10, `swing on swing` +50) and this route fires all four,
so 100 is provably the maximum, not just a high score.

Three of the 22 commands are the author's own duds and are **kept on purpose**:
`x wood` and `climb tree` both land on the disambiguator, and `tire swing to
tree` is a typo for `tie`. ADRIFT's "Please be more clear…" does not consume
the following line, so a dud costs nothing but keeping it means the golden is a
faithful record of the published transcript. No env is needed — the game never
paginates, and the transcript is byte-identical with and without
`SCR_SKIP_WAITKEY=1`.

## 2026-08-03 (later, 8) — four verbatim delron replays, wired — 150/150 PASS

Four games whose delron command lists replay **without a single repair**, which
is worth recording because it is the exception, not the rule:

| Game | Cmds | Ending | Row env |
|---|---|---|---|
| Beanstalk the and Jack (David Welbourn, 2008) | 49 | `*** You have won ***` | none |
| Black Sheep's Gold (2004) | 99 | "You've beaten Black Sheep's Gold!" | `SCR_SKIP_WAITKEY=1` |
| Doomed Xycanthus (2006) | 82 | "Congratulations!" | none |
| Dancing Even Him? (Richard Otter, 2006) | 17 | anagram reveal | none |

Notes:

* *Beanstalk the and Jack* is a reverse-chronology retelling, so the command
  list reads backwards — it opens on `chop beanstalk` and ends with Jack waking
  up. That is the game, not a scrambled walkthrough.
* *Black Sheep's Gold* is the only one of the four that needs
  `SCR_SKIP_WAITKEY=1`: its epilogue stops on "(press any key to continue)" and
  without the skip the pause eats the trailing `quit`, so the ending never
  prints and the win marker looks absent.
* *Dancing Even Him?* — the title is an anagram of "Vending Machine", which the
  ending text spells out, so that line is the win marker.

The only work here was trimming the delron page footers ("Any donation would be
much appreciated", "Home | About Me") off the extracted text.

**Correction (2026-08-04):** that trim had in fact only been done for two of the
four. *Beanstalk the and Jack* (56 → 49) and *Doomed Xycanthus* (89 → 82) still
carried the seven footer lines as trailing commands. They were harmless — both
games end on an EndGame action (`sleep` and `jump` respectively) and SCARE stops
reading input at that point, so the junk was never consumed and the goldens are
byte-identical before and after — but the counts above were wrong and the files
now stop at the last real command. Both still PASS unchanged.

## 2026-08-03 (later, 7) — A Spot Of Bother — **WIN, 100/100**, wired — 146/146 PASS

*A Spot Of Bother* (David Whyld, 2005) reaches the author's own stated maximum,
and the upstream transcript needed exactly **one** repair in 270 commands: a
second `push door` in the gymnasium. The first push is a task that only reveals
the trap wire ("A wire, previously hidden, slips into view") — it does not open
the door, so after `break wire` the east exit still answers "You need to open
the door first." and `open door` answers "You can't open the door!". Pushing
again opens it and the remaining 197 commands replay verbatim.

The upstream file is a full session log including the title menu, so the first
two commands of the solution are the menu picks `2` (read the introduction) and
`1` (play). The row needs `SCR_SKIP_WAITKEY=1`: the game paginates heavily with
`[MORE]`, and without it every pause eats a command.

Everything else — including the `enter password elephant` crossword answer, the
three consecutive `3`s at the computer menu, and the long combination-lock
sequence on the safe — matched the upstream text response-for-response on the
first try, which is a good sign for the engine on a 2005-era 4.0 game with this
much task machinery.

## 2026-08-03 (later, 6) — Troll! — **WIN, 185/190**, wired — 145/145 PASS

*Troll!* (Peter Frøhlich-style pastiche, 2003 ADRIFT) is winnable, and the
route now reaches the ending with **zero** parser errors in 145 commands. Its
real ceiling is **185 of 190**, not 190: the game has 38 scoring tasks worth 5
points each, and one of them is structurally unreachable.

```
TASK 80 [* pay * barman *]    needs obj67 "fourtune", turns it into obj68
TASK 82 [* pay * landlord *]  needs obj67 "fourtune", turns it into obj68
TASK 81 [* pay * landlord *]  needs obj68 "fortune",  turns it into obj69
```

80 and 82 consume the same object, so at most one can fire — and 80 is
compulsory, because it is the task that summons the coach home (it moves
`obj21 coach` and `obj78 horse` and walks the player to room 3). 80 + 81 is
therefore the best pair available and **82's 5 points are dead**.

The published walkthrough scores 160/190. Everything else was recoverable:

| Recovered | Why the walkthrough missed it |
|---|---|
| `pay landlord` (TASK 81, +5) | it never pays the landlord at all — the `in` / `pay landlord` / `out` detour at Outside Inn on the way home is ours |
| `take breadcrumbs` (TASK 85, +5) | it says `get crumbs`, which the standard library resolves happily without firing the task |
| `w` upstairs **before** `unlock door` (TASK 86, +5) | TASK 86 is gated on TASK 64 `[* unlock * door *]` **not** being done, so the order matters: you score for walking into the locked door, *then* unlock it |
| `put breadcrumbs in basin` a **second** time (TASK 51, +5) | TASK 51 wants `obj48`, and `obj48` only reaches the player's hands as an *action* of TASK 52 (the firewater step). The first put is the library's |

Two of the walkthrough's commands are simply the wrong way round for the
task patterns, neither of which carries a reversed `ALTCMD`:

* TASK 61 is `[* show * barman * medallion *]` → `show barman medallion`
* TASK 83 is `[* give * chief * backward burp berries *]` → `give chief backward burp berries`

And two inventory hazards: the opening `get all` overflows the carry-weight
limit on a pink flyer, a green notice and a blue advert (dropped; five
explicit drops added instead), and the tavern drops must happen **on entry**,
not after `drink whiskey` — a timed event throws the player outside three
turns after the last drink, and the walkthrough's following `out` then fails.

The game ends inside `tickle frog` and never prints a final score, so the
golden's win marker is the closing line and the `score` just before it reads
180; the last task's 5 points land in the ending text.

## 2026-08-03 (later, 5) — The Hangover — **UNWINNABLE, max 5/7**, wired — 144/144 PASS

*The Hangover* (Red Conine, IFComp 2009 ADRIFT) is unwinnable as shipped, and
both dead ends are the author's. The two tasks the published walkthrough's
endgame turns on carry `Where/Type = 0` (`ROOMLIST_NO_ROOMS`), so they can
never run in **any** room:

```
TASK 10 where=0 room=-1 restr=1 cmd=[give the doctor some french fries]
TASK 14 where=0 room=-1 restr=3 cmd=[give approval notes to platypus]
```

Every other room-scoped task in the game is `where=1`. Confirmed against the
real run390.exe, which answers "You can't do that here!" to both. So the
ceiling is **5 of 7**: bill→fries (Deby), kick bum, mail→secretary, open the
filing cabinet, `type 119-228-337-446`. The two lost points are the doctor's
approval form and the ending itself.

Losing the doctor costs only the point — the Psycho Hospital's south exit is
*not* gated on his keys, so the route runs all the way through the jet, the
bathrobe parachute and back to the Form Process Office. The office even prints
"Except this time, the platypus is here. He seems happy. You give approval
notes to platypus." as room description while the task that would do it sits
permanently unreachable, which is a good demonstration that the author never
tested the ending.

Row (`the_hangover_solution.txt`, 53 commands, no `SCR_SKIP_WAITKEY` needed):
`the_hangover_solution.txt|hangover.taf|Your score is 5 out of a maximum of 7.`

Two derivation notes:

- **The room descriptions lie about exits.** Fedrick Avenue says "To your east
  is a bus stop"; the bus stop is **west**. The Approval Form Office says the
  first form is "through the door behind me to the north" and that one is
  right, but the walkthrough says south. The Psycho Hospital Room's exit is
  west, not east. Trust `You can't go in that direction, but you can move …`.
- The first command of the transcript is a dead `Take what?` — the walkthrough
  opens with "Get Up", which this build does not implement (`get` with no
  noun). It is left out of the solution file; the player starts standing.

### Follow-up: the task refusals — **IMPLEMENTED 2026-08-10**

Both run390.exe and run400.exe carry the string ` can't do that here!` (VB6
UTF-16; grep the .exe decoded as `utf-16-le`, plain `strings` misses it), and
run390 prints it for these two tasks. SCARE had no such message anywhere:
`task_can_run_task` simply returned FALSE for a room-gated task, and the command
fell through to the standard library — here to "Give what?".

The condition was unproven when this was first filed, so it was left unfixed.
It has since been probed live — a synthetic 3.90 game
(`harness/make_39_whereprobe.py`, tasks with every `Where/Type`, plus a
task-state restriction and a non-repeatable task, plus a one-turn always-
restarting event so a turn is visible) driven through run390 under Wine, with
run370/run380/run400 checked against real corpus games. Measured:

| Runner | out-of-room task command | `Where/Type = 0` task | nonsense word | library-handled command | task whose restriction fails silently | done non-repeatable task |
|---|---|---|---|---|---|---|
| run370 (castle.taf)   | "You can't do that here." | — | DontUnderstand | — | — | — |
| run380 (marooned.taf) | "You can't do that here." | — | DontUnderstand | — | — | — |
| run390 (probe, hangover) | "You can't do that here!" (types 1 **and** 2) | same refusal | DontUnderstand | library wins | DontUnderstand | "You have already done that." |
| run400 (4.0 probe)    | DontUnderstand | DontUnderstand | DontUnderstand | — | — | — |

So the condition is **narrow and purely about the room**: the task's command
pattern must match, and the *only* thing blocking the run must be the `Where`
room list. Restrictions are irrelevant — a task whose restriction fails
silently gets DontUnderstand, not the refusal — and so is anything the standard
library already handled. The message is pre-4.0 only; 4.0 dropped it and prints
DontUnderstand instead. Punctuation follows the period: 3.7/3.8 end in `.`,
3.9 in `!`. The leading word follows `Globals/Perspective` — "I" for
`LIB_FIRST_PERSON`, "You" otherwise (pre-4.0 has only the two; run390 answers
"You" for perspectives 1, 2 and 3 alike). And it **consumes a turn**: with a
one-turn ticking event running, `gamma` prints the refusal followed by the
event text, where a nonsense word prints DontUnderstand and no tick.

Implemented as `run_where_refusal()` in `scrunner.cpp`, last in
`run_all_commands()` after `run_standard_commands()`, using the new
`task_is_room_refused()` predicate (`sctasks.cpp`) — which is
`task_can_run_task_directional()` with the room half inverted, the two halves
having been split into `task_state_allows_run()` / `task_where_allows_run()`.
An empty input line returns early, the same guard the DontUnderstand fallback
uses: without it a game with a bare `*` wildcard task command outside the
player's room turns every press-a-key blank line into a refusal
(`archie_solution.txt` caught exactly that).

**The corpus did not move**: 203/203 PASS with zero re-blessing. A solved
walkthrough route never types a task command in a room the task cannot run in,
which is why the feature needed synthetic coverage of its own —
`make -f Makefile.headless wheretest` runs the three probe games (Perspective
1, 0 and 2) against `harness/where_refusal_expected.txt`,
`harness/where_refusal_1p_expected.txt` and
`harness/where_refusal_3p_expected.txt`, and is part of `make test`.

One gap, accepted:

- The 3.7/3.8 period wording is proved live (run370 *Castle Quest*, run380
  *Marooned*) and gated on `version < TAF_VERSION_390`, but has no synthetic
  regression — the probe generator writes 3.90 only, and the V380/V370 GLOBAL,
  ROOM, OBJECT and TASK schemas all differ enough to need a second generator.
  The 3.7/3.8 corpus rows (`castle_quest`, `alices_restaurant`, `marooned`,
  `twilight`, …) pass unchanged.
The sibling divergence in the last column above — pre-4.0 Runners answer a
completed non-repeatable task with **"You have already done that."**, also a
turn — was **implemented the same day**, on the same probe extended with two
more tasks (`eta`, carrying a RepeatText; `theta`, both done *and* out of its
room) and a matching 4.0 pair. Three things the extension settled: an authored
**RepeatText displaces** the message; that half is **not** pre-4.0 — run400
prints RepeatText too, and only the bare message was dropped; and when both
blockers apply the **room wins**, so the already-done test carries the room
condition. `run_where_refusal()` became `run_task_refusal()` and covers both.
Again zero corpus movement (203/203, nothing re-blessed) even though **62 of
196** corpus games author a non-repeatable task with a RepeatText (528 tasks —
`SCR_DUMP_TASKS=1`, new `rpt=` column): solved routes do not re-type completed
one-shot tasks. Full record in `RUNNER_TESTS_TODO.md` §4/§5.

The last finding from the same probe run — pre-4.0 has **only two
perspectives**, so run390 renders `Globals/Perspective` 1, 2 and 3 alike in the
second person where Scarier read 2 as third — was **implemented 2026-08-10**
too, as `lib_get_perspective()` in `sclibrar.cpp`. The probe is generated a
third time at Perspective 2 and the script types `i`, so the regression is that
`where_refusal_3p_expected.txt` stays byte-identical to
`where_refusal_expected.txt`. A census of the corpus (new `GAME version=
perspective=` dump line) found **no pre-4.0 game authoring Perspective 2** — the
three that do are all 4.0 and keep their third person — so again no walkthrough
golden moved. The **capacity probe** goldens did: both are authored
Perspective 2 at 3.9, and their 144 changed lines each move onto the run390
transcript recorded in `RUNNER_TESTS_TODO.md` ("You put the b1 inside the
c52t.").

## 2026-06-25: deaths (*Death's Door*) — **WON, full 100/100**

`deaths_walkthrough.md`; solution `goldens/deaths_solution.txt` (1st two lines =
name `Hero` + gender `male` — both start-up prompts). ADRIFT **3.90** dungeon
crawl ("free the house of death"): break into a 3-storey building, collect four
coloured keys, kill the **Dark force** in the attic. Was on the "untouched/boot
via name+gender prompt" list. **Not a hang — boots fine once name/gender are
fed** (same class as Theannihilationofthink2/CyberCow). Deterministic; **100/100
is the true max** (9 ChangeScore tasks sum exactly to 100, no orphans). Key
mechanics RE'd from the structural dump: **(1) one long key/door chain** — mail
box→silver-key→unlock Red kitchen→`kill jim`→gold-key→unlock Lit dining→`kill
ireen`→red-key→unlock Dark closet→`kill rooth`→crystal-key→unlock Living
room→Debbie/Beth/Ross (+45); **(2) the win stair (Third floor→Attic) opens via
`kill witch`** (task 23, a no-restriction scripted task that runs anywhere on the
Third floor — the witch needn't be present), and **`kill force`** in the Attic is
the type-6 EndGame win. **(3) Real Battle System: most enemies are harmless
(`str−def≤0`) but Rooth one-shots an unarmoured player on room entry** — the
intro's "upgrade your armor" is mandatory: buy `scale male` then `plate male` at
the Third-floor shop (money from the 500-gp kills + `sell silver-key` after using
it); plate male reduces incoming damage to 0, making Rooth AND the Attic boss
safe. The "kill *name*" tasks are scripted (restr=0) so the command itself kills;
the shop's other gear/special-attacks and the Driveway monsters
(Ghost/Wolfe/Cat/Vampire) are unscored flavour. Faithful to the 3.90 Runner; no
SCARE change needed. Verified 3× identical (100/100 + "crumbles into dust" win).
