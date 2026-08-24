# TODO: Runner-transcript verification of the v4 walkthroughs

The *Professor Von Witt* exercise, generalised. That game's author walkthrough
was replayed command-for-command in the real Windows Runner under Wine, the
Runner's own transcript was diffed against Scarier's, and three engine bugs
fell out of the diff (bare `pick` take-synonyms, room-alt `Var2` being a
1-based **global** object number, and the 1-2 vs 3+ surface-listing split) —
plus one non-bug that cost a session (see *Verbose* below). The write-up is
the comment block above the `professor_solution.txt` row in
`harness/run_v4_walkthroughs.sh`; the fixes are commit `6b61f2ab`.

Nothing in this file is done yet, apart from Professor itself. It is the
candidate list and the recipe.

**Scope: all four file versions.** The file started as a pre-4.0 list (3.90 /
3.80 / 3.70, 66 rows); the 4.00 pool — 124 further seed-invariant rows, listed
here since 2026-08-23 — is the larger half and is where Professor itself came
from, so it belongs in the same list under the same recipe. The only thing
that changes with the version is **which Runner binary to launch** (next
section but one) and **how the transcript is captured**; the seed-invariance
test, the Verbose/Appearance pre-flight and the diff discipline are identical.

## Why these games and not others

A walkthrough can only be diffed turn-for-turn against the Runner if the game
is *deterministic along that path*. The test, run 2026-08-23 over the whole
v4 harness:

    export SCR_SEED=97      && harness/run_v4_walkthroughs.sh
    export SCR_SEED=424242  && harness/run_v4_walkthroughs.sh

A row that still PASSes its seed-1 golden under **both** alternate seeds has
no visible randomness anywhere on its walkthrough path. One alternate seed is
not enough: the second seed killed nine rows that had survived the first by
luck. Result:

| | rows |
|---|---:|
| wired v4 walkthrough rows | 303 |
| carrying their own `SCR_SEED` / `SCR_ASSUME_COMBAT` (excluded) | 16 |
| seed-invariant under both seeds | **190** |
| — of those, 4.00 | 124 |
| — of those, 3.90 | 54 |
| — of those, 3.80 | 10 |
| — of those, 3.70 | 2 |

Re-run 2026-08-23 against the current 303-row harness: the four counts above
reproduce exactly, so no row has drifted in or out since the list was first
cut.

The 16 excluded rows are excluded because `$ROW_ENV` is applied *after* `env`
in `transcript()`, so a row that pins its own seed overrides the ambient
export and its PASS proves nothing.

Two caveats on the test itself:

- Seed-invariance is measured **in Scarier**. A randomness feature Scarier
  does not implement at all would be invisible to it. The Runner is the
  oracle, not the harness.
- It measures the *walkthrough path*, not the game. A game can be full of
  random events that the walkthrough happens never to trigger.

## Which Runner to launch

Match the Runner to the file's version — 3.9 and 4.0 differ in real
semantics (event task dispatch, take wording, division rounding). The version
is in the .taf header, bytes 8-10, the XOR-obfuscated version characters:

| bytes 8-10 | version | Runner |
|---|---|---|
| `93 45 3E` | 4.00 | `run400.exe` |
| `94 45 37` | 3.90 | `run390.exe` |
| `94 45 36` | 3.80 | `run380.exe` |
| `94 45 39` | 3.70 | `run370.exe` |

Do **not** use "is there a zlib `78 9c` at offset 22" as the test — that is
true only of 4.00 and silently classifies every older file as unknown/4.0
depending on which way the check is written. All four Runners live in
`~/adrift-battle/runner/wine/pfx/drive_c/adrift/`.

## Capturing a transcript

The Adventure menu splits at 3.90/3.80, and this was measured live, not
assumed:

**run390 (and run400) — "Start Transcript".** Live streaming to
`C:\adrift\Adrift_<N>.txt`, plain text, opened *before* play. Use the
existing helper unchanged, passing the Runner as `$3`:

    cd ~/adrift-battle/runner/wine
    sh runner_transcript.sh <game.taf> <cmdfile> run390.exe

⚠️ **For run400, use the `_safe` helpers instead.** run400 pops a modal
"Cannot play sounds" alert on games that carry sound (the prefix runs with
`mmdevapi=d` because Wine audio soft-locks the whole Mac), and that modal
**eats one Return** — the fed command it swallows is silently lost and every
later line lands one turn early, which reads exactly like an engine
divergence:

    sh runner_transcript_safe.sh <game.taf> <cmdfile> run400.exe
    sh drive_ckpt_safe.sh ...        # instead of drive_ckpt.sh

A startup sound alert can itself *be* the "cascaded window" a retry loop is
chasing, so dismiss it before concluding the window stack is wrong.

⚠️ **The transcript menu is dead until the game has begun.**  While the Runner
sits on a startup "press any key" pause it is in a modal key loop and ignores
the menu bar entirely: the click does nothing, no Save dialog appears, and
`measure.sh` reports "Save-transcript dialog never appeared".  Count the pauses
in the game's opening and pass that count as `measure.sh`'s 4th argument.
`humbug` has two -- `[Press any key]` after the ASCII-art title and `<MORE>`
after the credits -- so it needs `PRE=2`.  These are game text, so they are
visible in the golden; count them there.

⚠️ **Only ever click the menu bar** (window-relative y+43), and pick the item
by its accelerator (`t` for "Start &Transcript").  A click into the window body
that misses an open menu lands in the scrollback, and the Runner **copies the
clicked word into the command entry field** -- so a missed menu click does not
merely fail, it glues a stray word onto the next scripted command.  The menu
cannot be driven from the keyboard alone: Alt+A is swallowed by Wine, so the
top-level menu still needs that one click.  For the same reason
`drive_ckpt_safe.sh` no longer clicks to focus at all by default (fronting the
process is enough); its old hard-coded `CLICK_Y=825` was off the entry field,
which put every focus click into the scrollback.

⚠️ **Close every menu before driving.**  An open menu swallows the first typed
command *and its Return*, so the whole replay runs one turn behind the engine --
which in the diff is indistinguishable from an NPC-walk divergence.  On `humbug`
that cost a 50-minute run: the only visible symptom was Schrodinger the cat
arriving one command late, everywhere.  `drive_ckpt_safe.sh` now takes
`FIRSTCHECK=<transcript path>` and aborts if the first command never reaches the
game; `measure.sh` passes it.  Escape does not reliably close a Runner menu.

**run380 and run370 — "Save Transcript".** No live transcript at all: the
menu item dumps the whole scrollback *at the moment you click it* to
`C:\adrift\Adven_<N>.rtf` and pops a "Transcript saved" MsgBox (no Save-As
prompt; dismissed with key code 36). So the order is reversed — play first,
save last:

    sh runner_savetranscript.sh <game.taf> <cmdfile> run380.exe
    textutil -convert txt -stdout pfx/drive_c/adrift/Adven_1.rtf

The output is RTF with a colour table, which is a bonus: it preserves the
bold and the colours, so room headings and author styling survive.
`runner_savetranscript.sh` also carries run370's different click point — that
Runner's window is a fixed 559x498 and its entry field sits at window-relative
280,452, where the maximised 3.80/4.0 layout puts it at screen 400,825.

⚠️ `runner_transcript.sh` ends with `ls -t pfx/drive_c/adrift/Adrift_*.txt |
head -1`. Point it at a 3.80/3.70 game and it will happily print the path of
somebody else's hours-old transcript instead of failing — that is how the
"run370 can't save transcripts" wrong conclusion got made. Use
`runner_savetranscript.sh` for those two.

Unverified: whether the scrollback dump is capped for a long session. `cave`
(216 commands) is the one to check that on before trusting a 3.80 diff.

## Before measuring anything

- **Turn Verbose ON** (Options → Verbose, Ctrl+V). It resets to OFF on every
  launch and never persists. With it OFF, re-entering a visited room prints
  only `RoomName.` and NPC walker lines are *absent entirely*. Scarier models
  the Verbose-ON Runner, and author transcripts are Verbose-ON sessions.
  Measured in run400; assumed but **not yet verified** for run390/380/370 —
  check the Options menu on the first game of each version.
- **Check, do not assume, the Appearance checkboxes.** An earlier note here
  said "all five default OFF and never persist". Both halves are wrong, and
  the correction is sourced twice over (2026-08-24):
  * run400's options loader (`Proc_21_24_4747F8`, `run400.bas:89290`) reads
    each one through `Proc_21_25_44AC08(key, default)` -- args are pushed in
    reverse, so the byte pushed *before* the key string is the default. The
    defaults are `Myfont` 0, `Sound` 1, `Graphics` 1, **`showbrackets` 1**,
    `showgt` 0, **`showshortroom` 1**, `autopause` 1. So "Room names in
    descriptions" and "References in brackets" default **ON**, not off.
  * they *do* persist: this prefix's `pfx/user.reg` carries
    `[Software\\VB and VBA Program Settings\\ADRIFT\\Runner]` with
    `"showshortroom"="1"`, `"Graphics"="0"`, `"Sound"="1"`, `"Verbose"="False"`.
    (`run390`'s `m_showshortroom_Click` is a plain `SaveSetting`.) The old
    "nothing records it" reading was taken before anything had ever toggled
    the box, when the key simply did not exist yet.
  What this means in practice: room headings are **on** in this prefix, which
  is why `measure.sh` -- which only sends Ctrl+V for Verbose and never touches
  Appearance -- still matches Scarier's headings (FunHouse, 0/18 commands
  differ). Verbose is the only box that really does reset every launch.
  Read the key out of `user.reg` before a measurement rather than trusting
  either claim.
- **Look for randomised puzzle state before splicing a command file.**  The
  Runner rolls its own numbers, so any walkthrough that types a combination,
  a code or a count back at the game will break in the Runner even when the
  engines agree perfectly.  `humbug` is the worked example: it randomises a
  four-digit lock at game start and shows it on a slate as one roman numeral
  (`lock1` thousands ... `lock4` units).  Scarier at `SCR_SEED=1` rolls 3446,
  which is why the walkthrough says `Turn dial to 3/4/4/6`; run400 rolled 4937
  on the launch that mattered.  Fed the golden's digits, the Runner's case
  simply never opens and every later command runs against a different world.
  Grep the `.taf` for `%var%` inside object descriptions if you are unsure --
  humbug's slate reads
  `The numerals read [lock1=%lock1%][lock2=%lock2%][lock3=%lock3%][lock4=%lock4%].`
- **Fresh process per measurement.** Adventure → Restart game does not
  reliably reset NPC walk state.
- Feed with `drive_ckpt.sh`, which echo-verifies each line. Wine mangles
  input; read the echo, never assume the line landed.
- Rows marked **waitkey** below contain a "press any key" pause that swallows
  one fed keystroke. They are usable but need the feeder to account for it —
  prefer a non-waitkey game first.
- Real-time pauses (the Professor pie) only manifest live. They are not a
  divergence.
- Kill Wine afterwards, properly:
  `pkill -9 -f wine; pkill -f wineserver; pkill -9 -f 'C:\\'; pkill -9 -f 'start\.exe /exec'`
  then verify with `ps aux | grep -iE 'wine|\.exe'`. `pkill -f wine` alone
  matches nothing — Wine's Windows processes carry Windows command lines.

## Measured so far

Everything below was settled between 2026-08-02 and 2026-08-24.  The
walkthroughs themselves were never touched; where the Runner disagreed, the
engine changed and the golden was re-blessed, with the evidence written into
the row's comment block in `harness/run_v4_walkthroughs.sh`.

| game | version | how it was settled | outcome |
|---|---|---|---|
| `Professor.taf` | 4.00 | full run400 replay | the worked example; walk phase, arrival lines, presence lines |
| `FunHouse.taf` | 4.00 | full run400 replay, 0/18 commands differ | an **empty game-start walk preempts for ever**: NPC 3 WALK 1 and NPC 5 WALK 1 stay shut all game |
| `TheCatintheTree.taf` | 4.00 | full run400 replay | corroborates the same rule -- the boy (NPC 2 WALK 1) never arrives |
| `humbug.taf` | 4.00 | run400 P-code, room lister `Proc_19_63_472CA4`; then a two-phase replay that reaches command 373 of 1050 | ChangedDesc pick is task-state only, ascending, non-empty wins; the partial replay added the `On X is`, `and carrying` and pronoun-echo findings below.  **Not fully replayable** -- three randomised secrets, see "Still open" |
| `lair-of-the-cybercow.taf` | 3.90 | run390 P-code, viewroom `loc_447D1D` | same lister rule one Runner down; one line changes |
| `great.taf` | 3.80 | run380 P-code, `characters() '441928` | no expiry stamp at all, restart needs `Loop = 1`, preempt has no StoppingTask test |
| `maincourse`, `orient`, `xfiles`, `wamk` | 4.00 | re-blessed under the same two rules | `maincourse` lost its win marker to a faithful preemption |
| `iqsfot.taf` | 4.00 | see the row's comment block | NPC 16 WALK 2 is an empty game-start walk with no stops; it pins the patrol shut and the game cannot be won in run400 |
| `the_pk_girl.taf` | 4.00 | full run400 replay with a 96-command peddler hunt spliced in | the Runner WINS -- and that is what proved a finished 4.0 walk is stamped **-1**, not 255 |
| `arlo.taf` | 3.70 | full run370 replay, `Adven_6.rtf` | the 3.7 walk departure lines, incl. "walks off to not moved."; 3 differing of 85 |
| `tra.taf` | 3.80 | full run380 replay, `Adven_9.rtf` | "outside" takes no "to" in a departure line |
| `Melbourne Beach.taf` | 3.90 | full run390 replay, `Adrift_37.txt` | the 3.9 walk directions, incl. the diagonal a pre-4.0 8-exit scan cannot name |
| `Orient_Express.taf` | 4.00 | full run400 replay, `Adrift_36.txt` | the 4.0 walk directions; also the spurious "Gimme Atip enters." arrival |
| `S_Tar_Dus.taf` | 3.90 | full run390 replay, `Adrift_38.txt` | all 129 walk lines match count for count; pinned the not-a-room-zero arrival gate |

