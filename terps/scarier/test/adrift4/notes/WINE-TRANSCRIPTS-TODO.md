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
    textutil -convert txt -stdout pfx/drive_c/adrift/Adven_1_marooned.rtf

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

⚠️ **The Runner reuses transcript numbers, so old citations rot.** Both
`Adrift_<N>.txt` and `Adven_<N>.rtf` restart their numbering when the prefix's
`adrift` directory is emptied or the Runner is reinstalled, and the new run
silently overwrites the old file. A note that cites "measured in `Adrift_18`"
is therefore only trustworthy if the file's mtime is *later* than the note.

To stop that happening again, every transcript in
`~/adrift-battle/runner/wine/pfx/drive_c/adrift/` was renamed 2026-08-24 to
carry the game it is a transcript of, keeping the original index as a prefix:

    Adrift_22.txt  ->  Adrift_22_xfiles.txt
    Adven_1.rtf    ->  Adven_1_marooned.rtf

Real games were identified by matching the transcript's opening prose against
the golden solutions; the synthetic probes (`pET*`, `srd`, `p39*`, `pwear400`)
by the `.taf` whose mtime immediately precedes the transcript's. Keep the
convention for new captures: `Adrift_<N>_<slug>.txt`.

Citations in the tree that still resolve were updated to the new names. These
ones were **not** -- their files have since been overwritten by a later run and
the measurement they describe is no longer reproducible from disk:

    sclibrar.cpp:5646, :6196          Adrift_8   (ALEXIS/iachini, 2026-08-22)
    scrunner.cpp:1251, :1279          Adrift_14/15 (now relojero)
    scrunner.cpp:1271, :1272          Adrift_18/19 (now funhouse / cat-in-the-tree)
    scrunner.cpp:1761, :2304          Adrift_18
    scrunner.cpp:2488, :2489          Adrift_20/18 (now maincourse)
    RUNNER_TESTS_TODO.md:778-780      Adven_2 cited as the run380 haunt.taf run
    harness/make_39_doneprobe.py:4    Adrift_14
    goldens/life_solution.txt:55      Adrift_22 (string is in neither Adrift_22 nor Adven_8)

