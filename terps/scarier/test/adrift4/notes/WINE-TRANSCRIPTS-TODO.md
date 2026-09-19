# TODO: Runner-transcript verification of the v4 walkthroughs

Replay a wired walkthrough command for command in the real Windows ADRIFT
Runner under Wine and diff the Runner's own transcript against Scarier's.
Where they disagree, fix the engine, never the walkthrough. Then re-bless the
golden and write the evidence into the game's row comment in
`harness/run_v4_walkthroughs.sh`. Scope is all four file versions (3.70,
3.80, 3.90, 4.00). Only the Runner binary and the capture flow change with
the version.

**Pruned 2026-09-14 and twice on 2026-09-19.** This file holds the workflow,
the open leads, the deliberate deviations and a short index of every ported
rule. The dated write-ups (probe tables, Runner addresses, corpus fallout per
port) live in the row comments of `run_v4_walkthroughs.sh`, in the code
comment next to each function the index names, in
`~/Adrift_decompile/index/annotations.tsv`, and in git history:

    git show aee976374:terps/scarier/test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md   # before this prune; full 2026-09-19 entries
    git show 55dd84ee1:terps/scarier/test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md   # with the 2026-09-14..19 triage write-ups
    git show 72fd5ea08:terps/scarier/test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md   # last full version before the 09-14 prune
    git show 45e20596:terps/scarier/test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md    # before the 2026-09-06 compaction

Row comments and probe generators cite sections by title ("Ported
2026-09-10: the take-from handler's own answers" and so on). Grep the
`72fd5ea08` version for the title. The commit hashes and function names in
the index lead to the code.

---

## Where things stand (2026-09-19)

- **Goldens:** 429/429.
- **`runner_transcripts/`** holds one Runner transcript per row except
  dreamquest, each driven with the golden's feed, seed and popups. 3.9/4.0
  rows come from the vbrng Runners (`run390x`/`run400x`, xoshiro, seed 1234
  unless the row sets one), so draws compare value for value. 3.7/3.8 rows
  are `.rtf` captures with the last command grafted back from the Runner's
  window. `manifest.tsv` carries each row's verdict and `compare/<tag>.txt`
  the report for a row that differs. Regenerate with
  `harness/runner_transcripts.py` (its README explains how);
  `recompare <tag>` refreshes one row after an engine change.
- **Manifest:** 377 identical on every turn, 31 identical apart from
  whitespace, 19 with a report. Every differing row is classified under
  "Open leads" or "Nothing owed".
- **Which transcript to cite.** For a wired row, cite
  `runner_transcripts/<tag>.txt` (`.rtf` for 3.7/3.8), never the
  `Adrift_<N>_<tag>` file a drive left in the prefix. The manifest's `source`
  column records which archive file each copy came from, and a winning
  re-drive replaces the copy through `collect`. Only runs with no row keep
  their archive names: probes and one-off variants such as House_sober.
- **Older archives** under `~/adrift-battle/runner/wine/`
  (`transcripts_v4_corpus_2026-09-08/`, `transcripts_v4_xoshiro_2026-09-12/`)
  hold no engine lead. `pfx/drive_c/adrift/` is the live archive where every
  drive lands: never `rm` a glob there.

---

## Workflow

### 1. Pick and screen a row

- **The game must be deterministic along the route.** A roll only matters
  if its text can reach a room the route visits while the event runs. Rolls
  exclude the upper bound, so `start=1..2` always draws 1. With the vbrng
  Runners both engines draw the same stream, so this matters much less.
- **`harness/screen_wine_candidate.py <taf>`** reports the version (header
  bytes 8-10: `E7a` = 3.90, `E>a` = 4.00), the real command count, events
  and which can roll, NPCs and walkers, and silent typeable tasks.
- **`SCR_DUMP_TASKS=1 harness/scare <game>`** dumps tasks, events and walks
  to stderr on the first task check. A keypress-gated intro swallows a bare
  `look`, so feed the solution file instead.
- **Which Runner.** `run370`, `run380`, `run390`, `run400`, plus `run*x.exe`
  for the vbrng builds. Each file version needs its own Runner (run400
  refuses a 3.90 file, run380 refuses 3.70). The game must sit in
  `pfx/drive_c/adrift/` with no spaces in its filename.

### 2. Build the feed

- **`harness/make_wine_cmdfile.py`** builds the feed from the golden. It
  strips comments, reads the startup waitkeys that `SCR_MARK_WAITKEY=1`
  measured into `PRE`, and emits the `#sleep` and pause markers for every
  span, including the span after the last command. Whitespace-only lines go
  out as bare Returns.
- **Empty solution lines are commands**, not pauses. The Runner answers
  them "Huh?" or similar.
- **A pause marker must be answered by a blank line, never a real command.**
  Without `SCR_SKIP_WAITKEY`, scare's `[Press any key]` reads a whole stdin
  line, so a golden can pass while a pause swallows a command. The Runner's
  pauses never eat a typed line: it runs as a turn, and a `look` there ticks
  events and draws. light_up (ten lines) and mould (one) were repaired this
  way on 2026-09-19; both goldens are byte-identical after it.
- **Name and gender prompts are not feed lines.** Pass them as
  `POPUP_ANSWERS="Hero|male"` on the drive and as `--popup` on the compare.
  run400 always asks; run390 only while PlayerName is blank; 3.7/3.8 never.
- **Randomised puzzle state** (humbug's dials, keypad codes) is read off a
  transcript and spliced in at a `#save`/`#restore` checkpoint.

### 3. Drive the Runner

- **`fast.sh <game.taf> <cmdfile> [exe] [pre]`** drives one row and
  **`par.sh <jobfile> [maxpar]`** several. Both use `drv/drive.exe`, which
  posts Win32 messages into the prefix, so no foreground is needed. Both
  force Verbose ON and all five Appearance checkboxes ON, which is required.
  Run at most 3-5 Runners at once. A failed job leaves a 0-byte transcript:
  check the size before believing a row that "lost every command".
- **`measure.sh` / `measure38.sh`** (keystrokes, foreground) remain only for
  cross-checks.
- **vbrng:** set `VBRNG=xoshiro`, optionally `VBRNG_SEED` (default 1234) and
  `VBRNG_TRACE='C:\adrift\x.txt'` (a Windows path); pass `run400x.exe` /
  `run390x.exe`. The row drive is `xoshiro_par.sh` (7th field = popup
  answers). `drive.exe --temp` gives each run a private TMP; without it
  run400 redraws media temp names at load.
- **Reading the vbrng trace:** it appends, so isolate a run by its second
  `TURN 0`. `TURN n` is written after command n ran. `vbRND(Missing)` lines
  are the .taf codec, not draws.
- **3.7/3.8 have no live transcript.** Save Transcript writes an `.rtf`
  (par.sh names it `Adrift_<N>_<tag>.rtf`) that is missing the last command.
  A death or end-game modal wipes the scrollback. `£` comes out as `Â£`.
- **`DUMP_SCROLLBACK=<file>`** saves the RichTextBox's own text
  (`WM_GETTEXT`): the window itself, exactly, no screenshot or OCR. Use it
  to prove a line break is a transcript artefact and to recover what an
  `.rtf` could not hold. `dump_par.sh` drives a job file that way and
  `harness/graft_scrollback_tail.py` appends the difference to the archived
  transcript. It closed all 19 3.7/3.8 rows on 2026-09-17; on 3.9/4.0 the
  transcript is live, so end-of-feed losses there are the keypress wait
  eating a command with no text behind it. See `runner_transcripts/README.md`,
  "Grafted tails".
- **Kill Wine with `pkill -9 -f wine; pkill -f wineserver`.** fast.sh and
  par.sh reap orphaned `winedevice.exe`. The prefix is shared mutable state:
  never drive it from two sessions at once.

### 4. Compare

    python3 harness/compare_wine_transcript.py --taf G --feed F --runner T \
        --env SCR_RNG=xoshiro --env SCR_SEED=1234 [--popup Hero --popup male]

- **For a wired row** use `python3 harness/runner_transcripts.py recompare
  <tag>` instead. It runs this compare with the row's own feed, seed, env
  and popups, and rewrites `compare/<tag>.txt` and the manifest verdict.
- **Rule 2 comes first.** It prints every feed command the Runner never
  echoed. A lost command desynchronises everything after it and is a
  harness problem.
- **Then it diffs the aligned turns**, whitespace-normalised. The aligner
  can present a single text difference as a ±1 resync (house T263).
- **The compare does not apply the row's env.** Pass the row's `SCR_*`
  settings yourself. For draw parity, compare without `SCR_SKIP_WAITKEY`
  when the feed answers a "press enter" with a blank line.
- **Draw counts:** count `RND #` lines in the Runner trace against
  `SCR_TRACE_RAND=1` on Scarier's stderr. `2>&1` can glue a `RND #` onto
  unterminated text, so count unanchored.
- **Sweeps:** `harness/sweep_wine_turns.py` runs the compare over every
  archived row (`--only <tag>`, `--limit N`, `--lost`).
  `harness/sweep_wine_breaks.py` covers line structure; Runner-only breaks
  are ground truth and are at 0. `harness/sweep_v4_corpus.py` covers a
  manifest-driven capture (`--tsv` clusters).
- **Rebuild first** with `sh harness/build.sh`. A stale `harness/scare`
  makes every table lie. Compare sweep totals only against a baseline from
  the same day and transcript set.

### 5. Classify each differing turn

1. **Capture artefact, name it and ignore it:** the Runner's `[Press any
   key to end]` tail where Scarier has none (only the "just stop" ending,
   which run400 leaves promptless; `task_print_end_keyprompt()` buffers the
   prompt for every other ending); blank-line counts around an ending (whitespace collapses,
   goldens run through `cat -s`); a startup echo ("I don't understand what
   you mean." for a feed's leading blank lines, Glum_Fiddle); a `<centre>`
   join; a `<waitkey>` line join or `<waitkey><cls>` butt-join; a wrap
   inside an unbreakable token; `[MORE]` splits; `.rtf` mojibake; the
   epilogue cut at the final keypress; rule-2 "lost" lines after an
   identical ending.
2. **Harness.** Suspect first: Verbose OFF; missing popup answers; a
   startup pause that offset the streams; `[Y/N]` answers Scarier asks and
   the Runner never does (`hint`, `quit`); a `#save` in the compare (the
   echoed save is a turn in run390 but dropped by the compare, so Scarier
   runs one tick out per save).
