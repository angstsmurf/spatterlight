# TODO: Runner-transcript verification of the v4 walkthroughs

Replay a wired walkthrough command for command in the real Windows ADRIFT
Runner under Wine and diff the Runner's own transcript against Scarier's.
Where they disagree, fix the engine, never the walkthrough. Then re-bless the
golden and write the evidence into the game's row comment in
`harness/run_v4_walkthroughs.sh`. Scope is all four file versions (3.70,
3.80, 3.90, 4.00). Only the Runner binary and the capture flow change with
the version.

**Pruned 2026-09-14 and again 2026-09-19.** This file holds the workflow,
the open leads, the deliberate deviations and a one-line index of every
ported rule. The dated per-port write-ups (probe tables, Runner addresses,
corpus fallout per port) live in git history and in the row comments of
`run_v4_walkthroughs.sh`:

    git show 55dd84ee1:terps/scarier/test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md   # with the 2026-09-14..19 triage write-ups
    git show 72fd5ea08:terps/scarier/test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md   # last full version before the 09-14 prune
    git show 45e20596:terps/scarier/test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md    # before the 2026-09-06 compaction

Comments in `run_v4_walkthroughs.sh` and the probe generators cite sections
by title ("Ported 2026-09-10: the take-from handler's own answers" and so
on). Grep the `72fd5ea08` version for the title. The commit hashes in the
index below lead to the code.

---

## Where things stand (2026-09-19)

- **Goldens:** 429/429.
- **`runner_transcripts/`** holds one Runner transcript per row except
  dreamquest, each driven with the golden's feed, seed and popups. 3.9/4.0
  rows come from the vbrng Runners (`run390x`/`run400x`, xoshiro, seed 1234
  unless the row sets one), so draws compare value for value; 3.7/3.8 rows
  are `.rtf` captures with the last command grafted back from the Runner's
  window. `manifest.tsv` carries each row's compare verdict and
  `compare/<tag>.txt` the report for a row that differs. Regenerate with
  `harness/runner_transcripts.py`; its README explains how, and
  `recompare <tag>` refreshes one row after an engine change.
- **Manifest:** 364 identical on every turn, 29 identical apart from
  whitespace, 34 with a compare report. Every differing row is classified
  under "Open leads". A row that "lost" feed commands is either an ending
  both sides share (the keypress wait eats the rest of the feed, no text is
  missing) or one of the harness leads listed there.
- **Which transcript to cite.** For a wired row, compare against and cite
  `runner_transcripts/<tag>.txt` (`.rtf` for 3.7/3.8), never the
  `Adrift_<N>_<tag>` file a drive left in the prefix. The manifest's `source`
  column records which archive file each copy came from; a winning re-drive
  replaces the copy through `collect`, so the notes never need a new
  filename. Only runs with no row keep their archive names: probes and
  one-off variants such as House_sober.
- **Older archives** under `~/adrift-battle/runner/wine/`
  (`transcripts_v4_corpus_2026-09-08/`, native RNG;
  `transcripts_v4_xoshiro_2026-09-12/`, 44 rows) predate the whole-corpus
  capture and hold no engine lead (triaged 2026-09-14). `pfx/drive_c/adrift/`
  is the live archive where every drive lands: never `rm` a glob there.

---

## Workflow

### 1. Pick and screen a row

- **The game must be deterministic along the route.** A roll only matters
  if its text can reach a room the route visits while the event runs. Rolls
  exclude the upper bound, so `start=1..2` always draws 1. With the vbrng
  Runners this now matters much less, because both engines draw the same
  stream.
- **`harness/screen_wine_candidate.py <taf>`** reports:
  - the version, from header bytes 8-10 (`E7a` = 3.90, `E>a` = 4.00);
  - the real command count;
  - events and which of them can roll;
  - NPCs and walkers;
  - silent typeable tasks.
- **`SCR_DUMP_TASKS=1 harness/scare <game>`** dumps tasks, events and walks
  to stderr, and fires on the first task check. A keypress-gated intro
  swallows a bare `look`, so feed the solution file instead.
- **Which Runner.** The binaries are `run370`, `run380`, `run390` and
  `run400`, plus `run*x.exe` for the vbrng builds. Each file version needs
  its own Runner: run400 refuses a 3.90 file, and run380 refuses 3.70. The
  game must sit in `pfx/drive_c/adrift/` with no spaces in its filename.

### 2. Build the feed

- **`harness/make_wine_cmdfile.py`** builds the feed from the golden:
  - It strips comments.
  - It reads the startup waitkeys that `SCR_MARK_WAITKEY=1` measured into
    `PRE`.
  - It emits the `#sleep` and pause markers for every span, including the
    span after the last command.
- **Empty solution lines are commands**, not pauses. The Runner answers
  them "Huh?" or similar.
- **Name and gender prompts are not feed lines.** Pass them as
  `POPUP_ANSWERS="Hero|male"` on the drive and as `--popup` on the compare.
- **Versions split on whether they ask:**
  - run400 always asks.
  - run390 asks only while PlayerName is blank.
  - 3.7/3.8 never ask.