Re-measure before relying on any of them.

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
| `arlo.taf` | 3.70 | full run370 replay, `Adven_6_arlo.rtf` | the 3.7 walk departure lines, incl. "walks off to not moved."; 3 differing of 85 |
| `tra.taf` | 3.80 | full run380 replay, `Adven_9_timmy_reid.rtf` | "outside" takes no "to" in a departure line |
| `Melbourne Beach.taf` | 3.90 | full run390 replay, `Adrift_37_melbourne_beach.txt` | the 3.9 walk directions, incl. the diagonal a pre-4.0 8-exit scan cannot name |
| `Orient_Express.taf` | 4.00 | full run400 replay, `Adrift_36_orient_express.txt` | the 4.0 walk directions; also the spurious "Gimme Atip enters." arrival |
| `S_Tar_Dus.taf` | 3.90 | full run390 replay, `Adrift_38_stardust.txt` | all 129 walk lines match count for count; pinned the not-a-room-zero arrival gate |
| `asteroid_after.taf` | 4.00 | live run400 probes (six co-present valves) + the corpus' ALR tables + UTF-16 literals in all four exes | the 4.00 object-ambiguity rule, its wording, its follow-up prompt, and that NPCs share the object message -- see the MEASURED section below |
| `p4ALR` / `p4ALRSRC` / `p4WALKCOUNT` / `p4VARFREEZE` (built probes) | 4.00 + 3.90 | run400 and run390 replays of four packed probe games | the whole **4.0 output filter**: walk = repeat a length-descending pass until nothing changes, self-containing ALRs retired per walk, one walk per completing task plus the flush, variables frozen by each walk -- see the FIXED section below |
| `3monkeys.taf` | 4.00 | live run400 replay of the solution's first 36 commands, `Adrift_16.txt` | the Runner really does print the raw `CHIMPSIGNAL=0`; the variable freeze is not a port artefact |
| `sa.taf` (`sophie`) | 4.00 | live run400 replay, `Adrift_41_sophie.txt`..`Adrift_45_sophie.txt` (five runs of the solution's first fifty commands), plus the game's own 488-entry ALR table | the walk announcement is **joined into the turn's paragraph**, so 12 of sa.taf's 65 join-spanning ALRs fire and delete the arrivals they match -- see the FIXED section below |
| `p4WALKALR` (built probe) | 4.00 | run400 replay, `Adrift_47_p4walkalr.txt` | the join itself, in isolation: an ALR whose Original starts with the two-space separator matches |
| `The_X-Files_A_New_Beginning.taf` (`xfiles`) | 4.00 | live run400 replay of the solution's first 40-odd commands, `Adrift_22_xfiles.txt` | a **"The" prefix is never lower-cased**, and **what is *on* an object is listed before what is *in* it, in one sentence** -- see the two FIXED sections below.  Also closed the `knock` lead (a feed artefact) and left `burn memo` open |

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
  the same game, so the pair also cross-checks a re-release.  The first fifty commands were replayed
  2026-08-25 and pinned the walk-announcement join; the rest of the row is
  still open.
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
| `sa.taf` | `sophie` | 255 | 7 | 73 | 13 | yes | **partly done** 2026-08-25 (first 50 commands) -- see "Measured so far"; [Sophies_Adventure_walkthrough](Sophies_Adventure_walkthrough.md) |
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

Step 2 is now mechanical.  `harness/compare_wine_transcript.py` takes the
game, the command file that was driven in and the Runner's own
`Adrift_N.txt`, replays the same feed through `harness/scare`, and splits
both sides into turns — the Runner's by its echoed command line, scarier's by
the `>` prompt:

    python3 harness/compare_wine_transcript.py \
        --taf games/The_X-Files_A_New_Beginning.taf \
        --feed goldens/xfiles_solution.txt \
        --runner ~/adrift-battle/runner/wine/pfx/drive_c/adrift/Adrift_22_xfiles.txt

It prints, before any diff, **every feed command the Runner never echoed**,
and marks every later turn as past that point; then it diffs the turns that
did line up, whitespace-normalised so the Runner's own hard wrapping is not a
difference, and says so explicitly when the two streams re-synchronise a
prompt out.  Its self-test is the X-Files row, where it finds `feed[23] look`
— the one lost command that produced two apparent engine bugs (`take knife`
and `knock`) and cost a day of argument before anyone counted the echoes.
`burn memo` at turn 4, which is upstream of the loss, is the only real
divergence it reports there.

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
  **FIXED 2026-08-24**: the two `Take what?` hits and the `look at camera`
  refusal are the object `seen` gate, not the noun matcher -- ported and
  landed, see the seen-model section at the end of this file (`take knife` in
  `Adrift_22_xfiles.txt` is one of the two live measurements the port rests on).
  **CLOSED 2026-08-25: `knock` was never a divergence, and neither was the
  `take knife` difference.**  Both are downstream of one command the feed lost.
  The walkthrough reads `use key` / `look` / `take knife`; the transcript reads

      use key
      The garage door begins to groan open ...
      take knife
      Take what?

  -- no `look` echoed between them.  So the Runner entered Garage 5 through a
  `ShowRoomDesc = 0` task, nothing ever listed the Small Pocket Knife, and its
  `seen` byte stayed clear; Scarier *did* get the `look`, listed the knife and
  took it.  `knock` then follows mechanically: task 9 `Knock` has four ANDed
  restrictions (`#A#A#A#`) and the fourth is **Knife held**
  (`RESTR type=0 v1=26 v2=1 v3=0`), so the Runner answers with that
  restriction's own FailMessage, "You should check out the warehouse first."
  One lost command, two apparent bugs.  Note this does **not** weaken the
  seen-model measurement -- losing the `look` is exactly what left the Runner
  on the bare `ShowRoomDesc = 0` entry the model predicts a refusal for.  It
  does mean the walkthrough's own comment at `goldens/xfiles_solution.txt:25`
  overstates the case: the `look` on the next line would have listed the knife,
  and the Runner simply never saw it.
  `burn memo` is still open, but is narrowed -- see the OPEN section below.
- **FIXED 2026-08-25: the 4.0 battle narration names an NPC by its alias,
  not by its Name.**  Found by re-running `compare_wine_transcript.py` over
  `Adrift_36_orient_express.txt`, a transcript that was measured for the walk
  work and whose battle lines had been written off as noise.  run400 fights
  "the large man" and "BIG BOSS"; Scarier printed "Igotta Bigbottom" and
  "Ivill Getyou".  The rule, from `Battles.bas`: given a first alias, a blow
  names the NPC `"<Prefix> <Alias[0]>"` -- the player's blow (Proc_11_1,
  @45E1CE) from any NPC, an NPC's blow (Proc_11_2, @464F20 attacker /
  @464FF2 target) only from a combatant whose **current** `Battle.Attitude`
  is enemy (the record byte at +172, tested `= 2`).  Nothing else follows it:
  the corpse line reads the Name field raw (@44B115), and so does every room
  listing -- the very same transcript says "Igotta Bigbottom is here." one
  line above "You manage to avoid the large man's attack."  The dump confirms
  each name it produces (`SCR_DUMP_TASKS=1 SCR_DUMP_BATTLE=1`, which now
  prints an NPC's prefix and aliases): npc 3 `prefix=[the large] alias=[man]
  attitude=2`, npc 6 `prefix=[BIG] alias=[BOSS] attitude=2`, and npc 5
  `Thug ` with no alias at all -- which run400 duly fights as "Thug ",
  trailing space and all.  Ported as `battle_print_npc_name()` in
  `scbattle.cpp`, guarded by `battle_legacy` because the pre-4.0 battle
  system is a different set of strings and names by Name (run390 `Form1.frm`
  @4595DB).  Seven goldens moved: `orient_express` (now matching the Runner
  transcript on every battle line), `trabula`, `shadowpeak` x3, `cyber2` and
  `light_up`.  **Still unmeasured:** whether run400 capitalises a battle line
  that now *starts* with a lowercase alias -- `trabula` gives "a soldier
  attacks you with the rapier", and no saved transcript has an NPC-initiated
  blow to check it against.  The decompile concatenates raw, so the port does
  too; a probe on a game with a lowercase-prefixed enemy would settle it.
- **FIXED 2026-08-25: a bare Return is a parser complaint, not silence.**
  Found by rule 2 rather than in spite of it.  `cmdfile_stardust.txt` and
  `cmdfile_xfiles.txt` are the only CRLF feeds in
  `~/adrift-battle/runner/wine/` (`grep -c $'\r'`: 117 of 117 lines and 21 of
  21), so every command in those two runs was driven in followed by an extra
  empty Return -- and both Runners answered every one of them.  run390's
  `Adrift_38_stardust.txt` carries 115 copies of that game's ALR for
  DontUnderstand, "I are confused.  DURHH!"; run400's `Adrift_31_xfiles.txt`
  carries 22 of xfiles' "Nope!", two of them before the first command is even
  echoed.  Nothing else comes with the message -- no walk line, no event line
  -- so an empty command does **not** tick the turn.  Scarier printed nothing
  at all: upstream SCARE guarded the not-understood block with
  `if (!scr_strempty (command))`.  That guard is gone (`scrunner.cpp`); the
  no-tick half was already right, since the complaint returns FALSE and
  `run_main_loop()` ticks only on TRUE.  An empty `line_element` can only come
  from a genuinely empty input line -- the element splitter already takes the
  first character even when it is a separator, so `.` and `i. .` were
  complaining before this and still are.
  **38 goldens moved, every one of them purely additive** (192 lines added, 0
  removed, no route change, no win marker lost), because a blank line in a
  solution file is a turn like any other.  Two kinds of blank line turn out to
  live in the corpus, and the corpus now shows which is which: real
  empty-command turns (`cbn`'s five leading blanks, whose header already
  carried a FOOTGUN saying so -- one of them is what a `*` task turns into the
  move out of room 0) and mere layout (`iachini` and `wonderwombat` separate
  their commented sections with blanks; `wes_ghn` has 77 blank lines, 45 of
  which reach the parser).  The layout ones now print the game's complaint,
  which is faithful to that feed and is *why* they are worth seeing.  Cleaning
  them out of the solution files is optional and was NOT done: for a row
  without `SCR_SKIP_WAITKEY=1` the blanks are load-bearing -- the `<waitkey>`
  read eats them in file order -- so deleting the wrong one desyncs the route.
- **Games whose transcripts carry RNG-timed lines** (`xfiles`, `wamk`) need a
  *targeted* Runner probe rather than a full replay.  There is no harness for
  that yet; the p4WK* probe .taf files in
  `~/adrift-battle/runner/wine/pfx/drive_c/adrift/` were built by hand in
  gen400 and there is no script that regenerates them.  Note that RNG-timed
  lines do not by themselves make a game unmeasurable -- see `the_pk_girl`
  below, where the diff is noisy but the *outcome* lines are not.
- **FIXED 2026-08-24: a dead NPC still walks in Scarier** -- ported; see the
  section at the end of this file for the full P-code case and the Azra
  fallout.  Original note (read out of run400 while chasing the
  PK Girl walk counters, 2026-08-24):  run400's walk ticker opens with
  `Proc_19_1_468DA0` @0004685B6: `If npc.Room = &HFB Then GoTo 468D61`, i.e.
  it skips *every* walk of an NPC whose room is 251.  251 is the battle
  system's "dead" marker (`Battles.bas` @00044B127, right after the
  " falls down, dead." line).  Scarier has no such marker: `battle_npc_die()`
  in `scbattle.cpp` puts the corpse in location 0, which is "Hidden", and
  `npc_tick_npc()` goes on ticking its walks -- so a walk can march a dead
  NPC back into play.  A faithful fix needs a *separate* dead flag, because
  run400 does keep ticking the walks of a merely hidden NPC (that is how a
  hidden walker comes back); reusing location 0 for both would break that.
  (`&HFB` is really **-5**, not 251: `LitI2_Byte` sign-extends.)
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
- **CLOSED 2026-08-25: `the_pk_girl`'s second Detainment visit** (opened
  2026-08-24).  Scarier now prints exactly what the Runner does, at both
  visits, and the closing needed no new measurement -- only re-reading the
  old one against today's engine.  The original note called this "which
  alternate NPC description the room lister picks", and that was wrong on the
  mechanism: there is no selector.  Laurie's line is an **ALR** keyed on a
  variable -- `[[LAURIE_DOING=6]]` -> `is in your arms.`,
  `[[LAURIE_DOING=9]]` -> `is standing here.` -- so the two engines were
  simply printing the room at different values of `laurie_doing`, 6 against 9.
  Both Detainment entries are `ShowRoomDesc` tasks (task 1866 for the first
  visit and task 1898 for the second, both `where=1 room=104 srd=106`), and
  the reunion that sets `laurie_doing = 6` is task 1955, reached from those
  entries.  So the room description has to be printed against the **pre-action**
  world state, which is exactly what the "ShowRoomDesc prints BEFORE the task's
  actions" port established a day later; it fixed this row as a side effect and
  nobody came back to cross it off.  Verified by replaying the golden under
  `SCR_TRACE_TASKS=1` and diffing both visits against
  `Adrift_27_thepkgirl.txt` lines 2807 and 3004: "Laurie is lying on the
  floor." then "Laurie is standing here.", both identical now.
- **NOT A BUG 2026-08-25: a typed task that prints nothing is refused, and
  run400 refuses it too.**  Noticed while building
  `make_400_walkcapprobe.py`: typing `sil1` at `p4WC.taf` (the walk-count
  probe, whose `romeo`/`sil1`/`sil2`/`sil3` have empty CompleteText, no
  ShowRoomDesc and no AdditionalMessage) gets "I don't understand." from
  Scarier, while `kilo` next to it answers "K qqqball."  That looked like a
  matcher gate.  It is not: the Runner's own typed-command task dispatcher,
  `Proc_19_24_44CCE0` (run400 `mdlSpreadTheLoad.bas:21595`, called as `tasks`
  from `generaltasks`), ends

      loc_44CCC0:  If MemVar_4941B0 = "" Then  Result = 0        ' FALSE
                   Else  MemVar_4941B0 = Proc_21_18_47A3DC(MemVar_4941B0)
                         Result = var_86

  -- so however the match went, a turn that left the output buffer empty is
  reported as *not handled*, and the caller falls through to the library and
  then to the unknown-command message.  The match itself does happen
  (`Proc_19_66_454EF0` returns the task index at `loc_44CBDB`, and
  `Proc_19_11_45A3EC` = `execute_task` runs it at `loc_44CC3C`), so the
  task's ACTIONS still run before the refusal is printed.  Scarier's
  `task_run_task_unrestricted()` (`sctasks.cpp`) returns the same FALSE by
  accumulating a per-print `status`, which is why the probe's silent cells
  behaved the way they did; giving each cell a real CompleteText was the
  right workaround, not a workaround for a bug.

  One difference is worth keeping in view and is **not** measured: run400
  tests the WHOLE turn buffer, Scarier tests the task's own output.  They
  differ only if something has already written to the buffer before the verb
  dispatch runs -- `generaltasks` does have the References-in-brackets echo
  ahead of it -- in which case run400 would answer TRUE where Scarier answers
  FALSE.  It needs a command that both triggers that echo and matches a
  silent task; no corpus row is known to.
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
    python3 <scratch>/hb_partb.py pfx/drive_c/adrift/Adrift_30_humbug.txt \
        <repo>/goldens/humbug_solution.txt cmdfile_hb_B.txt

    # phase B -- into the SAME pid, no relaunch
    FIRSTCHECK=pfx/drive_c/adrift/Adrift_30_humbug.txt sh drive_ckpt_safe.sh <pid> cmdfile_hb_B.txt

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
    sweet on the plinth." (the "Okay." is *doubled*), Scarier printed one.
    **FIXED 2026-08-24** -- the whole 4.0 output filter, see the section at the
    bottom.  Neither "Okay." is authored.
  * command 254 `W` -- Scarier prints "(Getting off the stool first)", run400
    prints nothing.  **FIXED 2026-08-24, and it was never a bug in the mover**
    -- see "the bracket checkbox governs three more lines" below.
  * command 321 `X Grandad` -- this was the "and is carrying" bug, now FIXED
    (see Open leads).
- Next candidates down the list, in order: `arlo.taf` (3.70, 11 walks / 85
  commands -- the best pre-4.0 target, and it shows up twice in the killer-walk
  scan), then `goldilocks`, `cibass`, `sophie`/`sa.taf`.  (`arlo` and
  `sophie` are both done as of 2026-08-25 -- `sophie` only for its first fifty
  commands -- so the next two down are `goldilocks` and `cibass`.)

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
| arlo.taf | 3.70 | run370 | `Adven_5_arlo.rtf` (Verbose off), `Adven_6_arlo.rtf` (Verbose on) | 6 differing of 84, all NPC-walk payload |
| akron.taf | 3.80 | run380 | `Adven_7_akron.rtf` | **0 differing / 44** |
| mikes.taf | 3.80 | run380 | `Adven_8_mikes.rtf` | 5 differing of 103, all downstream of one desync |

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
  `pfx/drive_c/adrift/Adrift_40_merry_murders.txt`).  The Runner desynced at command 4:
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
  the stool first)").  Both are now **CLOSED 2026-08-24** -- the second was a
  display setting, not an engine bug (see the section below), and the first
  turned out to be the whole 4.0 output filter (last section).
- **Next candidates** down the list: `goldilocks`, `cibass` (`sophie`/`sa.taf`
  has since been replayed for its first fifty commands, 2026-08-25).
  With the pre-3.9 pool now clean, the remaining 3.90 and 4.00 candidates are
  where the next divergences will come from.

## DIAGNOSED 2026-08-24 -- the Runner's co() object-ambiguity test (mikes)

The second pre-3.9 divergence, and the only one left in the pre-3.9 pool.
run380 answers mikes cmd 27 `take truck keys` with

    Which keys.  The mustang keys or the truck keys?

and does not take them (`Adven_8_mikes.rtf` line 207).  Scarier binds the truck
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
good evidence run400 does something else; the obvious candidate, narrowing on
the longest match, is wrong, and the section below has what it really does.
It cannot be read off the decompile: run400 keeps
its messages in a table, so neither `run400.bas` nor `run400.p32dasm.txt`
resolves the "Which " literal, although the literal really is in the binary
(UTF-16LE at file offset 0x17a2c in run400.exe, and likewise at 0x9568 /
0xb270 / 0xee08 in run370/run380/run390).

At 3.7/3.8/3.9, where the rule *is* established, mikes cmd 27 is the corpus'
only divergence -- one row, whose walkthrough would then need re-deriving
(drop the mustang keys before taking the truck keys, presumably; there is no
adjective that would disambiguate).  Porting on that alone would mean
inventing a 4.00 rule.

### MEASURED 2026-08-24 -- run400 narrows, but not by the longest match

`open second valve` in run400 answers normally, so 4.00 does narrow and the
"the game would be unplayable" argument above is void.  What it does *not* do
is prefer the longest matching name.  Twenty-odd probes into a live run400 on
`asteroid_after.taf` (pid kept warm, `look` between probes -- see the first
footgun below) give a two-pass rule:

* **Pass 1 -- Short.**  Any present object whose **Short** appears
  word-bounded anywhere in the command resolves the reference outright.
* **Pass 2 -- aliases, last writer wins.**  If no Short hit, the objects are
  walked in object order and every object that hits *overwrites* the term
  with its own last-hitting alias.  Only the final -- highest-numbered --
  hitting object's term survives, and the count of present objects whose
  Short or alias equals that term decides.

The consequence is brutal, and it is the part no amount of reading the
decompile would have suggested: **an alias that uniquely names an earlier
object is unreachable whenever a later object shares any alias with it.**

| typed | run400 | why, under the rule |
| --- | --- | --- |
| `open second valve` | opens it | pass 1, Short `second valve` |
| `x first valve`, `x fifth valve`, `x sixth valve` | resolves | pass 1 |
| `x valve` | ambiguous | obj5 writes last: term `valve`, present 6 |
| `x valve one`, `x valve 1` | **ambiguous** | obj0 has alias `valve one`, but obj5 writes last and its only hit is `valve` |
| `x valve two`, `x valve 2`, `x valve 3`, `x valve four` | ambiguous | same |
| `x valve five`, `x valve 5` | **ambiguous** | obj4 has alias `valve five`, obj5 still writes last |
| `x valve six`, `x valve 6` | sixth valve | obj5's last hit is `valve six` / `valve 6`, present 1 |
| `x safety`, `x safety valve` | sixth valve | obj5's last hit, present 1 |
| `x sixth`, `x six`, `x 6`, `x 6th` | sixth valve | only obj5 hits at all |
| `x one`, `x first` | first valve | only obj0 hits |
| `x 2nd` | second valve | only obj1 hits |

`x valve five` against `x valve six` is the decisive pair: obj4 and obj5 carry
the identical alias shape (`fifth, valve, 5th, 5, five, valve 5, valve five`
vs `sixth, valve, 6th, 6, six, valve 6, valve six, safety, safety valve`) and
only the *later* one resolves.  That kills every "best match wins" reading.

### MEASURED 2026-08-25 -- run390 is not longest-match either, and it costs stardust the game

The 117-command `S_Tar_Dus.taf` / run390 replay (`Adrift_38_stardust.txt`)
was re-read against today's engine.  Once the Verbose brief-mode headings are
set aside (rule 1 -- that session had Verbose OFF, so every re-entry reads
"You move west.  Open Area." against our full description) the whole
transcript holds **exactly one** engine divergence, and it decides the game:

    turn 38  take needle box
      run390   You've already got the sharp needle!
      scarier  You take a needle box from the desk.

The game has `obj3` Short `needle` Prefix `a sharp` and `obj12` Short
`needle box` Alias `box`, both on the desk, and `take needle` at turn 37 has
already taken the needle.  No task matches `take needle box` (the only needle
task is TASK 4 `put needle in box`), so this is the library's own reference
resolution: run390 resolved `needle box` to the **lower-numbered obj3**, whose
Short is merely *contained* in the command, over obj12, whose Short is the
whole of it.  It never prompts -- only one present object answers to the term
`needle`, so the ambiguity count of step 4 above is 1 -- which makes this a
measurement of the *resolution* order rather than of the prompt.  It is the
3.90 twin of the run400 asteroid_after finding: **neither generation takes the
longest match.**

The cost is the ending.  Without the box:

    turn 99  put needle in box
      run390   You don't have the box.  Put the sharp needle inside what?
      scarier  You put the needle in the box.  Smart move. ...

-- TASK 4's first restriction fails with its own message and the command then
falls through to the library, which is the pre-4.0 "library wins over a
restricted task" rule already pinned elsewhere in this file.  TASK 4 stays
unspent, so the `sw` at turn 116 misses T35's gate and falls through to T36:
the Runner's transcript ends "You step through the portal...  Better luck next
time.", where our golden ends "You decide to go with the plant lady and ...
Well done - you scored maximum points!"  **`stardust` is therefore the first
corpus row where this item changes an OUTCOME and not just a line**, and the
walkthrough's starred WIN is, as things stand, a Scarier-only win.

The repair is known and cheap, which is worth recording now so that the port
is not blocked on re-deriving a route: `take box` -- obj12's own alias, which
no other object answers to -- takes the needle box, and the route still
reaches T35 (verified offline, one `sed` over the solution file).  It is
deliberately NOT applied yet: the walkthrough is only wrong once the engine is
right.

### Two footguns, either of which invalidates a measurement

**run400 has a disambiguation follow-up prompt.**  After `Which ...?` the
*next* line is consumed as the answer, not as a fresh command, so a probe file
of back-to-back ambiguous commands measures nothing at all:

```
x valve one   -> Which valve?  The first valve, ... or the sixth valve?
x valve 2     -> That is still ambiguous!            <- eaten as the ANSWER
x valve three -> Which valve?  ...
x valve 4     -> That is still ambiguous!            <- eaten
```

The alternation is the tell.  Put a neutral `look` between probes.  The answer
is re-joined with the pending verb (`first` examines the first valve,
`sixth valve` the sixth); an answer naming nothing falls through to the
ordinary parser (`xyzzy` gets the XYZZY refusal, and the prompt is dropped);
an answer that is itself ambiguous gets `That is still ambiguous!`, a wrong
one `That wasn't one of the options!`.  Scarier has none of this -- it prints
its message and forgets.

**Read the game's own ALR table before recording any library wording.**
asteroid_after carries `ALR [Which valve.] -> [Which valve?]`, so the live
Runner prints `Which valve?  The first valve, ...` and the raw run400 message
is `Which valve.  The first valve, ...`, with a period.  Measuring the
punctuation off that screen would have recorded the game's rewrite as the
Runner's wording.

### The ALR tables are a free, offline oracle for Runner wording

An ALR's *Original* is the author's own transcription of Runner output, typed
while looking at the real thing.  `scdump.cpp` now dumps them one per line as
`ALR [orig] -> [repl]` under `SCR_DUMP_TASKS`.  Across the 253 v4-era corpus
games, 38,582 ALRs:

| shape | rows |
| --- | --- |
| `Which <term>.  <list>?` | 92 |
| `It is not clear which <term> you are referring to.` | 18 |
| `That is still ambiguous!` | 8 |
| `That wasn't one of the options!` | 1 |
| `Please be more clear...` (ours) | **0** |

Message shape, read off those 92 rows: `"Which " & term & ".  " & list & "?"`
-- two spaces after the period, items `tense(Prefix) & " " & Short` joined
`", "` with `" or "` before the last, first item sentence-capitalised.  An
empty Prefix leaves its space in, which is why asteroid_after transcribed
`Which satellite.   satellite,  satellite or  satellite?` (three spaces) and
Vendetta `Which girl.   girl or  girl?`.

The term is whatever *matched*, not a noun.  `cursed` alone supplies
`Which fallen.`, `Which broken.`, `Which small.`, `Which loose.`,
`Which metal.` and `Which red.` -- adjectives lifted straight out of the alias
lists, exactly as pass 2 predicts, alongside the ordinary
`Which armour.  The silver suit of armour or the gold suit of armour?`.

### The version split, read out of the four exes

VB6 keeps these as UTF-16LE literals, so `strings` misses them; extract with
`re.finditer(rb'(?:[\x20-\x7e]\x00){3,}', exe)`.

| literal | 370 | 380 | 390 | 400 |
| --- | --- | --- | --- | --- |
| `Which ` | yes | yes | yes | yes |
| ` would you like to take.  ` | yes | yes | yes | yes |
| ` would you like to drop.  ` | yes | yes | yes | yes |
| ` would you like to examine.  ` | yes | yes | -- | -- |
| `That wasn't one of the options!` | -- | -- | yes | yes |
| `That is still ambiguous!` | -- | -- | -- | yes |
| `It is not clear which ` + ` referring to.` | -- | -- | -- | yes |
| `It is not clear which object you are referring to.` | -- | -- | -- | yes |
| `Sorry, I'm not sure which object you're referring to.` | -- | -- | -- | yes |
| anything with *who*, *character* or *person* | -- | -- | -- | -- |

So the answer state machine arrives at 3.9 and grows `That is still
ambiguous!` at 4.0; the whole `It is not clear which ...` family is 4.0-only.
`Please be more clear` is in no Runner and in no ALR: SCARE invented it.

### NPCs are disambiguated -- by the same message, and by alias

There is no "who" message in any Runner because characters go through the
*object* builder.  Vendetta's `ALR [Which girl.   girl or  girl?]` is two
**characters** sharing a room: Chloe (room 5, aliases `girl, lady, woman,
female, friend`) and Sally (room 5, aliases `sal, woman, girl, female`), both
with an empty Prefix.  Note what is printed -- the *matched alias*, not the
character's Name -- confirmed independently by the same game's
`ALR [I don't think girl would appreciate being handled.] ->
[I don't think she would appreciate being handled.]`, the Runner's NPC-touch
refusal naming the character "girl".

`lib_disambiguate_npc()` is therefore wrong twice: the wording, and rendering
the Name where the Runner renders the matched alias.  The three
`Please be more clear, who do you want to attack?  Red Riven or Blue Riven?`
lines in `light_up_solution.expected.txt` are both.

### `get scammin's ring` turns out to be a non-test

mysteryofcaves obj9 `Blocker's Ring` (aliases `Blockers Ring`, `ring`) is
behind the boulder door in the hidden grotto; obj10 `Scammin's Ring` (aliases
`Scammins Ring`, `ring`) is in the chest on the island.  They are never in the
same room on the walkthrough's path, so the shared `ring` alias never
collides and nothing narrows.  (That the author supplied apostrophe-free
aliases for both is a separate hint -- that run400 strips the apostrophe out
of typed input.)  The valve set already supplies the co-present case the ring
pair was meant to.

### Why it is *still* not ported -- a better reason than before

run400's disambiguation is **per-handler**, and only two handlers have it
(measured the same day, same live Runner):

| typed | run400 |
| --- | --- |
| `x valve`, `read valve`, `look in valve` | `Which valve.  ...?` |
| `take valve`, `drop valve` | `It is not clear which valve you are referring to.` |
| `open valve`, `close valve` | `You can't open/close that.` |
| `turn/move/sit on/touch valve` | `You can't <verb> that.` |
| `wear valve` | binds the FIRST candidate silently: `You are not holding the first valve.` |
| `push valve`, `pull valve` | `You push/pull, but nothing happens.` |
| `eat valve`, `put valve in X` | `I don't understand...` |

Scarier prints its ambiguity message at roughly 25 call sites.  Matching
run400 means deleting it from most of them, adding the two 4.0 messages and
the follow-up-prompt state machine, and version-splitting the lot against
3.7/3.8's `Which X would you like to examine.`  Every corpus walkthrough
passes today (303/303) precisely because they name objects by their Short.
Left unported deliberately -- but this section is now the specification for
doing it, and nothing above needs Wine again.

## DIAGNOSED 2026-08-24 -- the run370 double matcher pass (arlo)

The one remaining pre-3.9 divergence, read out of the run370 p-code and
matched line for line against `Adven_10_arlo.rtf`.  **Understood in full, and
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

Measured in run400's `Adrift_23_where_are_my_keys.txt` (WhereAreMyKeys.taf, 4.00):

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

The nested case was deliberately left alone here: when an object is both *on*
and *in* the associate, run400 reaches the same lister with `var_9E == 1` and
prints a prefixed ", and inside is `<list>`", which scarier did not model.
**Ported 2026-08-25** off the xfiles replay, which does exercise it -- see
"what is ON an object is listed before what is IN it" below.

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

**Probe built 2026-08-25, waiting on Wine.**
`harness/make_400_stateprobe.py` -> `p4STATE.taf`, staged in
`~/adrift-battle/runner/wine/pfx/drive_c/adrift/` with `cmdfile_state.txt`
next to it.  Six stateful objects in one room, each carrying one of the
corpus's destructive shapes -- `In the UP position`, `Facing South`,
`Locked Off`, `R1`, `Sur la gauche` -- plus `switched off` as the control,
which is already lower case and so must come back unchanged under either
rule.  Each is read through all three callers, because they are three
separate sites in run400 too and need not agree: `x <obj>` (the examine
lister, `BStateListed` on), `ob <obj>` (a task printing `OB=[%obstate%]`) and
`st <obj>` (one printing `ST=[%state_<obj>%]`); a fourth cell, `mid <obj>`,
puts the same name mid-sentence in case run400 only lower-cases what opens a
line.  `flip` then moves the lever to `In the DOWN Position` so all four reads
repeat on a state the game switched to rather than started in.  Scarier's
answers, for the diff:

    x lever    A test object.  The lever is In the UP position.
    ob panel   OB=[R1]
    st sign    ST=[Sur la gauche]
    mid lever  MID: the lever reads In the UP position today.
    flip / x lever   A test object.  The lever is In the DOWN Position.

Run it with

    cd ~/adrift-battle/runner/wine
    LOAD_SLEEP=22 sh measure.sh p4STATE.taf cmdfile_state.txt

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
| `tra.taf` | run380 | `Adven_9_timmy_reid.rtf` | no "Sting walks towards you." -- but "Canadian couple walks towards you from the north." *is* there, so the gate is the zero, not the walk |
| `S_Tar_Dus.taf` | run390 | `Adrift_38_stardust.txt` | full 117-command replay: **all 129 walk lines match count for count** across four walkers and six directions, and the bare "Plant Lady prances along." is absent from the Runner while its four directional siblings are in both |
| `Orient_Express.taf` | run400 | `Adrift_36_orient_express.txt` | "Gimme Atip enters." is printed by Scarier and by no Runner -- the divergence that started the item |

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
| `arlo.taf` | run370 | `Adven_6_arlo.rtf` | all three 3.7 departure lines; arlo down to **3 differing of 85** |
| `tra.taf` | run380 | `Adven_9_timmy_reid.rtf` | "Hovey shuffles off outside." -- no "to"; the old golden was wrong |
| `Melbourne Beach.taf` | run390 | `Adrift_37_melbourne_beach.txt` | all four changed sites, incl. the dropped diagonal ("David strolls in.") |
| `Orient_Express.taf` | run400 | `Adrift_36_orient_express.txt` | every 4.0 walk line, incl. "...walks off to the west." and "...wobbles in from the east." |

At 3.9 only the NPC *verb text* ever differed from the Runner (the walk's
random alternate texts), never the direction.

Two residual items came out of this.  The not-a-room-zero **arrival** gate is
closed above, the same day.  The fact that the walk **move** and meet-task
dispatch still sit outside the exact-tick test is still an open lead.

## FIXED 2026-08-25 -- the walk announcement is JOINED into the turn's paragraph

The Runner does not give an NPC walk announcement a line of its own.  It
appends it to the buffer the turn has built so far, with the same two-space
section separator every other run-on uses -- so the announcement is part of
the turn's ONE paragraph, and an author's ALR pass sees the join.  Scarier
started the line instead.  **61 goldens re-blessed, corpus 304/304 PASS.**

### The separator, and its version split

3.9 and 4.0 call the shared "pspace" routine (run390 `loc_45A99E`, run400
`loc_468A67` arrival / `loc_4688D3` departure).  3.7 and 3.8 write it inline
and shorter:

    run370 loc_4395AA / run380 loc_441740
        If Right(buf, 1) <> Chr(10) And Len(buf) > 0 Then buf = buf & "  "

which tests only the LAST character, so a buffer already ending in two spaces
gets two more.  Pre-3.9 really can make four spaces where 3.9/4.0 make two.
Ported as `pf_buffer_join()` (pspace) and `pf_buffer_join_always()` (the
pre-3.9 inline form) in `scprintf.cpp`, split at `TAF_VERSION_390` in
`npc_announce()`.

### The Name is capitalised in 4.0 ONLY

`Proc_21_3_446BB4` (run400 `General.bas:75`) is
`UCase(Left(s,1)) & Right(s, Len(s)-1)`, and run400 pipes the NPC Name
through it at **both** `npc_announce()` sites -- `loc_468A79` (arrival) and
`loc_4688E0` (departure).  It is called at neither site in run370
(`loc_43961A`), run380 (`loc_4417B0`) or run390 (`loc_45AA0F`), which push the
raw Name.  Nor at run400's **hidden**-departure site `loc_468CF9`, which
appends a bare `"  "` with no pspace call and no capitalisation at all (as
does run370 `loc_4397A3`).  So: capitalise in 4.0's two `npc_announce()`
sites, nowhere else.

The corpus case is `baroo.taf` (4.00), whose NPC Names are lower-case
"wizard" and "warlock": the golden reads "Wizard walks off ...".  Gated on
`is_400` in `npc_announce()`.

### Why it matters beyond whitespace: the ALRs span the join

Because the announcement lands in the same buffer, an author who wants to
reword a walker writes an ALR whose Original starts with the separator.
`sa.taf` (sophie, 4.00) has **65** such join-spanning ALRs, **12** of which
fire in the walkthrough -- and each one *deletes* the arrival it matches.
That is the whole substance of the two sophie goldens' 12 lost arrival lines;
every one was matched mechanically back to an ALR Original in the table
(count at 0-based plain-text line 255954 of `taf_pattern_scan.plaintext()`'s
body, pairs on the lines after it).  `circus.taf` corroborates from the other
direction with `'  Joe' -> '  The vendor'`, which only fires once the two
spaces are there.

### What was measured, and where

Runner transcripts confirm the join on **every** generation:

| Game | Runner | Transcript |
|---|---|---|
| `arlo.taf` | run370 | `Adven_6_arlo.rtf` |
| `tra.taf` | run380 | `Adven_9_timmy_reid.rtf` |
| `Melbourne Beach.taf`, `S_Tar_Dus.taf` | run390 | `Adrift_37_melbourne_beach.txt`, `Adrift_38_stardust.txt` |
| `Orient_Express.taf` | run400 | `Adrift_36_orient_express.txt` |

(The melbourne_beach "Kitty comes in" / golden "Kitty saunters in" mismatch is
the walk's random alternate verb texts, already documented on the arlo row --
the direction and the join match.)

The join itself was additionally pinned by a built probe, `p4WALKALR.taf`
(`harness/make_400_walkalrprobe.py`, transcript `Adrift_47_p4walkalr.txt`).

Of the 61 goldens this moved, **57 differ from their predecessors in
whitespace alone**; the other four are `baroo` (the capital), `circus` (the
`'  Joe'` ALR) and the two `sophie` rows (the 12 deleted arrivals).  The
canonical write-up is the dated block above the `arlo.taf` row in
`harness/run_v4_walkthroughs.sh`, with pointer comments on the `circus`,
`sophie`, `sophie_comp` and `baroo` rows.

### Still to do

`harness/make_400_walkcapprobe.py` is the live confirmation of the 4.0-only
capitaliser -- **built and not yet run**: the Mac's screen was locked and the
Wine harness drives the Runner's menus through System Events, so it cannot run
without an unlocked desktop.  It packs two rooms and one NPC named `"bob"` on
sophie's walk, and four cells: `e`/`w` (a mid-paragraph join) and `pb`/`pa`
(a task whose CompleteText ends in `<br><br>`, so pspace adds nothing and the
Name opens a line).  Expectation, from the listings: `"bob"` -> `"Bob"` in all
four.  Run it with

    LOAD_SLEEP=22 sh measure.sh p4WALKCAP.taf cmdfile_walkcap.txt

and replace the probe's "ANSWERED FROM THE LISTINGS 2026-08-25" docstring with
the live transcript.

Two harness facts fell out of the failed attempts, both worth keeping:

- **`measure.sh` now forces Verbose ON in the registry**, not with a blind
  Ctrl+V toggle.  `pfx/user.reg` carries `"Verbose"="True"` under
  `VB and VBA Program Settings\ADRIFT\Runner` and it *survives* launches, so
  the old blind toggle flipped a Verbose-ON prefix OFF and made every room
  re-entry look like a divergence (`run400-verbose-toggle` says the value
  resets each launch; it does not).  Decide whether to mirror the
  `~/adrift-battle/runner/wine/measure.sh` changes back into the repo.
- A locked screen fails in a way that looks like a game problem: three runs
  died with "WARNING: 3 pause-dismiss Return(s) sent" / "Save-transcript
  dialog never appeared", and the known-good `p4WALKALR.taf` failed
  identically.  The tell is elsewhere: `swift winlist.swift` shows
  `loginwindow 30000x30000`, `osascript` returns `-1719 Can't get process 1
  whose frontmost = true`, and `screencapture -R`/`-l` both refuse.

## FIXED 2026-08-25 -- a "The" prefix keeps its capital

The X-Files: A New Beginning gives its Memo the Prefix `The` (straight out of
`SCR_DUMP_TASKS=1`: `OBJNAME obj=20 [Memo] prefix=[The]`).  run400 prints it
back with the capital intact, everywhere -- the surface clause of an examine,
the take-all list, and the last-resort refusal:

    x desk
    ... Your Coffee Mug and The Memo are on Your Desk, and inside is ...

    take all from desk
    You take Your Coffee Mug, ... Your Badge and The Memo from Your Desk.

    burn memo
    I don't understand what you want me to do with The Memo.

Scarier printed `the Memo` in the first two of those, because
`lib_print_object_np()` carried a `the` branch alongside its `a`/`an`/`some`
ones and re-emitted the article in lower case.

**The Runner has no such branch.**  Its normalizer is `tense`
(`Proc_21_13_44F474` @44F474), reached from the name builder
`Proc_21_31_448710` in its normalizing mode 0 -- and note that `tense` is
handed `Prefix & " " & Short`, the whole thing, not the prefix alone.  It
tests exactly six things and returns its argument untouched otherwise:

    = "a"        -> "the"
    = "an"       -> "the"
    = "some"     -> "the"
    Left(s,2) = "a "     -> "the " & Right(s, Len(s) - 2)
    Left(s,3) = "an "    -> "the " & Right(s, Len(s) - 3)
    Left(s,5) = "some "  -> "the " & Right(s, Len(s) - 5)

`"The Memo"` matches none of them.  Pre-3.9's `tense` (run370 @420F28, run380
@425FA8, byte-identical) is the same shape with the two `some` tests missing,
so it does not rewrite `the` either -- and that branch of
`lib_print_object_np()` was already right.  3.9 is bracketed rather than read:
the run390 decompilation does not reach its normalizer, but both neighbours
leave `the` alone and the only thing 3.9 is known to have added to `tense` is
the `some` pair.