3. **RNG.** With xoshiro the draw counts decide: equal counts mean any value
   difference is real. On a native-RNG capture, re-run Scarier under 2-3
   `SCR_SEED`s and watch the line move. A menu chosen by `ACT type=3 v2=2`
   is seed-locked (mould), so a clean feed is still not a comparable run.
4. **Engine.** Find the Runner's code path (decompiles `run400.bas`,
   `run390_3.bas`, `run370.bas`; always confirm against the `.p32dasm.txt`,
   since the `.bas` drops statements and `push &HFF 'Byte` is -1;
   `~/Adrift_decompile`'s `find.py -v runNNN ADDR` resolves addresses).
   Settle the version gate with the string census (which exe's UTF-16 pool
   holds the literal; find the use site, near-miss literals are menu
   captions). Measure with a probe if the corpus does not isolate the rule.
   Port behind a `TAF_VERSION` gate, run the suite, re-bless with `--bless
   <substr>`. Record the measurement in the row comment and a line in the
   index. Add the addresses to `~/Adrift_decompile/index/annotations.tsv`
   and run `sh index/refresh.sh`.

### Probes and offline oracles

- **4.00 probes:** `harness/make_400_*probe.py` and `make_arena_probe.py`,
  packed with `taftool.py` against a donor .taf. Give each probe a `probe`
  task that prints `PROBE OK.` first and last, so a shifted command shows.
  Separate cells with `look`; a pending ambiguity answer slot eats the next
  cell otherwise.
- **3.90 probes:** `harness/make_39_*probe.py` writes the schema directly.
  The generators only convert upward.
- **3.70/3.80 probes:** `make_37_/make_38_darkprobe.py`,
  `make_3738_examprobe.py`, `make_surfprobe.py`, hand-authored. A
  hand-built .taf must parse to exact EOF in `harness/scare`. Every version
  needs its own probe file (run400 says `Incorrect version`).
- **The .taf files stay untracked**; the generator is the artefact.
- **Other oracles:** the corpus's ALR *Original* strings (an author only
  rewrites what the Runner printed); the exe string pools;
  `~/Adrift_decompile`.

---

## Open leads

None blocks a golden. Rows not named here differ only by a capture
artefact. The 4.0 and 3.9 engine lists are empty: the remaining 3.9
differences (alexis and alexis_worn_cube T99 `open chest`, everything T38
`read diary`, lifesimulation T6, life `piss`, the_hangover) are the
silent-task deviation below, and since 2026-09-19 they differ by text only,
not by a tick.

### Engine, every version

- **`go <place>` that names no exit** (every version): the Runners'
  gotoplace answers " can't get there from here." or "Unknown place."
  (run370 42BE15/42BE26, run380 432030/432041, run390 43CC0F/43CC20,
  run400 464E3B/464E4C). Scarier keeps upstream SCARE's "I don't know how
  to get there from here." plus the exits list (`lib_cmd_go_room`). The
  p39ASK probe's `go stone` gives "Unknown place."; 3.8 answers that line
  from therest's go arm instead ("Just a direction will do."). Unmeasured
  beyond that one cell; which of the two strings applies when is unread.

### Harness and compare

- **house** is comparable only as `House_sober.taf` with
  `cmdfile_house_sober.txt` (identical on every turn; no row, so it lives
  only as `Adrift_128_housesober.txt` in the prefix). The original row is
  10+ turns blank on the Runner side from T1 because of the `%drunk%` ALR
  stack overflow (a deliberate deviation).
- **gmylm** (15 MB .taf, ~400 MB draw trace at load) needs `LOAD_SLEEP=600`
  to get past drive.exe's 25 s load cap. Its copy is identical on every
  turn; this matters only for a re-drive.
- **Permanently unmeasurable:** `dreamquest` (run400 cannot load a task
  with an empty Command vector); the `to_hell_and_beyond` assisted rows
  (Scarier-only by design).

### Nothing owed (capture and compare artefacts)

- Epilogue or pause text one turn late or cut at the final keypress:
  endgame T9, mortality T29/T32, iqsfot T41-42.