- **Randomised puzzle state** (humbug's dials, keypad codes) is read off a
  transcript and spliced in at a `#save`/`#restore` checkpoint.

### 3. Drive the Runner

- **`fast.sh <game.taf> <cmdfile> [exe] [pre]`** drives one row, and
  **`par.sh <jobfile> [maxpar]`** drives several. Both use `drv/drive.exe`,
  which posts Win32 messages into the prefix, so no foreground is needed.
- **Settings and parallelism:**
  - Both force Verbose ON and all five Appearance checkboxes ON, which is
    required.
  - Run at most 3-5 Runners at once.
  - A job that fails leaves a 0-byte transcript. Check the size before
    believing a row that "lost every command".
- **`measure.sh` and `measure38.sh`** (keystrokes, foreground) remain only
  for cross-checks.
- **vbrng:**
  - Set `VBRNG=xoshiro`, optionally with `VBRNG_SEED` (default 1234).
  - Set `VBRNG_TRACE='C:\adrift\x.txt'` (a Windows path).
  - Pass `run400x.exe` / `run390x.exe`.
  - The drive for a row is `xoshiro_par.sh`, whose 7th field is the popup
    answers.
  - `drive.exe --temp` gives each run a private TMP. Without it, run400
    454874 redraws media temp names at load.
- **Reading the vbrng trace:**
  - It appends. Isolate a run by its second `TURN 0`.
  - `TURN n` is written after command n ran.
  - `vbRND(Missing)` lines are the .taf codec and are not draws.
- **3.7/3.8 have no live transcript.**
  - Save Transcript writes an `.rtf`, and par.sh names it
    `Adrift_<N>_<tag>.rtf`.
  - The last command is missing from it. Graft it back from the window --
    see `DUMP_SCROLLBACK` below.
  - A death or end-game modal wipes the scrollback.
  - `£` comes out as `Â£`.
- **`DUMP_SCROLLBACK=<file>`** saves the RichTextBox's own text
  (`WM_GETTEXT`), which is the window itself, exactly, and needs no
  screenshot or OCR. Use it to prove a line break is a transcript artefact,
  and to recover what a transcript file could not hold:
  `dump_par.sh` drives a whole job file that way and
  `harness/graft_scrollback_tail.py` appends the difference to the archived
  transcript. It closed all 19 3.7/3.8 rows on 2026-09-17 and recovered
  nothing on 3.9/4.0, whose transcript is written live -- there the
  end-of-feed losses are the keypress wait eating the command, with no text
  behind them. `runner_transcripts/README.md`, "Grafted tails".
- **Kill Wine with `pkill -9 -f wine; pkill -f wineserver`.** fast.sh and
  par.sh also reap the orphaned `winedevice.exe` processes. The prefix is
  shared mutable state: never drive it from two sessions at once.

### 4. Compare

    python3 harness/compare_wine_transcript.py --taf G --feed F --runner T \
        --env SCR_RNG=xoshiro --env SCR_SEED=1234 [--popup Hero --popup male]

- **For a wired row, T is `runner_transcripts/<tag>.txt`** (see "Which
  transcript to cite" above). `python3 harness/runner_transcripts.py
  recompare <tag>` runs this compare with the row's own feed, seed, env and
  popups. It rewrites `runner_transcripts/compare/<tag>.txt` and the
  manifest verdict. Use it after an engine change instead of hand-built
  arguments.

- **Rule 2 comes first.** It prints every feed command the Runner never
  echoed. A lost command desynchronises everything after it and is a
  harness problem.
- **Then it diffs the aligned turns**, whitespace-normalised. The aligner
  can present a single text difference as a ±1 resync (house T263).
- **The compare does not apply the row's env.** Pass the row's `SCR_*`
  settings yourself.
- **For draw parity, compare without `SCR_SKIP_WAITKEY`** when the feed
  answers a "press enter" with a blank line.
- **Draw counts:** count the `RND #` lines in the Runner trace against
  `SCR_TRACE_RAND=1` on Scarier's stderr. Merging with `2>&1` can glue a
  `RND #` onto unterminated text, so count unanchored.
- **`harness/sweep_wine_turns.py`** runs the compare over every archived
  row. Use `--only <tag>` for one row, `--limit N` for example turns, and
  `--lost` for rows that lost a command.
- **`harness/sweep_wine_breaks.py`** covers line structure. Runner-only
  breaks are ground truth and are currently at 0.
- **`harness/sweep_v4_corpus.py`** covers a manifest-driven capture;
  `--tsv` clusters its output.
- **Rebuild first** with `sh harness/build.sh`. A stale `harness/scare`
  makes every table lie. Compare sweep totals only against a baseline from
  the same day and the same transcript set.

### 5. Classify each differing turn

1. **Capture artefact, name it and ignore it:**
   - the Runner's `[Press any key to end]` tail, only where Scarier has no
     tail of its own: since 2026-09-17 the engine buffers the same prompt
     (`task_print_end_keyprompt()`, headless harness opt-in), so an ending
     turn normally matches outright; what is left is the "just stop" ending,
     which run400 leaves promptless (45E11F). Blank-line counts around it
     are never a difference (the comparison collapses whitespace, the
     goldens run through `cat -s`); the 9 rows one blank short there are
     endings whose text carries its own trailing `<br>`, which the print
     filter collapses against the block terminator it adds;
   - a startup echo: the Runner answers a feed's leading blank lines with
     "I don't understand what you mean." inside turn 0 (Glum_Fiddle);
   - a `<centre>` join (the tag converter drops alignment breaks);
   - a `<waitkey>` line join;
   - a `<waitkey><cls>` butt-join;
   - a wrap inside an unbreakable token (Renuntio's asterisks);
   - `[MORE]` splits;
   - `.rtf` mojibake;
   - the epilogue cut at the final keypress;
   - rule-2 "lost" lines after an identical ending.
2. **Harness.** Suspect these first:
   - Verbose OFF;
   - popup answers missing;
   - a startup pause that offset the streams (drop that many leading
     lines);
   - `[Y/N]` answers that Scarier asks but the Runner never does
     (`hint`, `quit`);
   - a `#save` in the compare. The echoed save is a turn in run390 but is
     dropped by the compare, so the Scarier side runs one tick out per save.
3. **RNG.**
   - With xoshiro, the draw counts decide: equal counts mean any value
     difference is real.
   - On a native-RNG capture, re-run Scarier under 2-3 `SCR_SEED`s and
     watch the line move.
   - A menu chosen by `ACT type=3 v2=2` is seed-locked (mould), so a clean
     feed is still not a comparable run.
4. **Engine.**
   - Find the Runner's code path. Decompiles are `run400.bas`,
     `run390_3.bas` and `run370.bas`. Always confirm against the
     `.p32dasm.txt`: the `.bas` drops statements, and `push &HFF 'Byte` is
     -1. `~/Adrift_decompile`'s `find.py -v runNNN ADDR` resolves addresses.
   - Settle the version gate with the string census: which exe's UTF-16
     pool holds the literal. Find the use site, because near-miss literals
     are menu captions.
   - Measure with a probe if the corpus does not isolate the rule.
   - Port it behind a `TAF_VERSION` gate and run the suite.
   - Re-bless with `--bless <substr>`.
   - Record the measurement in the row comment and a line in the index
     below.
   - Add the Runner addresses to `~/Adrift_decompile/index/annotations.tsv`,
     then run `sh index/refresh.sh`.

### Probes and offline oracles

- **4.00 probes:**
  - `harness/make_400_*probe.py` and `make_arena_probe.py`, packed with
    `taftool.py` against a donor .taf.
  - Build each probe with a `probe` task that prints `PROBE OK.` first and
    last, so a shifted command shows up.
  - Separate cells with `look`. A pending ambiguity answer slot eats the
    next cell otherwise.
- **3.90 probes:** `harness/make_39_*probe.py` writes the schema directly.
  The generators only convert upward.
- **3.70/3.80 probes:**
  - `make_37_/make_38_darkprobe.py`, `make_3738_examprobe.py` and
    `make_surfprobe.py`, all hand-authored.
  - A hand-built .taf must parse to exact EOF in `harness/scare`.
  - Every version needs its own probe file: run400 says `Incorrect
    version`.
- **Probe files:** the .taf files stay untracked; the generator is the
  artefact.
- **Other oracles:**
  - the corpus's ALR *Original* strings (an author only rewrites what the
    Runner printed);
  - the exe string pools;
  - `~/Adrift_decompile`.

---

## Open leads

None of these blocks a golden. Every closed item's write-up is in git
history (`55dd84ee1`) and in its row comment; the ported rules are in the
index below. Rows not named here differ only by a capture artefact (see
"Nothing owed").

### Engine, 4.0

- **cellar T120 `undo`:** the Runner's "Undone." replays the restored turn's
  story text (with its `[MORE]`); Scarier's replays a not-understood line.
  Probably the known undo-slot deviation (Scarier skips administrative
  lines), not confirmed.

### RNG or draw placement (draw counts owed)

Each needs the `RND #` count against `SCR_TRACE_RAND` before it can be
called real; equal counts with differing text mean a value or candidate
rule, not RNG (deaths, 2026-09-19).

- hhorror (seed 50): T25 dark text, then the zombie's attack roll at
  T38/T134/T142.
- alchemist (3.90): from T303 (`give rose to king`), unread.
- reluctantvampire (seed 6): the `[Press a key, <epithet>]` roll is one draw
  apart from T132; the Runner's T133 is Scarier's T132.
- warlord (seed 6) T288 `push barrel`: the barrel rolls west in the Runner
  and northwest in Scarier; 66 lost from feed[318] follow.
- marooned (3.80, seed 3) T53 `throw map`: the Runner has no shark.
- Battle rolls: cyber2 T15/T26; alexis T126-127 (companion strike order,
  Haron's arrival one turn apart).

### Engine, 3.9

- **Refusal wording:**
  - thetest_win T68-77 `unlock door`: "You can't do that here!" against
    "You can't unlock the door.".
  - alexis T99 `open chest`: "Command not understood" against "You can't
    open that.".
  - cybercow T62 `put bones in robot`: "You can't do that!" against
    Scarier's "You're not holding the little bones to install them...".
- **cybercow_win:**
  - T72 `x fairy`: a different description (state);
  - T97 `read envelope`: the Runner adds "The envelope is closed.";
  - T103: "CyberCow is here." against "CyberCow and Robot are here.";
  - T118 `x berry`: the Runner says "I can tell you nothing about that.",
    Scarier asks "Which berry.". The held berry came from task 170, and
    run390's task executor never stamps a seen byte for an object handed to
    a character (index, "Task move-object"), so run390 counts one seen berry.
    Dropping the move-action stamp below 4.0 wholesale regressed ~50 rows
    (2026-09-14); the 2026-09-19 per-destination port did not move this
    row, so something else keeps Scarier's berry seen.
- **fantasyworld T224-296:** the Royal Knight follows the player in the
  Runner. In Scarier he is not in the room.
- **alexis_worn_cube:** T124 has an event line one turn earlier in the
  Runner, and 34 `attack urgorn` are lost after an earlier end.
- **everything T38 `read diary`:** the run390 silent-task DontUnderstand
  (deliberate deviation); the ending at T39 is identical.

### Engine, 3.7 / 3.8

- **haunt (3.80) T84 `down`, the winning move:** both sides print "You hear
  the chiming of the grandfather clock." The Runner puts it LAST, after the
  ending block and `[Press any key to end]`; Scarier puts it in the turn
  after "Horace lurches in from above.". Suspect the capture first: text
  after the keypress prompt is what a timer tick during the ending's wait
  looks like, and light_up's dump ends with two event paragraphs past its
  own prompt the same way. A probe wants a 3.80 game with a short-period
  event and a task that ends the game on a known turn.

### Harness and compare

- **light_up (seed 133), 203 lost from feed[294] (217 before the 2026-09-19 trim):** at T293 `west` both
  sides of the compare die ("scored 58 out of the maximum 0"). The blessed
  golden never dies there and has no `maximum` line, so the compare's
  rebuilt feed does not reproduce the golden's route. Diff the feed against
  the solution (pause blanks) before reading anything into it.
- **showtime from T65:** the feed's blank pause answers become extra turns in
  Scarier under SCR_SKIP_WAITKEY, so turn numbers slip against
  `runner_transcripts/showtime.txt` from there on. Not the engine.
- **wonderwombat, yonastoundingcastle:** the Runner joins `> w` onto the line
  after a pause-answer blank, which RULE 2 reads as lost commands
  (wonderwombat "124 lost" from T116); `read_feed` misclassifies yonas's
  pause blanks. Both rows are turn-identical when compared with blank lines
  stripped.
- **motion:** the minigame's keypresses are its turns.
  `runner_transcripts/motion.txt` echoes all 351 feed commands and, apart
  from whitespace, differs at T257-258 and T350 (the drive minigame's map).
  Unread.
- **house** is comparable only as `House_sober.taf` with
  `cmdfile_house_sober.txt` (identical on every turn; no row, so it exists
  only as `Adrift_128_housesober.txt` in the prefix). The original row is
  10+ turns blank on the Runner side from T1 because of the `%drunk%` ALR
  stack overflow (a deliberate deviation).
- **mould (seed 1):** the Runner's pauses eat no line, so the feed's
  throwaway `1` enters the imp fight on both sides and neither reaches the
  win; a feed with that line dropped would. Otherwise only the `hint`
  deviation and pause joins differ (58 = 58).
- **gmylm** (15 MB .taf, ~400 MB draw trace at load) needs `LOAD_SLEEP=600`
  to get past drive.exe's 25 s load cap.
- **Permanently unmeasurable:** `dreamquest` (run400 cannot load a task with
  an empty Command vector); the `to_hell_and_beyond` assisted rows
  (Scarier-only by design).

### Nothing owed (capture and compare artefacts)

- Epilogue or pause text landing one turn late, or cut at the final
  keypress: endgame T9, mortality T41-48,
  iqsfot T41-42.
- Whitespace-only joins or the trailing `[Press any key to end]`; the
  28 "apart from whitespace" rows. Glum_Fiddle turn 0 is the startup echo
  above.
- Lost commands after an ending: thelasthour's last `wait` only. That row
  runs without SCR_SKIP_WAITKEY, so one ending "Press a key." swallows a
  solution line in Scarier and the extra `wait` makes up for it; the Wine
  feed answers the pause with its own blank line. The other rows' trailing
  padding was trimmed and re-driven 2026-09-19 (1cc5dbc55; the kept lines
  the Runner needs are in the row comments).
- lifesimulation T6 `turn off tv`, life `piss` and the_hangover are the
  silent-task deviation;
  alices_restaurant the run370 double matcher pass; sandy_meta_number the
  SCARE meta-commands.
- Load failures left from the 09-06/09-07 corpus batches: six rows raised
  `evaluate error - Subscript out of range` mid-game; TheADRIFTProject
  crashed with run-time error 401 at command 92. (The 3.90 games that
  "failed to load" had been fed to run400; all load on run390x.)

### Engine, needs a probe (4.0)

- **Silent put confirmation for object #1.** run400 prints no `put`
  confirmation when the moved object is dynamic object #1 (Adrift_82-87);
  run390 prints one. The listing does not single out index 0 (name_object
  46E23C/46E3FA, insides 46639C); next is a live trace of var_A4/var_A6. No
  corpus row touches it.
- **name_object's own list loops (46E04E / 46E0B2)** are not rebuilt. Six
  cells, wording off the Runner.
- **`put box in box`:** run400 announces the take and prints nothing, where
  Scarier says "can't put an object inside itself!".
- **Put resolution ordering:** the 4.0 put fragment fallback scores every
  present object in one pass, where 463640 mode 2 tries held objects first
  (index, "Put and take-from", hub T79). The held-first pass is ported
  for plain `drop` (`lib_drop_resolve_400`), not for `put`; a held/loose
  tie on a put line would differ.
  Also unread: why the seen-gated %text% parse missed TheADRIFTProject's
  already-seen battery (`put battery in remote`), which the fallback covers.
- **Scope.** None of these is measured:
  - the never-seen "You can't see that." branch at 471995;
  - the two-pass `%object%` scope filter proper (`SCR_TRACE_SCOPE`); its
    seen gate is ported, its present-before-absent binding order is not;
  - the NPC seen gate for `%character%` (xfiles `look up byers`).
- **Second-noun ambiguity:** the wording of an instrument ambiguity
  (sswhore `unlock drawer with key`); a tie inside either half of a " with "
  split; lock/unlock with a Key whose left half resolves to nothing; absent
  lock/unlock where the object really is locked.
- **Ambiguity prompts:** co()'s crowded arm (454454) and its &HFE/&HFF
  answers; "That wasn't one of the options!" has never been triggered;
  whether an object ambiguity on a task-answered line also suppresses the
  tick.
- **Events:** an event with RestartType=2, an immediate starter and a
  non-zero length fires once in run400, but Scarier re-arms it. Corpus
  exposure is zero.
- **Put row corners:** `(Taking X first)` ahead of a closed-container
  refusal; `put all in <the container, held alone>`.
- **Output filter:** where the ALR pass sees trailing spaces; the NewParse
  `%` pattern binary path; the drop rebuild at 46F33B.
- **Drop/take/wear setter branches** 46FB7D, 47C7F1 (and run390's wears at
  43D289) have been read but not measured.
- **The run400 loader's "Anonymous" fill** for an empty PlayerName with
  PromptName off.
- **Silent-task test scope, the unported rest.** run400 tests the whole turn
  buffer. Scarier counts anything a task's run adds (baroo) but still
  ignores text written before the dispatch. No corpus row is known.
- **Turn sectioning, the unported rest.** run400 builds the turn as one
  string joined with pspace() and runs the ALR pass over it. Scarier joins
  only the room block and, at 4.0, an executed task's text plus every
  AdditionalMessage (`pf_buffer_join_line`). Everything else is its own
  section, so an ALR Original spanning a join does not match. Unfinished,
  not policy:
  - an event's text after a task's text (p4SRC `xray`: run400 prints
    "X.  EV qball." on one line); measure first;
  - the rest of the turn (thetest, a two-sentence Original);
  - expect a large reblessing: the task-text join alone moved 94 rows (92
    whitespace only), and sweep_wine_breaks still counts 5622 Scarier-only
    breaks.

### Engine, needs a probe (3.9)

- **`give <x> to <y>`** belongs in characters(), below the room refusal
  (the_hangover t42). Only the 3.7/3.8 give is ported (f83e1cf87).
- **"Please be more clear, who do you want to <verb>?"** is a SCARE
  invention (alexis_worn_cube t79). The `who` form is unmeasured; the object
  form is the co() prompt (cybercow_win T118, above).
- **Per-verb absent-NPC branches:** `talk to <npc>` elsewhere (hcw,
  alchemist); `give obj to npc` elsewhere; run390's take `is not here!` at
  4596E1 (Battle System off); kill/kick/punch lines go through other grammar
  first. The attack branch is ported.
- **run390's Who-prefix consumption** at 460022 is assumed, not measured.
- **Take-from:** the " and " clause picks the last container in 3.9 and the
  first in 4.0; 3.9's " and " collection bug; the pending slot after `Get X
  from what?`; surface-vs-container wording of the parent-derivation arm.
- **Put:** the put parser at 461769 ("can't put anything inside/on that!");
  "onto"; a locked container; a named static; an object on a floor
  supporter; `put all in <nothing>`.
- **Two-object canonical prefixed retry:** the run390 half is not
  re-measured. The 4.0 half is closed.
- **Examine:** run390's examine state line has not been read. Ask-topic
  overwrite: run390 lets the last matching topic win, Scarier keeps the
  first.
- **Events, pre-4.0:** the rolls-0 restart rule is ported at 4.0 (index,
  "Events and RNG"); run390/run380's finish test and restart store are
  unread, so pre-4.0 still finishes such an event on its next tick.

### Engine, needs a probe (3.7 / 3.8)

- **Handlers other than take** were measured on single matches only.
- **run380's post-take-from task sweep** after a *refused* take-from is not
  ported.
- **Examine:** 3.7/3.8 examines have no bare-verb exit (unmeasured corners).
  run370's sit/stand/lie has no location test (42AEC8, unported).
- **Ask topics:** a topic whose Task gate picks an empty AltReply is
  unmeasured (substring matching with the last match winning is ported).
- **Put:** 3.7 static container `open`; 3.7 bare take from a held
  container; `put X in Y` where X names nothing and Y is a bad container; a
  supporter that is neither held nor static nor a container; a static
  container; `except` forms; `put all in <nothing>`.
- **Pre-4.0 room-name alt walk** is unmeasured.
- **run380 442F5D** (the catch-all speaks for the first present, seen
  object) is unmeasured.
- **`clear`** at run370/380/390 is unmeasured.

---

## Deliberate deviations (measured, not ported)

- **Pre-4.0 silent-turn DontUnderstand.** A matched task whose turn prints
  nothing gets "I don't understand what you mean!" in run390. Scarier runs
  the task and falls through to the library.
  - Cases: hangover `open the filing cabinet`, everything `read diary`,
    lifesimulation `turn off tv`, life `piss` in the bathroom (Scarier's
    library swearing row answers it; see the row comment. The stats at T31
    follow).
  - The world state agrees in all three. The *spent*-task half is ported
    (cc7470bf8).
- **run370 double matcher pass** (arlo `get out of bus`).
- **SCARE meta-commands** `wait N`, `hist N` and `redo N` exist in no
  Runner. Eleven more inventions are compiled out by
  `SCARIER_NO_ABBREVIATIONS`.
- **Not ported (policy):**
  - `both` right after a `Which <term>. <list>?` prompt. 3.9/4.0 re-run
    the saved candidate list ("the red ball or the blue ball?") as the
    typed line (run400 loc_48AE94, run390 loc_43B4D5), which is unmeasured.
    A bare `both` with no prompt before it is measured and already ours:
    "I don't understand.", two turns (p39WITH T25);
  - the battle-disabled `status`/`statusline` fallback;
  - the help/about/time/version texts;
  - `turns`/`version` version gates.
- **Empty-Prefix double space** in object listings.
- **ALR stack overflows.** House's `%drunk%` ALR loop and a mutual `A -> B`
  / `B -> A` pair overflow the stack in run400, which then prints nothing.
  Scarier's depth cap prints the intended line.
- **run400 `put all on <supporter in the room>`** with two or more items
  runs out of stack and moves only the first item. Scarier completes the
  move.
- **Undo slots:** Scarier skips administrative lines, which the Runner
  records.
- **`NPCWalkAlert`:** a synthesized task pair with no run400 counterpart.
  It anticipates the ticker's restart by a tick, and nothing depends on it.
- **mould `hint`:** run400 has no interactive hints.

---

## Rules measured and ported (index)

One line each, with the game or probe that found the rule and the commit
that ported it. Version gates are in brackets: `[4.0]` means 4.00 only,
`[3.9+]` means 3.90 and 4.00, `[<3.9]` means 3.70/3.80. No bracket means
every Runner.

### Parser and dispatch

- **Task matching:**
  - One game task per typed line; a silent task lets the library run, never
    a second task. House (c74b1b90c).
  - "Silent" means the turn buffer did not grow while the task ran, so an
    event its execute-task action starts speaks for it (run390 tasks()
    42BDAD, run400 44CCC0). `[3.9+]` baroo T107 (`run_task_run_speaks`,
    2026-09-14)
  - 4.0 task matching is verb-literal. `*`, `[..]` and `{..}` patterns
    compare binary. A rebuilt line keeps its capitals, so a pre-match can
    fail to dispatch and fall to DontUnderstand. `[4.0]` hcw T162
    (b6d2f4f1f, 5fc9ef8d1)
  - The task pre-matcher is restriction-aware. The fallback pass wants a
    FailMessage or a spent RepeatText, and a fallback hit is silent. `[4.0]`
    House task 60 (c38297f1e, 5fc9ef8d1)
  - The pre-matcher's fallback reads a per-task restriction cache (one T/F
    per restriction, task record +100). The cache is written only when a
    walk with arg_10=1 FAILS (455BEE/455C3B) and is never cleared. Those
    walks are task_pick's, BEFORE the pattern match, on every eligible task
    up to the first that passes and matches (454DDF); the fallback's own
    walk when the cache is empty (453F2D); and dispatch-by-index (45FA02).
    The fallback re-checks each cached 'F' alone (453F7D). One still
    failing with a FailMessage is a hit (453FA1), and a clean scan hits on
    a RepeatText (453FE2). So a stale cache can claim a line silently, and
    get_piece then answers DontUnderstand. `[4.0]` 3monkeys T41 `get husk`
    after `hit coconut` (`restr_cache_fallback`, 2026-09-19)
  - A trailing space in an all-literal task command must be typed. sommeril
    `get placemat ` (093a12d5e)
  - The SYNONYM table is sequential whole-string rewrites. Vardock
  - The `*` matcher (457D68) does not backtrack. Each literal piece is found
    by the first InStr and the line is cut past it. A space goes back on only
    when the rest of the pattern starts with one, and a line gets a leading
    space only for a pattern starting "* ". So `buy *** *rawhide armor*`
    misses `buy rawhide armor`, and ` *Buzzer*` misses `buzzer`. run390's
    checkwild (4346A8) never cuts the line. `[4.0]` the_town_of_azra T13,
    xfiles T69 (`uip_wildcard_match_400`, 2026-09-15)
  - The already-done scan passes over a spent task with no RepeatText, so a
    later spent task's RepeatText answers the line. `[4.0]` crookedestate T41
    (`run_task_refusal`, 2026-09-15)
  - A task command's %character% binds the FIRST matching NPC in index order
    (run400 468DFC leaves for 469574 on the first hit). `[4.0]` iqsfot T158
    (`uip_match_entity`, 2026-09-15)
  - characters()' take arm (47F70B) overwrites an object take's answer with
    "I don't think <NPC> would appreciate being handled." when the line has
    take/get/pick up and names a present NPC; the take stands. `[4.0]` onnafa
    T155 (`lib_take_npc_overwrite_400`, 2026-09-15)
  - A 3.7/3.8 task's Obj2 location restriction reads Obj2 - 1 as a raw index
    into the whole object table (run380 tasks() 44CA87, run370 441867), and
    the Runner's put stores the container's object index as parent (run380
    446166). An Obj2 naming a static object therefore always fails, where the
    dynamic-index conversion had moved the test onto the preceding dynamic
    object. Only twilight's corpus task 59 (Obj2 57 = the stove) has one.
    `[3.7/3.8]` twilight T48, 500 -> 485 (`parse_fixup_v380_objstate_restr`,
    2026-09-15)
- **Word rules:**
  - `take` becomes `get` before parsing. `[3.8]` great
  - `z` means wait only from 3.90. cave
  - The repeat words `again`/`last`/`previous` are tested on the whole line
    before any task, and `g` joins them from 3.90 (run390 45F094, run400
    89FE2). shadowpeak TASK 404's riddle `g` repeats the last command.
    (`run_is_repeat_word`, 2026-09-14)
- **Line splitting:**
  - The splitter cuts at `,`, `. `, ` and ` and ` then ``. It is suppressed
    when the tail starts with any object's Short, Prefix word or Alias
    (case-sensitive). The put clause loop puts several objects in one turn.
    `[4.0]` p4AND (dffce55df)
- **Spent tasks:**
  - Pre-4.0 checktask 44B4DD writes a spent task's RepeatText to the buffer
    and keeps scanning. A later passing task still runs, and a later
    restriction's message replaces the text. `[<4.0]` journ2, vampire,
    merry_murders (cc7470bf8)
  - A spent task's RepeatText outranks the library. The dispatcher at
    48A481 sits mid-library. `[4.0]` p4REPEAT (4e53b89ce)
  - The post-library RepeatText fallback also checks restrictions. `[4.0]`
    hcw T227 (b6d2f4f1f)
- **The task-ran NPC gate.** Once a task has run for the line, the
  character handler's take, examine, where, attack and talk-to branches are
  shut; give, ask-about and kiss survive. The take-NPC line names Prefix +
  Alias(0). `[3.9+]` House (c32748d14)
- **Give and ask:**
  - Give works in any word order. House (1c0d8c6b9)
  - The ask/talk block splits on `about`; only the branch without it prints
    the hint. thelasthour
  - The character pronouns have no-antecedent seeds: "No male" / "No
    female" at 3.9+, "Nobody" below. showtime
  - A 3.9 topic reply overwrites the task text. `[3.9]` zombies, ms_mobius,
    alchemist (8c4d3260b)
- **Case handling.** Every Runner lower-cases input, but the character
  resolver's tail is case-sensitive, so a SYNONYM that carries a capital
  makes its NPC unreferenceable. bandera (0390eb300)
- **Question prefixes.** "Wear what?", "Remove what?", the give prefix,
  checkverb on a bare verb and "...with?" store the line as a prefix for
  the next input. The "with?" line is not a turn. `[4.0]` (daf951a81)
- **The " with " split.** therest resolves both halves: "don't have", "Don't
  be daft!", or a " with <X>" suffix on about thirty verb refusals. The
  corners are open/close/read/fix/clear with and take looking up only
  `get <name>`. `[4.0]`; run390 twin `lib_with_clause_390` (daf951a81,
  b526c013b)
- **Line endings:**
  - A task that ends the game takes the unhandled-verb tail off the line.
    `[4.0]` relojero, easter (f038d76bf)
  - It takes ALL of therest off, not just that tail: 48AC62 jumps past the
    call at 48AFE4, so the "You can't <verb> X" arms go too and an empty
    buffer prints DontUnderstand; only the catch-all subset is kept.
    `[4.0]` iachini T185 (STANDARD_ENDED_FALLBACK_COMMANDS, ab85a3e4e)
  - CompleteText is tested raw: a task text of just spaces counts as output,
    so the line is handled and no DontUnderstand follows. `[4.0]` wumpusrun
    T10, probes Adrift_128_wumpA..D (task_run_task_unrestricted, ab85a3e4e)
  - The catch-all tests the line-top object's presence after the task, and
    that answer is a turn. `[4.0]` seaside `do form` (24dcc8e5a)
  - A line a task answered that names a term two present NPCs share is not
    a turn. `[4.0]` sun_empire, salutations (8c4d3260b)
- **Administrative turns and the counter:**
  - An NPC examine and a nothing-found examine are administrative turns;
    so is `read` via examines. `[4.0]` EV14-16, house T150
  - Only examines' "see no such thing" (471F02) sets the flag. characters()'
    NPC arm sets none. So `x <npc>` whose line has a unique seen but absent
    object as the winner is a turn. `[4.0]` humbug T634 `X robot`
  - In run390 hint, help, clear, time, version, save, restore and undo are
    ordinary turns, and the counter counts every line element, so `both`
    counts twice. `[3.9]` p39ADMIN (a211db2f1, b526c013b)
  - There are no administrative turns and no startup tick below 3.9.
- **The room refusal** runs inside the library, ahead of therest. `[3.9]`
  (9fbb40881) Before 3.9, drop, put and give refuse ahead of it. `[<3.9]`
  cave, greatc (f83e1cf87)
- **run380's post-take-from task sweep.** tra `get meat` also runs `get
  *knives*`. `[3.8]` (4f79695e4)
- **The player-name prompt** splits by version (see Workflow step 2).
  (39bfe4cec)
- **Meta commands:**
  - `stats` is in no Runner; `status` exists only inside 3.9/4.0 dobattle.
    suburbanprodigy3
  - 3.9's `status` is three tab-joined rows (Stamina, Hit strength, Defense
    value, each `value (max)`), not the 4.0 table. `[3.9]` the_town_of_azra
    (`lib_print_battle_status_390`, 2026-09-14)
  - The Runner's `past`, `bye`/`end`, `endgame`, control panel and quit
    decline text are ported (6033124f5).
  - `undo` answers by version: none at 3.7, "I can't undo your
    blundering." at 3.8, "Undone." / "I can't undo any more of your
    blunderings!" at 3.9/4.0 (6314d19d4). At 3.9+ it replays the restored
    turn's output (1c834df1b).

### Nouns, scope and the seen model

- **Nothing is referenceable until something lists it.**
  - The loader seeds the seen byte (4909B5), and afteroa sweeps it
    (statics only at 3.9).
  - A unique absent-seen winner answers "can't see X from here!" and ticks.
    A tie or no winner answers "see no such thing" and does not tick.
    `[3.9+]` (386c9c570)
  - A part-of-character static is stamped seen by obhere (452E08/452E5E)
    when its holder is the player, or a seen NPC in the player room. 463640
    calls obhere on every object once per line. `[4.0]` humbug T727
    `X teeth` (an older drive's "Nothing Special." was a desync)
- **463640 is the 4.0 noun resolver.**
  - Scoring: Short whole word +1, first alias +1, +1 per Prefix word, and an
    empty Prefix counts as `a`.
  - With two objects named, a tie gives the game's DontUnderstand.
  - NPCs are never candidates. `[4.0]` House throw (0743cefef)
  - therest's absent-seen clause scores every object the line names.
    `[4.0]` warlord, house doors (5662e7397)
  - Mode 2 (a plain `drop X`, put_drop_list -> name_object) scores held
    objects first, then everything present; the best score carries over.
    A tie prompts `Which <term>.` only when Me(424) (MemVar_4941EC) is set,
    and that is set by comparing each tied object's Short with the object
    TWO indexes past the previous tie (Abs(-(k+2)), 4633F0), else "It is
    not clear which <term> ..." (46E192). `[4.0]` wilkins T110-T117
    (`lib_drop_resolve_400`, 2026-09-19)
- **4.0 named take:**
  - The take falls back on every seen object: "nothing worth taking here" /
    "not clear which". `[4.0]` p4TAKE (7d051a7d7)
  - The take runs the tasks' `get <the object>` before both refusals.
    `[4.0]` icecream T0 (8f2898bd4)
  - A refused take leaves the typed line (take becoming get) to the task
    dispatcher. `[4.0]` (3eefa06b2)
- **Auto-"from" take.** 4.0 retakes a seen object "from" its holder, and
  the referenced object is cleared before each typed line. A type-1
  Var1=0 restriction is then silent. `[4.0]` warlord T104 (68bc1382a)
- **Examine:**
  - referencedob answers a tie with an absent, unseen object: "You can't see
    that." `[4.0]` warlord T72/T76 (b6d2f4f1f)
  - `x <character elsewhere>` answers "You cannot see <Name> from here." In
    4.0 it also needs the NPC's seen byte. (233e1b9c8)
  - The object-ambiguity prompt is `Which <term>.  <NP> or <NP>?`: examine
    prompts on a Short-or-Alias tie, an unhandled verb only on a Short tie.
    It has a pending answer slot. `[4.0]` p4CO (5d90e8793)
  - 3.7-3.9 co() has its own prompt, which replaces the output while the
    action still happens. `[<4.0]` mikes; 3.9 counts and lists only seen
    namesakes (run390 generaltasks 45F346 calls co(obj, 0) per object):
    troll T64 `drop cup`, secret_of_lost_world T56 `take scroll`
    (2026-09-14)
  - The article test is case-sensitive: only lower-case `a`/`an`/`some`
    become `the`. p4PFX (602428ad6)
  - A typed look is an exact whole-line list, and a bare `x` exits examines
    (3.9+). The NPC examine overwrites the object's text and still ticks
    (4.0). lair (5662e7397)
  - The examine state line is always " is ". `[4.0]` magicshow T80
    (e0f709c46)
- **Carried objects.** Only a listing, or the task mover, reveals what the
  player carries, and `i` stamps them seen. yak_shaving (8e006d2f5)
- **Task move-object** stamps seen per destination. `[4.0]` aliasagent
  (e478cdbd6)
  - 3.9 stamps by destination the same way (run390 455C24-4573DA: room only
    when the player's, into/onto only when the parent is seen, held/worn by
    the player always, a character's room when it is the player's), except
    that an object handed to or worn by a character is never stamped
    (456099-4560DA, 45618B-4561DA). `[3.9]` secret_of_lost_world T52/T53
    (2026-09-19)
- **Darkness** is the condition AND HideObjects, and it gates the seen
  flag. A dark examine answers "can't see X very clearly." `[<4.0]` p39DARK
  (9aff3fda9)
- **Absent-object refusals:**
  - co() matches anywhere; the handlers refuse with Prefix + Short, and in
    takes() the last match speaks. `[<3.9]` p37EXAM/p38EXAM (b526c013b)
  - takes() walks every object in index order and lets each one co() names
    write; a HELD namesake below the target says "You've already got X!"
    (run390 4551E4, run380 43E3F6) and the target is never taken. 3.9's co()
    mode 1 recounts loose room objects and falls back to the Prefix's last
    word (43B37B/43B572), so deardiary T52 `take blue plate` still takes the
    blue one. `[<4.0]` stardust T38, secret_of_lost_world T53
    (`lib_co_pre400`, 2026-09-19)
  - 3.9 drop answers "You don't have the stone!".
- **Lock and wear:**
  - lock/unlock has no refusal for an object without a key, so the
    catch-all answers. `[4.0]` hcw T189 (b6d2f4f1f)
  - lock/unlock refuses a seen, absent object's state. `[4.0]` sswhore
    (fee19ae2a)
  - wear marks only the whole line's 463640 winner, and a tie gives "Wear
    what?". `[4.0]` beer (84b0bb73a)
  - Dobattle's wield arm uses a binary InStr on the raw Short or first
    Alias. villains_and_kings T12 (5662e7397)
- **Unknown nouns (pre-4.0).** `x <unknown noun>` answers "Nothing
  special."; no Runner says "Open what?"; the reach rule is "You can't reach
  X from Y!". A trailing-space Short is unreferenceable. (f8fe84a3c)
- **Other verbs:**
  - `turn` on a seen, absent object: "can't see X." `[4.0]` hcw T81
    (b6d2f4f1f)
  - `pull` on a seen, absent object: "You can't see X.", the same therest
    clause `push` already had. `[4.0]` grumble T207 (`lib_cmd_verb_absent_400`
    on the `pull %text%` row, 2026-09-15)
  - `open`/`close` act on 463640's unique present-and-seen winner over the
    whole line (4756AB, 4759D5), even when the parser bound nothing:
    `open phone book` opens the held Cell Phone (alias "Phone"). `[4.0]`
    xfiles T62 (`lib_open_close_resolved_400`, 2026-09-15)
  - `turn on/off` refusals append the particle. `[4.0]` thepkgirl
  - `kiss` answers "I'm not sure she would appreciate that!". `[3.9+]`
    (07bbd664d)
  - sit, stand and lie need the object on the room floor. `[3.8+]` house
    T124 (4e7df6dff)
- **A task's `%object%` binds only a seen object.** run400's matcher skips
  an object whose seen byte is clear (458E6C, [48]); run390's checktask
  binding does the same (44ABEA, [44]). `take cushion` with the cushion
  lying unlisted on the pile misses the task, and the library answers "Take
  what?". The present-before-absent pass order is not ported. `[3.9+]`
  Glum_Fiddle T16-22; the feed now examines the pile first, and
  `runner_transcripts/Glum_Fiddle.txt` wins (`uip_match_entity`, 2026-09-15)

### Put and take-from

- **4.0 put/task precedence:**
  - A completable library put beats a passing task.
  - A size or capacity refusal prints without claiming the line, and the
    task follows.
  - The implicit take is gated on a mode-1 pre-match, and it runs *before*
    the put handler's task look-up.
  - The class-filter bytes are 104/105.
  - `[4.0]` House, frustrated, ShadricksUnderground (794ed4cd3)
- **4.0 put wording:**
  - A 4.0 put names the container first: "Where do you want to put...?",
    "...put things inside/onto." (no turn), "can't put anything
    inside/onto X!". `[4.0]` p4PUT (1ad720476)
  - A static is named by put ("can't take X!"), and an empty hand answers
    "You are carrying nothing!". `[4.0]` p4PSTAT (0f061d3ab)
  - `put X in Y` where X names nothing present clobbers the line to `put X
    `, so the catch-all speaks. A silent unnamed put also leaves the answer
    to the catch-all. `[4.0]` icecream, house T263 (8f2898bd4, 5a8ac82fb)
- **4.0 single take of a worn object.** get_piece tests possession with
  44615C, which counts worn as held, so `get <worn thing>` says "You are
  already carrying X." (not "already wearing"). `[4.0]` 3monkeys T65
  (2026-09-19)
- **The put handler's own answers** (rules A-H):
  - the bang;
  - "is closed";
  - "already inside" (4.0 only);
  - the pre-4.0 put universe;
  - the object's failure outranking the container's;
  - `Put X inside what?`;
  - the `put all` empty-list wording;
  - drop reaching into a carried container.
  - Measured on all four exes (ea58a1f17, f5a95d64e).
- **Put ON.** Below 3.9, put IN and put ON are one handler, and the
  target's kind picks the preposition. 4.0 holding only the supporter gives
  silence, then the catch-all. The 4.0 splitter keeps " on " on a tie.
  `make_surfprobe.py` (ada92cc47)
- **3.9 post-put task sweep.** A named put that moved its object empties the
  buffer and runs the tasks on the typed line; the put text comes back only
  if nothing printed (run390 4626B6/4626C5/46275A). `[3.9]` losttomb T85
  (`lib_put_task_sweep_390`, 2026-09-14)
  - Re-read 2026-09-19: the sweep (462553-462760) is claimed by the first
    object co() finds named in the line among those held, lying in the
    player's room or worn (o(22) = 0 / room / &H9C); the claimant runs
    tasks(1) = checktask(line, 0), QUIET (a failing restriction restores the
    buffer, 452BE6).  Nothing eligible named leaves the result 0 and
    generaltasks' tasks(0) (45F48B) runs LOUD on the line with the put text
    still in the buffer (FailMessage overwrites, 452BBD).  troll T116 (quiet,
    held basin), losttomb T85 and secret_of_lost_world T118 (loud).
    (`lib_put_sweep_claims_390`, `run_typed_line_task_commands`, 2026-09-19)
  - 3.9 insides() resolves the single put's object with co(obj, 0) over every
    object -- Short or Alias as a word, the prefix only when present -- and
    moves the first seen, present, held-or-loose one; a prefix naming an
    absent namesake is not heard. `[3.9]` secret_of_lost_world T118 `put red
    gem on statue` moves the green gem (`lib_put_co_resolve_390`,
    2026-09-19)
- **A 3.8 in/on object with an unset parent** goes in the first container.
  (5cf3d7059)
- **The take-from handler's own answers.** The 3.9 insides() decision
  procedure; `empty` is take-all-from in 4.0 only. p39DARK/p4TFROM
  (2ab1a7c5d)
- **4.0 take capacity.** Each object is tested for size first ("<Your> hands
  are full.") and weight second ("<The X> is too heavy for you to carry at
  the moment."), even out of a container the player holds (run400 46302C,
  462EA0/462F09). A player-held object loads with its [2E] container field
  cleared (4906A2), so it never phantom-weighs object 0. `[4.0]` wilkins
  T22/T107, businessasusual T20/T24, provenance T722/T724, riding_home T1
  (`lib_take_over_capacity`, 2026-09-15)
- **3.9 take-from capacity.** insides() tests size first and weight second
  (4638C8/4638DE). Weight is waived when the container is held; size never
  is. A single named take-from refuses per object ("<Your> hands are full." /
  "That is too heavy for you to carry."). The all/and forms work
  differently:
  - A pre-pass counts the objects that fit. If none do, the answer is "<Your>
    hands are full." or "That is too heavy." and nothing is taken
    (4634F9).
  - Otherwise nothing is refused per object. One summary follows: "<You>
    can't take any more, as it is too heavy." if any object failed on
    weight, else "... as <your> hands are full." (463BDB/463C04).
  - `[3.9]` alexis T28, alexis_worn_cube T27
    (`lib_take_from_over_capacity_390`, 2026-09-15)
- **4.0 put names a present-but-unseen object by asking the scorer
  directly.** The seen-gated %text% matcher can't name an object inside an
  unlisted-but-open container, but run400's object scorer (463640, its
  "mode 2") finds it anyway by scoring every present object regardless of
  seen state, moves it, and leaves it unseen -- so the name composer
  answers "that" for it rather than its real name (the same "unseen ->
  that" rule as the "Getting off X" precedent). Only fires when the top
  parse's failure really is the seen-gate: a present object the top parse
  missed for some other reason, while already seen, still gets its real
  name. `[4.0]` hub T79 (`lib_put_fragment_present_object`,
  `lib_put_print_object_or_that`, 2026-09-19)

### NPCs, walks and battle

- **Walk announcements.** The announcement joins the turn's paragraph, and
  ALRs span the join. 4.0 capitalises the Name. sa.taf, p4WALKALR,
  p4WALKCAP
- **Walk steps:**
  - A walk step is exact-tick gated. Merry_Murders
  - An empty game-start walk preempts for ever. FunHouse, iqsfot
  - A finished 4.0 walk is stamped -1. the_pk_girl
  - A Hidden walk stop stamps the walker's location whether or not it
    moved. (0e9c8af36)
- **The walk tick** stamps NPCs in the player's room seen first, and the
  player-side meet fires only on a typed move. `[4.0]` the_pk_girl T52,
  Laurie's walk-meet task 413 (d1c61b0c5, 2dfd964cf)
- **The player-side meet** skips a walk that a higher-numbered walk preempts,
  by the same test as the tick (run400 4754A5-47557B). The Runner reaches the
  CharTask through the dispatcher by command text; Scarier still runs it by
  index, which the corpus does not tell apart. `[4.0]` greekschool, Paul's
  task 22
- **Absent NPCs:**
  - dobattle's "<Name> isn't here!" is a turn. `[3.9+]` alexis_worn_cube
    (41b1f93d3)
  - The 3.9 character catch-all says "<Name> is not here!" if the NPC is
    seen and "Who?" if not; the reference test is Name or first Alias.
    `[3.9]` ALEXIS probes (f89d7c8ac)
  - 4.0 matches Name or any alias, capitalised. `[4.0]` thepkgirl
  - With the Battle System off, an attack on an absent NPC answers "The
    <name> is not here!". `[3.9+]` thepkgirl (07bbd664d)
- **Battle:**
  - Narration names `<Prefix> <Alias[0]>`, corpse lines use the Name, and
    narration follows Perspective. orient_express (00ee24864, 161c822d8)
  - run400 capitalises an NPC attacker that leads its sentence, at five
    sites. (1409cade5)
  - dobattle names its target by Name alone at 4.0 (Name or first Alias at
    3.9). It strikes every NPC a line names, then asks about namesakes as an
    administrative line. A verb naming no NPC asks "Who do you want to
    attack?", continued into the next line. shadowpeak, light_up
    (327feeb9c, e4e85ea89, 3d59b700d, 13f13e66d, 512543515)
  - Type-7 attribute raises are capped at max with no zero floor. The dodge
    pronoun is he/she/it by Gender. `[4.0]` wes_ghn, les_feux (4c7c20f64)
  - 3.9 caps the same way: run390 execute_action 4573DC writes
    Proc_2_1_427948(max, cur + delta) = min() for strength (456EFA/456F64)
    and defence (45714E/4571B8); the max attributes are a plain add
    (45704D/4572A1). Now ungated. secret_of_lost_world T125-126/T164,
    spirits_flight T17/T27 (2026-09-19)
  - The attitude action stores Var3 RAW into the NPC's byte (run390
    456BB1-456BF4, run400 48D747-48D78A; 0 neutral, 1 ally, 2 enemy) -- no
    combo reorder. deaths T42/T48/T49 (2026-09-19)
  - The 3.9 speed action indexes the NPC by Var2 RAW (45738D), no
    referenced-character case. `[3.9]` deaths T35-T49 (2026-09-19)
  - The 3.9 blow (chardohit 442C7C) has no accuracy roll: hit iff
    hitstrength > armourstrength (4424F1), damage = max(0, hit - armour)
    (442585, Proc_2_0_4279A4 = max), no draw. hitstrength 42B7EC = strength
    + best weapon; armourstrength 42DB6C = defence + worn objects' field 76.
    getnexthit 42C678: speed 1 draws Int(Rnd*1)+1, the rest are constants.
    Already Scarier's `battle_legacy` path; read 2026-09-19.
  - Stamina recovery is a per-line pass that revives the dead;
    `battle_select_target` takes 0-stamina targets. (544868698, e78827349)

### Events and RNG

- **Timing:**
  - There is no startup tick before 3.9; `delay N` starts on turn N.
  - `score` ticks events in 3.7/3.8.
  - A finishing event re-checks lower-indexed events in the same tick.
    Vardock
  - Rolls exclude the upper bound. A backwards range still draws, flooring
    toward minus infinity. hyper_b_s (a288e471b)
  - A zero-length event that starts after its tick this turn keeps the +1
    (start from state 2 sets clock = roll + 1, 46FE49; the running block
    runs once per turn, 46FF48). It parks until the next tick rather than
    finishing at once. riding_home T50 (event 9, task 118's 90% hint), cursed
    T137 (event 89, wet fur). 2026-09-15
  - An event that starts PAUSED (pauser done, resumer not) keeps the
    start's roll + 1: the pause test inside the running block exits
    checkevent before the decrement (run400 46FFCC / 47013E, run390
    44891F, run380 43A335), so after the resume it ends one turn later
    than its roll. thepkgirl T152-T171 (event 118, Time 18), which moved
    event 319's roll and the T312 "slips you a buck". 2026-09-19
  - A 4.0 event whose restart ROLLS 0 (Time1 = 0) parks for good: the
    restart stores the roll with no +1 (4705E3-470605), the next running
    block takes the clock to -1, and the finish test is `clock = 0`
    (470251), so no PrefTime text, finish or restart draw ever follows.
    Pre-4.0 unmeasured, keeps the authored-length test. zelda T52-60
    (event 5, the shopkeeper's ocarina; 468 = 468 draws). 2026-09-19
- **Completed tasks.** A 4.0 event that runs a completed task still walks the
  task's restrictions (45FB78 calls 455C60 before the done/repeatable test),
  so a failing restriction prints its message. A passing one prints nothing.
  riding_home T47 (event 8, task 104). 2026-09-15
- **Static objects moved by events.** A 4.0 static's presence is its
  per-room array o(28) (obhere 452E67). The start mover (Obj1,
  Proc_19_16_45614C) replaces the rooms; the finish movers (Obj2/Obj3,
  checkevent 4702FF-4704F7) only add one, clearing for hidden or held
  alone. The array starts as the Where list, so finish moves pile up.
  3monkeys T109 (the anvils in all four corners). 2026-09-19
- **Look text.** An event's look text is gated on the room being described,
  not on the player's room. goldilocks, cybercow
- **RNG parity.** `SCR_RNG=xoshiro` matches vbrng draw for draw, and it is
  the harness default (991a5f8d9, b150980c8). A death prints the end-game
  score summary. (5f20dd8ce)

### Output, wording and the room block

- **The room block is ONE string.**
  - viewroom joins the description, InRoomDescs, "Also here", the "X is
    here." sentence, NPC texts and event LookTexts with pspace(), a
    conditional two spaces.
  - The heading is `"\n" + name + "\n"`.
  - A leading `<br>` collapses only against Scarier's own break.
  - perspectives, datewithdeath, Vagabond (a4d61a288, 5bd9ace04)
- **Executed tasks.** Executed-task CompleteText and every AdditionalMessage
  join the turn with pspace. `[4.0]` the_pk_girl T156 (07bbd664d)
- **Room listing:**
  - It is decided by the OnlyWhenNotMoved byte (449B6C), not by whether
    InRoomDesc is empty. The library take and a task move spend the byte.
    camelot15, zelda (fe67c45de)
  - An InRoomDesc of one space counts as present. topaz (aeafcbf34)
- **ShowRoomDesc.** It prints before the task's actions, except that an
  empty CompleteText plus a non-empty AdditionalMessage builds it after
  them. `[4.0]` SRD4 (771d03b85)
- **ALRs:**
  - The ALR list is walked once, longest Original first. 4.0 recurses into
    each replacement; 3.9 does not. qui_a_tue_dana (d93a331b7)
  - ALRs apply after characters() (c2b28179c).
  - Originals ending in a space match. reluctantvampire T78 (b6d2f4f1f)
  - Double spaces next to pattern groups match (b1e45887b).
- **Variable substitution:**
  - The 4.0 output filter freezes variables per completing task. humbug
  - User variables substitute in index order, Replace-all each, after the
    system tags. `[3.9+]` datewithdeath (8cf9cce63)
  - `%status_<name>%` is the lowest-indexed openable Short match.
    briefcase (32852b749)
- **Library wording:**
  - Third-person library text is not conjugated. herrdoktor (81231bc37)
  - Pre-4.0 says "You pick up" where 4.0 says "You take".
  - Pronoun echoes use round brackets. 4.0 takes the article from the last
    composing handler, and 3.9 keeps the authored one. 3.7/3.8 store
    tense(Prefix) & Short for any verb, but splice only the Short into the
    line (run380 441EF1/441F09). wrecked
  - "(Getting off that first)" for an unseen parent. `[3.9+]` gateway
    (run390 431943)
  - The 3.7/3.8 inventory listing does not claim the line, so a matching
    task's text follows it (run380 4421C2). wrecked T24
  - The 3.7/3.8 ask topic matches by substring, and the last match wins
    (run380 4408B2). wrecked T129/T211
  - The examine-self full stop is 3.9+.
- **Other formatting:**
  - The "<Name> is here." sentence is capitalised only by the 4.0 loader's
    `#` substitution (Proc_21_3_446BB4 at 491EF3). run390/380/370 append
    the raw Name, and no Runner capitalises an author's own " is here."
    text. `[4.0]` goldilocks; twilight T12-34 (2026-09-15)
  - `isare()` is exact and case-sensitive, and the loader fills an empty
    Prefix with "a". yeh (496c115f2)
  - 4.0 room names take every matching alt's Changed. togetyou (fee19ae2a)
  - The multi-take line comes before the earlier task text. `[4.0]`
    fullcircle T43 (b6d2f4f1f)
  - The (Getting off X first) bracket line prints on its own line.
    Monsters_r2
  - The score summary prints after every EndGame; NotifyScore defaults to
    OFF.