The fix is a deletion.  Falling through leaves the prefix in `normalized`,
which the tail of the function prints verbatim followed by a space, so the
branch only ever changed the author's capital -- `the foo` and `The foo` came
out of it identically apart from that one letter.

Four goldens moved, in both affected generations:

| golden | version | line |
|---|---|---|
| `xfiles_solution` | 4.00 | `... Your Badge and The Memo from Your Desk.`, `You take The Warehouse Key from Case File 10193.` |
| `cyber2_solution` | 4.00 | `You open The Teleporter.` |
| `afdfr_solution` | 4.00 | `You take The Grim Reaper's Scythe.` |
| `spirits_flight_solution` | 3.90 | nine lines -- `The Spirit Dagger`, `The Orb of Storms`, `The Amber of Flames` |

303/303 after re-blessing.

**Follow-up, settled from the P-code and re-blessed 2026-08-25.**
`lib_print_object_np()` also stripped a leading `a`/`an`/`the`/`some` off the
object's **Short name**, inherited from SCARE on the grounds that "some games
may avoid prefix and do this instead".  No Runner can do that.  The whole path
is readable end to end:

- **The object loader normalizes both fields as it reads them.**  run400
  `loc_4900E3` (`mdlSpreadTheLoad.bas:7594`) `LineInput`s the Prefix,
  substitutes a literal `"a"` for an empty one (`loc_4900EC`), then loops
  stripping trailing spaces (`loc_490100`..`loc_49015C`).  It `LineInput`s the
  Short at `loc_49016C` and loops stripping **leading** spaces
  (`loc_490170`..`loc_4901CC`), then moves straight on to the alias count.
  Nothing anywhere looks at an article.