- motion T257-258: the Runner's echo for T258 landed one room block early
  (`l` shows the drive room twice, each block opening with `<cls>`); with
  whitespace stripped both sides show the same frames in the same order.
  The other reported turns are Scarier's 80-column wrap on long `O----`
  rows.
- mould (seed 1; the Runner wins 150/150): the `hint`/`y` deviation, plus
  T99-101, where a bare Return after `s` is a real empty turn and the next
  blank answers the pause; the compare splits the turn differently, same
  text.
- Whitespace-only joins or the trailing `[Press any key to end]`: the 31
  "apart from whitespace" rows. Glum_Fiddle turn 0 is the startup echo.
- Lost commands after an ending: thelasthour's last `wait` only (that row
  runs without SCR_SKIP_WAITKEY, so one ending "Press a key." swallows a
  solution line and the extra `wait` makes up for it). Other rows' trailing
  padding was trimmed and re-driven 2026-09-19 (1cc5dbc55).
- Deliberate deviations by row: the silent-task rows above;
  alices_restaurant (run370 double matcher pass); sandy_meta_number and
  hero's closing `statusline` (SCARE meta-commands; hero uses `statusline`
  on purpose to print the score).
- Load failures from the 09-06/09-07 batches: six rows raised `evaluate
  error - Subscript out of range` mid-game; TheADRIFTProject crashed with
  run-time error 401 at command 92. (The 3.90 games that "failed to load"
  had been fed to run400; all load on run390x.)

### Engine, needs a probe (4.0)

- **Silent put confirmation for object #1.** run400 prints no `put`
  confirmation when the moved object is dynamic object #1; run390 prints
  one. The listing does not single out index 0 (name_object 46E23C/46E3FA,
  insides 46639C); next is a live trace of var_A4/var_A6. No corpus row.
- **name_object's own list loops (46E04E / 46E0B2)** are not rebuilt. Six
  cells, wording off the Runner.
- **`put box in box`:** run400 announces the take and prints nothing;
  Scarier says "can't put an object inside itself!".
- **Put resolution ordering:** the 4.0 put fragment fallback scores every
  present object in one pass, where 463640 mode 2 tries held objects first.
  The held-first pass is ported for plain `drop` (`lib_drop_resolve_400`),
  not for `put`; a held/loose tie on a put line would differ. Also unread:
  why the seen-gated %text% parse missed TheADRIFTProject's already-seen
  battery (`put battery in remote`), which the fallback covers.
- **Scope, unmeasured:** the never-seen "You can't see that." branch at
  471995; the two-pass `%object%` scope filter proper (`SCR_TRACE_SCOPE`):
  its seen gate is ported, its present-before-absent order is not; the NPC
  seen gate for `%character%` (xfiles `look up byers`).
- **Second-noun ambiguity:** wording of an instrument ambiguity (sswhore
  `unlock drawer with key`); a tie inside either half of a " with " split;
  lock/unlock with a Key whose left half resolves to nothing; absent
  lock/unlock where the object really is locked.
- **Ambiguity prompts:** co()'s crowded arm (454454) and its &HFE/&HFF
  answers; "That wasn't one of the options!" has never been triggered;
  whether an object ambiguity on a task-answered line also suppresses the
  tick; 454454's prefix contest handing the write to a namesake with more
  Prefix words typed (not modelled in `lib_co_400_line_leaves_which_pending`).
- **Events:** an event with RestartType=2, an immediate starter and a
  non-zero length fires once in run400, but Scarier re-arms it. Corpus
  exposure is zero.
- **Put corners:** `(Taking X first)` ahead of a closed-container refusal;
  `put all in <the container, held alone>`.
- **Output filter:** where the ALR pass sees trailing spaces; the NewParse
  `%` pattern binary path; the drop rebuild at 46F33B.
- **Drop/take/wear setter branches** 46FB7D, 47C7F1 (and run390's wears at
  43D289): read, not measured.
- **The run400 loader's "Anonymous" fill** for an empty PlayerName with
  PromptName off.
- **Silent-task test scope, the unported rest.** run400 tests the whole
  turn buffer. Scarier counts anything a task's run adds (baroo) but still
  ignores text written before the dispatch. No corpus row is known.
- **Turn sectioning, the unported rest.** run400 builds the turn as one
  string joined with pspace() and runs the ALR pass over it. Scarier joins
  only the room block and, at 4.0, an executed task's text plus every
  AdditionalMessage (`pf_buffer_join_line`); everything else is its own
  section, so an ALR Original spanning a join does not match. Unfinished,
  not policy: an event's text after a task's text (p4SRC `xray`); the rest
  of the turn (thetest, a two-sentence Original). Expect a large
  reblessing: the task-text join alone moved 94 rows, and
  sweep_wine_breaks still counts 5622 Scarier-only breaks.
- **p4TAMB side finding:** `rub red box` answers "You can't rub the red
  box." although the task is `rub %object%`, while `rub box` runs it. Not
  chased.

### Engine, needs a probe (3.9)

- **checkwild, the unported half.** `uip_wildcard_match_pre400` only
  vetoes a tree match. checkwild's middle pieces need not be in order, so
  a line the tree refuses (`* king * rose *` typed as "rose ... king") may
  still match in the Runner. 3.9's %object% substitution (44AAD6) is not
  emulated, so 3.9 commands with a reference skip the check. Group
  patterns (`[`, `{`) skip it at every version.
- **"Please be more clear, who do you want to <verb>?"** is a SCARE
  invention (alexis_worn_cube t79). The `who` form is unmeasured. For the
  object form, 3.9 examine answers "Nothing special." (ported); 3.9's other
  verbs are unmeasured.
- **Per-verb absent-NPC branches:** `talk to <npc>` elsewhere (hcw,
  alchemist); `give obj to npc` elsewhere; run390's take `is not here!` at
  4596E1 (Battle System off); kill/kick/punch go through other grammar
  first. The attack branch is ported.
