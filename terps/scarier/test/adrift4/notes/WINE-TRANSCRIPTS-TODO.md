# TODO: Runner-transcript verification of the v4 walkthroughs

**Compacted 2026-09-06.**  This file had grown to 6800 lines of dated
per-game write-ups.  Everything measured up to 2026-09-05 is summarised
below; the full text of every FIXED / CLOSED / DIAGNOSED section and every
per-game drive log is in git history:

    git log -p -- terps/scarier/test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md

(commit `45e20596` is the last pre-compaction version).  The canonical
write-up of every engine change is still the comment block above the game's
row in `harness/run_v4_walkthroughs.sh`, and the memory notes it cites.

**What this is.**  The *Professor Von Witt* exercise generalised: replay a
wired walkthrough command for command in the real Windows Runner under Wine,
diff the Runner's own transcript against Scarier's, fix the engine where they
disagree (never the walkthrough), re-bless the golden, and write the evidence
into the harness row comment.  Scope is all four file versions
(3.70 / 3.80 / 3.90 / 4.00); only the Runner binary and the capture flow
change with the version.  State as of 2026-09-05: the 3.70 (2), 3.80 (10) and
3.90 (54) tables carry a measured or deferred verdict on every row; 74 of the
124 rows in the 4.00 table do.  Suite baseline 428 PASS / 0 FAIL.

## Why these games and not others

A walkthrough can only be diffed turn for turn against the Runner if the game
is deterministic along that path.  The screen, run over the whole harness on
2026-08-23:

    export SCR_SEED=97      && harness/run_v4_walkthroughs.sh
    export SCR_SEED=424242  && harness/run_v4_walkthroughs.sh

Rows whose golden is identical under both seeds are the candidates (66
pre-4.0, 124 at 4.00).  Two refinements learned since:

- **Double-seed invariance cannot see a roll whose texts never reach the
  route.**  `Captive.taf`'s event 12 really rolls 3..5, but its texts only
  print in rooms the route has left.  The `SCR_DUMP_TASKS=1` dump can tell:
  a spread in `time1..time2` or `start=lo..hi` is a hazard only if the
  event's texts can reach a room the route visits while it runs -- and only
  when `hi - lo >= 2`, because ADRIFT's rolls are exclusive of the upper
  bound (`start=1..2` always draws 1).
- **RNG-timed lines do not by themselves make a game unmeasurable.**
  `the_pk_girl` was made replayable by brute-forcing one randomly placed NPC
  out of the way (`cmdfile_pkhunt.txt`); `humbug` was fully replayed with
  `#save` / `#restore` checkpoints, reading each randomised secret off the
  transcript.  `xfiles` and `wamk` (RNG-timed event lines) and the `great.taf`
  car chase (four events with random lengths) remain argue-from-P-code plus
  targeted-probe territory.

Priority within the list is NPC **walk** count first, then length: every
Professor-class divergence lived in walk phase, arrival announcements or
walker presence lines.

## Which Runner to launch

All four live in one prefix, `~/adrift-battle/runner/wine/` (`run370`,
`run380`, `run390`, `run400`; games in `pfx/drive_c/adrift/`).  The .taf
header bytes 8-10 give the version; `harness/screen_wine_candidate.py`
prints it.  Launch a **fresh process per measurement** -- Verbose resets OFF
on every launch.  3.7/3.8 have no startup event tick and no administrative
turns; both are gated `>= 3.90` in the engine.

## Capturing a transcript

- **3.9 / 4.0**: Adventure menu -> Start Transcript writes `Adrift_N.txt`
  live; archive it as `Adrift_N_<slug>.txt`.  `measure.sh <game>` does the
  whole drive: forces Verbose and the five Appearance checkboxes in
  `pfx/user.reg`, Sound OFF, answers InputBox prompts from `POPUP_ANSWERS`,
  dismisses `PRE` startup pauses (`PRE_SLEEP` between them), starts the
  transcript, then hands the feed to `drive_ckpt_safe.sh` (`#save NAME` /
  `#restore NAME` / `#sleep N` directives, liveness guard, accent-dropping
  `type_line.py`).
- **3.7 / 3.8**: no live transcript.  `measure38.sh` drives the feed and
  uses Save Transcript at the end, giving `Adven_N.rtf`.  The last command
  is not in the .rtf, a death or an end-game modal wipes the scrollback (so
  stop the feed before the ending), and `£` comes out as `Â£`.
- **Feed**: `harness/make_wine_cmdfile.py` from the golden.  It strips
  comment lines, reads the startup waitkeys measured by `SCR_MARK_WAITKEY=1`
  (not leading blank lines) for `PRE`, and emits the pause / `#sleep`
  markers of every span in the order `SCR_MARK_WAITKEY` / `SCR_MARK_WAIT`
  printed them, including the span after the last command.
- **Compare**: `harness/compare_wine_transcript.py --taf --feed --runner`
  replays the feed through `harness/scare`, prints every feed command the
  Runner never echoed (rule 2), then diffs the aligned turns
  whitespace-normalised (`--offset`, `--start`, `--scarier`; `#` lines are
  stripped, blank lines kept).  Feed a CP1252 solution file for non-ASCII
  games.
- **Offline oracles**, when Wine is not available: the UTF-16 constant pools
  of the four exes (which Runner holds which literal settles most version
  gates; 3.7 753 strings, 3.8 864, 3.9/4.0 more), the corpus' ALR
  *Original* strings (an author only rewrites what the Runner prints), and
  the decompiles (`run400.bas`, `run390_3.bas`, `run370.bas`, always
  confirmed against the `.p32dasm.txt`; `push &HFF 'Byte` is -1, not 255).
- **Probes**: `harness/make_arena_probe.py` (4.00, packed with `taftool.py`
  against a donor .taf), `harness/make_39_fwprobe.py` and
  `harness/make_39_putprobe.py` (3.9 schema written directly; generators
  only convert upward).  The `p4WK*` walk probes were hand-built in gen400
  and have no regenerator.

## Before measuring anything

1. **Verbose ON, all five Appearance checkboxes ON, fresh process.**  They
   default OFF, never persist, and governed two of Professor's three
   "divergences".  Brackets ON is policy since 2026-08-29: Scarier prints
   the `(the X)` echoes and the goldens carry them.
2. **Every feed command must be echoed.**  A sound-alert modal, a long
   cutscene, a real-time `<wait>`, a startup pause or a missed menu click
   each eat a Return, and a swallowed command reads as two engine bugs
   (xfiles `feed[23] look`).  Read the echoes before the diff.
3. **Screen the row first**: `harness/screen_wine_candidate.py` gives the
   version, the real command count, events and which can roll, NPCs and
   walkers, and silent typeable tasks (reverse commands included) with a
   `TYPED` note when the walkthrough actually types one.  The candidate
   table's `cmds` counts golden LINES including comments; `wc -l` the
   cmdfile.
4. **Pre-4.0 silent-task rule.**  A matched task whose TURN printed nothing
   is answered with the game's DontUnderstand string; the actions still run.
   Scarier falls through to the library instead (deliberate, below).  A task
   can print through EndGame, ShowRoomDesc or any other action, so a
   `SILENT-TYPEABLE` flag is a suspicion, not a verdict.
5. **The game must be in `pfx/drive_c/adrift/`** (`measure.sh` does not copy
   it) and its filename must have **no spaces** (VB `Command$` keeps the
   quotes; the symptom is a 0-byte transcript and a misleading
   `first command never reached the game` abort).
6. **Startup prompts**: name/gender InputBoxes at LOAD take
   `POPUP_ANSWERS="Hero|male"` and are NOT feed lines (lifesimulation,
   Phoenix_Destiny, murdergreatfalls); PromptName games want their name
   (`FarFromHome`/`windy2` "Sam", `imagination` "Jenny"); re-read the window
   geometry after a dialog.
7. **Starting on or in something**: pre-4.0 refuses container commands from
   the bed ("You can't reach X from Y!"), so routes begin `get up`.
8. **Endings**: the `[Press any key to end]` tail-only diff is a property of
   the ending, not a law (`forest` has none, `Wheel105` says `[Game ended]`,
   `Matt's House` ends on `score`).  A game ending on a `<waitkey>` writes
   its summary only after a key (`Chosen`).  After the summary run400
   reloads the game, so trailing feed lines go to the restart (`QuestI`).  A
   golden's `quit` / `y` closes the Runner before the transcript is saved:
   stop at `score`.
9. **Never compare turn for turn across a `#save` in run390.**  The echoed
   `> save` turn moved FarFromHome's event clock one tick per checkpoint
   (cause unread); checkpoints are for getting back to a position.
10. **Harness-only artifacts** to name and ignore: a wrap inside an
    unbreakable token (78-column formatter; Renuntio's 90 asterisks), a
    `<waitkey><cls>` butt-join and the 25-newline `<cls>` phantom space
    (`os_ansi.cpp`), `<waitkey>` line joins in a live transcript
    (`InMemory`, `cyber`, `sommeril`), `.rtf` mojibake.
11. **Kill Wine properly** (`pkill -9 -f wine; pkill -f wineserver`).
    Killing `measure.sh` does not kill its child `drive_ckpt_safe.sh`; a
    dead Runner used to leak the rest of the feed into the front window
    (guarded now).  A locked screen discards all synthetic input.  Never
    `rm` a glob in `pfx/drive_c/adrift/` -- the archive was wiped on
    2026-08-30 and restored from Time Machine.
12. **Randomised puzzle state** (humbug's dials, magic word, keypad; Viking
    phone numbers) has to be read off the transcript and spliced in at a
    checkpoint.

## Measured so far

One line per game.  Everything was settled between 2026-08-02 and
2026-09-05; where the Runner disagreed the engine changed and the golden
was re-blessed, with the evidence in the row's comment block in
`harness/run_v4_walkthroughs.sh`.  Transcripts live in
`~/adrift-battle/runner/wine/pfx/drive_c/adrift/`.


| game | version | evidence | outcome (first sentence; full row in git history and the harness row comment) |
|---|---|---|---|
| `Professor.taf` | 4.00 | full run400 replay | the worked example; walk phase, arrival lines, presence lines |
| `FunHouse.taf` | 4.00 | full run400 replay, 0/18 commands differ | an **empty game-start walk preempts for ever**: NPC 3 WALK 1 and NPC 5 WALK 1 stay shut all game |
| `TheCatintheTree.taf` | 4.00 | full run400 replay | corroborates the same rule -- the boy (NPC 2 WALK 1) never arrives |
| `humbug.taf` | 4.00 | `Adrift_4_humbug.txt`, `Adrift_5_humbug.txt` | ChangedDesc pick is task-state only, ascending, non-empty wins; the partial replay added the `On X is`, `and carrying` and pronoun-echo findings below.  **Not fully replayable** -- three randomised secrets, see "Still open" |
| `lair-of-the-cybercow.taf` | 3.90 | run390 P-code, viewroom `loc_447D1D` | same lister rule one Runner down; one line changes |
| `great.taf` | 3.80 | run380 P-code, `characters() '441928` | no expiry stamp at all, restart needs `Loop = 1`, preempt has no StoppingTask test |
| `maincourse`, `orient`, `xfiles`, `wamk` | 4.00 | re-blessed under the same two rules | `maincourse` lost its win marker to a faithful preemption |
| `iqsfot.taf` | 4.00 | see the row's comment block | NPC 16 WALK 2 is an empty game-start walk with no stops; it pins the patrol shut and the game cannot be won in run400 |
| `the_pk_girl.taf` | 4.00 | full run400 replay with a 96-command peddler hunt spliced in | the Runner WINS -- and that is what proved a finished 4.0 walk is stamped **-1**, not 255 |
| arena probes EV14/EV15/EV16 | 4.00 | run400, `harness/make_arena_probe.py` (Adrift_1_ev14..16.txt) | **`x <npc>` and a nothing-found examine are administrative turns** -- no turn count, no walk, no event tick; `x me`, `x <object>`, `look`, `i` are normal; "Time passes..." carries its own vbCrLf; "1 turns so far" never singularised |
| `BobBobsly.taf` | 3.90 | run390 (Adrift_1_bob390.txt) | 3.9 counts NPC examine, failed examine and `turns` as turns; `z` = 1 turn under WaitTurns 3 -- see Open leads |
| `CAH.taf` (cruel) | 3.90 | run390 probe (Adrift_1_cruelprobe.txt) | `take it` -> "You can't take the jacket." |
| `man overboard.taf` | 4.00 | full run400 replay, 99/99 identical but the tail | settles the `again` echo, the give/ask rewrites and "(a Cupboard)" |
| `princess1.taf`, `Tear.taf`, `lobster.taf`, `PTGOOD.taf` | 4.00 | full run400 replays | 78/78, 36/36, 54/54, 6/6 (+7/7 ptgood_again) identical |
| `Beanstalk.taf` | 4.00 | full run400 replay, 49/49 | the turn-45 stranger greeting is one command later because `x stranger` is administrative |
| `CIBASS.taf` | 4.00 | partial run400 replay | identical to turn 16, then waitkey prompts desync the script |
| `arlo.taf` | 3.70 | `Adven_6_arlo.rtf` | the 3.7 walk departure lines, incl. "walks off to not moved."; 3 differing of 85 |
| `tra.taf` | 3.80 | `Adven_9_timmy_reid.rtf` | "outside" takes no "to" in a departure line |
| `Melbourne Beach.taf` | 3.90 | `Adrift_37_melbourne_beach.txt` | the 3.9 walk directions, incl. the diagonal a pre-4.0 8-exit scan cannot name |
| `Orient_Express.taf` | 4.00 | `Adrift_36_orient_express.txt` | the 4.0 walk directions; also the spurious "Gimme Atip enters." arrival |
| `S_Tar_Dus.taf` | 3.90 | `Adrift_38_stardust.txt` | all 129 walk lines match count for count; pinned the not-a-room-zero arrival gate |
| `asteroid_after.taf` | 4.00 | live run400 probes (six co-present valves) + the corpus' ALR tables + UTF-16 literals in … | the 4.00 object-ambiguity rule, its wording, its follow-up prompt, and that NPCs share the object message -- see the MEASURED section below |
| `p4ALR` / `p4ALRSRC` / `p4WALKCOUNT` / `p4VARFREEZE` (built probes) | 4.00 + 3.90 | run400 and run390 replays of four packed probe games | the whole **4.0 output filter**: walk = repeat a length-descending pass until nothing changes, self-containing ALRs retired per walk, one walk per completing task plus the flush, variables frozen by each walk -- see the FIXED section below |
| `3monkeys.taf` | 4.00 | `Adrift_16.txt` | the Runner really does print the raw `CHIMPSIGNAL=0`; the variable freeze is not a port artefact |
| `Oh_Human.taf` | 4.00 | `Adrift_1_ohhuman.txt` | 9/9 identical on every turn; compared 2026-08-30 |
| `wingman1.taf` | 3.90 | `Adrift_3_wingman1.txt` | 32/32 identical but the tail -- once the 3.9 `(Getting off ...)` correction below landed |
| `gamma.taf` | 3.90 | `Adrift_3_gamma.txt` | 185/185 identical but the tail, all 4 walks and 10 NPCs in step -- once the pre-4.0 openness-line fix below landed |
| `tcom.taf` | 3.90 | `Adrift_3_tcom.txt` | 13/13 identical but the tail; the three walk scenes line up |
| `windy2.taf` | 3.90 | `Adrift_3_windy2.txt` | 147/147 identical but the tail; 8 NPCs, both walks and the fixed skinny-dip event all in step |
| `Richard.taf` | 3.90 | `Adrift_3_richard.txt` | 70/70 identical but the tail -- once the 3.9 WinText pspace join below landed; 1000/1000 |
| `cleft.taf` | 3.90 | `Adrift_3_cleft.txt`, `Adrift_3_cleft2.txt` | first drive 90/90 echoed with 3 divergent turns -- the 3.9 event-move seen-byte split below; re-drive with the `look` added 91/91 identical but the tail, Runner wins 100/100. … |
| `sa.taf` (`sophie`) | 4.00 | `Adrift_41_sophie.txt`, `Adrift_45_sophie.txt` | the walk announcement is **joined into the turn's paragraph**, so 12 of sa.taf's 65 join-spanning ALRs fire and delete the arrivals they match -- see the FIXED section below |
| `p4WALKALR` (built probe) | 4.00 | `Adrift_47_p4walkalr.txt` | the join itself, in isolation: an ALR whose Original starts with the two-space separator matches |
| `The_X-Files_A_New_Beginning.taf` (`xfiles`) | 4.00 | `Adrift_22_xfiles.txt` | a **"The" prefix is never lower-cased**, and **what is *on* an object is listed before what is *in* it, in one sentence** -- see the two FIXED sections below. … |
| `p4BURN` (built probe) + an `xfiles` bisect | 4.00 | `Adrift_6_p4burn.txt`, `Adrift_2_p4burn.txt`, `Adrift_12/13_p4burn.txt` +1 more | **4.0 substitutes an object's Short or Alias into a `%object%` task command verbatim** and compares it to the lower-cased input, so a capitalised Short can never bind and no article, Prefix or partial name binds either. … |
| `p4STATE` (built probe) | 4.00 | `Adrift_1_p4state.txt` | **only `%state_<obj>%` lower-cases an object's state name, and it folds the whole string**; the examine lister and `%obstate%` print it verbatim.  One golden, three lines |
| `p39CASE` (built probe) | 3.90 | `Adrift_1_p39case.txt` | the 3.90 half of the same rule: **strict binding starts at 3.90, the case fold is only lost at 4.0**.  Moved five rows in Scarier and no goldens |
| `p4WALKCAP` (built probe) | 4.00 | `Adrift_1_p4walkcap.txt` | **4.0 capitalises a walk announcement's Name wherever the sentence lands** -- joined mid-paragraph and opening a line both print `Bob` for an NPC named `bob`.  Confirmed the ported reading; no change |
| `p4PALR` (built probe) | 4.00 | `Adrift_1_p4palr.txt` | **punctuation in an ALR changes nothing**: all seven cells fire, leading `, `/` `/`: ` Originals and pure-punctuation Replacements alike.  Confirmed `sophie.taf`'s `[, and] -> [:]`; no change |
| `p39EXAM` / `p4EXAM` (built probes) | 3.90 + 4.00 | `Adrift_41/43_p39exam.txt`, `Adrift_1_p4exam.txt` | the whole **examine / read / open / close refusal family**, plus the empty room description: four splits found and ported, and 3.90 now agrees with Scarier on all 48 rows.  See the FIXED sections below |
| `hauntedhouse.taf` | 4.00 | `Adrift_1_hauntedhouse.txt` | **clean: 41 of 42 turns identical, and the 42nd differs only by the Runner's `[Press any key to end]` tail**, which Scarier emits as a waitkey pause rather than as text.  Supersedes the mispaired `Adrift_16/17` run except for the two engine bugs that one found |
| `goldilocks.taf` | 4.00 | `Adrift_1_goldilocks.txt` | one real divergence in 252 turns, and it was an engine bug: **an event's look text is gated on the room being described, not on the room the player is standing in** -- see the FIXED section below. … |
| `lair-of-the-cybercow.taf` | 3.90 | `Adrift_1_cybercow.txt` | the *other* direction of the same rule: the Runner **does** print the day/night event's look text in the Chapel Yard a ShowRoomDesc task shows, while the player is still at the bottom of the well. … |
| `Monsters_r2.taf` | 4.00 | `Adrift_1_monsters.txt` | **brackets ON prints `(Getting off Sissy's four poster bed first)` on its own line** (turns 5, 23); after the port 37/38 identical, the 38th is the `[Press any key to end]` tail |
| `ADRIFTMaze.taf` | 4.00 | `Adrift_1_adrift_maze.txt` | **the 4.0 pronoun echo `(a trophy)`** on turns 24-25; otherwise identical bar the echoed name |
| `BlackSheepsGold.taf` | 4.00 | `Adrift_1_black_sheeps_gold.txt` | clean: 98/99 identical, the 99th cut off at the Runner's last `(press any key to continue)`.  Needs `--offset 0` |
| `Space Boy's First Adventure.taf` | 4.00 | full run400 replay, 133/133 echoed | clean: 132/133, the tail only |
| `angeldevilhuman`, `cyber`, `demonhunter`, `plunder_gargoyle`, `renegade_brainwave`, `ptgood`, `srsintro`, `imagination` | 4.00 | full run400 replays, every command echoed | clean: all turns identical except the `[Press any key to end]` tail (and the echoed name for `imagination`) |
| `cyber2.taf` | 4.00 | full run400 replay, 29/29 echoed | 26/29; turns 15 and 26 differ by one **battle roll** line each (rule 3), 28 is the tail |
| `dragonshrine`, `through_time`, `invasion_shirts`, `qui_a_tue_dana`, `whitterscap`, `hyper_b_s`, `cibass`, `allhallowseve` | 4.00 | partial run400 replays -- a cutscene, real-time pause or waitkey eats a fed command … | identical up to the loss (105, 12, 13, 15, 13, 3, 3 and 3 turns respectively); nothing after it is comparable |
| `Vardock Bates.taf` | 4.00 | full run400 replay (Adrift_1_vardock_bates.txt), then a checkpointed probe … | `<waitkey 4>` is a zero-second wait; co() feeds the generic verbs; a finishing event's task re-checks LOWER-indexed events in the same tick; the SYNONYM table is a sequence of whole-string rewrites -- see the two FIXED 2026-08-29 sections |
| `ECOD3.taf` | 3.90 | `Adrift_3_ecod3.txt` | clean: 11/11 echoed, 10/11 turns identical and the Usher's walk in step; the 11th is the tail -- the transcript stops mid-epilogue at the final pause, so the alley arrival and score summary never flush.  Measured 2026-08-31 |
| `largo-winch.taf` | 3.90 | `Adrift_3_largo_winch.txt` | clean: 323/323 echoed, 322/323 turns identical, all 42 NPCs and 22 events in step; the tail is the Runner's `[Press any key a end]` only. … |
| `mudergreatfalls.taf` | 3.90 | `Adrift_3_murder_great_falls.txt` (`POPUP_ANSWERS="Hero\|male"`, PRE=2, compare `--start 2`) | clean: 101/101 echoed, every turn identical; the tail is the winning `accuse ken` cut at the Runner's endgame pause |
| `report.taf` | 3.90 | `Adrift_3_report.txt` | clean: 165/165 echoed, every turn identical but the `[Press any key to end]` tail; 100/100 |
| `Archie's Birthday V 1-2.taf` | 3.90 | `Adrift_3_archie.txt` | 205/205 echoed; two engine divergences, both fixed: run390 echoes `(a camcorder)` on `take it` (Scarier's 3.9 gate was wrong) and appends `.` to a PlayerDesc that lacks one; clean after the fix but the `[Press any key to end]` tail; 50/50 |
| `veteran.taf` | 3.90 | `Adrift_3_veteran_probe.txt` | `take it` then `open it` both echo `(a bag)`: 3.9 keeps the authored article after a take, unlike 4.0's `(the bag)` |
| `yak_shaving.taf` | 4.00 | `Adrift_4_yak_probe.txt` | `x me` answers `...after your journey.` -- 4.0 appends the full stop too |
| `croft.taf` | 3.90 | `Adrift_5_croft.txt` | 101/101 echoed; zero engine divergences -- the only diff is the Runner's `[Press any key to end]` after the final score summary; 150/150 |
| `DarkTower.taf` | 3.90 | `Adrift_6_darktower.txt` | 121/121 echoed; zero engine divergences -- only the Runner's `[Press any key to end]` after the 0/0 score summary; "restored power to the building." |
| `FarFromHome.taf` | 3.90 | `Adrift_8.txt`, `Adrift_7.txt` | 71/71 echoed; zero engine divergences -- the only diff is the Runner transcript stopping at the `<waitkey>` inside the ending text. … |
| `EnqueteAHautsRisques.taf` | 3.90 | `Adrift_9_enquete.txt` | 145/145 echoed; zero engine divergences -- 144 of 145 turns byte-identical (French, CP1252) and the 145th, the winning `se coucher`, differs only by the Runner's `[Press any key a end]`. … |
| `Captive.taf` | 3.90 | `Adrift_9_captive.txt` | 57/57 echoed; zero engine divergences -- 56 of 57 turns byte-identical and the 57th, the winning `put diamond on pedestal`, differs only by the Runner's `[Press any key to end]`. … |
| `The Screen Savers On Planet X.taf` | 3.90 | `Adrift_9_screensavers.txt` | 133/133 echoed; zero engine divergences -- 132 of 133 turns byte-identical and the 133rd, the winning `look`, differs only by the Runner's `[Press any key to end]`.  All 19 events are `start=0..0 time1=1 time2=1` and the 10 NPCs never walk; 142/142 |
| `thewoods.taf` | 3.90 | `Adrift_9_thewoods.txt` | 73/73 echoed; zero engine divergences -- 72 of 73 turns byte-identical and the 73rd, the winning `take head`, differs only by the Runner's `[Press any key to end]`. … |
| `Chosen.taf` | 3.90 | `Adrift_9_chosen.txt` | 52/52 echoed; zero engine divergences -- 51 of 52 turns byte-identical and the 52nd, the winning `plug t block`, differs only by the Runner's `[Press any key to end]`.  The dump has 0 events and 0 NPCs, so nothing on the route can roll. … |
| `Renuntio.taf` | 3.90 | `Adrift_9_renuntio.txt` | 39/39 echoed; zero engine divergences -- the three unequal turns are the Runner's `[Press any key to end]` and, twice, the 90-asterisk scene divider that the harness wraps 78 + 12 (an unbreakable token, the one wrap whitespace normalisation cannot undo). … |
| `as.taf` (Asylum) | 3.90 | `Adrift_9_asylum.txt` | 27/27 echoed; zero engine divergences -- the two unequal turns are the `[Press any key to end]` tail and one `<cls>` welded between two sentences with no `<br>` (see the bullet above).  0 events and one non-walking NPC; … |
| `sleaze.taf` | 3.90 | `Adrift_9_sleaze.txt` | 43/43 echoed; zero engine divergences and no artifacts either -- 42 of 43 turns byte-identical and the 43rd, the winning `serve`, differs only by the Runner's `[Press any key to end]`.  0 events and 0 NPCs; 100/100 |
| `everything.taf` | 3.90 | `Adrift_9_everything.txt` | 38/38 echoed; … |
| `A_Morning_with_a_Headache.taf` | 3.90 | `Adrift_9_morning.txt` | 53/53 echoed; zero engine divergences after one port -- 52 of the 53 turns identical and the 53rd, the winning `open door`, differs only by the Runner's `[Press any key to end]`. … |
| `mhpquest.taf` | 3.90 | `Adrift_11_mhpquest.txt` | 53/53 echoed; zero engine divergences -- 52 of the 53 turns identical and the 53rd, the winning `feed clover to crystal`, differs only by the Runner's `[Press any key to end]`. … |
| `chicago.taf` | 3.90 | `Adrift_9_chicago.txt` | 42/42 echoed; one real engine divergence, now fixed -- a second `listen` where run390 says "You have already done that." and Scarier gave the library's "You hear nothing out of the ordinary.". … |
| `CAH.taf` | 3.90 | `Adrift_9_cah.txt` | 30/30 echoed, tail only.  0 events, 0 NPCs, 0 silent tasks -- a pure parser/library row, and it passes clean |
| `forest.taf` | 3.90 | `Adrift_9_forest.txt` | 27/27 echoed and **identical on every turn**, tail included; the ending does not stop for a keypress.  Four NPCs, no events |
| `amonkeytoomany.taf` | 3.90 | `Adrift_9_amonkey.txt` | 12/12 echoed, tail only; 25/25 both sides |
| `Toxically_Earth.taf` | 3.90 | `Adrift_9_toxically.txt` | 11/11 echoed, tail only.  Seventeen NPCs, none of whom speaks on the route; third confirmation that a silent task with a bare `ACT type=6` prints and so never reaches the DontUnderstand fallback |
| `Insane.taf` | 3.90 | `Adrift_9_insane.txt` | 16/16 echoed, tail only; 1000/1000 both sides.  Exposed a harness bug: the solution's three leading blanks are **empty commands**, not startup pauses, and `make_wine_cmdfile.py` was moving them to PRE, which would have sent them before Start Transcript |
| `tq3.taf` | 3.90 | `Adrift_9_tq3.txt` | 51/51 echoed, tail only.  Four events, none rollable, two NPCs; 60/2400 both sides |
| `DFU.taf` | 3.90 | `Adrift_9_dfu.txt` | 21/21 echoed, tail only; 999999999/999999999 both sides |
| `CRM.taf` | 3.90 | `Adrift_9_crm.txt` | 21/21 echoed, tail only; 25/25 both sides |
| `ECOD2.taf` | 3.90 | `Adrift_9_ecod2.txt` | 24/24 echoed, tail only |
| `lostsouls.taf` | 3.90 | `Adrift_10_lostsouls.txt` | 21/21 echoed; the only differences are the `[Press any key to end]` tail and the known `<waitkey><cls>` butt-join.  The first drive broke off at `> open door` because the three-beat ending's pauses were not in the feed at all; … |
| `Wheel105.taf` | 3.90 | `Adrift_9_wheel105.txt` | 19/19 echoed; the only differences are the ending's `[Game ended]` -- this game does not say `[Press any key to end]` -- and three `<waitkey><cls>` butt-joins.  Fifteen events, none rollable. … |
| `veteran.taf` | 3.90 | `Adrift_9_veteran.txt` | 47/47 echoed, **zero content differences**; the only difference is the ending's `[Press any key to end]`.  No events at all, three NPCs, no silent tasks; both sides finish 0/0 at 100% |
| `BobBobsly.taf` | 3.90 | `Adrift_10_bobbobsly.txt` | 25/25 echoed, **zero content differences**; tail only.  Includes a `yes` answering the game's own question and a `beam me up scotty` easter egg; both sides win 155/155 |
| `tcom.taf` | 3.90 | `Adrift_11_tcom.txt` | 13/13 echoed, **zero content differences**; tail only.  First row to prove the new trailing-span emission: the ending is four real-time `<wait>`s long and the feed's `#sleep`s held the drive there long enough to record all of it |
| `lifesimulation.taf` | 3.90 | `Adrift_12_lifesim.txt` (`POPUP_ANSWERS="Hero\|male"`) | 15/15 echoed; one divergence, `turn off tv` -- the silent-turn rule reached through a ReverseCommand with an empty ReverseMessage (deliberate deviation) |
| `LOST.TAF` (`lost`) | 3.90 | `Adrift_18_lost.txt` | 38/38 echoed, **zero content differences**; tail only.  Eleven events, none rollable, a ghost NPC and five `z` waits in a row: the richest per-turn machinery measured clean so far |
| `LOST.TAF` (`lost_down`) | 3.90 | `Adrift_19_lost_down.txt` | 38/38 echoed, **zero content differences**; tail only.  Same route, the other ending (`down` instead of `up` at the last command) |
| `Matt's House.taf` | 3.90 | `Adrift_20_matts.txt` | 8/8 echoed, **identical on EVERY turn, the last included**: the golden ends on `score`, so there is no EndGame and no `[Press any key to end]`.  Third such row, after `forest.taf` and `Wheel105.taf`.  Must be driven as the space-free copy `matts.taf` |
| `Richard.taf` | 3.90 | `Adrift_21_richard.txt` | 70/70 echoed, **zero content differences**; tail only.  Thirteen events, five NPCs; both sides win 1000/1000 |
| `windy2.taf` | 3.90 | `Adrift_22_windy2.txt` | 147/147 echoed, **zero content differences**; tail only.  400 tasks, eight NPCs, 17 variables -- the longest 3.90 row measured, and the second POPUP game after `lifesimulation.taf` |
| `impulso.taf` | 3.90 | `Adrift_23_impulso.txt` | 8/8 echoed, **zero content differences**; tail only.  Screened as a guaranteed silent-task divergence and was not one: its CompleteText-less `atacar * chico` has `srd=5`, so the turn prints a room description |
| `Dreams.taf` | 3.90 | `Adrift_24_dreams.txt` | 9/9 echoed, **zero content differences**; tail only.  Screened the same way and was also not a divergence: its CompleteText-less win task ends the game, and the game's win text prints |
| `Phoenix_Destiny.taf` | 3.90 | `Adrift_18_phoenix.txt` | 18/18 echoed, **identical on every turn** -- and on the last one too: the walkthrough ends on `wealth`, the game never ends, so there is no `[Press any key to end]` tail.  27 events (none rollable) and 17 NPCs all in step.  Measured 2026-09-05 |
| `superliam.taf` | 3.80 | `Adven_1_superliam.rtf` | 85/85 echoed; … |
| `cave.taf` | 3.80 | `Adven_1_cave.rtf`, `Adven_1_cave2.rtf`, `Adven_1_cave3.rtf` | 215/215 echoed each time; FOUR engine findings, all FIXED: `z` is not 3.80 vocabulary (whole-line `= "z"` test only exists from run390_3 45FCB0 -- seven `z` -> `wait`); … |
| `haunt.taf` | 3.80 | `Adven_1_haunt.rtf` | 84/84 echoed; … |
| `jb2000.taf` | 3.80 | `Adven_1_jb2000.rtf` | 22/22 echoed; … |
| `Crime_Adventure.taf` | 3.80 | `Adven_1_crime.rtf` | every command echoed; 0 engine differences once the take->get rewrite was in.  Re-blessed: 65/95 finish (the `score` before `stand on chair` ticks the events in 3.8) |
| `mikes.taf` | 3.80 | `Adven_1_mikesb.rtf` | cmd 27 `take truck keys` -> `Which keys.  The mustang keys or the truck keys?` -- the end-of-turn co() prompt, now **PORTED for 3.7/3.8** (see the DIAGNOSED section's 2026-09-04 addendum); identical through cmd 52 after the port; … |
| `great.taf` | 3.80 | `Adven_1_greatx1.rtf` | 121/121 echoed, **0 engine differences** (turns 5/102/109 differ only by the .rtf's `Â£` mojibake). … |
| `akron.taf` | 3.80 | `Adven_7_akron.rtf` | still clean: 43/43 echoed, 0 differences (the 44th, `knock`, wins and is never echoed).  No events in the game, so the 2026-09-04 tick changes could not have moved it |
| `microwaveman.taf` | 3.80 | `Adven_1_microwaveman.rtf` | clean: 8/8 echoed, 0 differences.  Its one event is fixed-length (5) and StarterType 3 |
| `duck.taf` | 3.80 | `Adven_1_duck.rtf` | clean: 12/12 echoed, 0 differences |
| `first.taf` | 3.80 | `Adven_1_first.rtf` | clean: 18/18 echoed, identical on every turn |
| `haunted.taf` | 3.80 | `Adven_1_haunted.rtf` | clean: 115/115 echoed, 0 differences.  Both of its events (rain 15..20 delay / 10..15 length, chains 20..50 delay) are RNG-timed but carry no room list, so their texts never show; nothing to diverge on |
| `castle.taf` | 3.70 | `Adven_1_castle.rtf` | clean: 16/16 echoed, 0 differences.  The older `Adven_3_castle_quest.rtf` (723 bytes, 2026-08-23, driven by hand before `measure38.sh`) holds no turns at all and is superseded |
| `ptbad.taf` | 4.00 | `Adrift_19_ptbad.txt` | clean: 1/1 echoed, tail only.  Later re-used as the library-message probe game -- see `Adrift_36_ptbad_probe3.txt` / `Adrift_37_ptbad_probe4.txt` below |
| `Phoneb.taf` | 4.00 | `Adrift_20_phoneb.txt` | clean: 2/2 echoed, tail only |
| `rift.taf` | 4.00 | `Adrift_21_rift.txt` | clean: 3/3 echoed, tail only |
| `Newton.taf` | 4.00 | `Adrift_22_newton.txt` | clean: identical on every turn, no ending keypress |
| `The_Shuffling_Room.taf` | 4.00 | `Adrift_23_shufflingroom.txt` | clean: 10/10 echoed, tail only.  8 NPCs and the circle-of-men text all in step |
| `door.taf` | 4.00 | `Adrift_24_door.txt` | clean: identical on every turn |
| `smote.taf` | 4.00 | `Adrift_25_smote.txt` | clean: 9/9 echoed, tail only |
| `Undefined1.taf` | 4.00 | `Adrift_26_undefined.txt` | clean: 4/4 echoed, tail only, 3/3 both sides. … |
| `hungry.taf` | 4.00 | `Adrift_27_hungry.txt` | clean: identical on every turn |
| `Way Out.taf` | 4.00 | `Adrift_28_wayout.txt` | clean: 5/5 echoed, tail only |
| `agent_4F[1].A.taf` | 4.00 | `Adrift_29_agent4f.txt` | clean: 5/5 echoed, tail only |
| `TheAmulet.taf` | 4.00 | `Adrift_30_theamulet.txt` | clean: 12/12 echoed, tail only |
| `herrdoktor.taf` | 4.00 | `Adrift_31_herrdoktor.txt`, `Adrift_34_herrdoktor_probe.txt` | 15/15 echoed; ONE engine finding, FIXED: the Runner's third-person library messages are **not conjugated**. … |
| `Sandy.taf` | 4.00 | `Adrift_32_sandy.txt` | clean: identical on every turn.  Corroborates the "Sandy is unwinnable" verdict from the engine side: the Runner refuses the same commands and ends on the same "You see no such thing." |
| `shreddem.taf` | 4.00 | `Adrift_33_shreddem.txt` | clean: 15/15 echoed, tail only; 65/65 both sides |
| `Main Course.taf` | 4.00 | `Adrift_35_maincourse_probe.txt` | the third-person probe: Perspective 2 with no ALRs to confound it. … |
| `ptbad.taf` probes 3 + 4 | 4.00 | `Adrift_36_ptbad_probe3.txt`, `Adrift_37_ptbad_probe4.txt` | three **perspective-independent** message corrections, all FIXED: a second `drop all` answers "You are carrying nothing!" (4.0) / "You are not carrying anything." (pre-4.0), `wear all` with an empty inventory answers "You don't have anything to wear.", and … |
| `outline.taf` | 4.00 | `Adrift_38_outline.txt` | clean: 16/16 echoed, tail only -- the winning `x outline` differs by the Runner's `[Press any key to end]`; maximum points both sides |
| `QuestI.taf` | 4.00 | `Adrift_39_questi.txt` | clean through the death at turn 13: 13/13 identical, both sides 10/10.  The Runner then presses on past `[Press any key to end]` and **reloads the game** -- "Loading... … |
| `The_Stowaway.taf` | 4.00 | `Adrift_40_stowaway.txt` | clean: 16/16 echoed, tail only.  The ending arrives on a `wait`, so the whole "Time passes..." + event cascade is compared and matches |
| `longbarrow.taf` | 4.00 | `Adrift_41_longbarrow.txt` | clean: 19/19 echoed, tail only; the eleven repeated `dig with trowel` turns are byte-identical, so the dig counter and its event are in step |
| `Vagabond.taf` | 4.00 | `Adrift_42_vagabond.txt` (superseded by `Adrift_440_vagabond.txt`, **clean**, 2026-09-07) | 10/10 echoed; ONE divergence, and it is the **known ALR-over-a-joined-paragraph residual** of section 3, not a new one -- closed by the room-block port, see "Compared 2026-09-07".  Room 4's Long ends "A toolbox is here." and George's InRoomText is `#`, so the Runner's joined paragraph reads "A toolbox is here. … |
| `1HRGAME.taf` (`masochists_heaven`) | 4.00 | `Adrift_43_1hrgame.txt` | clean: 13/13 echoed, tail only; 15/15 both sides |
| `ARGH_sGreatEscape.taf` | 4.00 | `Adrift_44_argh.txt` | clean: 12/12 echoed, tail only; the escape ending is byte-identical up to `[Press any key to end]`; 98/125 both sides |
| `ShadricksTravels.taf` | 4.00 | `Adrift_45_shadricks.txt` | 22/22 echoed; ONE divergence, the **first live corpus sighting of the 2026-08-24 disambiguation wording** -- `climb tree` answers `Which tree.  The old oak tree or the pine tree?` in run400 and `Please be more clear, what do you want to climb? ...` in Scarier. … |
| `topaz.taf` | 4.00 | `Adrift_46_topaz.txt` | 23/23 echoed; ONE real engine divergence, now **FIXED** -- turn 11 listed "Also here is a Topaz." into a room whose own text had just described the sword. … |
| `Wreckage.taf` | 4.00 | `Adrift_47_wreckage.txt` | clean: 11/11 echoed, tail only; the winning `use the computer` matches to the last word |
| `SRSintro.taf` | 4.00 | `Adrift_48_srsintro.txt` | clean: **identical on every turn**, tail included -- the ending does not stop for a keypress |
| `All Hallows Eve.taf` | 4.00 | `Adrift_49_allhallowseve.txt` | clean: 16/16 echoed, tail only; 23/26 both sides.  measure.sh warned "2 pause-dismiss Return(s) sent -- PRE was wrong" but RULE 2 shows every command echoed and every turn aligned, so the extra Returns fell in the opening and cost nothing |
| `whitterscap.taf` | 4.00 | `Adrift_50_whitterscap.txt` | clean: 21/21 echoed, tail only; 2/2 and "ending 2 of 2" both sides.  The game's TYPED silent tasks (`* s *`, `* south *`) never fire because the wired route spells the direction out |
| `The Vault.taf` | 4.00 | `Adrift_51.txt` | clean: the single `read bible` turn, the opening of the vault and the whole "Inside" room are identical |
| `Cut_the_Red_Wire.taf` | 4.00 | `Adrift_52.txt` | clean: the one `undo` turn wins the game and matches to the last word, 1/1 both sides.  The Runner then prints "Press RETURN if you feel like giving it another go." and restarts into the intro, which is where its transcript keeps going and ours stops |
| `hiker.taf` | 4.00 | `Adrift_53.txt` | clean: `kill the hitchhiker` reaches Ending Three of Three identically |
| `P2P.taf` | 4.00 | `Adrift_54.txt` | clean: 4/4 echoed, tail only; maximum points both sides |
| `Existence.taf` | 4.00 | `Adrift_56.txt` | clean: 4/4 identical.  `Adrift_55.txt` is the same drive cut short -- it stops at the closing `[Press a key when you're ready to continue.]`, which is why it looks as though the Runner never printed the IntroComp sign-off; the re-drive shows it does |
| `zacksmackfoot.taf` | 4.00 | `Adrift_57.txt` | 5/5 echoed; ONE divergence, **OPEN** -- on `put knife in slot` run400 prints the library refusal `Your penknife is too big to fit inside the slot.` and *then* the task's text, where Scarier prints the task's text alone.  See "Still open" below |
| `zombiecow.taf` | 4.00 | `Adrift_58.txt` | clean: 7/7 echoed, tail only |
| `headless.taf` | 4.00 | `Adrift_59_headless.txt` | clean: 10/10 echoed, tail only |
| `MammothVacuum.taf` | 4.00 | `Adrift_60_mammoth.txt` | clean: 11/11 echoed, tail only |
| `Sandy.taf` (`sandy_meta_number`) | 4.00 | `Adrift_61_sandy_meta.txt` | 10/10 echoed; TWO divergences, both **deliberate** -- `wait 2` answers `Time passes...` in run400 and `hist 2` answers `I don't understand what you mean!`.  Neither `wait <n>` nor `hist <n>` exists in the Runner at all: they are SCARE's own meta-commands. … |
| `competition2006__adrift__ptgood__PTGOOD.taf` | 4.00 | `Adrift_62_ptgood.txt` | clean: 6/6 echoed, tail only |
| `The Dangers of Driving at Night.taf` | 4.00 | `Adrift_63_dangers.txt` | clean: 11/11 echoed, tail only |
| `rollingthedough.taf` | 4.00 | `Adrift_64_rollingthedough.txt` | clean: 13/13 echoed, tail only; maximum points both sides |
| `Witness_Demon_vs_Vampire.taf` | 4.00 | `Adrift_65_witnessdemon.txt` | clean: 13/13 echoed, tail only |
| `InMemory.taf` | 4.00 | `Adrift_66_inmemory.txt` | clean: 15/15 echoed.  The two apparent differences are the `<waitkey>` transcript-join artifact -- see the dated section |
| `MurderMansionntro.taf` | 4.00 | `Adrift_67_murdermansion.txt` | clean: every one of the Runner's 15 turns is identical |
| `Pilfers.taf` | 4.00 | `Adrift_68_pilfers.txt` | clean: 16/16 echoed, tail only; 107/107 both sides |
| `raccoon.taf` | 4.00 | `Adrift_69_raccoon.txt` | clean: 16/16 echoed, tail only |
| `dancingevenhim.taf` | 4.00 | `Adrift_70_dancingevenhim.txt` | clean: 17/17 echoed, tail only |
| `Through time.taf` | 4.00 | `Adrift_71_throughtime.txt` | clean on game text; the only difference is in how many feed lines each side's pauses swallowed -- see the dated section |
| `cyber.taf` | 4.00 | `Adrift_72_cyber.txt` | clean: 20/20 echoed; 150/150 both sides.  The one apparent difference at the ending is the `<waitkey>` transcript-join artifact |
| `The Angel the Devil and the Human.taf` | 4.00 | `Adrift_73_angeldevil.txt` | clean: 25/25 echoed, tail only |
| `Renegade_Brainwave.taf` | 4.00 | `Adrift_74_renegade.txt` | clean: 26/26 echoed, tail only |
| `I am the Law.taf` | 4.00 | `Adrift_75_law.txt` | clean: 26/26 echoed, tail only |
| `frog.taf` | 4.00 | `Adrift_76_frog.txt` | clean: 10/10 echoed, tail only |
| `SPAM.taf` | 4.00 | `Adrift_77_spam.txt` | 15/15 echoed; ONE divergence, now **FIXED** -- `ask about ingredients` prints its `(Nobody)` echo BEFORE the task's text, not after.  See the dated section: the echo is a direct display call, the task text is buffered |
| `sommeril.taf` | 4.00 | `Adrift_78_sommeril.txt`, `Adrift_79_somm_npcprobe.txt`, `Adrift_80_somm_placemat.txt` | 79/79 echoed.  Three findings, two of them now **FIXED** -- the `(GARGOYLE)` echo ordering (same fix as `SPAM`), the every-line last-named-character register, and the **trailing space in a task command pattern**, which run400 requires the input to have. … |
| `House.taf` | 4.00 | `Adrift_91.txt`, `Adrift_92.txt`, `Adrift_93.txt` (checkpoint drives from a Scarier-made `.tas`, `#restore` after the title menu's `2`) | the put/task precedence model **confirmed** and one gate rule **corrected and FIXED**: at the fireplace with the wood on the floor and the axe in hand, `put wood in fireplace` / `place wood in fireplace` print `(Taking the wood first)` then `Your hands are full.  You are not holding the wood.`; with the wood held every spelling (`put`, `place`, `drop wood in fireplace`, `put some wood into the fire place`) is the library put, task 459 never fires, `light fire` refuses with `You need some wood or coal to make a proper fire.` -- **House is unwinnable in run400**.  Scarier used to skip the implicit take because take-flagged task 60 `* %object%` pre-matched: run400's pre-matcher is restriction-aware, and task 60's silently-failing restriction drops it.  Every move pops an `evaluate error - Out of stack space` alert (the `%drunk%` ALR loop, see "Deliberate deviations").  `get cathy` there was `Take what?` in run400 against the library's take-NPC line in Scarier: five more drives (`Adrift_94`-`98`) pinned it -- the first line naming Cathy after the checkpoint runs the once-only silent task 200 `*cathy*` (`# attention on cathy grave vision`), and a task having run for the line shuts the take/examine/where/attack/talk-to branches of the character handler (`MemVar_4941F8`; ask-about, give and kiss survive -- Humbug's silent `ask * hacker about * humbug` still answers), so `get cathy` falls to `Take what?` and `x cathy` to `You see no such thing.`; the second mention gets "I don't think girl would appreciate being handled." (Prefix + first Alias, not the Name) and her description.  Restore does NOT clear her seen byte.  Both rules **PORTED 2026-09-06** (see "Ported 2026-09-06: the task-ran NPC gate"); `Adrift_99` confirms `ask cathy about grave` answers on the first mention; `Adrift_100` (`kiss cathy` x2, `#restore`, `where is cathy` x2) pins the **one-task-per-line** rule: the first `kiss cathy` runs silent task 200 and then the LIBRARY's `I'm not sure she would appreciate that!`, never the game's kiss task 882 (`Cathy gently but firmly pushes you back` only on the second kiss); `where is cathy` is `I don't know where that is!` then `Cathy is dining room.  (Right next to you silly!)`.  **PORTED 2026-09-06** (see "Ported 2026-09-06: one task per typed line"); `Adrift_101` (`hit cathy` x2, `talk to cathy` x2) matches Scarier line for line with NO change: first `hit cathy` = `You hit, but nothing happens.` (attack branch shut, generaltasks_verbs fallback), second = task 881 `Cathy is awake now and looks capable of hitting you back if you tried.`; first `talk to cathy` = the talk-to-nobody line (one of three random replies, all ALR-mapped by House to `Use the format "ask Cathy about [subject] or "give Cathy [object]""`), second = the character handler's `Use the format "ask Cathy about [subject]".`; `Adrift_102`-`104` (give): `give diary to cathy` answers on the FIRST mention (give is not task-ran gated), `give cathy diary` and `give diary cathy` give too (word-order-free give, **PORTED 2026-09-06** as two table rows), bare `give diary` echoes `(to Nobody)` / `(to Cathy)` from the last-named character (already ported), and a dropped diary gets `You don't have the diary!` in every order; `Adrift_105`-`109` (throw, five drives, 39 lines): House has BattleSystem off, so `throw` is an unhandled verb (run400 only knows it inside dobattle 47F084, reached from 48A4A2 when MemVar_494282 = 1) and the line falls to the catch-all -- ONE present object named = `I don't understand what you want me to do with the diary.` (`throw diary at cathy`, `throw diary to cathy`, `wibble diary cathy`: an NPC is not a candidate), TWO present objects tied = NOTHING from the library, the game's `[error=%help_value%]` DontUnderstand (`throw diary at fireplace`, `throw fireplace diary`, `wibble diary fireplace`, `throw window at fireplace`, `throw hook at plaster`), and a typed Prefix word breaks the tie (`throw diary at the hook` -> the hook, `throw the plaster at fireplace` -> the plaster, `throw dining room window at fireplace` -> `dining room window` with no article, `throw a hook at diary` / `throw a diary at plaster` -> the diary).  An EMPTY Prefix is `a` (loader 4900EC), so `throw a diary at fireplace`, `throw diary at a fireplace`, `throw a fireplace at the hook` and `throw diary at the hook a` all tie; `an` is not `a` (`throw an diary at the hook` -> the hook).  **PORTED 2026-09-06** (see "Ported 2026-09-06: an unhandled verb naming two objects") |


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
| `To_Hell_And_Beyond.taf` | `to_hell_and_beyond` | 3 | 19 | 41 | 7 | -- | [To_Hell_And_Beyond_walkthrough](To_Hell_And_Beyond_walkthrough.md) **done** 2026-09-06 -- clean in run400 (Adrift_124_to_hell_and_beyond.txt) |
| `goldilocks.taf` | `goldilocks` | 252 | 8 | 6 | 10 | -- | [Goldilocks_walkthrough](Goldilocks_walkthrough.md) **measured** -- see "Measured so far" |
| `CIBASS.taf` | `cibass` | 40 | 8 | 2 | 8 | yes | [CIBASS_walkthrough](CIBASS_walkthrough.md) **measured** -- see "Measured so far" |
| `FunHouse.taf` | `funhouse` | 18 | 8 | 9 | 0 | -- | **done** 2026-08-24 -- see "Measured so far" |
| `sa.taf` | `sophie` | 255 | 7 | 73 | 13 | yes | **partly done** 2026-08-25 (first 50 commands) -- see "Measured so far"; [Sophies_Adventure_walkthrough](Sophies_Adventure_walkthrough.md) |
| `sophie.taf` | `sophie_comp` | 255 | 6 | 72 | 13 | yes | [Sophies_Adventure_walkthrough](Sophies_Adventure_walkthrough.md) **measured** 2026-09-06 -- desyncs early, stream misaligned (Adrift_173_sophie_comp.txt), see "Measured 2026-09-06" |
| `Oh_Human.taf` | `ohhuman` | 9 | 6 | 3 | 5 | -- | -- **measured** -- see "Measured so far" |
| `TheCatintheTree.taf` | `the_cat_in_the_tree` | 8 | 5 | 4 | 1 | yes | **done** 2026-08-24 -- see "Measured so far" |
| `Monsters_r2.taf` | `monsters` | 38 | 3 | 3 | 4 | -- | -- **measured** -- see "Measured so far" |
| `The Angel the Devil and the Human.taf` | `angeldevilhuman` | 25 | 3 | 3 | 3 | -- | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `Through time.taf` | `through_time` | 18 | 3 | 10 | 3 | -- | [Through_time_walkthrough](Through_time_walkthrough.md) **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `Vardock Bates.taf` | `vardock_bates` | 103 | 2 | 4 | 4 | yes | [Vardock_Bates_walkthrough](Vardock_Bates_walkthrough.md) **measured** -- see "Measured so far" |
| `Professor.taf` | `professor` | 86 | 2 | 9 | 4 | -- | **done** -- the worked example |
| `cyber2.taf` | `cyber2` | 29 | 2 | 8 | 1 | -- | [cyber2_walkthrough](cyber2_walkthrough.md) **measured** -- see "Measured so far" |
| `ADRIFTMaze.taf` | `adrift_maze` | 26 | 2 | 5 | 5 | -- | [ADRIFT_Maze_walkthrough](ADRIFT_Maze_walkthrough.md) **measured** -- see "Measured so far" |
| `cyber.taf` | `cyber` | 20 | 2 | 3 | 1 | -- | [Cyber_walkthrough](Cyber_walkthrough.md) **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `DragonShrineR43.taf` | `dragonshrine` | 136 | 1 | 1 | 7 | yes | [The_Curse_of_DragonShrine_walkthrough](The_Curse_of_DragonShrine_walkthrough.md) **done** 2026-09-06 -- clean in run400 but the ending tail (Adrift_152_dragonshrine.txt) |
| `BlackSheepsGold.taf` | `black_sheeps_gold` | 99 | 1 | 11 | 1 | yes | -- **measured** -- see "Measured so far" |
| `QuiATueDana.taf` | `qui_a_tue_dana` | 63 | 1 | 4 | 0 | yes | **driven** 2026-09-06 -- the Runner lost a feed command (Adrift_162_qui_a_tue_dana.txt); re-feed before reading anything into it, see "Measured 2026-09-06" |
| `plunder_gargoyle.taf` | `plunder_gargoyle` | 43 | 1 | 3 | 4 | -- | [Pirates_Plunder_walkthrough](Pirates_Plunder_walkthrough.md) **done** 2026-09-06 -- clean in run400 but the ending tail (Adrift_156_plunder_gargoyle.txt) |
| `demonhunter.taf` | `demonhunter` | 40 | 1 | 2 | 2 | -- | [Apprentice_of_the_Demonhunter_walkthrough](Apprentice_of_the_Demonhunter_walkthrough.md) **done** 2026-09-06 -- clean in run400 but the ending tail (Adrift_137_demonhunter.txt) |
| `Invasion of the Second-Hand Shirts.taf` | `invasion_shirts` | 39 | 1 | 3 | 0 | -- | [Invasion_of_the_Second-Hand_Shirts_walkthrough](Invasion_of_the_Second-Hand_Shirts_walkthrough.md) **done** 2026-09-06 -- clean in run400 (Adrift_131_invasion_shirts.txt) |
| `Imagination.taf` | `imagination` | 35 | 1 | 1 | 0 | -- | [Just_My_Imagination_walkthrough](Just_My_Imagination_walkthrough.md) **measured** 2026-09-06 -- clean but for the ending tail (Adrift_135_imagination.txt); compare with `--popup Jenny` |
| `hyper_b_s.taf` | `hyper_b_s` | 34 | 1 | 2 | 1 | -- | [hyper_b_s_walkthrough](hyper_b_s_walkthrough.md) **driven** 2026-09-06 -- the Runner lost a feed command (Adrift_145_hyper_b_s.txt); re-feed before reading anything into it, see "Measured 2026-09-06" |
| `Renegade_Brainwave.taf` | `renegade_brainwave` | 25 | 1 | 5 | 3 | -- | [Renegade_Brainwave_walkthrough](Renegade_Brainwave_walkthrough.md) **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `whitterscap.taf` | `whitterscap` | 21 | 1 | 3 | 4 | -- | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `All Hallows Eve.taf` | `allhallowseve` | 16 | 1 | 4 | 0 | yes | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `SRSintro.taf` | `srsintro` | 13 | 1 | 2 | 3 | -- | [SRSintro_walkthrough](SRSintro_walkthrough.md) **done** 2026-09-05 -- clean in run400 (identical on every turn), see "Measured so far" |
| `competition2006__adrift__ptgood__PTGOOD.taf` | `ptgood` | 6 | 1 | 1 | 0 | -- | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `The Plague - Redux.taf` | `plague` | 266 | 0 | 10 | 20 | yes | [The_Plague_Redux_walkthrough](The_Plague_Redux_walkthrough.md) **measured** 2026-09-06 -- desyncs early, stream misaligned (Adrift_174_plague.txt), see "Measured 2026-09-06" |
| `vetknow.taf` | `vetknow` | 228 | 0 | 15 | 38 | yes | [Veteran_Knowledge_walkthrough](Veteran_Knowledge_walkthrough.md) **done** 2026-09-06 -- clean in run400 but the ending tail (Adrift_170_vetknow.txt) |
| `TheCellar.taf` | `cellar` | 176 | 0 | 1 | 1 | yes | [TheCellar_walkthrough](TheCellar_walkthrough.md) **driven** 2026-09-06 -- the Runner lost a feed command (Adrift_172_cellar.txt); re-feed before reading anything into it, see "Measured 2026-09-06" |
| `mysteryofcaves.taf` | `mysteryofcaves` | 146 | 0 | 6 | 1 | yes | [mysteryofcaves_walkthrough](mysteryofcaves_walkthrough.md) **done** 2026-09-06 -- clean in run400 (Adrift_151_mysteryofcaves.txt) |
| `Space Boy's First Adventure.taf` | `space_boy` | 145 | 0 | 1 | 1 | -- | [Space_Boy_walkthrough](Space_Boy_walkthrough.md) **measured** -- see "Measured so far" |
| `vetknow2.taf` | `vetknow2` | 141 | 0 | 15 | 38 | yes | [Veteran_Knowledge_walkthrough](Veteran_Knowledge_walkthrough.md) **done** 2026-09-06 -- clean in run400 but the ending tail (Adrift_171_vetknow2.txt) |
| `shardsofmemory.taf` | `shardsofmemory` | 122 | 0 | 6 | 5 | yes | [Shards_of_Memory_walkthrough](Shards_of_Memory_walkthrough.md) **measured** 2026-09-06 -- desyncs early, stream misaligned (Adrift_169_shardsofmemory.txt), see "Measured 2026-09-06" |
| `man overboard.taf` | `man_overboard` | 99 | 0 | 5 | 0 | yes | [Man_Overboard_walkthrough](Man_Overboard_walkthrough.md) **measured** -- see "Measured so far" |
| `relojero.taf` | `relojero` | 88 | 0 | 0 | 2 | -- | [La_hija_del_relojero_walkthrough](La_hija_del_relojero_walkthrough.md) **measured** 2026-09-06 -- 1 turn differs (Adrift_133_relojero.txt), see "Measured 2026-09-06" |
| `salutations.taf` | `salutations` | 88 | 0 | 3 | 2 | yes | [Salutations_walkthrough](Salutations_walkthrough.md) **measured** 2026-09-06 -- 4 turns differ (Adrift_129_salutations.txt), see "Measured 2026-09-06" |
| `CBN.taf` | `cbn` | 82 | 0 | 1 | 0 | yes | [The_Revenge_Of_Clueless_Bob_Newbie_walkthrough](The_Revenge_Of_Clueless_Bob_Newbie_walkthrough.md) **measured** 2026-09-06 -- 5 turns differ (Adrift_149_cbn.txt), see "Measured 2026-09-06" |
| `forum2.taf` | `forum2` | 82 | 0 | 1 | 0 | yes | [Forum_2_walkthrough](Forum_2_walkthrough.md) **done** 2026-09-06 -- clean in run400 (Adrift_140_forum2.txt) |
| `asdfa.taf` | `asdfa` | 80 | 0 | 4 | 0 | yes | [ASDFA_walkthrough](ASDFA_walkthrough.md) **measured** 2026-09-06 -- 1 turn differs (Adrift_143_asdfa.txt), see "Measured 2026-09-06" |
| `mortality.taf` | `mortality` | 78 | 0 | 4 | 5 | yes | [Mortality_walkthrough](Mortality_walkthrough.md) **driven** 2026-09-06 -- the Runner lost a feed command (Adrift_168_mortality.txt); re-feed before reading anything into it, see "Measured 2026-09-06" |
| `princess1.taf` | `princess_in_the_tower` | 78 | 0 | 4 | 1 | -- | [Princess_In_The_Tower_walkthrough](Princess_In_The_Tower_walkthrough.md) **measured** -- see "Measured so far" |
| `Private Eye.taf` | `private_eye` | 74 | 0 | 0 | 0 | yes | [Private_Eye_walkthrough](Private_Eye_walkthrough.md) **measured** 2026-09-06 -- desyncs early, stream misaligned (Adrift_167_private_eye.txt), see "Measured 2026-09-06" |
| `AFDFR.taf` | `afdfr` | 73 | 0 | 32 | 17 | yes | [A_Fine_Day_For_Reaping_walkthrough](A_Fine_Day_For_Reaping_walkthrough.md) **done** 2026-09-06 -- clean in run400 but the ending tail (Adrift_165_afdfr.txt) |
| `chooseyourown.taf` | `chooseyourown` | 72 | 0 | 0 | 0 | yes | [chooseyourown_walkthrough](chooseyourown_walkthrough.md) **measured** 2026-09-06 -- desyncs early, stream misaligned (Adrift_160_chooseyourown.txt), see "Measured 2026-09-06" |
| `hauntedhouse.taf` | `hauntedhouse` | 72 | 0 | 4 | 1 | -- | [The_Haunted_House_of_Hideous_Horror_walkthrough](The_Haunted_House_of_Hideous_Horror_walkthrough.md) **measured** -- see "Measured so far" |
| `valley.taf` | `valley` | 72 | 0 | 6 | 0 | yes | [HappyValley_walkthrough](HappyValley_walkthrough.md) **done** 2026-09-06 -- clean in run400 (Adrift_166_valley.txt) |
| `yak_shaving.taf` | `yak_shaving` | 71 | 0 | 5 | 3 | yes | [Yak_Shaving_walkthrough](Yak_Shaving_walkthrough.md) **measured** -- see "Measured so far" |
| `unravel.taf` | `unraveling_god_lou` | 70 | 0 | 4 | 10 | yes | **done** 2026-09-06 -- clean in run400 (Adrift_164_unraveling_god_lou.txt) |
| `unravel.taf` | `unraveling_god` | 70 | 0 | 4 | 10 | yes | **done** 2026-09-06 -- clean in run400 (Adrift_163_unraveling_god.txt) |
| `lobster.taf` | `lobster` | 65 | 0 | 1 | 4 | -- | **measured** 2026-09-06 -- 6 turns differ (Adrift_161_lobster.txt), see "Measured 2026-09-06" |
| `Tear.taf` | `Tear` | 62 | 0 | 0 | 3 | -- | [Tears_of_a_Tough_Man_walkthrough](Tears_of_a_Tough_Man_walkthrough.md) **done** 2026-09-06 -- clean in run400 but the ending tail (Adrift_147_Tear.txt) |
| `cbn2.taf` | `cbn2` | 60 | 0 | 2 | 0 | yes | [The_Revenge_Of_Clueless_Bob_Newbie_2_walkthrough](The_Revenge_Of_Clueless_Bob_Newbie_2_walkthrough.md) **measured** 2026-09-06 -- 1 turn differs (Adrift_138_cbn2.txt), see "Measured 2026-09-06" |
| `imagi.taf` | `imagidroids` | 60 | 0 | 0 | 7 | yes | [ImagiDroids_walkthrough](ImagiDroids_walkthrough.md) **measured** 2026-09-06 -- 9 turns differ (Adrift_139_imagidroids.txt), see "Measured 2026-09-06" |
| `saffire.taf` | `saffire` | 58 | 0 | 0 | 1 | -- | [Saffire_walkthrough](Saffire_walkthrough.md) **driven** 2026-09-06 -- the Runner lost a feed command (Adrift_134_saffire.txt); re-feed before reading anything into it, see "Measured 2026-09-06" |
| `CD.taf` | `crimsondetritus` | 53 | 0 | 1 | 0 | yes | [CrimsonDetritus_walkthrough](CrimsonDetritus_walkthrough.md) **done** 2026-09-06 -- clean in run400 but the ending tail (Adrift_136_crimsondetritus.txt) |
| `exercise.taf` | `too_much_exercise` | 51 | 0 | 0 | 0 | -- | [Too_Much_Exercise_walkthrough](Too_Much_Exercise_walkthrough.md) **done** 2026-09-06 -- clean in run400 but the ending tail (Adrift_157_too_much_exercise.txt) |
| `marika.taf` | `marika` | 50 | 0 | 0 | 1 | yes | **done** 2026-09-06 -- clean in run400 but the ending tail (Adrift_158_marika.txt) |
| `second chance.taf` | `second_chance` | 50 | 0 | 23 | 9 | yes | [Second_Chance_walkthrough](Second_Chance_walkthrough.md) **measured** 2026-09-06 -- desyncs early, stream misaligned (Adrift_159_second_chance.txt), see "Measured 2026-09-06" |
| `Beanstalk.taf` | `beanstalk` | 49 | 0 | 3 | 1 | -- | -- **measured** -- see "Measured so far" |
| `goblinhunt.taf` | `goblinhunt` | 48 | 0 | 2 | 0 | yes | [Goblin_Hunt_walkthrough](Goblin_Hunt_walkthrough.md) **measured** 2026-09-06 -- 6 turns differ (Adrift_144_goblinhunt.txt), see "Measured 2026-09-06" |
| `shore.taf` | `shore` | 46 | 0 | 1 | 1 | -- | [The_Farthest_Shore_walkthrough](The_Farthest_Shore_walkthrough.md) **done** 2026-09-06 -- clean in run400 but the ending tail (Adrift_153_shore.txt) |
| `chicken.taf` | `chicken` | 45 | 0 | 2 | 0 | -- | [The_Evil_Chicken_of_Doom_walkthrough](The_Evil_Chicken_of_Doom_walkthrough.md) **done** 2026-09-06 -- clean in run400 but the ending tail (Adrift_141_chicken.txt) |
| `buried.taf` | `buried_alive` | 43 | 0 | 1 | 1 | -- | [Buried_Alive_walkthrough](Buried_Alive_walkthrough.md) **done** 2026-09-06 -- clean in run400 but the ending tail (Adrift_155_buried_alive.txt) |
| `Percy.taf` | `percy` | 41 | 0 | 1 | 1 | -- | [The_Saga_of_Percy_the_Viking_walkthrough](The_Saga_of_Percy_the_Viking_walkthrough.md) **done** 2026-09-06 -- clean in run400 (Adrift_125_percy.txt) |
| `marlin_affair.taf` | `marlin_affair` | 40 | 0 | 0 | 1 | yes | [Marlin_Affair_Prologue_walkthrough](Marlin_Affair_Prologue_walkthrough.md) **done** 2026-09-06 -- clean in run400 (Adrift_154_marlin_affair.txt) |
| `microbe_willie.taf` | `microbe_willie` | 40 | 0 | 2 | 2 | -- | [Microbe_Willie_vs_The_Rat_walkthrough](Microbe_Willie_vs_The_Rat_walkthrough.md) **done** 2026-09-06 -- clean in run400 but the ending tail (Adrift_132_microbe_willie.txt) |
| `pyramid.taf` | `pyramid` | 38 | 0 | 0 | 2 | yes | [The_Pyramid_of_Hamaratum_walkthrough](The_Pyramid_of_Hamaratum_walkthrough.md) **done** 2026-09-06 -- clean in run400 but the ending tail (Adrift_128_pyramid.txt) |
| `Confession(1).taf` | `confession` | 37 | 0 | 1 | 3 | yes | [Confession_walkthrough](Confession_walkthrough.md) **driven** 2026-09-06 -- the Runner lost a feed command (Adrift_148_confession.txt); re-feed before reading anything into it, see "Measured 2026-09-06" |
| `togetyou.taf` | `togetyou` | 34 | 0 | 1 | 8 | yes | [We_Are_Coming_To_Get_You_walkthrough](We_Are_Coming_To_Get_You_walkthrough.md) **measured** 2026-09-06 -- 2 turns differ (Adrift_146_togetyou.txt), see "Measured 2026-09-06" |
| `Griswold.taf` | `griswold` | 33 | 0 | 0 | 1 | yes | [Griswold_walkthrough](Griswold_walkthrough.md) **done** 2026-09-06 -- clean in run400 (Adrift_150_griswold.txt) |
| `endgame.taf` | `endgame` | 32 | 0 | 1 | 0 | -- | [The_Game_To_End_All_Games_walkthrough](The_Game_To_End_All_Games_walkthrough.md) **driven** 2026-09-06 -- the Runner lost a feed command (Adrift_127_endgame.txt); re-feed before reading anything into it, see "Measured 2026-09-06" |
| `frog.taf` | `frog` | 27 | 0 | 3 | 0 | -- | [The_Green_Princess_walkthrough](The_Green_Princess_walkthrough.md) **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `SPAM.taf` | `spam` | 27 | 0 | 2 | 3 | yes | [SPAM_walkthrough](SPAM_walkthrough.md) **done** 2026-09-05 -- run400 divergence found and FIXED, see "Measured so far" |
| `I am the Law.taf` | `law` | 26 | 0 | 5 | 3 | yes | [IAmTheLaw_walkthrough](IAmTheLaw_walkthrough.md) **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `topaz.taf` | `topaz` | 23 | 0 | 0 | 4 | yes | [Topaz_walkthrough](Topaz_walkthrough.md) **done** 2026-09-05 -- run400 found the exact-empty `InRoomDesc` rule (FIXED), then clean; see "Measured so far" |
| `Wreckage.taf` | `wreckage` | 23 | 0 | 0 | 2 | -- | [Wreckage_walkthrough](Wreckage_walkthrough.md) **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `ARGH_sGreatEscape.taf` | `argh` | 22 | 0 | 0 | 1 | -- | [ARGHs_Great_Escape_walkthrough](ARGHs_Great_Escape_walkthrough.md) **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `ShadricksTravels.taf` | `shadricks_travels` | 22 | 0 | 3 | 0 | -- | **done** 2026-09-05 -- run400 differed on ONE turn, the disambiguation wording, **ported 2026-09-07** and the golden re-blessed; see "Measured so far" |
| `1HRGAME.taf` | `masochists_heaven` | 20 | 0 | 0 | 0 | -- | [Masochists_Heaven_walkthrough](Masochists_Heaven_walkthrough.md) **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `Pieces of eden.taf` | `pieces_of_eden` | 20 | 0 | 1 | 3 | -- | [Pieces_of_eden_walkthrough](Pieces_of_eden_walkthrough.md) **driven** 2026-09-06 -- the Runner lost a feed command (Adrift_130_pieces_of_eden.txt); re-feed before reading anything into it, see "Measured 2026-09-06" |
| `longbarrow.taf` | `longbarrow` | 19 | 0 | 0 | 2 | -- | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `Vagabond.taf` | `vagabond` | 19 | 0 | 3 | 2 | yes | [Vagabond_walkthrough](Vagabond_walkthrough.md) **done** 2026-09-05 -- run400 was clean on re-drive 2026-09-07 (`Adrift_440`); the one ALR-over-a-joined-paragraph turn closed with the room-block port |
| `agent_4F[1].A.taf` | `agent4f` | 18 | 0 | 0 | 5 | -- | [Agent_4-F_from_Mars_walkthrough](Agent_4-F_from_Mars_walkthrough.md) **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `dancingevenhim.taf` | `dancing_even_him` | 17 | 0 | 0 | 1 | yes | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `Undefined1.taf` | `undefined` | 17 | 0 | 0 | 0 | -- | [Undefined_walkthrough](Undefined_walkthrough.md) **done** 2026-09-05 -- clean in run400 (POPUP_ANSWERS name dialog), see "Measured so far" |
| `outline.taf` | `outline` | 16 | 0 | 0 | 0 | -- | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `Pilfers.taf` | `pilfers` | 16 | 0 | 0 | 1 | yes | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `QuestI.taf` | `questi` | 16 | 0 | 0 | 1 | -- | [QuestI_walkthrough](QuestI_walkthrough.md) **done** 2026-09-05 -- clean in run400 through its death ending, see "Measured so far" |
| `raccoon.taf` | `raccoon` | 16 | 0 | 0 | 0 | yes | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `The_Stowaway.taf` | `stowaway` | 16 | 0 | 2 | 2 | -- | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `herrdoktor.taf` | `herrdoktor` | 15 | 0 | 0 | 1 | -- | **done** 2026-09-05 -- run400 found the third-person conjugation bug (FIXED), then clean; see "Measured so far" |
| `InMemory.taf` | `inmemory` | 15 | 0 | 0 | 9 | yes | [InMemory_walkthrough](InMemory_walkthrough.md) **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `MurderMansionntro.taf` | `murdermansionntro` | 15 | 0 | 0 | 0 | yes | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `Sandy.taf` | `sandy` | 15 | 0 | 0 | 0 | -- | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `shreddem.taf` | `shred_em` | 15 | 0 | 0 | 1 | -- | [Shred_Em_walkthrough](Shred_Em_walkthrough.md) **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `rollingthedough.taf` | `rollingthedough` | 13 | 0 | 1 | 3 | yes | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `Witness_Demon_vs_Vampire.taf` | `witnessdemon` | 13 | 0 | 0 | 0 | yes | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `TheAmulet.taf` | `the_amulet` | 12 | 0 | 0 | 3 | -- | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `The Dangers of Driving at Night.taf` | `dangersdrivingnight` | 11 | 0 | 4 | 0 | yes | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `MammothVacuum.taf` | `mammoth` | 11 | 0 | 1 | 0 | yes | [MammothVacuumButtonOfDeath_walkthrough](MammothVacuumButtonOfDeath_walkthrough.md) **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `headless.taf` | `headless` | 10 | 0 | 4 | 4 | yes | [TeenageHeadlessExperiment_walkthrough](TeenageHeadlessExperiment_walkthrough.md) **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `Sandy.taf` | `sandy_meta_number` | 10 | 0 | 0 | 0 | -- | **done** 2026-09-05 -- two DELIBERATE differences (SCARE meta-commands), see "Measured so far" |
| `The_Shuffling_Room.taf` | `shufflingroom` | 10 | 0 | 0 | 8 | -- | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `smote.taf` | `smote` | 9 | 0 | 0 | 0 | -- | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `The Foggy Banana Adventure.taf` | `foggybanana` | 8 | 0 | 3 | 1 | -- | **done** 2026-09-06 -- clean in run400 but the ending tail (Adrift_123_foggybanana.txt) |
| `The Fly Human.taf` | `flyhuman` | 7 | 0 | 0 | 3 | -- | **done** 2026-09-06 -- clean in run400 (Adrift_126_flyhuman.txt) |
| `hungry.taf` | `hungry` | 7 | 0 | 2 | 1 | -- | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `zombiecow.taf` | `zombiecow` | 7 | 0 | 0 | 2 | yes | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `asteroid_after.taf` | `asteroidafter` | 6 | 0 | 11 | 3 | yes | -- **measured** -- see "Measured so far" |
| `door.taf` | `door` | 5 | 0 | 0 | 1 | -- | [Door_walkthrough](Door_walkthrough.md) **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `Existence.taf` | `existence` | 5 | 0 | 1 | 1 | yes | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `Newton.taf` | `newton` | 5 | 0 | 0 | 1 | -- | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `Way Out.taf` | `wayout` | 5 | 0 | 0 | 0 | -- | **done** 2026-09-05 -- clean in run400 (staged as `wayout.taf`), see "Measured so far" |
| `zacksmackfoot.taf` | `zacksmackfoot` | 5 | 0 | 0 | 2 | yes | **measured** 2026-09-05 -- one OPEN run400 divergence, see "Measured so far" |
| `P2P.taf` | `p2p` | 4 | 0 | 0 | 4 | yes | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `hiker.taf` | `hiker` | 3 | 0 | 1 | 5 | -- | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `rift.taf` | `rift` | 3 | 0 | 0 | 1 | -- | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `Phoneb.taf` | `phoneb` | 2 | 0 | 0 | 0 | -- | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `ptbad.taf` | `ptbad` | 1 | 0 | 1 | 0 | -- | **done** 2026-09-05 -- clean in run400, and the probe game for the three library-message corrections; see "Measured so far" |
| `Cut_the_Red_Wire.taf` | `redwire` | 1 | 0 | 1 | 0 | yes | [CutTheRedWire_walkthrough](CutTheRedWire_walkthrough.md) **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `The Vault.taf` | `vault` | 1 | 0 | 1 | 1 | -- | **done** 2026-09-05 -- clean in run400, see "Measured so far" |

### 4.00 — 165 more games (found in the corpus 2026-09-06, all driven)

Every 4.00 row in `run_v4_walkthroughs.sh` that the 124-game table above did
not list, less the 11 `SCR_SEED` and 2 `SCR_ASSUME` rows.  Feeds are
`cmdfile_q_<solution>.txt`; `dreamquest` has no transcript because run400
refuses to load it.

| solution | .taf | transcript | verdict | first difference |
|---|---|---|---|---|
| `ADRIFTMAS_Party` | ADRIFTMAS_Party.taf | `Adrift_287_ADRIFTMAS_Party.txt` | diff 2 | T2 `make snowball`: run400 'You scoop up some of the fresh fallen snow and pack it into a nice fir' vs scarier 'You gather a large handful of snow and form it into a loosly packed ba' |
| `aegis` | Aegis.taf | `Adrift_265_aegis.txt` | diff 4 | T24 `examine netting`: run400 'A tangled pile of netting, most of it damaged past repair. An old net ' vs scarier 'A tangled pile of netting, most of it damaged past repair. An old net ' |
| `albert_is_lost` | Albert is Lost! An Adventure in Real Life.taf | `Adrift_214_albert_is_lost.txt` | diff 22 | T3 `e`: run400 'Tiberius goes east. Fortune Teller 5000 Tiberius has now ventured into' vs scarier 'Tiberius blunders off to the east. Fortune Teller 5000 Tiberius has no' |
| `aliasagent` | Alias Undercover Agent.taf | `Adrift_346_aliasagent.txt` | lost-cmd | 1 lost, first feed[39] `score` |
| `amy` | amy.taf | `Adrift_348_amy.txt` | diff 1 | T17 `fuck amy's pussy`: run400 'You take Amy in your arms and lay her down on the bed. She looks up at' vs scarier 'You take Amy in your arms and lay her down on the bed. She looks up at' |
| `apokalupsis` | apokalupsis.taf | `Adrift_241_apokalupsis.txt` | endtail 1 | T45 `go west`: run400 'Thank you for playing the introduction to Apokalupsis. I hope that you' vs scarier 'Thank you for playing the introduction to Apokalupsis. I hope that you' |
| `backhome` | Back Home.taf | `Adrift_247_backhome.txt` | diff 4 | T36 `d`: run400 'You move down. On the Ladder to the Attic You are perched on a ladder,' vs scarier 'You move down. On the Ladder to the Attic You are perched on a ladder,' |  **RNG 2026-09-07** -- T36 is gone; the last divergence was the T52 telephone chain, and `SCR_SEED=3` matches run400 exactly.
| `bandera` | Bandera.taf | `Adrift_232_bandera.txt` | RESOLVED 2026-09-07 | T18 `x marife` was the case-sensitive tail of the character resolver, not the seen model -- see "Ported 2026-09-07: the character resolver's case-sensitive tail" below.  Row now `endtail 1` (the final keypress prompt only). |
| `barneysproblem` | BarneysProblem.taf | `Adrift_302_barneysproblem.txt` | diff 6 | T9 `w`: run400 'You move west. Front Room Your front room is every bit as dismal and g' vs scarier 'You move west. Front Room Your front room is every bit as dismal and g' |
| `baroo` | baroo.taf | `Adrift_314_baroo.txt` | diff 4 | T33 `ask brogo about temple`: run400 'Brogo looks at the wizard from the village, "Did you not tell the anci' vs scarier 'Brogo looks at the wizard from the village, "Did you not tell the anci' |
| `beer` | beer.taf | `Adrift_269_beer.txt` | diff 22 | T9 `west`: run400 'You move west. Fountain You are at the public water fountain. You can ' vs scarier 'You move west. Fountain You are at the public water fountain. You can ' |
| `bigcitylaundry` | Big City Laundry.taf | `Adrift_292_bigcitylaundry.txt` | diff 2 | T1 `get socks`: run400 'You take pair of socks from your closet.' vs scarier 'You take pair of socks from your closet. Your feet are freezing! Put s' |
| `blast` | blast.taf | `Adrift_204_blast.txt` | diff 10 | T3 `n`: run400 'You move north. Filing In front of you is a counter which basically cu' vs scarier 'You move north. Filing In front of you is a counter which basically cu' |
| `blood` | blood.taf | `Adrift_278_blood.txt` | diff 34 | T2 `w`: run400 'You move west. London Road Lined by sad little houses you cannot help ' vs scarier 'You move west. London Road Lined by sad little houses you cannot help ' |
| `bloodrelatives` | Blood_Relatives.taf | `Adrift_179_bloodrelatives.txt` | ws-only | separator only; transcript writer drops <centre> breaks |
| `boiledeggs` | boiled eggs.taf | `Adrift_198_boiledeggs.txt` | endtail 1 | T18 `open box`: run400 'You open the box. You summon the willpower to keep the box shut until ' vs scarier 'You open the box. You summon the willpower to keep the box shut until ' |
| `briefcase` | briefcase.taf | `Adrift_199_briefcase.txt` | diff 4 | T5 `z`: run400 'Time passes... A whine escapes the mouth of the dog - he seems to be b' vs scarier 'Time passes...' |
| `bsg22` | BSG TWENTY TWO Final.taf | `Adrift_191_bsg22.txt` | diff 1 | T13 `fuck tricia's ass with spike`: run400 'You step back as Tricia turns around, sticking out her ass and leaning' vs scarier 'You step back as Tricia turns around, sticking out her ass and leaning' |
| `businessasusual` | Business As Usual.taf | `Adrift_209_businessasusual.txt` | diff 9 | T3 `wait`: run400 'You wait a bit... Yellow lights flicker. Somebody grabbed the lamp!' vs scarier 'You wait a bit... Blue lights flash. The book has been taken!' |
| `camelot15` | Camelot 1,5.taf | `Adrift_248_camelot15.txt` | diff 1 (DONE 2026-09-06, room-content listing; only `[Press any key to end]` left) | T32 `go north`: run400 'You gently push the silk curtains aside and walk into the adjacent roo' vs scarier 'You gently push the silk curtains aside and walk into the adjacent roo' |
| `choosethreehour` | Choose_Your_Own_Three_Hour_Adventure.taf | `Adrift_190_choosethreehour.txt` | clean |  |
| `cluelessbob` | In_the_Claws_of_Clueless_Bob.taf | `Adrift_234_cluelessbob.txt` | clean |  |
| `cobl` | COBL.taf | `Adrift_286_cobl.txt` | diff 22 | T0 `3`: run400 'After the ending of the last adventure game you played left you feelin' vs scarier 'After the ending of the last adventure game you played left you feelin' |
| `cowboyblues` | CowboyBlues.taf | `Adrift_330_cowboyblues.txt` | lost-cmd | 12 lost, first feed[248] `nw` -- T143 `x wall` "You see no such thing." (tie) matched 2026-09-06, see the harness row |
| `crookedestate` | The Crooked Estate.taf | `Adrift_242_crookedestate.txt` | lost-cmd | 1 lost, first feed[44] `save` |
| `cursed` | cursed.taf | `Adrift_334_cursed.txt` | diff 40 | T57 `z`: run400 'You wait for something to happen. Your vulpine hearing detects some so' vs scarier 'You wait for something to happen. Your vulpine hearing detects some so' |
| `darkness` | darkness.taf | `Adrift_290_darkness.txt` | lost-cmd | 11 lost, first feed[100] `z` |
| `datewithdeath` | datewithdeath.taf | `Adrift_333_datewithdeath.txt` | lost-cmd | 1 lost, first feed[5] `book` |
| `dayattheoffice` | DayAtTheOffice.taf | `Adrift_231_dayattheoffice.txt` | clean |  |
| `deadman` | The Dead Man.taf | `Adrift_252_deadman.txt` | diff 13 | T11 `open kit`: run400 'You open the first aid kit. Among the usual things you would find in a' vs scarier 'You open the first aid kit. Among the usual things you would find in a' |
| `deadreckoning` | DeadReckoning.taf | `Adrift_218_deadreckoning.txt` | clean |  |
| `del_sol` | Del Sol.taf | `Adrift_249_del_sol.txt` | diff 2 | T44 `z`: run400 'Time passes... WOW. Ms moreland just accidentally knocked over the lab' vs scarier 'Time passes... WOW. Ms moreland just accidentally knocked over the lab' |
| `digby` | For_Love_of_Digby.taf | `Adrift_284_digby.txt` | clean |  |
| `donuts_intro` | donuts_intro.taf | `Adrift_176_donuts_intro.txt` | lost-cmd | 1 lost, first feed[1] `hide` |
| `doortoutopia` | Door to Utopia, The.taf | `Adrift_253_doortoutopia.txt` | endtail 1 | T58 `w`: run400 'You step into the doorway and find yourself in a wonderful place: the ' vs scarier 'You step into the doorway and find yourself in a wonderful place: the ' |
| `dreamquest` | Dream Quest.taf | `Adrift_315_dreamquest.txt` | lost-cmd | 20 lost, first feed[0] `n` |
| `drinks` | Drinks.taf | `Adrift_239_drinks.txt` | endtail 1 | T17 `open casket`: run400 'The lock of the casket quietly clicked open, and I lifted the lid curi' vs scarier 'The lock of the casket quietly clicked open, and I lifted the lid curi' |
| `dusk` | dusk.taf | `Adrift_221_dusk.txt` | endtail 1 | T32 `x sapling`: run400 'You decide to look more closely, take another step toward the sapling.' vs scarier 'You decide to look more closely, take another step toward the sapling.' |
| `easter` | easter.taf | `Adrift_273_easter.txt` | diff 12 | T5 `x newspaper rack`: run400 'The newspaper rack is clearly more functional than aesthetic, construc' vs scarier 'The newspaper rack is clearly more functional than aesthetic, construc' |
| `egghunt` | Egg_Hunt.taf | `Adrift_245_egghunt.txt` | lost-cmd | 1 lost, first feed[51] `score` |
| `elascensor` | El ascensor.taf | `Adrift_184_elascensor.txt` | endtail 1 | T9 `abrir trampilla con la tapa del boligrafo`: run400 '¡Pues la verdad es que no es mala idea!... haciendo equilibrio sobre e' vs scarier '¡Pues la verdad es que no es mala idea!... haciendo equilibrio sobre e' |
| `escape_to_new_york` | EscapeToNewYork.taf | `Adrift_323_escape_to_new_york.txt` | diff 40 | T18 `east`: run400 'You move east. Crew Mess Hall, E Deck Unlike the passenger parts of th' vs scarier 'You move east. Crew Mess Hall, E Deck Unlike the passenger parts of th' |
| `finalquestion` | The_Final_Question.taf | `Adrift_193_finalquestion.txt` | clean |  |
| `firstpug` | The First To Arise Alone With A Pug.taf | `Adrift_215_firstpug.txt` | endtail 1 | T30 `open front door with danthil`: run400 'Summoning Danthil's power to enhance your strength, you tug at the jam' vs scarier 'Summoning Danthil's power to enhance your strength, you tug at the jam' |
| `fluffykins` | Mr_Fluffykins_Most_Harrowing_Misadventure.taf | `Adrift_178_fluffykins.txt` | clean |  |
| `foresthouse2` | TheForestHouse_2.taf | `Adrift_225_foresthouse2.txt` | endtail 1 | T33 `examine mirror`: run400 'The mirror is covered in dust. You wipe the dust away with your sleeve' vs scarier 'The mirror is covered in dust. You wipe the dust away with your sleeve' |
| `foresthouse3` | ForestHouse3.taf | `Adrift_264_foresthouse3.txt` | diff 8 | T48 `d`: run400 'You continue to descend the stairs... You get the feeling that somethi' vs scarier 'You continue to descend the stairs...' -- golden re-blessed 2026-09-06 with SCR_SKIP_WAITKEY=1 (the old golden was a stuck-on-the-porch run); still diff 8, first at T48 |
| `forum` | forum.taf | `Adrift_261_forum.txt` | endtail 1 | T19 `1`: run400 'You pull out Lancer Sykera from the depths of your pockets. Woodfish r' vs scarier 'You pull out Lancer Sykera from the depths of your pockets. Woodfish r' |
| `frustrated` | frustrated.taf | `Adrift_274_frustrated.txt` | endtail 1 | was diff 4; T53-T55 FIXED 2026-09-06 by moving the 4.0 implicit take ahead of the put handler's task look-up (see "Measured so far") |
| `fullcircle` | Full_Circle.taf | `Adrift_322_fullcircle.txt` | diff 40 | T43 `get all`: run400 'You take the helm and the locket. You take the branch.' vs scarier 'You take the branch. You take the helm and the locket.' |
| `ghosttown` | Ghost town v1,05.taf | `Adrift_325_ghosttown.txt` | diff 34 | T2 `n`: run400 'You move north. Bedroom As you enter the bedroom you are surprised by ' vs scarier 'You move north. Bedroom As you enter the bedroom you are surprised by ' -- diff 23 after the 2026-09-06 seen/resolver port (T2 is the kerosene-lamp listing, unchanged); `x posters` and the dusk-event tick now match |
| `Glum_Fiddle` | Glum Fiddle.taf | `Adrift_289_Glum_Fiddle.txt` | lost-cmd | 1 lost, first feed[1] `say cow` |
| `gmylm` | GMYLM_2010.taf | `Adrift_259_gmylm.txt` | endtail 1 | T64 `pull strings`: run400 'I look around, and am pleased to see the bullies are done. It's clear ' vs scarier 'I look around, and am pleased to see the bullies are done. It's clear ' |
| `gorxungula` | gorxungula.taf | `Adrift_343_gorxungula.txt` | diff 10 | T3 `w`: run400 'You blunder off to the west.' vs scarier 'You blunder off to the west. You are dead! I'm afraid you are dead! Yo' |
| `greekschool` | Greek School Adventure.taf | `Adrift_349_greekschool.txt` | diff 40 | T0 `get up`: run400 'You stand up from that.' vs scarier 'You stand up from your bed.' |
| `grumble` | Whatever_Happened_to_Uncle_Grumble.taf | `Adrift_329_grumble.txt` | lost-cmd | 1 lost, first feed[262] `y` |
| `halloweenhijinks` | HalloweenHijinks.taf | `Adrift_280_halloweenhijinks.txt` | clean |  |
| `hcw` | hcw.taf | `Adrift_326_hcw.txt` | diff 40 | T81 `turn on intercom`: run400 'You can't see the intercom.' vs scarier 'You can't turn that.' |
| `helsing` | Helsing.taf | `Adrift_181_helsing.txt` | endtail 1 | T7 `put beads on dance floor`: run400 'You shake the beads in your fist like a pair of lucky dice and roll th' vs scarier 'You shake the beads in your fist like a pair of lucky dice and roll th' |
| `hero` | competition2004__adrift__hero__hero.taf | `Adrift_294_hero.txt` | diff 7 | T38 `undo`: run400 'Undone. Time passes...' vs scarier 'Inside The Shadowy Milk Factory On Chabbow Street [The previous turn h' |
| `howitstarted` | howitstarted.taf | `Adrift_212_howitstarted.txt` | lost-cmd | 1 lost, first feed[28] `score` |
| `hub` | hub.taf | `Adrift_293_hub.txt` | diff 16 | T35 `take watch`: run400 'I take my watch. I'm beginning to wish I was fully clothed.' vs scarier 'I take my watch.' |
| `iachini` | iachini.taf | `Adrift_327_iachini.txt` | diff 21 | T27 `read card in mirror`: run400 'You hold the index card up to the mirror and read the reflection. You ' vs scarier 'You hold the index card up to the mirror and read the reflection. You ' |
| `icecream` | IceCream.taf | `Adrift_177_icecream.txt` | ~~diff 3~~ DONE 2026-09-07 | T0 `take cone`: run400 'You already have an empty cone.' vs scarier 'You are already carrying the cone.' |
| `igor` | igor.taf | `Adrift_201_igor.txt` | endtail 1 | T21 `press 4th switch`: run400 'The MONSTER LIVES ! Well done, The Master has created a better servant' vs scarier 'The MONSTER LIVES ! Well done, The Master has created a better servant' |
| `ilgolem` | Il Golem.taf | `Adrift_281_ilgolem.txt` | endtail 1 | T88 `leggi libro`: run400 'Apri il libro Golem per Dummies, sulla prima pagina c'è una dedica di ' vs scarier 'Apri il libro Golem per Dummies, sulla prima pagina c'è una dedica di ' |
| `jailbreakbob` | jailbreakbob.taf | `Adrift_233_jailbreakbob.txt` | endtail 1 | T30 `n`: run400 'As you approach the gate with the gun, you experience a moment's worry' vs scarier 'As you approach the gate with the gun, you experience a moment's worry' |
| `JGrim` | JGrim1.0.taf | `Adrift_310_JGrim.txt` | diff 5 | T48 `wait`: run400 'Time passes...' vs scarier 'Time passes... The maid vacuums away, but as the cleaner goes over the' |
| `jimpond` | JimPond.taf | `Adrift_300_jimpond.txt` | diff 1 | T29 `look under desk`: run400 'The underside of the desk seems strangely empty now I've removed the b' vs scarier 'Two desks, both of them smashed beyond repair. Whatever was on top of ' |
| `jinxtron` | JINXTRON.taf | `Adrift_344_jinxtron.txt` | diff 1 | T6 `jinxtron`: run400 'No, don't even think about sayin' jinx until we say the same word at t' vs scarier 'No, don't even think about sayin' jinx until we say the same word at t' |
| `jinxtron_full` | JINXTRON.taf | `Adrift_345_jinxtron_full.txt` | diff 6 | T7 `EDAM`: run400 'PLATYPUS' vs scarier 'Oh, man, I hope you don't jinx me.' |
| `justanotherday` | Just Another Day.taf | `Adrift_303_justanotherday.txt` | clean |  |
| `lair` | Lair of the Vampire.taf | `Adrift_332_lair.txt` | diff 40 | T9 `open door`: run400 'The door appears already open. > Try something different.' vs scarier 'Sitting slumped against the cold stone wall, Vardo is not a well man. ' |
| `lca` | Lights_Camera_Action.taf | `Adrift_328_lca.txt` | diff 2 | T91 `chop tree`: run400 'Which tree. The tree or the tree?' vs scarier 'Whatever you're trying to do, you can't. Either check out the Film Dir' |
| `magicshow` | magicshow.taf | `Adrift_351_magicshow.txt` | diff 30 | T0 `say abracadabra`: run400 '"Abracadabra!" You feel reality shift ever so slightly. [Your magic ra' vs scarier '"Abracadabra!" You feel reality shift ever so slightly. [Your magic ra' |
| `mangiasaur` | Mangiasaur.taf | `Adrift_279_mangiasaur.txt` | diff 40 | T11 `eat bud`: run400 'You stoop to bite one off the branch and catch a whiff of the sweet po' vs scarier 'You stoop to bite one off the branch and catch a whiff of the sweet po' |
| `mindofmaster` | competition2007__adrift__mindofmaster__mind of master.taf | `Adrift_213_mindofmaster.txt` | clean |  |
| `mishmash` | mishmash.taf | `Adrift_318_mishmash.txt` | diff 2 | T191 `z`: run400 'Time passes... You hear memoryblam call out from the west, "We don't h' vs scarier 'Time passes... You hear memoryblam call out from the west, "Let's fini' |
| `motion` | Motion.taf | `Adrift_305_motion.txt` | diff 15 | T7 `next`: run400 'The Rocket Launch Fuel Remaining: [ \| ! ] [ ! \| ! ] [ ! \| ! ] [ ! \| ! ' vs scarier 'The Rocket Launch Fuel Remaining: [ \| ! ] [ ! \| ! ] [ ! \| ! ] [ ! \| ! ' |
| `mould` | mould.taf | `Adrift_335_mould.txt` | lost-cmd | 20 lost, first feed[18] `y` |
| `mustescape` | mustescape.taf | `Adrift_271_mustescape.txt` | lost-cmd | 20 lost, first feed[2] `punch` |
| `mutaydid` | mutaydid.taf | `Adrift_210_mutaydid.txt` | diff 3 | T20 `attack mystery meat with cleaver`: run400 'With frantic, wide swings you cleave a ham from off the mystery meat, ' vs scarier 'With frantic, wide swings you cleave a ham from off the mystery meat, ' |
| `neighbours` | neighbours.taf | `Adrift_258_neighbours.txt` | clean |  |
| `oldchurch` | The Old Church.taf | `Adrift_194_oldchurch.txt` | endtail 1 | T17 `give sword to mouse`: run400 'You give the sword to the mouse. She says: "Thank you. Let's hope for ' vs scarier 'You give the sword to the mouse. She says: "Thank you. Let's hope for ' |
| `onnafa` | ONNAFA.TAF | `Adrift_316_onnafa.txt` | diff 40 | T13 `talk to stimmons`: run400 '"An honour to serve, sir," remarks Stimmons and gives you a proud salu' vs scarier '"An honour to serve, sir," remarks Stimmons and gives you a proud salu' |
| `overtheedge` | Over the Edge1.0.taf | `Adrift_263_overtheedge.txt` | diff 2 | T1 `x men`: run400 'The men mill around, leaning on their rifles, talking in low voices. T' vs scarier 'The men mill around, leaning on their rifles, talking in low voices. T' |
| `paint` | Paint.taf | `Adrift_270_paint.txt` | diff 10 | T16 `call mertle`: run400 'You hear the sound of sighing down the corridor then the receptionist ' vs scarier 'You hear the sound of sighing down the corridor then the receptionist ' |
| `patient7` | Patient7.taf | `Adrift_254_patient7.txt` | lost-cmd | 1 lost, first feed[57] `wait` |
| `perfectspy` | The Perfect Spy.taf | `Adrift_272_perfectspy.txt` | endtail 1 | T19 `n`: run400 'You run away from the cat and out of the alley. For the first few mome' vs scarier 'You run away from the cat and out of the alley. For the first few mome' |
| `perspectives` | perspectives.taf | `Adrift_240_perspectives.txt` | endtail 1 (was diff 2; FIXED 2026-09-07 -- the room lister now builds the Runner's one concatenated string, so the ALR `' Also here is a gun. '` matches) | T0 `look`: run400 'Locked In A Bathroom It's a terribly small bathroom, with scarcely muc' vs scarier 'Locked In A Bathroom It's a terribly small bathroom, with scarcely muc' |
| `pestilence` | pestilence.taf | `Adrift_276_pestilence.txt` | diff 5 | T38 `read card`: run400 'The record card has lots of medical mumbo-jumbo but you can make out t' vs scarier 'The record card has lots of medical mumbo-jumbo but you can make out t' |
| `petespunkin` | Pete's Punkin Junkinator.taf | `Adrift_208_petespunkin.txt` | endtail 1 | T26 `pull crank`: run400 'The sound makes you nervous, like stepping on broken glass, and the fe' vs scarier 'The sound makes you nervous, like stepping on broken glass, and the fe' |
| `picture` | Picture.taf | `Adrift_121_picture.txt` | diff 2 | T0 `sit on bench`: run400 'You sit down on the wooden bench and hear a voice coming from the pict' vs scarier 'You sit down on the wooden bench and hear a voice coming from the pict' |
| `provenance` | provenance.taf | `Adrift_342_provenance.txt` | diff 40 | T3 `g`: run400 '(follow the blood) You follow the drops of blood northwest along the b' vs scarier '(follow the blood) You follow the drops of blood northwest along the b' |
| `puzzlebox` | puzzlebox.taf | `Adrift_275_puzzlebox.txt` | diff 40 | T7 `push button`: run400 'You carefully press the red button. Nothing seems to happen.' vs scarier 'You carefully press the red button. You hear a click from within the b' |
| `r2dc` | R2DC.taf | `Adrift_185_r2dc.txt` | endtail 1 | T10 `climb down cable`: run400 'You grab a hold of the cable and slide down to the great hall below in' vs scarier 'You grab a hold of the cable and slide down to the great hall below in' |
| `rain` | rain.taf | `Adrift_222_rain.txt` | endtail 1 | T32 `unlock the shackles`: run400 'Rain gazes at you with renewed hope, her eyes now clear and gleaming a' vs scarier 'Rain gazes at you with renewed hope, her eyes now clear and gleaming a' |
| `reactor1` | reactor_1.taf | `Adrift_186_reactor1.txt` | diff 1 | T10 `1`: run400 'A quick glance at the computer tells you that the vent mechanism has b' vs scarier 'A quick glance at the console tells you that the vent mechanism has be' |
| `regrets` | Regrets.taf | `Adrift_188_regrets.txt` | clean |  |
| `reluctantvampire` | The_Reluctant_Vampire.taf | `Adrift_317_reluctantvampire.txt` | lost-cmd | 1 lost, first feed[189] `fang` |
| `requiem` | competition2006__adrift__requiem__requiem.taf | `Adrift_266_requiem.txt` | clean |  |
| `riding_home` | Riding_Home.taf | `Adrift_350_riding_home.txt` | lost-cmd | 6 lost, first feed[11] `wait` |
| `rking` | rking.taf | `Adrift_291_rking.txt` | lost-cmd | 1 lost, first feed[18] `nudge something odd` |
| `rockband` | Rock Band.taf | `Adrift_203_rockband.txt` | diff 12 | T6 `use green button`: run400 'You hit green! You got it! Score: 10 You see a red note!' vs scarier 'You hit green! You got it! Score: 10 You see a yellow note!' |
| `scandal` | Scandal.taf | `Adrift_243_scandal.txt` | lost-cmd | 20 lost, first feed[2] `single shot` |
| `seaside` | ADayAtTheSeaside.taf | `Adrift_236_seaside.txt` | diff 6 | T25 `do form`: run400 'You must be in the same room as the leisure access card form to be abl' vs scarier 'I don't understand what you want me to do with the completed form.' |
| `secidenoddcomp` | seciden_oddcomp.taf | `Adrift_267_secidenoddcomp.txt` | diff 6 | T6 `n`: run400 'You move north. Living Room Though it still retains a feeling of empti' vs scarier 'You move north. Living Room Though it still retains a feeling of empti' |
| `sexismental` | Sex is Mental.taf | `Adrift_223_sexismental.txt` | endtail 1 | T32 `fuck pussy`: run400 'You plant your lips on Mary before sliding you hands up her legs and a' vs scarier 'You plant your lips on Mary before sliding you hands up her legs and a' |
| `shadow_of_the_past` | Shadow_Of_The_Past.taf | `Adrift_205_shadow_of_the_past.txt` | diff 2 | T19 `get crown`: run400 'As you grab the crown, you notice the beast inside start to stir. Your' vs scarier 'As you grab the crown, you notice the beast inside start to stir. Your' |
| `ShadricksUnderground` | ShadricksUnderground.taf | `Adrift_285_ShadricksUnderground.txt` | diff 11 | T81 `put large boulder on short plinth` FIXED 2026-09-06 with `frustrated` T53; first difference now T42 `ne`: run400 'I move northeast. A Dark Tunnel This tunnel is overrun with bat droppi' vs scarier 'I move northeast. A Dark Tunnel This tunnel is overrun with bat droppi' |
| `shetland` | The_Shetland_Enigma.taf | `Adrift_260_shetland.txt` | endtail 1 | T65 `mount bike`: run400 'You mount the little pod-bike, and draw its protective shield about yo' vs scarier 'You mount the little pod-bike, and draw its protective shield about yo' |
| `showtime` | Showtime_at_the_Gallows.taf | `Adrift_312_showtime.txt` | FIXED 2026-09-06 | T59 `get her hand` `(No female)`: ported, golden re-blessed.  A re-drive still reports a first difference at the same turn, but that is the feed's two blank `<waitkey>` lines drifting the streams (`z` re-synchronises two turns later), not the echo |
| `sigurd` | Sigurd_Fafnesbane.taf | `Adrift_189_sigurd.txt` | endtail 1 | T11 `kill regin`: run400 'You kill your deceitful stepfather. Regin falls dead over his anvil. Y' vs scarier 'You kill your deceitful stepfather. Regin falls dead over his anvil. Y' |
| `skydiver` | The_Skydiver.taf | `Adrift_246_skydiver.txt` | **FIXED 2026-09-07** (a Hidden walk stop stamps the location whether or not the walker moved) | T15 `z`: run400 'Time passes... Pelican A pelican flocked toward me..' vs scarier 'Time passes...' |
| `spooked` | Spooked_The_Wonders_of_Science.taf | `Adrift_226_spooked.txt` | clean |  |
| `spot_of_bother` | A_Spot_of_Bother.taf | `Adrift_331_spot_of_bother.txt` | diff 12 | T138 `sprinkle eye of toad into cauldron`: run400 'You sprinkle some of the eye of toad into the cauldron. The cauldron s' vs scarier 'You sprinkle some of the eye of toad into the cauldron. The cauldron b' |
| `sswhore` | ss whore.taf | `Adrift_304_sswhore.txt` | lost-cmd | 1 lost, first feed[135] `score` |
| `stationxiii` | Station_XIII.taf | `Adrift_283_stationxiii.txt` | diff 4 | T25 `take laser cutter`: run400 'You take the laser cutter.' vs scarier 'You take the laser cutter. Something wet lands on your nose...' |
| `suburbanprodigy3` | MikeDesert_SuburbanProdigy3.taf | `Adrift_219_suburbanprodigy3.txt` | clean (DONE 2026-09-07, `stats` is not a Runner command; only `[Press any key to end]` left) | T31 `stats`: both 'Listen dude, you've played these games before. Step it up! You scored 80 out of the maximum 80!' |
| `sun_empire` | Sun_Empire_Quest_For_The_Founders.taf | `Adrift_277_sun_empire.txt` | lost-cmd | 2 lost, first feed[82] `quit` |
| `suzypowers` | competition2011__adrift__powers__how suzy got her powers.taf | `Adrift_216_suzypowers.txt` | diff 1 | T30 `lift beam`: run400 'You place one end of the trident under the beam and say to the woman, ' vs scarier 'You place one end of the trident under the beam and say to the woman, ' |
| `takeone` | takeone.taf | `Adrift_202_takeone.txt` | diff 1 (DONE 2026-09-06, room-content listing; only `[Press any key to end]` left) | T4 `s`: run400 'Indianette Jones moves south. Ruined Statue (on screen 3) Above Indian' vs scarier 'Indianette Jones moves south. Ruined Statue (on screen 3) Above Indian' |
| `target` | target.taf | `Adrift_224_target.txt` | diff 18 | T0 `1`: run400 'Roof of the Building You are on the roof of the gothic revival Appleto' vs scarier 'Roof of the Building You are on the roof of the post-modern Bakewell I' |
| `templeofthesun` | Temple_Of_The_Sun.taf | `Adrift_217_templeofthesun.txt` | endtail 1 | T30 `wear robes and headdress`: run400 'You quickly don the golden headdress and colorful robe. You finally ge' vs scarier 'You quickly don the golden headdress and colorful robe. You finally ge' |
| `tenebraesemper` | TenebraeSemper.taf | `Adrift_227_tenebraesemper.txt` | clean |  |
| `terrified` | Terrified.taf | `Adrift_251_terrified.txt` | clean |  |
| `the_demon_hunter` | TheDemonHunter.taf | `Adrift_229_the_demon_hunter.txt` | diff 4 | T30 `kill hajar`: run400 'You swing your spear high above your head and swing down hard, strikin' vs scarier 'Lashing out with all your might, you slice a deep gash in Hajar's side' |
| `the_town_of_azra` | The_Town_Of_Azra.taf | `Adrift_339_the_town_of_azra.txt` | diff 9 | T6 `gulp the coffee`: run400 'You tip the cup back and take a medium-sized gulp of the coffee. There' vs scarier 'You tip the cup back and take a medium-sized gulp of the coffee. There' |
| `TheADRIFTProject` | TheADRIFTProject.taf | `Adrift_341_TheADRIFTProject.txt` | lost-cmd | 11 lost, first feed[3] `open door` |
| `thehunter` | The_Hunter.taf | `Adrift_347_thehunter.txt` | diff 1 | T59 `fire ballista`: run400 'The ballista fires with a bang, and the bolt soars through the air, th' vs scarier 'The ballista fires with a bang, and the bolt soars through the air, th' |
| `thelasthour` | thelasthour.taf | `Adrift_297_thelasthour.txt` | lost-cmd | 6 lost, first feed[119] `wait`.  T80 `ask sly about him` gave up the ask/talk-to `about` split, FIXED 2026-09-06 |
| `theroad` | the_road.taf | `Adrift_282_theroad.txt` | clean |  |
| `theseance` | The_Seance.taf | `Adrift_195_theseance.txt` | endtail 1 | T17 `yes`: run400 '"I am so happy my love" Emily asks for your hand, and taking one final' vs scarier '"I am so happy my love" Emily asks for your hand, and taking one final' |
| `thesisters` | TheSisters.taf | `Adrift_308_thesisters.txt` | diff 6 | T84 `get shoes`: run400 'You take the shoes from the tall wardrobe.' vs scarier 'You take the shoes from the tall wardrobe. You pause, a chill running ' |
| `thorn` | Thorn.taf | `Adrift_230_thorn.txt` | endtail 1 | T27 `x thorn`: run400 'You see what you feared, on what had been the bare branches of the tho' vs scarier 'You see what you feared, on what had been the bare branches of the tho' |
| `threeminutes` | 3 minutes1.0.taf | `Adrift_211_threeminutes.txt` | diff 2 | T8 `press button`: run400 'You reach out and press the blue button. The saw whines loudly, then r' vs scarier 'You reach out and press the blue button. The saw whines loudly, then r' |
| `ticktick` | ticktick.taf | `Adrift_220_ticktick.txt` | diff 9 | T0 `s`: run400 'You move south. Living Room This sparsely furnished room is where you ' vs scarier 'You move south. Living Room This sparsely furnished room is where you ' |
| `tictactoe` | Tic-Tac-Toe.taf | `Adrift_183_tictactoe.txt` | endtail 1 | T7 `7`: run400 'filled 7 Gathering Of The Gods You are in the gathering of the Gods. T' vs scarier 'filled 7 Gathering Of The Gods You are in the gathering of the Gods. T' |
| `to_hell_in_a_hamper` | Hamper.taf | `Adrift_262_to_hell_in_a_hamper.txt` | diff 3 | T30 `look`: run400 'In the basket of a balloon I am in the basket of the balloon, high, th' vs scarier 'In the basket of a balloon I am in the basket of the balloon, high, th' |
| `tophat` | tophat.taf | `Adrift_175_tophat.txt` | endtail 1 | T2 `up`: run400 'I leap out of the hat to get just one more word to Boss, but trip on t' vs scarier 'I leap out of the hat to get just one more word to Boss, but trip on t' |
| `trabula` | Trabula.taf | `Adrift_268_trabula.txt` | diff 3 | T8 `e`: run400 'You move east. Middle Bridge The bridge is no more stable here. It con' vs scarier 'You move east. Middle Bridge The bridge is no more stable here. It con' |
| `trickortreat` | Trick or Treat.taf | `Adrift_321_trickortreat.txt` | clean |  |
| `unauthorized_termination` | unauthorized.taf | `Adrift_288_unauthorized_termination.txt` | diff 9 | T5 `2`: run400 'The room slowly fades and is replaced with your destination. Centre fo' vs scarier 'The room slowly fades and is replaced with your destination. Centre fo' |
| `unfortunately` | Unfortunately.taf | `Adrift_255_unfortunately.txt` | clean |  |
| `vague` | vague.taf | `Adrift_298_vague.txt` | diff 40 | T0 `1`: run400 'Nothingness, then you are here. With amnesia it is quite possible to h' vs scarier 'Nothingness, then you are here. With amnesia it is quite possible to h' |
| `vendetta` | Vendetta.taf | `Adrift_320_vendetta.txt` | diff 23 | T19 `open cargo hold`: run400 'You can't open that. There is a bleeping sound coming from the videoco' vs scarier 'You can't see the cargo hold. There is a bleeping sound coming from th' |
| `videotapedecay` | Video_Tape_Decay.taf | `Adrift_306_videotapedecay.txt` | endtail 1 | T138 `north`: run400 'As you move north, you wonder what life will be like with your daughte' vs scarier 'As you move north, you wonder what life will be like with your daughte' |
| `viewtohome` | A View to a Home.taf | `Adrift_295_viewtohome.txt` | diff 22 | T5 `north`: run400 'You move north. Passage You are in a junction of many passageways. You' vs scarier 'You move north. Passage You are in a junction of many passageways. You' |
| `volant` | volant.taf | `Adrift_256_volant.txt` | lost-cmd | 1 lost, first feed[5] `z` -- `x racks` re-blessed 2026-09-06 (DontUnderstand, Adrift_256 125-126) |
| `warlord` | warlord.taf | `Adrift_337_warlord.txt` | lost-cmd | 2 lost, first feed[307] `x artefacts` |
| `wax_worx` | wax_worx.taf | `Adrift_244_wax_worx.txt` | diff 2 | T15 `ask charlie about house`: run400 'The voice that replies might be your own breathing. "What a pad. We ch' vs scarier 'Words form, not on the motionless lips, but in your mind. "What a pad.' |
| `wes_ghn` | WesGHN.taf | `Adrift_307_wes_ghn.txt` | diff 8 | T46 `take candle`: run400 '"Why, Wes, shugah, what do you think you are doin'?" Hope says with a ' vs scarier '"Why, Wes, shugah, what do you think you are doin'?" Hope says with a ' |
| `whitesingularity` | The White Singularity.taf | `Adrift_180_whitesingularity.txt` | endtail 1 | T5 `pull out the life support generator`: run400 'You chew your lip, agonizing over the decision. Suddenly, a brilliant ' vs scarier 'You chew your lip, agonizing over the decision. Suddenly, a brilliant ' |
| `wilkins` | The_Strange_Tale_of_Dr_Wilkins.taf | `Adrift_313_wilkins.txt` | diff 34 | T1 `take base tincture`: run400 'I take the base tincture.' vs scarier 'I take the base tincture. I quickly jot down everything so far, so I c' |
| `will` | Will.taf | `Adrift_296_will.txt` | lost-cmd | 1 lost, first feed[123] `score` |
| `witchtale` | A Witch Tale.taf | `Adrift_238_witchtale.txt` | endtail 1 | T43 `mix ingredients`: run400 '"Are you ready, dearie?" I dump all the strange ingredients into the c' vs scarier '"Are you ready, dearie?" I dump all the strange ingredients into the c' |
| `withoutaclue` | WithoutAClue.taf | `Adrift_299_withoutaclue.txt` | clean |  |
| `wolvesatthedoor` | Wolves_at_the_Door.taf | `Adrift_207_wolvesatthedoor.txt` | clean |  |
| `woof` | Woof.taf | `Adrift_340_woof.txt` | diff 1 | T24 `z`: run400 'Time passes... "Rex!!!! I'm back." Du dum... Well done, Rex! You score' vs scarier 'Time passes... "Rex!!!! I'm back." Woooooooof! Well done! You scored 3' |
| `worstgame` | WorstGameInTheWorld.taf | `Adrift_196_worstgame.txt` | diff 1 | T10 `z`: run400 'Time passes... u cri out in agonee as the bomer its u a few times-' vs scarier 'Time passes... "uguggkgggk" u screm as the bomer whaks u in the ead!!!' |
| `wumpusrun` | competition2006__adrift__wumpusrun__wumpusRun.taf | `Adrift_187_wumpusrun.txt` | diff 11 | T0 `south`: run400 'You press on to the south. One Big Empty This cavern is completely emp' vs scarier 'You advance cautiously to the south. Bog of Eternal Stench Methinks it' |
| `yadfa` | YADFA.TAF | `Adrift_336_yadfa.txt` | diff 1 | T86 `in`: run400 'Lair of the Bugha Lair of the BughaYou're in a dark, dismal cave, home' vs scarier 'Lair of the Bugha Lair of the Bugha You're in a dark, dismal cave, hom' |
| `ynkaboom` | YNKaboom.taf | `Adrift_206_ynkaboom.txt` | endtail 1 | T24 `yes`: run400 'Frantically you toss your interpreter at the Eagle Beast. It catches i' vs scarier 'Frantically you toss your interpreter at the Eagle Beast. It catches i' |
| `yonastoundingcastle` | yonastoundingcastle.taf | `Adrift_324_yonastoundingcastle.txt` | diff 22 | T39 `x fount`: run400 'Yon fountain doth bubble forth with ye waters of some effervescence. Y' vs scarier 'Yon fountain doth bubble forth with ye waters of some effervescence. Y' |
| `zelda` | zelda.taf | `Adrift_319_zelda.txt` | diff 5 | T60 `buy ganon mask`: run400 '"Excellent choice, sir. You'll be very happy with your new mask." The ' vs scarier '"Excellent choice, sir. You'll be very happy with your new mask." The ' |


### 3.90 — 54 games (ALL MEASURED or DEFERRED as of 2026-09-05)

| game | solution | cmds | walks | NPCs | events | waitkey | notes |
|---|---|---:|---:|---:|---:|---|---|
| `Merry_Murders.taf` | `merry_murders` | 181 | 8 | 8 | 2 | yes | [Merry_Murders_walkthrough](Merry_Murders_walkthrough.md) **measured 2026-08-31** (dated section below) |
| `Vampire.taf` | `vampire` | 205 | 7 | 11 | 11 | yes | [The_Vampire_With_A_Conscience_walkthrough](The_Vampire_With_A_Conscience_walkthrough.md) -- **measured 2026-08-31**, Runner walls at 70/100 (T61 spent-claim), see section below |
| `gamma.taf` | `gamma` | 315 | 4 | 10 | 0 | -- | -- **measured** (`Adrift_3_gamma.txt`, 185/185) |
| `S_Tar_Dus.taf` | `stardust` | 199 | 4 | 6 | 0 | -- | [S_Tar_Dus_T_walkthrough](S_Tar_Dus_T_walkthrough.md) **measured** (`Adrift_38_stardust.txt`, all 129 walk lines) |
| `wingman1.taf` | `wingman1` | 33 | 3 | 3 | 0 | -- | -- **measured** (`Adrift_3_wingman1.txt`, 32/32) |
| `tcom.taf` | `tcom` | 13 | 3 | 1 | 0 | -- | [tcom_walkthrough](tcom_walkthrough.md) -- **measured 2026-09-05**, clean (tail only); the trailing-span `#sleep`s captured its four-`<wait>` ending |
| `windy2.taf` | `windy2` | 200 | 2 | 8 | 1 | -- | **measured 2026-09-05**, clean (tail only), 147/147; needs `POPUP_ANSWERS="Sam"` for its name InputBox |
| `Richard.taf` | `richard` | 189 | 2 | 5 | 13 | yes | [WhereIsRichard_walkthrough](WhereIsRichard_walkthrough.md) -- **measured 2026-09-05**, clean (tail only), 70/70, 1000/1000 both sides |
| `cleft.taf` | `cleft` | 115 | 1 | 2 | 1 | -- | [The_Cleft_in_the_Rock_walkthrough](The_Cleft_in_the_Rock_walkthrough.md) **measured**, re-measured 2026-09-05 |
| `ECOD3.taf` | `ecod3` | 26 | 1 | 1 | 0 | -- | [ECOD3_walkthrough](ECOD3_walkthrough.md) -- **measured 2026-08-31**, clean (tail only) |
| `BobBobsly.taf` | `bob_bobsly` | 25 | 1 | 2 | 0 | -- | [Bob_Bobsly_walkthrough](Bob_Bobsly_walkthrough.md) -- **measured 2026-09-05**, clean (tail only), 155/155 both sides |
| `largo-winch.taf` | `largo_winch` | 323 | 0 | 42 | 22 | -- | [Largo_Winch_walkthrough](Largo_Winch_walkthrough.md) -- **measured 2026-09-05**, clean (tail only) |
| `mudergreatfalls.taf` | `murder_great_falls` | 255 | 0 | 0 | 0 | yes | [Murder_in_Great_Falls_walkthrough](Murder_in_Great_Falls_walkthrough.md) -- **measured 2026-09-05**, clean (tail only) |
| `report.taf` | `report` | 254 | 0 | 0 | 0 | -- | [Report_Espionage_walkthrough](Report_Espionage_walkthrough.md) -- **measured 2026-09-05**, clean (tail only) |
| `Archie's Birthday V 1-2.taf` | `archie` | 240 | 0 | 8 | 0 | yes | [Archies_Birthday_walkthrough](Archies_Birthday_walkthrough.md) -- **measured 2026-09-05**, two engine fixes (3.9 pronoun echo, examine-self full stop) |
| `croft.taf` | `croft` | 193 | 0 | 4 | 1 | -- | -- -- **measured 2026-09-05**, clean |
| `DarkTower.taf` | `darktower` | 174 | 0 | 0 | 0 | -- | [The_Dark_Tower_walkthrough](The_Dark_Tower_walkthrough.md) -- **measured 2026-09-05**, clean |
| `FarFromHome.taf` | `farfromhome` | 167 | 0 | 0 | 0 | yes | [Far_From_Home_walkthrough](Far_From_Home_walkthrough.md) -- **measured 2026-09-05**, clean (tail only); do not checkpoint a measurement drive |
| `EnqueteAHautsRisques.taf` | `enquete_a_hauts_risques` | 145 | 0 | 13 | 7 | -- | **measured 2026-09-05**, clean (tail only) |
| `Captive.taf` | `captive` | 141 | 0 | 2 | 19 | -- | [Captive_Universe_walkthrough](Captive_Universe_walkthrough.md) -- **measured 2026-09-05**, clean (tail only); 57 real commands, not 141 |
| `The Screen Savers On Planet X.taf` | `screen_savers` | 133 | 0 | 10 | 19 | -- | [The_Screen_Savers_On_Planet_X_walkthrough](The_Screen_Savers_On_Planet_X_walkthrough.md) -- **measured 2026-09-05**, clean (tail only) |
| `thewoods.taf` | `thewoods` | 133 | 0 | 0 | 0 | yes | [The_Woods_Are_Dark_walkthrough](The_Woods_Are_Dark_walkthrough.md) -- **measured 2026-09-05**, clean (tail only); 73 real commands, not 133 |
| `Chosen.taf` | `chosen` | 123 | 0 | 0 | 0 | yes | [Chosen_walkthrough](Chosen_walkthrough.md) -- **measured 2026-09-05**, clean (tail only); 52 real commands, not 123 |
| `Renuntio.taf` | `renuntio` | 118 | 0 | 0 | 3 | yes | [Renuntio_walkthrough](Renuntio_walkthrough.md) -- **measured 2026-09-05**, clean (tail + one wrap artifact); 39 real commands, not 118 |
| `as.taf` | `asylum` | 102 | 0 | 1 | 0 | yes | [Asylum_walkthrough](Asylum_walkthrough.md) -- **measured 2026-09-05**, clean (tail + one `<cls>` artifact); 27 real commands, not 102 |
| `A_Morning_with_a_Headache.taf` | `morning_headache` | 88 | 0 | 3 | 8 | -- | [A_Morning_with_a_Headache_walkthrough](A_Morning_with_a_Headache_walkthrough.md) -- **measured 2026-09-05**, clean (tail only) after porting the pre-4.0 reach rule; 53 real commands, not 88 |
| `sleaze.taf` | `sleaze` | 86 | 0 | 0 | 0 | -- | [Sleaze_City_walkthrough](Sleaze_City_walkthrough.md) -- **measured 2026-09-05**, clean (tail only); 43 real commands, not 86 |
| `Wheel105.taf` | `wheels_must_turn` | 77 | 0 | 4 | 15 | yes | [The_Wheels_Must_Turn_walkthrough](The_Wheels_Must_Turn_walkthrough.md) -- **measured 2026-09-05**, clean (`[Game ended]` tail plus three `<waitkey><cls>` butt-joins); exposed the pause/`#sleep` ordering bug |
| `tq3.taf` | `tq3` | 76 | 0 | 2 | 4 | -- | [The_Quest_Moody_walkthrough](The_Quest_Moody_walkthrough.md) -- **measured 2026-09-05**, clean (tail only) |
| `mhpquest.taf` | `mhpquest` | 68 | 0 | 2 | 0 | -- | [MHP_Quest_walkthrough](MHP_Quest_walkthrough.md) -- **measured 2026-09-05**, clean (tail only); refined the silent-task rule; 53 real commands, not 68 |
| `everything.taf` | `everything` | 68 | 0 | 0 | 0 | yes | [Everything_Emanuelle_walkthrough](Everything_Emanuelle_walkthrough.md) -- **measured 2026-09-05**, one deliberate deviation (`read diary`, silent task); 38 real commands, not 68 |
| `ECOD2.taf` | `ecod2` | 61 | 0 | 0 | 0 | yes | [ECOD2_walkthrough](ECOD2_walkthrough.md) -- **measured 2026-09-05**, clean (tail only) |
| `chicago.taf` | `chicago` | 60 | 0 | 3 | 0 | -- | [Chicago_walkthrough](Chicago_walkthrough.md) -- **measured 2026-09-05**, clean (tail only) after porting the pre-4.0 done-refusal ordering; 42 real commands, not 60 |
| `hangover.taf` | `the_hangover` | 56 | 0 | 16 | 0 | -- | -- **measured 2026-08-30**; one known deliberate deviation (the filing cabinet's silent task) |
| `veteran.taf` | `veteran` | 47 | 0 | 3 | 0 | -- | [Veteran_Experience_walkthrough](Veteran_Experience_walkthrough.md) -- **measured 2026-09-05**, clean (tail only), 47/47 turns identical |
| `lostsouls.taf` | `lost_souls` | 47 | 0 | 0 | 0 | -- | [Lost_Souls_walkthrough](Lost_Souls_walkthrough.md) -- **measured 2026-09-05**, clean (tail plus the known `<waitkey><cls>` butt-join); exposed the missing trailing-span pauses |
| `CRM.taf` | `crm` | 46 | 0 | 0 | 0 | -- | [That_Crazy_Radioactive_Monkey_walkthrough](That_Crazy_Radioactive_Monkey_walkthrough.md) -- **measured 2026-09-05**, clean (tail only) |
| `Villains_And_Kings.taf` | `villains_and_kings` | 44 | 0 | 0 | 0 | -- | [Villains_And_Kings_walkthrough](Villains_And_Kings_walkthrough.md) **deferred**: combat RNG |
| `DFU.taf` | `dfu` | 44 | 0 | 1 | 0 | -- | [Dance_Fever_USA_walkthrough](Dance_Fever_USA_walkthrough.md) -- **measured 2026-09-05**, clean (tail only) |
| `impulso.taf` | `impulso` | 43 | 0 | 0 | 0 | -- | [Impulso_walkthrough](Impulso_walkthrough.md) -- **measured 2026-09-05**, clean (tail only); its CompleteText-less `atacar * chico` prints a room description, so the turn is not silent |
| `Colony.taf` | `colony` | 40 | 0 | 3 | 3 | -- | [Colony_walkthrough](Colony_walkthrough.md) **deferred**: rollable event ("planetary holocaust") |
| `LOST.TAF` | `lost` | 38 | 0 | 3 | 11 | yes | [Albert_is_Lost_walkthrough](Albert_is_Lost_walkthrough.md) -- **measured 2026-09-05**, clean (tail only), 38/38 |
| `LOST.TAF` | `lost_down` | 38 | 0 | 3 | 11 | yes | **measured 2026-09-05**, clean (tail only), 38/38; the other ending of the same route |
| `amonkeytoomany.taf` | `amonkeytoomany` | 34 | 0 | 1 | 0 | -- | [A_Monkey_Too_Many_walkthrough](A_Monkey_Too_Many_walkthrough.md) -- **measured 2026-09-05**, clean (tail only); 12 real commands, not 34 |
| `Phoenix_Destiny.taf` | `phoenix_destiny` | 33 | 0 | 0 | 0 | -- | [Phoenix_Destiny_walkthrough](Phoenix_Destiny_walkthrough.md) **measured 2026-09-05** |
| `CAH.taf` | `cruel` | 30 | 0 | 0 | 0 | -- | [Cruel_and_Hilarious_Punishment_walkthrough](Cruel_and_Hilarious_Punishment_walkthrough.md) -- **measured 2026-09-05**, clean (tail only); the 2026-08-29 `take it` probe is now a full 30-command replay |
| `forest.taf` | `forest_on_the_norm` | 27 | 0 | 4 | 0 | -- | [Forest_On_The_Norm_walkthrough](Forest_On_The_Norm_walkthrough.md) -- **measured 2026-09-05**, identical on EVERY turn, tail included |
| `Locked_door_with_water_trap.taf` | `locked_door` | 21 | 0 | 0 | 0 | yes | -- **deferred**: rollable event ("Water Rises!") |
| `Theannihilationofthink2.taf` | `think2` | 19 | 0 | 0 | 0 | -- | [Theannihilationofthink2_walkthrough](Theannihilationofthink2_walkthrough.md) -- **deferred 2026-09-05**: six mid-game `<waitkey>` pauses, the CIBASS desync shape |
| `lifesimulation.taf` | `lifesimulation` | 19 | 0 | 0 | 0 | -- | [lifesimulation_walkthrough](lifesimulation_walkthrough.md) -- **measured 2026-09-05**, one deliberate deviation: `turn off tv` is a silent **ReverseCommand** (see section below) |
| `Insane.taf` | `escape_from_insanity` | 16 | 0 | 0 | 0 | -- | [Escape_from_Insanity_walkthrough](Escape_from_Insanity_walkthrough.md) -- **measured 2026-09-05**, clean (tail only); its three leading blanks are empty commands, not startup pauses |
| `Toxically_Earth.taf` | `toxically_earth` | 11 | 0 | 17 | 0 | -- | [Toxically_Earth_walkthrough](Toxically_Earth_walkthrough.md) -- **measured 2026-09-05**, clean (tail only); third confirmation of the refined silent-task rule |
| `Dreams.taf` | `dreamland` | 10 | 0 | 0 | 1 | -- | [Dreamland_walkthrough](Dreamland_walkthrough.md) -- **measured 2026-09-05**, clean (tail only); its CompleteText-less win task ends the game, which prints |
| `Matt's House.taf` | `matts_house` | 8 | 0 | 5 | 0 | -- | [Matts_House_walkthrough](Matts_House_walkthrough.md) -- **measured 2026-09-05**, **identical on every turn including the last** (the golden ends on `score`, not an EndGame); drive it as the space-free copy `matts.taf` |

### 3.80 — 10 games (ALL MEASURED as of 2026-09-05)

| game | solution | cmds | walks | NPCs | events | waitkey | notes |
|---|---|---:|---:|---:|---:|---|---|
| `haunt.taf` | `haunt` | 88 | 5 | 4 | 3 | -- | **done** 2026-09-04 -- see "Measured so far" |
| `superliam.taf` | `super_liam` | 86 | 5 | 11 | 0 | -- | **done** 2026-08-31 -- see "Measured so far" |
| `cave.taf` | `cave` | 216 | 2 | 5 | 12 | -- | **done** 2026-08-31 -- see "Measured so far" |
| `akron.taf` | `akron` | 44 | 2 | 4 | 0 | -- | **done** 2026-08-24 (`Adven_7_akron.rtf`, 0/44), re-checked 2026-09-05 against the current engine: still 43/43 identical -- see "Measured so far" |
| `jb2000.taf` | `james_bond` | 20 | 1 | 1 | 0 | -- | **done** 2026-09-04 -- see "Measured so far" (take->get rewrite, 3.80 only) |
| `haunted.taf` | `haunted_house` | 116 | 0 | 0 | 2 | -- | **done** 2026-09-05 -- clean, see "Measured so far" |
| `Crime_Adventure.taf` | `crime_adventure` | 90 | 0 | 2 | 3 | -- | **done** 2026-09-04 -- see "Measured so far"; [Crime_Adventure_walkthrough](Crime_Adventure_walkthrough.md) |
| `first.taf` | `fistandantalus` | 18 | 0 | 1 | 0 | -- | **done** 2026-09-05 -- clean, see "Measured so far" |
| `duck.taf` | `duck_mccloud` | 13 | 0 | 0 | 0 | -- | **done** 2026-09-05 -- clean, see "Measured so far" |
| `microwaveman.taf` | `microwave_man` | 9 | 0 | 1 | 1 | -- | **done** 2026-09-05 -- clean, see "Measured so far" |

### 3.70 — 2 games (ALL MEASURED as of 2026-09-05)

| game | solution | cmds | walks | NPCs | events | waitkey | notes |
|---|---|---:|---:|---:|---:|---|---|
| `arlo.taf` | `alices_restaurant` | 85 | 11 | 9 | 9 | -- | [ADRIFT_370](ADRIFT_370.md) **measured** (`Adven_6_arlo.rtf`; the three differing turns are the 3.7 walk departure lines, fixed) |
| `castle.taf` | `castle_quest` | 17 | 0 | 1 | 0 | -- | **done** 2026-09-05 -- clean in run370, see "Measured so far"; [ADRIFT_370](ADRIFT_370.md) |

`arlo.taf` is the single best target in the pre-4.0 half: 11 walks in 85
commands, and 3.70 is the least-exercised parse schema in the engine. Across
the whole file `goldilocks` and `sophie` (4.00) are denser, but they test a
schema Professor has already been through — arlo tests one nothing has.


## What to do with a diff

1. Rule out Verbose and the Appearance checkboxes.
2. Rule out the feed: `compare_wine_transcript.py` prints every command the
   Runner never echoed before any diff.
3. Classify each remaining turn: capture artifact (item 10 above), RNG (the
   dump names the rollable event or walk destination), or engine.
4. For an engine difference: find the Runner's code path in the decompile,
   settle the version gate with the string census, port behind a
   `TAF_VERSION_*` gate, run the whole suite, re-bless, and record the
   measurement in the harness row comment.  A difference deliberately not
   ported goes in the list below with its reason.

## Rules measured and ported (index)

One line each; the game or probe that found it, then the pointer.  Dates
and decompile addresses are in the harness row comments and in git history.

**Parser and dispatch**
- The `(<npc>)` ask/talk echo is printed before task matching, and every
  typed line notes its characters (SPAM, sommeril; `uip_print_ask_echo()`).
- The character pronouns have no-antecedent seeds of their own: "No male" /
  "No female" from 3.90, one "Nobody" below, echoed and spliced exactly as
  "Absolutely nothing" is for `it`/`them` (showtime `get her hand`;
  `uip_replace_pronouns()`).
- Every Runner splits its `ask`/`talk to` block on `about`: only the branch
  WITHOUT it prints the `ask [character] about [subject]` hint, and the
  branch with it seeds "<You> can't talk to that." (thelasthour `ask sly
  about him`; `lib_cmd_ask_about_nothing()`).
- A trailing space in a task command must be present in the input, for
  all-literal patterns only (`NODE_HARD_WHITESPACE`; sommeril
  `get placemat `, JGrim `in `, wax_worx `d `).
- 4.0 task matching is verb-literal; the pre-action retry uses the typed
  verb (man_overboard `take poster`).
- SYNONYM table = sequential whole-string rewrites (Vardock).
- 3.80-only `take` -> `get` pre-parse rewrite (great `steal picasso`).
- `z` = wait only from 3.90 (cave).
- Pre-4.0 done-refusal ("You have already done that.") before the library:
  literal patterns, the game's default message only, movement exempt
  (chicago `listen`); 4.0 keeps the library first.
- A matched catch-all `*` task clears the room refusal.
- 3.7/3.8 `co()` object-ambiguity prompt `Which X.  list?` replaces the
  turn's output but the action still happens (mikes truck keys) -- ported
  for 3.7/3.8; 3.9's version is handler-scoped and not longest-match
  (stardust), unported.
- 3.9 `%object%` binds strictly, 4.0 case-sensitively; 4.0 resolves a
  seen-but-absent noun (examine word-score tie-break, no "(at present)",
  blocked-exit refusal lists the open exits, `put <unresolvable>` ->
  "It is not clear which object you are referring to." / "Drop what?").
- Pre-4.0: `x <unknown noun>` -> "Nothing special."; no Runner says
  "Open what?"; `close <not closeable>` loses its bang; reach rule
  "You can't reach X from Y!" (`lib_cannot_reach_container()`, < 4.00,
  A_Morning_with_a_Headache).
- Raw-name noun matching: an unlisted or trailing-space Short is
  unreferenceable (superliam "Take what?").

**Objects and the seen model**
- Nothing is referenceable until something LISTS it; the seen byte is
  restored by the loader; a 3.9 event move sets no seen stamp (cleft).
- An `InRoomDesc` of one space is PRESENT and suppresses "Also here is"
  (topaz; `lib_inroomdesc_is_absent()`).
- What is ON an object lists before what is IN it; `%in_X%`/`%on_X%`
  format; a "The" prefix keeps its capital; recursive holding; empty-M1
  room alts; `where`/`find`/`locate` wording; "<Name> is dead!" for the
  battle corpse flag (-5, 3.9+).
- Put family: size/capacity scale bases; "is too big to fit inside";
  4.0 put-unclear fallback row.

**NPCs, walks, battle**
- The walk announcement JOINS the turn's paragraph (separator version
  split; Name capitalised in 4.0 only; ALRs span the join).
- A walk step, move included, is exact-tick gated (Merry_Murders); an empty
  game-start walk preempts for ever (FunHouse); not-a-room-zero arrival
  gate; non-looping walk StartTask 0 never runs pre-4.0; dead NPCs do not
  walk (dead flag).
- 4.0 battle narration names an NPC `<Prefix> <Alias[0]>`; corpse and room
  lines use the Name (orient_express; `battle_print_npc_name()`,
  `battle_legacy` guard).
- 4.0 administrative turns: an NPC or nothing-found examine ticks nothing
  (EV14-16); none pre-3.9.
- 4.0: a task that ends the game takes the unhandled-verb tail off the rest
  of the line -- both catch-alls and, unless the line names a character, the
  DontUnderstand text too (relojero, easter; run400 `48AC62`, `4805CD`).
  The verb branches ahead of it (open/close, movement, wear, look, wait)
  still run, and a silent task that does NOT end the game changes nothing
  (seaside).
- A line that ran a task gets no already-done refusal: the RepeatText is
  printed inside the dispatcher, which only ever picks one task (easter).

**Events**
- No startup tick pre-3.9; `delay N` starts on turn N; `score` ticks
  events in 3.7/3.8; a finishing event re-checks lower-indexed events the
  same tick; rolls are exclusive-hi; immediate events at LOAD; PrefTime1/2
  texts and the event's `Where` list are in the dump (FarFromHome).

**Output, wording, version splits**
- 4.0 output filter: ALR repeat-until-stable and a variable freeze per
  completing task (humbug's doubled "Okay.").
- Third-person library text is NOT conjugated (one literal per message,
  seven-slot pronoun array; slot 6 `s` only in the twelve compass moves);
  six messages read `he`/`she` (`%player_pronoun%`); `drop all` /
  `wear all` / `remove all` empty-inventory wording (herrdoktor, Main
  Course, ptbad probes).  Pre-4.0 clamps to second person.
- Pronoun echo round brackets in every version, the article rule 4.0-only;
  examine-self full stop 3.9+.
- "You pick up" pre-4.0 vs "You take" in 4.0; "You've already got X!"
  pre-4.0 (cave, confirmed by thewoods' own ALR).
- "There is nothing of interest here." substituted at LOAD in 3.8 (before
  alts) and at render time in 3.9.
- 3.9 WinText pspace; pre-4.0 openness line "The <Short>"; ", and
  carrying" from 3.90; score summary after every EndGame, NotifyScore
  default OFF; 3.8 AdditionalMessage double-space drop; empty room
  description substitution (inverse census).
- ShowRoomDesc prints BEFORE the task's actions.
- The room block is ONE string, never sectioned: `viewroom` concatenates the
  description, the object InRoomDescs, the "Also here" list, the joined
  "X is here." sentence, the characters' own texts and the event LookTexts
  onto `MemVar_4941B0`, joined by `pspace()` @0044A9F4 (a *conditional* two
  spaces) except where the literals `"  Also here"` @00472696 and `"  "`
  @0047295B put them in unconditionally.  A character's in-room text is
  tested and trimmed raw, so its leading `<br>` survives.  This is what lets
  an author's ALR Original carry the Runner's own separator spaces
  (perspectives, circus, datewithdeath, Vagabond).  See "Ported 2026-09-07".
- The room-content listing is decided by `Proc_19_75_449B6C` on the object's
  OnlyWhenNotMoved byte, not by "is the InRoomDesc empty": an unspent mode 1,
  or a mode 2 frozen to the object's start room, is handled by the
  description branch and NEVER listed -- so with an empty InRoomDesc the
  object vanishes from the room text.  The byte is spent by the library take
  and by any task move action on a dynamic object (camelot15, takeone,
  zelda; `obj_shows_initial_description()`).  See "Ported 2026-09-06".
- 4.0 put/task precedence: a completable library put beats a passing task,
  a size/capacity refusal prints and lets the task follow on one line, the
  implicit take is gated on a mode-1 task pre-match, and the pre-matcher's
  take/put class filter (task bytes 104/105).  See "Ported 2026-09-06".
- The 4.0 implicit take runs BEFORE the put handler's task look-up, so
  `(Taking the X first)` (and a `get`-task that claims during it) precedes
  the text of a task that goes on to claim the put (frustrated T53-T55,
  ShadricksUnderground T81).  See "Ported 2026-09-06".
- The task pre-matcher (`Proc_19_35_453C50`) is restriction-aware: pass one
  wants a runnable task whose restrictions PASS; the fallback wants the
  lowest failing restriction's FailMessage non-empty, or a spent task's
  RepeatText.  A task restricted with no message never pre-matches (House
  task 60, measured 2026-09-06).
- Bracketed References echo removed; the bracket checkbox governed
  "(Getting off the stool first)" and two more lines.
- `stats` is not a Runner command in ANY of the four Runners, and `status`
  exists only in 3.90/4.00 and only inside the Battle System handler.  The
  `[status/stats]` row's `stats` synonym was dropped (suburbanprodigy3 T31).
  See "Ported 2026-09-07" at the end of this file.
- The rest of the `STANDARD_COMMANDS` meta table was audited the same way
  (2026-09-07).  Eleven more words are SCARE inventions absent from all four
  Runner binaries, but none of them currently diverges on the corpus.  See
  "Audited 2026-09-07" at the end of this file.  All eleven are now compiled
  out by `SCARIER_NO_ABBREVIATIONS`; see "Gated 2026-09-07" at the end.
- 4.0's take gives the tasks `get <the object>` BEFORE both of its refusals,
  not just before " can't take ": run400 46302C @462C5C runs the look-up ahead
  of " already carrying " too, so a task restricted on already holding the
  object answers with its own FailMessage (icecream T0).  See "Ported
  2026-09-07: `icecream`'s two rules" at the end of this file.
- The ALR list is walked ONCE, in descending order of Original length; 4.0
  adds recursion into each replacement (and a %tag% substitution at the top of
  every recursive call), not a second pass.  A fix-up pair longer than the pair
  that produces it can therefore never fire (qui_a_tue_dana, barneysproblem,
  iqsfot), and Replace-all's leftovers are not re-examined (threeminutes).
  See "Ported 2026-09-07: the ALR list is walked once" at the end of this file.
- A 4.0 `put X in Y` whose X names nothing PRESENT leaves the command line
  clobbered to the fragment `put X `: put_drop_list (459DB4) runs before the
  task dispatch, and name_object's 46E142 exit is the one that does not put the
  line back.  The tasks then match nothing and the catch-all speaks for the
  container.  `place`/`set` and the `on` form are unaffected.  Same section.

## Deliberate deviations (measured, not ported)

- **Pre-4.0 silent-turn DontUnderstand.**  A matched task whose turn prints
  nothing gets "I don't understand what you mean!" in the Runner; Scarier
  runs the task and falls through to the library.  Instances: hangover
  `open the filing cabinet` (score-only), everything `read diary`
  (variable-only; the game's centrepiece text is unreachable in the real
  Runner), lifesimulation `turn off tv` (a ReverseCommand with an empty
  ReverseMessage).  The world state agrees on both sides in all three.
- **Pre-4.0 spent-task claim beyond the chicago port** (wildcard patterns,
  tasks with their own RepeatText): Journ2's Lair brick walls run390 at
  5/90 where we score 30/90; Vampire T61 walls the Runner at 70/100;
  merry_murders.  Scarier prints the RepeatText when non-empty.
- **run370 double matcher pass** (arlo `get out of bus`: the Runner prints
  the task line and no exits list).
- **SCARE meta-commands** `wait N` / `hist N` / `redo N` do not exist in
  any Runner (sandy_meta_number).
- **Empty-Prefix double space** in object listings.
- **ALR Originals that span the joined paragraph** (thetest `You remove
  your clothes.  Nice try fish face`): the Runner joins the whole turn into
  one paragraph so a two-sentence Original matches; Scarier still sections
  the turn outside the room block.  The room block's half of this -- Vagabond
  room 4 -- was ported 2026-09-07, see the last section, and that closed
  Vagabond's last divergence.  `the_pk_girl` T103 shows the other half and
  what the paragraph really is: a completing task's own text joined to the
  output of the task it executes (`done soon."  The toaster is now on` ->
  `Laurie turns on the toaster`).  Measured 2026-09-07, `Adrift_427`.
- **A walk-triggered task one step early** (`the_pk_girl` T52): task 413
  `# Laurie rejoins you at lot` is reachable only from Laurie's `WALK 2`
  (`charTask=413`), and Scarier fires it in the chapter-1 closing turn where
  run400 does not.  Measured 2026-09-07, `Adrift_427`.
- **`Motion.taf`'s minigame turns are its keypresses** and the feed cannot
  tell them apart: 137 lines, 124 echoed, and the rocket's fuel gauge is one
  burn out from frame 0 (run400 loses the minigame by turn 7).  Re-cut the
  feed before reading this as an engine difference.  `Adrift_425`.
- **House's `%drunk%` ALR loop.**  House.taf rewrites "You move" to
  `%drunk%` and the string variable `drunk` is "You move", so every move in
  run400 pops an `evaluate error - Out of stack space` alert (dismissed,
  the turn then prints nothing at all for the move).  Measured 2026-09-06,
  re-measured 2026-09-07 from a cold start.  Scarier's expansion is
  depth-capped, so the walk bottoms out and the filter's next pass
  interpolates what it wrote: `You move east.`, the line the author meant a
  sober player to see.  (Before the ALR walk went in on 2026-09-07 we
  printed the literal `%drunk% east.`.)
- ~~**run400 "Which <term>.  <list>?" disambiguation wording** (shadricks
  `climb tree`)~~ **PORTED 2026-09-07** -- measured with `p4CO.taf`
  (Adrift_925-930) and ported from the two handlers, with the pending answer
  slot; three goldens moved.  See "Measured 2026-09-07: the 4.0
  object-ambiguity prompt" at the foot of this file.

## Still open

Engine leads, measured or half-measured, none blocking:

- **Put/task precedence at 4.0** -- the one port with a written spec; see
  the next section.
- **run400 prints no `put` confirmation when the moved object is dynamic
  object #1** (`Adrift_82`-`87`, follows the object number, not the
  container, position or command spelling; 4.0-only, run390 prints).
  Mechanism unlocated in `name_object`; no corpus row touches it.
- **Two-object canonical prefixed retry** in `lib_try_game_command_common`:
  Scarier re-tries `put a bean in a jar` and lets the task claim; neither
  run390 (`Adrift_88`) nor run400 (`Adrift_82`) does that for a two-object
  put.  The retry was pinned on Wax Worx's one-object `get * head`; wants a
  probe with a prefixed take and a prefixed put in one game.
- **Absent-noun probes from Main Course** (`Adrift_35`): `put zzz in yyy`
  -> "I don't understand what you want to put things inside."; `ask zzz
  about yyy` -> "You can't talk to that."; `wield zzz` -> "Remove what?";
  unmeasured `put all in X` with nothing carried (run400 has no `" else"`
  literal).
- **4.0 scope**: the never-seen "You can't see that." branch at 471995,
  the two-pass `%object%` scope filter proper (present first, then
  absent-but-seen; tail self-call `loc_458E64`, `SCR_TRACE_SCOPE`), and the
  NPC seen gate `npc.global_26` for `%character%` (xfiles `look up byers`).
- **Unmeasured put first-noun cases**: a seen-but-absent FIRST noun, and an
  unknown first noun with an ambiguous seen-absent second one.
- **3.70/3.80 halves of the absent-object refusal rows** (p39EXAM/p4EXAM
  have no 3.7/3.8 twins; 3.7 `open <not openable>` composes with a period
  at 43D1E0 where 3.8/3.9 end in a bang).
- **3.8 referenceability / where-fail model**: "You can't do that here."
  for a matched task in the wrong room (cave stuck tail, greatc turn 49),
  "You can't see X from here!" / "You don't have X!" where Scarier says
  "Take what?", and run380's wildcard `get *knives*` matching `get meat`.
  No wired walkthrough reaches them.
- **3.90 administrative set**: the measured set ends at
  information/end/turns; Scarier also treats hint, help, clear, where and
  a dozen 4.0-era verbs as administrative for 3.9 and nothing has measured
  them under run390.
- **Battle**: whether run400 capitalises a blow that starts with a
  lowercase alias (trabula "a soldier attacks you"); the port concatenates
  raw.
- (`isare()` vs `obj_appears_plural()`: **measured and ported 2026-09-07**,
  see "Ported 2026-09-07: `isare()` cell by cell, and the empty Prefix the
  loader fills in" at the foot of this file.  The empty-prefix half of the
  lead turned out to be a false alarm -- the loader substitutes an "a" -- and
  three other cells were real.)
- **Silent-task test scope**: run400 tests the whole turn buffer, Scarier
  the task's own output; differs only when something wrote before the verb
  dispatch (the References echo).  No corpus row known.
- **Timed events a turn out of step** (the_pk_girl 138/470 turns,
  orient_express) has not been re-measured since the event-start-tick and
  exact-tick walk fixes; re-run the compare before assuming it is still
  there.
- **run390 `#save` event clock** (FarFromHome +1 tick per echoed save;
  largo-winch with no echoed save turn was fine): `opensave()` on paper
  skips the tick.  Probe `x` / `save` / `x` around a 2-turn event.
- **`NPCWalkAlert`** synthesized task pair (`sctasks.cpp`) with no run400
  counterpart; anticipates the ticker's restart by a tick, nothing depends
  on it.
- **4.0 output filter unmeasured corners**: whether 3.9 also drops the
  pre-variable-change checkpoint; a mutual `A -> B` / `B -> A` ALR pair
  (the loop bound is a guard, not a model).
- (The break in front of a room heading, 19 breaks over 9 rows: **resolved
  and ported 2026-09-07**, see "Ported 2026-09-07: the room heading's own two
  breaks, and a stale position marker".  The archive's real direction is now
  at zero -- `sweep_wine_breaks.py` reports `runner-only 0` over all 267 rows
  -- so there is no measured line-structure divergence left to chase.)
- **Merry_Murders** feed turns 45/46 FLAG wording, minor (git history).
- **Deferred candidates**, each for a reason that will not change:
  `Colony`, `Locked_door_with_water_trap` (a rollable event on the route),
  `Villains_And_Kings` (combat RNG plus a name/gender POPUP),
  `Theannihilationofthink2` (six mid-game waitkeys), `To_Hell_And_Beyond`
  (19 rollable events), `Pieces of eden`, `The Fly Human`, `The Foggy
  Banana Adventure`, `hyper_b_s`; `sophie` measured for its first fifty
  commands only, `CIBASS` partial, the `great.taf` car chase unmeasurable.
- (House `get cathy` at the Dining room fireplace, Adrift_93 turn 3: **resolved
  and ported 2026-09-06**, see "Ported 2026-09-06: the task-ran NPC gate".)

## Ported 2026-09-06: the task-ran NPC gate, and the take-NPC wording

Five more House checkpoint drives (`Adrift_94`-`98`, each `2`, `#restore
housewood`, then the commands; `ck_housewood2.tas` saved after the first
mention).  The measured rule: after restoring the checkpoint, the FIRST
line that names Cathy misfires whatever full turns come before it (`look`,
`get fireplace`, `take fireplace`), and every later mention works:

```
> x cathy
You see no such thing.
> get cathy
I don't think girl would appreciate being handled.
> x cathy
Cathy is a medium sized woman with long red hair. ...
```

Saving after that first mention and restoring the new save answers `x cathy`
at once, so restore does not clear anything: the two saves differ (besides
event timers and turn-driven variables) in exactly one task-done bit, plain
line 1776 of the decoded stream = task 200 (0-based) `# attention on cathy
grave vision`, patterns `*cathy*` / `* cathy *` / `examine cathy`, not
repeatable, restrictions Cathy present and Damien absent, actions set
`%grave_var%` and execute task 191 (which does not run there).  It runs
silently on the first mention and is spent from then on.

Why a silent task turns `get cathy` into `Take what?`: run400's character
handler `Proc_19_0_480674` guards most of its NPC verb branches -- who
(47F32C), hit/kill/kick/punch/attack (47F452), get/take/pick up (47F734),
talk to/speak to (47F863), the ask-without-about hint (47FB93),
where/find/locate (47FCB1), x/examine/look (47FE4F), take-from (4803DD) --
with `MemVar_4941F8 = 0`.  That flag is cleared at the top of the input
routine (489FF6) and set by `execute_task` (45A176) and by the execute-task
action (48D5DA), i.e. it means "a task has run for this line".  The P-code
at 47F710-47F73E is `(c("take") Or c("get") Or c("pick up")) And
MemVar_4941F8 = 0` -- the Or's are folded before the And, so the flag gates
all three verbs (an earlier reading of the same lines as `pick up And flag`
was wrong).  With the NPC branches shut the line reaches the object take
(`Proc_19_6_47C83C`) which, finding no object and no "from", says `Take
what?`; the examine says `You see no such thing.`.  run390 guards the same
branches with `MemVar_468198` (45939D, 459658); run370 has no flag at all
(4386BC); run380's rendering (44054B) is too ambiguous to lean on, so the
port gates at 3.90+.

NOT every NPC branch, though -- a first, blanket port of this rule broke
Humbug's `Ask hacker about humbug` (cmd 250 of the golden), where the
silent scoring task 98 `ask * hacker about * humbug` runs and run400 STILL
prints the hacker's topic reply (`Adrift_4_humbug.txt` lines 1068-1069).
The branches that survive a task are reached by another route: give is
handled in the input routine at 48A98A with no flag test; the `ask X about
Y` branch at 47F900 is `npc present And ((4941F8 = 0 And 4942E0 = 0) Or
buffer = "<player> can't talk to that.")`, and generaltasks_verbs seeds
exactly that buffer at 488C65 whenever no object took the ask (buffer empty
after a silent task), so the flag never bites -- run390 tests no flag at
all at its ask head 4597FE; kiss (47F7E7) tests the buffer, not the flag;
and the handler's closing "I don't understand what you want to do with"
fallback (4805DA) only asks for an empty buffer and a present NPC.
(`MemVar_4942E0`, the second flag in those tests, is "the pre-matcher found
a task", set in task_prematch at 453FAF/454024 and cleared at 489FFE.)

Ported in `run_try_command_table()` (scrunner.cpp): once
`run_tasks_ran_this_command` records any task for the line, the library
rows whose pattern names `%character%` are skipped, except give, `ask/talk
to %character% about`, kiss, status and the last-resort `* %character% *`
(`run_npc_row_blocked()`).  Scarier already ran the library after a silent
task (the silent-literal peek work), so only the NPC rows needed the gate.
Corpus 428/428 after the narrowing; the House drives replay exactly.

Confirmed live on House itself (`Adrift_99`, `2`, `#restore housewood`):
`ask cathy about grave` as the FIRST mention -- silent task 200 runs -- still
prints Cathy's `[no_comment=1]` reply "Can we chat some other time?  I'm
trying to concentrate on staying alive at the moment", the same as the
second mention and as Scarier.  (`ask cathy about damien` differs between
the two, but its reply is `[Tdamien2=%talk_value%]` with talk_value rolled
random(1,4) by task 892 on every command, so that line is seed noise, not a
rule.)

The wording: the "handled" line names the NPC by Prefix + first Alias, not by
Name -- run400 47F750-47F7BC and run390 45969B-4596C6 print `"I don't think
" & [Prefix & " "] & Alias(0)` when the alias is set (the prefix only when
it is too) and fall back to the Name when the alias is empty; run380 44057D
and run370 4386EE always print `Prefix & " " & Alias(0)`.  Cathy (alias
"girl", no prefix) gets "I don't think girl would appreciate being handled."
Ported in `lib_cmd_take_npc()`.

## Ported 2026-09-06: one task per typed line

`Adrift_100` (House checkpoint, `2`, `#restore housewood`, `kiss cathy`,
`kiss cathy`, `#restore housewood`, `where is cathy`, `where is cathy`):

```
> kiss cathy
I'm not sure she would appreciate that!
> kiss cathy
Cathy gently but firmly pushes you back ...
> where is cathy
I don't know where that is!
> where is cathy
Cathy is dining room.  (Right next to you silly!)
```

The first `kiss cathy` is the first line naming Cathy, so it runs the
once-only silent task 200 `*cathy*` -- and then NOTHING else from the game:
task 882 `[hug/kiss/touch/shake] [her/cathy]` (restrictions pass, it fires
on the second kiss) is skipped and the library's kiss line prints.  Scarier
ran 200 and then 882 on the same line.

The Runner dispatches exactly ONE task per typed line.  run400's dispatcher
`Proc_19_24_44CCE0` (called at 48A481 as `Proc_19_24_44CCE0(1,4)`; TRUE
sends 48A481 past the library to 48B4E3) asks the pre-matcher
`Proc_19_66_454EF0(arg_10,1)` for a single task, runs it forwards
(`execute_task` 45A3EC) or in reverse (`Proc_19_51_443DC8`), and its final
result at 44CCC0 is FALSE when the message buffer is still empty -- a silent
task lets the library run, but no second task; the restriction-failure pass
`Proc_19_68_45404C` runs only when no task was found at all (44CCA5).
run390's `tasks()` 42BDC4 is the same shape (checktask picks one task,
execute_task runs it, -1 when a task executed even silently), so a silent
task claims the line in 3.9 too -- with the library still reachable through
the character/object handlers' own pre-matcher look-ups.

Ported as a guard at the top of `run_game_commands_common()`: once any task
has run for the line (`run_tasks_ran_this_command`), the later passes (no
restrictions, restrictions) return without scanning.  Library callbacks
(`is_library`) are exempt: they model the handlers' own look-ups.  Four
goldens re-blessed, each an old Scarier double-dispatch:

| game | old | new | why the new text is right |
|---|---|---|---|
| `Rock Band.taf` | living-room `look` text for counter 2 | counter 1 | the repeatable silent `l{ook} {living room}` task 60 (`%RM-living_ex%+1 mod 5`) ran TWICE per look (no-restrictions pass, then the restrictions pass); once per line now, as the Runner |
| `3monkeys.taf` | `The mandrill closes in towards you` (looms=2) | `The mandrill looms dangerously near` (looms=1) | run400 `Adrift_16_3monkeys.txt` line 173 prints the looms=1 text after the first move, so task 797 (`n`/`s`/... silent, `if(%looms%=3,3,%looms%+1)`) runs once per move |
| `baroo.taf` | task 128's restriction failure `The equipment is still operating and will not open.` | `The machine is now opened.` | silent task 112 opens the machine and the LIBRARY then complains `The machine is already open!`, which the author ALR-remaps to `The machine is now opened.` -- the author saw exactly this fall-through; the old restrictions pass reached task 128 instead |
| `The Crooked Estate.taf` | first `open door` says `I open the door again, with inexplicable trepidation` | `I open the door, and vertigo nearly floors me` | once-only silent task 41 and repeatable silent task 43 both match `* open * door *`; 43 (sets `%door_open%` to the "again" text) ran on the SAME first line, now only after 41 is spent |

`Adrift_101` (same checkpoint, `hit cathy` x2, `#restore`, `talk to cathy`
x2) confirms the model with no engine change -- Scarier already printed all
four lines:

```
> hit cathy
You hit, but nothing happens.
> hit cathy
Cathy is awake now and looks capable of hitting you back if you tried.
> talk to cathy
Use the format "ask Cathy about [subject] or "give Cathy [object]"".
> talk to cathy
Use the format "ask Cathy about [subject]".
```

First mention: silent task 200 runs, the character handler's attack branch
(47F452) and talk-to branch (47F863, `4941F8=0 And 4942E0=0`) are shut, and
the line falls to generaltasks_verbs 489F4C: ` hit, but nothing happens.`
(4896A0) and the talk-to-nobody trio at 488E05-488E3F (`Gosh, that was very
impressive.` / `Not surprisingly, no-one takes any notice of <you>.` /
`Wow!  That achieved a lot.`, picked by Rnd, all three ALR-mapped by House
to the same text, so the seed cannot show).  Second mention: the game's
task 881 for hit, and the handler's `Use the format "ask <npc> about
[subject]".` for talk-to.

## Ported 2026-09-06: give in any word order

Three House checkpoint drives (`Adrift_102`-`104`, 2026-09-06):

```
> give diary to cathy            (first mention of Cathy after the restore)
Cathy takes the diary off of you and flicks through the pages. ...
> give cathy diary
Cathy takes the diary off of you ...
> give diary cathy
Cathy takes the diary off of you ...
> give diary                     (nobody named yet this game)
(to Nobody)
Give the diary to who?
> x cathy / x cathy / give diary
(to Cathy)
Cathy takes the diary off of you ...
> drop diary
> give diary to cathy
You don't have the diary!
> give cathy diary
You don't have the diary!
```

Give survives the task-ran NPC gate (the first `give diary to cathy` runs
silent task 200 and still answers), and it is word-order free in every
Runner: the character handler's give branch tests `c("give")`, a present
NPC, and then EVERY object whose name appears anywhere in the line, held or
worn -> "<npc> doesn't seem interested in <object>." (House ALR-maps that to
Cathy's diary reply); run400 48022F-480384, run390 45A0BA, run380 440E8C,
run370 438F79.  The not-held case is answered earlier by generaltasks_verbs
(488A96: `<You> don't have <object>!`, with the bang), so the handler's own
`don't have <object>.` (480338, full stop, buffer-empty gated) never
prints for a named NPC.  Scarier only matched `give %object% to
%character%` and answered `give cathy diary` with "Give the diary to who?";
two rows, `give %character% %object%` and `give %object% %character%`,
now route both orders to `lib_cmd_give_object_npc()`.
One golden moved: the_hangover `give the doctor some french fries` now
gets "Doctor doesn't seem interested in the french fries." instead of
"Give the french fries to who?".  run390 itself runs the game's Where=0
task there ("You can't do that here!", Adrift_1_hangover_run390.txt), the
deliberate deviation already recorded in the harness; the new line is what
its character handler prints once no task takes the line.

The bare `give diary` completion was already ported: the input routine
(run400 48A98A, run390 45FAB9, run380 44272C, run370 43BFC9) appends
` to <last-named character>` and echoes `(to <name>)` when no character is
named and the line has no `to`; the register starts as "Nobody" (45A7F5)
and is set at 47F3A2 whenever a line names a character.  Scarier printed
all three transcripts identically.


## Ported 2026-09-06: an unhandled verb naming two objects

Five House checkpoint drives from the Dining-room fireplace (`Adrift_105`-
`109`, 2026-09-06; diary held, Prefix `a`; fireplace Prefix empty, alias
`fire`; hook and plaster Prefix `the`; window Prefix `dining room`; House's
BattleSystem is off, so `throw` is unhandled -- run400 only knows it inside
dobattle):

```
> throw diary at cathy                 I don't understand what you want me to do with the diary.
> throw diary                          I don't understand what you want me to do with the diary.
> wibble diary cathy                   I don't understand what you want me to do with the diary.
> throw diary at fireplace             What?           (House's [error=6] DontUnderstand)
> throw fireplace at diary             What?
> wibble diary fireplace               What?
> throw window at fireplace            What?
> throw hook at plaster                What?
> throw diary at the hook              I don't understand what you want me to do with the hook.
> throw the plaster at fireplace       I don't understand what you want me to do with the plaster.
> throw dining room window at fireplace
                                       I don't understand what you want me to do with dining room window.
> throw a hook at diary                I don't understand what you want me to do with the diary.
> throw a diary at plaster             I don't understand what you want me to do with the diary.
> throw a diary at fireplace           What?
> throw diary at a fireplace           What?
> throw a fireplace at the hook        What?
> throw a diary at the hook            What?
> throw diary at the hook a            What?
> throw an diary at the hook           I don't understand what you want me to do with the hook.
> wibble dining diary                  I don't understand what you want me to do with the diary.
```

generaltasks resolves the line's object ONCE, before any verb branch: 48A3F5
calls the noun resolver Proc_21_58_463640 (mode 0, candidates = objects both
present and seen; a second pass over every seen object only when the first
finds no unique winner) and parks the answer in MemVar_4942F8.  Each
candidate scores 1 for its Short as a whole word of the line (454CB0), PLUS 1
for its first alias found as a whole word (the alias loop 4632D3 runs whether
or not the Short hit -- corrected 2026-09-06 from Adrift_111/112, below), and
then one more for EVERY word of its Prefix found anywhere in the line
(4632A9-463387).  The unique maximum wins; two
candidates with different names on the same score leave a negative index
(4633C3-463405), and the catch-all at 48B1B0-48B236 (`I don't understand
what you want me to do with X.`) needs MemVar_4942F8 > -1, so a tie prints
nothing and the game's DontUnderstand text follows.  The loader stores `a`
for an empty Prefix (4900EC) -- hence the fireplace ties the diary whenever
`a` is typed, and the printed name is Prefix + Short through the definite
tense (`the diary`, `dining room window`).  NPCs are never candidates.

Scarier's `* %object% *` row spoke for the first object it bound.
`lib_verb_object_resolve_400()` now runs the same scoring over the present,
seen objects (empty Prefix = `a`) and `lib_cmd_verb_object()` returns FALSE
on a tie, so the line reaches the game's DontUnderstand.  4.0-gated: run380
442F5D walks the objects in index order and speaks for the first present,
seen one (unmeasured).  The wording of House's DontUnderstand is task 887's
`%help_value%` roll, so Scarier's `Huh?` / `Sorry I didn't understand what
you just typed.` against run400's steady `What?` is RNG, not a divergence.
### Measured 2026-09-06: Adrift_110-112 -- dobattle's "isn't here!" and the put prompt

Three more checkpoint drives, each a Scarier-made `.tas` restored in run400:

- **House `throw diary at margo` with Margo seen and elsewhere** (Adrift_110,
  BattleSystem on in this probe): `Margo isn't here!  Seeker hums!` -- a REAL
  turn.  dobattle `Proc_11_4_47F084` (entered when MemVar_494282 = 1) walks
  every NPC whose Name is a whole word of the line (47EB46); one that is seen
  (field 26, which 476FA3 restores) and not in the player's room, with no
  named or referenced NPC present (var_92 = 0, var_8A = 0 after the 45E99C
  loop), prints `<Name> isn't here!` (47EFE5) and the loop goes on to the next
  such NPC; var_8A = 1 afterwards, so no "Who do you want to attack?".  An
  NPC never yet seen is not named, and the line falls to the no-turn
  DontUnderstand.  Ported as `lib_battle_absent_npc_400()`, called from the
  bare and with-weapon attack handlers when no NPC binds (4.0-gated; 3.9
  unmeasured).
- **thelasthour `put bowl near spyhole`** (Adrift_111): `Where do you want to
  put the spyhole?` -- the spyhole scores Short + Prefix `the` = 2 over the
  bowl's 1, which is only consistent with the Short and the alias hit ADDING
  (the alias loop 4632D3 runs after a Short hit; the earlier "else" reading
  was wrong).  Scarier's `lib_verb_object_name_score()` is now additive.
- **Shadowpeak `put sword near cell door`** (Adrift_112): `Where do you want
  to put the sword?` -- "cell door" scores 0 (a two-word Short is never one
  whole word), so the sword wins outright.

The put prompt is the put/drop list parser `Proc_19_40_459DB4`'s branch
46DC34-46DD2C, reached from generaltasks at 48A462 (before dobattle 48A4A2
and the catch-all): the line holds whole-word `put`, no ` in ` / ` on `, no
word `down`, no task pre-matches it; the noun resolver runs (46DCC7) and
prints `Where do you want to put <the name>?` (46DCDB) or `...that?`
(46DD19), then MemVar_494281 = 1 (46DD25) -- NOT a turn.  Ported as the
`put *` runner row `lib_cmd_put_where_400()` plus the same check inside
`lib_cmd_verb_object()`, `game->is_admin`.

Corpus consequences (428/428 after 13 re-blesses): the "isn't here!" turn
retired the Shadowpeak `attack X; z` self-sync trick (the `z` after such an
attack dropped, attacks with no `z` dropped themselves; light_up lost 71 no-op
attacks, wes_ghn 2; trabula only rewords), and the no-turn catch-all / put
prompt needed a one-tick filler after `put batter in remote`, `activate orb`,
`a cauldron`, `put bowl near spyhole`, `write on wall`, `peel wallpaper`,
`search rubbish`, `water plant`, `do form`.  **Pitfall:** the filler is
`look`, not `z`, in games whose `WaitTurns` global is 3 (COBL,
TheADRIFTProject, ADayAtTheSeaside, Trabula, WesGHN) -- `z` ticks three times
there.  The re-derivation was done one edit per iteration from the new
engine's own transcript (`fixsol2.py`, scratchpad), consulting the OLD golden
to decide whether a line had been a no-turn DontUnderstand.

## Ported 2026-09-06: the 4.0 put/task precedence split

Measured 2026-09-05 with three arena probes (`harness/make_arena_probe.py`
PUT4/PUT5/PUT6/PUT7 for run400, `Adrift_81`-`87`;
`harness/make_39_putprobe.py` `put39.taf` for run390, `Adrift_88`).  Each
pairs a passing put task with `zzinN` reporter tasks whose one restriction is
"object N is inside container M", so the game itself answers whether the
object moved.

| case | run400 | run390 | Scarier today |
|---|---|---|---|
| passing task, put completable (`put pill in cup`) | library puts, task never runs, object moved | task claims, nothing moved | task claims, nothing moved |
| passing task, put refused on SIZE (`put rock in slot`) | `The rock is too big to fit inside the slot.  PUTBIG.` -- refusal, then the task, one line | task claims, **no refusal printed** | task claims, no refusal |
| task spelled with articles (`put a bean in a jar`) | library puts | library puts | two-object canonical retry lets the task claim (open, above) |

Read off run400 `insides` (`Proc_19_43_46639C`): every message path exits
without setting the return byte `var_86`, so a refusal returns 0 = printed,
not claimed; a completed move returns 1; the task-claim exits return 2.
Pre-4.0 needs no change: Scarier matches run390 on eleven of twelve
`put39.taf` lines.

**What was ported (all gated >= 4.00), read off the p-code and the two
probes:**

1. The silent-literal peek no longer hands put/drop-into and put/drop-on
   lines to a task ahead of `run_priority_commands()`; a put the library
   can complete beats a matching, passing task.
2. A put refused on size or capacity prints its refusal, does not claim,
   and the general task pass runs afterwards, joined on one line with two
   spaces (`zacksmackfoot` turn 3).  A 4.0 put of a static piece exits
   silently and unclaimed (`lib_put_drop_statics_400`).
3. The insides handler's canonical rebuild is the definite form, `put the X
   in the Y` (name mode 0 turns a/an/some into "the"; `Proc_21_31_448710`),
   and it is pre-matched in mode 2.
4. The implicit take (`name_object` @46E23C): `(Taking the X first)` + a
   `get the X` / `get the X from the Y` task attempt, then the library take
   -- but ONLY if no task pre-matches the typed line in mode 1.  A hit
   leaves the piece unheld and the handler prints `<player> not holding
   the X.` and claims.  **Corrected 2026-09-06 after the House measurement:
   the pre-match is restriction-aware** (see the House subsection below);
   `lib_task_prematches_input()` now calls `run_does_command_match(game,
   line, TRUE)`.
5. The pre-matcher's mode byte (`Proc_21_57_4494FC` on task record bytes
   104/105, computed at LOAD @4931B5/@493225): mode 1 considers only tasks
   with a command pattern containing `get`/`take`/`pick` (substring), mode
   2 only those containing `drop`/`leave`/`put`, a pattern that is exactly
   `*` sets both, mode 0 is unfiltered.  Take gate and get-refusal exits use
   1; name_object's gates, the canonical rebuild and `drop all` use 2; the
   insides handler's typed-line fallback uses 0.  `run_set_task_class_filter()`
   in scrunner.cpp.

Corpus after the port: 428 PASS / 0 FAIL (baseline 428/0).  Model-derived
rows re-blessed, with the reasoning in each row's harness comment:
`sommeril` (feed: `put fish in water`), `zacksmackfoot`, `deadman` (feed:
`place hand on green plate`), `thelasthour` (take-flag quirk through an
alternate command), `ShadricksUnderground` (feed names the boulder),
`sophie`/`sophie_comp` (feed names the crystal colours), and `house`, which
is UNWINNABLE under the model and had its marker downgraded (confirmed in
run400 the next day, below).

**Wine candidates opened by the port** (all run400; every line below is a
model prediction, not a measurement):

- ~~**House, TOP**~~ **MEASURED 2026-09-06**, see the subsection below:
  the put half of the prediction held (library put, task 459 never runs,
  `light fire` refused), the hands-full half did not -- run400 attempts the
  take.
- **thelasthour**: `put knife in hole` with the knife on the floor after
  the mouse scene (prediction: "I am not holding the little knife." because
  task 16's alternate `get {the} [supper/soup/dinner]` flags it take-like).
  Cleanest single test of the mode-1 filter.  STILL UNTESTED, and the row can
  no longer carry it: since the 2026-09-07 re-derivation our feed TAKES the
  knife first, and with it in hand both engines answer "The little knife
  can't fit inside the little hole at the moment.  I put the knife in the
  mouse hole." (Adrift_366).  Needs a dedicated probe that skips `take
  knife`.
- **sommeril** turn 15: `put fish in water` (prediction: task 18 runs) vs
  `put fish in fountain` (Adrift_78 already: library put).
- **deadman** line 59: `put hand on green plate` (prediction: library put,
  task silent) vs `place hand on green plate` (task).
- **ShadricksUnderground** line 66: `put boulder on medium plinth` with two
  boulders held (prediction: "Which boulder.  ..." prompt, next line eaten,
  no task pre-match on the ambiguity path).
- **sophie** line 138: `put crystal in throat` holding the dark and red
  crystals (prediction: the ALR-rewritten prompt "Please be more clear, what
  do you want to move?  The dark crystal or the red crystal?").
- **ADP**: `put battery in charger`; **hub**: `put all in bin`; the
  canonical-matches-but-restriction-fails corner (a task spelled `put the X
  in the Y` whose restriction fails: prediction = the fail text claims).
- The substring rule itself: a task `[read] the getaway note` (contains
  "get") should be considered by the take gate; a task `put the X in the Y`
  should NOT suppress an implicit take.

### Measured 2026-09-06: House, and the pre-matcher is restriction-aware

Three checkpoint drives (`Adrift_91`-`93`; a Scarier-made `.tas` restored
in run400, because the `%drunk%` alert on every move desyncs a full replay).
At the fireplace, wood on the floor, axe in hand:

```
> put wood in fireplace
(Taking the wood first)
Your hands are full.  You are not holding the wood.
```

and after `drop axe` + `take wood`, every spelling is the library put
("You put the wood inside the fireplace."); `put newspaper under firewood`
then `light fire` answers "You need some wood or coal to make a proper
fire."  House is unwinnable in run400 4.00, as the model said.

What the model got wrong: it predicted NO take attempt, because
take-flagged task 60 `* %object%` (`# get objects while house spin`)
pattern-matches the line and the pre-match was read as restriction-blind.
Read off `Proc_19_35_453C50` and its fallback `Proc_19_68_45404C`
(`~/Adrift_decompile`, banners corrected the same day):

- pass one: a task in scope, state-runnable, whose `restriction_walk`
  PASSES and whose pattern matches;
- fallback (`arg_10 = 1`, which the take gate passes): a pattern-matching
  task in scope is a hit only if its LOWEST failing restriction has a
  non-empty FailMessage (`Proc_19_2_481DA0(task, i, 1)` stores it; the
  hit is "the message buffer changed"), or, with nothing failing, a
  non-empty RepeatText.

Task 60's one restriction (task 119 "house spin" done) fails with an empty
message, so run400 never sees it and takes the wood.  Ported the same day:
`run_does_command_match()` gained a `check_restrictions` flag, used only by
`lib_task_prematches_input()`; the letter-expansion probe in `scinterf.cpp`
stays restriction-blind.  Corpus 428/428, `house` re-blessed to the two
Runner lines (marker unchanged).  Scarier's `restr_eval_task_restrictions()`
already returned exactly that lowest-failing message, so the port is a
dozen lines.

## REFERENCE -- run370 facts established while chasing arlo

Two of these reverse working models earlier sessions were built on.

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



## Measured 2026-09-06: all 51 pending 4.00 rows, driven through fast.sh/par.sh

Every 4.00 row in the "Candidates" table that had never been driven was
driven, in four `par.sh` batches, up to **8 Runners at once** and entirely
in the background (the message driver does not need the foreground -- see
the `wine-fast-message-driver` memory).  2621 feed lines, ~9 minutes of
wall clock for the lot.  Feeds were regenerated fresh with
`harness/make_wine_cmdfile.py` into `~/adrift-battle/runner/wine/cmdfile_p_<solution>.txt`;
25 games were staged as `pfx/drive_c/adrift/w_<solution>.taf`.  Transcripts
are `Adrift_123..174_<solution>.txt` in `pfx/drive_c/adrift/`.

Scoring is `harness/compare_wine_transcript.py`.  **These verdicts are the
2026-09-06 RE-SCORE**, after all five alignment fixes described under "Five
tool fixes" in the 165-row section below.  The first pass scored seven of
these rows as long desyncs that were entirely an artefact of the comparison.

- **35 rows clean** -- 16 identical outright, 19 identical apart from the
  `[Press any key to end]` tail the Runner writes and the headless engine
  does not.  These need nothing.
- **8 rows diverge** with every command echoed: five of them on a single
  turn, none on more than five.
- **8 rows lost a feed command** in the Runner, so everything after the loss
  is out of step and is not evidence until the feed is fixed and the row is
  re-driven.

### Clean (16) -- identical on every turn

| solution | transcript |
|---|---|
| `chooseyourown` | `Adrift_160_chooseyourown.txt` |
| `flyhuman` | `Adrift_126_flyhuman.txt` |
| `forum2` | `Adrift_140_forum2.txt` |
| `griswold` | `Adrift_150_griswold.txt` |
| `imagidroids` | `Adrift_139_imagidroids.txt` |
| `invasion_shirts` | `Adrift_131_invasion_shirts.txt` |
| `marlin_affair` | `Adrift_154_marlin_affair.txt` |
| `mysteryofcaves` | `Adrift_151_mysteryofcaves.txt` |
| `percy` | `Adrift_125_percy.txt` |
| `plague` | `Adrift_174_plague.txt` |
| `private_eye` | `Adrift_167_private_eye.txt` |
| `shardsofmemory` | `Adrift_169_shardsofmemory.txt` |
| `to_hell_and_beyond` | `Adrift_124_to_hell_and_beyond.txt` |
| `unraveling_god` | `Adrift_163_unraveling_god.txt` |
| `unraveling_god_lou` | `Adrift_164_unraveling_god_lou.txt` |
| `valley` | `Adrift_166_valley.txt` |

### Clean but for the ending tail (19)

`Tear` (`Adrift_147`), `afdfr` (165), `buried_alive` (155), `chicken` (141),
`crimsondetritus` (136), `demonhunter` (137), `dragonshrine` (152),
`foggybanana` (123), `goblinhunt` (144), `imagination` (135), `lobster`
(161), `marika` (158), `microbe_willie` (132), `plunder_gargoyle` (156),
`pyramid` (128), `shore` (153), `too_much_exercise` (157), `vetknow` (170),
`vetknow2` (171).

The only difference is the Runner's `[Press any key to end]` after the last
line.  That is a property of the ending, not a law -- see "Before measuring
anything".

`buried_alive` and `lobster` joined this group in the final re-score, from 40
and 6 differing turns: both are not-SKIP-wired rows, and both were misread by
the comparison rather than by the engine (fix 5 below).  42/42 and 60/60
echoed, offset 0, every turn identical.

### Diverging, every command echoed (8)

| solution | transcript | diffs | symptom |
|---|---|---:|---|
| `asdfa` | `Adrift_143` | 1 | T2 `x cauldron`: run400 "You see no such thing.", Scarier "You can't see the cauldron from here!" -- FIXED 2026-09-06, identical on every turn |
| `cbn2` | `Adrift_138` | 1 | T17 `light match`: run400 prints the task text **and then** a second refusal ("...that was a strange command..."); Scarier prints only the task text -- **artefact, closed 2026-09-07**: `Adrift_138_cbn2.txt:104` shows that refusal has its own `> ` prompt, it is the game's DontUnderstand answering an empty feed line |
| `relojero` | `Adrift_133` | 1 | T10 `arreglar fenix`: run400 prefixes the task with "Disculpa pero no te entiendo."; Scarier prefixes "Extraños pensamientos afloran en mi mente a proposito de..." -- **FIXED 2026-09-07**, the ending takes the object catch-all off the line and the game's DontUnderstand is what is left |
| `second_chance` | `Adrift_159` | 1 | T49 `s`: Scarier appends the END GAME text run400 does not reach |
| `sophie_comp` | `Adrift_173` | 1 | T169 `put black crystal in mouth`: run400 "You can't", Scarier "It is not clear which object you're referring to" -- the 4.0 put prompt |
| `togetyou` | `Adrift_146` | 2 | T16: the room short name is "The Infected Ear" in run400, "The Ear" in Scarier -- a task-driven room-name change Scarier does not apply |
| `salutations` | `Adrift_129` | 4 | the sack event fires one turn earlier in run400 (T6 vs Scarier's T7) |
| `cbn` | `Adrift_149` | 5 | T6 `x desk`: run400 "You see no such thing." plus a second refusal; the `cbn2`/`asdfa` pair again -- T6 `x desk` FIXED 2026-09-06; the second refusal ("> Clueless Bob is confused!") remains |

Seven rows left this table in the two re-scores.  `chooseyourown`,
`private_eye`, `shardsofmemory`, `plague` and `imagidroids` (at 40, 40, 40,
40 and 9 differing turns) are now **clean**; `buried_alive` (40) and `lobster`
(6) are clean but for the ending tail.  The "numbered-choice menu answer
Scarier does not consume" that the first five were said to share **did not
exist**: they were misaligned by the comparison, one turn per pause.
`goblinhunt` likewise dropped from 6 differing turns to the ending tail
alone.  Nothing in this batch is now longer than five differing turns.

### Lost a feed command (8) -- re-feed before reading anything into them

ALL EIGHT RE-FED 2026-09-07 -- see "Measured 2026-09-07: the 19 re-fed
rows" at the end of this file.  Seven of the eight now echo every feed
command; only `hyper_b_s` still stops short, on the battle divergence the
last column already names.  The verdicts below are the PRE-re-feed ones.

| solution | transcript | lost | also seen before the loss |
|---|---|---|---|
| `cellar` | `Adrift_172` | `feed[119] undo` | T43 `x dust`: run400 "You see no such thing." vs Scarier "You can't see the dust from here!" -- the same divergence as `asdfa` -- FIXED 2026-09-06; the row's first diff is now T114 `take satchel`: run400 "There is nothing worth taking here." vs Scarier "Take what?" |
| `confession` | `Adrift_148` | `feed[16..36]`, 21 `z` in a row | the Runner stopped echoing after 16 turns; the row needs `#sleep` pacing |
| `endgame` | `Adrift_127` | `feed[9] z` | T8 `turn on pc`: Scarier prefixes "You have trouble controlling yourself..." |
| `hyper_b_s` | `Adrift_145` | `feed[20,21,23,24,26,27]`, the `a`/`p` battle keys | T5 `p`: the Flare Rat is on 23 HP in run400, 27 in Scarier, and the player on 94 against 97 -- a real divergence, and it is before the first loss.  **FIXED 2026-09-07**, and not in the Battle System: `FLARERATHP += rand(-3,-10)` is a backwards range, which `scr_randomint` used to refuse.  The whole fight is now run400-identical |
| `mortality` | `Adrift_168` | `feed[33] e` | -- |
| `pieces_of_eden` | `Adrift_130` | `feed[3] x officer` | re-synchronises afterwards |
| `qui_a_tue_dana` | `Adrift_162` | `feed[20] parler` | 62/63 echoed; T21 run400 prints the refusal **twice** where Scarier prints it once |
| `saffire` | `Adrift_134` | `feed[5] turn on torch` | everything after is out of step |

`mortality` used to carry the note "Scarier answers every numbered choice
with *Stephanie is expecting an answer*".  That was the blank-line drift, not
the menu; after the fix its only problem is the one lost `e`.

### The seen-model finding

`asdfa` T2 (`x cauldron`) and `cellar` T43 (`x dust`) are the same bug and
the most concrete result of the batch.  run400 says **"You see no such
thing."**; Scarier says **"You can't see the &lt;X&gt; from here!"**.  Per
`sclibrar.cpp:4383-4420` that second wording is 4.0's second matcher pass
over everything the player has *seen* (`lib_absent_seen_object()` /
`lib_cant_see_absent_object()`).  So the rule is ported correctly and the
divergence is in the **seen model**: Scarier is marking these objects seen
where run400 has not.  Chase the seen marking, not the message.

### Two harness traps found and fixed while driving

- **`drive.exe`'s `ClaimName()` is not atomic across processes.**  At 8-way,
  `griswold` and `shore` both claimed `Adrift_142.txt` and one overwrote the
  other (the wreck is kept as `Adrift_142_COLLIDED_griswold_shore.txt`).
  `par.sh` now picks the transcript name itself before dispatching a row --
  a `next_free` scan plus an immediately created placeholder file, passed to
  `fast.sh` as `TRANSCRIPT=` -- so the name is decided in one process.
  Renumbering batch 3 (153-174) also came out of this.
- **The compare tool and the driver disagreed on the feed's encoding.**
  `drive.cs` reads the cmdfile as UTF-8; `compare_wine_transcript.py`'s
  `read_feed` reads it as latin-1.  On `qui_a_tue_dana` that made the tool
  mis-read its own feed and report three accented commands ("prendre
  téléphone") as lost when the Runner had echoed them perfectly.  Compare
  non-ASCII feeds against a CP1252 copy until `read_feed` is taught UTF-8.

Three jobs (`griswold`, `mysteryofcaves`, `dragonshrine`) also failed with
"transcript was not created" at 8-way and succeeded unchanged at 3-way, so
8 is above this machine's comfortable ceiling for the Save-dialog step.  The
0-byte stubs those failures left behind were then read by the comparison as
"every command lost"; if a row reports total loss, check the transcript's
size before believing it.

## Measured 2026-09-06: 165 MORE 4.00 rows -- the corpus, not just the candidates

The "Candidates" table was never the whole 4.00 pool.  `run_v4_walkthroughs.sh`
has 351 rows; 186 of them are 4.00 games (`.taf` header byte 10 = 0x3e) that
the table had never listed.  Of those, 11 are `SCR_SEED` rows and 2 are
`SCR_ASSUME_COMBAT`/`SCR_ASSUME_MOVES` rows -- both classes are unmeasurable
against the Runner, which reseeds itself and has no assist flags -- and 8 had
already been driven under another name.  **The remaining 165 were all driven**,
5 Runners at a time, 15417 commands, one hard failure.

    cd ~/adrift-battle/runner/wine && ./par.sh jobs_q.txt 5    # 165 rows
    ./par.sh jobs_r.txt 5                                      # the 14 re-drives

Feeds are `cmdfile_q_<solution>.txt`, games are staged as
`w_<solution>.taf`, transcripts are `Adrift_175..~351_<solution>.txt`.
Scoring is `harness/compare_wine_transcript.py` (see the five tool fixes
below); the per-row verdicts are the table in "### 4.00 -- 165 more games".

| verdict | rows |
|---|---|
| clean -- identical on every turn | 21 |
| clean but for the `[Press any key to end]` tail | 31 |
| clean but for whitespace | 1 |
| diverging, 1-9 differing turns | 53 |
| diverging, 10+ differing turns | 34 |
| lost a feed command (not evidence yet) | 25 (incl. `dreamquest`) |

These are the **twice re-scored** numbers (2026-09-06, after tool fixes 4 and
5 below).  The first pass through the same transcripts read 11 clean / 24
endtail / 104 diverging: sixty rows changed verdict on fix 4, all but three
of them for the better, because the comparison -- not the engine and not the
drive -- was feeding scarier one extra empty command per pause.  Fifteen rows
went straight from "diff 40, desynced from turn 0" to clean.  Fix 5 moved six
more: `foresthouse3` 40 -> 8, `iachini` 40 -> 21, `wes_ghn` 40 -> 8,
`the_town_of_azra` 16 -> 9, `tictactoe` 5 -> the ending tail alone.  (The
end-tail test also learned to match the Runner's tail by its BRACKETS rather
than by the English inside them, which recovered `ilgolem`'s
`[ Game over ... premi un tasto]` and `elascensor`'s
`[Pulsa cualquier tecla para terminar]`.)  Any diff count
written down before those fixes is worthless; re-score before quoting one.

`tictactoe` going clean matters beyond its own row: its T0 (`1` answered
"... What?" in run400 while scarier started the game) was the last surviving
scrap of the "numbered-choice menu" lead, and it is gone.

`dreamquest` is not a harness failure: run400 answers
`Error loading adventure - [Subscript out of range,9,10]` and never opens a
window.  Scarier loads and finishes it.  Six other rows raise a run-time
`evaluate error - Subscript out of range` mid-game and keep going.

**Why run400 will not open it (settled 2026-09-06).**  `Dream Quest.taf` is a
perfectly ordinary ADRIFT 4.00 file -- exact V400 signature, serial
`00026161`, zlib from offset 22, 15-byte `Wild` trailer, and tafpretty parses
it to the last line with nothing left over (68 rooms, 101 objects, 58 tasks,
11 events, 10 NPCs).  The file is fine; **run400 has a load-time bug**, and one
single task trips it.

Bisected under Wine by rebuilding the game with taftool and cutting it down
(`try.sh` = launch run400, look for the 274x114 error dialog): rooms+objects
alone load; adding the tasks fails; halving the task list narrows it to
**task index 22** (0-based), the only task in the game with an **empty Command
vector**.  That task is command-less on purpose -- it has `CompleteText`
"You fall limply to the ground, drained. Boy were you suckered!", `Where` =
room 63 and a single action, so it is only ever meant to be fired by another
task or an event.  Rebuilt with one dummy command line added to that task and
*nothing else changed*, the whole game opens in run400 and plays.

The P-code says why.  openadv's task loop reads the Command count and then
does a bare

    ReDim cmd(0 To n - 1)        ' 490DA7-490DBE, no clamp

so `n = 0` asks VB6 for `0 To -1`, which is error 9, "Subscript out of range" --
raised at load stage &HA (490D26), hence the `[...,9,10]` in the dialog.  Every
other string vector in the same record is clamped: ReverseCommand at
490EAC-490EF5 wraps the bound in `Proc_21_0_442D10` (= `Max(a, b)`), which is
why all 58 of this game's tasks having zero reverse commands, and 61 of its
objects having zero aliases, cost nothing.  So an empty Command list is the
one case ADRIFT 4.00's own Runner cannot load, and the Generator will happily
write one.

Consequence for this file: `dreamquest` can never be Runner-measured.  It is
not in the "no transcript yet" backlog, it is out of reach permanently.

### Five tool fixes this batch forced

1. **The startup gender form is not the Hiscore Table.**  ADRIFT asks
   "Please choose player gender" in a VB form (`ThunderRT6FormDC`), the same
   window class as the end-of-game Hiscore Table, so `drive.cs` read it as
   "the game has ended" and aborted five rows at command 1.  There is now a
   gender branch before the Hiscore branch; it answers from the same
   `POPUP_ANSWERS` queue the name InputBox draws from (so a game with both
   wants `POPUP_ANSWERS="<name>|male"`) and clicks the button with
   **`BM_CLICK`** -- a VB command button ignores the `WM_COMMAND` id that
   works on a `#32770` dialog.
2. **`POPUP_ANSWERS` is not optional.**  `make_wine_cmdfile.py` prints it and
   leaves the name/gender answers OUT of the command file; 20 rows were driven
   without it, so the Runner got an empty name and every later `%player%`
   diverged.  Worse, the compare tool replays the *command file* through
   scarier, which asks the same questions on stdin -- so even a correctly
   driven row scored wrong.  `compare_wine_transcript.py` now takes
   `--popup ANSWER` (repeatable) and prepends them to scarier's stdin.  This
   alone turned batch 1's `imagination` from "DIFF13, Scarier starts in a
   different state" into a clean row.
3. **`timeout` does not exist on macOS.**  A comparison sweep wrapped in it
   wrote `command not found` into all 53 outputs, and the classifier scored
   every one of them "clean" because they contained zero turn blocks.  Use
   `perl -e 'alarm 420; exec @ARGV' sh ...`, and make the classifier report an
   empty output as NORUN rather than as agreement.
4. **The comparison was feeding scarier the Runner's pause Returns.**  This
   was the big one, and it invalidated the whole first scoring pass.
   `make_wine_cmdfile.py` puts one BLANK line in the command file per
   `<waitkey>` the game reaches, because the Runner has no
   `SCR_SKIP_WAITKEY` and each pause eats a keystroke.  `read_feed` drops
   those blanks on the Runner side -- but `run_scarier` kept them, and forced
   `SCR_SKIP_WAITKEY=1` on top, so scarier saw each one as an **empty
   command**: it prompts, and it ticks the events.  The two streams therefore
   drifted by one more turn after every pause in the game, cumulatively, and
   no single `--offset` could hold them together.  `run_scarier` now drops
   the blanks when the row is SKIP-wired and keeps them when it is not (a
   non-SKIP row's blanks are the solution's own pause answers, which scarier
   really does eat), and no longer forces the flag on -- the caller passes
   the row's real env.  `zelda` went from 200 scarier turns at offset 8,
   diverging from turn 0, to 188 turns at offset 0 with a single real
   divergence at turn 60.
5. **...and then, on a not-SKIP-wired row, scarier's own pauses ate the
   commands.**  Fix 4 was half a fix.  Replaying a non-SKIP row without the
   flag makes scarier stop at every `<waitkey>` and READ A LINE -- but the
   command file has no line for the startup pauses to read, because
   `make_wine_cmdfile.py` hands those to the driver's `PRE` and strips the
   blanks.  scarier's startup pauses therefore swallowed the first real
   commands, scarier turn 0 became `feed[PRE]`, and no forward `--offset`
   could put the two sides back together: that is the whole of
   `iachini`/`the_town_of_azra`/`wes_ghn` reading as whole-game divergences
   from turn 0.  The replay now **always** forces `SCR_SKIP_WAITKEY=1` (a
   pause is pure output; skipping one changes no game state) and is fed the
   FEED rather than the file, so feed[i] is scarier turn i by construction.
   What is left is deciding which blank lines are feed entries, and that is
   measured, not assumed: `read_feed` replays the candidate feed with
   `SCR_MARK_WAITKEY=1`, counts the pauses each command printed, lets each
   pause eat the blank that follows it, and iterates to a fixed point.  Both
   answers occur -- `lobster`'s ten blanks all answer real pauses, while
   `sommeril` has no `<waitkey>` at all and its four blanks are real empty
   commands the Runner echoes as `> ` -- so a rule either way is wrong.
   A pause sitting on a NON-blank line is left in the feed on purpose: the
   Runner ate that command too, and the lost-command report should say so.

### The `<centre>` join is a TRANSCRIPT artefact, not an engine difference

Six rows (`cowboyblues` 40/40, `onnafa` 36/40, `yadfa` 30/40, `grumble`
25/40, `bloodrelatives`, `deadman`, and single turns in `threeminutes`,
`warlord`, `worstgame`) differed only in whether a separator exists.  YADFA
is the clean case: the room Long is `<ding0>Market<ding1>As markets go...`
where the ALRs expand `<ding0>` to `<b><centre><font ...>` and `<ding1>` to
`<font size=12></centre><font ...>`.  The Runner's `Adrift_N.txt` has
`The MarketAs markets go` with nothing between them; scarier emits a newline.

Settled by reading the Runner's own RichTextBox instead of its transcript --
`fast.sh` now takes `DUMP_SCROLLBACK=<file>` (drive.exe `--dump-scrollback`,
UTF-16 out):

    DUMP_SCROLLBACK=/tmp/yscroll.txt ./fast.sh w_yadfa.taf /tmp/ydump.txt run400.exe 0

The control holds `Outside the castle walls\nThe walls of Castle...`.  **The
Runner does break the line; its transcript writer drops a break that exists
only as a paragraph-alignment change.**  scarier is right, and none of those
rows is a bug.  `compare_wine_transcript.py` now reports a whitespace-only
turn under its own heading and counts it separately -- it does not hide it,
because a genuinely missing join looks identical from the transcript alone
(cf. `adrift-walk-announcement-join`); `DUMP_SCROLLBACK` settles any one case.

### The sharpest new leads

Re-derived from the post-fix scoring (2026-09-06), and unchanged by the
second re-score -- none of the six rows fix 5 moved is on this list.  Ordered
by how small the reproduction is.  `oldchurch` T17 and `thorn` T26, listed
here before the re-score as a task-matching pair, are both **clean but for
the ending tail** now; so is the whole "numbered-menu cluster" that was
written up here (see the batch-1 section), and so, after fix 5, is
`tictactoe`, which had been its last surviving member.

- ~~**A pronoun-resolution echo, `showtime` T59.**~~  DONE 2026-09-06; see
  "Ported 2026-09-06: the character pronouns have no-antecedent seeds too"
  below.  It was the third member of the `adrift4-pronoun-echo-article` /
  `adrift4-ask-echo-before-tasks` family, and it dragged a second rule out
  with it (`thelasthour` T80, the ask/talk-to `about` split).
- ~~**Battle damage wording, 2 rows.**~~  **RETRACTED 2026-09-07.**  The
  "opposite directions" reading was stale -- the current build misses on both
  rows, and both turns are plain rolls (Beast Accuracy 35 vs a player Agility
  rolled 0..49; MoReLaND 0..100 vs 0..100).  `del_sol` is not double-seed
  invariant either.  RNG rows, do not chase.  The battle formulas are
  unchanged and correct; what was really broken in this area was
  `scr_randomint` on a backwards range -- see "Ported 2026-09-07: a backwards
  random range still draws" at the end of this file.
- ~~**`icecream`, 3 turns, 3 commands in.**~~  **DONE 2026-09-07**, and it was
  two unrelated rules.  T0 was not an alternate description at all -- it is the
  game's own task 14 FailMessage, which run400 reaches because its take piece
  dispatches `get <the object>` before BOTH refusals, not just before
  `can't take`.  T2 is the 4.0 put/drop list parser leaving the command line
  clobbered to the fragment `put ice cream ` when the direct object names
  nothing present.  See "Ported 2026-09-07: `icecream`'s two rules" at the end
  of this file.
- **`bandera` T18 -- NOT the seen model after all.**  `x marife` -> run400
  `No ves tal cosa.`, scarier describes her.  It looked like batch 1's
  `asdfa` T2 / `cbn` T6 / `cellar` T43, but those three were the object
  resolver and this one is a character.  DONE 2026-09-07: it is the
  case-sensitive tail of the character resolver, tripped by the game's own
  SYNONYM replacements -- see "Ported 2026-09-07: the character resolver's
  case-sensitive tail" below.
- **An object list one side prints and the other does not, 3 rows.**
  DONE 2026-09-06 for the two real ones -- see "Ported 2026-09-06: the
  room-content listing is o(132), not 'is the InRoomDesc empty'" below.
  `camelot15` T32 and `takeone` T4 both match now (`Proc_19_75_449B6C`
  @00449B6C; an object whose OnlyWhenNotMoved byte is still unspent is never
  listed, even with an empty InRoomDesc, and the byte is spent by the library
  take AND by any task move action).  `perspectives` T0 turned out not to
  belong here at all -- it was the room lister's whitespace, and it was
  DONE 2026-09-07; see "Ported 2026-09-07: the room block is one string,
  joined by pspace()" below.
- **An event line one side prints and the other does not, 6 rows.**
  `skydiver` T15 (`Pelican A pelican flocked toward me..` only in run400),
  `briefcase` T5 and `backhome` T36 (only in run400), `overtheedge` T1,
  `bigcitylaundry` T1 and `stationxiii` T25 (only in scarier).  The heading
  is a guess and `skydiver` has already broken it: that line is not an event
  at all and not a tick error either -- the walk step fires on exactly the
  right tick in both engines and the ARRIVAL ANNOUNCEMENT was suppressed.
  DONE 2026-09-07; see "Ported 2026-09-07: a Hidden walk stop stamps the
  walker's location whether or not it moved" below.  Five rows left, and
  they should be re-triaged one at a time rather than as a set.
  **CLOSED 2026-09-07.**  Re-triaged one at a time, and the heading was wrong
  for all six: `briefcase` T5 was already fixed and its real divergence was
  `%status_door%` at T10 (an object lookup, not an event), and `overtheedge`
  T1, `stationxiii` T25, `bigcitylaundry` T1 and `backhome` T36 are all RNG --
  each one is an event with a random Time1..Time2 duration, and a seed exists
  that reproduces run400 exactly (`bigcitylaundry` at SCR_SEED=42, `backhome`
  at SCR_SEED=3, `overtheedge` at seed 5).  See the item-5 entry under "Next
  candidates" for the per-row detail.
- **`suburbanprodigy3` T31 `stats`** -- DONE 2026-09-07.  run400 ran a game
  task, scarier answered with a built-in status line (`Celler | Score: 80`),
  and the guess was right: `stats` is a SCARE invention no Runner carries.
  Synonym dropped; the row is clean.  See "Ported 2026-09-07: `stats` is not
  a Runner command" at the end of this file.
- **`reactor1` T10** -- run400 `A quick glance at the computer` and
  `Initializing ... failed!`, scarier `at the console` and `... done!`.
  CLOSED 2026-09-07, RNG.  The game holds two tasks on the same command with
  different CompleteText, one for a successful coolant vent and one for a
  failed one, so the opening sentence differs by *branch*, not by
  substitution.  run400 failed; scarier fails too at seeds 1, 2, 3, 6, 7, 8
  (and at 1, 6, 7, 8 the whole compare is clean), succeeds at 4 and 5.  The
  harness row stays pinned at `SCR_SEED=4` because the walkthrough is wired
  to the winning ending.
- **`lca` T91** -- `chop tree` -> run400 asks `Which tree.  The tree or the
  tree?`, scarier refuses outright.  A disambiguation prompt over two
  identically-named objects; see `adrift4-disambiguation-and-alr-oracle`.
  MEASURED 2026-09-07 but **not yet ported** -- the whole 4.0 rule is written
  up under "Measured 2026-09-07: the 4.0 object-ambiguity prompt" at the end
  of this file.  `lca` T251 (`ne`, scarier lists "The ever alluring Daisy is
  here." in the Haunted House room block and run400 does not) is a second,
  unrelated divergence on the same row and is still untouched.
- **`wax_worx` T15** -- `ask charlie about house` opens with a different
  sentence on each side; the rest of the answer matches.
- **`trabula` T8 and `threeminutes` T8** are one character each: a leading
  `A`/`a` on a joined sentence, and a counter reading 46 vs 53.
  `threeminutes` DONE 2026-09-07 -- the counter is `timer += rand(-1,-3)` and
  it was running at half speed; that row is now run400-identical on every
  turn.  See the backwards-random-range section at the end of this file.
- **RNG rows, do not chase.**  `jinxtron` T6 differs only in a randomly
  chosen word (`HOOSELDORF` vs `EIGHT`); `worstgame` T10 and `woof` T24 pick
  different members of a random message list.  The Runner reseeds itself, so
  these are unmeasurable by construction.

### Rows that lost a feed command (25) -- but half of them are not feed bugs

The 11 mid-game rows were all re-fed on 2026-09-07 and all 11 are settled --
see "Measured 2026-09-07: the 19 re-fed rows" at the end of this file.  The
guesses below about `datewithdeath` ([MORE]), `crookedestate` (a file
dialog) and `mustescape` (the echo matcher losing the ASCII art) were all
wrong; the verdicts below are the PRE-re-feed ones.

`aliasagent`, `cowboyblues`, `crookedestate`, `darkness`, `datewithdeath`,
`donuts_intro`, `dreamquest`, `egghunt`, `Glum_Fiddle`, `grumble`,
`howitstarted`, `mould`, `mustescape`, `patient7`, `reluctantvampire`,
`riding_home`, `rking`, `scandal`, `sswhore`, `sun_empire`,
`TheADRIFTProject`, `thelasthour`, `volant`, `warlord`, `will`.

`dreamquest` is the trivial one: its transcript is a zero-byte file because
run400 never opened a window (see the load error above), so every command is
"lost".  Checking where the other transcripts stop splits them in two:

- **The game had ENDED (13 rows)** -- `aliasagent`, `darkness`,
  `donuts_intro`, `egghunt`, `howitstarted`, `patient7`, `rking`, `scandal`,
  `sswhore`, `sun_empire`, `volant`, `will`, `TheADRIFTProject`.  The Runner
  stops taking input at `[Press any key to end]`, so every remaining
  walkthrough line is unechoed *by design*.  Six of them lose exactly one
  command and it is a trailing `score` or `quit`.  These are not feed bugs
  and re-driving will not change them; they are the Runner side of
  `adrift4-actions-after-endgame`.  Two are worth a second look because the
  game ended EARLY: `scandal` dies at feed[1] (`I'm afraid you are dead!`,
  score 0/0) where scarier plays on, and `darkness` finishes at feed[99]
  with 11 walkthrough lines still to go.  `TheADRIFTProject` is a Runner
  crash -- `Run-time error '401': Can't show non-modal form when modal form
  is displayed` at command 92.
- **Stopped mid-game (11 rows)** -- `cowboyblues`, `crookedestate`,
  `datewithdeath`, `Glum_Fiddle`, `grumble`, `mould`, `mustescape`,
  `reluctantvampire`, `riding_home`, `thelasthour`, `warlord`.  These are
  the real feed problems.  `datewithdeath` stops on an unanswered `[MORE]`
  (a pause `make_wine_cmdfile.py` did not count), `crookedestate` on `save`
  (a file dialog), `mould` on `hint` (a hint window).  `mustescape`'s
  transcript is full of ASCII art and echoed 99 prompts, so its "20 lost from
  feed[2]" is probably the echo matcher losing the art, not the Runner losing
  keys.

### Measured 2026-09-06: sommeril re-driven, both model-derived turns hold

`sommeril` was the last row still carrying an unsettled model-derived edit.
Re-driven with `cmdfile_s_sommeril.txt` (79 lines, `take placemat` at line
45 and `put fish in water` at line 16) into `Adrift_353_sommeril.txt`:

    python3 harness/make_wine_cmdfile.py sommeril ~/adrift-battle/runner/wine/cmdfile_s_sommeril.txt
    ./fast.sh sommeril.taf cmdfile_s_sommeril.txt run400.exe 0

**79/79 echoed, offset 0, and neither edited turn is among the differences.**
Both changes -- the trailing-space rule that made `get placemat ` unreachable
by `get placemat`, and the 4.0 put/task precedence that makes `put fish in
water` reach task 18 -- are now measured, not model-derived.

What remains is ten turns of atmospheric events, and all three of them are
random-interval: `SCR_DUMP_TASKS=1` gives `EVENT 0 [bells] starter=2
restart=2 start=15..25`, `EVENT 1 [Hooded Man] start=10..20`,
`EVENT 2 [mice] start=15..20`.  The Runner reseeds itself, so the hooded man,
the mice and the bell land on different turns on the two sides and nothing
else does.  The row is clean.

This drive is also what turned up tool fix 5: the game has NO `<waitkey>`
anywhere, so its four opening blank lines are empty commands -- run400 echoes
`> ` and answers each with "Much like a dream, that never happened." -- and
the comparison had been dropping them, reporting an offset of four and ten
false event-timing divergences.  The stale comment in the solution that
called them "the four intro `<waitkey>` pauses" is corrected.

## Next candidates

The 4.00 pool is now **fully driven**: 124 candidate rows plus the 165 corpus
rows found on 2026-09-06, 289 in all, minus the 13 `SCR_SEED`/`SCR_ASSUME`
rows that cannot be measured against the Runner and `dreamquest`, which
run400 refuses to load.  There is nothing left to *drive*; what is left is
follow-up on rows that have been driven, in this order:

1. **The seen model** -- `asdfa` T2, `cbn` T6, `cellar` T43, `bandera` T18.
   DONE 2026-09-06 for the object rows (see "Ported 2026-09-06: the faithful
   seen seed and the 463640 second pass" below): `asdfa` identical, `cbn` T6
   and `cellar` T43 match.  `bandera` T18 turned out NOT to belong here --
   it is a character, and the rule is the resolver's case-sensitive tail;
   DONE 2026-09-07, see the last section.
2. **Parenthesised parser asides** -- DONE 2026-09-06.  `showtime` T59
   `(No female) ` is ported (see "Ported 2026-09-06: the character pronouns
   have no-antecedent seeds too" below), and with it a second rule the fix
   exposed, the ask/talk-to `about` split.  The other member, `frustrated`
   T53 `(Taking the small rock first) `, was done the same day -- see
   "Ported 2026-09-06: the 4.0 implicit take runs before the put handler's
   task look-up" below.
3. **The room-content listing** -- DONE 2026-09-06.  `camelot15` T32 and
   `takeone` T4 match; ten goldens re-blessed, six measured rows improved.
   `perspectives` T0 was reclassified: not the listing rule, but the room
   description's whitespace defeating an ALR -- and that was done in turn on
   2026-09-07, see the last section.
4. **Battle hit vs no-damage** -- DONE 2026-09-07, and the item's own premise
   was wrong twice over.  `shadow_of_the_past` T18 and `del_sol` T44 do *not*
   "disagree in opposite directions": the current build misses in both, and
   both are plain rolls -- the Beast's Accuracy is a flat 35 against a player
   Agility rolled 0..49 (70% hit), MoReLaND's is 0..100 against 0..100 (a coin
   flip), and running the seeds produces every outcome.  `del_sol` is not even
   double-seed invariant (seeds 97 and 424242 differ by 4 lines), so it should
   never have been on the measurable list.  Both are RNG rows; do not chase.
   `hyper_b_s` T4 *was* a real engine bug, but not in the Battle System at
   all -- it was `scr_randomint()` refusing an author-entered backwards range.
   Fixed, and the whole `hyper_b_s` fight is now run400-identical number for
   number.  See "Ported 2026-09-07: a backwards random range still draws"
   at the end of this file.
5. **A single event one tick out** -- `skydiver` T15, `briefcase` T5,
   `backhome` T36 (run400 prints it) against `overtheedge` T1,
   `bigcitylaundry` T1, `stationxiii` T25 (scarier prints it).  Six rows,
   split evenly, so this is the tick and not a missing event.
   **The premise is wrong for at least one row.**  `skydiver` is DONE
   2026-09-07 and it was neither an event nor a tick: the Pelican's walk
   stepped on the correct turn in both engines and scarier suppressed the
   arrival line, because a walker that was already nowhere when a Hidden stop
   came round never got the walk-hidden stamp.  See "Ported 2026-09-07: a
   Hidden walk stop stamps the walker's location whether or not it moved".
   Do not assume the other five share a mechanism -- `bigcitylaundry` T1's
   extra text ("Your feet are freezing! Put s...") really does look like an
   event, and the even split that suggested "tick" was an artefact of lumping
   an NPC line in with them.  Re-triaged one at a time, 2026-09-07, and the
   premise is now wrong for four of the six:
   - `skydiver` T15 -- DONE, the walk-hidden stamp (above).
   - `overtheedge` T1 -- **RNG, do not chase.**  The Captain's line comes out
     of a RANDOMIZER event; six seeds gave six different lines, and seed 5
     reproduces run400's exactly.  Not an engine difference.
   - `stationxiii` T25 -- **RNG, do not chase.**  "Something wet lands on your
     nose..." moves turn or disappears entirely with the seed.
   - `briefcase` T5 -- **the premise was wrong.**  T5 was already fixed by an
     earlier port and the re-fed compare is clean (19/19 echoed, only the known
     `[Press any key to end]` tail).  The row's real divergence is at turn 10
     and it is not an event at all: `%status_door%`.  DONE -- see "Ported
     2026-09-07: `%status_<name>%` is the lowest-indexed openable Short match".
   - `bigcitylaundry` T1 -- **RNG, do not chase.**  Event 0 "cold feet" has
     Time1 = 1, Time2 = 4, i.e. a random 1..4-turn countdown, so whether "Your
     feet are freezing!  Put some socks on!" lands inside the 78-command
     walkthrough at all is a seed question.  An earlier note claimed it fired
     on turn 1 "for every seed"; that was wrong -- seeds 1 and 5 print it
     twice, 2/3/4/1234 once, and seed 42 not at all.  At `SCR_SEED=42` the
     whole row is run400-identical apart from the known `[Press any key to
     end]` tail.  There is no paused-event rule here to port.
   - `backhome` T36 -- **RNG, do not chase.**  The recorded T36 divergence is
     long gone (fixed by a later port); the last one standing was at T52, where
     run400's room description carries "You can hear the telephone ringing
     inside the house." and ends the turn with "You hear the telephone start
     ringing."  Both come from the telephone event chain, and event 8
     ("telephone starts ringing", StarterType 3 off task 104) has Time1 = 1,
     Time2 = 3, with events 9 and 10 chaining off it on 10..12 and 6..7 -- so
     the whole chain phases with the seed.  Scarier prints the pair once or
     twice depending on the seed; run400 prints it twice.  At `SCR_SEED=3` the
     row is run400-identical apart from the `[Press any key to end]` tail.
   With those two, **item 5 is closed**: of the six rows, one was a real engine
   bug (`skydiver`, the walk-hidden stamp), one was a real engine bug that had
   nothing to do with events (`briefcase`, `%status_`), and the other four are
   all RNG.  "A single event one tick out" was never a mechanism -- it was four
   random event durations and two unrelated bugs sharing a symptom.
6. **`icecream`** -- DONE 2026-09-07.  Two rules, neither of them the guess in
   this item: the take refusal is a game task's FailMessage that run400 reaches
   ahead of " already carrying ", and `put ice cream in cone` is refused
   because put_drop_list clobbers the command line to `put ice cream ` when
   that fragment names nothing present.  `suburbanprodigy3` used to sit here
   too -- DONE 2026-09-07, the `stats` synonym was a SCARE invention and is
   gone.
7. **The refusal that accompanies a task** -- DONE 2026-09-07, and it was
   three different things.  `qui_a_tue_dana` T21 was the ALR list walked
   once; `cbn2` T17 is not a second message at all but the game's
   DontUnderstand answering an empty feed line (`Adrift_138_cbn2.txt:105`
   has its own `> ` prompt); `relojero` T10 -- and `easter`, which was
   filed elsewhere -- is the real rule: an ending takes the unhandled-verb
   tail off the line (run400 `48AC62` -> `48B4E3`).  See "Ported
   2026-09-07: an ending takes the unhandled-verb tail off the line" at the
   end of this file.
8. **Re-feed the 19 rows that really lost a command** -- DONE 2026-09-07,
   and none of the guesses in this item survived: it was not pacing, and the
   `[MORE]` was already counted.  Fifteen of the nineteen now echo every feed
   command; `mould` and `grumble` lose only the golden's answers to scarier's
   own [Y/N] confirmations; `confession` and `thelasthour` never lost
   anything (both games END where the Runner stopped); `hyper_b_s` is the
   battle divergence of item 4.  Four harness bugs came out of it -- see
   "Measured 2026-09-07: the 19 re-fed rows" at the end of this file.
9. **`scandal`** -- SETTLED 2026-09-06, and it was an engine bug, not a lost
   command.  run400 kills the player at feed[1] and scores 0/0 where scarier
   played the walkthrough out because `Scandal.taf`'s opening battle turn is
   the session's very first `scr_randomint` (the game has no events, so
   nothing draws at load), and the first draw after `scr_reseed_random_sequence`
   was pinned to the top half of every range for every seed below 127774:
   `rand_state = seed*16807 + 2147483647` lands in `[2^31, 2^32)`, so the
   `(rand * range) >> 31` mapping could only pick the upper half.  The
   sailor's `random(0,1)` therefore never came up "she fires" -- 200 seeds,
   200 veer-offs -- while run400 fires about half the time.  Fixed by one
   warm-up step in the reseed branch of `scr_congruential_rand`; turn 1 is now
   99/101 over 200 seeds.  See [[scarier-randomint-lowbit-fix]] for the
   corpus fallout (every seeded transcript re-threaded).
Before chasing any diff count in this file, check that it was produced after
the 2026-09-06 alignment fixes (tool fixes 4 and 5 above).  Sixty of the
batch-2 rows and five of batch 1's changed verdict on fix 4, six and two more
on fix 5, and everything the earlier scoring said about "numbered-choice
menus" was that bug.

Feeds live in `~/adrift-battle/runner/wine/` as `cmdfile_p_<solution>.txt`
(batch 1) and `cmdfile_q_<solution>.txt` (batch 2).  Drive with `par.sh` at
3-5 Runners; 8 lost three Save dialogs.  A row with a name or gender question
needs `POPUP_ANSWERS` on BOTH sides -- `par.sh`'s sixth field for the drive,
`--popup` for `compare_wine_transcript.py`.  Wine driving does not need the
Mac's screen unlocked: the message driver never takes the foreground.

## Ported 2026-09-06: the faithful seen seed and the 463640 second pass

Two engine changes, both from the "seen model" item above, landed together:

1. `scgamest.cpp`: the load-time seed in `gs_populate()` is the loader's
   @004909B5 -- dynamics start seen only when held or worn, statics only when
   `InitialPosition == 1` -- followed by run400's `afteroa` sweep @0046F0B4
   (loop @0046EDA5, stamp @0046EDCE) marking seen everything present and
   reachable in the start room (`Proc_21_53_44B578`).  Gated `>= 3.90`; below
   4.00 statics only, as run390's inline sweep @004418FF.  Nothing before 3.90
   (run370 co() @004261B4 and run380 co() @0042DE60 never read the byte).
2. `sclibrar.cpp lib_absent_seen_object()`: the 4.0 seen-but-absent resolver
   now scores with the Runner's noun score (`lib_verb_object_name_score()`,
   shared with the unhandled-verb catch-all) instead of a count of Short-name
   words.

**The crux the parked note pointed at is settled.**  `Proc_19_88_457034` is
NOT the examine resolver: the P-code (`~/Adrift_decompile/run400/
run400.p32dasm.txt` exists; the earlier claim that it was missing was wrong)
shows the "co(i, 4)" at @00456E6A is one vestigial call with no loop around
it, and pass A @00456E2D (`co(i, 3)`) needs a *present* match.  What examine
actually reads first (@00456DFC) is `MemVar_4942F8`, the result of the
up-front resolver `Proc_21_58_463640(line, 0, 0)` that generaltasks calls
once per line at @0048A3F5:

    pass 1  @00463119-137   candidates = obhere() AND seen
    score   @004632AC-387   Short whole-phrase 1, + first alias 1 (loop exits
                            at 463304), + 1 per Prefix word; no Short/alias
                            hit -> not a candidate (var_9E)
    best    @004633C3-443   higher score -> new best; equal -> tie, var_86 =
                            -i-2, Me(424) = -1
    pass 2  @0046360D-63B   only if pass 1 left no unique winner: GoTo 4630BC
                            over EVERY seen object (@00463143-156), present or
                            not; var_94 (best score) is NOT reset
    result                  >= 0 unique object, -1 none, <= -2 tie

A tie or none falls to 457034's pass A, finds nothing present, and answers
&HFF -- "<player> see no such thing." (@004719D9).  A unique winner that is
not present answers "<player> can't see <it> from here!" (@00471958), and
that branch does NOT set `MemVar_494281`, so it is a real turn; only the
"see no such thing" tail (@00471F02) is administrative.

Evidence, all archived run400 transcripts:

- `cowboyblues` Adrift_330 line 1070, T143 `x wall` in the Sheriff's Office:
  the Back Room's "east wall" (obj 91, alias "wall", visited at line 432) and
  Blood Alley's "walls" (obj 96, alias "wall") both score 1 on the alias and
  tie -> "You see no such thing."  The old Short-word count gave 91 one point
  and 96 none and printed "You can't see the east wall from here!".
- `humbug` Adrift_4 lines 1602-1604 still hold: `X machine` -> the washing
  machine alone is seen -> "I can't see the washing machine from here!";
  `X chute` -> several seen chutes tie -> the ALR'd "Nothing Special.".
- `cellar` Adrift_172 163-164 `x dust` -> "You see no such thing." (the dust
  is a static in a room never visited; the old seed had it seen).
- `volant` Adrift_256 125-126 `x racks` -> the game's DontUnderstand "That
  isn't of any concern to you at the moment." (racks unseen).
- `ghosttown` Adrift_325 520-521 `x posters` -> "You can't see the torn
  pictures from here!", and because that is a turn the dusk event now fires
  after `ninette follow` (Adrift_325 590) as run400 does, the saloon is
  entered in its night text (628-634) and the tumbleweed line moves.  The
  row's diff count against Adrift_325 is 23, down from 34; T2 (the kerosene
  lamp "Also here is") is untouched by this and is the next thing there.
- `foresthouse3`: the four changed lines were all in a golden that had been
  a stuck-on-the-porch run -- the two opening `[Press a key]` prompts ate
  `look under mat` and `unlock door with key`, and the timed ending still
  printed the win marker.  The row now sets `SCR_SKIP_WAITKEY=1`; its
  command list matches Adrift_264 exactly and the first diff is still T48.
- `asdfa` Adrift_143 is now identical on every turn; `cbn` T6 matches.

Re-blessed: `cellar` (1 line), `volant` (1 line), `ghosttown` (event shift),
`foresthouse3` (whole golden).  `cowboyblues` and `humbug` unchanged.  Suite
428/428.  Memories `adrift4-absent-noun-resolution` and
`adrift4-object-seen-model` corrected; `~/Adrift_decompile/index/
annotations.tsv` rows 457034 and 463640 corrected.

Still open from this thread: nothing.  (`cbn` is CLOSED 2026-09-07 -- a
generator bug, not an engine one; `ghosttown` T19 is explained by the
SRD4 measurement.  Both are in the last two sections.)
(`cellar` T114 `take satchel` is DONE 2026-09-07 -- see the last section.)  (`bandera` T18 `x marife` was listed here as an NPC seen
model; it is not -- DONE 2026-09-07, see the last section.)


## Ported 2026-09-06: the 4.0 implicit take runs before the put handler's task look-up

`frustrated` T53-T55 (`Adrift_274_frustrated.txt`) and
`ShadricksUnderground` T81 (`Adrift_285_ShadricksUnderground.txt`) were the
same bug, and it was an ORDERING bug, not a missing message.  Scarier already
had the whole `(Taking the X first)` machinery (`lib_put_implicit_take()` in
`sclibrar.cpp`, ported 2026-09-06 with the put/task precedence split); it just
ran it in the wrong place.

**What run400 does.**  `name_object` (`Proc_19_41_46E5D8`) loops over the
objects the noun matched, and for each one, at `loc_46E2B5`:

  * skips the take if the object is static (`Proc_21_46_44615C`), if its
    parent already IS the target (`obj.global_46 <> var_92`), or if there is
    no target (`var_92 > &HFF`, i.e. `>= 0` -- see
    [[adrift-decompile-signed-byte-literals]]);
  * asks the pre-matcher for a TAKE-family task on the typed line
    (`Proc_19_35_453C50(MemVar_49428C, 1, 1)`, class-filter mode 1) and skips
    the take on a hit;
  * otherwise prints `"(Taking " & name & " first)" & vbCrLf`
    (`loc_46E2EA`-`loc_46E30C`), then runs the take piece
    (`Proc_19_39_46302C @loc_46E31F`), which gives the tasks `get the X`
    first;

and only THEN, at `loc_46E34F`, calls `insides` (`Proc_19_43_46639C`) -- which
is where the canonical `put the X in/on the Y` line reaches the tasks
(`loc_465EB5`), ahead of its own possession test (`loc_465EED`).

So the announcement is emitted **even when a task goes on to claim the put**,
and a `get`-task that claims during the take piece prints its text too, ahead
of the put task's.

**What Scarier did.**  `lib_put_in_backend()` / the `put on` loop tried
`lib_try_game_command_with_object_400()` first and `continue`d on a claim, so
`lib_put_implicit_take()` was only ever reached when no task wanted the line.

**The fix** (`sclibrar.cpp`, both backends): call `lib_put_implicit_take()`
before the task look-up, and let the look-up run even for an object the take
could not acquire -- a claim then clears the object out of the "You are not
holding ..." report (`multiple_references`), matching run400's order of
`insides`' `tasks()` call ahead of its possession test.  No new gate, no
version work: `lib_put_implicit_take()` still returns immediately for
pre-4.0.

**Evidence, both directions, straight from the Runner transcripts.**

  * `frustrated` T53 `put small rock on left pan` matches task 511
    `put*small*left*` (three commands, CompleteText `I put the small rock on
    the left pan.`), and run400 still opens with `(Taking the small rock
    first)`.  The mode-1 class filter is why: none of the three spellings
    contains `get`/`take`/`pick`, so the take gate's pre-match finds nothing.
    T53, T54 and T55 all match now; the row is `diff 4` -> `endtail 1`.
  * `ShadricksUnderground` T81 `put large boulder on short plinth`
    (transcript line 517) is the harder case -- announcement, then a
    `get`-task claim, then the put task, with the put text JOINED to the
    get-task refusal by the usual two spaces:

        > put large boulder on short plinth
        (Taking the large boulder first)
        "Grrrrr! Hrrrrrmmmmp. EEEeeeeOooKaaaaay!"

        It's too heavy.  I can pick it up for a second, but that is about
        it.  I'll have to find another way to move it.  "Mmmmgggphh. ...

    Scarier reproduces the whole turn, joining included; the row is
    `diff 12` -> `diff 11` with the first difference unchanged at T42 (bat
    walker drift).

**Corpus 428/428** after re-blessing three goldens -- `frustrated`,
`ShadricksUnderground` and `hcw`.  `hcw`'s `put susan in trunk` turn gained the
same shape (`(Taking sleeping Susan first)` + the Fembot get-task + the put
task); its own run400 transcript had already diverged well before that turn
(T81 `turn on intercom`), so it is blessed on the rule, not on a measurement.
Only four measured transcripts contain `(Taking ` at all -- `frustrated`,
`ShadricksUnderground`, `Glum_Fiddle` (lost feed[1], unchanged) and
`humbug` (already clean) -- so the blast radius of the reorder is exactly the
three rows above.

## Ported 2026-09-06: the character pronouns have no-antecedent seeds too

`showtime` T59, `get her hand`, is the whole measurement:

```
> get her hand
(No female)
"Katie!" you say as you throw your arm out to her.
```

Scarier printed the task text with no aside.  It already knew that `it` and
`them` with no antecedent resolve to the literal string "Absolutely nothing"
and are echoed and spliced as such -- there is no "no reference" state in any
Runner, only a seeded one.  What it did not know is that the *character*
pronouns have the same treatment, from their own registers:

- 4.0 keeps two, seeded at new-game with "No male" and "No female"
  (run400 `loc_45A7F9` / `loc_45A800` in `Proc_19_4_45AA98`), reassigned by
  the character's gender byte at `loc_47F3B9` / `loc_47F3D0`.
- 3.9 is the same shape, seeds at run390 `loc_434969` / `loc_434970` in
  `clear()`, assignment at `loc_4592D7` / `loc_4592EE`.
- 3.7 and 3.8 keep ONE register for all four of `he`/`him`/`she`/`her`,
  seeded "Nobody" (run370 `loc_42398D` -> `MemVar_4460B4`, read at
  `42CAFA` / `42CBC6` / `42CC9B` / `42CD67`; run380 `loc_4289F1`).

So the gate is `>= TAF_VERSION_390` for the two-register form, "Nobody"
below it.  As with `it`, the echo and the splice are the same string, which
is why the task still fires in run400: `get no female hand` still matches
task 52's wildcard command `get * hand`.  `uip_replace_pronouns()` in
`scparser.cpp` now carries an explicit `echo` alongside the replacement
instead of special-casing "Absolutely nothing" at the `pf_buffer_reference()`
call.

### The rule the fix exposed: the ask/talk-to `about` split

Blessing `showtime` broke `thelasthour`, whose T80 is `ask sly about him` in
a game with no male character at all.  With the seed in place scarier echoed
`(No male)` correctly and then answered

```
Use the format "ask [character] about [subject]".
```

where run400 (`Adrift_297_thelasthour.txt` line 402) answers

```
(No male)
I can't talk to that.
```

Every Runner runs one `c("ask") Or c("talk to")` block (run400
`loc_488B61`..`loc_488B71`) that immediately splits on `c("about")`
(`loc_488B87`):

- **with** "about": look for an object named anywhere in the line and answer
  "<You> get no reply from <it>." (`loc_488BFA`); failing that, and only if
  the response buffer is still empty, seed it with
  `MemVar_4941D0(0) & " can't talk to that."` (`loc_488C65`).
- **without** "about": the `ask [character] about [subject]` hint
  (`loc_488CB3`), gated on `MemVar_4941F8 = 0`, i.e. no task ran.

The hint scarier was printing is the *other* branch.  Same split at run380
`loc_444039` (seed `loc_44410C`), run370 `loc_43E9B7` (seed `loc_43EAA8`),
run390 `loc_45D8E5`; the string is in all four exes, so no version gate.
It also explains why "No-one listens to your rabblings." never appears for
`talk to X about Y`: that clause (`loc_488DB6`) fires only when the response
buffer is still empty, and the seed has already filled it.

`lib_cmd_ask_about_nothing()` in `sclibrar.cpp` prints it, wired into
`STANDARD_FALLBACK_COMMANDS` as `ask * about *` and `talk to * about *`
immediately above the `ask *` / `talk *` catch-alls -- so the character rows
(`ask %character% about %text%`), the object row (`ask %object% *`) and any
matched task all still outrank it, which is the Runner's order.

Suite unchanged at 428 PASS; two goldens re-blessed (`showtime`,
`thelasthour`).  `showtime` re-driven still reports a first difference at the
same turn, but that is the feed's two blank `<waitkey>` lines drifting the
streams -- the tool notes "re-synchronised, scarier turn +2" two turns later
-- and not the echo, which now matches character for character.


## Ported 2026-09-06: the room-content listing is o(132), not "is the InRoomDesc empty"

`camelot15` T32 and `takeone` T4 -- item 3 of the "next candidates" list --
were both scarier printing an `Also here is ...` / `On the ground is a jewel.`
line that run400 does not print at all.

The whole rule is one Runner function, `Proc_19_75_449B6C` @00449B6C, called
four times inside `viewroom` @00472CA4 -- once from the description pass
(@0047257C) and once from each of the three list passes (@0047260C,
@0047264F, @0047270D, each under `Not(...)`).  It takes an object and a room
and answers "this object's presence is handled by the description branch":

```
o = Objects(obj)
If (o(26) = room And o(24) = 0 And o(127) = 0)          ' dynamic, ListFlag 0
   Or (o(24) = 1 And o(127) = 1 And o(28)(room) = 1)    ' static, ListFlag 1
Then
  If o(128) = "" Then                                   ' InRoomDesc empty
    If o(132) = 1 Or o(132) = room + 1 Then Result = True
  Else
    If o(132) = 0 Or o(132) = 1 Or o(132) = room + 1 Then Result = True
  End If
End If
```

`o(132)` is the OnlyWhenNotMoved field, read straight from the .taf
(@00490B7A).  The loader immediately *freezes* mode 2 into a room number --
`If o(132) = 2 Then o(132) = o(26) + 1` @00490B96 -- where `o(26)` is the
location code it has just computed from InitialPosition (@00490255-@004902BD:
0 hidden -> -1, 1 held -> 0, 2 in-container -> -10, 3 on-surface -> -20,
>= 4 -> InitialPosition - 3).  So mode 2 means "show it only in the room it
started in", and the comparison `o(132) = room + 1` is that frozen room.

Mode 1 -- "only when not moved" -- is a *spendable byte*, not a live query of
where the object is.  There are exactly two writes to it after the loader,
and both say `If o(132) = 1 Then o(132) = &HFF`:

- the library take, at @0047BF66 in `takes` @0047C83C and again at @00463011
  in `Proc_19_39_46302C` (the "get the X" piece);
- **any task move action on a dynamic object**, at @0048C377 in
  `execute_action` @0048E860 -- immediately after the static refusal
  @0048C371 (`If o(24) = 1 Then GoTo 48C98A`) and *before* the destination
  Select Case, so every destination spends it, including "to hidden" and
  including a move back to the room the object started in.

Nothing else touches it: the event mover does not, `gs_object_move_into` /
`_onto` / `_to_room` have no counterpart, and the other twelve `(132)`
references in the four exes are the *room* struct's alt-description count.

Two consequences scarier had backwards:

- an object whose byte still matches is **never listed**, even when its
  InRoomDesc is empty -- it simply vanishes from the room text.  That is
  camelot15 (four cocktails, mode 1, empty InRoomDesc) and takeone (the
  jewel, mode 2, empty InRoomDesc, still in its initial room).
- an object whose byte still matches and *has* an InRoomDesc prints the
  InRoomDesc rather than being listed, for as long as the byte is unspent --
  which is longer than scarier's old "has it moved" test, because scarier was
  clearing "unmoved" on every `gs_object_*` position change.

`zelda` is the row that pinned the task-move spend: the small key is
InitialPosition hidden, OnlyWhenNotMoved 1, non-empty InRoomDesc, and is never
taken (an earlier `get key` answers "There is nothing worth taking here.").
The Like-Like task moves it into the Graveyard and run400 then answers "Also
here is a small key." -- because the move spent the byte.  Without the
@0048C377 half, scarier printed the key's own description and `zelda` went
from 5 differing turns to 6.

Engine changes:

- `scobjcts.cpp`: `obj_initial_location_code()` reproduces the loader's
  `o(26)`, and `obj_shows_initial_description (game, object, room,
  inroomdesc_absent)` is @00449B6C's tail.
- `scgamest.cpp`: `unmoved` is now seeded from OnlyWhenNotMoved == 1 at
  `gs_create()` and is no longer cleared by the eight `gs_object_*` movers --
  it is the live half of the Runner's byte, not a position tracker.
- `sclibrar.cpp`: both loops of `lib_print_room_contents()` ask the new
  predicate, and the three library take sites spend the byte.
- `sctasks.cpp`: `task_move_object()` spends it, after the static refusal.

Measured against the archived run400 replays -- every one of the thirteen
games with a transcript is at least as good as before, six are better:

| game | before | after |
| --- | --- | --- |
| `camelot15` | 2 | 1 (only `[Press any key to end]`) |
| `takeone` | 2 | 1 (only `[Press any key to end]`) |
| `ghosttown` | 22 | 18 |
| `aegis` | 4 | 2 |
| `sun_empire` | 35 | 34 |
| `beer` | 27 | 26 |
| `through_time`, `ShadricksUnderground`, `baroo`, `zelda`, `magicshow`, `mould`, `suburbanprodigy3` | unchanged | unchanged |

Direct line-for-line confirmations: `Adrift_277_sun_empire` line 36 (the
clothes trunk's InRoomDesc), `Adrift_269_beer` line 42 (the woolly jumper),
`Adrift_265_aegis` lines 573 and 690 (the unicorn's horn, twice),
`Adrift_351_magicshow` line 242 (the white cloth).

Ten goldens re-blessed -- `sun_empire`, `through_time`, `humbug`, `takeone`,
`beer`, `aegis`, `camelot15`, `magicshow`, `mould`, `ghosttown` -- all of them
either a listing line replaced by an InRoomDesc or a listing line for an
empty-InRoomDesc object disappearing.  Suite 428/428; ADRIFT 5 unchanged
(MATCH 180, DIVERGE 17).

**`perspectives` T0 is not this bug**, but the room lister's whitespace --
its ALR Original is `' Also here is a gun. '`, with a leading *and* a trailing
space, and it could not fire while scarier emitted `"\nAlso here is ...\n"`.
Ported the next day; see the section below.

## Ported 2026-09-07: the room block is one string, joined by pspace()

The last of the four `perspectives` leads, and the one that had been deferred
twice.  Scarier printed the room block as *sections*, one list to a line:

```
<description>\n  \nAlso here is a gun.\n  <NPC sentence>\n
```

run400's `viewroom` (`Proc_19_63_472CA4`, @00472024-00472CA3) never emits a
terminator at all.  It concatenates the entire block onto ONE module string,
`MemVar_4941B0`, in seven appends:

1. the room name block (`vbCrLf` guard, `"<b>" & Short & "</b>" & vbCrLf`);
2. the Long / alternate descriptions;
3. the object InRoomDesc loop @00472515 -- `pspace()` @00472591, then the
   InRoomDesc verbatim @00472596;
4. the "Also here" fallback list @00472690 -- the *hard-coded literal*
   `"  Also here"` @00472696, `isare()` @0047273B, then `Prefix & " " & Short`
   @0047274B-00472764, `", "` @00472784, `" and "` @00472798, `"."` @004727AC;
5. the joined `X, Y and Z are here.` sentence @00472950 -- again a hard-coded
   `"  "` @0047295B, then per character `Left(text, Len(text) - 9)` @004729FE,
   `", "` @00472A2E, `" and "` @00472A42, `" is here."` / `" are here."`;
6. the characters' own in-room texts @00472A93 -- `pspace()` @00472B01, then
   the text verbatim @00472B06;
7. the event LookTexts (already ported, already a `pf_buffer_join()`).

`pspace()` is `Proc_21_50_44A9F4` @0044A9F4 (module `General`):

```
If s <> "" Then
  If Right(s,2) <> "  " And Right(s,1) <> Chr(10) And Right(s,4) <> "<br>" Then
    s = s & "  "
```

-- a *conditional* two-space clause gap, which is why a Long ending in one
space comes out with three (`man_overboard`: `...off the ship.   Bob is
standing...`), and why a Long ending in `<br>` gets no gap of its own.  Steps
4 and 5 are the exception: their two spaces are literals, so they go in
whatever the string already ends with.

**Ported** as `pf_buffer_join()` (pspace + take back our own trailing
newline) at steps 3 and 6, and `pf_undo_auto_break()` + a literal `"  "` at
steps 4 and 5.  `lib_print_room_description()` now records its terminator via
`pf_note_trailing_auto_break()` so the contents can take it back; the block
gets one terminator at the very end, and only if it wrote anything.

The same pass removed `lib_skip_leading_breaks()`.  The Runner's test is
`Right(text, 9) = " is here."` @004729A7 and its trim `Left(text, Len - 9)`
@004729FE, both on the raw text -- so a character whose in-room text opens
with `<br>` keeps that break, and it lands *after* the two separator spaces
rather than instead of them.

**Ground truth.**  `Adrift_240_perspectives.txt` T0 now matches byte for
byte; the pre-ALR string is

```
...wooden planks.<br><br>  Also here is a gun.  On the floor, bleeding
profusely is a dark haired male.   <br>Jonah is here, hammering nails...
```

and the ALR ` Also here is a gun. ` eats the second space of the first pair
and the first of the next, which is exactly the one leading space the
transcript shows before `On top of...` and before `On the floor...`, and the
three trailing spaces before Jonah's `<br>`.  Every space accounted for.

Four more transcripts confirm the run-on shape directly, on games other than
perspectives:

- `Adrift_154_marlin_affair.txt`: `...back of the shuttle.  A screwench lies
  here.  Also here are the handcuffs and a laser spinner.` -- step 3 into
  step 4.
- `Adrift_151_mysteryofcaves.txt`: `...Exits lie: east.  Also here is some
  meat.  Snugg the troll is here, looming massively you.` -- step 4 into
  step 6.
- `Adrift_135_imagination.txt`: `...leads into darkness.  Also here is a
  piece of dental floss and a small rock.  Jenny follows you  from the
  north`.
- `Adrift_226_spooked.txt` lines 120-122 and `Adrift_306_videotapedecay.txt`
  lines 514-516: the description line ends in the two separator spaces, then
  the author's own `<br><br>` opens a blank line, then `Samuel, your
  scientist pal, is here.` -- the raw-text trim.
- `Adrift_1_cybercow.txt` lines 230-231: one `<br>`, so one break.
- `Adrift_42_vagabond.txt`: `George is here.` appears ZERO times; the Runner
  prints the author's ALR replacement `A technician is hunched over a power
  conduit here...` instead.  Scarier now agrees.

**Four goldens changed content, all four of them an author ALR that could
not fire before** -- the same class as `perspectives`, and the real proof
that the Runner's separator spaces are part of the string authors write
against:

| game | ALR Original | was | now |
| --- | --- | --- | --- |
| `perspectives` | `' Also here is a gun. '` | list line unreplaced | `On top of the medicine cabinet is a pistol.` |
| `datewithdeath` | `' Hrolf, Strug and Bark are here.'` | `Hrolf,Your loyal bodyguards - Strug and Bark - are here.` | `Your loyal bodyguards - Hrolf, Strug and Bark - are here.` |
| `vagabond` | (room 4, spans the join) | `A toolbox is here.` / `George is here.` | the technician paragraph |
| `circus` | `'  Joe'`, `'  Leo'`, ... each followed by a lowercase generic | `the vendor is here.` | `The vendor is here.` |

`circus` is the clearest: the author wrote a whole family of `'  <Name>'` ->
`'  The <role>'` ALRs, two leading spaces each, precisely so the room-list
occurrence capitalises while the mid-sentence ones stay lowercase.  That only
works against a room string with the Runner's separator in it.

**Cost.**  288 goldens re-blessed, 284 of them whitespace-only.  Suite
428/428; ADRIFT 5 unchanged (MATCH 180, DIVERGE 17); the 61-transcript
`compare_wine_transcript.py` sweep gives identical verdicts before and after,
which is the no-content-regression gate (that tool collapses whitespace, so
it is blind to this change by construction and useful only as a gate).
`perspectives` goes from `diff 2` to `endtail 1`.

## Measured 2026-09-07: the 19 re-fed rows -- 15 were the harness, not the engine

Follow-up 8 above.  All 19 rows that "really lost a command" were re-fed with
freshly generated feeds (`cmdfile_r_<solution>.txt`, `cmdfile_s_mould.txt`)
and a driver that answers the pauses the feed does not.  **Fifteen of them now
echo every feed command**; not one of the four that still do not is a lost
command.

| solution | transcript | before | after |
|---|---|---|---|
| `cellar` | `Adrift_361` | `feed[119] undo` lost | **132/132** |
| `cowboyblues` | `Adrift_363` | 12 lost, `feed[248..268]` | **271/271** |
| `crookedestate` | `Adrift_358` | `feed[44] save` lost | **47/47** |
| `datewithdeath` | `Adrift_355` | `feed[5] book` -> `> ok` | **303/303** |
| `endgame` | `Adrift_362` | `feed[9] z` lost | **10/10** |
| `Glum_Fiddle` | `Adrift_367` | `feed[1] say cow` -> `> y cow` | **70/70** |
| `mortality` | `Adrift_365` | `feed[33] e` lost | **78/78** |
| `mustescape` | `Adrift_374` | 81 lost, `punch` -> `> nch` | **83/83** |
| `pieces_of_eden` | `Adrift_375` | `feed[3] x officer` -> `> officer` | **11/11** |
| `qui_a_tue_dana` | `Adrift_369` | `feed[20] parler` lost | **63/63** |
| `reluctantvampire` | `Adrift_357` | `feed[189] fang` -> `> g` | **198/198** |
| `riding_home` | `Adrift_368` | `feed[11] wait` lost | **56/56** |
| `saffire` | `Adrift_371` | `feed[5]` -> `> urn on torch` | **16/16** |
| `warlord` | `Adrift_373` | `feed[307] x artefacts` -> `> facts` | **356/356** |
| `mould` | `Adrift_376` | 305 lost (aborted at the first `hint`) | 313/313 echoed, in order -- but the run is NOT COMPARABLE, see below |
| `confession` | `Adrift_372` | 21 `z` "lost" | 16/16 -- **both engines end at turn 16** |
| `thelasthour` | `Adrift_366` | 6 `wait` "lost" | the game ended at 119 -- but the two streams are OFFSET BY TWO COMMANDS, see below |
| `grumble` | `Adrift_356` | `feed[262] y` lost | 262/262 -- the `y` answers `quit` |
| `hyper_b_s` | `Adrift_359` | 6 battle keys lost | **FIXED 2026-09-07** -- the backwards random range, not the battle formulas; all ten damage draws now match run400 |

### Four harness bugs, and none of them was pacing

The note above guessed `#sleep` pacing for `confession` and `hyper_b_s` and
an uncounted `[MORE]` for `datewithdeath`.  All three guesses were wrong.
What was actually broken:

1. **`make_wine_cmdfile.py` split the row's env wrong.**  Twenty-five rows
   space-join two assignments inside ONE `|` field -- `SCR_SEED=33
   SCR_SKIP_WAITKEY=1` -- exactly as the harness's own `env $ENV` word-splits
   it.  The generator partitioned the whole field, so `SCR_SEED` became
   `"33 SCR_SKIP_WAITKEY=1"` and, far worse, the SKIP wiring vanished: the
   replay then stopped at every `<waitkey>` and ate the next solution line as
   the answer, and the feed it emitted was a desynced run of the game.
   `warlord` regenerated 77 blank lines short.  Fixed by splitting each field
   on whitespace.
2. **A `<waitkey>` the feed does not answer eats the next command.**  The feed
   carries one blank per pause *scarier's replay* printed, so a pause only
   run400 reaches -- an author's `[MORE]` in a passage scarier walks past, or
   the text run400 re-prints after `undo` -- has no blank behind it and eats
   the first characters of whatever is typed next.  `drive.exe` now answers
   those itself before each command (`ClearStalePauses`), the way a human at
   the keyboard would.  `warlord` cleared 6, `mustescape` 70, `mould` 12.
   This alone fixed `cellar`, `cowboyblues`, `mortality`, `qui_a_tue_dana`,
   `datewithdeath`, `endgame`, `riding_home` and `saffire`.
3. **A pause can arrive WHILE the command is being typed.**  Clearing before
   typing is not enough: the Runner sets the input-mode byte from its own
   message loop, so a pause whose text was still rendering lands mid-command
   and eats however many characters it is ahead of us.  `drive.exe` now reads
   the entry box back after typing and retypes when it is short (Auto
   complete only ever EXTENDS what was typed, so a short entry is
   unambiguous).  `mustescape` retyped 14 times, `pieces_of_eden` once; both
   went clean, and so did `warlord`'s last stubborn command.
4. **`compare_wine_transcript.py` dropped a `> save` echo.**  `save` is a verb
   a game can give a task of its own -- The Crooked Estate answers it with
   "The estate is decayed beyond saving." -- and the tool skipped that echo as
   if it were the Runner's own Save dialog.  It now skips it only when the
   walkthrough did not type it.  `crookedestate` went from "1 lost" to 47/47
   with no re-drive at all.

### `mould`, `grumble`: a golden's answers to SCARIER's own [Y/N] are not commands

`hint` and `quit` are interpreter meta-commands, not game turns: scarier asks
"Do you really want to view hints? [Y/N]" and reads the answer off stdin
without a prompt of its own, while the Runner puts up a modal window (a
`Hints` VB form; a MsgBox for quit).  So the walkthrough's `y`/`n` lines after
them are answers to *scarier*, and the Runner has nothing to type them into.

- `mould` fed 11 of those, and `drive.exe`'s Hiscore branch read the unknown
  `Hints` form as "the game has ended" and abandoned the run at command 22.
  The driver now closes a `Hints`/`About` form and plays on, and driving the
  hint-free feed `cmdfile_s_mould.txt` echoes **every one of its 313 lines in
  order**.  The 12 the compare tool still calls lost are a PAUSE-COUNT
  difference, not a loss -- see the next paragraph.
- `grumble`'s single lost command is the last line of the walkthrough, the `y`
  that answers `quit`.  262/262 real commands.

`compare_wine_transcript.py` now says so out loud: when every line of the
command file came back as an echo in order, it reports a pause-count
difference instead of a lost command.  For a row that is not SKIP-wired the
tool classifies each blank as a pause answer or an empty turn *from scarier's
pauses*, so a game where run400 pauses where scarier does not turns the
surplus blanks into turns on one side only.  `mould` is the first row where
that shows: 10 blanks, all 10 empty turns in run400, only 3 in scarier.

### `mould` is NOT COMPARABLE: the imp fight redraws its form every round

A clean feed is not a comparable run.  `Adrift_376_mould.txt` echoes all 313
lines of `cmdfile_s_mould.txt` in order, and still never prints
`Congratulations on winning The Potter and the Mould`: it ends inside the
Act-1 shapeshifting-imp fight, with **51** `What do you want to turn your hand
into?` menus against `mould_solution.expected.txt`'s **12**.  That is the
game's RNG, not the harness.

`SCR_DUMP_TASKS=1 harness/scare games/mould.taf` shows the fight redrawing the
imp's attack form on every round:

```
TASK 432 [#fight started]      -> exec TASK 433
TASK 433 [#random imp change]  ACT type=3 v1=40 v2=2 v3=0   ; impstate = random(0,4)
                               -> exec TASKS 434..439
TASK 434..438 [#change0..4]    RESTR type=4 v1=42 v2=2 v3=N ; print the drawn attack
TASK 439 [#mold]               "What do you want to turn your hand into?"
```

`ACT type=3` with `v2=2` is `sctasks.cpp` case 2, `scr_randomint(var3, var5)`.
TASKS 440..474 then hard-gate every answer on the value drawn, exactly one
winning digit per form:

| imp draws | winning answer | task |
|---|---|---|
| 0 baseball | `5` bat | T465 |
| 1 bird | `2` shield | T446 |
| 2 crowbar | `1` crowbar | T442 |
| 3 lasso | `4` knife | T463 |
| 4 chain | `3` hook | T459 |

The golden's `1 4 3 2 4` is that table applied to the draws OUR RNG makes
under `SCR_SEED=221` -- crowbar, lasso, chain, bird, lasso.  Round 1 agrees
with run400 by luck (both draw the crowbar).  Round 2 does not: the golden
gets "The imp moves in your direction before turning into a lasso"
(`mould_solution.expected.txt` ~line 2076), run400 gets "The imp flies at you,
turning into a baseball in mid-flight" (`Adrift_376_mould.txt` lines
968-1075), and `4` loses to a baseball.  Every later round is an independent
draw, so a fixed sequence never recovers; the fight never resolves and the
~200 remaining Act-2 commands are all refused with "You don't have time for
anything else, apart from the fight."  ROOM 103 `[THE END]` is unreachable.

A secondary offset compounds it without causing it: scarier consumes the
solution's first `1` (`mould_solution.txt` line 183) at a `(Press a key)`
pause, while `drive.exe`'s `ClearStalePauses` answers that pause itself -- so
run400 gets 6 digits into the fight where scarier feeds 5.  The drawn forms
differ regardless of alignment.

**Consequence for the row.**  `mould` is seed-locked and must not be counted
as a run400 divergence; the row comment in `run_v4_walkthroughs.sh` now says
so.  Comparing it for real needs an ADAPTIVE driver -- parse the announced
form each round and answer from the table above -- not replayed digits.  The
same caution applies to any other row whose walkthrough answers a menu whose
prompt is chosen by `ACT type=3 v2=2`.

### The two that are real

- **`confession` and `thelasthour` never lost anything.**  Both games END
  where the Runner stopped: `confession` prints "Striking a plea deal" -- the
  row's own win string -- at turn 16, and scarier's replay prints it at turn
  16 too and consumes exactly 16 prompts.  The golden's 21 trailing `z` are
  dead lines neither engine reads.  `thelasthour`'s trailing `wait` lines are
  NOT the same story -- 6 are dead in run400 but only 1 in scarier, because
  the driver answered two startup pauses that scarier feeds command lines
  into.  See "`thelasthour`: two pauses, two turns, one dead subplot" below;
  the "119/119" in the table above is not a like-for-like comparison.
- **`hyper_b_s` is the battle divergence, from before the first loss.**  The
  Flare Rat dies at run400's 7th punch, the Hiscore Table form comes up and
  the drive ends at command 18.  It belongs to follow-up 4, not to this one.

### `thelasthour`: two pauses, two turns, one dead subplot

The mirror image of `mould`.  There the driver's pause handling cost us
nothing; here it silently shifted the entire command stream.

`thelasthour` opens with a content-warning screen carrying TWO `Press a key.`
pauses.  The row is not SKIP-wired, so scarier feeds two command lines into
them -- its first echoed prompt is feed line 3.  `drive.exe`'s
`ClearStalePauses` answers both itself, so run400's first echoed command is
feed line 1.  Net effect: **run400 executed two extra `remember` turns and ran
two turns ahead of scarier for the whole game.**  `Adrift_366` echoed
feed[1..119]; our golden ran 3 `remember` + feed[6..].  The streams are not
aligned, and the table's "119/119" compares different runs.

Two turns is exactly the margin that mattered.  The supper EVENT at turn 45 is
what OPENS the spyhole, and `put bowl near spyhole` was feed line 46 -- turn
46 in run400 (open, the put lands) and turn 44 in scarier (shut, so the
command falls through to the 4.0 put prompt "Where do you want to put the
spyhole?", which `Adrift_111` confirms run400 gives in the same state).  From
there the whole optional strand died in our golden and lived in run400:

| command | old golden | `Adrift_366` |
|---|---|---|
| `put bowl near spyhole` | `Where do you want to put the spyhole?` | `I put the bowl near the spyhole.` |
| `eat soup` | `No more soup...` | `I eat the soup. Just few gulps...` |
| `take knife` | `Take what?` | `I take the little knife.` |
| `enlarge hole with knife` | `No way. Need something to enlarge it.` | `That's it. It's larger now.` |
| `x photo` | `I see no such thing.` | the photo description |

The failed examines then produced admin turns, and three `z` lines had been
added to the walkthrough to absorb them -- papering over the symptom.  None of
it was an engine divergence.

**Fixed 2026-09-07** by re-deriving the walkthrough, not the engine: two `z`
now precede `put bowl near spyhole`, the three stale `z` compensators are
gone, and the row has no admin turns left at all.  Every one of those five
steps now matches run400 line for line.  The feed is 122 lines (121 is the
exact minimum, 119 read as prompts + 2 eaten by the pauses, 1 spare).  A
side effect: `ask sly about interrogation` now lands after Sly has spoken and
answers "Did Mr.Frey ask you something?"; the ask/talk-to `about` split is
still exercised by `ask sly about him`.

**The transferable rule.**  A row whose game pauses before the first prompt is
driven by two different command streams unless the feed is adjusted: scarier
consumes feed lines at pauses, `drive.exe` does not.  Before comparing such a
row, count the game's startup pauses and drop that many leading lines from the
cmdfile -- or the two runs will be silently offset from turn one, and every
downstream difference will look like an engine bug.  Sibling trap to
"a golden's answers to SCARIER's own [Y/N] are not commands" above.

Feeds for these rows are now `cmdfile_r_<solution>.txt` (all 19, regenerated
2026-09-07) and `cmdfile_s_mould.txt` (hint-free).  Job files
`jobs_fu8.txt` / `jobs_fu8b.txt`.


## Ported 2026-09-07: the character resolver's case-sensitive tail

`bandera` T18 was the last open row of the "seen model" family, and it was
never the seen model.  On turn 18 of the Bandera walkthrough `x marife`
answers `No ves tal cosa.` in run400 (`Adrift_232_bandera.txt`, the game's
ALR for `You see no such thing.`) while scarier printed Marifé's
description.  The character seen byte is not involved at all: run400 stamps
`char(26) = 1` unconditionally at 47F2EC for every character in the player's
room, every line.

**The rule, in four steps.**

1. **The typed line is lower-cased before anything parses it.**  run400's
   `Text1_KeyPress` echoes the raw command -- `"> " & cmd` through
   `Proc_21_19_47B568` at loc_45C5C3 -- and only THEN assigns
   `cmd = LCase(cmd)` (loc_45C5D1..45C5E5), pushing the lower-cased copy
   into the command history array `MemVar_49415C` as well.  All four
   Runners do it, ungated: run390 loc_436235..436249, run380 loc_426FA9,
   run370 loc_422091.  Every later splice into the command line re-LCases
   the whole thing the same way -- the pronoun substitutions
   (`Proc_19_49_461F38` loc_461ACB, loc_461BA5, ...), the alias->Short
   rewrite (`Proc_19_48_44EE50` loc_44ED91, loc_44EE27), the give and
   ask/talk reference rewrites (which splice `LCase(Name)`).
2. **The game's own SYNONYM table is the one rewrite that does not.**  It
   runs after the LCase and splices the author's replacement text verbatim.
   Bandera's table has five Marife entries and every one of them replaces
   with the capitalised `Marifé`, so whatever the player types the live
   command line is `x Marifé`.
3. **The character resolver ends with a case-SENSITIVE InStr.**
   `Proc_21_40_45E99C` picks the Name -- or, failing that, the LAST matching
   Alias, the loop at loc_45E623..45E67D assigning without breaking while a
   matching Name jumps it at loc_45E620 -- using the case-INSENSITIVE
   whole-word test `Proc_21_38_454CB0`, lower-cases the winner into `var_98`
   at loc_45E6A6..45E6B2, and returns

       InStr(1, cmd, var_98, 0)        ' loc_45E743, loc_45E8B3, loc_45E938

   with compare mode 0 = `vbBinaryCompare`.  `marifé` is not in
   `x Marifé`, so it returns 0.
4. **So the examine never reaches her.**  It falls through to the 4.0
   "see no such thing" refusal at 4801E1, which the game's ALR renders as
   `No ves tal cosa.`

Normally step 3 can never fail -- step 1 has already made the whole line
lower case, so a name that matched case-insensitively matches
case-sensitively too.  A SYNONYM replacement carrying a capital is the only
way to break it, and when it does, that character becomes permanently
unreferenceable by ANY library command.  Only the author's own tasks reach
Marifé, because task matching is case-insensitive: `hablar con marife` and
`besar a marife` both work in the same transcript.

**Measured, both directions.**  `Adrift_900_bandcase.txt`: `x cabo`,
`x Cabo` and `x CABO` all examine the corporal (there is no synonym for
him), proving the resolver is case-tolerant about what the *player* types;
`x marife` and `x Marife` are both refused.  `Adrift_901_bandlc.txt`: the
same .taf repacked with `taftool.py`, with the five synonym replacements
lower-cased to `marifé` and nothing else changed, answers `x marife` with
`Una excelente camarera y muy atractiva...`.  The capital is the whole
cause.

**Ported.**  Three changes, all in the shared v4 engine:

- `run_player_input()` (`scrunner.cpp`) lower-cases the line straight out of
  `if_read_line()`, before `pf_filter_input()` runs the synonyms.  The `> `
  echo is Glk's and is unaffected, as run400's is.
- `uip_replace_pronouns()` (`scparser.cpp`) lower-cases the whole buffer
  after each splice, as the Runner does.  Without it the new gate would
  refuse the character the pronoun had just named: `wrecked`'s
  `ask him about pens` becomes `ask harold about pens`, not `ask Harold
  about pens`.
- `uip_case_folds_name()` / `uip_case_folds_name_in()` (`scparser.cpp`) are
  the InStr tail.  `uip_match_entity()` applies it to every `%character%`
  bound by a LIBRARY pattern (`!uip_strict_reference`; task commands go
  through a different Runner routine and are exempt), and `uip_npc_named()`
  -- the last-named-character register and the give rewrite, both of which
  really are 45E99C in run400 (call sites 47F395 and 48A9AC) -- applies it
  to whichever of Name/alias 45E99C would have chosen.  Objects are NOT
  affected: `co()` (`Proc_21_39_46486C`) has no such tail.

**Cost.**  One golden line re-blessed (`bandera` T18).  Suite 428/428;
`compare_wine_transcript.py` against `Adrift_232_bandera.txt` now reports
`endtail 1` -- the final `[Pulsa cualquier tecla para terminar]` prompt --
where it reported `diff 2`.  The map corpus (1212 views) and
`scproj_regress.sh` are byte-identical before and after.

## Ported 2026-09-07: a backwards random range still draws

`scr_randomint(low, high)` used to open with

```c
/* If the range is invalid, just return the low value given.  This mimics
   Adrift under the same conditions. */
if (high < low)
  return low;
```

which is wrong on both counts: it does not mimic Adrift, and it returns
without drawing.  Both author-facing callers -- the "change variable to/by a
random value" task action (`task_run_change_variable_action`, Var2 = 2 and 3)
and the `rand(x,y)` expression function -- are the same VB idiom in the
Runner,

```
CLng(Var3 + Int(Rnd * ((Var5 - Var3) + 1)))
```

(`mdlSpreadTheLoad.bas` `loc_48D1E0` / `loc_48D261` inside `execute_action`
@48E860; `Express.bas` `loc_485C44` for the expression).  Nothing there tests
the order of the bounds.  If the author entered the range backwards the span
simply goes negative, `Rnd` is still drawn, and VB's `Int()` floors *towards
minus infinity*, so:

| authored | span | `Int(Rnd * span)` | result | old scarier |
| --- | --- | --- | --- | --- |
| `rand(-3,-10)` | -6 | -6..-1 | **-9..-4** | flat -3, no draw |
| `rand(-3,-15)` | -11 | -11..-1 | **-14..-4** | flat -3, no draw |
| `rand(-1,-3)` | -1 | -1 (0 only if `Rnd` is exactly 0) | **-2** | flat -1, no draw |
| `rand(-1,-2)` | 0 | 0 | **-1** | -1, no draw |

The last row matters: a zero span still consumes a draw even though its value
is unchanged, so games full of `rand(-1,-2)`-shaped actions (`House` has
twenty) re-thread the RNG stream without changing a single number.

`floor(-x) == -ceil(x)`, so the fix is the same multiply-shift rounded the
other way; positive spans are bit-for-bit what they were.

### The measurement

`hyper_b_s.taf` is the ideal witness -- its entire scripted battle is two
backwards ranges, `FLARERATHP += rand(-3,-10)` and `HP += rand(-3,-15)`, five
firings each, and nothing else in the game draws.  run400 takes 8, 5, 9, 5, 7
off the rat and 13, 8, 8, 10, 8 off the player: every one of them inside
4..9 and 4..14, none of them the flat 3 the old build produced -- which is
why the rat sat there being punched forever and the row never finished.  With
the fix, scarier reproduces run400's fight **exactly, over ten consecutive
draws**: rat 30 -> 22 -> 17 -> 8 -> 3 -> -4, player 100 -> 87 -> 79 -> 71 ->
61 -> 53, "The Flare Rat is dead! Mission complete!" on the fifth punch.

`3 minutes1.0.taf` is the second win and it closes the separate T8 lead in
"The sharpest new leads" (a counter reading 46 against run400's 53): its
countdown is `timer += rand(-1,-3)` fired 24 times, running at half speed
under the old build.  That row is now **run400-identical on every turn**,
the `<centre>` transcript artefact aside.  (Replay it with this row's env,
`SCR_SEED=8 SCR_SKIP_WAITKEY=1`; `compare_wine_transcript.py` does not apply
the row's env for you, and without it the diff is nonsense.)

### Corpus exposure, and the two rows that only re-phased

Exactly 8 of the 426 games use a backwards range: `3 minutes1.0`,
`British.Fox.and.the.Celebrity.Abductions`, `House` (20x, all zero-span),
`The Dead Man`, `The Plague - Redux`, `TheDemonHunter`, `wumpusRun` and
`hyper_b_s`.  Four goldens moved; all four still win, and the suite is back
to green.

`The Dead Man` and `TheDemonHunter` are RNG rows and their turn counts should
not be read as regressions.  `The Dead Man` fires `tic += rand(-1,-3)` *once*,
but the extra draw re-phases every random event length after it, so its
blackout visions land on different `z` turns than run400's: the raw diverging
turn count went 25 -> 38 while the count of genuinely differing turn *bodies*
only went 17 -> 19.  It fails the double-seed screen outright (seeds 97, 8 and
424242 give three transcripts 56-63 diff lines apart).  `TheDemonHunter`'s
`hajar health`/`player health` rolls went the other way, 18 -> 17 differing
bodies, and what is left is the fight picking different battle messages --
a live roll on both sides.

**Reading rule for this file:** when a fix adds or removes an RNG *draw*,
compare the number of turns whose bodies actually differ, not the number of
lines `compare_wine_transcript.py` prints.  Its `streams re-synchronised`
lines are alignment bookkeeping, and a re-phased stream generates them in
bulk without a single new engine difference.


## Ported 2026-09-07: `stats` is not a Runner command

`suburbanprodigy3` T31 was the last of the one-line 4.00 diffs, and the guess
in "The sharpest new leads" was right: the standard-command row

```c
{"[status/stats]", lib_cmd_status_player},
```

carried a `stats` synonym that no ADRIFT Runner has ever had.  The game's own
task for `stats` therefore never got the line -- scarier answered it with
`lib_cmd_statusline()` (`Celler | Score: 80`) before the task could claim it.
Dropping the synonym to `{"[status]", ...}` makes the turn run400-identical:

```
turn 31  stats
  run400   Listen dude, you've played these games before. Step it up! You scored 80
           out of the maximum 80! That is 100% of the game! Well done - you scored
           maximum points! [Press any key to end]
  scarier  (the same, less the [Press any key to end] tail)
```

One golden re-blessed (`suburbanprodigy3_solution.expected.txt`); the rest of
the v4 walkthrough suite is unchanged and green.  `life_solution` also types
`stats` and was never affected -- its game task already claimed the line
ahead of the library table.

### The census: what the four Runners actually have

Both the VB6 constant pools of `run{370,380,390,400}.exe` and the four
decompiled listings agree, and neither contains the string `stats` at all:

| literal | 3.70 | 3.80 | 3.90 | 4.00 |
| --- | --- | --- | --- | --- |
| `"stats"` | -- | -- | -- | -- |
| `"status"` | -- | -- | yes | yes |
| `"statusline"` | -- | -- | -- | -- |

(Method: [[adrift-runner-string-census]] -- scan for `(?:[\x20-\x7e]\x00){3,}`
and keep a run whose preceding uint32 LE equals its byte length.  `LC_ALL=C`
and `grep -a` on the listings, per [[adrift-decompile-index]].)

So the answer to "do they *all* have a `status` command?" is **no**.  3.70 and
3.80 have no `status` at all -- consistent with the Battle System being 3.90+
([[adrift39-battle-attribute-indices]], [[scare-battle-system-port]]).

And in the two that do, `status` is not a general command.  Its only two uses
are:

- **`dobattle`** -- run400 `Proc_11_4_47F084` @47DCA1, run390 `dobattle`
  @44C510.  `c("status", cmdline)` (the whole-word `InStr` matcher
  `Proc_21_38_454CB0`), plus " can't get the status of a character you've not
  seen yet!" for the `status <character>` form.
- **`Text1_KeyDown`** -- run400 @4840A1, run390 @4537AB.  This is the input
  box's **Auto complete** word list (`checkb("stand"...)`, `checkb("status"...)`,
  `checkb("stop"...)`, `checkb("take"...)`, `checkb("talk"...)` in a row), not
  the parser; see [[scare-g-means-get]].

`dobattle` is reached from `generaltasks` @48C0F0 behind

```
loc_48A496: push MemVar_494282
loc_48A49E: push (from_stack_2 = from_stack_1)
loc_48A49F: If from_stack_1 Then
loc_48A4A2:   Proc_11_4_47F084()      ' dobattle
```

i.e. **only when the game's Battle System flag is on**.  Scarier already gates
`lib_cmd_status_player`/`lib_cmd_status_npc` on `battle_is_enabled()`.

### Still not measured: the battle-disabled fallback

With the Battle System off, scarier's `status` (and its `statusline`, which is
in no Runner either) still falls through to `lib_cmd_statusline()` and prints
the status line, where run400 would let a game task have the line and
otherwise say it does not understand.  Both are inherited SCARE inventions,
not measured behaviour.  Corpus exposure is nil: only four goldens type
either word.  `life`, `the_town_of_azra` and `suburbanprodigy3` type `stats`
and in all three a game task claims the line; `the_town_of_azra`'s 3.90
golden is the one that types bare `status`, and that game HAS a Battle System,
so it takes the real `lib_cmd_status_player()` path and prints the
Stamina/Hit strength/Accuracy/Defense/Agility table -- exactly the 3.90
`dobattle` behaviour the census predicts.  Nothing in the corpus reaches the
battle-disabled fallback, so the row is left alone until a Runner transcript
forces it.

## Audited 2026-09-07: the rest of the meta-command table

`stats` (above) was found by a Runner transcript, not by inspection, so the
whole of `STANDARD_COMMANDS` (`scrunner.cpp` ~600-673) was then run through
the same two checks:

1. **String census.**  Does the literal occur in the VB6 constant pool of
   `run370.exe` / `run380.exe` / `run390.exe` / `run400.exe`?  (Method:
   [[adrift-runner-string-census]].)  A hit was only accepted after locating
   the *use* -- several near-misses are menu captions, registry keys or game
   settings rather than parser literals.
2. **Corpus exposure.**  Does any of the 426 v4 test games define a task whose
   `cmd`/`ALTCMD` contains the word, is typeable (no `#`/`!` prefix) and is
   reachable (`where != 0`; `ROOMLIST_NO_ROOMS` tasks cannot be reached from
   typed input at all)?  And is that task *silent* (no `COMPLETE` text), which
   is the only shape that lets the library steal the line -- see
   [[adrift4-one-task-per-line]].

### Scarier-only inventions (literal in NO Runner binary)

games/tasks/reachable/silent counted over the 426-game v4 corpus:

| word | games | tasks | reachable | silent | bites today? |
| --- | ---: | ---: | ---: | ---: | --- |
| `stats` | 5 | 6 | 6 | 2 | **yes -- removed, see above** |
| `hints` | 11 | 224 | 224 | 0 | no |
| `q` | 4 | 5 | 4 | 1 | no (verified live) |
| `brief` | 3 | 5 | 5 | 0 | no |
| `notification` | 1 | 4 | 4 | 0 | no |
| `verbose` | 3 | 3 | 3 | 0 | no |
| `notify` | 2 | 3 | 3 | 0 | no |
| `redo` | 1 | 1 | 1 | 0 | no |
| `hist` | 0 | 0 | 0 | 0 | no |
| `gpl` | 0 | 0 | 0 | 0 | no |
| `license` | 0 | 0 | 0 | 0 | no |
| `statusline` | 0 | 0 | 0 | 0 | no |

The Runner words these are *nearly* spelled like, and which do exist, are
`hint` (not `hints`), `quit` (not `q`), `history` (not `hist`), `Verbose` (a
Runner menu caption + registry key, not a typed command; see
[[run400-verbose-toggle]]), `NotifyScore` (a 4.0 game setting; see
[[adrift4-endgame-score-summary]]).  A naive case-insensitive census reports
all five as present -- always find the use site.

Why none of them bites: the game's tasks are matched *before*
`run_standard_commands()`, so a task with `COMPLETE` text simply wins and the
invention never runs.  Only a silent task leaves a gap for the library to fill,
which is exactly what happened to `suburbanprodigy3` T31.  The four non-`stats`
silent/edge candidates were each checked live:

- `Blood_Relatives.taf` T445 `cmd=[q]` is `where=0` -- unreachable from typed
  input in either engine.
- `forum2.taf` T43 `cmd=[quit]` / `ALTCMD[1]=[q]` is a deliberate quit-silencer
  (no actions, no text).  It wins in scarier and prints nothing; the quit
  prompt does *not* leak in behind it.
- `whitterscap.taf` `cmd=[*q*]` wins in scarier ("...your keyboard lacks a Q
  key.").
- `MikeDesert_SuburbanProdigy3.taf` T45 `cmd=[Undoo voodoo]` /
  `ALTCMD[1]=[undo]` / `ALTCMD[2]=[redo]` -- same game, same room 7 as the
  `stats` task, but this one has `COMPLETE` text, so the task wins in both
  engines.

`hints` has by far the widest footprint (224 tasks across 11 games, e.g.
`Showtime_at_the_Gallows.taf` `[hints/hint] {BASEMENT}`), but every one of them
carries `COMPLETE` text.  It is the row most likely to bite the moment a game
ships a silent `hints` task.

### Real Runner commands that scarier does not version-gate

Present in the Runner, but not in all four:

| word | first Runner | scarier gate |
| --- | --- | --- |
| `turns` | 3.80 | none |
| `undo` | 3.80 | none |
| `version` | 3.90 | none |
| `status`, `wield` | 3.90 | `battle_is_enabled()` (a 3.7/3.8 game cannot have one, so effectively gated) |
| `z`, `g` | 3.90 | already gated, see [[adrift-z-wait-vocabulary-390]] |

Corpus exposure is nil: the 19 games at 3.70/3.80 define no task naming
`turns`, `undo` or `version`.  Left alone until a transcript forces it.

### Reverse gap: Runner meta-commands scarier lacked (PORTED 2026-09-07)

From `generaltasks` (run400 @489FD4-48C0EC; the far more readable 3.7 listing
is run370.bas @43B4xx-43C3C6), four rows were missing and have now been added:

| command | Runner | scarier |
| --- | --- | --- |
| `past` | synonym of `history`, one whole-line test (run370 loc_43BA2A, run380 loc_44228D, run400 loc_48A51A) | added to the `[hist/history]` rows |
| `bye`, `end` | synonyms of `quit`, one three-way whole-line test that unloads the form (run370 loc_43C06B, run400 loc_48AA85) | added to the `[quit]` row |
| `endgame` | ends the game inline with the score summary (run370 loc_43C095, run400 loc_48AAC9) | `lib_cmd_endgame()` |
| `control panel`, `control-panel`, `control`, `panel` | opens the Runner's Control Panel window: "Control Panel on", or "Control Panel already on." (run370 loc_43C34E, run400 loc_48AEAE) | `lib_cmd_control_panel()` -- no such window here, so it says so |

Every literal is in all four constant pools, so none of them is version-gated.
Suite after: 428/428 PASS.

`endgame` is **not** a `quit` synonym, and it is not the ending machinery
either.  The Runner writes the two summary lines inline and only then sets the
gameover byte, so `Form1.endmessage` never runs: no WinText, no "Better luck
next time.", no "Well done - you scored maximum points!" / "You finished N
points short.".  It also handles a scoreless game differently from
`task_print_end_game_summary()` -- the 0 -> 1 fix-up at loc_48AB3D applies only
to the divisor, after the "out of the maximum" figure has already been
composed, so a MaxScore of 0 prints "out of the maximum 0!" and "That is 0% of
the game!" where the ending path skips the summary entirely (4.0) or reports
100% (pre-4.0).  The two printers are therefore kept apart on purpose.

Two corrections to the first pass of this audit:

- The `ls` / `cp` / `mv` / `ln` / `dir` easter egg was **already** in scarier
  (`{"[cp/mv/ln/ls] *", lib_cmd_unix_like}` and `{"dir *", lib_cmd_dos_like}`).
- `endgame` was listed as a `quit` synonym.  It is a separate branch, as above.

### Not ported: `both`

3.90 and 4.00 only (absent from the 3.7/3.8 pools).  It is a dead branch:

```
run400 loc_48AE94:  If cmd = "both" Then cmd = <saved>.field0: GoTo loc_489FEB
run400 loc_48BB97:  <saved>.field0 = MemVar_4941F0   ' end of every turn
```

and `MemVar_4941F0` is the **disambiguation candidate list** -- the string the
`Which <term>.  <list>?` prompt is built from (run400 loc_4733E1, loc_46E220).
3.90 builds the same variable explicitly (`MemVar_468194`, run390_3.bas
loc_43B4D5-43B54A): the object names joined with `", "` and `" or "`, then a
trailing `"?"`.  So `both` re-feeds a string like `the red ball or the blue
ball?` to the parser as if the player had typed it, which cannot resolve to
anything.  Porting that would be porting a bug with no observable useful
behaviour, so it is left out until someone measures what a live Runner
actually prints.

### Ported 2026-09-07: the `quit` decline text

All four Runners answer "I'm so glad you said no..." when the quit
confirmation is declined.  The branch is two statements, and the second runs
whatever the first did:

```
run370 loc_43C07E:  Me.Global.Unload MemVar_4461A8
run370 loc_43C089:  MemVar_4460E4 = "I'm so glad you said no..."
```

(run400 loc_48AABA/48AAC2 is the same pair.)  VB's `Unload` raises
`Form_QueryUnload`, which is where the Runner puts its "Are you sure?" box:
confirm and the process is gone before the assignment can matter, decline and
`Unload` simply returns, leaving the line as the turn's whole output.  It is an
assignment, not an append, so it replaces anything the turn had buffered --
moot here, since a matched task would have taken the line before
`run_standard_commands()` ran.  The literal is in all four pools, so it is not
version-gated.  `lib_cmd_quit()` now prints it on the decline path; suite
428/428 PASS.

### Gated 2026-09-07: the eleven inventions behind `SCARIER_NO_ABBREVIATIONS`

`SCARIER_NO_ABBREVIATIONS` already dropped the `g`/`i`/`z`/`x`-family
shorthands that no Runner has.  The eleven words the audit above found are the
same kind of thing -- library rows that can eat a line the Runner would have
handed to a task -- so they now compile out under the same macro:

| word | how it is gated |
| --- | --- |
| `redo` | row split; the Runner-less `redo`/`redo N`/`redo TEXT` forms go, `!`, `!5`, `!take` stay |
| `hist` | dropped from `[hist/history/past]` (both the bare and `%number%` rows) |
| `hints` | `[hint/hints]` becomes plain `hint` |
| `brief`, `verbose` | rows removed |
| `notify`, `notification` | both rows removed (bare and `%text%`) |
| `gpl`, `license` | `#ifndef` around `[gpl/license]` |
| `statusline` | `#ifndef` around the row |
| `q` | dropped from `[quit/q/bye/end]` (already gated before this change) |

Nothing else moves: `history`, `past`, `hint`, `!`, `!5`, `!take`, `quit`,
`bye`, `end`, `again`/`last`/`previous`, `inventory`, `endgame` and the control
panel rows are all real Runner vocabulary and stay in both builds.

Nothing in the tree defines the macro and `harness/build.sh` does not pass it,
so the default build is unchanged (suite 428/428 PASS).  A hand-built
`-DSCARIER_NO_ABBREVIATIONS` binary was checked against
`MikeDesert_SuburbanProdigy3.taf`, whose catch-all prints "Listen dude, you've
played these games before.  Step it up!" for anything the game does not know:
all twelve dropped words (the eleven plus `stats`) reach that catch-all, while
every row in the paragraph above still answers from the library.

## Ported 2026-09-07: `icecream`'s two rules -- the take that a task claims, and the put whose direct object names nothing

`icecream` was the last of the "sharpest new leads" with two diffs in it, and
they turned out to be two unrelated 4.00 rules.  Both are now ported, and the
game's three-diff row is closed.

### T0 -- `take cone`: a held object still gets its task look-up

```
turn 0  take cone
  run400    You already have an empty cone.        (two leading spaces)
  scarier   You are already carrying the cone.
```

The lead read this as "the Runner used the object's alternate description".
It is not a description at all.  `IceCream.taf` task 14 is `[take/get] cone`
with one restriction -- the player must NOT be holding the cone -- and
FailMessage `  You already have an empty cone.`  run400 runs that task; scarier
never offered it the line.

run400's take piece is `Proc_19_39_46302C`.  Before it refuses anything it
builds `"get " & name(obj, 0)` (the definite form, plus a ` from <holder>`
clause when the object is inside another) at @462AED/@462B84, pre-matches it in
the take-family class, and dispatches it at @462C5C with

```
loc_462C5C: Proc_19_24_44CCE0(1, 1)     ' run tasks, restriction-failure pass ON
```

A claim exits the piece.  Only if nothing claims does it reach the
" can't take " test at 462CA0 and the " already carrying " one at 462D01.
Scarier had exactly this retry, but only ahead of `can't take` -- so a task
restricted on *already holding* the object could never win.  Moving it to the
top of `lib_take_backend_common()` (before the held-object list is spoken for)
makes both refusals share the one look-up, as 46302C does.

Note the second `1`: the restriction-failure pass.  That is why a task whose
restriction FAILS still prints its FailMessage and still counts as claimed --
the same argument pair `lib_try_game_command_take_definite()` already passes
for 4.0's implicit take ([[adrift4-put-family-precedence]]).

### T2 -- `put ice cream in cone`: the clobbered command line

```
turn 2  put ice cream in cone
  run400    I don't understand what you want to do with the cone.
  scarier   (ran task 15 and won the turn)
```

This one is a genuine Runner wart and it reaches well past `icecream`.

`generaltasks` hands every line holding the whole word `put` or `drop` to
**put_drop_list `Proc_19_40_459DB4`** at @48A462 -- *before* the task dispatch
at @48A481.  put_drop_list normalises the line with four plain VB `Replace()`
calls (459B3D-459BAC; substring, not word, and `"inside"` is replaced before
`"into"` so the shorter pattern cannot reach it):

```
"drop "  -> "put "
"inside" -> "in"
"into"   -> "in"
"onto"   -> "on"
```

then splits at the first `" in "` (459BCD) and calls **name_object
`Proc_19_41_46E5D8`** to name the direct object.  name_object *installs* the
fragment `Left(line, split)` -- everything up to and including the space before
`in`, i.e. `"put ice cream "` -- as the global command line MemVar_494174
(46DE99), and resolves it with the noun scorer `Proc_21_58_463640` in **mode 2**
(every object PRESENT, seen or not).

Every exit of name_object puts the line back at 46E5CD -- except one.  When the
fragment names nothing at all (`&HFF`), 46E142 tests whether a put/drop-class
task pre-matches the *typed* line (`Proc_19_35_453C50`, arg_14 = 2); on a hit it
prints nothing and returns at **46E23B with the fragment still installed**.
The task passes then run against `"put ice cream "`, match nothing, and the
catch-all speaks -- with the noun MemVar_4942F8 that `generaltasks` resolved
up front from the ORIGINAL line (48A3F5), which is the cone.

Measured directly, `Adrift_900_icecream2.txt` (run400, 2026-09-07).  The game
has three tasks sharing one pattern,
`[put/place/set]{the/some/all}{of}{the}[ice cream]{in/in the/on/on the}[cone]`,
so every line below is the same task pattern and only the wording differs:

```
> put ice cream in cone
I don't understand what you want to do with the cone.

> put the ice cream in the cone
I don't understand what you want to do with the cone.

> place ice cream in cone
  Holding the cone in one hand, you place the scooped ice cream carefully ...

> put ice cream on cone
  You don't have any ice cream in the scoop.

> set ice cream in cone
  You don't have any ice cream in the scoop.
```

`place` and `set` never enter put_drop_list (no whole-word `put`/`drop`).
`put ... on ...` is saved by the `"on"` branch's own escape at 459C39, which
zeroes the split when the fragment resolves to nothing, so the line reaches the
tasks intact.  **The clobber is the `in` form only.**

The list branches leave before the clobber and are not rebuilt: a whole-word
`all` or `and` in the fragment is answered by the loops at 46E04E and 46E0B2,
and an `" and "` at or beyond the split (`put a in b and put c in d`) sends
put_drop_list round its own loop at 459C75 -- the `InStr` there starts at the
split (459C60), so it only ever sees the second clause.

**Port.**  `lib_put_in_multiple_common()` calls the new
`run_priority_unnamed_put_object()` when its `%text%` parse finds no object,
and `run_all_commands()` then runs the three task passes against
`run_unnamed_put_fragment(line)` instead of the line.

The gate needs care.  `lib_parse_multiple_objects()` failing is *not* the same
test as 463640 answering `&HFF`: the library parse also applies the put filter,
so an object whose name the fragment plainly holds can still fail it.  `hub`'s
`put soup in pan` is the case -- object 42 is Short "minestrone soup" with
aliases "minestrone" and "soup", so the Runner scores it 1 and names it, while
scarier's parse rejects it for sitting inside the can.  Gating on the parse
alone regressed that turn.  `lib_put_fragment_names_nothing()` therefore asks
the ported scorer `lib_verb_object_name_score()` directly, over every object
`obj_indirectly_in_room()` (mode 2 -- no seen gate), and only an all-zero sweep
clobbers.

### Corpus fallout: three goldens, two of them real

Full v4 suite green afterwards.  Three transcripts moved, and only `icecream`
needed its walkthrough re-derived:

| game | line | why it clobbers now |
| --- | --- | --- |
| `icecream` | `put ice cream in cone` | no object's name is in `"put ice cream "` -- all five present objects have Prefix "the" and aliases "ice cream scoop"/"ice cream cone" |
| `house` | `put web in kettle` | object 149 is Short "cobweb", no aliases, Prefix "some" -- score 0 |
| `iachini` | `put sheet in dryer` | object 26 is Short "sheets", alias "box", Prefix "a box of dryer" -- score 0 |

`house`'s line was already a no-op (the kettle has no boiling water yet) and is
kept as coverage; its golden just moved to the catch-all wording.  `iachini`'s
was load-bearing, and the author had foreseen it: task T18 carries
`place * sheet * dryer` as its second command, so the walkthrough now types
`place sheet in dryer` and wins 115/115 again.  `icecream`'s walkthrough moved
from `put` to `place` for the same reason.

`hub` is the control that keeps the gate honest, and `house`'s own
`put thyme in kettle` one line earlier is a second one -- "thyme" IS the
object's Short, so it names and never clobbers.

## Ported 2026-09-07: the ALR list is walked once

Chasing item 7's `qui_a_tue_dana` diff -- run400 answers `in` with "Vous vous
deplacez in." where we printed the game's own fix-up "Vous entrez." -- turned
out to be the whole ALR model rather than one game's quirk.

### What the Runner does

`Proc_21_20_44C7DC` in run400, read straight:

```
Function ALRs(text, noalr)
  text = substitute_percent_tags(text)         ' 47A3DC, called at 44C6EE
  If dont_convert_ALRs Then Return text
  If noalr <> 1 Then
    For i = 0 To ALRCount - 1                  ' length-descending order
      If InStr(1, text, ALR(i).Original, 0) > 0 Then
        If text = ALR(i).Replacement Then Return text          ' 44C75E
        expansion = ALRs(ALR(i).Replacement, 0)                ' 44C76F
        text = Replace(text, ALR(i).Original, expansion, 1, -1, 0)
      End If
    Next
  End If
  Return text
```

run390's twin is the plain loop at `loc_45BD43`, the tail of its output filter
`Proc_2_28_45CBD0` (run390_3.bas:55465): the same single ordered pass, with no
recursion and no equality test.

So neither version repeats the pass.  **4.0's extra work is depth, not
breadth** -- it filters each replacement before splicing it in, and it
substitutes the `%tags%` at the top of *every* recursive call.  That reading is
what the 2026-08-24 probes (`harness/make_400_alrprobe.py`,
`harness/make_39_alrprobe.py`) had already measured cell by cell; it had been
implemented as "3.9 walks once, 4.0 walks until stable", which agrees with the
probe but not with real games.

Two consequences, and both of them are visible in shipped corpus games:

1. **A fix-up ALR longer than the pair that produces it can never fire.**  The
   sort has already walked past it by the time the text contains its original,
   and the walk never comes back.
2. **What `Replace`-all leaves behind in the same string is not re-examined.**

### Four measurements, all run400 under Wine, 2026-09-07

| game | line | why |
| --- | --- | --- |
| `qui_a_tue_dana` | "Vous vous deplacez in." (Adrift_369:210) | `[You move]` (8) makes the original of `[Vous vous deplacez in.]` (22) |
| `barneysproblem` | "The TV itself is looks every bit as battered as you remember." (Adrift_302:57, :94) | 45-character pair makes the original of a 61-character fix-up |
| `threeminutes` | "terrace . . and it all went dark.", "you've got . ." | `[. .] -> [.]` turns the authored ". . ." into ". .", and the one pass leaves it |
| `House` | `evaluate error - Out of stack space`, nothing printed for the move | `[You move] -> [%drunk%]` with the variable `drunk` = "You move" -- proof the `%tag%` substitution really is inside the recursion |

`threeminutes` needed a detour to measure: both dot sites are in the one
Introduction block, which ends `<waitkey><cls>` and so never reaches a
transcript.  The `.taf` was repacked with `harness/taftool.py` minus that tail
(`pfx/drive_c/adrift/p_3min_nocls.taf`) and driven through `fast.sh` with an
**empty command file** under `DUMP_SCROLLBACK`, which dumps the window straight
after load.  That trick works for any pre-`<cls>` intro.

A fifth game, `iqsfot`, carries the same shape and confirms it in the other
direction: of its 405 pairs exactly two are fix-ups for a pair's own output,
`[Irvine picks ups] -> [Irvine picks up]` and `[Irvine seats himselfs] ->
[Irvine seats himself]`, and both are longer than their makers (`[Irvine take]`
11, `[Irvine sit]` 10).  The take fix-up shows no damage because nothing on the
route prints the string "Irvine takes" -- the library message is the bare
"Irvine take the ..." that 2026-09-05's de-conjugation established -- while the
ending's authored prose does say "Irvine sits", so that one lands.

### The port

`pf_replace_alrs()` in `scprintf.cpp` now drives a new recursive
`pf_alr_walk()`, depth-capped at 32 (a mutually-rewriting pair would recurse
for ever, as House shows; no corpus game has one).  The retirement flags SCARE
kept -- `alr_applied[]`, one bool per ALR -- are gone: 4.0 holds a
self-containing ALR to one expansion per walk with the whole-text equality test
at 44C75E, and 3.9 does not hold it at all.  `alr_single_pass` (< 4.00) now
selects "splice the replacement verbatim" instead of "run one pass".

Five goldens re-blessed, all of them games whose authors had written a fix-up
that the real Runner never applies: `qui_a_tue_dana`, `barneysproblem`,
`threeminutes`, `iqsfot` and `house`.  House's deliberate deviation changed
value in the process -- our depth-capped walk bottoms out and the filter's next
pass interpolates what it wrote, so we now print "You move east.", the line the
author meant a sober player to see, instead of the literal `%drunk% east.`.

Full v4 suite: 428 PASS / 0 FAIL.

## Ported 2026-09-07: an ending takes the unhandled-verb tail off the line

Item 7, "the refusal that accompanies a task", closed.  Three rows were filed
under it and they turned out to be three different things:

- `qui_a_tue_dana` T21 -- the ALR list, fixed by the previous section.
- `cbn2` T17 -- not a second message at all.  `Adrift_138_cbn2.txt:101-105`
  shows the task's answer, then a *separate* `> ` prompt, and the game's
  DontUnderstand ("...that was a strange command...") answering the empty
  line the feed left behind.  A harness artefact; scarier is right.
- `relojero` T10 -- the real rule, and `easter` turned out to be the same
  one.

### The rule

run400's `generaltasks` tests the gameover byte immediately after the turn
counter:

```
loc_48AC62: If MemVar_4941AD <> 0 Then GoTo loc_48B4E3
```

and `48B4E3` is the **tail** of the routine.  So once a task has ended the
game, the rest of the line is gone: the object-counting loop at `48AFF0`, the
unhandled-verb catch-all at `48B19A` ("I don't understand what you want me to
do with", "must be in the same room as", "What X?") and the question words at
`48B31F`.  Everything *ahead* of `48AC62` still runs -- wear `48A48C`, remove
`48A491`, hint/help, look, the give rewrite `48A98A`, open/close `48A515`,
the movement refusal `48A5D8`, the wait loop `48ABDA` -- so an ending does not
silence the library as such; it is only the unhandled-verb tail that is lost.

What the tail then does is call `characters()` at `48B56E`, and that has the
same test again, because it sits below the jump:

```
loc_4805CD: If MemVar_4941AD > 0 Then Exit Sub
```

right above its own catch-all "I don't understand what you want to do with "
at `480603`.  With both catch-alls gone the buffer is still empty, and the
last line of the routine is the two-part test

```
loc_48B573: If MemVar_4941B0 = "" And var_29C = 0 Then MemVar_4941B0 = MemVar_4941A8
```

-- `var_29C` being set by the walk over the characters at `48B53C..48B569`,
`MemVar_4941A8` the game's DontUnderstand text.  Outside an ending that
second condition never shows itself, because a line naming a present
character that nothing else answered gets `characters()`' catch-all and so
never arrives with an empty buffer.

### Three measurements

| game | line | run400 |
| --- | --- | --- |
| `relojero` | `arreglar fenix` (the win) | "Disculpa pero no te entiendo." -- the game's DontUnderstand (plain line 25), *not* its ALR'd object catch-all "Extraños pensamientos afloran en mi mente a proposito de Fenix de laton."  `Adrift_909.txt` |
| `easter` | `show basket to shopkeeper` (the win) | **nothing at all** before the WinText; the line names the shopkeeper, so `var_29C = 1` and even the DontUnderstand stays quiet.  `Adrift_273_easter.txt:304-308` |
| `seaside` | `do form` | the object catch-all, "You must be in the same room as the leisure access card form to be able to do anything with it."  `Adrift_236_seaside.txt:123` |

`seaside` is the control: its TASK3 is silent and its actions do run (it
hides two objects, moves the completed form to the player and scores), and
run400 still prints the catch-all.  So it is the **ending** that does this,
never a silent task on its own -- which is what the row was mis-filed under.

### The port

- `lib_cmd_verb_object()` and `lib_cmd_verb_npc()` in `sclibrar.cpp` return
  FALSE when `lib_is_version_400 (game) && game->pending_endgame != 0`.
  `pending_endgame` is set by `task_run_end_game_action()` and consumed by
  `task_print_end_game_message()` at end of turn, so during the library it is
  exactly `MemVar_4941AD <> 0`.
- `run_process_input_line()` in `scrunner.cpp` prints no DontUnderstand for
  such a line when `uip_line_names_npc()` (new, `scparser.cpp`, the same walk
  `uip_note_named_npcs()` makes) says the line names a character.  Gated on
  the ending so the shape of the test cannot disturb an ordinary line.
- `run_task_refusal()` now returns FALSE outright when any task ran for this
  line.  This is not the ending rule but it surfaced with it: `easter`'s
  winning line drew a *different* task's RepeatText, "Since you already have
  Max's list...", once the catch-all stopped claiming it.  In the Runner the
  RepeatText is not a late pass -- it is printed inside the dispatcher
  `Proc_19_24_44CCE0`, which asks the pre-matcher `Proc_19_66_454EF0` for
  exactly ONE task and then runs it, reverses it, or prints its RepeatText.
  The task that refuses and the task that runs are always the same one, and
  the restriction-failure pass at `44CCA5` is likewise entered only when no
  task was found.

Two goldens re-blessed, `relojero` and `easter`.  Full v4 suite: 428 PASS /
0 FAIL.


### Ported 2026-09-07: a Hidden walk stop stamps the walker's location whether or not it moved

`skydiver` T15 was filed under "a single event one tick out".  It is not an
event, and nothing is a tick out.  run400 prints

```
> z
Time passes...
Pelican A pelican flocked toward me..
```

(`Adrift_246_skydiver.txt:47-49`) and scarier printed only `Time passes...`.
The Pelican's walk stepped into the player's room on exactly the same turn in
both engines -- what was missing was the arrival ANNOUNCEMENT.

### The gate, and the term that was failing

`npc_announce()` fires on an NPC's walk arrival when

```
ShowEnterExit  AND  old <> playerroom  AND  old <> 0
```

(run400 @468A5D, run390 loc_45A99B, run380 @4416F4; run370 @43955E has no
`old <> 0` term at all).  `old` is the walker's previous location field.  The
last term is what keeps a game quiet about characters it has never placed:
an NPC with StartRoom 0 that a task drops straight into your room says
nothing.

But the Runner also uses that same field to record "hidden by a walk", by
writing `&HFF` into it -- so a walker parked by a Hidden stop is at `&HFF`,
not at 0, and its next arrival passes the term.  Scarier keeps that as a
separate `walk_hidden` flag (`gs_set_npc_walk_hidden`), because
`gs_set_npc_location` has to clear it the way the Runner's write clobbers the
field.

The bug: scarier stamped the flag from **inside** the "did the NPC actually
move" branch.  The Pelican has StartRoom 0 and a walk whose FIRST stop is
Hidden, so when that stop came round it was already nowhere, `start ==
dest == -1`, the branch was skipped, and it stayed on a genuine zero for the
rest of the game.  run400 writes its `&HFF` from the Hidden branch itself
(`loc_468D4A`) with no move test in front of it.

### The probe

Neither shape occurs often enough in the corpus to settle by replay, and the
one 3.90 row that moves with the answer (`ALEXIS.TAF`) is seeded and so
cannot be replayed at all.  `harness/make_400_walkhiddenprobe.py` and
`harness/make_39_walkhiddenprobe.py` build a three-cell game instead -- three
walkers that all arrive in the room the player never leaves:

| cell | shape | run400 (`Adrift_910`) | run390 (`Adrift_911`) |
|---|---|---|---|
| `Hid` | StartRoom 0, stops Hidden then Alpha -- the Skydiver's shape | `Hid wanders in.` | `Hid wanders in.` |
| `Nev` | StartRoom 0, one stop Alpha -- a genuine never-touched zero | *(silent)* | *(silent)* |
| `Base` | StartRoom Bravo, one stop Alpha -- a real room to leave | `Base wanders in from the east.` | `Base wanders in from the east.` |

`Nev` is the cell that proves `old <> 0` belongs in the gate at all; `Hid` is
the cell that proves the stamp is unconditional; `Base` is the baseline, and
it also shows the direction clause only survives when there was a real room
to come from.  Both Runners answer all three the same way, so the rule is not
a 4.0 rewording.  Post-fix scarier reproduces every cell.

(3.9 needs each walk started by a task: a NON-LOOPING game-start walk never
runs before 4.0 -- `npc_start_walk_is_390_noop`.  The 4.0 probe can use a
game-start walk for `Hid`, and does.)

### The port

In `npc_tick_npc_walk()` (`scnpcs.cpp`) the stamp moved out of the move
branch and after it:

```c
  if (is_exact && destnum == 0)
    gs_set_npc_walk_hidden (game, npc, TRUE);
```

It has to come after, because `gs_set_npc_location()` clears the flag on any
placement -- both stand in for one Runner field.

### Fallout

Four goldens re-blessed, one line each, all four of them a line the Runner
prints and we were dropping:

- `skydiver` -- the Pelican, `Adrift_246_skydiver.txt:49`.
- `the_cat_in_the_tree` -- Huey after `lean ladder against tree`,
  `Adrift_19_the_cat_in_the_tree.txt:12-13`.  The 2026-08-24 measurement on
  that row had already recorded this line; the golden was carrying the
  Runner's *other* NPC line (the boy's expired arrival) and missing this one.
- `alexis` and `alexis_worn_cube` -- "  Haron follows you." on the `open
  door` turn that introduces him.  3.90 and seeded, which is what the 3.9
  probe stands in for.

Full v4 suite: 428 PASS / 0 FAIL.

## Ported 2026-09-07: `%status_<name>%` is the lowest-indexed openable Short match

`briefcase`'s study ends its description with

    A door (%status_door%) leads out to the south-east.

and the golden read `(open)` where run400 prints `(closed)`.  The game has two
objects whose Short is `door`, and scarier was answering with the wrong one.

Scarier resolved the marker by asking the *parser*:
`uip_match ("%object%", name + 7, game)`, then read `Openable` off
`vars->referenced_object`.  Two things are wrong with that.  `uip_match_entity()`
(scparser.cpp:2066) walks every entity with no room or visibility filter and
calls `var_set_ref_object()` on each hit, so the surviving index is the LAST
match -- the highest-indexed namesake, not the first.  And because it is the
real parser it also rewrote `game->object_references` in the middle of
rendering a room description, which is a side effect a text marker has no
business having.

### The probe

`make_400_statusprobe.py` -> `p4STATUS.taf`, six static objects across three
rooms, every cell of the rule in one game:

    object 0  "a door"       Alpha,   Openable = CLOSED
    object 1  "a door"       Bravo,   Openable = CLOSED
    object 2  "a portal"     Charlie, OPEN, Alias[0] "gate"
    object 3  "a hatch"      Charlie, NOT openable
    object 4  "a hatch"      Charlie, OPEN
    object 5  "the grate"    Charlie, OPEN

Alpha and Bravo print `ST=[%status_door%]`; Charlie prints that plus
`AL=[%status_gate%] AP=[%status_a gate%] HA=[%status_hatch%]`
`PF=[%status_the grate%] PB=[%status_grate%]`.  Eleven commands
(`look / e / open door / look / w / open door / e / close door / look / e /
look`), run400 under Wine, `Adrift_921-923.txt`, 2026-09-07, every command
echoed.

### Which `door` answers

    watching from | door 0 | door 1 | run400 | scarier (before)
    Bravo         | closed | OPEN   | closed | open
    Alpha         | closed | open   | closed | open
    Bravo         | open   | CLOSED | open   | closed
    Charlie       | open   | closed | open   | closed

Row 1 is the one that matters: standing in Bravo, with Bravo's own door open
and Alpha's closed, run400 still says `closed`.  There is no room filter, no
visibility filter and no preference for the local object -- it is the lowest
index, full stop.  Rows 3 and 4 rule out "the one the player last touched",
which was the other candidate.

### What counts as a name

    AL=[%status_gate%]        -> "%status_gate%"        verbatim
    AP=[%status_a gate%]      -> "%status_a gate%"      verbatim
    HA=[%status_hatch%]       -> "open"
    PF=[%status_the grate%]   -> "open"
    PB=[%status_grate%]       -> "open"

So: aliases are never searched, even though the parser matches them; the Short
answers both bare and with its Prefix in front; a name matching nothing is left
in the text verbatim (`pf_interpolate_vars()` already copies a marker through
when `var_get()` returns FALSE, so that fell out for free); and the unopenable
`hatch` at index 3 is *skipped* on the way to index 4, not matched and then
rejected -- had it been matched-then-rejected, `HA` would have gone verbatim.

### The port

`var_status_object()` in scvars.cpp, a plain loop over the object bundle that
skips `Openable == 0`, compares Short case-insensitively, then Prefix + " " +
Short, and returns the first hit or -1.  No parser, no reference clobbering.
Post-fix scarier reproduces all nine probe cells exactly.

### Corpus exposure

Fourteen games use `%status_`, and every one of them is version 4.00, so
run400 alone covers the rule: EscapeToNewYork, Il Golem, Rock Band, Terrified,
WhereAreMyKeys, aparty, baroo, blood, briefcase, darkness, door, magicshow,
target, vague.  `aparty` is the interesting one -- its object 10 is Prefix
"the", Short "china cupboard", Alias[0] "china cabinet", and it writes
`%status_the china cabinet%`, which is an *alias* reference: the Runner leaves
it in the text verbatim, and so do we now.

One golden re-blessed: `briefcase`, `A door (open)` -> `A door (closed)`.
Full v4 suite: 428 PASS / 0 FAIL.

## Measured 2026-09-07: the 4.0 object-ambiguity prompt

`lca` T91 has run400 answering `chop tree` with

    Which tree.  The tree or the tree?

while scarier refuses the line outright.  That contradicts the gate comment on
`lib_co_ambiguity_prompt()` (`sclibrar.cpp`), which says 4.0 "never raises this
prompt from the dispatcher, so the port stops at 3.9".  4.0 does raise it -- it
just raises it from somewhere else, under a narrower test, with a different
follow-up.  Measured with a purpose-built probe; **nothing is ported yet**.

### The probe

`harness/make_400_coprobe.py` -> `p4CO.taf`.  Two rooms (Alpha east <-> Bravo
west), eight static objects, all with the same `A plain thing.` description so
that only the *choice* shows:

    0  a red tree    "tree"           Alpha    Short ambiguity, distinct NPs
    1  a blue tree   "tree"           Alpha
    2  a rock        "rock"           Alpha    unique, the control
    3  a mustang key "mustang key"    Alpha    alias "keys"
    4  a truck key   "truck key"      Alpha    alias "keys"
    5  a hut         "hut"            Alpha    alias "shed"
    6  a shed        "shed"           Alpha    Short "shed"
    7  the tree      "tree"           Bravo    never co-present, presence control

One task `poke %object%` -> `POKE.`, DontUnderstand `NO IDEA.`, and one event
printing `TICK.` so a swallowed turn would show.

Transcripts: `Adrift_924.txt` (first pass), `Adrift_925.txt` (second pass,
cells back to back), `Adrift_926.txt` (every cell isolated by a neutral
`look`), `Adrift_927.txt` (the answer-slot cells).  **`Adrift_924/925`
mislead**: run them back to back and the prompt's pending answer slot eats the
next probe command, so `x tree` right after a `chop tree` prompt reads as an
*answer* and prints `That is still ambiguous!`.  Only the `look`-separated run
measures the commands themselves.

### Which commands prompt (Adrift_926, cells isolated)

    chop tree   ->  Which tree.  The red tree or the blue tree?
    x    tree   ->  Which tree.  The red tree or the blue tree?
    chop shed   ->  NO IDEA.
    x    shed   ->  Which shed.  The hut or the shed?
    chop keys   ->  NO IDEA.
    x    keys   ->  Which keys.  The mustang key or the truck key?
    chop rock   ->  I don't understand what you want me to do with the rock.
    x    rock   ->  A plain thing.

Two different tests, then:

* the library **examine** path prompts whenever two or more *present* objects
  answer to the typed term, by Short **or** by Alias (`shed` = one Short plus
  one alias, `keys` = two aliases -- both prompt);
* the **unhandled-verb** path prompts only when the term is the **Short** of
  every candidate (`tree`).  A Short+alias or an alias+alias tie is not
  ambiguous enough for it and the game's DontUnderstand text comes out
  instead.

Presence really is filtered: object 7 is a third `tree`, and in Bravo, with one
`tree` present, `chop tree` gives the plain
`I don't understand what you want me to do with the tree.` (Adrift_924).

Wording is `Which <term>.  <NP> or <NP>?` -- a full stop, two spaces, the
object noun phrases in index order joined `, ` / ` or `, then `?`.  Identical
to the 3.7/3.8 prompt already ported, which is why `lca` reads `The tree or
the tree?`.  The turn's other output is replaced by the prompt alone.

Note that scarier's own ambiguity wording, `Please be more clear, what do you
want to <verb>?` (`lib_disambiguate_object_common()`, `sclibrar.cpp:4428`), is
a **SCARE invention**: the string is in none of the four Runner binaries.

### What a name is, and what gets listed (Adrift_928, 929, 930)

    chop key        ->  NO IDEA.
    x    key        ->  You see no such thing.
    chop mustang    ->  NO IDEA.
    x    mustang    ->  You see no such thing.
    chop truck key  ->  I don't understand what you want me to do with the
                        truck key.
    tree            ->  Which tree.  The red tree or the blue tree?
    rock            ->  I don't understand what you want me to do with the rock.
    mustang key     ->  I don't understand what you want me to do with the
                        mustang key.
    x    tree rock  ->  Which tree.  The red tree, the blue tree or the rock?
    x    rock tree  ->  Which tree.  The red tree, the blue tree or the rock?
    chop tree rock  ->  Which tree.  The red tree, the blue tree or the rock?

Four things fall out of those cells:

* **A name matches whole or not at all.**  `key` is a word inside two Shorts
  and `mustang` a word inside one, and neither names anything -- so
  `unlock drawer with key` in `sswhore` is *not* an ambiguity 4.0 would
  prompt about, and the remaining `Please be more clear` lines in the corpus
  goldens are a separate, still-unmeasured question.
* **The list is every object the line referenced**, in index order, not just
  the term's namesakes: `x tree rock` lists the rock too.
* **The term comes from the lowest-indexed ambiguous object, not from the
  typed order**: `x rock tree` still says `Which tree`.
* **No verb is needed.**  A bare `tree` raises the prompt; a bare `rock` gets
  the unhandled-verb catch-all naming it.

### The pending answer slot (Adrift_927, 929, 930)

The prompt leaves a question pending, and the *next* line is not always a
fresh command:

    x keys      / mustang  ->  That is still ambiguous!
    chop tree   / red      ->  I don't understand what you want me to do with
                               the red tree.
    x tree      / zzz      ->  That is still ambiguous!
    x tree rock / rock     ->  That is still ambiguous!
    x tree rock / blue     ->  A plain thing.
    x tree rock / x rock   ->  A plain thing.
    chop tree   / look     ->  (the room description; the question is dropped)
    chop tree   / n        ->  (lca Adrift_328_lca.txt:738 -- the player moves
                               north)

The slot claims the line **only when the line did nothing** -- the
DontUnderstand path, or the unhandled-verb catch-all -- and the turn's own
output goes with it, the way the 3.8 prompt replaces a turn wholesale.  A line
that did something runs normally and simply spends the question: `x rock` and
`look` are answered on their own terms even though `rock` and `look` are as
much "answers" as anything else.  The pair `rock` / `x rock` is the cell that
settles it: bare `rock` gets the catch-all in isolation (Adrift_930), so with a
question open it is claimed and comes back `That is still ambiguous!`, while
`x rock` examines the rock.

What the slot does with the line: the typed words go in front of the pending
term and the prompt's own candidates are re-scored with the 4.0 noun scorer
(Short whole word = 1, +1 for any alias hit, +1 per Prefix word found).  A
unique winner re-runs the **original** command with that object forced;
anything else prints `That is still ambiguous!`.  The scores explain every
cell: `red tree` = 2 against the blue tree's 1; `blue tree` = 2 against 1 and
the rock's 0; `rock tree` = 1/1/1, because the answer is scored against the
pending term and not on its own; `mustang keys` = 1/1, the Short not matching
across the plural and both aliases hitting.  The sibling
`That wasn't one of the options!` was never triggered by any cell and is still
unexplained.

Both the prompt turn and a `That is still ambiguous!` turn are
**administrative**: the probe's event is due on turn 1 and its `TICK.` lands on
the first `look`, whatever number of prompts and answers went before it.

### Ported 2026-09-07

`sclibrar.cpp` gained a `lib_co_400_*` block next to the 3.7/3.8 port:
`lib_co_400_raise()` prints the prompt (or the short `That is still
ambiguous!` when a question was already open) and stores the pending term,
command and candidates; `lib_co_400_raise_for_references()` is the examine
test, called from `lib_disambiguate_object_common()` ahead of SCARE's invented
`Please be more clear` listing; `lib_co_400_raise_for_short_tie()` is the
unhandled-verb test, called from `lib_cmd_verb_object()` on both the tie the
4.0 resolver reports and the positional matcher's multi-reference branch (the
`lca` cell reaches the second: scarier binds both trees, so the count is 2 and
the existing 4.0 block never ran);  `lib_co_400_answer_object()` scores an
answer.  `scrunner.cpp` calls `lib_co_400_begin_line()` before every line and
runs the answer slot after `run_all_commands()`, on `!status` or on the
catch-all's `lib_co_400_line_refused()` flag, emptying the filter first.  All
five probe feeds (Adrift_925-930) now match run400 cell for cell.

Three goldens moved, all three confirmed against a run400 transcript:
`lca` (`chop tree` -> `Which tree.  The tree or the tree?`, Adrift_328),
`shadricks_travels` (`climb tree` -> `Which tree.  The old oak tree or the
pine tree?`, Adrift_45) and `sswhore` (`examine chair` -> `Which chair.  The
velvet chair or the desk chair?`, Adrift_304:849, plus the one-turn shift of
the Oberst's two impatience events that follows from the prompt turn being
administrative -- run400 puts the first of them on the *next* command,
Adrift_304:856).  Full v4 suite: 428 PASS / 0 FAIL; a5 unchanged at
180 MATCH / 18 DIVERGE.

Still open from this measurement:

* **The second-noun ambiguity has no measured wording.**  `sswhore`'s
  `unlock drawer with key` still gets SCARE's invented `Please be more clear,
  what do you want to unlock?  The brass key or the old skeleton key?`, and
  `key` is not a name 4.0 would prompt on at all.  What run400 prints for an
  ambiguous *instrument* is unmeasured; the probe has no lockable objects.
* **`RestartType=2` with an immediate starter and a non-zero length fires
  once and never re-arms.**  The probe's ticker is StarterType 1,
  Time1=Time2=1, RestartType 2, and run400 prints `TICK.` on the first real
  turn of every feed and never again (Adrift_925-930, eight turns in
  Adrift_926); scarier re-arms it and ticks every turn.  This is the
  non-zero-length twin of the 2026-08-02 zero-length finding in
  `RUNNER_TESTS_TODO.md` section 4, whose fix gated only zero-length events.
  Corpus exposure is **zero** -- all 34 `restart=2` events in the 121-game
  corpus are `starter=2`, the random-delay starter, which re-arms through
  `ES_WAITING` -- and whether the Runner leaves the event parked in a running
  state (its LookText still in room descriptions) or finished is not measured,
  so this is recorded rather than ported.

## Ported 2026-09-07: the article test is case-SENSITIVE

Every Runner replaces a leading `a` / `an` / `some` with `the` when it prints
an object with the definite article (`lib_print_object_np`'s "the" form), and
scarier did the same through `scr_compare_word()`, which folds case.  It does
not fold case.

### The probe

`harness/make_arena_probe.py PFX` -> `p4PFX.taf`: one room, eleven objects
whose Shorts differ only in how the article is spelled, driven through
`fast.sh` in run400 (`Adrift_940_pfx.txt`).  The whole transcript in one
table -- left column the authored Prefix + Short, right column what `take`
printed:

    a alpha        Player take the alpha.          folded
    A bravo        Player take A bravo.            KEPT
    an charlie     Player take the charlie.        folded
    An delta       Player take An delta.           KEPT
    some echo      Player take the echo.           folded
    Some foxtrot   Player take Some foxtrot.       KEPT
    a big golf     Player take the big golf.       folded, adjective kept
    A big hotel    Player take A big hotel.        KEPT
    SOME india     Player take SOME india.         KEPT
    the juliet     Player take the juliet.         already definite
    The kilo       (never reached -- hands full)

So the test is a plain byte comparison of the leading word: lower-case `a`,
`an` and `some` are the only three spellings that become `the`, and anything
else -- `A`, `An`, `Some`, `SOME` -- is copied through untouched, definite
article and all.  `i` and the room listing print the Prefix verbatim in every
case, which is how the capitals were confirmed to be the author's.

`lib_compare_article()` in `sclibrar.cpp` is the case-sensitive replacement,
used at the three article sites only; `scr_compare_word()` stays case-folding
everywhere else (it is the parser's word test, and the parser really does
fold).  Four rows moved and were re-blessed: `ghosttown` (`You take A lump of
hard grease.`, run400 Adrift_325:774), `xfiles` (`(Getting off A Stool
first)`, Adrift_424:130), `spirits_flight` (four `Lamanluie cuts Kelorano
with An old scimitar.` battle lines) and `yeh` (`You pick up A Bow of Icy
Arrows.` / `You drop ...`).  The first two are confirmed against their own
run400 transcripts; the other two follow from the probe.

## Ported 2026-09-07: a static is named by `put`, and the empty-hand report

`thelasthour` turn 26 is the row that opened this.  `put hands into hole` --
the hand a STATIC object -- reads, in run400 (`Adrift_422_thelasthour.txt`
:161):

    > put hands into hole

    I can't take the hand!  I am carrying nothing!  Can't take the mouse. Too far.

scarier printed only the last sentence, the game's own task 15.  Two separate
rules were missing, both measured on a purpose-built probe.

### The probe

`harness/make_arena_probe.py PSTAT` -> `p4PSTAT.taf`, second person, one
room: dynamic `coin`, `box` (a container), `ring`, `cap` (wearable); statics
`anvil` and `slab`; and one task `put slab in box` -> `SLABTASK.`.  Nineteen
commands, `Adrift_941_pstat.txt` (13) and `Adrift_942_pstat2.txt` (19).  The
cells and what they say:

    put anvil in box   (empty hands)
        (Taking the anvil first)
        You can't take the anvil!  You are carrying nothing!

    put anvil in box   (coin in hand)
        (Taking the anvil first)
        You can't take the anvil!  <- and nothing more

    put slab in box    (empty hands, the task matches)
        (Taking the slab first)
        You can't take the slab!  You are carrying nothing!  SLABTASK.

    put coin in box    (coin already inside the box, both inventory states)
        You are not holding the coin.

    i                  (cap worn, nothing held)
        You are wearing a cap, and you are carrying nothing.

    put box in box     (either state)
        (Taking the box first)          <- and nothing whatever after it

The rules ported:

1. **A named static reaches the take piece at 4.0.**  There is no static test
   at name time; the one that turns the piece away lives in `insides`
   (@465ED7) and is silent, so the line is claimed, `name_object` announces
   the take and the take refusal is spoken in the take piece's own words
   (@47329D, `" can't take "` + name + `"!"`).  A refused static is *not*
   carried on into the "not holding" leftover report -- cell 2 ends flat --
   so the reference is cleared.  `lib_put_named_filter()` /
   `lib_put_implicit_take()`.
2. **The take phase closes with "You are carrying nothing!"** when nothing at
   all is held and nothing is left referenced.  Worn does not count (cell 5);
   the leftover list outranks it (cell 4 prints "You are not holding the
   coin." and no report); and it comes out *ahead* of the handler's task, so
   the task look-up had to be deferred to a second pass over the named
   objects (cell 3).  `lib_put_nothing_carried_400()`.
3. **A put that only refused does not claim the line.**  Cell 3's `SLABTASK.`
   is the general task pass answering after the library refused, the same
   split PUT4 measured for the size/capacity refusals; `is_refusal_only` now
   takes `static_refused && !has_moved` as well.

Recorded, **not** ported:

* `put box in box` announces the take and then says nothing at all, leaving
  the box unheld.  scarier keeps its own `You can't put an object inside
  itself!`, which is at least an answer; the recursion guard now suppresses
  the empty-hand report on that line so the two do not compound.
* `put coin in box` with the coin in hand is silent in run400 -- the known
  PUT7 quirk, no confirmation when the moved object is dynamic object #1.

### The tie-break: statics never win a shared name

Widening the named filter first broke three winning walkthroughs, all of them
a put whose noun is shared with a static standing in the room: `easter`'s
`put egg in basket`, `helsing`'s `put beads on dance floor`, `provenance`'s
`put wood on stump`.  All three are measured, and run400 raises no prompt on
any of them:

    easter      Adrift_273:135   You put the creme egg inside the Easter basket.
    helsing     Adrift_181:50    (the game's own task text)
    provenance  Adrift_342:1781  You place the piece of wood on the stump.

So the *resolver* -- the tie-break scarier applies when several present
objects answer to the typed word -- stays dynamic-only, while the *filter*
that selects the objects the handler then works on is the wide one.
`lib_put_resolve_filter()` and the `lib_put_{in,on}_resolve_filter()` twins;
only `lib_parse_multiple_objects()` gets the narrow form.

Full v4 suite after all of this: 428 PASS / 0 FAIL, with `thelasthour`,
`ghosttown`, `xfiles`, `spirits_flight` and `yeh` re-blessed.

## Ported 2026-09-07: only a LISTING reveals what the player is carrying

`yak_shaving` (4.00) was the live row.  Its jar of pickled eggs is dynamic
object 0 -- Prefix `a jar of pickled`, Short `eggs`, one alias `jar`,
InitialPosition 0 (hidden) -- and the Dada Lama's event puts it straight into
the player's hands on the way into the Sanctum Sanctorum.  From then on
run400 cannot resolve the noun at all (Adrift_401_yak_shaving.txt):

    turn 13  x eggs                run400   Either that isn't here, or it's
                                            not important.
                                   scarier  A jar of eggs, pickled in vinegar.
                                            ... is closed.
    turn 14  open eggs             run400   You can't open that.
                                   scarier  the game's own grapple text
    turn 16  give eggs to acolyte  run400   Give what?
                                   scarier  the game's own refusal
    turn 66  x eggs                run400   Either that isn't here ...

while turn 70 `give eggs to lama` is IDENTICAL either side, because
`give*eggs*lama` is a task pattern and resolves no noun.  The player of that
walkthrough never once refers to the jar by name and still wins.

### Probe SEEN

Four hidden objects, one control, one room; each object reaches the player by
a different route and is examined immediately afterwards.  `zza`/`zzb`/`zzc`
are tasks whose move-object action goes to held-by-player, worn-by-player and
"to room 1" (the room the player is standing in, no room description printed);
`zzd` is a task that starts an event whose Obj1 goes to held-by-player.

Driven in run400 2026-09-07, Adrift_p4seen.txt:

| line                | run400                        |
|---------------------|-------------------------------|
| `x alpha` (hidden)  | You see no such thing.        |
| `zza` / `x alpha`   | ZZA. / A probe object.        |
| `zzb` / `x bravo`   | ZZB. / A probe object.        |
| `zzc` / `x gamma`   | ZZC. / A probe object.        |
| `zzd` / `x delta`   | ZZD. / **You see no such thing.** |
| `i`                 | You are wearing a bravo, and you are carrying an alpha and a delta. |
| `x delta` after `i` | A probe object.               |
| `look` / `x gamma`  | (lists gamma) / A probe object. |

So:

* the task mover's post-move seen stamp is real and covers all three of its
  player-visible destinations (`task_move_object()` already had it);
* the **event** mover has no such stamp off its held/worn branches -- only
  the player-room compare at @00456124 that `evt_move_object()` already
  carries -- so an event can hand the player an object that stays
  unreferenceable;
* the **inventory listing** reveals: `i` marks everything it prints seen,
  exactly as the NPC lister does for an NPC's possessions.

### What was ported

`obj_turn_update()` -- a sweep that marked everything held or worn seen at the
top of every turn, and again at `obj_setup_initial()` -- is **gone**, along
with its two call sites in `scrunner.cpp` and its prototypes.  Its comment
justified it from run390's LOADER and inventory lister, neither of which is a
per-turn sweep; the loader's half is already done in `scgamest.cpp` from
`#InitialPosition`, and the lister's half now lives in `lib_cmd_inventory()`,
which stamps both the worn list and the carried list as it builds them.

`yak_shaving` re-blessed, 4 lines (the three refusals above plus one
acolyte-flavour line that moves with the extra turn), and the row is now
identical to run400 on all 71 turns.  Full v4 suite: 428 PASS / 0 FAIL; the
a5 suite is unmoved at 180 MATCH / 17 DIVERGE, all at baseline.

## Triaged 2026-09-07: the 48-row re-run -- every row accounted for

`par/summary.txt` rows 1-48 (transcripts `Adrift_377`..`Adrift_424`) were
re-driven and compared with `compare_wine_transcript.py`.  **No row is an
unexplained engine divergence.**  The table below is the whole batch, so that
a future session does not re-chase a row someone has already closed.

| rows | verdict |
|---|---|
| `murdermansionntro`, `ohhuman`, `redwire`, `zacksmackfoot` | **identical**, zero differing turns |
| `angeldevilhuman`, `asteroidafter`, `dangersdrivingnight`, `existence`, `goldilocks`, `hiker`, `man_overboard`, `masochists_heaven`, `p2p`, `professor`, `renegade_brainwave`, `shadricks_travels`, `shred_em`, `space_boy`, `vardock_bates`, `vault`, `yak_shaving`, `zombiecow` | identical apart from the Runner's own `[Press any key to end]` tail |
| `sophie` | 2 turns, **whitespace only** -- the `.txt` transcript drops alignment-only breaks (see the `<centre>` note above) |
| `sandy_meta_number` | 2 turns, the **already-documented** deliberate difference: `wait <n>` and `hist <n>` are SCARE's own meta-commands and exist in no Runner (harness row comment, measured 2026-09-05 in `Adrift_61`) |
| `3monkeys`, `jason_vs_salm`, `les_feux`, `light_up`, `shadowpeak`, `shadowpeak_allgargoyles`, `shadowpeak_killwraith`, `thelasthour` | RULE 2 -- the Runner never echoed 1..N feed commands, so everything after the first of them is out of step.  **Harness, not engine**; nothing may be read out of these past that point |
| `adriftorama`, `humbug`, `iachini`, `icecream`, `iqsfot`, `orient_express`, `puzzlebox`, `rockband`, `scandal`, `snakes_and_ladders`, `sommeril`, `ticket`, `xfiles` | **RNG.**  Each was confirmed by re-running scarier under two or three seeds and watching the disputed text move (see below) |
| `house` | **run400 defect** -- every successful move raises "Out of stack space"; the row is not comparable at all |
| `to_hell_and_beyond_assisted`, `to_hell_and_beyond_assisted_max` | **assists, by design** -- run400 cannot follow these feeds; see below.  (They trip RULE 2 as well, at `feed[115]`, but that is 23 commands downstream of where the assist has already parted the two streams) |

### The RNG rows, and how each was settled

Not one of these needed the .taf decompiled: re-run scarier under a second and
third `SCR_SEED` and the disputed line moves.  What is random in each:

* `adriftorama` -- the game randomises itself every play, by design.
* `humbug` -- Schrodinger's walk picks a random room each stop
  (`seed 1/2/7` give three different first-six sequences; run400's matches
  none of them).  humbug also randomises three start-up secrets.
* `iachini` -- the keypad number the mirror card shows ("PUSH KEY 54" vs
  "PUSH KEY 80") and the hot tub's starting pH.
* `icecream` -- whether the scoop falls off the cone.
* `iqsfot` -- three flavour tasks fired from timed events: `#heal_over_time`
  ("Irvine's health recovers slightly."), `#grupensips` and the HiRBy hover
  line.  Counts across the 169-turn replay: seed 31 -> 21/0/0,
  seed 32 -> 12/1/0, seed 99 -> 17/1/1, run400 -> 19/1/1.
* `orient_express`, `sommeril` -- timed flavour events (whistle, bell, mice)
  landing one or two turns either side.
* `puzzlebox` -- the box's locks are drawn per play.
* `rockband` -- the note colours.
* `scandal` -- the naval combat rolls (range, damage, "veers off to port").
* `snakes_and_ladders` -- the dice, from turn ~20.
* `ticket` -- the cat's walk and the lost girl's topic replies; the cat is in
  run400's transcript too (35 lines), just in other rooms.
* `xfiles` -- the travel destination: `n` at turn 56 reaches Bellefleur in
  run400 and the Davis warehouse in scarier, and scarier reaches Bellefleur
  0 or 2 times depending on the seed.

### `house`: run400 cannot run this game

`Adrift_381_house.txt` answers **every successful movement with nothing at
all** -- no "You move east.", no room heading, no body -- while `look` in the
same room prints the full block and a refused move prints "You can't go in
that direction, ...".  That is not the Verbose toggle and not the "Room names
in descriptions" checkbox (both were set by `fast.sh`'s registry prep, and
both would still leave *something*).

Re-driven 2026-09-07 with an eight-command probe
(`cmdfile_housemove.txt` = `2, e, look, e, look, w, look, x house`,
`Adrift_p4housemove.txt`): every `e`/`w` raises

    dialog 'ADRIFT Runner': evaluate error - Out of stack space

which the driver clears.  The move itself has already happened -- the `look`
after each one names the right room -- but the turn's output is thrown away
with the stack.  So run400 is **not an oracle for this game**: the 45 blank
turns are the crash, and the 112 later content differences are downstream of
the desync it causes (the transcript shows a `> Huh?` double prompt).  Leave
the row alone unless the crash itself is ever wanted as a measurement.

### `to_hell_and_beyond_assisted*`: the assist is the difference

Both rows diverge from turn 92 (`open drawer`) and never re-converge.  The
root is one task: "jump down from balconies" carries a single
`TASK_ACTION` type 1 (move player) with `Var2 = -1` -- an unset destination --
and `Var3 = 42`.  run400's `Select Case` ignores it, so the Runner prints the
task's text and leaves the player on the Balconies for the rest of the
replay; scarier, with `SCR_ASSUME_MOVES=1`, honours `Var3` as a room and walks
on.  That is exactly what the assist is for: the game is listed in
`GSC_GAME_ASSIST_TABLE` (os_glk.cpp) as "This game cannot be completed as
authored", and the plain `to_hell_and_beyond` row -- the one that stops before
the broken task -- was measured clean in run400 on 2026-09-06
(`Adrift_124`).  Nothing to fix; the two `_assisted` rows simply have no
run400 counterpart past command 92.


## Ported 2026-09-07: 4.0's named take falls back on every object SEEN

`cellar` T114 `take satchel` -- run400 "There is nothing worth taking here.",
Scarier "Take what?" (`Adrift_361_cellar.txt:724`) -- and `zelda`'s `get key`
in the Graveyard (`Adrift_319_zelda.txt:573`), the same refusal where Scarier
had the other one.  Both nouns name an object the player has seen and is no
longer with, and 4.0's take handler has an answer for that which this port
did not.

### The p-code

run400's take handler (`Proc_19_6`) binds its direct object with the co()
style resolver `General.Sub_22_66` at 00073011, mode 1.  Mode 1's eligibility
test (00063161) is `Sub_22_61(obj) And obj[18] = 0 And obj[30] = 1` -- and
`Sub_22_61` (0004B49C) is presence: location 0 (held), -100 (worn), -200/-300
(held/worn by an NPC), the player's room, or -20/-10 recursing into a parent
that is itself present and, for -10, open (`parent[34] < 6`).  So the first
pass sees only what is here.

The pass that matters is the second one.  At 00063465, after the object loop:

    If var_9C = 1 And var_A0 = 0 And param_10 > 0 Then
        var_9C = 0 : param_10 = 0 : Branch 000630BC     ' back to the top

`var_A0` is the count of candidates, so a first pass that bound nothing
re-enters the whole procedure with the mode set to 0, and mode 0's test
(0006310D) is the object's **seen byte alone**.  The three answers are then
printed at the tail of the take handler: "Take what?" at 0007332B and again
at 00073A25, "It is not clear which " & term & " you are referring to." at
0007335C, and "There is nothing worth taking here." at 00073A13 -- which is
where a line that bound an object but took nothing lands, since `If var_92 >
255` at 00073798 is only the "take X from Y" test.

The scoring loop, 000632BE .. 00063387, is the ordinary co() one: 1 if the
line contains the whole Short, 1 more for the first Alias it contains, and 1
more for each word of the Prefix that also appears.  Prefix words are only
counted once a name has matched, so a prefix word on its own names nothing.
`mdlSpreadTheLoad.Sub_20_43` (00046BFC) picks the term for the message the
same way: Short if the line contains it, else the first Alias it contains.

### The probe

`harness/make_400_takeprobe.py` builds three games -- p4TAKE (coin loose in
Alpha, statue static in Alpha, widget and gizmo loose in Bravo), `--tie`
p4TAKE2 (a red widget and a blue widget, both Short "widget", Prefix "a
red"/"a blue", plus a lamp aliased "light"), and `--hidden` p4TAKE3 (that
pair, with `vanish`/`vanish2` tasks moving either or both to hidden).  Six
feeds under run400 (`cmdfile_take1` .. `cmdfile_take6`, transcripts
`Adrift_p4take1` .. `Adrift_p4take6`):

    take widget   (Bravo never entered)        ->  Take what?
    take widget   (from Alpha, Bravo seen)     ->  There is nothing worth taking here.
    take statue   (from Bravo, Alpha seen)     ->  There is nothing worth taking here.
    take light    (from Alpha, an ALIAS)       ->  There is nothing worth taking here.
    take widget   (two seen absent namesakes)  ->  It is not clear which widget you are referring to.
    take widget   (one, then both, hidden)     ->  It is not clear which widget you are referring to.
    take red      (a Prefix word only)         ->  Take what?
    take zzz                                   ->  Take what?
    take widget   (both present, in Bravo)     ->  Which widget.  The red widget or the blue widget?

So statics count, hidden objects count, aliases count, prefix words alone
never do, and a present namesake hands the line back to the ordinary path --
which is where the present tie's co() prompt is raised, the one already
ported.

A hidden-object exclusion was tried first and is WRONG: it was fitted to
`zelda`, `light_up` and `yonastoundingcastle`, and p4TAKE3 (both widgets
hidden, still ambiguous) killed it.  Two of those three rows have no run400
oracle at all at the lines in question, which is how a wrong rule looked
supported: `Adrift_384_light_up.txt` dies in the battle long before the Waste
Land, and the `Adrift_324_yonastoundingcastle.txt` replay never reached ye
takery.

### What zelda was really about

Both of zelda's keys are Short "key", both are seen, neither is present --
and run400 still answers with the flat refusal rather than the ambiguity.
The scores are why: the super-hot key carries an Alias "key" on top of its
Short, so it scores 2 against the iron key's 1 and wins outright.  Only a tie
for the best score is ambiguous.

### The port

`lib_cmd_take_absent()` (sclibrar.cpp), wired as the new first row of
`STANDARD_FALLBACK_COMMANDS[]` in scrunner.cpp, ahead of the `[get/take/pick
up/pick] *` catch-all that still prints "Take what?" when it declines.  It is
4.0-only.  `lib_take_absent_score()` alongside it is the Runner's score.

Re-blessed: `cellar` (1 line) and `zelda` (1 line), both against run400
transcripts; `light_up` (the Waste Land `take lighter` block) and
`yonastoundingcastle` (1 line, `get title`), both extrapolations of the
measured rule with no run400 oracle at those lines, noted as such on their
harness rows.  Suite 428/428.

## Ported 2026-09-07: an empty CompleteText + an AdditionalMessage moves ShowRoomDesc AFTER the actions

Probe **SRD4** driven at last (`sh fast.sh p4SRD4.taf cmdfile_srd4.txt
run400.exe`, transcript `Adrift_949_SRD4.txt`).  Every cell has the same
ShowRoomDesc = Back Room and the same two actions, "Bob -> Store" then
"player -> Back Room", so a cell that omits "Bob is here, looking dangerous."
is one whose room block was built AFTER the actions:

| cell | field under test | Bob listed? |
|---|---|---|
| `b0` | CompleteText, nothing else | **yes** |
| `b1` | empty CompleteText | **yes** |
| `b2` | empty CompleteText + AdditionalMessage | **NO** |
| `b3` | CompleteText + AdditionalMessage | **yes** |
| `b4` | Repeatable = 0 | **yes** |
| `b5` | Where = one room | **yes** |
| `ne` | all five at once (lca task 237 to the letter) | **NO** |

So it is neither field alone: the block moves behind the actions only when the
task has **no CompleteText and a non-empty AdditionalMessage**.  Read against
the Runner's one-string room block ([[adrift4-room-block-one-string]]), the
shape is that with no CompleteText to carry it, the description rides out with
the AdditionalMessage instead -- which is emitted after the actions have run.
`b3` proves the AdditionalMessage does not by itself move anything, and `b1`
that an empty CompleteText does not either.

This closes **`lca` T252** (task 237: no CompleteText, an AdditionalMessage,
ShowRoomDesc = Haunted House, actions move the player in and Daisy out --
run400 drops "The ever alluring Daisy is here.", scarier keeps it).

It also closes **`ghosttown` T19** `u`, found the same day and the same shape
the other way round: task 129 `{go} [u/up]` in the cellar has CompleteText ""
and AdditionalMessage `"   "`, ShowRoomDesc = 4 (the Kitchen), and its actions
are *Ninette -> the Kitchen*, then *player -> the Kitchen*, then a redirect.
run400 lists "Ninette is here." in the block; scarier, building it before the
actions, does not.  Two games, opposite directions, one rule -- and
`AdditionalMessage = "   "` shows the test is on the FIELD, not on whether it
prints anything visible.

### The port

`task_defers_room_desc()` (sctasks.cpp) is the new predicate;
`task_run_task_unrestricted()` reads the AdditionalMessage up front, and when
the pair matches it skips the pre-action `task_show_room_desc()` and calls it
at the AdditionalMessage flush instead.  Position in the output does not
change -- b3 shows the room block joined to its AdditionalMessage by the
ordinary "  " and b2 has the same layout -- only the world state it is built
from.  4.0 only: nothing has measured the pre-4.0 Runners here, and their
AdditionalMessage handling is already entangled with the room block in its own
way (3.8's double-space test reads the description's tail, see
`task_suppresses_additional_message()`).  `task_show_room_desc()` is otherwise
untouched: [[adrift4-showroomdesc-before-actions]] stays right for every other
shape.

Both halves of the test are raw `<> ""`, not `scr_strempty()`, which is
whitespace-blind.  That is not a detail: written with `scr_strempty()` the
port fixed `lca` and left `ghosttown` exactly as it was, because "   " reads
as empty.  Only the AdditionalMessage half is measured; nothing in the corpus
has a whitespace-only CompleteText for the other half to bite on.

Nine goldens moved, and seven were confirmed line for line against their own
run400 transcripts before blessing:

| row | what moved | oracle |
|---|---|---|
| `lca` | "The ever alluring Daisy is here." drops | `Adrift_328_lca.txt` -- the 261-turn replay is now **identical on every turn** |
| `ghosttown` | "Ninette is here." appears | `Adrift_325_ghosttown.txt:144` |
| `vendetta` | "A thespian is here." drops | `Adrift_429_vendetta.txt:250-253` |
| `zelda` | "Zelda is present." appears | `Adrift_319_zelda.txt` -- clean but for one timed shopkeeper line |
| `lair` | "You can also see Lara's doll." appears | `Adrift_332_lair.txt:1315` |
| `yadfa` | "The Bugha is here." appears | `Adrift_336_yadfa.txt` -- identical apart from `<centre>` whitespace |
| `grumble` | bandits listed; "Uncle Grumble is here." drops twice | `Adrift_329_grumble.txt:1960, 2100, 2118` |
| `reluctantvampire` | "A zombie by the name of Harry is here." drops three times | `Adrift_357_reluctantvampire.txt:1124, 1134, 1144` |
| `3monkeys` | the untying and "You step over to the west." now precede the room block | **none** -- see below |

`3monkeys` is the one extrapolation.  Task 226 (`go * w`) has no CompleteText,
`ADDMSG = "<c></c>"` and three Execute-Task actions ahead of the player move,
so it is the measured shape to the letter -- ghosttown's invisible
AdditionalMessage included -- but the row trips RULE 2 and neither
`Adrift_16_3monkeys.txt` nor `Adrift_404_3monkeys.txt` reaches the turn.
Noted as an extrapolation on its harness row.

Suite back to 428 PASS / 0 FAIL.  `ghosttown`'s 12 remaining differing turns
are the two known classes and nothing else: the kerosene lamp dies at T31 in
run400 and T32 here, and NPC 3's tumbleweed is a roomgroup walk, so the rows
it lands on are RNG.

`ghosttown`'s two remaining diff classes after this are the kerosene lamp
dying one turn earlier in run400 (T31 vs T32) and the tumbleweed: that is
NPC 3's roomgroup walk (`Rooms[0] = 51` = room group 0, `Times[0] = 2`,
`npc_random_adjacent_roomgroup_member`), so its placement is RNG and the rows
it lands on are not comparable.

## Fixed 2026-09-07: make_wine_cmdfile.py dropped the solution's empty commands

`cbn`'s "second refusal" was never an engine difference.  Under SKIP the
generator treated a blank solution line as nothing at all and left it out of
the feed, on the theory that the compare tool re-aligns the offset -- but an
empty line is a TURN: it ticks events and a game can hang a task off it.
`CBN.taf` is the case.  Its solution opens with five empty commands and TASK
38 turns the first of them into the move out of [The Story So Far...], so with
them dropped the Runner spent that task on `open door`, never opened the door,
and all 35 commands after it ran off-route -- which is what put a bare `> `
prompt in front of each "Clueless Bob is confused!" in `Adrift_149_cbn.txt`.

The generator now emits them.  Re-driven with the corrected feed
(`Adrift_943_cbn.txt`): **identical on every turn**, only the Runner's
trailing `[Press any key to end]` differs.  `cbn` is closed.

25 wired SKIP rows have blank commands in their solutions and every measured
one of them was fed the same way, so their transcripts are all suspect.  All
24 with a game to hand were re-driven with regenerated feeds; 18 produced
transcripts (`Adrift_425`-`448`, tagged), **not yet compared**:

    motion justanotherday thepkgirl vendetta cbn2 adriftorama ticket
    mysteryofcaves asdfa spam vagabond yonastoundingcastle forum pyramid
    imagidroids crimsondetritus existence secidenoddcomp

Six failed to load in run400 at both 4-way and 2-way ("no titled Runner window
for pid N", so the Runner never opened the game): `wonderwombat`,
`the_town_of_azra_v390` (a 3.90 game -- run390 fodder), `ecod2`, `everything`,
`archie`, `chosen`.  Chase the load failure before reading anything into it.

## Fixed 2026-09-07: compare_wine_transcript.py could not read a re-blanked feed

The regenerated feeds could not be compared at all, because the compare tool
still assumed the old generator's output.  `read_feed()` short-circuited on
`skip_wired` and dropped every blank line -- correct while the only blanks in
a SKIP feed were the Returns that answer a `<waitkey>`, and wrong the moment
the generator started emitting the solution's own empty commands as well.
`vendetta`'s 207-command feed read as 205 commands and the whole row came out
as a turn-0 divergence.

The fix is to stop special-casing SKIP: whenever the row has a `.taf`, run the
pause-count fixed-point classifier over every blank, which is what already
told the two kinds apart on the non-SKIP rows.  A blank whose span the game
prints a pause into is a Return; a blank that survives the fixed point is an
empty command and a turn.  Verified against the old feeds as well -- the
classifier reaches the same answer there, so nothing already blessed moved.

Every diff count taken against a regenerated feed before this is worthless in
the same way the 2026-09-06 blank-line drift made the older ones worthless.

## Compared 2026-09-07: the 18 re-driven rows, every row accounted for

With the generator emitting the solution's empty commands and the compare
tool able to read the result, `Adrift_425`-`448` were finally diffed.  Ten of
the eighteen are clean, seven are RNG and one is new evidence.

### Clean (10)

`justanotherday` (426), `mysteryofcaves` (435) and `imagidroids` (444) are
**identical on every turn**.  `cbn2` (431), `asdfa` (436), `spam` (439),
`vagabond` (440), `pyramid` (443), `crimsondetritus` (445) and `existence`
(447) are identical apart from the Runner's own `[Press any key to end]`.

`vagabond` is the one worth calling out: its single divergence used to be the
ALR-over-a-joined-paragraph residual at room 4 ("A toolbox is here." +
George's `#` InRoomText), and the room-block-as-one-string port on 2026-09-07
closed it.  The row is now clean, and the note at "Measured so far" that says
otherwise is superseded.

### RNG, and what makes each one random (7)

| row | the random thing |
|---|---|
| `forum` (442) | EVENT 0 "Monk walks in" is `time1=2 time2=6` -- a random duration.  The monk arrives at scarier T4 and run400 T6; the two transcripts are otherwise word-identical. |
| `secidenoddcomp` (448) | the house's atmospheric one-liners ("Something howls in the distance.", "You hear the faint sound of cackling laughter") -- different picks on different turns. |
| `ticket` (434) | the cat and the lost girl are roomgroup walkers.  Verbose was ON (17 `strolls`/`wanders` lines in the Runner's own transcript), so this is not RULE 1. |
| `vendetta` (429) | the weather and crowd-noise events.  Also a tail artefact: the compare tool cannot align the feed's last two blanks, so its "turn 206" is the ending, which both sides print in full. |
| `adriftorama` (433) | a golf minigame -- ball colour, course, hazard and opponent are all rolled.  Nothing is comparable past turn 2. |
| `yonastoundingcastle` (441) | Goblin Bob's thefts and idle antics.  ALSO **RULE 2**: 2 commands were never echoed, the first at feed[170] `yorick`, so everything past 170 is out of step and the row needs re-driving before anything after it is read. |
| `thepkgirl` (427) | the umbrella peddler, the pervert and the shopkeeper's idle lines are all walkers or Rnd picks.  Two turns in it are not RNG -- see below. |

### `thepkgirl` T103: an ALR spans a task and the task it executes

The clearest new evidence in the batch.  Task "# Laurie says good morning"
has

    COMPLETE=[... They should be done soon."]
    ACT type=5 -> task 700 ("turn on * toaster", COMPLETE=[The toaster is now on.])

and the game's ALR table carries

    Original     done soon."  The toaster is now on
    Replacement  done soon."<br><br>    Laurie turns on the toaster

Note the two spaces in the Original.  run400 prints "Laurie turns on the
toaster."; Scarier prints "The toaster is now on.", because it filters each
string on its own and the ALR never sees the join.  This is the known
**ALR Originals that span the joined paragraph** lead under "Still open", and
it says something the room-block port did not cover: the paragraph the Runner
filters is not just the room block, it is the completing task's own text plus
the output of every task it executes.  Add `the_pk_girl.taf` T103 to that
bullet's evidence.

### `thepkgirl` T52: a walk-triggered task fires in Scarier and not in run400

`open window` ends chapter 1.  run400's turn stops at "Press enter to
continue" and the next keypress brings CHAPTER 2; Scarier prints, in the same
turn, task 413 `# Laurie rejoins you at lot`

    COMPLETE=[<br><br>    Laurie becomes aware of your presence and lifts her
    head from her hands.  They are soaked with tears.<br><br>[L-7,0,0]]

with its 1/2/3 menu, and only then CHAPTER 2.  No `ACT type=5` anywhere in the
game executes 413; the only reference is Laurie's `WALK 2 loop=1
startTask=410 charTask=413(task412) meetChar=0 stopTask=412`, so this is a
walk-step firing, and the walk is one step ahead of run400's here.  The
solution never answers the menu and the run still wins, which is why the row
was blessed with it.  Open lead; the walk rules to re-read are
[[adrift4-walk-exact-tick-move]] and
[[adrift4-hidden-stop-stamps-walker]].

### `motion` (425): not comparable as fed, and the reason is worth chasing

`Motion.taf`'s rocket-launch minigame is played by pressing Enter, so its
turns and its `<waitkey>` answers are the same keystroke and the two sides
disagree about which is which: 137 feed lines, 124 turns echoed, and the
compare tool reports the seven "lost" commands as a pause-count difference
rather than RULE 2.  The frames themselves diverge from the very first one --
`Fuel in Rocket:` is `* * * * * * * * !` in run400 against `* * * * * * * * *`
in Scarier, and by turn 7 run400 has burned all nine and lost the minigame
while Scarier still has five.  Nothing else in the turn differs (the ASCII
frames are word-identical but for the fuel gauge).  Either the Runner is
taking more ticks per Enter than the feed assumes, or the generator owes this
game a different blank-per-frame rule.  Left open: re-cut the feed for a game
whose turn IS its keypress before reading anything into the gauge.

## Ported 2026-09-07: a leading `<br>` is collapsed only against a break of SCARIER's own

### The archive is 267 free line-structure oracles

Every comparison in this file so far has been about *content* -- which words
a turn prints.  The archived transcripts also carry line structure, and it
has never been mined, because the obvious reading of it is wrong in one
direction and right in the other:

- `Adrift_N.txt` **drops** line breaks the RichTextBox really has, wherever
  the alignment changes -- the `<centre>` artefact under
  [[adrift-runner-transcript-centre-artefact]].
- It never **invents** one.

So a break that is in the `.txt` and not in Scarier's output ("runner-only")
is real ground truth; a break in Scarier's output that the `.txt` lacks
("scarier-only") is suspect and mostly artefact.  267 archived rows, and
nobody had ever counted the first kind.

To count them exactly, and not through the 78-column wrap, `os_ansi.cpp` now
takes its wrap width from `SCR_WRAP_WIDTH` (default 79, so no golden moves).
Set it wide and there is nothing to infer: every newline in Scarier's output
is one the engine meant.  `harness/sweep_wine_breaks.py` is the sweep -- it
reuses `compare_wine_transcript.py`'s reader and runner, aligns the two sides
word by word over every row of `~/adrift-battle/runner/wine/jobs_*.txt` that
has an archived transcript, and reports each break by which side has it and
whether it is a single newline (k1) or a blank line (k2).  Only turns that are
already word-identical are compared, so it can never confuse a content
difference for a structural one.

    sh build.sh && python3 sweep_wine_breaks.py            # 267 rows, ~4 min
    python3 sweep_wine_breaks.py --only ghosttown --limit 40

Baseline: **107 runner-only breaks over 24 rows**, against 6011 scarier-only
over 202 (the artefact).

### The bug

53 of the 107 were the same shape: an event, atmosphere or NPC message that
begins with `<br>` follows something that already ended in a break, and
run400 prints a **blank line** where Scarier printed one newline.
`Ghost town v1,05.taf` at the `ask ninette to come with you` turn is the
clearest (`Adrift_325_ghosttown.txt` 586-592):

    She is a bit annoyed being told what to do, but follows you anyway.
    <blank>
    The sun is slowly settling behind the distant mountains ...

`pf_buffer_paragraph()` dropped a single leading break whenever the buffer
already ended with a break of any kind:

    if (buffered && !join_pending
        && pf_text_ends_with_break (buffered)      /* '\n' OR "<br>" */
        && pf_text_leads_with_break (string))
      string += (*string == '\n') ? 1 : 4;

That collapse exists for a real reason -- Scarier terminates a room block, an
exits list and an NPC announcement with a newline of its own, and the Runner
does not, so without it every event text after a room description would open
with a blank line the Runner does not print.  But the test could not tell
those newlines from a break the Runner really stores, and there are two of
those:

- **an author's own trailing `<br>`.**  The tag is still standing verbatim in
  the buffer at that point -- tags are translated at filter time, not at
  buffer time -- so the buffer really does end `"...follows you anyway.<br>"`.
  Ghost town's task text ends `<br>`, its event text starts `<br>`, and
  run400 has both.
- **4.0's `"Time passes...\n"`**, which the Runner stores with `vbCrLf`
  concatenated (already modelled as `pf_buffer_hard_break()`).
  `Adrift_254_patient7.txt` 81-87 is the case: `Time passes...`, a blank
  line, then the event.

### The rule

Collapse only against a break SCARIER put there, and the discriminator is
already in the buffer:

    pf_text_ends_with_newline (buffered)          /* a literal '\n', never "<br>" */
      && pf_text_leads_with_break (string)
      && !(hard_break_at == buffer.size ())       /* 4.0's "Time passes...\n" */
      && !(reference_at  == buffer.size ())       /* a bracketed reference line */

A `<br>` still at the end of the buffer is the author's and is never collapsed
against; a bare newline there is one of ours (room block, exits list,
`npc_announce()`'s `".\n"`, `pf_buffer_paragraph_line()`'s terminator) and
still is.

An earlier attempt gated the collapse on `auto_break_at`, the position
`pf_buffer_paragraph_line()` records for its own terminator.  It fixed the
same 88 breaks and cost 19 new scarier-only ones, because plenty of our
newlines are written as part of a longer string and never recorded --
`npc_announce()` ends `pf_buffer_string (filter, ".\n")`, so does
`lib_print_exits_list()`, so does the pre-3.9 brief room line.  The
buffer-contents test needs no bookkeeping at all and gets those for free.
`auto_break_at` is left alone; it still serves `pf_undo_auto_break()` and
`pf_ends_with_double_space()`.

### Measurement

| | before | after |
|---|---|---|
| runner-only breaks | 107 over 24 rows | **19 over 9 rows** |
| scarier-only breaks | 6011 over 202 rows | 6011, **unchanged row for row** |
| word alignment | -- | byte-identical on all 267 rows |

15 rows closed outright: `patient7` 19, `provenance` 16, `datewithdeath` 9,
`ghosttown` 8, `thelasthour` 3, `dragonshrine` / `reactor1` / `theseance` 2,
and `howitstarted` / `iqsfot` / `riding_home` / `second_chance` /
`snakes_and_ladders` / `sswhore` / `tictactoe` 1 each.  `vendetta` 21 -> 1.
`provenance` is now line-structure exact over the 832 turns that align, and
`patient7`, `dragonshrine` and `snakes_and_ladders` have zero breaks in
either direction.

30 goldens re-blessed, 130 added blank lines, **no content change anywhere**
(no removed line, no non-blank added line).  Suite 428 PASS / 0 FAIL.
ADRIFT 5 is untouched -- nothing under `adrift5/` calls
`pf_buffer_paragraph()`.

### The 19 that remain -- two new leads

Neither is the collapse; both are about the break in front of a **room
heading**, and both want a probe rather than a guess.

**k1, the heading runs on (9 breaks, 4 rows).**  `professor` t11/t18/t38/t54,
`viewtohome` t20/t56, `tophat` t0/t1, `woof` t8.  Scarier prints
`You move west. Whimsington Square` and then the NPC line on the same line
the Runner breaks:

    You move west.  Whimsington Square
    Shelly is walking slowly, delivering the mail.   <- run400 breaks here

**k2, a blank line before the heading (10 breaks, 5 rows).**  `baroo`
t120/t121, `blood` t30/t70, `cursed` t76/t134, `thepkgirl`
t125/t177/t257, `vendetta` t113.  A task or event text ends, and run400 puts
a blank line before the room name where Scarier puts one newline:

    ... paddles the canoe faster down the river.
    <blank>                                        <- run400 has this
    Ravine River
    The river performs a lazy turn ...

Both smell like the room-name heading's own leading break (a 3.9+ feature,
see `lib_describe_player_room()`), not like paragraph spacing, and the k2 set
is all ShowRoomDesc-from-a-task displays.  A `SRD`-style probe varying what
precedes a room display would settle them.

## Ported 2026-09-07: the room heading's own two breaks, and a stale position marker

The two leads left by the section above -- k1, a missing break *after* a room
heading, and k2, a missing blank line *before* one -- are the same fact seen
from two sides, and porting it takes the archive's **real** direction to
zero: `sweep_wine_breaks.py` now reports

    TOTAL runner-only 0  scarier-only 6013

over the same 267 rows, with the word alignment byte-identical to the run
before (`aligned N/M` unchanged on every row).  Not one line break that a
Runner transcript really has is missing from Scarier's output any more.

### The heading is "\n" + name + "\n", not a heading with spacing around it

SCARIER treats the room name as a heading it is inserting into the Runner's
prose, and gives it a blank line above (`pf_buffer_paragraph_break()`) and a
terminator below.  The archive says the Runner is doing something simpler and
flatter: `showshortroom` concatenates a break, the name and a break onto the
one output string the turn is building, and everything downstream just carries
on appending to it.  Every consequence of that model is measured:

  * *After* the heading, the break is the Runner's, not a section terminator
    of ours -- so `pspace()`, which only ever *appends* a separator, leaves it
    standing.  SCARIER's `pf_buffer_join()` popped it, and a room whose Long
    is empty ran its contents straight on after the name:

        Inside the Top Hat  A bunny twitches its whiskers at me   <- SCARIER
        Inside the Top Hat                                        <- run400
        A bunny twitches its whiskers at me

    Fixed by having `lib_print_room_name()` mark its newline with
    `pf_buffer_hard_break()`.  Closes all 9 k1 breaks: `professor`
    t11/t18/t38/t54, `viewtohome` t20/t56, `tophat` t0/t1, `woof` t8.

  * *Before* the heading, the Runner's leading break lands after whatever the
    turn had already said.  SCARIER's equivalent is
    `pf_buffer_paragraph_break()`, which tops the buffer up to two trailing
    breaks -- but it asked `pf_get_buffer()`, which does not look through the
    hidden-prefix barrier.  A 4.0 task runs its actions with the turn's text
    hidden (`pf_hide_prefix()`, so that text an action prints opens its
    paragraph as it does pre-4.0), so a task that an action runs showed its
    room into what looked like an empty buffer and got no leading break at
    all.  Fixed by looking through the barrier when the visible part is empty
    but something is hidden behind it.  Closes all 10 k2 breaks: `baroo`
    t120/t121, `blood` t30/t70, `cursed` t76/t134, `thepkgirl`
    t125/t177/t257, `vendetta` t113.  All five games are 4.00 (signature bytes
    8-10 = `147 69 62`), which is what put them on the hidden-prefix path.

### The stale position marker the first fix exposed

`hard_break_at`, `reference_at` and `auto_break_at` each record a buffer
*length*, and a length means "the end of the buffer" only until something else
is buffered.  `pf_buffer_string()` cleared `auto_break_at` on every append and
left the other two standing, which was survivable while `hard_break_at` was
set once a turn for 4.0's "Time passes...".  Setting it on every room heading
made it bite immediately, in `circus`:

    [HARD SET] tail=You move north.\n\n<b>Animal Cages</b>\n     (length 53)
    ... three turns later ...
    [HARD KEEP] tail=You get no reply from the videotape.\n      (length 53)

-- the same length by coincidence, so the walk announcement that should have
joined that line ("You get no reply from the videotape.  Barb arrives from the
south.") broke instead.  `shadowpeak` and `ticket` collided the same way.
`pf_buffer_string()` now drops all three markers on any append, which is also
the right answer for `pf_prepend_string()`, where the two positional ones were
silently wrong (a prepend shifts every position in the buffer).

### What moved

41 goldens re-blessed, **whitespace only** -- every changed file has an
identical word stream to the one it replaces.  160 added lines against 18
removed; the removals are all re-wraps where a heading moved onto its own
line.  Suite **428 PASS / 0 FAIL**; the ADRIFT 5 suite unchanged.

Two new *suspect*-direction breaks appear, `blood` t76 and `buried_alive` t2
(6011 -> 6013): a blank line before a heading that the `.txt` does not have.
That is the direction the transcript is known to lie in -- `Adrift_N.txt`
drops breaks the RichTextBox really has and never invents one -- and
`buried_alive` writes its room names as `--The Kitchen--`, exactly the
centred-heading shape the artefact comes from.  Nineteen real breaks closed
against two suspect ones is the trade.

One golden gains a blank line with no transcript to check it against:
`hungry`, whose "Escaped!!!!" room opens its Long with a `<br>`.  Marking the
heading break as the Runner's also stops `pf_buffer_paragraph()` collapsing
that `<br>` against it, which is what the model says should happen -- the
Runner has no collapse at all, only SCARIER does, and only for breaks it
supplied itself -- but no archived transcript covers it.  `whitterscap` moves
too and has no transcript either, though its change is a pure re-wrap.


## Ported 2026-09-07: `isare()` cell by cell, and the empty Prefix the loader fills in

The last of the "Still open" engine leads, and the only one whose written
form was wrong: `obj_appears_plural()` was never the Runner's rule, but not
in the way the lead said.

### The helper, read offline

All four Runners carry the same `isare(prefix, name)` -- run370 `423E5C`,
run380 `428EAC`, run390 `431038`, run400 `4507BC` (`Proc_19_69`) -- and the
four decompilations are the same VB line for line:

    r = " is "
    If Left(prefix, 4) = "some" And Right(name, 1) = "s" Then r = " are "
    If Right(name, 1) = "s" Then
      If Mid(name, Len(name) - 1, 1) <> "u" Then r = " are "
    End If
    If prefix = "a"  Or Left(prefix, 2) = "a "  Then r = " is "
    If prefix = "an" Or Left(prefix, 3) = "an " Then r = " is "

It is a chain of overwrites, not a nest.  Inherited SCARE's guess -- "not
a/an/empty, then a trailing 's' not preceded by 'u'" -- differs in four
places, and `where <name>` is a per-object oracle for all of them, `whereis`
composing `name & isare(prefix, short) & LCase(room) & "."` (run400 `468115`,
run380 `4374E1`).

### The probe

`harness/make_arena_probe.py ISARE` -> `p4ISARE.taf`, one room, twelve
objects on the floor, one per prefix spelling, driven through `fast.sh` in
run400 (`Adrift_isare.txt`, 2026-09-07).  Opening `look`, then `where` on
each:

    Also here is a boots, a cactus, some gloves, some walrus, a beads,
    an eggs, A shoes, the keys, the NAILS, a big pins, Some socks and  rings.

    Prefix    Short     where says                       scarier said
    ""        boots     The boots is test arena.         is    agrees
    ""        cactus    The cactus is test arena.        is    agrees
    "some"    gloves    The gloves are test arena.       are   agrees
    "some"    walrus    The walrus are test arena.       is    DIFFERS
    "a"       beads     The beads is test arena.         is    agrees
    "an"      eggs      The eggs is test arena.          is    agrees
    "A"       shoes     A shoes are test arena.          is    DIFFERS
    "the"     keys      The keys are test arena.         are   agrees
    "the"     NAILS     The NAILS is test arena.         are   DIFFERS
    "a big"   pins      The big pins is test arena.      is    agrees
    "Some"    socks     Some socks are test arena.       are   agrees
    " "       rings      rings are test arena.           is    DIFFERS

So, four cells:

1. **`some` reaches a name the -us exception would spare.**  The "some"
   clause runs first and the -us test never puts the singular back, so `some
   walrus` is plural.
2. **The article test is case-SENSITIVE**, the same Option Compare Binary
   fact probe PFX measured for the normalizer: `A shoes` is not an "a"
   prefix, and is plural.
3. **The trailing-`s` test is case-sensitive too**: `NAILS` ends in a capital
   and is singular.
4. **An empty prefix does not force the singular** -- and this is where the
   lead in "Still open" was wrong about the mechanism.  `isare()` has no
   empty-prefix test because it never sees one: the loader substitutes a
   literal `"a"` for an empty Prefix (run400 `loc_4900EC`, run380 `4481B2`,
   run370 `43F5DA`) *before* stripping its trailing spaces.  So `boots` with
   no prefix is an "a" object, singular, exactly as scarier already said.
   What is plural-capable is a prefix authored as a single **space**: not
   empty, so it escapes the substitution, then trimmed to nothing.

### The whitespace-only prefix, and what it printed

That last cell also settles what the printers do with a genuinely empty
prefix, which no amount of reading the article normalizer could:

    Also here is ... Some socks and  rings.     <- two spaces, no article
    > where rings
     rings are test arena.                      <- leading space, no article

The Runner's name builder is a plain `tense(Prefix) & " " & Short`, so the
separator is unconditional and an empty prefix costs a space and nothing
else.  Scarier had three separate imitations of the loader substitution
instead -- `"the "` in `lib_print_object_np`, `"a "` in `lib_print_object`,
`"a"` in `lib_print_object_raw` -- plus a fourth in the pronoun antecedent in
`scparser.cpp`, each keyed on `scr_strempty()`, which is TRUE for a
whitespace-only string and so folded the two cases together.

### The port

`parse_trim_object_names()` in `sctafpar.cpp` now does the substitution, in
the loader where the Runner does it and **before** the trailing-space strip
(the order is the whole point: `""` becomes `"a"`, `" "` becomes `""`).  The
four downstream imitations are gone, the two printers concatenate
unconditionally, and `obj_appears_plural()` in `scobjcts.cpp` is the exact
`isare()` above, case-sensitive, with a note on the one-character Short that
faults VB's `Mid(name, 0, 1)` and does not exist in the corpus.

### Corpus exposure

Measured on the raw pre-trim fields, not the loaded ones -- a temporary
`SCR_DUMP_RAWPREFIX` in the loader, 426 games, **27386 objects**.  2844 carry
an exactly empty Prefix; **none** is whitespace-only, so cell 4 is
faithfulness rather than a fix.  Cells 1-3 move **twelve objects in six
games**, all from " is " to " are ":

    The X-Files: A New Beginning   A Pair Of Dockers, A Pair Of Blue Jeans,
                                   A Pair Of Nikes, A Bowl Of Peanuts,
                                   A Set Of Directions
    pestilence                     some zeus, some apparatus
    sa, sophie                     some fungus
    xycanthus                      A pile of debris
    yeh                            A bag of apples, A Bow of Icy Arrows

Five of the six have wired walkthroughs and only one golden moved: `yeh`
line 80, `Also here is A Bow of Icy Arrows.` -> `are`.  That is the same
object the case-sensitive article port re-blessed earlier the same day, and
like that one it follows from the probe rather than from a run400 transcript
of `yeh` itself.  Suite **428 PASS / 0 FAIL**, the ADRIFT 5 suite unchanged.