- **The name is built in one place**, `Proc_21_31_448710`
  (`General.bas:7041`, 232 call sites), as the single string
  `Prefix & " " & Short` (`loc_4486B7`..`loc_4486CF`).
- **tense** (`Proc_21_13_44F474`, `General.bas:1728`) tests **exactly six
  things**: the whole string against `"a"`, `"an"` and `"some"`, and
  `Left(s,2)`/`Left(s,3)`/`Left(s,5)` against `"a "`, `"an "` and `"some "`.
  Everything else comes back untouched.  Callers tense the result a *second*
  time (`Battles.bas:261` then `:265`), which changes nothing.

So the only characters ever inspected are at the head of the concatenation,
and after the loader's substitution the head is **always** the prefix.  An
object with Prefix `The` and Short `the Memo` comes out of run400 as
`The the Memo`.

That loader substitution also settles the empty-prefix question that this note
used to leave open, and corrects a claim in `sclibrar.cpp`: 4.0 was said to
have no `"a"` substitution and to default in the printers instead.  It has
one, at `loc_4900EC`, exactly like run370 `@43F5DA` and run380 `@4481B2`.  It
is simply invisible, because scarier's own two defaults -- `"the "` in
`lib_print_object_np()` and `"a "` in `lib_print_object()` -- reproduce its
effect.  It is what makes run400 answer `coger fenix` in La hija del relojero
with "You take **the** Fenix de laton de el cajon." for an object whose Prefix
really is empty (`OBJNAME obj=6 [Fenix de laton] prefix=[]`): the loader makes
it `a`, and `tense("a Fenix de laton")` is `the Fenix de laton`.  The one
place the default was *missing* was `lib_print_object_raw()`, the pre-3.9
`remove` wording, which concatenated the raw prefix and would have opened its
message with a stray space; it now defaults to `a` as well.