- **run390's Who-prefix consumption** at 460022 is assumed, not measured.
- **Take-from:** the " and " clause picks the last container in 3.9 and the
  first in 4.0; 3.9's " and " collection bug; the pending slot after `Get X
  from what?`; surface-vs-container wording of the parent-derivation arm.
- **Put:** the container refusals at 461769/461803 ("can't put anything
  inside/on that!") also precede checktask, unported; "onto"; a locked
  container; a named static; an object on a floor supporter; `put all in
  <nothing>`.
- **Two-object canonical prefixed retry:** the run390 half is not
  re-measured. The 4.0 half is closed.
- **Examine:** run390's examine state line has not been read.
- **Events, pre-4.0:** the rolls-0 restart rule is ported at 4.0 only;
  run390/run380's finish test and restart store are unread, so pre-4.0
  still finishes such an event on its next tick.

### Engine, needs a probe (3.7 / 3.8)

- **Handlers other than take** were measured on single matches only.
- **run380's task sweep after a *refused* take-from** is not ported.
  run380's count<2 put refusal also precedes checktask, but run380 has no
  sweep (445A0F).
- **Examine:** 3.7/3.8 examines have no bare-verb exit (unmeasured
  corners). run370's sit/stand/lie has no location test (42AEC8, unported).
- **Put:** 3.7 static container `open`; 3.7 bare take from a held
  container; `put X in Y` where X names nothing and Y is a bad container; a
  supporter that is neither held nor static nor a container; a static
  container; `except` forms; `put all in <nothing>`.
- **Pre-4.0 room-name alt walk** is unmeasured.
- **run380 442F5D** (the catch-all speaks for the first present, seen
  object) is unmeasured.
- **`clear`** at run370/380/390 is unmeasured.
- **run380's event route** to the task-ran flag (set in tasks() at 44D0BA)
  is unread; the 3.9 rule is ported.

---

## Deliberate deviations (measured, not ported)

- **Pre-4.0 silent-turn DontUnderstand.** A matched task whose turn prints
  nothing gets "I don't understand what you mean!" in run390. Scarier runs
  the task and falls through to the library. Cases: hangover `open the
  filing cabinet`, everything `read diary`, lifesimulation `turn off tv`,
  life `piss`, alexis `open chest`. The world state agrees in all of them.
  The *spent*-task half is ported (cc7470bf8), and since 2026-09-19 the
  clock half too at 3.9: the line is administrative, as run390's
  DontUnderstand is (`silent_task_390` in run_all_commands()), so only the
  text differs. That fixed every alexis battle difference after T99.
- **run370 double matcher pass** (arlo `get out of bus`).
- **SCARE meta-commands** `wait N`, `hist N` and `redo N` exist in no
  Runner. Eleven more inventions are compiled out by
  `SCARIER_NO_ABBREVIATIONS`.
- **Not ported (policy):** `both` right after a `Which <term>. <list>?`
  prompt (3.9/4.0 re-run the saved candidate list as the typed line, run400
  48AE94 / run390 43B4D5, unmeasured; a bare `both` with no prompt is
  measured and ours: "I don't understand.", two turns, p39WITH T25); the
  battle-disabled `status`/`statusline` fallback; the help/about/time/
  version texts; `turns`/`version` version gates.
- **Empty-Prefix double space** in object listings.
- **ALR stack overflows.** House's `%drunk%` ALR loop and a mutual `A -> B`
  / `B -> A` pair overflow the stack in run400, which then prints nothing.
  Scarier's depth cap prints the intended line.
- **run400 `put all on <supporter in the room>`** with two or more items
  runs out of stack and moves only the first. Scarier completes the move.
- **Undo slots:** Scarier skips administrative lines, which the Runner
  records.
- **`NPCWalkAlert`:** a synthesized task pair with no run400 counterpart.
  It anticipates the ticker's restart by a tick; nothing depends on it.
- **mould `hint`:** run400 has no interactive hints.

---

## Rules measured and ported (index)

One entry each, with the game or probe that found the rule and the commit,
function or date that ported it. Version gates are in brackets: `[4.0]` is
4.00 only, `[3.9+]` is 3.90 and 4.00, `[<4.0]` is 3.70-3.90, `[<3.9]` is
3.70/3.80. No bracket means every Runner. Addresses are in the code comment
next to the named function and in `annotations.tsv`.

### Parser and dispatch

- **Task matching:**
  - One game task per typed line; a silent task lets the library run, never
    a second task. House (c74b1b90c)
  - "Silent" means the turn buffer did not grow while the task ran, so an
    event its execute-task action starts speaks for it. `[3.9+]` baroo
    T107 (`run_task_run_speaks`, 2026-09-14)
  - 4.0 task matching is verb-literal; `*`, `[..]`, `{..}` compare binary.
    A rebuilt line keeps its capitals, so a pre-match can fail to dispatch
    and fall to DontUnderstand. `[4.0]` hcw T162 (b6d2f4f1f, 5fc9ef8d1)
  - The task pre-matcher is restriction-aware. The fallback pass wants a
    FailMessage or a spent RepeatText, and a fallback hit is silent. `[4.0]`
    House task 60 (c38297f1e, 5fc9ef8d1)
  - The fallback reads a per-task restriction cache (one T/F per
    restriction) written only when a task_pick, fallback or
    dispatch-by-index walk FAILS, and never cleared. It re-checks each
    cached 'F' alone; one still failing with a FailMessage is a hit, and a
    clean scan hits on a RepeatText. So a stale cache can claim a line
    silently and get_piece answers DontUnderstand. `[4.0]` 3monkeys T41
    `get husk` after `hit coconut` (`restr_cache_fallback`, 2026-09-19)
  - A trailing space in an all-literal task command must be typed. sommeril
    `get placemat ` (093a12d5e)
  - Before 4.0 a `*` command also has to pass checkwild: prefix before the
    first `*`, each later piece anywhere in the line, the text after the
    last `*` equal to the line's end. run390 pads the line for a leading
    "* " or trailing " *"; run380/run370 pad nothing. 3.7/3.8 put the Short
    of the lowest-index object c() finds in place of %object% first.
    `[<4.0]` alchemist T300, `[<3.9]` marooned T53 `throw map`
    (`uip_wildcard_match_pre400`, 2026-09-19)
  - The SYNONYM table is sequential whole-string rewrites. Vardock
  - The 4.0 `*` matcher does not backtrack: each literal piece is found by
    the first InStr and the line is cut past it; a space goes back on only
    when the rest of the pattern starts with one. So `buy *** *rawhide
    armor*` misses `buy rawhide armor`. run390's checkwild never cuts.
    `[4.0]` the_town_of_azra T13, xfiles T69 (`uip_wildcard_match_400`,
    2026-09-15)
  - The already-done scan passes over a spent task with no RepeatText, so a
    later spent task's RepeatText answers the line. `[4.0]` crookedestate
    T41 (`run_task_refusal`, 2026-09-15)
  - A task command's %character% binds the FIRST matching NPC in index
    order. `[4.0]` iqsfot T158 (`uip_match_entity`, 2026-09-15)
  - characters()' take arm overwrites an object take's answer with "I don't
    think <NPC> would appreciate being handled." when the line has
    take/get/pick up and names a present NPC; the take stands. `[4.0]`
    onnafa T155 (`lib_take_npc_overwrite_400`, 2026-09-15)
  - A 3.7/3.8 task's Obj2 location restriction reads Obj2 - 1 as a raw
    index into the whole object table, and put stores the container's
    object index as parent. An Obj2 naming a static object therefore always
    fails. `[<3.9]` twilight T48 (`parse_fixup_v380_objstate_restr`,
    2026-09-15)
- **Word rules:** `take` becomes `get` before parsing `[3.8]` (great); `z`
  means wait only from 3.90 (cave); `again`/`last`/`previous` are tested on
  the whole line before any task, and `g` joins them from 3.90 (shadowpeak
  TASK 404, `run_is_repeat_word`, 2026-09-14).
- **Line splitting:**
  - 4.0 cuts at `,`, `. `, ` and ` and ` then `, suppressed when the tail
    starts with any object's Short, Prefix word or Alias (case-sensitive).
    The put clause loop puts several objects in one turn. `[4.0]` p4AND
    (dffce55df)
  - Pre-4.0 never looks at the object table. run370/run380 cut only at
    `then`, at the first substring hit (`x athens then look` runs `x a`,
    `s`, `look`). run390 cuts at the first `,`, then `. ` in the head, then
    `then`. No pre-4.0 Runner cuts at a period with no space after it.
    run390 replaces an empty then-head with everything queued behind it;
    3.8 answers it with DontUnderstand. `[<4.0]` p38ASK/p39ASK,
    `cmdfile_psplit.txt` + `cmdfile_psplit2.txt` (`run_find_split_pre400`,
    2026-09-19)
  - No Runner drops the rest of a line after a DontUnderstand element;
    upstream SCARE's discard is gone at every version. (2026-09-19)
  - A name followed by punctuation the splitter left still resolves: `,`
    ends a word at 3.7/3.8, `,` or `.` at 3.9, as in c(). (`uip_is_word_end`,
    2026-09-19)
  - Pre-4.0 therest() answers "Nothing special." to c("look") anywhere in
    the line: `look,`, `zzz, look`, `, look` (3.8) and `look.` (3.9 only).
    c()'s FIRST hit at a word start decides, so `look. look` at 3.8 stays
    DontUnderstand. The row sits above the object and character catch-alls.
    `[<4.0]` same probes (`lib_cmd_look_anywhere_pre_400`, 2026-09-19)
  - The rest of therest()'s cascade works the same way: every
    `If c("<verb>") Then msg = ...` arm tests the WHOLE line and the LAST
    matching arm wins, so `push and pull stone`, `push stone pull` and
    `drink push stone` answer as pull/pull/push, `sing and dance` dances,
    `stone jump` jumps and `please push stone` is "Your kindness gets you
    nowhere.". Arms that need an empty message (talk, block and lock at
    3.7/3.8; only talk at 3.9) only win when no earlier arm has.
    Handlers above therest (open, read, examine, take, ...) still answer
    first: `open stone push` = "You can't open the stone!". Scarier moves
    the winning keyword to the front of the line and dispatches the
    library again. `[<4.0]` p38ASK/p39ASK, `cmdfile_pkw.txt` (run380x
    Adrift_128_pkw38.rtf, run390x Adrift_130_pkw39.txt), 21/21 cells at
    3.8 and 20/21 at 3.9 (`go stone`, above) (`run_therest_pre400`,
    2026-09-19)
- **Spent tasks:**
  - Pre-4.0 checktask writes a spent task's RepeatText to the buffer and
    keeps scanning: a later passing task still runs, and a later
    restriction's message replaces the text. `[<4.0]` journ2, vampire,
    merry_murders (cc7470bf8)
  - A spent task's RepeatText outranks the library. `[4.0]` p4REPEAT
    (4e53b89ce)
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
  - Character pronouns have no-antecedent seeds: "No male" / "No female" at
    3.9+, "Nobody" below. showtime
  - A 3.9 topic reply overwrites the task text. `[3.9]` zombies, ms_mobius,
    alchemist (8c4d3260b)
  - Which topic answers an ask is one loop in every Runner: every subject
    of every topic, no break; a match whose chosen reply (AltReply once the
    Task is done) is empty does not count; a "*" subject answers only while
    nothing has; the last answer stands. From 3.9 a subject matches by c():
    whole word, case-insensitive, text may go on after it. p39ASK/p4ASK/
    p38ASK (make_{39,400,38}_askprobe.py)
- **Case handling.** Every Runner lower-cases input, but the character
  resolver's tail is case-sensitive, so a SYNONYM carrying a capital makes
  its NPC unreferenceable. bandera (0390eb300)
- **Question prefixes.** "Wear what?", "Remove what?", the give prefix,
  checkverb on a bare verb and "...with?" store the line as a prefix for
  the next input. The "with?" line is not a turn. `[4.0]` (daf951a81)
- **The " with " split.** therest resolves both halves: "don't have",
  "Don't be daft!", or a " with <X>" suffix on about thirty verb refusals.
  Corners: open/close/read/fix/clear with, and take looking up only `get
  <name>`. `[4.0]`; run390 twin `lib_with_clause_390` (daf951a81,
  b526c013b)
- **Line endings:**
  - A task that ends the game takes ALL of therest off the line, not just
    the unhandled-verb tail: the "You can't <verb> X" arms go too, an empty
    buffer prints DontUnderstand, only the catch-all subset is kept. `[4.0]`
    relojero, easter, iachini T185 (f038d76bf,
    STANDARD_ENDED_FALLBACK_COMMANDS, ab85a3e4e)
  - CompleteText is tested raw: a task text of just spaces counts as
    output, so no DontUnderstand follows. `[4.0]` wumpusrun T10
    (task_run_task_unrestricted, ab85a3e4e)
  - The catch-all tests the line-top object's presence after the task, and
    that answer is a turn. `[4.0]` seaside `do form` (24dcc8e5a)
  - A line a task answered that names a term two present NPCs share is not
    a turn. `[4.0]` sun_empire, salutations (8c4d3260b)
  - The object analogue: when a task ran and the line names an NPC (or is a
    `give` with the NPC present), characters() runs co(obj, 0) over every
    object, and the last object whose name word is on the line decides. Two
    or more present, seen namesakes leave the pending index set, which
    skips the tick and prints the task's text with no question. `[4.0]`
    cyber2 T14 `give electric uniform to lightning` (two "Uniform"s; 44 =
    44 draws), p4TAMB `poke toy` with no NPC IS a turn
    (`lib_co_400_line_leaves_which_pending`, 2026-09-19)
- **Administrative turns and the counter:**
  - An NPC examine and a nothing-found examine are administrative turns;
    so is `read` via examines. `[4.0]` EV14-16, house T150
  - Only examines' "see no such thing" sets the flag; characters()' NPC arm
    sets none, so `x <npc>` whose line has a unique seen-but-absent object
    as the winner is a turn. `[4.0]` humbug T634 `X robot`
  - In run390 hint, help, clear, time, version, save, restore and undo are
    ordinary turns, and the counter counts every line element, so `both`
    counts twice. `[3.9]` p39ADMIN (a211db2f1, b526c013b)
  - There are no administrative turns and no startup tick below 3.9.
- **The room refusal** runs inside the library, ahead of therest. `[3.9]`
  (9fbb40881) Before 3.9, drop, put and give refuse ahead of it. `[<3.9]`
  cave, greatc (f83e1cf87)
- **Pre-4.0 give to a present NPC** runs below the room refusal (run390
  characters(), run380 therest()), and run390's give writes only into an
  empty message or one holding " might need " / "I don't understand". So a
  Where=0 task matching the line wins with "You can't do that here!".
  `[<4.0]` the_hangover T42 (run_standard_give_npc_commands; the rows are
  deferred out of the verb pass). At 4.0 the give sits above the refusal.
- **No lock handler before 4.0.** Pre-4.0 carries only therest's checkverb
  " can't lock " / " can't unlock "; " is not locked!" and the key messages
  are run400's alone. Every pre-4.0 lock/unlock line is therest's and loses
  to the room refusal. `[<4.0]` thetest_win T68-77 (fe64ab0f3)
- **run380's post-take-from task sweep.** tra `get meat` also runs `get
  *knives*`. `[3.8]` (4f79695e4)
- **The player-name prompt** splits by version (Workflow step 2).
  (39bfe4cec)
- **Meta commands:**
  - `stats` is in no Runner; `status` exists only inside 3.9/4.0 dobattle.
    suburbanprodigy3
  - 3.9's `status` is three tab-joined rows (Stamina, Hit strength, Defense
    value, each `value (max)`). `[3.9]` the_town_of_azra
    (`lib_print_battle_status_390`, 2026-09-14)
  - The Runner's `past`, `bye`/`end`, `endgame`, control panel and quit
    decline text are ported (6033124f5).
  - `undo` answers by version: none at 3.7, "I can't undo your
    blundering." at 3.8, "Undone." / "I can't undo any more of your
    blunderings!" at 3.9/4.0 (6314d19d4). At 3.9+ it replays the restored
    turn's output (1c834df1b).
- **A task an event or walk runs is "a task ran"**, so the end-of-turn
  "Which X." prompt stays silent. 3.9 examine then answers an ambiguous
  pair "Nothing special." (co() is false for each, referencedob returns
  -1). `[3.9]` cybercow_win T118 `x berry` (`run_note_dispatched_task_ran`,
  2026-09-19)

### Nouns, scope and the seen model

- **Nothing is referenceable until something lists it.** The loader seeds
  the seen byte and afteroa sweeps it (statics only at 3.9). A unique
  absent-seen winner answers "can't see X from here!" and ticks; a tie or
  no winner answers "see no such thing" and does not tick. `[3.9+]`
  (386c9c570). A part-of-character static is stamped seen by obhere when
  its holder is the player or a seen NPC in the player's room. `[4.0]`
  humbug T727 `X teeth`
- **463640 is the 4.0 noun resolver.** Scoring: Short whole word +1, first
  alias +1, +1 per Prefix word, an empty Prefix counts as `a`. With two
  objects named, a tie gives the game's DontUnderstand. NPCs are never
  candidates. `[4.0]` House throw (0743cefef). therest's absent-seen clause
  scores every object the line names. `[4.0]` warlord, house doors
  (5662e7397)
  - Mode 2 (a plain `drop X`) scores held objects first, then everything
    present; the best score carries over. A tie prompts `Which <term>.`
    only when Me(424) is set, which happens by comparing each tied object's
    Short with the object TWO indexes past the previous tie, else "It is
    not clear which <term> ...". `[4.0]` wilkins T110-T117
    (`lib_drop_resolve_400`, 2026-09-19)
- **4.0 named take.** The take falls back on every seen object: "nothing
  worth taking here" / "not clear which" (p4TAKE, 7d051a7d7). It runs the
  tasks' `get <the object>` before both refusals (icecream T0, 8f2898bd4).
  A refused take leaves the typed line (take becoming get) to the task
  dispatcher (3eefa06b2). `[4.0]`
- **Auto-"from" take.** 4.0 retakes a seen object "from" its holder, and
  the referenced object is cleared before each typed line, so a type-1
  Var1=0 restriction is silent. `[4.0]` warlord T104 (68bc1382a)
- **Examine:**
  - referencedob answers a tie with an absent, unseen object: "You can't
    see that." `[4.0]` warlord T72/T76 (b6d2f4f1f)
  - `x <character elsewhere>` answers "You cannot see <Name> from here."; in
    4.0 it also needs the NPC's seen byte. (233e1b9c8)
  - The object-ambiguity prompt is `Which <term>.  <NP> or <NP>?`: examine
    prompts on a Short-or-Alias tie, an unhandled verb only on a Short tie.
    It has a pending answer slot. `[4.0]` p4CO (5d90e8793)
  - 3.7-3.9 co() has its own prompt, which replaces the output while the
    action still happens (mikes). 3.9 counts and lists only seen namesakes:
    troll T64 `drop cup`, secret_of_lost_world T56 `take scroll`. `[<4.0]`
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
  (e478cdbd6). 3.9 stamps the same way (room only when the player's,
  into/onto only when the parent is seen, held/worn by the player always,
  a character's room when it is the player's), except that an object handed
  to or worn by a character is never stamped. `[3.9]` secret_of_lost_world
  T52/T53 (2026-09-19)
- **Darkness** is the condition AND HideObjects, and it gates the seen
  flag. A dark examine answers "can't see X very clearly." `[<4.0]` p39DARK
  (9aff3fda9)
- **Absent-object refusals:**
  - co() matches anywhere; the handlers refuse with Prefix + Short, and in
    takes() the last match speaks. `[<3.9]` p37EXAM/p38EXAM (b526c013b)
  - takes() walks every object in index order and lets each one co() names
    write; a HELD namesake below the target says "You've already got X!"
    and the target is never taken. 3.9's co() mode 1 recounts loose room
    objects and falls back to the Prefix's last word, so deardiary T52
    `take blue plate` still takes the blue one. `[<4.0]` stardust T38,
    secret_of_lost_world T53 (`lib_co_pre400`, 2026-09-19)
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
  - `turn` and `pull` on a seen, absent object: "can't see X." `[4.0]` hcw
    T81, grumble T207 (b6d2f4f1f, `lib_cmd_verb_absent_400`, 2026-09-15)
  - `open`/`close` act on 463640's unique present-and-seen winner over the
    whole line even when the parser bound nothing: `open phone book` opens
    the held Cell Phone (alias "Phone"). `[4.0]` xfiles T62
    (`lib_open_close_resolved_400`, 2026-09-15)
  - `turn on/off` refusals append the particle. `[4.0]` thepkgirl
  - `kiss` answers "I'm not sure she would appreciate that!". `[3.9+]`
    (07bbd664d)
  - sit, stand and lie need the object on the room floor. `[3.8+]` house
    T124 (4e7df6dff)
- **A task's `%object%` binds only a seen object** (run400 matcher and
  run390 checktask alike). `take cushion` with the cushion lying unlisted
  on the pile misses the task, and the library answers "Take what?". The
  present-before-absent pass order is not ported. `[3.9+]` Glum_Fiddle
  T16-22 (`uip_match_entity`, 2026-09-15)

### Put and take-from

- **4.0 put/task precedence.** A completable library put beats a passing
  task. A size or capacity refusal prints without claiming the line, and
  the task follows. The implicit take is gated on a mode-1 pre-match and
  runs before the put handler's task look-up. The class-filter bytes are
  104/105. `[4.0]` House, frustrated, ShadricksUnderground (794ed4cd3)
- **4.0 put wording.** The container is named first: "Where do you want to
  put...?", "...put things inside/onto." (no turn), "can't put anything
  inside/onto X!" (p4PUT, 1ad720476). A static is named by put ("can't
  take X!"), and an empty hand answers "You are carrying nothing!" (p4PSTAT,
  0f061d3ab). `put X in Y` where X names nothing present clobbers the line
  to `put X `, so the catch-all speaks; a silent unnamed put also leaves the
  answer to the catch-all (icecream, house T263; 8f2898bd4, 5a8ac82fb).
  `[4.0]`
- **4.0 single take of a worn object** counts worn as held, so `get <worn
  thing>` says "You are already carrying X." `[4.0]` 3monkeys T65
  (2026-09-19)
- **The put handler's own answers** (rules A-H): the bang; "is closed";
  "already inside" (4.0 only); the pre-4.0 put universe; the object's
  failure outranking the container's; `Put X inside what?`; the `put all`
  empty-list wording; drop reaching into a carried container. Measured on
  all four exes (ea58a1f17, f5a95d64e).
- **Put ON.** Below 3.9, put IN and put ON are one handler, and the target's
  kind picks the preposition. 4.0 holding only the supporter gives silence,
  then the catch-all. The 4.0 splitter keeps " on " on a tie.
  `make_surfprobe.py` (ada92cc47)
- **3.9 put and its task sweep:**
  - A named put that moved its object empties the buffer and runs the tasks
    on the typed line; the put text comes back only if nothing printed.
    `[3.9]` losttomb T85 (`lib_put_task_sweep_390`, 2026-09-14)
  - The sweep is claimed by the first object co() finds named in the line
    among those held, lying in the player's room or worn; the claimant runs
    checktask QUIET (a failing restriction restores the buffer). Nothing
    eligible named leaves generaltasks' pass to run LOUD with the put text
    still in the buffer. troll T116 (quiet), losttomb T85 and
    secret_of_lost_world T118 (loud) (`lib_put_sweep_claims_390`,
    `run_typed_line_task_commands`, 2026-09-19)
  - insides() resolves the single put's object with co(obj, 0) over every
    object (Short or Alias as a word, the prefix only when present) and
    moves the first seen, present, held-or-loose one; a prefix naming an
    absent namesake is not heard. `[3.9]` secret_of_lost_world T118 `put
    red gem on statue` moves the green gem (`lib_put_co_resolve_390`,
    2026-09-19)
  - insides() answers a put whose line names fewer than two objects (object
    absent or unseen, or no container named) BEFORE its task look-up: "Put
    <the X> inside/onto what?" or "You can't do that!", then the sweep.
    `[3.9]` cybercow T62 `put bones in robot` with the bones never made
    (`lib_put_refusal_first_390`, 2026-09-19)
- **A 3.8 in/on object with an unset parent** goes in the first container.
  (5cf3d7059)
- **The take-from handler's own answers.** The 3.9 insides() decision
  procedure; `empty` is take-all-from in 4.0 only. p39DARK/p4TFROM
  (2ab1a7c5d)
- **4.0 take capacity.** Each object is tested for size first ("<Your>
  hands are full.") and weight second ("<The X> is too heavy for you to
  carry at the moment."), even out of a container the player holds. A
  player-held object loads with its container field cleared, so it never
  phantom-weighs object 0. `[4.0]` wilkins T22/T107, businessasusual
  T20/T24, provenance T722/T724, riding_home T1
  (`lib_take_over_capacity`, 2026-09-15)
- **3.9 take-from capacity.** insides() tests size first and weight second;
  weight is waived when the container is held, size never. A single named
  take-from refuses per object. The all/and forms pre-count the objects
  that fit: if none, "<Your> hands are full." / "That is too heavy." and
  nothing is taken; otherwise nothing is refused per object and one summary
  follows ("<You> can't take any more, as it is too heavy." if any failed
  on weight, else "... as <your> hands are full."). `[3.9]` alexis T28,
  alexis_worn_cube T27 (`lib_take_from_over_capacity_390`, 2026-09-15)
- **4.0 put names a present-but-unseen object by asking the scorer
  directly.** The seen-gated %text% matcher can't name an object inside an
  unlisted-but-open container, but 463640 mode 2 scores every present
  object regardless of seen state, moves it, and leaves it unseen, so the
  name composer answers "that" for it. Fires only when the top parse's
  failure really is the seen gate. `[4.0]` hub T79
  (`lib_put_fragment_present_object`, `lib_put_print_object_or_that`,
  2026-09-19)

### NPCs, walks and battle

- **Walk announcements.** The announcement joins the turn's paragraph, and
  ALRs span the join. 4.0 capitalises the Name. sa.taf, p4WALKALR,
  p4WALKCAP
- **Walk steps.** A walk step is exact-tick gated (Merry_Murders). An empty
  game-start walk preempts for ever (FunHouse, iqsfot). A finished 4.0 walk
  is stamped -1 (the_pk_girl). A Hidden walk stop stamps the walker's
  location whether or not it moved (0e9c8af36).
- **The walk tick** stamps NPCs in the player's room seen first, and the
  player-side meet fires only on a typed move. `[4.0]` the_pk_girl T52,
  Laurie's walk-meet task 413 (d1c61b0c5, 2dfd964cf)
- **The player-side meet** skips a walk that a higher-numbered walk
  preempts, by the same test as the tick. The Runner reaches the CharTask
  through the dispatcher by command text; Scarier runs it by index, which
  the corpus does not tell apart. `[4.0]` greekschool, Paul's task 22
- **Absent NPCs.** dobattle's "<Name> isn't here!" is a turn `[3.9+]`
  (alexis_worn_cube, 41b1f93d3). The 3.9 character catch-all says "<Name>
  is not here!" if the NPC is seen and "Who?" if not; the reference test is
  Name or first Alias `[3.9]` (ALEXIS probes, f89d7c8ac). 4.0 matches Name
  or any alias, capitalised `[4.0]` (thepkgirl). With the Battle System
  off, an attack on an absent NPC answers "The <name> is not here!" `[3.9+]`
  (thepkgirl, 07bbd664d).
- **Battle:**
  - Narration names `<Prefix> <Alias[0]>`, corpse lines use the Name, and
    narration follows Perspective. orient_express (00ee24864, 161c822d8)
  - run400 capitalises an NPC attacker that leads its sentence, at five
    sites. (1409cade5)
  - dobattle names its target by Name alone at 4.0 (Name or first Alias at
    3.9). It strikes every NPC a line names, then asks about namesakes as
    an administrative line. A verb naming no NPC asks "Who do you want to
    attack?", continued into the next line. shadowpeak, light_up
    (327feeb9c, e4e85ea89, 3d59b700d, 13f13e66d, 512543515)
  - Type-7 attribute raises are capped at max with no zero floor, at 3.9
    and 4.0 alike (ungated); the max attributes are a plain add. The dodge
    pronoun is he/she/it by Gender. wes_ghn, les_feux (4c7c20f64);
    secret_of_lost_world T125-126/T164, spirits_flight T17/T27 (2026-09-19)
  - The attitude action stores Var3 RAW into the NPC's byte (0 neutral, 1
    ally, 2 enemy), no combo reorder. deaths T42/T48/T49 (2026-09-19)
  - The 3.9 speed action indexes the NPC by Var2 RAW, no
    referenced-character case. `[3.9]` deaths T35-T49 (2026-09-19)
  - The 3.9 blow has no accuracy roll: hit iff hitstrength >
    armourstrength, damage = max(0, hit - armour), no draw; hitstrength =
    strength + best weapon, armourstrength = defence + worn objects' field
    76; getnexthit at speed 1 draws Int(Rnd*1)+1, the rest are constants.
    Scarier's `battle_legacy` path; read 2026-09-19.
  - Stamina recovery is a per-line pass that revives the dead;
    `battle_select_target` takes 0-stamina targets. (544868698, e78827349)
  - A type-7 stamina action that leaves an NPC at <=0 kills it. The player
    dies from it only in 4.0; the 3.9 player arm has no test. `[3.9+]`
    cybercow_win T103 task 167 (`battle_change_attribute`, 2026-09-19)
- **Task move "to same room as" (Var2 = 2)** names its NPC by RAW array
  index at 3.9, in the NPC arm and the player arm alike: no player or
  referenced-character slots. run400 keeps 0 = player, 1 = referenced, N =
  NPC N-2. 3.7/3.8 movements never produce it. `[3.9]` fantasyworld task 92
  (the Royal Knight goes to King Harmon, not the Barmaid; T224-296
  identical). Corpus exposure at 3.90: fantasyworld's two, panic's one
  (`task_same_room_npc_390`, 2026-09-19)

### Events and RNG

- **Timing:**
  - There is no startup tick before 3.9; `delay N` starts on turn N.
  - `score` ticks events in 3.7/3.8.
  - A finishing event re-checks lower-indexed events in the same tick.
    Vardock
  - Rolls exclude the upper bound. A backwards range still draws, flooring
    toward minus infinity. hyper_b_s (a288e471b)
  - A zero-length event that starts after its tick this turn keeps the +1
    and parks until the next tick rather than finishing at once.
    riding_home T50, cursed T137 (2026-09-15)
  - An event that starts PAUSED (pauser done, resumer not) keeps the
    start's roll + 1: the pause test exits checkevent before the decrement,
    so after the resume it ends one turn later than its roll. thepkgirl
    T152-T171 (event 118), which moved the T312 "slips you a buck".
    (2026-09-19)
  - A 4.0 event whose restart ROLLS 0 (Time1 = 0) parks for good: the
    restart stores the roll with no +1, the next running block takes the
    clock to -1, and the finish test is `clock = 0`, so no PrefTime text,
    finish or restart draw ever follows. Pre-4.0 unmeasured, keeps the
    authored-length test. `[4.0]` zelda T52-60 (event 5; 468 = 468 draws)
    (2026-09-19)
- **Pre-4.0 ending mid-tick.** The ending (WinText, summary, "[Press any
  key to end]") is composed as the task that armed it finishes, right after
  the action loop. The prompt is no wait, and the tail's ended test was
  made before characters(), so a walk's task that ends the game is followed
  by events() in the same tick. Event texts join pre-4.0 with the two-space
  separator unless the buffer ends in Chr(10) or "  ". `[<4.0]` haunt T84
  ("...end]  You hear the chiming of the grandfather clock."). 4.0 keeps
  the end-of-turn endmessage. (2026-09-19)
- **Completed tasks.** A 4.0 event that runs a completed task still walks
  the task's restrictions, so a failing restriction prints its message and
  a passing one prints nothing. riding_home T47 (event 8, task 104)
  (2026-09-15)
- **Static objects moved by events.** A 4.0 static's presence is its
  per-room array o(28). The start mover (Obj1) replaces the rooms; the
  finish movers (Obj2/Obj3) only add one, clearing for hidden or held
  alone. The array starts as the Where list, so finish moves pile up.
  `[4.0]` 3monkeys T109 (the anvils in all four corners) (2026-09-19)
- **The phantom object.** The 3.9/4.0 object array is `0 To count` and
  loaded 0..count-1. The spare slot's zero position reads as "held", and
  the "all held" scan tests it before its exit, so the first "all held"
  move of a game also moves the phantom, and a roomgroup destination draws
  getaroom. After that it isn't held unless it's handed back. `[3.9+]`
  hhorror T25 (4979 = 4979 draws) (`task_move_phantom_object`, 2026-09-19)
- **Look text.** An event's look text is gated on the room being described,
  not on the player's room. goldilocks, cybercow
- **RNG parity.** `SCR_RNG=xoshiro` matches vbrng draw for draw, and it is
  the harness default (991a5f8d9, b150980c8). A death prints the end-game
  score summary. (5f20dd8ce)

### Output, wording and the room block

- **The room block is ONE string.** viewroom joins the description,
  InRoomDescs, "Also here", the "X is here." sentence, NPC texts and event
  LookTexts with pspace(), a conditional two spaces. The heading is `"\n" +
  name + "\n"`. A leading `<br>` collapses only against Scarier's own
  break. perspectives, datewithdeath, Vagabond (a4d61a288, 5bd9ace04)
- **Executed tasks.** Executed-task CompleteText and every
  AdditionalMessage join the turn with pspace. `[4.0]` the_pk_girl T156
  (07bbd664d)
- **Room listing.** It is decided by the OnlyWhenNotMoved byte, not by
  whether InRoomDesc is empty; the library take and a task move spend the
  byte (camelot15, zelda, fe67c45de). An InRoomDesc of one space counts as
  present (topaz, aeafcbf34).
- **ShowRoomDesc.** It prints before the task's actions, except that an
  empty CompleteText plus a non-empty AdditionalMessage builds it after
  them. `[4.0]` SRD4 (771d03b85)
- **ALRs.** The list is walked once, longest Original first; 4.0 recurses
  into each replacement, 3.9 does not (qui_a_tue_dana, d93a331b7). ALRs
  apply after characters() (c2b28179c). Originals ending in a space match
  (reluctantvampire T78, b6d2f4f1f). Double spaces next to pattern groups
  match (b1e45887b).
- **Variable substitution.** The 4.0 output filter freezes variables per
  completing task (humbug). User variables substitute in index order,
  Replace-all each, after the system tags `[3.9+]` (datewithdeath,
  8cf9cce63). `%status_<name>%` is the lowest-indexed openable Short match
  (briefcase, 32852b749).
- **Library wording:**
  - Third-person library text is not conjugated. herrdoktor (81231bc37)
  - Pre-4.0 says "You pick up" where 4.0 says "You take".
  - Pronoun echoes use round brackets. 4.0 takes the article from the last
    composing handler, and 3.9 keeps the authored one. 3.7/3.8 store
    tense(Prefix) & Short for any verb but splice only the Short into the
    line. wrecked
  - "(Getting off that first)" for an unseen parent. `[3.9+]` gateway
  - The 3.7/3.8 inventory listing does not claim the line, so a matching
    task's text follows it. wrecked T24
  - The 3.7/3.8 ask topic matches by substring, and the last match wins
    (wrecked T129/T211). A typed `<subject>` with no topic is "Smart Alec!";
    run390/run400 escape "<" at input, so 3.9 and 4.0 never reach theirs.
    p38ASK
  - The examine-self full stop is 3.9+.
  - The NPC examine overwrite applies at 3.9 too: a task's text is replaced
    by the NPC's description; the namesake check stays 4.0 only. `[3.9+]`
    cybercow_win T72 `x fairy` (2026-09-19)
  - Pre-4.0 read ends in the examine tail, the openness line and the
    contents. `[<4.0]` cybercow_win T97 `read envelope`
    (`lib_read_tail_pre400`, 2026-09-19)
- **Other formatting:**
  - The "<Name> is here." sentence is capitalised only by the 4.0 loader's
    `#` substitution. run390/380/370 append the raw Name, and no Runner
    capitalises an author's own " is here." text. `[4.0]` goldilocks;
    twilight T12-34 (2026-09-15)
  - `isare()` is exact and case-sensitive, and the loader fills an empty
    Prefix with "a". yeh (496c115f2)
  - 4.0 room names take every matching alt's Changed. togetyou (fee19ae2a)
  - The multi-take line comes before the earlier task text. `[4.0]`
    fullcircle T43 (b6d2f4f1f)
  - The (Getting off X first) bracket line prints on its own line.
    Monsters_r2
  - The score summary prints after every EndGame; NotifyScore defaults to
    OFF.