Three of these -- `xfiles`, `wamk` and `humbug` -- are **not measurable by
full replay**.  For `xfiles` and `wamk` the reason is RNG-timed event lines, so
the Runner's stream cannot be aligned against ours command for command; for
`humbug` it is randomised puzzle answers that the walkthrough hard-codes.  For
those, argue from the P-code and from a short targeted probe instead.
`the_pk_girl` looked like a fourth
until 2026-08-24, and it is worth knowing why it was not: what blocked it was
one randomly-placed NPC, and brute-forcing him out of the way (see
`cmdfile_pkhunt.txt`) made the whole game replayable.  Its transcript still
carries RNG-timed lines, so a command-for-command diff is noisy -- but the
*outcome* lines are not noisy at all, and the outcome was the whole question.

## Candidates

Sorted by NPC **walk** count first, then by length. Walks are the payload:
every Professor-class divergence found so far lived in walk phase, walk
arrival announcements, or walker presence lines. `walks`/`NPCs`/`events` come
from `SCR_DUMP_TASKS=1 harness/scare <game>`. `cmds` is the walkthrough
length. Solution files are `goldens/<solution>_solution.txt`.

The dump is one-shot and fires from the first task check, so it needs a turn
to be taken: `printf 'look\nquit\ny\n' | SCR_DUMP_TASKS=1 harness/scare
games/<game>` on stderr. Twelve of the 4.00 games open on a keypress-gated
intro that swallows that `look` and print nothing at all — feed them their own
solution file (with `SCR_SKIP_WAITKEY=1` where the row uses it) instead of
concluding the game has no tasks. Counts are `^NPC `, `^  WALK ` and `^EVENT `
lines.

### 4.00 — 124 games

Professor is in this table (marked **done**) so the exemplar sits next to its
peers. 122 distinct .taf files; `Sandy.taf` and `unravel.taf` each carry two
rows, and `sa.taf` / `sophie.taf` are the two releases of *Sophie's
Adventure*.

Shape of the pool: 29 rows author at least one walk, 88 author at least one
event, 59 need the waitkey allowance, and the lengths are strongly bimodal —
12 rows of 100+ commands against 52 of 20 or fewer. So there are two ways in:
a short row to calibrate the feeder cheaply, then a long walk-rich row for
the payload.

⚠️ The `walks` column counts **authored** walks, not walks the walkthrough
traverses, and at 4.00 that gap can be total: `To_Hell_And_Beyond` heads the
table on 19 walks but its row is a 3-command partial that reaches Oran and
stops, so it exercises essentially none of them. Read `walks` against `cmds`
before picking.

The four best targets, by walks x length:

- `goldilocks` — 252 commands, 8 walks, 10 events. The strongest row in the
  4.00 pool, and it strictly dominates Professor (86 / 2 / 4).
- `sophie` (`sa.taf`) — 255 commands, 7 walks, and **73 NPCs**, far more than
  anything else here; NPC presence lines are exactly where the Professor
  divergences lived. `sophie_comp` (`sophie.taf`) replays the comp release of
  the same game, so the pair also cross-checks a re-release.
- `cibass` — 40 commands, 8 walks, 8 events. Short enough to finish in one
  session at full walk density.
- `vardock_bates` — 103 commands, 2 walks, waitkey; the closest structural
  match to Professor, useful as a control.