**Four corpus lines move, and they are the proof.**  `Shadowpeak.taf` (4.00)
is the only game in the corpus that reaches this: two objects with an empty
Prefix whose Short opens with an article, `[The horn of the angels]` and
`[The dead Margo]`.  scarier used to print

    You take the  horn of the angels.
    I don't understand what you want me to do with the  dead Margo.

-- with the tell-tale double space left where the stripped `The` had been.
The Runner prints `the The horn of the angels`, and the same shape is
confirmed live in the xfiles replay, where a `The` **prefix** survives as
`The Memo`.  Three goldens re-blessed, corpus **303/303**.

### The loader's whitespace trims, same reading

Reading that loader out also settled a second, smaller thing.  Having
LineInput'd Prefix and substituted `"a"` for an empty one, run400 loops

    loc_490100..loc_49015C:  If Right(s, 1) = " " Then s = Left(s, Len(s) - 1)

stripping every **trailing** space from the prefix; having then LineInput'd
Short, it loops

    loc_490170..loc_4901CC:  If Left(s, 1) = " " Then s = Right(s, Len(s) - 1)

stripping every **leading** space from the short name.  Only those two, and
only in those two directions: a Short written `pictures ` keeps its trailing
space, and the aliases that follow are never touched.  run370 (@43F5DA) and
run380 (@4481B2) carry the same pair of loops, so it is not version-split.

Contrary to what this file said an hour ago, the corpus *does* exercise it --
fourteen objects across nine games, found by dumping every game's `OBJNAME`
lines and looking for a bracketed field that opens or closes with a space:

    Crime_Adventure.taf  obj 0 `[an arcade token ]`, obj 17/20 `[a ]`
    arlo.taf             obj 22 `[the ]`
    first.taf            obj 9 `[fresh ]`, obj 10/12 `[old ]`
    superliam.taf        obj 0 `[red ]`
    tra.taf              obj 6 `[the ]`
    ADRIFTMAS_Party.taf  obj 24 `[ bathroom door]`, 85 `[ rack]`, 154 `[ potted plant]`
    hhorror.taf          obj 51 `[ floorboards]`
    marooned.taf         obj 15 `[ trash]`

Every one of them printed as a **double space** where the name is joined:
`Also here is a pile of  trash.`, `and a  cookery book.`, `Also here is fresh
bread and fresh  turkey.`, `Also here are the  floorboards.`  The trim is now
done in `parse_trim_object_names()`, at parse time rather than in the printers,
because in the Runner it happens in the loader and so the noun matcher sees the
trimmed text as well.  Four goldens re-blessed, corpus **303/303**.

## FIXED 2026-08-25 -- what is ON an object is listed before what is IN it

`x desk`, `Adrift_22_xfiles.txt` line 9:

    run400   Your Desk is open.  Your Coffee Mug and The Memo are on Your
             Desk, and inside is Gun Holster, Your Cell Phone, Neatly Wrapped
             Gift and Your Badge.
    scarier  Your Desk is open.  Inside Your Desk is Gun Holster, Your Cell
             Phone, Neatly Wrapped Gift and Your Badge.  Your Coffee Mug and
             The Memo are on Your Desk.

Two differences in one line: the **order** (surface first, not container
first) and the **join** (one sentence, not two, and the container is not
named the second time).

The Runner does not have a container lister and a surface lister the way
SCARE does.  It has one combined helper, **`whatisinon`**,
`Proc_19_26_46A950` @46A950 (`run400/Project/mdlSpreadTheLoad.bas:21880`, body
46A058-46A94A), and its second argument is a mode:

| guard | half | at |
| --- | --- | --- |
| `arg_14 <> 0` | the ON list | `loc_46A083` |
| `arg_14 <> 1` | the IN list | `loc_46A41E` |

so mode 0 is containers only, mode 1 surfaces only, and mode 2 both.  All four
callers, and what they pass:

| caller | mode | |
| --- | --- | --- |
| `openclose` `Proc_19_3_476468` | 0 | @475852 |
| the room lister, `General.bas` | 0 | @479919 |
| `inventory` `Proc_19_70_45C304` | **2** | @45C2C8 |
| `examines` `Proc_19_87_471F94` | **2** | @471928 |

That is exactly why `open desk`, one command earlier in the same transcript,
already agreed byte for byte (`You open Your Desk.  Inside Your Desk is ...`):
the open path never sees the surface at all.  Only examine and inventory
combine.

The join is a flag, `var_9E`, set to 1 once the ON list has printed something.
The IN half tests it **first**, before any format choice (`loc_46A786`):

    loc_46A786:  If var_9E = 1 Then
    loc_46A795:      MemVar & ", and inside is "
    loc_46A79D:      GoTo loc_46A7E0            ' the plain list loop

-- no container name, no `pspace`, no new sentence.  The single closing `.`
is appended once at the very end of the sub (`loc_46A8C6`, and only if
anything was added at all), which is why the ON clause carries no period of
its own when an IN clause follows it.

**The count-1 and count-2 arms are unreachable when a surface listed.**  The
`"<a> is inside <cont>"` branch is guarded `var_98 = 1 And var_9E = 0`
(`loc_46A49E`-`loc_46A4AE`) and the `"<a> and <b> are inside <cont>"` branch
`var_98 = 2 And var_9E = 0` (`loc_46A607`-`loc_46A617`).  Each of them then
contains an inner `If var_9E = 1` arm (`loc_46A4F1`, `loc_46A66C`) that can
never run -- leftovers of the VB source.  Taken literally, a surface listing
forces `", and inside is <list>"` **whatever the in-count**, and that is what
is implemented.

Ported in `sclibrar.cpp` as `lib_list_in_on_object()`, with
`lib_list_in_object_joined()` for the joined wording; `lib_list_on_object()`
gained an "omit the period" argument and `lib_list_in_object()` a "joined"
one.  Both mode-2 call sites now go through it -- `lib_describe_object()` and
the inventory loop -- while the open handler keeps calling
`lib_list_in_object()` directly, as run400's mode 0 does.

**Pre-3.9 is excluded.**  There is no combined lister there at all: run380 has
`whatisin1` @4297AC and `whatisin2` @42998C as separate subs, and its
`examines` @43D5EC carries its listing inline as an either/or on one field --
`loc_43D07A` prints `"  Inside <obj>"` when it is 1, `loc_43D0D0` prints
`"  On <obj>"` when it is 2, never both.  And the literal `", and inside is "`
is absent from `run370.exe` and `run380.exe`, appearing first in `run390.exe`
-- the same boundary as `" is inside "` and `" is on "` (counted in the four
binaries as UTF-16LE, 2026-08-25).  So a pre-3.9 game keeps the older
container-then-surface pair of sentences.

