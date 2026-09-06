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
| `Vagabond.taf` | 4.00 | `Adrift_42_vagabond.txt` | 10/10 echoed; ONE divergence, and it is the **known ALR-over-a-joined-paragraph residual** of section 3, not a new one.  Room 4's Long ends "A toolbox is here." and George's InRoomText is `#`, so the Runner's joined paragraph reads "A toolbox is here. … |
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
| `House.taf` | 4.00 | `Adrift_91.txt`, `Adrift_92.txt`, `Adrift_93.txt` (checkpoint drives from a Scarier-made `.tas`, `#restore` after the title menu's `2`) | the put/task precedence model **confirmed** and one gate rule **corrected and FIXED**: at the fireplace with the wood on the floor and the axe in hand, `put wood in fireplace` / `place wood in fireplace` print `(Taking the wood first)` then `Your hands are full.  You are not holding the wood.`; with the wood held every spelling (`put`, `place`, `drop wood in fireplace`, `put some wood into the fire place`) is the library put, task 459 never fires, `light fire` refuses with `You need some wood or coal to make a proper fire.` -- **House is unwinnable in run400**.  Scarier used to skip the implicit take because take-flagged task 60 `* %object%` pre-matched: run400's pre-matcher is restriction-aware, and task 60's silently-failing restriction drops it.  Every move pops an `evaluate error - Out of stack space` alert (the `%drunk%` ALR loop, see "Deliberate deviations").  `get cathy` there was `Take what?` in run400 against the library's take-NPC line in Scarier: five more drives (`Adrift_94`-`98`) pinned it -- the first line naming Cathy after the checkpoint runs the once-only silent task 200 `*cathy*` (`# attention on cathy grave vision`), and a task having run for the line shuts the take/examine/where/attack/talk-to branches of the character handler (`MemVar_4941F8`; ask-about, give and kiss survive -- Humbug's silent `ask * hacker about * humbug` still answers), so `get cathy` falls to `Take what?` and `x cathy` to `You see no such thing.`; the second mention gets "I don't think girl would appreciate being handled." (Prefix + first Alias, not the Name) and her description.  Restore does NOT clear her seen byte.  Both rules **PORTED 2026-09-06** (see "Ported 2026-09-06: the task-ran NPC gate") |


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
| `goldilocks.taf` | `goldilocks` | 252 | 8 | 6 | 10 | -- | [Goldilocks_walkthrough](Goldilocks_walkthrough.md) **measured** -- see "Measured so far" |
| `CIBASS.taf` | `cibass` | 40 | 8 | 2 | 8 | yes | [CIBASS_walkthrough](CIBASS_walkthrough.md) **measured** -- see "Measured so far" |
| `FunHouse.taf` | `funhouse` | 18 | 8 | 9 | 0 | -- | **done** 2026-08-24 -- see "Measured so far" |
| `sa.taf` | `sophie` | 255 | 7 | 73 | 13 | yes | **partly done** 2026-08-25 (first 50 commands) -- see "Measured so far"; [Sophies_Adventure_walkthrough](Sophies_Adventure_walkthrough.md) |
| `sophie.taf` | `sophie_comp` | 255 | 6 | 72 | 13 | yes | [Sophies_Adventure_walkthrough](Sophies_Adventure_walkthrough.md) |
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
| `DragonShrineR43.taf` | `dragonshrine` | 136 | 1 | 1 | 7 | yes | [The_Curse_of_DragonShrine_walkthrough](The_Curse_of_DragonShrine_walkthrough.md) |
| `BlackSheepsGold.taf` | `black_sheeps_gold` | 99 | 1 | 11 | 1 | yes | -- **measured** -- see "Measured so far" |
| `QuiATueDana.taf` | `qui_a_tue_dana` | 63 | 1 | 4 | 0 | yes | -- |
| `plunder_gargoyle.taf` | `plunder_gargoyle` | 43 | 1 | 3 | 4 | -- | [Pirates_Plunder_walkthrough](Pirates_Plunder_walkthrough.md) |
| `demonhunter.taf` | `demonhunter` | 40 | 1 | 2 | 2 | -- | [Apprentice_of_the_Demonhunter_walkthrough](Apprentice_of_the_Demonhunter_walkthrough.md) |
| `Invasion of the Second-Hand Shirts.taf` | `invasion_shirts` | 39 | 1 | 3 | 0 | -- | [Invasion_of_the_Second-Hand_Shirts_walkthrough](Invasion_of_the_Second-Hand_Shirts_walkthrough.md) |
| `Imagination.taf` | `imagination` | 35 | 1 | 1 | 0 | -- | [Just_My_Imagination_walkthrough](Just_My_Imagination_walkthrough.md) |
| `hyper_b_s.taf` | `hyper_b_s` | 34 | 1 | 2 | 1 | -- | [hyper_b_s_walkthrough](hyper_b_s_walkthrough.md) |
| `Renegade_Brainwave.taf` | `renegade_brainwave` | 25 | 1 | 5 | 3 | -- | [Renegade_Brainwave_walkthrough](Renegade_Brainwave_walkthrough.md) **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `whitterscap.taf` | `whitterscap` | 21 | 1 | 3 | 4 | -- | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `All Hallows Eve.taf` | `allhallowseve` | 16 | 1 | 4 | 0 | yes | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `SRSintro.taf` | `srsintro` | 13 | 1 | 2 | 3 | -- | [SRSintro_walkthrough](SRSintro_walkthrough.md) **done** 2026-09-05 -- clean in run400 (identical on every turn), see "Measured so far" |
| `competition2006__adrift__ptgood__PTGOOD.taf` | `ptgood` | 6 | 1 | 1 | 0 | -- | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `The Plague - Redux.taf` | `plague` | 266 | 0 | 10 | 20 | yes | [The_Plague_Redux_walkthrough](The_Plague_Redux_walkthrough.md) |
| `vetknow.taf` | `vetknow` | 228 | 0 | 15 | 38 | yes | [Veteran_Knowledge_walkthrough](Veteran_Knowledge_walkthrough.md) |
| `TheCellar.taf` | `cellar` | 176 | 0 | 1 | 1 | yes | [TheCellar_walkthrough](TheCellar_walkthrough.md) |
| `mysteryofcaves.taf` | `mysteryofcaves` | 146 | 0 | 6 | 1 | yes | [mysteryofcaves_walkthrough](mysteryofcaves_walkthrough.md) |
| `Space Boy's First Adventure.taf` | `space_boy` | 145 | 0 | 1 | 1 | -- | [Space_Boy_walkthrough](Space_Boy_walkthrough.md) **measured** -- see "Measured so far" |
| `vetknow2.taf` | `vetknow2` | 141 | 0 | 15 | 38 | yes | [Veteran_Knowledge_walkthrough](Veteran_Knowledge_walkthrough.md) |
| `shardsofmemory.taf` | `shardsofmemory` | 122 | 0 | 6 | 5 | yes | [Shards_of_Memory_walkthrough](Shards_of_Memory_walkthrough.md) |
| `man overboard.taf` | `man_overboard` | 99 | 0 | 5 | 0 | yes | [Man_Overboard_walkthrough](Man_Overboard_walkthrough.md) **measured** -- see "Measured so far" |
| `relojero.taf` | `relojero` | 88 | 0 | 0 | 2 | -- | [La_hija_del_relojero_walkthrough](La_hija_del_relojero_walkthrough.md) |
| `salutations.taf` | `salutations` | 88 | 0 | 3 | 2 | yes | [Salutations_walkthrough](Salutations_walkthrough.md) |
| `CBN.taf` | `cbn` | 82 | 0 | 1 | 0 | yes | [The_Revenge_Of_Clueless_Bob_Newbie_walkthrough](The_Revenge_Of_Clueless_Bob_Newbie_walkthrough.md) |
| `forum2.taf` | `forum2` | 82 | 0 | 1 | 0 | yes | [Forum_2_walkthrough](Forum_2_walkthrough.md) |
| `asdfa.taf` | `asdfa` | 80 | 0 | 4 | 0 | yes | [ASDFA_walkthrough](ASDFA_walkthrough.md) |
| `mortality.taf` | `mortality` | 78 | 0 | 4 | 5 | yes | [Mortality_walkthrough](Mortality_walkthrough.md) |
| `princess1.taf` | `princess_in_the_tower` | 78 | 0 | 4 | 1 | -- | [Princess_In_The_Tower_walkthrough](Princess_In_The_Tower_walkthrough.md) **measured** -- see "Measured so far" |
| `Private Eye.taf` | `private_eye` | 74 | 0 | 0 | 0 | yes | [Private_Eye_walkthrough](Private_Eye_walkthrough.md) |
| `AFDFR.taf` | `afdfr` | 73 | 0 | 32 | 17 | yes | [A_Fine_Day_For_Reaping_walkthrough](A_Fine_Day_For_Reaping_walkthrough.md) |
| `chooseyourown.taf` | `chooseyourown` | 72 | 0 | 0 | 0 | yes | [chooseyourown_walkthrough](chooseyourown_walkthrough.md) |
| `hauntedhouse.taf` | `hauntedhouse` | 72 | 0 | 4 | 1 | -- | [The_Haunted_House_of_Hideous_Horror_walkthrough](The_Haunted_House_of_Hideous_Horror_walkthrough.md) **measured** -- see "Measured so far" |
| `valley.taf` | `valley` | 72 | 0 | 6 | 0 | yes | [HappyValley_walkthrough](HappyValley_walkthrough.md) |
| `yak_shaving.taf` | `yak_shaving` | 71 | 0 | 5 | 3 | yes | [Yak_Shaving_walkthrough](Yak_Shaving_walkthrough.md) **measured** -- see "Measured so far" |
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
| `Beanstalk.taf` | `beanstalk` | 49 | 0 | 3 | 1 | -- | -- **measured** -- see "Measured so far" |
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
| `frog.taf` | `frog` | 27 | 0 | 3 | 0 | -- | [The_Green_Princess_walkthrough](The_Green_Princess_walkthrough.md) **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `SPAM.taf` | `spam` | 27 | 0 | 2 | 3 | yes | [SPAM_walkthrough](SPAM_walkthrough.md) **done** 2026-09-05 -- run400 divergence found and FIXED, see "Measured so far" |
| `I am the Law.taf` | `law` | 26 | 0 | 5 | 3 | yes | [IAmTheLaw_walkthrough](IAmTheLaw_walkthrough.md) **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `topaz.taf` | `topaz` | 23 | 0 | 0 | 4 | yes | [Topaz_walkthrough](Topaz_walkthrough.md) **done** 2026-09-05 -- run400 found the exact-empty `InRoomDesc` rule (FIXED), then clean; see "Measured so far" |
| `Wreckage.taf` | `wreckage` | 23 | 0 | 0 | 2 | -- | [Wreckage_walkthrough](Wreckage_walkthrough.md) **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `ARGH_sGreatEscape.taf` | `argh` | 22 | 0 | 0 | 1 | -- | [ARGHs_Great_Escape_walkthrough](ARGHs_Great_Escape_walkthrough.md) **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `ShadricksTravels.taf` | `shadricks_travels` | 22 | 0 | 3 | 0 | -- | **done** 2026-09-05 -- run400 differs on ONE turn, the disambiguation wording (recorded, not ported); see "Measured so far" |
| `1HRGAME.taf` | `masochists_heaven` | 20 | 0 | 0 | 0 | -- | [Masochists_Heaven_walkthrough](Masochists_Heaven_walkthrough.md) **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `Pieces of eden.taf` | `pieces_of_eden` | 20 | 0 | 1 | 3 | -- | [Pieces_of_eden_walkthrough](Pieces_of_eden_walkthrough.md) |
| `longbarrow.taf` | `longbarrow` | 19 | 0 | 0 | 2 | -- | **done** 2026-09-05 -- clean in run400, see "Measured so far" |
| `Vagabond.taf` | `vagabond` | 19 | 0 | 3 | 2 | yes | [Vagabond_walkthrough](Vagabond_walkthrough.md) **done** 2026-09-05 -- run400 differs on ONE turn, the known ALR-over-a-joined-paragraph residual; see "Measured so far" |
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
| `The Foggy Banana Adventure.taf` | `foggybanana` | 8 | 0 | 3 | 1 | -- | -- |
| `The Fly Human.taf` | `flyhuman` | 7 | 0 | 0 | 3 | -- | -- |
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
- 4.0 put/task precedence: a completable library put beats a passing task,
  a size/capacity refusal prints and lets the task follow on one line, the
  implicit take is gated on a mode-1 task pre-match, and the pre-matcher's
  take/put class filter (task bytes 104/105).  See "Ported 2026-09-06".
- The task pre-matcher (`Proc_19_35_453C50`) is restriction-aware: pass one
  wants a runnable task whose restrictions PASS; the fallback wants the
  lowest failing restriction's FailMessage non-empty, or a spent task's
  RepeatText.  A task restricted with no message never pre-matches (House
  task 60, measured 2026-09-06).
- Bracketed References echo removed; the bracket checkbox governed
  "(Getting off the stool first)" and two more lines.

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
- **ALR Originals that span the joined paragraph** (Vagabond room 4,
  thetest): the Runner joins the whole turn into one paragraph so a
  two-sentence Original matches; Scarier sections the NPC line.
- **House's `%drunk%` ALR loop.**  House.taf rewrites "You move" to
  `%drunk%` and the string variable `drunk` is "You move", so every move in
  run400 pops an `evaluate error - Out of stack space` alert (dismissed,
  the turn then prints nothing for the move).  Scarier's bounded expansion
  prints the literal `%drunk% east.` instead.  Measured 2026-09-06.
- **run400 "Which <term>.  <list>?" disambiguation wording** (shadricks
  `climb tree`; 4 goldens, 6 lines).  Unported because the expensive half
  is the two-pass Short/alias narrowing that decides `<term>`; the
  answer-eating follow-up prompt exists only for examine/read/look-in and
  take/drop, so a non-owning verb reads the next line as a command.

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
- **`isare()` vs `obj_appears_plural()`**: the Runner (run380 428EAC) says
  " are " for a plural Short with an EMPTY prefix, Scarier says singular;
  24 call sites, needs a live probe before touching.
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

The wording: the "handled" line names the NPC by Prefix + first Alias, not by
Name -- run400 47F750-47F7BC and run390 45969B-4596C6 print `"I don't think
" & [Prefix & " "] & Alias(0)` when the alias is set (the prefix only when
it is too) and fall back to the Name when the alias is empty; run380 44057D
and run370 4386EE always print `Prefix & " " & Alias(0)`.  Cathy (alias
"girl", no prefix) gets "I don't think girl would appreciate being handled."
Ported in `lib_cmd_take_npc()`.

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
  Cleanest single test of the mode-1 filter.
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



## Next candidates

4.00, by command count: `endgame.taf` (13), `griswold` (18, PRE=4,
`Griswold.taf`), `togetyou` (34, PRE=1), `imagination` (13,
`POPUP_ANSWERS="Jenny"`, `Imagination.taf`), then `confession`, `pyramid`,
`colony`, `marlin_affair`, `villains_and_kings`, `marika`, `second_chance`,
`crimsondetritus`, `goblinhunt` and on up the table.  Feeds for all of these
already exist in `~/adrift-battle/runner/wine/`.  `sommeril` also wants a
re-drive with the corrected `take placemat` feed (`cmdfile_w_sommeril.txt`,
79 lines).  Wine driving needs the Mac's screen unlocked.