| game | solution | cmds | walks | NPCs | events | waitkey | notes |
|---|---|---:|---:|---:|---:|---|---|
| `To_Hell_And_Beyond.taf` | `to_hell_and_beyond` | 3 | 19 | 41 | 7 | -- | [To_Hell_And_Beyond_walkthrough](To_Hell_And_Beyond_walkthrough.md) |
| `goldilocks.taf` | `goldilocks` | 252 | 8 | 6 | 10 | -- | [Goldilocks_walkthrough](Goldilocks_walkthrough.md) |
| `CIBASS.taf` | `cibass` | 40 | 8 | 2 | 8 | yes | [CIBASS_walkthrough](CIBASS_walkthrough.md) |
| `FunHouse.taf` | `funhouse` | 18 | 8 | 9 | 0 | -- | **done** 2026-08-24 -- see "Measured so far" |
| `sa.taf` | `sophie` | 255 | 7 | 73 | 13 | yes | [Sophies_Adventure_walkthrough](Sophies_Adventure_walkthrough.md) |
| `sophie.taf` | `sophie_comp` | 255 | 6 | 72 | 13 | yes | [Sophies_Adventure_walkthrough](Sophies_Adventure_walkthrough.md) |
| `Oh_Human.taf` | `ohhuman` | 9 | 6 | 3 | 5 | -- | -- |
| `TheCatintheTree.taf` | `the_cat_in_the_tree` | 8 | 5 | 4 | 1 | yes | **done** 2026-08-24 -- see "Measured so far" |
| `Monsters_r2.taf` | `monsters` | 38 | 3 | 3 | 4 | -- | -- |
| `The Angel the Devil and the Human.taf` | `angeldevilhuman` | 25 | 3 | 3 | 3 | -- | -- |
| `Through time.taf` | `through_time` | 18 | 3 | 10 | 3 | -- | [Through_time_walkthrough](Through_time_walkthrough.md) |
| `Vardock Bates.taf` | `vardock_bates` | 103 | 2 | 4 | 4 | yes | [Vardock_Bates_walkthrough](Vardock_Bates_walkthrough.md) |
| `Professor.taf` | `professor` | 86 | 2 | 9 | 4 | -- | **done** -- the worked example |
| `cyber2.taf` | `cyber2` | 29 | 2 | 8 | 1 | -- | [cyber2_walkthrough](cyber2_walkthrough.md) |
| `ADRIFTMaze.taf` | `adrift_maze` | 26 | 2 | 5 | 5 | -- | [ADRIFT_Maze_walkthrough](ADRIFT_Maze_walkthrough.md) |
| `cyber.taf` | `cyber` | 20 | 2 | 3 | 1 | -- | [Cyber_walkthrough](Cyber_walkthrough.md) |
| `DragonShrineR43.taf` | `dragonshrine` | 136 | 1 | 1 | 7 | yes | [The_Curse_of_DragonShrine_walkthrough](The_Curse_of_DragonShrine_walkthrough.md) |
| `BlackSheepsGold.taf` | `black_sheeps_gold` | 99 | 1 | 11 | 1 | yes | -- |
| `QuiATueDana.taf` | `qui_a_tue_dana` | 63 | 1 | 4 | 0 | yes | -- |
| `plunder_gargoyle.taf` | `plunder_gargoyle` | 43 | 1 | 3 | 4 | -- | [Pirates_Plunder_walkthrough](Pirates_Plunder_walkthrough.md) |
| `demonhunter.taf` | `demonhunter` | 40 | 1 | 2 | 2 | -- | [Apprentice_of_the_Demonhunter_walkthrough](Apprentice_of_the_Demonhunter_walkthrough.md) |
| `Invasion of the Second-Hand Shirts.taf` | `invasion_shirts` | 39 | 1 | 3 | 0 | -- | [Invasion_of_the_Second-Hand_Shirts_walkthrough](Invasion_of_the_Second-Hand_Shirts_walkthrough.md) |
| `Imagination.taf` | `imagination` | 35 | 1 | 1 | 0 | -- | [Just_My_Imagination_walkthrough](Just_My_Imagination_walkthrough.md) |
| `hyper_b_s.taf` | `hyper_b_s` | 34 | 1 | 2 | 1 | -- | [hyper_b_s_walkthrough](hyper_b_s_walkthrough.md) |
| `Renegade_Brainwave.taf` | `renegade_brainwave` | 25 | 1 | 5 | 3 | -- | [Renegade_Brainwave_walkthrough](Renegade_Brainwave_walkthrough.md) |
| `whitterscap.taf` | `whitterscap` | 21 | 1 | 3 | 4 | -- | -- |
| `All Hallows Eve.taf` | `allhallowseve` | 16 | 1 | 4 | 0 | yes | -- |
| `SRSintro.taf` | `srsintro` | 13 | 1 | 2 | 3 | -- | [SRSintro_walkthrough](SRSintro_walkthrough.md) |
| `competition2006__adrift__ptgood__PTGOOD.taf` | `ptgood` | 6 | 1 | 1 | 0 | -- | -- |
| `The Plague - Redux.taf` | `plague` | 266 | 0 | 10 | 20 | yes | [The_Plague_Redux_walkthrough](The_Plague_Redux_walkthrough.md) |
| `vetknow.taf` | `vetknow` | 228 | 0 | 15 | 38 | yes | [Veteran_Knowledge_walkthrough](Veteran_Knowledge_walkthrough.md) |
| `TheCellar.taf` | `cellar` | 176 | 0 | 1 | 1 | yes | [TheCellar_walkthrough](TheCellar_walkthrough.md) |
| `mysteryofcaves.taf` | `mysteryofcaves` | 146 | 0 | 6 | 1 | yes | [mysteryofcaves_walkthrough](mysteryofcaves_walkthrough.md) |
| `Space Boy's First Adventure.taf` | `space_boy` | 145 | 0 | 1 | 1 | -- | [Space_Boy_walkthrough](Space_Boy_walkthrough.md) |
| `vetknow2.taf` | `vetknow2` | 141 | 0 | 15 | 38 | yes | [Veteran_Knowledge_walkthrough](Veteran_Knowledge_walkthrough.md) |
| `shardsofmemory.taf` | `shardsofmemory` | 122 | 0 | 6 | 5 | yes | [Shards_of_Memory_walkthrough](Shards_of_Memory_walkthrough.md) |
| `man overboard.taf` | `man_overboard` | 99 | 0 | 5 | 0 | yes | [Man_Overboard_walkthrough](Man_Overboard_walkthrough.md) |
| `relojero.taf` | `relojero` | 88 | 0 | 0 | 2 | -- | [La_hija_del_relojero_walkthrough](La_hija_del_relojero_walkthrough.md) |
| `salutations.taf` | `salutations` | 88 | 0 | 3 | 2 | yes | [Salutations_walkthrough](Salutations_walkthrough.md) |
| `CBN.taf` | `cbn` | 82 | 0 | 1 | 0 | yes | [The_Revenge_Of_Clueless_Bob_Newbie_walkthrough](The_Revenge_Of_Clueless_Bob_Newbie_walkthrough.md) |
| `forum2.taf` | `forum2` | 82 | 0 | 1 | 0 | yes | [Forum_2_walkthrough](Forum_2_walkthrough.md) |
| `asdfa.taf` | `asdfa` | 80 | 0 | 4 | 0 | yes | [ASDFA_walkthrough](ASDFA_walkthrough.md) |
| `mortality.taf` | `mortality` | 78 | 0 | 4 | 5 | yes | [Mortality_walkthrough](Mortality_walkthrough.md) |
| `princess1.taf` | `princess_in_the_tower` | 78 | 0 | 4 | 1 | -- | [Princess_In_The_Tower_walkthrough](Princess_In_The_Tower_walkthrough.md) |
| `Private Eye.taf` | `private_eye` | 74 | 0 | 0 | 0 | yes | [Private_Eye_walkthrough](Private_Eye_walkthrough.md) |
| `AFDFR.taf` | `afdfr` | 73 | 0 | 32 | 17 | yes | [A_Fine_Day_For_Reaping_walkthrough](A_Fine_Day_For_Reaping_walkthrough.md) |
| `chooseyourown.taf` | `chooseyourown` | 72 | 0 | 0 | 0 | yes | [chooseyourown_walkthrough](chooseyourown_walkthrough.md) |
| `hauntedhouse.taf` | `hauntedhouse` | 72 | 0 | 4 | 1 | -- | [The_Haunted_House_of_Hideous_Horror_walkthrough](The_Haunted_House_of_Hideous_Horror_walkthrough.md) |
| `valley.taf` | `valley` | 72 | 0 | 6 | 0 | yes | [HappyValley_walkthrough](HappyValley_walkthrough.md) |
| `yak_shaving.taf` | `yak_shaving` | 71 | 0 | 5 | 3 | yes | [Yak_Shaving_walkthrough](Yak_Shaving_walkthrough.md) |
| `unravel.taf` | `unraveling_god_lou` | 70 | 0 | 4 | 10 | yes | -- |
| `unravel.taf` | `unraveling_god` | 70 | 0 | 4 | 10 | yes | -- |
| `lobster.taf` | `lobster` | 65 | 0 | 1 | 4 | -- | -- |
| `Tear.taf` | `Tear` | 62 | 0 | 0 | 3 | -- | [Tears_of_a_Tough_Man_walkthrough](Tears_of_a_Tough_Man_walkthrough.md) |
| `cbn2.taf` | `cbn2` | 60 | 0 | 2 | 0 | yes | [The_Revenge_Of_Clueless_Bob_Newbie_2_walkthrough](The_Revenge_Of_Clueless_Bob_Newbie_2_walkthrough.md) |
| `imagi.taf` | `imagidroids` | 60 | 0 | 0 | 7 | yes | [ImagiDroids_walkthrough](ImagiDroids_walkthrough.md) |
| `saffire.taf` | `saffire` | 58 | 0 | 0 | 1 | -- | [Saffire_walkthrough](Saffire_walkthrough.md) |
| `CD.taf` | `crimsondetritus` | 53 | 0 | 1 | 0 | yes | [CrimsonDetritus_walkthrough](CrimsonDetritus_walkthrough.md) |
| `exercise.taf` | `too_much_exercise` | 51 | 0 | 0 | 0 | -- | [Too_Much_Exercise_walkthrough](Too_Much_Exercise_walkthrough.md) |
| `marika.taf` | `marika` | 50 | 0 | 0 | 1 | yes | -- |
| `second chance.taf` | `second_chance` | 50 | 0 | 23 | 9 | yes | [Second_Chance_walkthrough](Second_Chance_walkthrough.md) |
| `Beanstalk.taf` | `beanstalk` | 49 | 0 | 3 | 1 | -- | -- |
| `goblinhunt.taf` | `goblinhunt` | 48 | 0 | 2 | 0 | yes | [Goblin_Hunt_walkthrough](Goblin_Hunt_walkthrough.md) |
| `shore.taf` | `shore` | 46 | 0 | 1 | 1 | -- | [The_Farthest_Shore_walkthrough](The_Farthest_Shore_walkthrough.md) |
| `chicken.taf` | `chicken` | 45 | 0 | 2 | 0 | -- | [The_Evil_Chicken_of_Doom_walkthrough](The_Evil_Chicken_of_Doom_walkthrough.md) |
| `buried.taf` | `buried_alive` | 43 | 0 | 1 | 1 | -- | [Buried_Alive_walkthrough](Buried_Alive_walkthrough.md) |
| `Percy.taf` | `percy` | 41 | 0 | 1 | 1 | -- | [The_Saga_of_Percy_the_Viking_walkthrough](The_Saga_of_Percy_the_Viking_walkthrough.md) |
| `marlin_affair.taf` | `marlin_affair` | 40 | 0 | 0 | 1 | yes | [Marlin_Affair_Prologue_walkthrough](Marlin_Affair_Prologue_walkthrough.md) |
| `microbe_willie.taf` | `microbe_willie` | 40 | 0 | 2 | 2 | -- | [Microbe_Willie_vs_The_Rat_walkthrough](Microbe_Willie_vs_The_Rat_walkthrough.md) |
| `pyramid.taf` | `pyramid` | 38 | 0 | 0 | 2 | yes | [The_Pyramid_of_Hamaratum_walkthrough](The_Pyramid_of_Hamaratum_walkthrough.md) |
| `Confession(1).taf` | `confession` | 37 | 0 | 1 | 3 | yes | [Confession_walkthrough](Confession_walkthrough.md) |
| `togetyou.taf` | `togetyou` | 34 | 0 | 1 | 8 | yes | [We_Are_Coming_To_Get_You_walkthrough](We_Are_Coming_To_Get_You_walkthrough.md) |
| `Griswold.taf` | `griswold` | 33 | 0 | 0 | 1 | yes | [Griswold_walkthrough](Griswold_walkthrough.md) |
| `endgame.taf` | `endgame` | 32 | 0 | 1 | 0 | -- | [The_Game_To_End_All_Games_walkthrough](The_Game_To_End_All_Games_walkthrough.md) |
| `frog.taf` | `frog` | 27 | 0 | 3 | 0 | -- | [The_Green_Princess_walkthrough](The_Green_Princess_walkthrough.md) |
| `SPAM.taf` | `spam` | 27 | 0 | 2 | 3 | yes | [SPAM_walkthrough](SPAM_walkthrough.md) |
| `I am the Law.taf` | `law` | 26 | 0 | 5 | 3 | yes | [IAmTheLaw_walkthrough](IAmTheLaw_walkthrough.md) |
| `topaz.taf` | `topaz` | 23 | 0 | 0 | 4 | yes | [Topaz_walkthrough](Topaz_walkthrough.md) |
| `Wreckage.taf` | `wreckage` | 23 | 0 | 0 | 2 | -- | [Wreckage_walkthrough](Wreckage_walkthrough.md) |
| `ARGH_sGreatEscape.taf` | `argh` | 22 | 0 | 0 | 1 | -- | [ARGHs_Great_Escape_walkthrough](ARGHs_Great_Escape_walkthrough.md) |
| `ShadricksTravels.taf` | `shadricks_travels` | 22 | 0 | 3 | 0 | -- | -- |
| `1HRGAME.taf` | `masochists_heaven` | 20 | 0 | 0 | 0 | -- | [Masochists_Heaven_walkthrough](Masochists_Heaven_walkthrough.md) |
| `Pieces of eden.taf` | `pieces_of_eden` | 20 | 0 | 1 | 3 | -- | [Pieces_of_eden_walkthrough](Pieces_of_eden_walkthrough.md) |
| `longbarrow.taf` | `longbarrow` | 19 | 0 | 0 | 2 | -- | -- |
| `Vagabond.taf` | `vagabond` | 19 | 0 | 3 | 2 | yes | [Vagabond_walkthrough](Vagabond_walkthrough.md) |
| `agent_4F[1].A.taf` | `agent4f` | 18 | 0 | 0 | 5 | -- | [Agent_4-F_from_Mars_walkthrough](Agent_4-F_from_Mars_walkthrough.md) |
| `dancingevenhim.taf` | `dancing_even_him` | 17 | 0 | 0 | 1 | yes | -- |
| `Undefined1.taf` | `undefined` | 17 | 0 | 0 | 0 | -- | [Undefined_walkthrough](Undefined_walkthrough.md) |
| `outline.taf` | `outline` | 16 | 0 | 0 | 0 | -- | -- |
| `Pilfers.taf` | `pilfers` | 16 | 0 | 0 | 1 | yes | -- |
| `QuestI.taf` | `questi` | 16 | 0 | 0 | 1 | -- | [QuestI_walkthrough](QuestI_walkthrough.md) |
| `raccoon.taf` | `raccoon` | 16 | 0 | 0 | 0 | yes | -- |
| `The_Stowaway.taf` | `stowaway` | 16 | 0 | 2 | 2 | -- | -- |
| `herrdoktor.taf` | `herrdoktor` | 15 | 0 | 0 | 1 | -- | -- |
| `InMemory.taf` | `inmemory` | 15 | 0 | 0 | 9 | yes | [InMemory_walkthrough](InMemory_walkthrough.md) |
| `MurderMansionntro.taf` | `murdermansionntro` | 15 | 0 | 0 | 0 | yes | -- |
| `Sandy.taf` | `sandy` | 15 | 0 | 0 | 0 | -- | -- |
| `shreddem.taf` | `shred_em` | 15 | 0 | 0 | 1 | -- | [Shred_Em_walkthrough](Shred_Em_walkthrough.md) |
| `rollingthedough.taf` | `rollingthedough` | 13 | 0 | 1 | 3 | yes | -- |
| `Witness_Demon_vs_Vampire.taf` | `witnessdemon` | 13 | 0 | 0 | 0 | yes | -- |
| `TheAmulet.taf` | `the_amulet` | 12 | 0 | 0 | 3 | -- | -- |
| `The Dangers of Driving at Night.taf` | `dangersdrivingnight` | 11 | 0 | 4 | 0 | yes | -- |
| `MammothVacuum.taf` | `mammoth` | 11 | 0 | 1 | 0 | yes | [MammothVacuumButtonOfDeath_walkthrough](MammothVacuumButtonOfDeath_walkthrough.md) |
| `headless.taf` | `headless` | 10 | 0 | 4 | 4 | yes | [TeenageHeadlessExperiment_walkthrough](TeenageHeadlessExperiment_walkthrough.md) |
| `Sandy.taf` | `sandy_meta_number` | 10 | 0 | 0 | 0 | -- | -- |
| `The_Shuffling_Room.taf` | `shufflingroom` | 10 | 0 | 0 | 8 | -- | -- |
| `smote.taf` | `smote` | 9 | 0 | 0 | 0 | -- | -- |
| `The Foggy Banana Adventure.taf` | `foggybanana` | 8 | 0 | 3 | 1 | -- | -- |
| `The Fly Human.taf` | `flyhuman` | 7 | 0 | 0 | 3 | -- | -- |
| `hungry.taf` | `hungry` | 7 | 0 | 2 | 1 | -- | -- |
| `zombiecow.taf` | `zombiecow` | 7 | 0 | 0 | 2 | yes | -- |
| `asteroid_after.taf` | `asteroidafter` | 6 | 0 | 11 | 3 | yes | -- |
| `door.taf` | `door` | 5 | 0 | 0 | 1 | -- | [Door_walkthrough](Door_walkthrough.md) |
| `Existence.taf` | `existence` | 5 | 0 | 1 | 1 | yes | -- |
| `Newton.taf` | `newton` | 5 | 0 | 0 | 1 | -- | -- |
| `Way Out.taf` | `wayout` | 5 | 0 | 0 | 0 | -- | -- |
| `zacksmackfoot.taf` | `zacksmackfoot` | 5 | 0 | 0 | 2 | yes | -- |
| `P2P.taf` | `p2p` | 4 | 0 | 0 | 4 | yes | -- |
| `hiker.taf` | `hiker` | 3 | 0 | 1 | 5 | -- | -- |
| `rift.taf` | `rift` | 3 | 0 | 0 | 1 | -- | -- |
| `Phoneb.taf` | `phoneb` | 2 | 0 | 0 | 0 | -- | -- |
| `ptbad.taf` | `ptbad` | 1 | 0 | 1 | 0 | -- | -- |
| `Cut_the_Red_Wire.taf` | `redwire` | 1 | 0 | 1 | 0 | yes | [CutTheRedWire_walkthrough](CutTheRedWire_walkthrough.md) |
| `The Vault.taf` | `vault` | 1 | 0 | 1 | 1 | -- | -- |

### 3.90 — 54 games