Three goldens move, corpus back to **303/303 PASS**:

| golden | in-count | new wording |
| --- | --- | --- |
| `xfiles_solution` | 4 | `... are on Your Desk, and inside is Gun Holster, ...` |
| `ADRIFTMAS_Party_solution` | 2 | `The suitcase is on the wardrobe, and inside is a leather jacket and an assortment of shoes.` |
| `yonastoundingcastle_solution` | 1 | `Ye olde desk clutter is on ye alchymist's desk, and inside is ye magic crystal.` |

Only the first is *measured*; the other two are the unreachable-arm cases and
rest on the disassembly alone.  **Probe still wanted** once the desktop is
unlocked: one .taf with a desk that is both a surface and an open container,
one object on it, and one, two and three objects inside across three cells,
`x desk` each time.  If the count-1 cell answers `A crystal is inside the
desk.` as a second sentence rather than `..., and inside is a crystal.`, the
inner arms are live after all and the guard order in `lib_list_in_object()`
has to move.

## OPEN 2026-08-25 -- `burn memo`  (probe built, waiting on Wine)

**run400 refuses a task Scarier runs.**  Task 24, `Burn %object%`, restr=2,
mask `#A#`:

    RESTR type=0 var1=1 var2=3 var3=0    "any object visible to the player"
    RESTR type=3 var1=0 var2=2 var3=-1   "the player is alone"

`SCR_TRACE_TASKS=1` shows both PASS in Scarier and the task running, printing
its CompleteText (`You incinerate the The Memo with a Zippo ...`).  run400
answers `I don't understand what you want me to do with The Memo.`, which is
`generaltasks`' end-of-turn fallback at `loc_48B1F5`
(`mdlSpreadTheLoad.bas:33899`; the other copy of the string is in `therest`,
`Proc_19_85_489F4C`, at `loc_488706`).  So run400 refused the task **and
printed nothing at all**.  It is not the empty-CompleteText refusal (see Open
leads) -- the CompleteText is not empty.

**The FailMessages narrow it to restriction 2.**  `scdump.cpp`'s RESTR line now
prints each restriction's `FailMessage`, because that is what decides how a
failure *looks* from outside:

    RESTR type=0 v1=1 v2=3 v3=0 fail=[There's nothing here to burn!]
    RESTR type=3 v1=0 v2=2 v3=-1 fail=[]

Whatever rule run400 uses to pick which failing restriction's message to print
-- first-failing, first-failing-with-a-message, last-failing -- they all agree
when only *one* restriction fails, because then there is only one candidate.
So if restriction 1 had failed, `There's nothing here to burn!` would have been
printed.  It was not.  Restriction 1 passed in run400; the silence is
restriction 2's empty message, or the task never matched at all.

**No mechanism found in the listings.**  Ruled out, in order:

- the feed -- every command in `Adrift_22_xfiles.txt` lines 5-21 echoes
  correctly, `burn memo` included;
- an NPC actually being present -- no NPC starts in Your Office (room 0); Ruth
  is prose only; the 16 NPC start rooms are 3, 15, 9, 9, 9, 9, 25, 24, 24, 5,
  28, 34, -1, 30, 17, 19;
- a hidden-NPC collision on room 0 -- run400's player room is 1-based
  (`Proc_19_27_4430F0`, `mdlSpreadTheLoad.bas:22660`, returns a room index
  unchanged while it is `< NumberOfRooms + 1`, which is only right for 1-based
  rooms; exit `Dest`s go into `unk_409011.global_0` raw), in the same space as
  NPC `global_14`, so a nowhere NPC at 0 can never match;
- an off-by-one in run400's Alone loop -- `loc_4812F0` is `var_86 = TRUE; For i
  = 0 To NumberOfNPCs-1: If playerroom = NPCs(i).global_14 Then var_86 = FALSE`,
  semantically identical to Scarier's `!(npc_count_in_room(playerroom) > 1)`;
- task 24 being spent -- xfiles has **no** `ACT type=5` (execute task) anywhere,
  and all six of its events are `starter=3`;
- another task claiming the command -- task 25's `Burn *Car` cannot match.

Task 24 carries the game's **only** `type=3 Var2=2` restriction, so nothing
else in the transcript can corroborate or refute it.

**The probe is built and staged**:
`harness/make_400_burnprobe.py` -> `p4BURN.taf`, with
`~/adrift-battle/runner/wine/cmdfile_burn.txt`.  Two rooms, one trinket each
side, and exactly one NPC, which starts **nowhere** (`StartRoom` 0) as xfiles'
NPC 12 does.  Thirteen cells: a restriction-free baseline, a restriction-free
`burn %object%` (is the verb intercepted before task matching?), each of
xfiles' two restrictions alone, xfiles' exact two-restriction shape, and
sure-failing twins (`no object is visible`, `the player is not alone`) in every
combination of empty and non-empty FailMessage.  Scarier's own answers, which
are what the Wine run is measured against, are:

    pa coin   PA PASS.        burn coin  BURN PASS.
    pb coin   PB PASS.        (restriction 1 alone)
    pc coin   PC PASS.        (restriction 2 alone -- THE test)
    pd coin   PD PASS.        (xfiles' exact shape)
    pe coin   PE PASS.
    pf coin   PF FAIL.        pg coin  PG FAIL.
    ph coin   PH FAIL A.      pi coin  PI FAIL B.
    pj coin   I don't understand what you want me to do with the coin.
    pk coin   PK FAIL A.
    pl coin   I don't understand what you want me to do with the coin.

`pb`/`pc` say which of xfiles' restrictions run400 disagrees with; `pa`/`burn`
say whether matching itself is the problem; `ph`/`pi`/`pj`/`pk` decode the
message-selection rule as a by-product (Scarier takes the *first* failing
restriction's message even when it is empty -- `restr_lowest_fail`,
`screstrs.cpp:906`, consumed at `:1176`).

The command file runs the alone cells in the start room, then again after `e`
and `w`.  That round trip is deliberate: the .taf stores the header's
`StartRoom` 0-based but every exit `Dest` 1-based, so a Runner that failed to
normalise the header would have the player at room 0 until the first move --
colliding with the nowhere NPC and making "alone" false in the start room only.
xfiles burns the memo in its start room.  If `pc` fails before the round trip
and passes after it, that is the whole bug.

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
  - **3.7** `arlo.taf` / run370 -- cmd 34 now byte-exact against `Adven_6_arlo.rtf`.
  - **3.9** `lair-of-the-cybercow.taf` / run390 -- `Adrift_35_cybercow.txt`.  Room 7's
    alt 0 is method 2, unconditional, "The end of a rope dangles here."; alt 1
    is method 1 on task 31 with M1 and M2 both empty.  Before `untie rope` the
    Runner prints the rope line; after it, it does not.
  - **4.0** `unravel.taf` / run400 -- `Adrift_34_unraveling_god.txt`.  Every "Outside the
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
  **FIXED 2026-08-25** -- the Runner's normalizer has no `the` branch at all;
  see "a \"The\" prefix keeps its capital" below.

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


## FIXED 2026-08-24 -- the object `seen` model (was PARKED on `scarier-seen-flag-port`)

The xfiles replay (`Adrift_22_xfiles.txt`) left one unexplained divergence: run400
answers `take knife` in Garage 5 with **"Take what?"** where Scarier takes the
knife.  Task 7 "Use Key" carries the player into Garage 5 with `ShowRoomDesc`
off, so no room description prints, and the knife is a dynamic that has been
lying there since the load.  The Runner's parser will not resolve a noun to an
object whose `seen` byte is clear, and nothing on that path ever sets it.

Scarier, by contrast, used to mark *everything* in the player's room seen on
every turn (`obj_turn_update`).  That was the bug.  The port was written on
`scarier-seen-flag-port` (commit `de7bffcc`) and landed on master on
2026-08-24, with fifteen walkthroughs re-derived for it (below).

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

### How it was settled

The parked note said only a live run400 probe could decide it, and the console
was locked for the whole session.  It never needed one: **the answer was
already in the archived transcripts.**

- **xfiles, `Adrift_22_xfiles.txt` lines 92-93.**  The exact case the branch changes,
  measured live months ago and never read closely:

        take knife
        Take what?

  Task 7 "Use Key" carries `ShowRoomDesc = 0`, the Small Pocket Knife (object
  31, `InitialPosition` 11 = room 7) is lying loose on the floor of Garage 5,
  and the very next command, `out`, moves normally -- so the player really is
  standing in the room and the knife simply does not exist to the parser.  The
  same transcript answers `take directions` and `get in the van` with "Take
  what?" too.
- **humbug, `Adrift_29_humbug.txt`.**  A command-for-command replay of the first 832
  of `cmdfile_humbug.txt` against both master and the branch found **exactly
  one** line where they differ -- `X teeth` at command 723 -- and the branch is
  the one that matches the Runner:

        RUNNER: Nothing Special.
        MASTER: The trouble with being a dentist is that you still have to ...
        BRANCH: Nothing Special.

  Grandad's teeth are a part-of-character static of an NPC the player has never
  had described, so they are unseen and the examine falls through to the
  default.

Two independent live confirmations, zero contradicting evidence, and the
P-code re-read above (`obhere`'s only two `(48) = 1` writes are in its
part-of-character branch; the player-move sweep at `loc_48CA32` is gated on
`global_24 = 1` **and** the static presence array) all agree.  The Renegade
Brainwave probe was never needed -- and on the branch it behaves exactly as
predicted:

    > take crowbar  =>  Take what?
    > look          =>  Yew tree  You stand under the spreading shadow of ...
    > take crowbar  =>  You take the crowbar.

So the note's own decision rule applied: *"Take what?" -> the branch is right,
and the 15 walkthroughs need re-deriving.*

### Landing it

Cherry-picked onto master as `scarier-seen-flag-land`; six files
(`scevents.cpp`, `scgamest.cpp`, `sclibrar.cpp`, `scobjcts.cpp`, `scprotos.h`,
`sctasks.cpp`).  The 15 regressions reproduced unchanged on top of the new
master, and each was repaired by inserting the reveal command a player would
actually type before the first reference:

| row | repair |
| --- | --- |
| renegade_brainwave | `look` before `take crowbar` |
| xfiles | `look` before `take knife` (the measured case) |
| mr_smith | `look` before `take gold key` |
| spirits_flight | `look` before `get cake` |
| spam | `look` first (`DispFirstRoom` off) |
| wreckage | `look` before `take repairbot` |
| imagination | `look` first (`DispFirstRoom` off) |
| valley | `look` before `get gloves` |
| to_hell_in_a_hamper | `look` before `put ear-trumpet in dog's ear` |
| deadman | `look` before `get all` |
| 3monkeys | `look` before `get stone`, **minus** the `z` that followed |
| colony | `look` + one `take all` **replacing** two separate takes |
| lair | `look` before the wake-up `get all`, plus two more `up` |
| wonderwombat | three `look`s, and the maze re-measured 12 -> 15 norths |
| humbug | **no route change** -- only `X teeth` moved |

Two of them could not afford the extra turn and had to stay turn-for-turn
identical: colony's alien kills in two hits (the old route died on the shifted
turn), and 3monkeys' mandrill corners you one turn later.  Folding an existing
turn into the reveal fixed both.

`lair` is the one worth reading.  TASK 313 (`open coffin` in the dream, room
31) moves the cobalt key to room 21 *while the player is still in the dream*,
so no reveal fires -- a task object move only reveals into the player's
**current** room.  The wake-up narration prints no room description, so without
a `look` the `get all` silently misses the key, the chest at the end cannot be
opened, and the game finishes at 221 instead of 226 while still printing its
win marker.  That is exactly the class of quiet loss the marker guard cannot
catch, so **check the score, not just the marker, on every seen-model repair.**
The added turn then desynced the random ruined-stairs collapse, which is why
that row now climbs four times.

Corpus after landing: v4 **303/303 PASS**, a5 unchanged (MATCH 180, DIVERGE 17
all at baseline, NOSCRIPT 2).

The three follow-up probes the parked note listed are now moot for `SPAM.taf`
and `Colony.taf` -- both re-derived and green, and Colony's two pre-existing
"Take what?" lines at golden 218/222 are unchanged, which is the right answer
for dynamics in a described room.  `1HRGAME.taf` (`x little table` then `take
bubbles`) is still worth a live check if a console ever comes back, but it
exercises the `examines` path that was already ported.

## FIXED 2026-08-24 -- `where` / `find` / `locate`, from P-code alone

No golden had ever run this command with an argument the Runner answers
positively, so upstream SCARE's wording had never been checked.  It is wrong
in four places.  Unusually, run370 and run380 decompile to readable VB here
(`run380/run380.bas`, `where`-for-objects at @436F4F, `where`-for-characters at
@440B3E), so the whole handler can be read rather than reconstructed, and
run390/run400 confirm every literal.

- **"<Name> is <lowercased room name>.", never "<Name> -- <Room Name>."**
  The lowercasing is a real `LCase()` call -- `ImpAdCallFPR4 LCase()` at
  run380 dasm @00040B94, and run400 @468143 (objects) / @47FD19 (characters).
  The string `" -- "` occurs in **none** of the four binaries.
  The character branch uses a literal `" is "`; the object branch calls
  `isare()`.
- **"is carrying", not "is holding"** for an object an NPC is holding.  The
  carrying/wearing pair sits together at run400 @467FC1/@46802E and run380
  @4372E6/@43736B.  "holding" was upstream's invention.
- **The object branch drops the "that"**: `"somewhere " & person(5) &
  " haven't been yet."` (run400 @4681B0, run380 @4375D3), where the character
  branch of the same command says `" is somewhere that " & person(5) &
  " haven't been yet."` (run400 @47FD89, run380 @440C11).  An inconsistency of
  ADRIFT's own that all four Runners carry.
- **The smart-alec clause was `#if 0`'d out** upstream.  All four Runners print
  it when the NPC is standing in the player's room, and there is **no comma**
  before "silly" -- the literals are `"  (Right next to "` and `" silly!)"`
  with the perspective pronoun spliced between (run380 @00040BCE/@00040BE2,
  run400 @47FD56/@47FD6A).

`viewroom` opens by copying `room(0)` into `room(4)` (run400 @472053) before
the alt selector runs, so run400's `room(4)` -- the field both branches read --
is the alt-resolved name, i.e. exactly what `lib_get_room_name()` returns.  The
new `lib_print_room_name_lower()` folds that.

Corpus movers: `TheADRIFTProject` ("DARWIN is central communications core.")
and `ticket` ("Young Girl is waiting room."), both re-blessed.  Nothing else in
the v4 corpus reaches these handlers, so nothing else moved.  The lowercasing
reads badly on games that name rooms in title case -- that is what the Runner
does.

### Two leads this turned up

- **`isare()` is not `obj_appears_plural()`.**  The real helper decompiles
  cleanly at run380 @428EAC:

      r = " is "
      If Left(prefix,4) = "some" And Right(name,1) = "s" Then r = " are "
      If Right(name,1) = "s" And Mid(name, Len(name)-1, 1) <> "u" Then r = " are "
      If prefix = "a"  Or Left(prefix,2) = "a "  Then r = " is "
      If prefix = "an" Or Left(prefix,3) = "an " Then r = " is "

  (the first test is redundant; the net rule is *plural iff the short name ends
  in `s` not preceded by `u`, and the prefix is not an `a`/`an` article*).
  SCARE's `obj_appears_plural()` in `scobjcts.cpp` adds a condition the Runner
  does not have: it returns singular for an **empty** prefix, where `isare`
  returns " are ".  Not changed here -- `obj_appears_plural()` feeds 24 call
  sites, several of which the Runner answers with a literal rather than
  `isare`, and VB6's default `Option Compare Binary` makes the `"a"`/`"s"`
  tests case-sensitive in a way SCARE's `scr_compare_word()` is not.  Wants a
  live probe on an object with a blank prefix and a plural short name before
  anyone touches it.
- **"<Name> is dead!"** -- **FIXED, see the section at the end of this file.**
  run390 @459D74 and run400 @47FDB9 answer `where` for
  a character whose room field is `&HFB` (which is **-5**, sign-extended, not
  251 -- `LitI2_Byte`) with that line; run370 and
  run380 have no such branch and no such string.  251 is the battle system's
  corpse marker (run400 sets it at Battles.bas @44B127, right after the
  " falls down, dead." line, and the walk ticker skips every NPC carrying it at
  @4685B6).  Scarier has no dead marker at all -- `battle_npc_die()` hides the
  corpse in location 0, which also means "hidden" -- so this stays with the
  parked *dead NPC still walks* lead above.  Both want the same fix: a separate
  dead flag, because the Runner does keep ticking the walks of a merely hidden
  NPC.

---

## FIXED 2026-08-24 -- a battle-killed NPC is dead for good (the parked *dead NPC still walks* lead)

Closes both halves of the pair above: the `where` answer "<Name> is dead!" and
the corpse that kept walking.  Done from P-code alone -- the console is still
locked, so no live Runner was needed or available.

### What the Runner does

`killchar` -- run390 `run390_3.bas` `@42D410`, run400 `Project/Battles.bas`
`@44B13C`, the same routine either side of the 4.0 rewrite -- does three things
in this order:

1. drops everything the NPC held or wore into the room it died in;
2. **if it has a KilledTask, runs it**, then suppresses the default
   " falls down, dead." line (run390 gates on `var_90(124) > 0` and dispatches
   through the matcher: `MemVar_468118 = tasks(idx-1).cmd(0)` then `tasks(1)`;
   run400 gates on `var_90(206) > 0` and calls the task directly.  `var_90(206)`
   is the **KilledTask index**, not a lives counter -- that was the open
   question from the previous pass and it is now answered);
3. **unconditionally** stamps the NPC's room field:

       loc_42D3FA: push &HFB 'Byte
       loc_42D3FC: var_90(12) = from_stack_1        ' run390, field 12
       loc_44B127: push &HFB 'Byte  ->  var_90(14)  ' run400, field 14

`&HFB` here is **−5**, not 251: it is pushed by `LitI2_Byte`, which
sign-extends.  Same family as the `push &HFF` = −1 already recorded in
`adrift-decompile-signed-byte-literals`.  −1 is the Runner's "hidden"; −5 is
its "dead".

Three independent sites confirm the field is the NPC's room in each build:
the walk-to-hidden write `var_16C(12) = &HFF` (run390 `loc_45ABBA`), the
compare against the player's room `If (var_16C(12) = unk_4082E6.global_0)`
(`loc_45AC74`), and the `where` compare at `loc_459D60`.

Exactly two readers of −5:

* **the walk ticker breaks out of the walk loop** --
  run390 `loc_45A4BC: If (var_16C(12) = &HFB) Then GoTo loc_45ABD0`, and
  `loc_45ABD0` is *after* `Next var_2D4` but before the per-NPC tail, so the
  same NPC still goes through `charbattle`; run400 `loc_4685B6 -> loc_468D61`,
  likewise past `Next var_A0` and before `Next var_94`.  A **break**, not a
  continue -- that distinction was checked, because it decides whether the
  walk's step counter keeps advancing.
* **`where <name>`** -- run390 `loc_459D74`, run400 `@47FDB9`.

And the ADRIFT 4 manual (`~/adrift-battle/runner/manual.txt` l. 2659) states it
outright:

> The default behaviour for when a character is killed (i.e. its stamina
> reaches zero) is for the character to disappear, and any objects it was
> holding are moved to the current room.  Typically you would want to create a
> dead body and have some message notifying the player of the recently
> deceased.

### The port

`NPC_DEAD_LOCATION = -5` in `scgamest.h` plus a `dead` flag on
`scr_npcstate_t`, cleared by `gs_set_npc_location()` so any later move revives:

* `scbattle.cpp` -- `battle_kill()` sets location 0 **and** the dead flag, in
  that order and *after* the KilledTask, matching killchar;
* `scnpcs.cpp` -- `npc_tick_npc()` breaks out of the walk loop on the flag;
* `sclibrar.cpp` -- `lib_cmd_locate_npc()` early-returns "<Name> is dead!";
* `scserial.cpp` -- saves write `NPC_DEAD_LOCATION` and restore special-cases it
  ahead of the range guard, so `.tas` round-trips.

### Fallout: The Town of Azra's economy was never real

Azra (3.90 build) is designed as a renewable hunting sandbox: tasks 19
`#banditkristdies` and 37 `#deerdies` each drop a corpse object, move their NPC
to hidden, and restore its stamina (+30 / +20), plainly expecting the looping
walk (`step0 dest=0`) to bring it back.  It never did in the Runner, so the
author's intro remark -- "you can continue to kill more bandits and sell more
carcasses to gain more money, of course. :)" -- is untested, and **goal 5, the
$7,500 house, is unreachable**: one bandit purse plus one $500 carcass tops out
at $959.68, and Stealth alone costs $800.

The old golden ran 505 turns and sold fifteen carcasses; that route existed only
because Scarier let the corpse keep walking.  Re-derived at `SCR_SEED=26` to
**58 turns**, goals 1/2/3/4/6, wealth $159.68.  Measured en route: 10 attacks
kill the bandit and 4 the deer, and overshooting is free -- `attack` at an
absent or dead NPC is a parser rejection that costs no turn, so the blocks are
self-syncing (11–15 bandit attacks give a byte-identical transcript).