| game | solution | cmds | walks | NPCs | events | waitkey | notes |
|---|---|---:|---:|---:|---:|---|---|
| `Merry_Murders.taf` | `merry_murders` | 181 | 8 | 8 | 2 | yes | [Merry_Murders_walkthrough](Merry_Murders_walkthrough.md) |
| `Vampire.taf` | `vampire` | 205 | 7 | 11 | 11 | yes | [LairOfTheVampire_walkthrough](LairOfTheVampire_walkthrough.md) |
| `gamma.taf` | `gamma` | 315 | 4 | 10 | 0 | -- | -- |
| `S_Tar_Dus.taf` | `stardust` | 199 | 4 | 6 | 0 | -- | [S_Tar_Dus_T_walkthrough](S_Tar_Dus_T_walkthrough.md) |
| `wingman1.taf` | `wingman1` | 33 | 3 | 3 | 0 | -- | -- |
| `tcom.taf` | `tcom` | 13 | 3 | 1 | 0 | -- | [tcom_walkthrough](tcom_walkthrough.md) |
| `windy2.taf` | `windy2` | 200 | 2 | 8 | 1 | -- | -- |
| `Richard.taf` | `richard` | 189 | 2 | 5 | 13 | yes | [WhereIsRichard_walkthrough](WhereIsRichard_walkthrough.md) |
| `cleft.taf` | `cleft` | 115 | 1 | 2 | 1 | -- | [The_Cleft_in_the_Rock_walkthrough](The_Cleft_in_the_Rock_walkthrough.md) |
| `ECOD3.taf` | `ecod3` | 26 | 1 | 1 | 0 | -- | [ECOD3_walkthrough](ECOD3_walkthrough.md) |
| `BobBobsly.taf` | `bob_bobsly` | 25 | 1 | 2 | 0 | -- | [Bob_Bobsly_walkthrough](Bob_Bobsly_walkthrough.md) |
| `largo-winch.taf` | `largo_winch` | 323 | 0 | 42 | 22 | -- | [Largo_Winch_walkthrough](Largo_Winch_walkthrough.md) |
| `mudergreatfalls.taf` | `murder_great_falls` | 255 | 0 | 0 | 0 | yes | [Murder_in_Great_Falls_walkthrough](Murder_in_Great_Falls_walkthrough.md) |
| `report.taf` | `report` | 254 | 0 | 0 | 0 | -- | [Report_Espionage_walkthrough](Report_Espionage_walkthrough.md) |
| `Archie's Birthday V 1-2.taf` | `archie` | 240 | 0 | 8 | 0 | yes | [Archies_Birthday_walkthrough](Archies_Birthday_walkthrough.md) |
| `croft.taf` | `croft` | 193 | 0 | 4 | 1 | -- | -- |
| `DarkTower.taf` | `darktower` | 174 | 0 | 0 | 0 | -- | [The_Dark_Tower_walkthrough](The_Dark_Tower_walkthrough.md) |
| `FarFromHome.taf` | `farfromhome` | 167 | 0 | 0 | 0 | yes | [Far_From_Home_walkthrough](Far_From_Home_walkthrough.md) |
| `EnqueteAHautsRisques.taf` | `enquete_a_hauts_risques` | 145 | 0 | 13 | 7 | -- | -- |
| `Captive.taf` | `captive` | 141 | 0 | 2 | 19 | -- | [Captive_Universe_walkthrough](Captive_Universe_walkthrough.md) |
| `The Screen Savers On Planet X.taf` | `screen_savers` | 133 | 0 | 10 | 19 | -- | [The_Screen_Savers_On_Planet_X_walkthrough](The_Screen_Savers_On_Planet_X_walkthrough.md) |
| `thewoods.taf` | `thewoods` | 133 | 0 | 0 | 0 | yes | [The_Woods_Are_Dark_walkthrough](The_Woods_Are_Dark_walkthrough.md) |
| `Chosen.taf` | `chosen` | 123 | 0 | 0 | 0 | yes | [Chosen_walkthrough](Chosen_walkthrough.md) |
| `Renuntio.taf` | `renuntio` | 118 | 0 | 0 | 3 | yes | [Renuntio_walkthrough](Renuntio_walkthrough.md) |
| `as.taf` | `asylum` | 102 | 0 | 1 | 0 | yes | [Asylum_walkthrough](Asylum_walkthrough.md) |
| `A_Morning_with_a_Headache.taf` | `morning_headache` | 88 | 0 | 3 | 8 | -- | [A_Morning_with_a_Headache_walkthrough](A_Morning_with_a_Headache_walkthrough.md) |
| `sleaze.taf` | `sleaze` | 86 | 0 | 0 | 0 | -- | [Sleaze_City_walkthrough](Sleaze_City_walkthrough.md) |
| `Wheel105.taf` | `wheels_must_turn` | 77 | 0 | 4 | 15 | yes | [The_Wheels_Must_Turn_walkthrough](The_Wheels_Must_Turn_walkthrough.md) |
| `tq3.taf` | `tq3` | 76 | 0 | 2 | 4 | -- | [The_Quest_Moody_walkthrough](The_Quest_Moody_walkthrough.md) |
| `mhpquest.taf` | `mhpquest` | 68 | 0 | 2 | 0 | -- | [MHP_Quest_walkthrough](MHP_Quest_walkthrough.md) |
| `everything.taf` | `everything` | 68 | 0 | 0 | 0 | yes | [Everything_Emanuelle_walkthrough](Everything_Emanuelle_walkthrough.md) |
| `ECOD2.taf` | `ecod2` | 61 | 0 | 0 | 0 | yes | [ECOD2_walkthrough](ECOD2_walkthrough.md) |
| `chicago.taf` | `chicago` | 60 | 0 | 3 | 0 | -- | [Chicago_walkthrough](Chicago_walkthrough.md) |
| `hangover.taf` | `the_hangover` | 56 | 0 | 16 | 0 | -- | -- |
| `veteran.taf` | `veteran` | 47 | 0 | 3 | 0 | -- | [Veteran_Experience_walkthrough](Veteran_Experience_walkthrough.md) |
| `lostsouls.taf` | `lost_souls` | 47 | 0 | 0 | 0 | -- | [Lost_Souls_walkthrough](Lost_Souls_walkthrough.md) |
| `CRM.taf` | `crm` | 46 | 0 | 0 | 0 | -- | [That_Crazy_Radioactive_Monkey_walkthrough](That_Crazy_Radioactive_Monkey_walkthrough.md) |
| `Villains_And_Kings.taf` | `villains_and_kings` | 44 | 0 | 0 | 0 | -- | [Villains_And_Kings_walkthrough](Villains_And_Kings_walkthrough.md) |
| `DFU.taf` | `dfu` | 44 | 0 | 1 | 0 | -- | [Dance_Fever_USA_walkthrough](Dance_Fever_USA_walkthrough.md) |
| `impulso.taf` | `impulso` | 43 | 0 | 0 | 0 | -- | [Impulso_walkthrough](Impulso_walkthrough.md) |
| `Colony.taf` | `colony` | 40 | 0 | 3 | 3 | -- | [Colony_walkthrough](Colony_walkthrough.md) |
| `LOST.TAF` | `lost` | 38 | 0 | 3 | 11 | yes | [Albert_is_Lost_walkthrough](Albert_is_Lost_walkthrough.md) |
| `LOST.TAF` | `lost_down` | 38 | 0 | 3 | 11 | yes | -- |
| `amonkeytoomany.taf` | `amonkeytoomany` | 34 | 0 | 1 | 0 | -- | [A_Monkey_Too_Many_walkthrough](A_Monkey_Too_Many_walkthrough.md) |
| `Phoenix_Destiny.taf` | `phoenix_destiny` | 33 | 0 | 0 | 0 | -- | [Phoenix_Destiny_walkthrough](Phoenix_Destiny_walkthrough.md) |
| `CAH.taf` | `cruel` | 30 | 0 | 0 | 0 | -- | [Cruel_and_Hilarious_Punishment_walkthrough](Cruel_and_Hilarious_Punishment_walkthrough.md) |
| `forest.taf` | `forest_on_the_norm` | 27 | 0 | 4 | 0 | -- | [Forest_On_The_Norm_walkthrough](Forest_On_The_Norm_walkthrough.md) |
| `Locked_door_with_water_trap.taf` | `locked_door` | 21 | 0 | 0 | 0 | yes | -- |
| `Theannihilationofthink2.taf` | `think2` | 19 | 0 | 0 | 0 | -- | [Theannihilationofthink2_walkthrough](Theannihilationofthink2_walkthrough.md) |
| `lifesimulation.taf` | `lifesimulation` | 19 | 0 | 0 | 0 | -- | [lifesimulation_walkthrough](lifesimulation_walkthrough.md) |
| `Insane.taf` | `escape_from_insanity` | 16 | 0 | 0 | 0 | -- | [Escape_from_Insanity_walkthrough](Escape_from_Insanity_walkthrough.md) |
| `Toxically_Earth.taf` | `toxically_earth` | 11 | 0 | 17 | 0 | -- | [Toxically_Earth_walkthrough](Toxically_Earth_walkthrough.md) |
| `Dreams.taf` | `dreamland` | 10 | 0 | 0 | 1 | -- | [Dreamland_walkthrough](Dreamland_walkthrough.md) |
| `Matt's House.taf` | `matts_house` | 8 | 0 | 5 | 0 | -- | [Matts_House_walkthrough](Matts_House_walkthrough.md) |

### 3.80 — 10 games

| game | solution | cmds | walks | NPCs | events | waitkey | notes |
|---|---|---:|---:|---:|---:|---|---|
| `haunt.taf` | `haunt` | 88 | 5 | 4 | 3 | -- | -- |
| `superliam.taf` | `super_liam` | 86 | 5 | 11 | 0 | -- | -- |
| `cave.taf` | `cave` | 216 | 2 | 5 | 12 | -- | -- |
| `akron.taf` | `akron` | 44 | 2 | 4 | 0 | -- | -- |
| `jb2000.taf` | `james_bond` | 20 | 1 | 1 | 0 | -- | -- |
| `haunted.taf` | `haunted_house` | 116 | 0 | 0 | 2 | -- | -- |
| `Crime_Adventure.taf` | `crime_adventure` | 90 | 0 | 2 | 3 | -- | [Crime_Adventure_walkthrough](Crime_Adventure_walkthrough.md) |
| `first.taf` | `fistandantalus` | 18 | 0 | 1 | 0 | -- | -- |
| `duck.taf` | `duck_mccloud` | 13 | 0 | 0 | 0 | -- | -- |
| `microwaveman.taf` | `microwave_man` | 9 | 0 | 1 | 1 | -- | -- |

### 3.70 — 2 games

| game | solution | cmds | walks | NPCs | events | waitkey | notes |
|---|---|---:|---:|---:|---:|---|---|
| `arlo.taf` | `alices_restaurant` | 85 | 11 | 9 | 9 | -- | [ADRIFT_370](ADRIFT_370.md) |
| `castle.taf` | `castle_quest` | 17 | 0 | 1 | 0 | -- | [ADRIFT_370](ADRIFT_370.md) |

`arlo.taf` is the single best target in the pre-4.0 half: 11 walks in 85
commands, and 3.70 is the least-exercised parse schema in the engine. Across
the whole file `goldilocks` and `sophie` (4.00) are denser, but they test a
schema Professor has already been through — arlo tests one nothing has.

## What to do with a diff

Same discipline as Professor:

1. Rule out Verbose and the Appearance checkboxes first. Two of the three
   apparent Professor divergences were display settings.
2. Rule out the feed. Scroll the Runner to the session top and confirm the
   commands actually landed in order before believing an output difference.
3. Only then treat it as an engine bug — fix the engine, never the
   walkthrough, re-bless the golden, and record the measurement that
   justifies any deliberate deviation in the row's comment block in
   `harness/run_v4_walkthroughs.sh`.

## Open leads

Things a measurement turned up that are **not** walk bugs and have not been
chased yet.  Each needs its own investigation; none of them should be folded
into a walk-related change.

- **run400 refuses commands Scarier accepts** (found while replaying
  `The_X-Files_A_New_Beginning`, 4.00, 2026-08-23).  `take knife` and
  `take directions` get "Take what?" from run400 while Scarier takes the
  object; `burn memo` gets "I don't understand what you want me to do with
  The Memo."; `look at camera` gets "You see no such thing."; `knock` gets a
  task refusal.  These are task-matching / parser divergences and they are
  what makes that row unmeasurable by full replay.  Worth its own probe --
  the common shape is a multi-word object name matched on a later word.
  **Partly diagnosed 2026-08-24**: the two `Take what?` hits and the
  `look at camera` refusal are the object `seen` gate, not the noun matcher --
  see the parked-port section at the end of this file.  `burn memo` and
  `knock` are still unexplained and remain task-matching suspects.
- **Games whose transcripts carry RNG-timed lines** (`xfiles`, `wamk`) need a
  *targeted* Runner probe rather than a full replay.  There is no harness for
  that yet; the p4WK* probe .taf files in
  `~/adrift-battle/runner/wine/pfx/drive_c/adrift/` were built by hand in
  gen400 and there is no script that regenerates them.  Note that RNG-timed
  lines do not by themselves make a game unmeasurable -- see `the_pk_girl`
  below, where the diff is noisy but the *outcome* lines are not.
- **A dead NPC still walks in Scarier** (read out of run400 while chasing the
  PK Girl walk counters, 2026-08-24).  run400's walk ticker opens with
  `Proc_19_1_468DA0` @0004685B6: `If npc.Room = &HFB Then GoTo 468D61`, i.e.
  it skips *every* walk of an NPC whose room is 251.  251 is the battle
  system's "dead" marker (`Battles.bas` @00044B127, right after the
  " falls down, dead." line).  Scarier has no such marker: `battle_npc_die()`
  in `scbattle.cpp` puts the corpse in location 0, which is "Hidden", and
  `npc_tick_npc()` goes on ticking its walks -- so a walk can march a dead
  NPC back into play.  A faithful fix needs a *separate* dead flag, because
  run400 does keep ticking the walks of a merely hidden NPC (that is how a
  hidden walker comes back); reusing location 0 for both would break that.
  Only battle games can reach it, so it is parked rather than fixed here.
- **FIXED 2026-08-24: "On X is", never "On X are"** (measured on `humbug`,
  4.00, then confirmed in P-code for 3.90 as well).  run400 prints
  "On the triangular table **is** some swimming goggles, a watch, a musket and
  a china doll."  Scarier printed "are": `lib_list_on_object_normal()` and
  `lib_list_in_object_normal()` in `sclibrar.cpp` chose the verb with
  `lib_select_plurality (game, list[0], ...)` -- i.e. from the plurality of the
  *first listed item*.  The Runner does have an is/are helper
  (`isare`, `Proc_19_69_4507BC` @4507BC, a string heuristic on the article and
  the noun's last letter) and calls it for "Also here is/are", but these two
  listings do not: the verb is a literal, run400 @46A31F ("On ") and @46A7C7
  ("Inside "), run390 @443944.  3.70 and 3.80 have no such listing at all, so
  there is no version split.  Both sites now emit `" is "` unconditionally.
- **FIXED 2026-08-24: "... is wearing a hat, and carrying a document."**
  (measured on `humbug`, 4.00; P-code checked in all four Runners).  Scarier
  printed "and **is** carrying".  `lib_list_npc_inventory()` in `sclibrar.cpp`
  emitted `", and"` and then `" is carrying "` unconditionally; the Runner puts
  the `" is"` in the *subject* clause, not the verb clause, so it appears only
  when there is no preceding "wearing" clause:
      run400 @45B901   worn count > 0  ->  var_AA = 1, "  " & np & " is wearing "
      run400 @45BA5F   If var_AA = 1 Then  MemVar_4941B0 &= ", and" : GoTo 45BAA2
      run400 @45BA76   Else                MemVar_4941B0 &= "  " & np & " is"
      run400 @45BAA2                       MemVar_4941B0 &= " carrying "
  run390 is byte-for-byte the same shape (@382E1 `", and"`, @382FF `" is"`,
  @38311 `" carrying "`).  **3.80 differs** and repeats the whole subject:
  @2CA67 appends `", and "` and then falls through to its own `np & " is"`,
  giving "... is wearing a hat, and Grandad is carrying a document."  run370
  has no NPC worn/carried listing at all (its only `" wearing "`/`" carrying "`
  literals, @2B457/@2B5CD, are the player's own inventory).  So the fix is
  gated at `TAF_VERSION_390`.  Corpus movers: `humbug`, `vague`, `target`
  (all 4.00); no 3.80 golden exercises the lister, so that branch rests on the
  P-code alone.

- **FIXED 2026-08-24: the bracketed pronoun echo is gone** (measured on
  `humbug`, 4.00).  `Drop it` gets a bare "Okay.  I have dropped the paper
  aeroplane." from run400, where Scarier printed an italic
  `[Drop a paper aeroplane]` line first; same for `Read it`.  That echo was
  upstream SCARE's, for synonyms and pronouns alike; an earlier session had
  already removed the synonym half as noise "the Runner never prints" and kept
  the pronoun half on the argument that "it"/"her" are ambiguous and the echo
  is how the player learns what they bound to.  The Runner does not agree, and
  the other three generations back run400 up by string search: **run370 and
  run380 contain no `[` string literal at all, and run390's only one is the
  `[More]` pager** (run400's messages live in a table, so it cannot be checked
  that way -- hence the live measurement).  Removed from `scrunner.cpp`; the
  substitution itself still happens.  14 goldens re-blessed, 94 lines, every
  one of them a bracket line and nothing else: `adrift_maze`, `archie`,
  `cellar`, `cruel`, `humbug`, `iqsfot`, `man_overboard`, `provenance`,
  `shred_em`, `TheADRIFTProject`, `veteran`, `wrecked`, `yak_shaving`,
  `yonastoundingcastle`.  Corpus 303/303.
- **`the_pk_girl`'s second Detainment visit** (2026-08-24).  With an identical
  command stream the Runner prints "Laurie is standing here." where Scarier
  prints "Laurie is in your arms."  Laurie is in the player's arms in both --
  what differs is which alternate NPC description the room lister picks, so
  this is the *selector*, not the walk state, and it is a different question
  from the ChangedDesc pick that `donuts_intro`/`maincourse`/`orient_express`
  already pin down.
- **Timed events run a turn out of step** in `the_pk_girl` (2026-08-24), the
  same class already noted on `orient_express`.  Of the 470 replayed commands
  138 differ, and the great majority are an event line landing one command
  early or late.  Nothing about the walk work touches this; it wants its own
  measurement on a small event-heavy game.

## Where the walk work stands, 2026-08-24

The walk rewrite in `scnpcs.cpp` is finished and every claim in it is
live-measured in run400.  (Stale when written and corrected 2026-08-24: this
work *is* committed -- it is in the history up to `1622d8fc` -- and the corpus
is **303** rows, not 304.  As of 2026-08-24 it is 303/303 PASS.)

### The finding: `push &HFF 'Byte` is -1, not 255

VB Decompiler renders a one-byte immediate as `push &HFF 'Byte` and the operand
is **signed**.  P32Dasm shows the same instruction as `F4 LitI2_Byte: 255
(True)`, and VB's `True` is -1.  The unambiguous case sits in the walk ticker
itself: at `468805` the same opcode with the same operand is the `Step` of
`For var_BC = (NumStops - 1) To 0`, a loop that runs at all only if the step is
-1.  So the counter a finished non-looping 4.0 walk is stamped with at `46860B`
is **-1**, a sentinel that compares false against every `> 0` test in the
routine -- not a 255-turn countdown.

That is the whole reason 4.0 could drop the pre-4.0 "only looping walks
restart" test from its restart branch: a spent walk holds itself shut on -1
instead.  Read as 255 it becomes a 256-turn cycle whose walks sit above zero
almost permanently, and because the precedence scan runs over the
*higher-numbered* walks, a handful of spent ones pin every lower walk shut
forever.  Written up in `~/Adrift_decompile/README.md` and in the
`run400 468DA0 npc_walk_tick` row of `~/Adrift_decompile/index/annotations.tsv`.

### Measured, not argued

- `funhouse`: 18/18 commands identical under Wine.  Pins the precedence rule --
  run400 lets a higher-numbered walk with `StartTask 0` shut a lower one down
  with no counter test at all (`Proc_19_1_468DA0` @4686FD-468747), even with no
  stops to walk -- and the task-state (not counter) test in
  `lib_get_npc_inroom_text()`.
- `the_pk_girl`: full replay under run400, and **the Runner wins** --
  `Congratulations! You got Katryn's ending.` / `Your Secret Letter is: E`, with
  24 "Laurie follows you".  This is what pins the sentinel.  A prior reading of
  the P-code had concluded the opposite (that Laurie's spent walks preempt her
  follow walk for good and the game cannot be won in run400); the replay
  disproved it, and the only way to make Scarier agree was -1.  The engine fix
  cut this game's golden diff from 512 lines to 30 and restored the ending.
- `donuts_intro`, `the_cat_in_the_tree`, `maincourse`, `orient_express`: pin the
  room lister's task-state ChangedDesc pick.
- `iqsfot` re-derived (185 -> 178 commands) and re-blessed.  Attribution
  confirmed by construction: suppress the empty walk's precedence and the
  pre-fix route reproduces the pre-fix golden byte for byte.
- `humbug` is the only other corpus row the sentinel change moves: two hunks at
  golden lines 6089/6095 (`Grandad stands nearby.` / `Grandad walks to the
  south.` disappear), at command 844 of 1050.  Under Wine now.

### How `the_pk_girl` was made replayable

Its blocker was never the RNG-timed event lines, it was one NPC.  NPC 26 [the
umbrella peddler] has one walk whose three stops are all the same *room group*
(`dest=119`), so which plaza room he is in is a fresh draw each arrival, and the
walkthrough must meet him ("give money to peddler" / "ask peddler about valley")
to unlock the `j) Wautomec Valley` motorcycle destination.  The first replay
stranded at the Plaza with 90 commands to go.  The fix was brute force:
`cmdfile_pkhunt.txt` (504 lines) splices a 96-command sweep of the plaza rooms,
retrying the meeting in each, in after feed index 308.  Run it with

    cd ~/adrift-battle/runner/wine && sh measure.sh pkgirl.taf cmdfile_pkhunt.txt run400.exe 0

The lesson generalises: a randomly-placed NPC is not an unmeasurable game, it is
a search, and the search is cheap compared to arguing from P-code and getting it
backwards.

### How `humbug` was made replayable

Same shape as `the_pk_girl`, different obstacle: a randomised combination
lock rather than a randomly-placed NPC, and the answer is a **two-phase
drive** rather than a brute-force sweep.  `measure.sh` leaves the Runner
running when its command file is exhausted, so a second file can be driven
into the same live process with `drive_ckpt_safe.sh` directly:

    # phase A -- everything up to the first dial (solution lines 1..165)
    sed -n '1,165p' goldens/humbug_solution.txt > ~/adrift-battle/runner/wine/cmdfile_hb_A.txt
    cd ~/adrift-battle/runner/wine && sh measure.sh humbug.taf cmdfile_hb_A.txt run400.exe 2

    # read the Runner's own slate out of the transcript and rewrite the dials
    python3 <scratch>/hb_partb.py pfx/drive_c/adrift/Adrift_30.txt \
        <repo>/goldens/humbug_solution.txt cmdfile_hb_B.txt

    # phase B -- into the SAME pid, no relaunch
    FIRSTCHECK=pfx/drive_c/adrift/Adrift_30.txt sh drive_ckpt_safe.sh <pid> cmdfile_hb_B.txt

`hb_partb.py` parses the roman numeral after `The numerals read`, zero-pads it
to four digits and rewrites the four `Turn dial to N` lines in walkthrough
order (Entrance Hall, East Alcove, South Alcove, North Alcove).  Always pass
`FIRSTCHECK` on the second drive: nothing has verified the process is at a
prompt, and an unnoticed dropped first command puts the whole phase a turn out
of step.

### Still open

- Scarier synthesizes an `NPCWalkAlert` task pair (`sctasks.cpp:1844-1873` ->
  `npc_start_npc_walk()`) for which run400 has no counterpart; in practice it
  only anticipates the ticker's own restart branch by a tick, so nothing in the
  corpus depends on it.  Unresolved, not urgent.
- Scarier has no equivalent of run400's dead-NPC walk gate; see the open lead
  above.
- **`humbug` is not measurable by full replay** -- it joins `xfiles` and `wamk`.
  The two-phase splice below gets the dial combination right, but the game
  randomises *three* secrets, not one, and the other two are unreachable the
  same way: the magic word (command 209 `Read runes`, "Jisanajen" here vs
  "Tedikebat" in the Runner) and the keypad code (command 344 `Read wall`,
  "HEL3761" vs "HEL1594").  The keypad is the hard break: at command 373
  `push button 7` the Runner answers only "Beep.  The liquid crystal display
  flickers." with no "The metal door to my west slides open.", so command 376
  `W` fails and the streams part for good.  A three-phase splice (dials, then
  magic word, then keypad) would work and costs about an hour of Wine
  wall-clock; nobody has run it.  Everything the replay *did* reach was worth
  having -- both wording fixes above came out of commands 800/2285 -- but the
  two `NPC_WALK_EXPIRED` lines at golden 6089/6096 sit past the break and stay
  unmeasured here.  They are measured on `the_pk_girl` instead.
  Also: the Grandad absence at command 843 that looked like a confirmation is
  **not** one.  Runner and Scarier Grandad lines agree only through command
  328; the Runner has none after that, and at command 583 `Blow trombone` it
  answers "But I am not carrying the trombone.", so his pub sequence never
  fired and his walk was never started.  The absence proves nothing.
- **Three RNG-independent `humbug` divergences, found but not chased.**  All
  three are inside the replayed prefix, so they are real and re-measurable
  cheaply:
  * command 217 `Put sweet on plinth` -- run400 prints "Okay.  Okay.  I put the
    sweet on the plinth." (the "Okay." is *doubled*), Scarier prints one.
  * command 254 `W` -- Scarier prints "(Getting off the stool first)", run400
    prints nothing.
  * command 321 `X Grandad` -- this was the "and is carrying" bug, now FIXED
    (see Open leads).
- Next candidates down the list, in order: `arlo.taf` (3.70, 11 walks / 85
  commands -- the best pre-4.0 target, and it shows up twice in the killer-walk
  scan), then `goldilocks`, `cibass`, `sophie`/`sa.taf`.

## PARKED 2026-08-24 -- pre-3.9 wording rules, round one done

The pre-3.9 round is finished and committed; the suite is **304/304 PASS**
with all nineteen pre-3.9 rows re-blessed.  The five rules, their P-code
evidence and the retracted empty-prefix inference are written up in the
comment block above the `akron_solution.txt` row in
`harness/run_v4_walkthroughs.sh`, which is the place to read them.  What is
left here is what is still *open*.

### Measured this round (Wine, three new replays)

| game | .taf | Runner | transcript | state |
|---|---|---|---|---|
| arlo.taf | 3.70 | run370 | `Adven_5.rtf` (Verbose off), `Adven_6.rtf` (Verbose on) | 6 differing of 84, all NPC-walk payload |
| akron.taf | 3.80 | run380 | `Adven_7.rtf` | **0 differing / 44** |
| mikes.taf | 3.80 | run380 | `Adven_8.rtf` | 5 differing of 103, all downstream of one desync |

cmdfiles are `~/adrift-battle/runner/wine/cmdfile_{arlo,akron,mikes}.txt`.
akron is the first pre-3.9 game to match the real Runner byte for byte.

### Open leads

- **The walk move and the meet-task dispatch sit outside the exact-tick
  test.**  run400 `loc_468841` is `If (counter = suffix_sum) And (suffix_sum
  > 0)` and it branches false straight to `loc_468D51`, which is the walk-step
  loop's `Next` -- so the *entire* step, the move included, happens only on
  the exact tick.  run390 has the identical shape (loop `loc_45A741`, gate
  `loc_45A780`, `End If` `loc_45ABC7`, `Next` `loc_45ABC2`, the `&HFF` hide
  stamp at `loc_45ABB8` inside it), so this is not a 4.0 rewrite.
  `npc_tick_npc_walk()` now gates both announcements on `is_exact` but still
  runs `if (start != dest) { move }` and the CharTask dispatch
  unconditionally, and still forces `is_arrival` true for the Hidden and
  roomgroup cases.  **Live exposure:** `provenance`'s butler is displaced by a
  task and Scarier warps him back on a non-exact tick, which is where its
  three now-deleted bare "The butler exits." lines came from.  The
  announcements are right now; the move timing is not.

  **Corpus cost, measured 2026-08-24 (engine experiment, not blessed):**
  gating both the move and the meet tasks on `is_exact` costs 10 rows
  (shadowpeak x3, `the_town_of_azra_v390`, `thetest_win`, `ticket`,
  `great_escape`, `textident_evil`, `merry_murders`, `provenance`); gating
  only the move costs 9 -- the same list minus `ticket`.  So the meet-task
  half is what breaks the "Ticket to No Where" roomgroup canary and the move
  half is the substantive change.  `merry_murders` does not merely re-word:
  it stops being winnable on its present script ("Trey's not here!"), so that
  row would need re-deriving, not re-blessing.

  **The live probe that was meant to settle it FAILED, and its result must
  not be quoted.**  `Merry_Murders.taf` (3.90) was driven through run390 with
  a 23-command probe ending in `look`s in the Hallway and the Plaza
  (`~/adrift-battle/runner/wine/mm2.cmds`, transcript
  `pfx/drive_c/adrift/Adrift_40.txt`).  The Runner desynced at command 4:
  the game's "Act II" cutscene warps the player to the Plaza, and the `w`
  typed straight afterwards never echoed at all -- the transcript goes
  cutscene, blank, "I don't understand what you mean!", so the keystroke was
  swallowed while the Runner was still painting the cutscene.  Every later
  command therefore landed in a different room from Scarier's, and in
  particular task 9 (`n` / `open door`, `Where.Room` 11) never ran there.
  That task is the one whose `NPCWalkAlert` `[0,2 2,0 5,0]` starts Nancy's
  walk 2, Mary's walk 0 and Trey's walk 0, so the Runner's "Trey is still in
  the Hallway, Mary is still in the Plaza" says nothing about walk timing --
  it only says the walks were never started.  **The lead is still
  unmeasured.**  Next time pick a probe game whose opening has no long
  cutscene, and check every command echoed before reading anything.

  Two things *were* nailed down from the decompiles on the way, and they
  cost nothing to keep.  (Read run390 from the cooked `run390/run390.bas`,
  not `Project2/Form1.frm` -- the stack-machine listing is unreadable next to
  it.)  The 3.9 walk struct is `(0)` StartTask, `(4)` Loop, `(8)` stops
  (`.global_2` = Times), `(12)` NumStops, `(13)` counter, `(18)`
  StoppingTask.  First, `loc_45A71E` gates the whole step loop on `enabled
  And counter > 0`, and the only reseed in the ticker is `loc_45A5A7`,
  `((counter < 0) Or (counter = 0 And Loop = 1)) And enabled` -> sum of Times
  (the `counter < 0` disjunct is dead: it is a Byte).  Scarier's
  `npc_tick_npc()` already has both.  Second, the seeding really is
  `1 + total`: the task-completion handler at `loc_43F095` walks every NPC
  and every walk and, `If walk.StartTask - 1 = taskno`, sets `counter = 1`
  and then adds each stop's Times -- which is exactly
  `npc_start_npc_walk()`.  Note the Runner finds the walk by **scanning
  every walk's StartTask**, where `task_start_npc_walks()` reads the task's
  own `NPCWalkAlert` list; the two agree on this corpus, but a game where
  they disagree would show it here first.

  When this is finally measured, check it against pre-4.0 too, plus the
  "Ticket to No Where" roomgroup case, which is the evidence that a Times>1
  roomgroup stop *does* re-run every tick.
- **arlo, `get out of bus` at the church** (cmds 37 and 64): run370 ends with
  the task's "You are no longer in the bus." and prints no exits list;
  scarier prints the exits and drops the task line.  **Diagnosed 2026-08-24,
  deliberately not ported** -- see "DIAGNOSED ... the run370 double matcher
  pass" below.
- **mikes replay desync**, for anyone re-running it: cmd 27 `take truck keys`
  hits a disambiguation prompt ("Which keys. The mustang keys or the truck
  keys?") that scarier resolves silently, and everything from cmd 53 on is a
  consequence.  Only commands before 27 are evidence.  **Diagnosed 2026-08-24,
  not ported -- blocked on one live 4.00 command**; see "DIAGNOSED ... the
  Runner's co() object-ambiguity test" below.
- **Humbug via SAVE points** (user's suggestion, untried): checkpoint the
  replay with the Runner's own `save`/`restore` so the three randomised
  secrets -- dial combination, magic word at cmd 209, keypad code at cmd 344
  -- can each be read out of the Runner's transcript and spliced in without
  re-driving 1050 commands after a desync.
- **Two logged-but-unchased humbug divergences:** cmd 217 `Put sweet on
  plinth` (run400 doubles "Okay."), cmd 254 `W` (scarier adds "(Getting off
  the stool first)").
- **Next candidates** down the list: `goldilocks`, `cibass`, `sophie`/`sa.taf`.
  With the pre-3.9 pool now clean, the remaining 3.90 and 4.00 candidates are
  where the next divergences will come from.

## DIAGNOSED 2026-08-24 -- the Runner's co() object-ambiguity test (mikes)

The second pre-3.9 divergence, and the only one left in the pre-3.9 pool.
run380 answers mikes cmd 27 `take truck keys` with

    Which keys.  The mustang keys or the truck keys?

and does not take them (`Adven_8.rtf` line 207).  Scarier binds the truck
keys silently, which is where the replay desyncs.

### What the Runner does

Our `%object%` matcher is positional: `uip_match_entity()` walks the pattern
and only considers a candidate that starts where the pattern's `%object%`
starts, so `take truck keys` can only ever bind the truck keys.

The Runner has no positional matcher at all.  `co(obnum)` -- run380 @42DE60,
run370 @4261B4, run390 `co(obnum, mode)` @43B6BC, one routine with the same
shape in all three -- does this instead:

1. Scan the **whole typed command** for the object's Short name; failing
   that, for its Alias.  The scanner is `c(search)` (run380 @429048): a
   case-insensitive `InStr` whose hit must begin at the string start or just
   after a space, and must end at the string end, a space, or a comma.  (No
   retry on a failed trailing boundary -- only a failed *leading* boundary
   loops, @42902F.  And `c("")` is False, because the zero-length hit fails
   every trailing test.)
2. Call whichever of the two matched `term`.
3. Count every object **present** (`obhere`) whose Short **or** Alias is
   *exactly* `term` -- string equality, not containment.
4. If that count is > 1, flag the command ambiguous by stamping the object's
   number into `MemVar_44F124` (@42DDC7).  That flag is read at the very end
   of the turn (@4431B0) and **replaces the whole turn's output** with
   `"Which " & term & ".  " & list & "?"` (@443303; the sibling @4432AA uses
   the Short name instead when the player typed it).  The list is built at
   @42DC1E from every present object with that term, `tense(Prefix) & " " &
   Short`, joined with ", " and " or ".
5. One escape hatch: if the player *also* typed the last word of that
   object's own Prefix -- "take **silver** key" -- @42DD4C stamps the
   resolved marker `&HFE` instead.  That marker outranks any ambiguity raised
   by any other object in the same scan, because @42DDC1 only writes an
   object number when the marker is not already set, while @42DD51 writes
   `&HFE` unconditionally.  So one resolvable object suppresses the prompt
   for the whole command.

mikes has no Prefix on either object -- Short "mustang keys"/Alias "keys" and
Short "truck keys"/Alias "keys", both Prefix "" -- so nothing rescues them.
At cmd 27 both are present (the mustang keys were taken at cmd 8 and are
carried), the mustang-keys object matches on its alias "keys", two present
objects answer to "keys", and the Runner asks.  Cmd 8 `take mustang keys` is
*not* ambiguous only because the truck keys were not yet in scope, which is
the transcript's own control.

### The measurement harness

`SCR_TRACE_CO` was added to `sclibrar.cpp` for this and is worth keeping.  It
reproduces `co()` from the player's raw command (via the new
`run_get_dispatch_input()`) at each `lib_disambiguate_object_common()` call
and prints

    CO-AMBIG verb=[take] input=[take truck keys] term=[keys] present=2 ours=1

whenever the Runner's test fires; `ours=` is our own post-filter reference
count, so `ours=1` is a real divergence and `ours>1` means only the *wording*
of the prompt differs.  It changes no behaviour.  Sweep the corpus with

    while IFS='|' read -r sol taf marker envs; do
      env $envs SCR_TRACE_CO=1 harness/scare "games/$taf" < "goldens/$sol" \
        2>&1 >/dev/null | grep '^CO-AMBIG'
    done < <(grep -E '^[A-Za-z0-9_.-]+\.txt\|' harness/run_v4_walkthroughs.sh)

It is a *lower* bound: a command claimed by a task never reaches the library
disambiguator at all, and the Runner's flag can be set from handlers we do
not model.

### Corpus exposure, measured 2026-08-24

**31 commands in 14 games.**  19 of them are commands our own disambiguator
already calls ambiguous (`ours>1`), so only the prompt's wording differs
there -- and that wording is wrong too: no Runner anywhere contains the
string "Please be more clear" (which is SCARE's invention), and only four
lines in the whole corpus print it (`cybercow` x3, `light_up` x1).  The three
Runners spell it "Which <term> would you like to take/drop/examine.  <list>?"
from the take/drop/examine handlers (run380 @43DE41/@438A8B/@43D2F6) and
"Which <term>.  <list>?" from everywhere else.

Of the remaining 12 genuine divergences, **eleven are in 4.00 games**:

| game | version | commands |
|---|---|---|
| mikes | 3.80 | `take truck keys` |
| mysteryofcaves | 4.00 | `get scammin's ring`, `wear scammin's ring` |
| A_Spot_of_Bother | 4.00 | `get metal bar` |
| ShadricksTravels | 4.00 | `x wood` |
| Witness_Demon_vs_Vampire | 4.00 | `get red bottle` |
| easter | 4.00 | `put egg/eggs/chicks in basket` |
| asteroid_after | 4.00 | `open/close {first..fifth} valve` x6 |

### Why it is not ported

4.00 is exactly the generation where the rule is *not* established.  Objects
carry `[1]$Alias` -- one alias, full stop -- in 3.7, 3.8 and 3.9, which is why
`co()` can read a single struct field 8; a 4.00 object carries `V$Alias`, a
list (see `V400_...` vs `V390_...` in `sctafpar.cpp`).  If a 4.00 object's
*every* alias were a `co()` term, `open second valve` would be ambiguous in
asteroid_after -- six valves, each aliased "valve", each with Prefix "the" so
the escape hatch never fires -- and the game would be unplayable.  That is
good evidence run400 does something else (narrowing on the longest match is
the obvious candidate).  It cannot be read off the decompile: run400 keeps
its messages in a table, so neither `run400.bas` nor `run400.p32dasm.txt`
resolves the "Which " literal, although the literal really is in the binary
(UTF-16LE at file offset 0x17a2c in run400.exe, and likewise at 0x9568 /
0xb270 / 0xee08 in run370/run380/run390).

At 3.7/3.8/3.9, where the rule *is* established, mikes cmd 27 is the corpus'
only divergence -- one row, whose walkthrough would then need re-deriving
(drop the mustang keys before taking the truck keys, presumably; there is no
adjective that would disambiguate).  Porting on that alone would mean
inventing a 4.00 rule.

### The one command that unblocks it

Run `asteroid_after.taf` in run400 under Wine and type `open second valve`.

* A "Which valve." prompt means the 4.00 rule is the same one and the port is
  a single code path (and asteroid_after's walkthrough is wrong).
* A normal answer means 4.00 narrows by the longest matching name, and the
  port has to be version-split -- in which case also check
  `get scammin's ring` in mysteryofcaves, whose two rings share the alias
  "ring" and differ only in their Short names, to see which way it narrows.

Also worth measuring while the Runner is up: the disambiguation *wording*,
since ours matches no Runner at all, and whether the Runner disambiguates
**NPCs** -- there is no "which character" message anywhere in run370/run380/
run390, so scarier's "Please be more clear, who do you want to attack?"
(3 lines in `cybercow`'s golden) may have no counterpart at all.

## DIAGNOSED 2026-08-24 -- the run370 double matcher pass (arlo)

The one remaining pre-3.9 divergence, read out of the run370 p-code and
matched line for line against `Adven_10.rtf`.  **Understood in full, and
deliberately not ported** -- see "Why it is not ported" at the end.

### What the transcript shows

    > get out of bus                                     (arlo, at the church)
    You're on foot.  You are in front of a gothic wooden church. ...
    There is a mailbox here.  You are no longer in the bus.

One RTF paragraph, one `\par`.  Scarier instead prints

    You're on foot.
    You are in front of a gothic wooden church. ... There is a mailbox here.

    You can move north and east.

so two things differ at once: the Runner *loses the exits sentence* and
*gains a second task's CompleteText*.  Both come from one mechanism.

### The mechanism

`generaltasks` runs, in this order (run370 @0003B935 onwards):

    takes()  drops()  inventory()  tasks(0)  wears()  removes()
    ... insides() sitstand() openclose() ... moves(playerroom) ... examines()

`takes()` is entered whenever the command contains `get`, `take`, `pick` or
the game's own take-verb **and does not contain `from`** (@00035D8C-35E28).
Its object scan then walks every object and, on the *first* one whose Short
name or an Alias appears in the command, calls the task matcher with
**mode 1 and the player's original command still in place** and `Exit For`
(@00036CA2-36CB5).  arlo's object 29 is `microbus` with the alias **`bus`**,
so `get out of bus` reaches that call.

The crucial bit: **`takes()` never stores a return value.**  Its p-code opens
with `ZeroRetValVar` and there is no store to the result slot anywhere in the
body -- the `takes = MemVar_4460E4` that VB Decompiler prints at
`loc_436D17` is its rendering of the `ExitProcCbHresult` opcode, not a
statement.  So `takes()` always returns Empty, `CBoolVarNull` is False, and
`generaltasks` **falls through to `tasks(0)` anyway**.  The matcher therefore
runs a second time on the same command, this time in **mode 0**.

Mode is the whole story for the buffer (@00041B90):

    If mode = 1 Then  buffer = <buffer as at this call's entry> & CompleteText
    Else              buffer = CompleteText            ' CLOBBER

So in arlo:

1. `takes()` -> matcher(mode 1) -> **task 72** (`get out of *bus*`,
   Where = room 7 = the bus, Repeatable) completes.  Buffer becomes
   "You're on foot.", then its `ShowRoomDesc = 1` runs `viewroom(room 0)`.
2. `viewroom`'s exits block (@000330FF) sets `command = "exits"`, calls
   `moves(broom)` -- which *replaces* the buffer with the exits sentence --
   and then, because the answer is not "... any direction!", **prints
   everything accumulated so far immediately and with no newline**
   (`Sub_3_27(saved & "  ")`, @0003315C) and leaves the buffer holding
   **only** "You can move north and east."
3. Task 72's Movements then put the player in room 0.
4. `tasks(0)` runs the matcher again.  Task 72's Where gate now fails (the
   player is no longer in the bus); **task 107** -- same five patterns,
   Where = room 0, CompleteText "You are no longer in the bus." -- matches
   instead, and mode 0 **clobbers** the buffer.  The exits sentence is
   destroyed before it is ever flushed.
5. The turn's final flush prints the clobbered buffer plus a newline.

Net screen text: the accumulated prefix (already printed in step 2) followed
by task 107's line, in one paragraph, with no exits.  Exactly the transcript.

Every other `get out of bus` in the same transcript corroborates it:

* at the Dump and at the Parking area the second pass finds nothing (task 107
  needs room 0), so only the first task's text survives -- and the exits are
  still missing there for the *other* reason, `viewroom` running before the
  Movements;
* `take garbage` (task 95, structurally identical to task 72, SRD room 0,
  Movements to room 0) keeps "You can move north and east." because nothing
  matches on the second pass;
* probing `get out of bus` while already on foot at the church prints
  "You are no longer in the bus." alone.

### Why it is not ported

Scarier's turn is "matcher once, then the library".  Reproducing this would
mean running the matcher a second time for every take-family command and
giving the second run clobber-the-buffer semantics that scarier's filter has
no equivalent for.  The preconditions are narrow -- a `get`/`take`/`pick`
command with no `from`, naming an object, matching a **repeatable** task whose
own effects then make a **different** task match -- and arlo is the only game
in a 303-row corpus that hits them.  The golden keeps scarier's single-pass
output; the row in `harness/run_v4_walkthroughs.sh` carries the measurement.

## REFERENCE -- run370 facts established while chasing arlo

Recorded here because two of them reverse working models earlier sessions
were built on.

- **The .bas decompile silently DROPS statements.**  Proven twice above and
  once more below.  `run370.bas` has neither the `var_A4 = 0` at
  `00040B7A` nor the whole `If var_A4 = 0 Then` gate at `00040BE5`, and it
  invents a `takes = MemVar_4460E4` where the p-code has only `ExitProc`.
  It also mis-attributes `Left()`/`Right()`/`InStr()` arguments and prints
  `For i = 0 To 0` where the real limit is a variable.  **Always confirm
  against `run370.p32dasm.txt`** (addresses there are VA - 0x400000; use
  `LC_ALL=C`, and find line numbers with `grep -n '^0004...'` rather than
  `sed -n '/^ADDR:/,/^ADDR:/p'`, which silently matches nothing).
- **One ordinary task per matcher call -- `var_A4` is the latch.**  Zeroed
  once *before* the outer task loop (`00040B7A`), tested at the top of every
  pattern iteration (`00040BE5`, `BranchF 00040ED7` when set), set to 1 the
  instant any task completes (`00041DE7`).  The single exception is the
  landing site itself: a `&&` ("always") pattern with mode 0 and an empty
  entry buffer still matches at `00040ED7`, so `&&` tasks are not latched
  out.  arlo has none.  **This retires the "exhaustive task loop" model**
  that earlier sessions inferred from the .bas, and with it the parked
  multi-task patch -- scarier's existing one-task-per-call behaviour is
  correct.  The two CompleteTexts in arlo come from two *calls*, not one.
- **Room-number offsets.**  `playerroom` is 1-based (scarier room `r` is
  `r+1`), and the rooms array `MemVar_446008` is indexed by that 1-based
  value, while `roombitmap` is indexed by the scarier index.  In a task,
  **`ShowRoomDesc` = scarier room + 1** and a Movement's **`Var2` = scarier
  room + 3**; a Movement moves the player when `Var1 = 1 And Var2 > 1`.
- **`viewroom`'s exits block** (`000330FF`) is gated by a game-header byte,
  `MemVar_44613D`, read at load (`0003F313`) -- game-wide "show exits", not a
  per-call flag.  When on, it stashes the buffer, calls `moves(broom)` (which
  *overwrites* the buffer with the exits sentence), then either restores the
  stash and prints nothing at all (answer ends "any direction!") or prints
  the stash immediately without a newline and leaves the exits sentence in
  the buffer for the turn's final flush.  Note the consequence: after a
  successful `viewroom`, the buffer contains **only** the exits sentence.
- **`moves()` counts the exits of `broom`, not of the player's room**
  (`00033F01`), and counts an exit when its task gate is 0 or when
  `tasks(gate-1).done = 1 - flag`.
- **`tasks(0)` returns the endgame flag**, so `generaltasks` normally *falls
  through* it to `wears`/`removes`/`insides`/`sitstand`/`openclose`/
  `moves(playerroom)`/`examines` before the `characters()` + `events()` tail
  -- those are not alternative branches.
- **`checktask(text)` is a pure predicate** ("would a task matching this text
  pass its restrictions").  It never executes a task.
- **Only ten call sites reach the matcher**: `characters()` x2 (CharTask,
  ObjectTask), `generaltasks` x1 (**the only mode-0 call**), `takes()` x3,
  `drops()` x3, `events()` x1.  Eight of them substitute
  `tasks(N-1).Command[0]` for the command first; the two that do not are
  `takes()` `@00036CAD` and `drops()` `@00030D38`, which re-match the
  player's original words.
- **Retraction: `break *garbage*` does match `break garbage with implement of
  destruction`.**  An earlier session used that command as a single-task
  probe; it never was one.

## CLOSED 2026-08-24 -- `%in_<obj>%` / `%on_<obj>%` listing format

`scvars.cpp` printed the contents of a container or surface named by
`%in_X%` / `%on_X%` / `%onin_X%` in the postfixed form unconditionally --
"A tub of butter, a butter knife and a bottle of milk are inside the fridge."
The library listers in `sclibrar.cpp` have selected the format **by content
count** since the 3.9 wording round, because run400 lists container and
surface contents from a single routine at 0006A418 that counts first and then
branches: `0006A49E` (count == 1) -> "`<obj>` is inside `<cont>`.";
`0006A607` (count == 2) -> "`<a>` and `<b>` are inside `<cont>`.";
`0006A786` otherwise -> "Inside `<cont>` is `<list>`."  Before
TAF_VERSION_390 only the prefixed form exists.  The variables took a
different path and missed all of it.

Measured in run400's `Adrift_23.txt` (WhereAreMyKeys.taf, 4.00):

```
open fridge
You open the fridge and the light comes on.  Well that's something. Inside
the fridge is a tub of butter, a butter knife and a bottle of milk.
```

-- task CompleteText is `"...  Well that's something. %in_fridge%"`, three
objects inside, so the prefixed form.  The two-object control is in the same
transcript and keeps the postfixed form:

```
open unit
A large knife and a jar of coffee are inside the kitchen unit.
```

so this is the count selector, not a blanket rewording.  Fixed with
`var_use_alternate_format()` in `scvars.cpp`, shared by all three variables.
Corpus exposure measured: 18 games use any of the three, `%onin_%` only
`WhereAreMyKeys` and `door`.  Exactly one golden line pair moved corpus-wide;
**303/303 PASS**.

The nested case is deliberately left alone: when an object is both *on* and
*in* the associate, run400 reaches the same lister with `var_9E == 1` and
prints a prefixed ", and inside is `<list>`", which scarier does not model.
No corpus row and no saved replay exercises it.

Still open from the same replay: run400 **lower-cases object state names**
("switched off", "switch in the on position") where we print the `States`
pipe-list verbatim.  Only first-character evidence exists, so `LCase` over
the whole string cannot be told apart from lowering the first letter, and the
corpus has states where the difference is destructive -- `in the UP position`
(TheADRIFTProject), `facing South` (The_Hunter), `Sur la gauche` (Les Feux de
l'enfer), `R1..R7` (Oh_Human), `Locked off` (baroo).  No saved transcript
covers any of them; `%state_` / `%obstate` are 4.00-only (UTF-16LE at
run400.exe 0x1d1cc and 0x1d108, absent from run370/380/390) and run400's
game-logic literals live in a runtime table that neither `run400.p32dasm.txt`
nor `run400-analysed/Form1.frm` resolves.  If it is ever measured, the single
place to change is `obj_state_name()` in `scobjcts.cpp`: its three callers
(`lib_list_object_state`, `%obstate%`, `%state_X%`) are all print sites and
nothing compares state names.

## CLOSED 2026-08-24 -- the not-a-room-zero arrival gate

The residual left open by the walk-announcement round below, closed the same
day.  20 goldens across 20 games re-blessed, corpus **303/303 PASS**.  Full
write-up in the comment block above the `stardust` row in
`harness/run_v4_walkthroughs.sh`.

3.8/3.9/4.0 gate a walker's arrival line on its **pre-move** location not
being the Runner's never-placed zero (run400 @468A64 is the whole test:
ShowEnterExit AND `old <> playerroom` AND `old <> 0`; run380 @4416F4 and
run390 `loc_45A99B` the same shape).  **3.7 has no such test** (run370
@43955E) -- and indeed no 3.7 row moved.

The Runner spells "not a room" two ways and only one suppresses: `0` for an
NPC the game never placed, `&HFF` for one a walk's Hidden stop just hid
(run400 `loc_468D4A`), the latter still printing a directionless arrival.
Scarier stored both as location 0, so `scr_npcstate_t` gained a `walk_hidden`
flag -- cleared by `gs_set_npc_location()`, set by `npc_tick_npc_walk()` right
after a Hidden stamp.  Deliberately **not** in the `.tas` stream: the Runner's
own save writes a room byte in `0..NumRooms`, so a restored hidden walker
reads back as never-placed at either engine.

Measured live at three generations (3.7 exempt, so none needed):

| Game | Runner | Transcript | What it showed |
|---|---|---|---|
| `tra.taf` | run380 | `Adven_9.rtf` | no "Sting walks towards you." -- but "Canadian couple walks towards you from the north." *is* there, so the gate is the zero, not the walk |
| `S_Tar_Dus.taf` | run390 | `Adrift_38.txt` | full 117-command replay: **all 129 walk lines match count for count** across four walkers and six directions, and the bare "Plant Lady prances along." is absent from the Runner while its four directional siblings are in both |
| `Orient_Express.taf` | run400 | `Adrift_36.txt` | "Gimme Atip enters." is printed by Scarier and by no Runner -- the divergence that started the item |

Everything removed is a first-ever arrival of a never-placed NPC, one to three
lines per game, and **no game lost a directional arrival** -- a useful shape
check if this is ever revisited.

## CLOSED 2026-08-24 -- NPC walk announcements, all four generations

`scnpcs.cpp`'s walk departure/arrival lines rewritten against the Runner's own
`wherefrom()` (run370 @422F8C, run380 @42800C, run390 @430200, run400
`Proc_19_20` @45234C -- one routine, unchanged across all four).  Twenty
goldens across nineteen games re-blessed, corpus back to **303/303 PASS**.
The canonical write-up is the comment block above the `alices_restaurant` row
in `harness/run_v4_walkthroughs.sh`; the short version:

- The direction names *the other room as seen from the player's*: scan the
  other room's exits for the player's room and print that exit's **opposite**,
  **last match winning** (there is no early break).  SCARE scanned the
  player's room forward and took the first match, which agrees only on a
  symmetric map.
- `EightPointCompass` is never consulted.  Pre-4.0 scans exits 0..7, 4.0 scans
  0..11, so a diagonal move is nameless before 4.0.
- Departure suppression is version-split: **3.7** suppresses only "nowhere"
  (so "Alice walks off to not moved." really does print, twice, in arlo);
  **3.8**/**3.9** suppress both; **4.0** suppresses only "not moved" and
  prints a bare "X walks off." on "nowhere".
- `"outside"` loses the "to" everywhere: "walks off outside." (run380
  @0004160D, run390 `loc_45A840`, run400 `loc_46891E`).
- A **follow-player stop is announced by no Runner**; a **hidden stop** gets a
  directionless, resource-less line in **3.7 and 4.0 only** (run370
  `loc_4397A3`, run400 `loc_468CF9` -- run380 `loc_4418DD` and run390 have
  nothing there but the "stamp the NPC nowhere" assignment).
- Both lines fire only on the exact tick, so a multi-turn stay is announced
  once.

Measured live under Wine, one game per generation:

| Game | Runner | Transcript | What it pinned |
|---|---|---|---|
| `arlo.taf` | run370 | `Adven_6.rtf` | all three 3.7 departure lines; arlo down to **3 differing of 85** |
| `tra.taf` | run380 | `Adven_9.rtf` | "Hovey shuffles off outside." -- no "to"; the old golden was wrong |
| `Melbourne Beach.taf` | run390 | `Adrift_37.txt` | all four changed sites, incl. the dropped diagonal ("David strolls in.") |
| `Orient_Express.taf` | run400 | `Adrift_36.txt` | every 4.0 walk line, incl. "...walks off to the west." and "...wobbles in from the east." |

At 3.9 only the NPC *verb text* ever differed from the Runner (the walk's
random alternate texts), never the direction.

Two residual items came out of this.  The not-a-room-zero **arrival** gate is
closed above, the same day.  The fact that the walk **move** and meet-task
dispatch still sit outside the exact-tick test is still an open lead.

## CLOSED 2026-08-24 -- empty-M1 room alts, and recursive holding

Two `sclibrar.cpp` fixes, three live Wine measurements, 19 goldens across 13
games re-blessed, corpus back to **303/303 PASS**.  The full write-ups live in
the row comment blocks in `harness/run_v4_walkthroughs.sh` (canonical block on
the `lair-of-the-cybercow` rows; see also `xfiles`, `unraveling_god`,
`alices_restaurant`).  In short:

- **A matching method-0/1 room alt is the description's starting point even
  when its own M1 is blank**, and everything accumulated before it is thrown
  away.  `lib_find_starting_alt()` used to skip such an alt and keep scanning
  backwards.  Only the *non*-matching branch is guarded, on M2.  Confirmed at
  all three generations, and deliberately on cases where the new behaviour
  *loses* text, which is the direction that needed proving:
  - **3.7** `arlo.taf` / run370 -- cmd 34 now byte-exact against `Adven_6.rtf`.
  - **3.9** `lair-of-the-cybercow.taf` / run390 -- `Adrift_35.txt`.  Room 7's
    alt 0 is method 2, unconditional, "The end of a rope dangles here."; alt 1
    is method 1 on task 31 with M1 and M2 both empty.  Before `untie rope` the
    Runner prints the rope line; after it, it does not.
  - **4.0** `unravel.taf` / run400 -- `Adrift_34.txt`.  Every "Outside the
    MagLab" ends at "...is to the south." and never carries the "As nice of a
    day as it is, though, ..." block the old golden had.
- **Room-alt "is/isn't holding" is the Runner's recursive possession
  predicate** (run400 4579C1/4579EB -> `Proc_21_46` @44615C), so an object
  inside or on something carried or worn counts as held.  SCARE tested the
  object's own position only.  `xfiles.taf` is the only corpus row it moves,
  and that replay is now Runner-exact.

### Newly logged, not fixed

- **xfiles cmd 17, article capitalisation.**  run400 prints "You take **The**
  Warehouse Key from Case File 10193."; scarier prints "the Warehouse Key".
  The same object's *examine* message capitalises correctly in both.
  Pre-existing, unrelated to either fix above.

### Harness lessons from this round

- **Write cmdfiles with plain LF, never CRLF.**  `drive_ckpt_safe.sh` reads
  with `IFS= read -r line` and leaves the `\r` on, so `keystroke "$line"`
  submits the command by itself and the following `key code 36` submits an
  *empty* line.  The tell is a parser refusal ("Nope!", "I'll be dammed if
  that makes any sense.") after every single turn.
- **`drive_ckpt_safe.sh` now takes `TYPE_SLEEP` / `ENTER_SLEEP`** (defaults
  0.25 / 0.45), forwarded by `runner_transcript_safe.sh`.  A game whose
  responses run to several screens can still be laying out text when the next
  line is typed: the keystroke lands before the `(press any key)` pause
  exists, is eaten as an empty command, and the pause then swallows the
  *following* real command -- which reads exactly like the engine dropping a
  turn.  `unravel.taf` needs `TYPE_SLEEP=0.6 ENTER_SLEEP=1.6`.
- **`runner_transcript_safe.sh` now picks the largest window for the pid**,
  not the first non-1x1 one.  A game that opens with its own modal -- e.g.
  `lair-of-the-cybercow.taf`, which asks for a player name and then a gender
  in two separate dialogs (417x162 and 282x127) -- otherwise has that dialog
  chosen as "the window", and the whole Start-Transcript click sequence is
  aimed at it, so nothing in the game is ever clicked and the script reports
  "could not identify our own new transcript file".  Note the modal also
  *blocks the Adventure menu*: answer the game's startup prompts by hand
  first, then start the transcript, then drive the remaining commands.


## PARKED 2026-08-24 -- the object `seen` model, branch `scarier-seen-flag-port`

The xfiles replay (`Adrift_22.txt`) left one unexplained divergence: run400
answers `take knife` in Garage 5 with **"Take what?"** where Scarier takes the
knife.  Task 7 "Use Key" carries the player into Garage 5 with `ShowRoomDesc`
off, so no room description prints, and the knife is a dynamic that has been
lying there since the load.  The Runner's parser will not resolve a noun to an
object whose `seen` byte is clear, and nothing on that path ever sets it.

Scarier, by contrast, has always marked *everything* in the player's room seen
on every turn (`obj_turn_update`).  That is the bug.  The port lives on
`scarier-seen-flag-port` (commit `de7bffcc`); master is untouched and green.

### What run400 actually does

- **The gate.**  `co()` (`Proc_21_39_46486C`, `run400/Project/General.bas:8711`)
  tests `(obhere(obj) Or mode = 4) And obj(48) = 1` in both its counting loop
  (`@00464372`) and its selection loop (`@00464693`).  `takes`
  (`Proc_19_6_47C83C`) reaches `co()` at `@0047B9DC`, `@0047BD9D` and
  `@0047C694` and has no bypass, so the seen byte gates `take` as hard as it
  gates `examine`.
- **The seed.**  `openadv` clears the byte and sets it at `@004909B5` when the
  location field is `0` (held by the player) or `&H9C` (worn).  Statics reach
  that test through the same `location = InitialPosition - 1` mapping at
  `@00490270`, so a static whose `Where/Type` is **ONE_ROOM (1)** lands on 0
  and *starts seen*; some-rooms (`&HF6`), all-rooms (`&HEC`),
  part-of-character (`&HE2`) and hidden (`-1`) statics start unseen.  The
  Runner plainly never noticed it was labelling single-room statics "held".
  This quirk is load-bearing: it is the only reason a game with
  `DispFirstRoom` off -- `ZAC.taf`, `1HRGAME.taf`,
  `secret_of_lost_world` -- can answer `x sand` on turn one, since `tstart`
  calls `viewroom` only when that flag is set (`@0044D68F`).
- **The writers.**  All 47 sites, by containing function: `execute_action` 14,
  `whatisinon` 6, `viewroom` 4, `examines` 3, then two each in `openadv`,
  `obhere`, `inventory`, `charinv`, `insides`, `drops`, `dobattle` and the
  event mover `Proc_19_16_45614C`.  Census both decompiler idioms or the
  answer is wrong: `Dim from_stack_1.global_48 As Byte: ... = from_stack_2`
  **and** `var_XXX(48) = from_stack_1`, across `*.bas` *and* `*.frm`, with
  `grep -a` (General.bas reads as binary).
- **Task player moves reveal statics only.**  `execute_action`'s three
  player-room destinations (`@0048CA32` "to room", `@0048CADD` "to roomgroup
  part", `@0048CB48` "to same room as") sweep the object table and set the byte
  where `global_24 = 1` **and** the static's presence array covers the new
  room.  Dynamics are skipped -- which is precisely the xfiles knife.
- **Task/event object moves reveal only into the player's room.**  Both
  `execute_action` (`@0048C40A` and siblings) and the event mover
  (`@00456124`) compare the object's freshly written location field against
  `unk_409011.global_0` and stamp the byte only on equality.

### Why it is parked

The port takes the v4 corpus from 0 to **15** regressions:

    renegade_brainwave colony xfiles mr_smith spirits_flight spam wreckage
    imagination to_hell_in_a_hamper 3monkeys humbug deadman lair valley
    wonderwombat

`xfiles` is the intended one.  The rest are all the same shape, and
**Renegade Brainwave is the one that needs a live answer**.  Its task 15
(`Command "* west *"`, `ShowRoomDesc 7`, `CompleteText "You move west."`) has
three actions in this order:

    0  move object 4 (the crowbar) to room 7-1 = 6   <- player is still in room 0
    1  move character 0 (the player) to room 6
    2  move character 4 (NPC 2) to room 9-1 = 8

Under the model above the crowbar is moved while the player is elsewhere (no
reveal), the player-move sweep that follows touches statics only, and
`ShowRoomDesc` prints room 6 from *pre-action* state (see
`adrift4-showroomdesc-before-actions`), so it never lists the crowbar either.
Scarier on the branch therefore answers `take crowbar` with "Take what?" one
move into the walkthrough.

**The probe that settles it** -- load `Renegade_Brainwave.taf` in run400, type
`west`, then `take crowbar`:

- *"You take the crowbar."*  -> the model is missing a reveal on the task
  player-move path (dynamics as well as statics, or a post-action lister).
  Find it before landing anything.
- *"Take what?"*  -> the branch is right, and the 15 walkthroughs were derived
  against a permissive engine.  They then need re-deriving with an explicit
  `look` (or an `x` of the container/surface) before the take, and re-blessing;
  the win-marker guard will refuse any that stop being winnable, which is the
  signal to check the route by hand.

Worth measuring in the same session, since each is one command:

- `SPAM.taf` (`DispFirstRoom` off): `take spam` as the very first command.
- `1HRGAME.taf`: `x little table` then `take bubbles` -- the surface listing
  inside an object description is what reveals the bubbles, and that path
  (`examines`, `@0047174D`/`@00471DF1`) is already ported.
- `Colony.taf` (`DispFirstRoom` **on**): the two `Take what?` hits at golden
  lines 218/222 are dynamics in a described room, so if they fail live the
  room lister's marking is wrong, not the model.