`notes/The_Town_Of_Azra_walkthrough.md` rewritten to match; the harness row
carries the measurement.  Shadowpeak's two routes were re-derived in the same
pass (corpses no longer draw a walk random each turn, which re-threads every
downstream walker and battle roll): `shadowpeak_killwraith` 710 -> **735/790**.

v4 corpus after the port: **303/303**.

## CLOSED 2026-08-24 -- the bracket checkbox governs three more lines

The humbug cmd 254 lead ("Scarier prints `(Getting off the stool first)`,
run400 prints nothing") was logged as an engine divergence.  It is not one.
It is rule 1 of *What to do with a diff* -- rule out the Appearance
checkboxes first -- and it was skipped.

**Options -> Display & Media... -> Appearance -> "References in brackets"**
(registry `showbrackets`) does not gate only the pronoun echo already written
up in `RUNNER_TESTS_TODO.md` §4.  From 3.9 on it also gates the mover's two
bracketed lines:

| Runner | `(Getting off X first)` | `(Standing up first)` | gate |
| --- | --- | --- | --- |
| run370 | `loc_42303C` | `loc_423078` | none -- no such menu |
| run380 | `loc_428244` | `loc_428280` | none -- no such menu |
| run390 | `loc_431911` | `loc_4319A0` | `m_showbrackets.Checked`, by name |
| run400 | `loc_450339` | `loc_4503BF` | `MemVar_4942BA = 1` |

`MemVar_4942BA` is `showbrackets`: run400 writes it to the registry under that
key at `4679A1` (`Form1.frm` 6289), and it is the same byte the pronoun echo
is already known to hang on -- `48A095`, the `Sub_20_62` site recorded in §4.
The whole set of nine `MemVar_4942BA` tests in run400 is: six pronoun echoes
(`him`, `he`, `her`, `she`, `it`, `them`, `Proc_19_49_461F38`), the
`ask about`/`talk about` rewrite (`47F15A`, `47F21D`), the general reference
echo (`48A095`), and these two.  Nothing else.

The checkbox starts unticked on every launch and is never restored from the
registry (run400 has a `SaveSetting` for `showbrackets` and no `GetSetting`),
so a default Runner prints neither line.  **Ported**: `lib_go()` in
`sclibrar.cpp` now prints both only below `TAF_VERSION_390`.  43 lines went
across 29 rows -- every one a bracket line, every diff a pure deletion, no
pre-3.9 row touched.  Corpus 303/303.

### The same finding says 7f7349c7 over-reached

`7f7349c7` ("drop the bracketed pronoun echo -- no Runner prints one") is
right about the default and wrong about the mechanism, and the mechanism is
what the commit message argues from.  The Runner *does* print a pronoun echo;
it prints it in **round** brackets, which is why searching run370/run380 for a
`[` literal found nothing and read as proof of absence.  What it really shows
is that upstream SCARE's square brackets are not the Runner's.

run370 `Sub Form1.its` @0002CA9C prints, for each of seven pronouns
(`him`, `he`, `her`, `she`, `it`, `them`, **`one`** -- 4.0 has no `one`),
`"(" & antecedent & ")"` followed by a newline, and it is **not gated**: 3.7
has no Appearance menu.  run380 @000326B4 is the same routine.  The antecedent
is the NPC's Name for the four personal pronouns (`MemVar_4460B4`, seeded
`"Nobody"`) and `tense(Prefix) & " " & Short` for the object ones
(`MemVar_4460AC`, seeded `"Absolutely nothing"`) -- so the pre-3.9 Runner
answers `drop it` with `(the paper aeroplane)`, not with the rewritten
command.

Corpus exposure of the over-reach is one row: `wrecked` (3.80) lost 25 lines
in that commit.  The other thirteen re-blessed rows are 3.90/4.00 and were
right to lose theirs.  Restoring the pre-3.9 half means writing a *new* echo
(round brackets, the antecedent alone, no italics), not reverting.  Not done
here.

## FIXED 2026-08-24 -- the ADRIFT 4.0 output filter (the humbug `Okay.  Okay.`)

The lead was humbug (4.00) command 217 `Put sweet on plinth`:

    run400   Okay.  Okay.  I put the sweet on the plinth.
    Scarier  Okay.  I put the sweet on the plinth.

Neither "Okay." is the author's.  Task 80's CompleteText is a bare "I put the
sweet on the plinth." (`SCR_DUMP_TASKS=1`, which now dumps CompleteText,
AdditionalMessage, RepeatText and ReverseMessage for exactly this reason), and
the game carries one ALR

    [I put ] -> [Okay.  I put ]

whose replacement contains its own original.  That is the only shape in which
the number of times the Runner applies an ALR is observable at all, which is
why it took four probe games to pin down.

### The rule, as measured

Four probe games were built with `harness/make_400_alr*probe.py`,
`make_400_walkcountprobe.py` and `make_400_varfreezeprobe.py`, packed with
`taftool.py`, and replayed in Wine.  Each script's docstring carries its own
cells and the transcript they answered with; the model they add up to is:

1. **A walk of the ALR list** is a full length-descending pass, repeated until
   a pass changes nothing.  An ALR whose replacement contains its own original
   is retired for the rest of the walk it fired in -- but only that walk.
   *(run400 `qqAAA.`, `QQ.`, `done.`; `Adrift_2/3/4.txt`.)*
2. **3.9 is exactly one plain pass** of that list, with no repeat and no
   retirement.  *(run390 `qAAA.`, `PPPP.`, `VVVV.`; `Adrift_5.txt`.)*
3. **A 4.0 turn walks its whole accumulated buffer once at the end of every
   task that completes**, and once more at the flush.  "Every task" means
   every one: tasks an action executes, at any nesting depth, and tasks an
   event's `TaskAffected` runs.  Refusing to repeat a non-repeatable task is
   not a completion and gets the flush walk alone.  *(`Adrift_13/14.txt`:
   `O qqqqqqball.` for four completions plus the event's plus the flush.)*
4. **That pass interpolates variables too**, so each one freezes the values
   then and there.  A task's own change-variable action still reaches text the
   task has already printed -- so 4.0 must *not* checkpoint the buffer before
   a variable change, the way pre-4.0 does -- but a task run by an action
   freezes the text before any action after it runs.  *(`Adrift_15.txt`:
   `B n=9` with the change alone, `A n=5` with a silent task run first.)*

### What it cost, and the one that had to be measured on a real game

Thirty-one goldens moved, all of them consequences of one of three shapes: a
self-containing ALR multiplied once per completing task (sophie's
`[north] -> [north (to the farmhouse)]`, shardsofmemory's
`[I move north.] -> [I move north.<br>]`), a variable frozen one step earlier
(ticket's clock, unauthorized_termination's charge level, the_town_of_azra's
turn counter -- its win marker moved 27 -> 26), and tokens that simply resolve
now where the golden had carried them raw (cursed's `[windmessage=Rixomas]`,
ticket's "telling off about the .").

3monkeys was the one that could not be blessed on a probe's word.  Its "chimp"
task prints `[CHIMPSIGNAL=%signal_to_chimp%]` -- an ALR original built out of
a variable -- then runs a silent bookkeeping task, and only then increments the
variable.  Rule 4 says the text freezes at `CHIMPSIGNAL=0`, no ALR has an
original for that, and the player is shown the raw token while the prose the
author wrote for `=1` arrives one signal late.  That is a bad enough outcome
for a well-liked game to be worth a run of its own, so it got one: run400, the
solution's first 36 commands, every command echoed (`Adrift_16.txt`).

    chimp, get coconut
    CHIMPSIGNAL=0
    The chimpanzee scans the ground immediately near his feet, but there are
    no fallen coconuts to be seen.

The Runner prints it.  Measured, not argued.

### Where it lives

`pf_filter_internal()` and `pf_replace_alrs()` in `scprintf.cpp` hold rules 1
and 2; `pf_refilter()`, called at the end of `task_run_task_unrestricted()` for
4.0 games, holds 3 and 4, and the pre-4.0 checkpoint in
`task_run_change_variable_action()` is now gated `< TAF_VERSION_400`.  4.0 task
actions no longer transfer the turn's buffer out and prepend it back; they hide
it behind a barrier instead (`pf_hide_prefix()` / `pf_reveal_prefix()`), so the
paragraph-spacing helpers still see what they saw before while the filter sees
the whole buffer.  Suite: **303/303 PASS**, and the ADRIFT 5 corpora are
unchanged.

### Left unmeasured

- Whether 3.9 also drops the pre-variable-change checkpoint.  The corpus cannot
  see it either way, so the gate keeps the old behaviour there.
- What run400 does with a mutual `A -> B` / `B -> A` ALR pair.  The repeat loop
  is bounded by the ALR count so it terminates; that bound is a guard, not a
  model of the Runner.

## FIXED 2026-08-25 -- a non-looping walk with StartTask 0 never runs before 4.0

`Adrift_37_melbourne_beach.txt` again, this time for the walk itself rather
than its announcement.

*Melbourne Beach* (3.90) gives Judy a **six-stop, non-looping** walk with
StartTask 0 -- Kitchen 10, Eating area 10, Den 5, Judy's bedroom 15, follow 5,
Outside den 1. Scarier walked her: room 8 on turns 1-10, 14 on 11-20, 5 on
21-25, 3 on 26-40 (`SCR_TRACE_JUDY=1` confirms the suffix-sum arithmetic
exactly). run390 does not. In its transcript Judy is still standing in the
Kitchen at turn 18, and all twenty `give trumpet to judy` typed in her bedroom
on turns 36-55 are refused by task 17's third restriction, "You can't do that
in your present company." (the restriction is *player in the same room as NPC
2*).

Those two observations cannot both be a phase shift. Judy in the Kitchen at
turn 18 needs the walk to start at `s` with 9 <= s <= 18; the bedroom window is
then `s+25 .. s+39`, which always intersects [36, 55]. There is no `s`. The
walk never starts at all.

That matches the P-code. Nothing in run370/380/390/400 seeds a walk counter at
game start; the only thing that ever puts a counter on a walk no task started
is the ticker's *restart a spent walk* branch, and pre-4.0 that branch is
gated on the walk **looping** (run380 441389, run390 45A585). 4.0 made it
unconditional -- which is exactly the version split Scarier already had, but
far too narrowly drawn.

`npc_start_walk_is_390_noop()` used to be `stops == 1 && !loop`, with a comment
naming this very game as the counterexample that proved it could not be wider.
The comment was wrong and the measurement says so: the rule is simply `!loop`,
which subsumes the old one-stop probe result as a special case.

**Cost:** one golden. `melbourne_beach_solution.txt` waited out Judy's walk
with two twenty-turn `give` loops in her bedroom; it now hands her the trumpet
and the music in the Kitchen, where she stands for the whole game, and is 44
lines shorter. Score unchanged, 38/41. Suite **303/303 PASS** -- no other row
in the corpus moved, which is the strongest evidence the wide rule is right.

