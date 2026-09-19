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
- **Output filter:** where the ALR pass sees trailing spaces; the NewParse
  `%` pattern binary path; the drop rebuild at 46F33B.
- **Drop/take/wear setter branches** 46FB7D, 47C7F1 (and run390's wears at
  43D289): read, not measured.
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
- **Per-verb absent-NPC branches:** talk, give and take are measured and
  ported (p39ABSNPC). kill/kick/punch still go through other grammar
  first. The attack branch is ported.
- **Take "and" with a candidate present:** only the zero-candidate summary
  is ported. The main multi loop at 454CA3 after a partial pre-pass is
  unread.
- **run390's Who-prefix consumption** at 460022 is assumed, not measured.
- **Take-from:** the " and " clause picks the last container in 3.9 and the
  first in 4.0; 3.9's " and " collection bug; the pending slot after `Get X
  from what?`; surface-vs-container wording of the parent-derivation arm.
- **Put:** the " is full." arm at 461E59 speaks only when something fits
  and the bag is still full, so it is effectively dead; it needs a size-0
  object.
- **Drop:** the pre-4.0 "and" arm skips an object inside a held container
  (o(22) 0 or &H9C only); ported that way, not measured.
- **Two-object canonical prefixed retry:** the run390 half is not
  re-measured. The 4.0 half is closed.
- **Examine:** run390's examine state line has not been read.

### Engine, needs a probe (3.7 / 3.8)

- **Handlers other than take** were measured on single matches only.
- **run380's task sweep after a *refused* take-from** is not ported.
  run380's count<2 put refusal also precedes checktask, but run380 has no
  sweep (445A0F).
- **Examine:** 3.7/3.8 examines have no bare-verb exit (unmeasured
  corners).
- **3.8 `lie on bed` from the next room** is "You can't lie on that." and
  4.0's is "You can't see the bed."; both already match. The 3.8 object
  loop's scope test is unread.
- **Pre-4.0 room-name alt walk** is unmeasured.
- **Take "and" with nothing takeable:** run370 4361B3 and run380 43DCC1
  say " can't get any of them." with no "either" form. The pre-pass is
  unread and not ported; 3.9's is.
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
- **3.7/3.8 `put all in <nothing>` / `put all on <nothing>` crash the
  Runner.** run370x and run380x stop with "Run-time error '9': Subscript
  out of range" and lose the transcript (p37PUT/p38PUT,
  `cmdfile_p3738putallin.txt` / `cmdfile_p3738putallon.txt`, 2026-09-19).
  `put everything in zzz` crashes both the same way (`cmdfile_p3739drop.txt`
  cmd 5, par/p37drop.log). Scarier keeps its sane answer.
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
- **run400's carried-weight cycle on dynamic object #1** (found 2026-09-19,
  closes the "silent put confirmation for object #1" lead). 447680, the
  recursive "weight of an object and its contents" walk, sums every object
  whose parent field (record +46) equals the object's index and never looks
  at the position, and the parent field of an object that has never been
  inside or on anything is still 0 = object #1. So the first put or drop of
  object #1 into a container, or onto a supporter, that has never itself
  been moved into anything makes a #1 <-> container cycle: move_object
  4528D8 has already written both fields when it calls the walk (4527FD),
  the walk dies of "Out of stack space", evaluate's handler (457298,
  Proc_19_76_4467A0 "evaluate error") swallows it and the turn prints
  nothing. The object IS inside, and every later weight walk that reaches
  the pair dies the same way, so `take <object #1>`, `take <container>`
  and `put all` are silent no-ops from then on. The old "`put all on
  <supporter in the room>` moves only the first" bullet (surf3/surf6,
  Adrift_995/1016) is this bug: `all` moves object #1 first, and the held
  supporter was immune (Adrift_997) because taking it had written -1 into
  its parent field. Measured on put7.taf: `put jar in box`, `take jar`,
  then `put bean in jar` prints and the bean comes back out
  (Adrift_put7_cycle); `put all in jar` moves only the bean
  (Adrift_put7_all); the vbrng.dll stack sampler caught the 447600-44767A
  frames (Adrift_put7_trace, `VBRNG_SAMPLE`). run390 has no recursive
  weight walk (its only self-recursive procedure is the `then` splitter
  42D820) and prints normally. Scarier prints the confirmation and moves
  every item. No corpus row reaches it.
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

- **`goto <place>` / `go <place>`:** gotoplace runs after the tasks and
  meta commands and before the room refusal and therest, so it outranks
  "Just a direction will do.". 3.9+ takes `goto` anywhere or a line
  starting `go `; 3.7/3.8 take `goto` or a line starting `go to` (so `go
  tower` asks for a place called "go tower"). The name matches exactly,
  then as a substring. Answers: `Which "x"?` list, "You are already x!",
  "You can't get there from here." (named but unreachable), "Unknown
  place.". A walk prints "Moving to x...", types each step (the last
  direction leading to the next room) as a turn of its own, then "Arrived
  x."; the goto line itself is no turn. The route finder tests every exit
  restriction as a task, and 4.0 uses visited rooms only. A bare `goto` is
  DontUnderstand. p38GOTO/p39GOTO/p4GOTO (`harness/make_3{8,9}_gotoprobe.py`,
  `make_400_gotoprobe.py`; run380x Adrift_132_pgoto38.rtf, run390x
  Adrift_133_pgoto39.txt, run400x Adrift_133_p4goto.txt, bare goto
  Adrift_133_pgoto38b.rtf / Adrift_134_pgoto39b.txt) (`lib_cmd_go_place`,
  2026-09-19)
- **`goto` with more on the line** (`go to kitchen, look`): each walk step
  runs as a nested generaltasks. Pre-4.0 keeps the split queue in a local
  (var_E4), so the walk and "Arrived" come first and the rest of the line
  follows in the same turn. run400 keeps it in a global and empties it at
  the top of every generaltasks (48A01F), so a goto that walks throws the
  rest of the line away; one that doesn't walk ("already", "Unknown
  place.") keeps it. Pre-4.0 examines() runs ahead of gotoplace and takes
  any line with one of its entry words (x, examine, look at, ex, exam,
  read; 3.8+ look in; 3.9 also look, l), so `go to kitchen and read` is
  "Nothing special." (3.8 `... and look` stays "Unknown place."). Feeds
  `~/adrift-battle/runner/wine/cmdfile_pgotosplit*.txt`: run390x
  Adrift_135_pgs39.txt / Adrift_136_pgs39b.txt, run400x Adrift_136_pgs4.txt
  / Adrift_137_pgs4b.txt, run380x Adrift_134_pgs38.rtf /
  Adrift_135_pgs38b.rtf (`run_goto_rest`, `lib_cmd_go_place`, 2026-09-19)
- **3.7 goto:** answers as 3.8. run370 also takes the game's own word for
  goto (command slot 15) anywhere, leaves on it alone, and cuts its length
  plus one off the front before the goto cuts: `rove kitchen` walks, `a
  rove hall` walks to "blue hall" ("e hall"), bare `rove` is
  DontUnderstand, `goto kitchen` still walks. It is no synonym (sctafpar
  V370 fixup). `[3.7]` p37GOTO/p37GOTOW (`harness/make_37_gotoprobe.py
  [word]`; run370x Adrift_136-141/143) (`lib_cmd_go_place`, 2026-09-19)
- **3.7 therest refuses an absent object first:** a line that reaches
  therest and names an object that is not here is "You can't see the X."
  (definite) before any verb arm (43D169): go, enter, push, smell, kiss,
  turn, jump, sing, look, climb, sit on, fly. Lines holding an earlier
  handler's word (take, put, wear, x, read, open, give, wait, where, goto,
  question words, ask/talk/say) keep their own answer. `[3.7]` p37GOTO,
  run370x Adrift_142_p37cantsee.rtf (`run_therest_absent_370`, 2026-09-19)
- **`wait` anywhere, every version:** `If c("wait") [Or line = "z"] And
  msg = ""` answers "Time passes..." and the wait turns (run370 43C1B3,
  run380 442A07, run390 45FCA2, run400 48ABB8; 4.0's c() too). It comes
  after the tasks and the handlers that enter on words anywhere in the line
  (take, drop, wear, remove, sit/stand/lie, open/close, examines, score,
  swearing), and before whereis, gotoplace and therest. So `wait stone`
  (stone elsewhere), `please wait`, `wait here`, `push stone wait` and
  `turn wait` pass time; `waiting` does not. `look wait` passes time at
  3.7/3.8 and is examines' from 3.9. run370's openclose writes nothing
  unless the object is openable, so 3.7 `open stone wait` passes time,
  while 3.8+ say "You can't open the stone!". gotoplace still runs after
  it: `goto hall wait` is "Time passes..." then "Unknown place.". A goto
  that walks jumps past the message print and the end tick (run370 42BDEE
  "&&&"), so only "Moving to..." shows and only the loop's WaitTurns - 1
  ticks run. run370x walks `wait goto hall`, cutting the game's goto word
  and then "goto" from the front. Lines with give, ask, talk, say,
  inventory or a direction keep their old answer (unmeasured). p37GOTO,
  p38GOTO, p39GOTO, p4EXAM, feeds
  `~/adrift-battle/runner/wine/cmdfile_pwait.txt` / `cmdfile_pwait4.txt`
  (run370x Adrift_144_pwait37.rtf, run380x Adrift_145_pwait38.rtf, run390x
  Adrift_146_pwait39.txt, run400x Adrift_147_pwait4.txt)
  (`run_wait_anywhere`, 2026-09-19)
- **Unknown verb, pre-4.0 catch-all:** a line no handler answers walks
  every object it names, in index order. A seen, present object gives "I
  don't understand what you want me to do with X."; a seen, absent one
  gives "<player> must be in the same room as X to be able to do anything
  with it."; an unseen one gives "What <Short>?". The present answer beats
  the absent one, and an unseen object speaks only if nothing else has
  (run380 442F5D-443134). 3.8's therest checks only the first *present*
  object (443C69), so `frob stone`, `z stone` and `eat statue` from the
  wrong room get the same-room answer, and `push statue` is "You push, but
  nothing happens.". 3.7's therest refuses absent objects first ("You
  can't see the X.", whether seen or not), so there only the "What X?" arm
  shows. In 3.9, co() matches only present, seen objects, so an absent-only
  line is DontUnderstand. 4.0 is unchanged ("You can't see the X."). The
  pre-4.0 eat arm speaks only for a present object, and "I don't
  understand what you are trying to eat." is 4.0's (4889C7). **Seen stamp,
  3.7/3.8:** the generaltasks pre-pass (run370 43B6C6, run380 441F21) runs
  on every line that names zero objects or two or more (Short/Alias,
  anywhere). It marks every present object seen: held, worn, loose on the
  floor, and statics in the room. It never touches container contents. So
  `n` reveals the room you leave, and `frob stone` (one name) does not.
  p37EXAM/p38EXAM/p39EXAM/p4EXAM, feeds `cmdfile_pverb.txt`,
  `cmdfile_pseenA.txt`, `cmdfile_pseenB.txt` (run370x Adrift_146_pverb37 /
  Adrift_150-151_pseen*37.rtf, run380x Adrift_147_pverb38 /
  Adrift_148-149_pseen*38.rtf, run390x Adrift_148_pverb39.txt, run400x
  Adrift_149_pverb4.txt) (`lib_verb_object_catch_all_pre390`,
  `lib_prepass_seen_3738`, 2026-09-19)
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
  - The four 4.0 cuts are passes in that order, each on the head the
    previous pass left, and the tail's first word is read inside that head:
    `x coin and box, x hat` cuts at the comma (the `, x hat` tail's word
    "x" names nothing) and never at ` and ` (its tail word is "box", an
    object). What the queue receives is the later tail followed by the
    older one. `[4.0]` p4AND Adrift_956 (`run_find_split_400`, 2026-09-19)
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
    3.8 and 21/21 at 3.9 since gotoplace was ported (`run_therest_pre400`,
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
  - run380 counts line elements too: its counter goes up at the top of
    generaltasks (441A21), so `turns` counts itself. `[3.8]` p38ADMIN
    Adrift_1202: `look probe clear cls clr turns` answers 6 (Scarier said
    5). run370 has no counter and answers `turns` "I don't understand."
    (p37ADMIN Adrift_1203; the `turns` version gate is policy).
- **clear.** Bare `clear`/`cls`/`clr` empties the window and prints "Screen
  cleared.", a turn below 4.0 and administrative at 4.0. Any other line
  holding the word goes to therest's clear arm: "You can't clear the rope."
  for an object, "You can't clear that." for a word naming nothing, a turn
  at every version. `[all]` p37ADMIN/p38ADMIN/p39ADMIN/p4WITHQ2,
  Adrift_1202-1205 (lib_cmd_clear_other). Scarier said "I don't
  understand." below 4.0 for both forms and at 4.0 for the second.
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
- **A whole-line take or drop with two names and no `and`** (`get coin,
  hat`, `drop coin, hat`, kept whole because "hat" names an object): the
  handler resolves the whole fragment with the noun scorer (463640 mode 1
  for take), and a tie between objects that share no name answers "It is
  not clear which <last tied object's typed name> you are referring to."
  and moves nothing; a unique winner is the only object taken, whatever
  else the line said. Two namesakes still get the "Which" question. c()
  ends a word at a space, `,`, `.` or `?`, so the comma-bound "coin" scores
  too. The take scorer's candidates are dynamic, seen, visible objects; the
  first pass leaves out anything held or worn (or inside something held),
  and a second pass admits them only when the first found no unique
  winner. `[4.0]` p4AND Adrift_955 (`lib_take_tie_400`,
  `lib_take_resolve_400_string`, `lib_drop_named_400`,
  `lib_input_contains_word_400`, 2026-09-19)
- **`get X and Y` with neither present:** the "and" list loop marks
  nothing, and the take handler prints "There is nothing worth taking
  here." with no per-object refusal; one present object takes it alone.
  `[4.0]` p4AND Adrift_956 (`lib_cmd_take_absent`, 2026-09-19)
- **`x coin and a hat`:** referencedob's prefix-word pass counts "a" for
  both objects, the scores tie, and the examine handler prints "Sorry, I'm
  not sure which object you're referring to." as a turn of its own (4719EA);
  `x coin and the hat` / `and large` examine the coin. `[4.0]` p4AND
  Adrift_955 (`lib_examine_referencedob_400`, `lib_disambiguate_object`,
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
- **Sit, stand and lie follow each Runner's sitstand.** `[all]` p37SIT,
  p38SIT, p39SIT, p4SIT (`make_3738_sitprobe.py`, feeds
  `cmdfile_p3738sit.txt` / `cmdfile_p3738sit2.txt`: run370x
  Adrift_162/164, run380x Adrift_163/165, run390x Adrift_164/166, run400x
  Adrift_166/167; `lib_stand_sit_lie`, 2026-09-19)
  - No object arm asks whether the player is already there: `sit on stool`
    twice sits twice. Sitting on an object is "sit down on" even from
    lying. Before 3.9 the object is named by its authored Prefix ("You sit
    down on a stool.").
  - Bare `stand` at position 0 is "already standing!", even standing on an
    object.
  - Pre-3.9 bare `sit`/`lie` keep the parent and never name it ("You sit
    up.", "You lie down on the ground."); only `stand` clears it ("You
    stand up from the stool."). 3.9+ names it: `sit` standing on O is "sit
    down on the O"; lying on a sittable O is "sit up on the O"; `lie` on a
    lieable O is "lie down on the O", otherwise "on the ground." and the
    parent goes.
  - 3.9+ `sit on the ground` on the floor is "are already sitting on the
    floor!" (or "ground!") with a literal "are".
  - Pre-3.9 moveroom only looks at the position: standing on an object,
    a move prints no "(Getting off ...)" and the parent survives it
    (run370 422FD0, run380 428244). `stand on crate`, `s`, `sit`, `stand`
    is "You stand up from the crate."
  - run370's object loop matches Short or Alias with no scope or location
    test, the last match winning (42AE7D), and runs before therest's
    "can't see" test: `sit on bed` from the next room sits on it
    (`lib_cmd_sit_scan_370`, `lib_sitstand_claims_370`).
  - 4.0 absent sit/lie targets are therest's "You can't see the X.", as
    stand already was (`lib_cmd_verb_absent_400` rows).
  - `sit`, `lie` or `stand on the ground/floor` is "You can't sit/lie/
    stand on that." when the line names on/in, except 3.9+ sit, which has
    a ground arm (run400 46BB03, run390 444807). Feed
    `cmdfile_p3738sit3.txt`: run370x Adrift_166, run380x Adrift_167,
    run390x Adrift_168, run400x Adrift_169 (`lib_floor_named`).
  - 3.9+ `get off X` answers "You are not standing on anything!" before it
    looks at X (run400 46B702, run390 4443E1).
  - Before 4.0, takes() runs first and excludes only `get on` and `get
    down` (run390 4544C6), so `get off stool` is a take. 3.7/3.8 have no
    get-off: "You pick up the stool.", "You can't take a chair.". In 3.9 a
    take that happened stands (45F439 leaves generaltasks); otherwise
    sitstand's answer replaces it.
  - The 3.7/3.8 static take refusal names the raw Prefix: "You can't take
    a chair." (run380 43E697, run370 4369FF).
  - sitstand is blocks entered on c("sit"), c("stand"), c("lie") ANYWHERE
    in the line, run in code order and each overwriting the one message
    (run390 444010 called at 45F50D, run400 46B370). `sit lie`, `lie
    stand`, `stand up sit down lie down` are "You lie down on the ground.";
    `stand sit` and `sit stand` "You stand up."; `please sit`, `sit
    quietly`, `push stone sit`, `open stool sit`, `wear coin sit` and
    pre-4.0 `sit and wait` "You sit down on the ground."; `sit on stool
    lie` lies on the stool; `sit on chair stand on stool` stands on the
    chair (last object in index order); `x stool sit` examines and still
    sits. Pre-4.0 `sit on stool and lie on chair` lies on the chair. Lines
    holding take/drop/inventory/give/ask/talk/say/direction/score/hint/
    profanity words are left alone (unmeasured); a successful wear or
    remove on such a line is unmeasured. Feed `cmdfile_p3738sit4.txt`:
    run370x Adrift_168, run380x Adrift_169, run390x Adrift_170, run400x
    Adrift_171 (`lib_sitstand_anywhere`). Volant `stand your ground` (solution line 45)
    is "You are already standing!" (Adrift_256).
  - `lay` is a lie word in run400 only (46BACE): 3.7-3.9 `lay down` is "I
    don't understand." and `lay on stool` therest's "I don't understand
    what you want me to do with the stool." (`lib_lay_pre400`).
  - 3.7/3.8 `x me` with an empty PlayerDesc is one string (run370 435AED,
    run380 43D43E). "circumstances." gets its full stop only before the
    sitting or lying clause. Standing on an object is "...the circumstances
    You are standing on a stool.", with two spaces and no full stop. The
    object is named by its raw Prefix.
- **A task's `%object%` substitutes the bare Short or Alias, and nothing
  else.** No Prefix, no article: `pa brass key` runs a `pa %object%` task
  over Short "brass key" / Prefix "a small", and `pa key`, `pa a brass
  key`, `pa the brass key` and `pa small brass key` all miss it. 3.90
  folds case, 4.0 does not; before 3.90 `%object%` matches nothing. A
  missed line falls to the library, whose noun resolver is prefix- and
  article-tolerant, so `rub red box` over `rub %object%` answers "You
  can't rub the red box." while `rub box` runs the task (p4TAMB, the
  former "side finding"; Scarier already matched). `[3.9+]` p39CASE
  Adrift_1_p39case (`uip_compare_reference_strict`, 2026-08-25; TAMB
  closed 2026-09-19)
- **A task's `%object%` binds only a seen object** (run400 matcher and
  run390 checktask alike). `take cushion` with the cushion lying unlisted
  on the pile misses the task, and the library answers "Take what?". The
  present-before-absent pass order is not ported. `[3.9+]` Glum_Fiddle
  T16-22 (`uip_match_entity`, 2026-09-15)

### Put and take-from

- **3.9 take "and" with nothing takeable.** takes()' "and" arm pre-passes
  the objects co(obj,1) names. When none of them is seen and loose in the
  room, or in or on something here, it prints "You can't get either of
  them." for exactly two named, else "any of them." (454B08-454B5B). A held
  stone is named but never a candidate. `[3.9]` p39ABSNPC T36
  (lib_take_and_none_390)

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
  - The target refusals also come before the task look-up, then the sweep:
    "You can't put anything inside/onto <the X>." (wrong kind), "... as it
    is closed!", and "You can't put anything inside/onto that!" (two or more
    objects named anywhere in the game, none after the preposition, no
    "all"). A failing task with a claimant leaves the refusal; with none
    (both objects in a bag) the LOUD FailMessage replaces it; a passing task
    replaces it. `[3.9]` pPUTREF39 (make_39_putrefprobe.py),
    Adrift_pputref39.txt / Adrift_pputref392.txt (`lib_put_refusal_sweep_390`,
    `lib_put_that_390`, 2026-09-19)
  - A put that comes to nothing but a size or capacity refusal moves nothing,
    so there is no sweep: generaltasks' LOUD pass gets the line and a
    matching task's FailMessage replaces the refusal. A matching FAILING
    task never claims a put that fits (pPUTCLM39: literal, wildcard,
    %object% and empty-FailMessage tasks all let the object move). `[3.9]`
    pPUTREF39 `put coin in bag` → "T6 FAIL." (`lib_put_named_pre400`,
    2026-09-19)
  - Capacity decodes the count from the FIRST digit only:
    Val(Left(Format(cap, "00"), 1)) × mult ^ Val(Right(..., 1)), so 100 is
    one size-1 object, not ten (run390 46537C). run400 takes Left(s, Len(s)
    - 1). `[3.9]` Adrift_pputrefv4.txt (`obj_get_container_capacity`,
    2026-09-19)
  - The target is chosen after InStr(line, Left(var_E0, 2)), a raw
    substring search, so the "in" inside "coin" or the "on" inside "stone"
    counts as the preposition. `put coin and stone in junk` targets the
    stone: "You can't put anything inside the stone." A valid target gets the
    other named objects put into it: `put stone and tray on junk` gives "You
    put the stone onto the tray." `[3.9]` Adrift_pputref39.txt:8,
    Adrift_pputref393.txt (`lib_put_target_390`, 2026-09-19)
  - The all/and put counts before it moves. It walks the named objects in
    index order and counts each one whose size fits in the space left after
    the ones before it. It then moves the FIRST that many, whatever their
    sizes. If any are left over it adds "  You can't put any more inside
    the bag as it is full." A count of 0 gives "Nothing will fit inside
    the bag." and names no object. A single object keeps "can't fit ... at
    the moment". `[3.9]` Adrift_pputref394/395/396.txt, the last on
    pPUTREF39E (`make_39_putrefprobe.py --emptybag`) (`lib_put_in_backend`,
    2026-09-19)
  - `put all in/on <nothing>`: c("all") skips the two-name test (461646)
    but not the target choice. With nothing named after the preposition
    the answer is "You can't put anything inside/onto that!", then the
    sweep. `[3.9]` pPUTFULL39 (make_39_putfullprobe.py),
    Adrift_pputfull39.txt (`lib_put_that_390`, 2026-09-19)
  - A present static named as the object: insides() counts only movable
    dynamics (461AF8), so the target's refusals come first, then "You can't
    see that." (4624EF) and the sweep. An object it cannot reach takes the
    same refusal-first path. `[3.9]` pPUTFULL39 `put statue in cupboard`
    (`lib_put_named_pre400`, `lib_put_not_reachable_pre400`, 2026-09-19)
  - Also measured identical on pPUTFULL39, with no code change:
    - an object on a floor supporter gives "You can't do that!";
    - the onto and-arm over capacity puts every candidate, floor objects
      included;
    - a static target in another room gives "Put the coin inside what?";
    - a full bag gives "can't fit ... at the moment" / "Nothing will fit";
    - `drop coin in junk` / `drop coin and stone in junk` are drops ("You
      drop the coin." / "... the stone."), so the drop spelling never
      reaches the "that!" refusal.

    Pre-4.0 has no lock state (the V390 schema reads no Key, and there is no
    "locked" string), so "locked container" is moot.
  - Pre-4.0 drops() picks its arm by c("all"), then c("and"). The "and"
    arm drops the named held/worn objects that no "drop <Short>" task
    claims, and it never names a missing one. With none dropped it says
    "You are not carrying anything." (run390 445841, run380 4388E6).
    `[3.7-3.9]` pPUTFULL39 `drop coin and stone on junk`
    (`lib_drop_and_arm_pre400`, 2026-09-19)
  - Player MaxSize/MaxWt use Val(Left(Format(v, "000"), 2)) × mult ^
    Val(Right(v, 1)) in both the run390 and run400 loaders (464B14,
    48F980). This matches the plain decode below 1000 (the largest in the
    corpus is 994), so it is not a divergence in practice.
    (`obj_convert_player_limit`, 2026-09-19)
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
- **4.0 `put box in box` takes the box first.** insides tests possession
  (44615C @465EED) before the itself-test (arg_10 = arg_C @465FA0), and
  name_object's take piece has already run by then: with the box on the
  floor run400 prints "(Taking the box first)" / "You can't put an object
  inside itself!" and the box is in hand afterwards; held, only the itself
  line; a ring inside the box changes nothing. The wording is chosen by the
  target's flags (465FDA "in or on itself!" container+surface, 46600C "onto
  itself!" surface only, 46602A "inside itself!"), not by the preposition.
  Scarier's guard used to run before the take and left the box on the floor.
  Both backends now test after the deferred task pass, only for an object
  the take left in hand. PSTAT's silent cmd 13/18 cells are the object #1
  weight-cycle deviation (the coin, #1, inside the box), not a rule. `[4.0]`
  probe PBOXBOX, Adrift_1192 (`lib_put_in_backend`, `lib_put_on_backend`,
  2026-09-19)
- **4.0 put noun = name_object's mode-2 scorer, held first.** 46E5D8 hands
  the object fragment to 463640 mode 2 (@46E02D): pass 0 over what is held
  (directly or inside something held), pass 1 over everything present, no
  seen gate; the same resolver as plain `drop`. `put key in box` with the
  brass key held and the iron key loose puts the brass key with no prompt;
  with the brass key inside the held box it is "already inside"; both gems
  loose tie in pass 1 and the 46355E restore clears the pending object, so
  "It is not clear which gem you are referring to."; both coins held tie in
  pass 0 and keep it, "Which coin.  The gold coin or the silver coin?".
  Scarier's seen-gated matcher asked "Which key." for the first two and
  prompted for the gems. The all/and/except forms keep the ordinary parse.
  TheADRIFTProject's `put battery in remote` now resolves here rather than
  through the present-object fallback. `[4.0]` probe PPUTTIE, Adrift_1193
  (`lib_put_named_400`, reusing `lib_drop_resolve_400`, 2026-09-19)
- **4.0 closed container: the take comes first; `put all` counts hands
  before it looks at the lid.** insides (46639C) tests the target's state
  at 4661BE/4661C9 (" is locked!" / " is closed!") after name_object's take
  piece, the tasks() call, the possession and itself tests, and ahead of
  the size test, so `put ring in box` with the box shut prints "(Taking the
  ring first)" / "The box is closed!" and the ring IS taken; a shut chest
  on the floor is the same shape. Scarier refused from the container check
  before the take. The refusal is size-like: printed, line left for the
  task pass. `put all in <X>` never reaches insides when nothing is held:
  name_object counts held objects (44615C @46E553-46E580) and says "You are
  carrying nothing!" (46E5BC) even against a shut container; with X the
  only thing carried it says nothing and the catch-all answers, shut or
  open, as the surface row already did. Scarier had printed the closed
  refusal in both cases. The all rows' tentative pass now settles a refusal
  like the named rows' (they are not put_first), or the STANDARD twin
  printed it twice. `[4.0]` probe PCLOSED, Adrift_1194/1195
  (`lib_put_in_closed_400`, `lib_put_all_common`, 2026-09-19)
- **Pre-4.0 put leftovers: a fragment that names nothing, `except`, the
  3.7 static open, drop-all-except.** In insides() (run380 4457A1) the
  co() name count var_A6 comes before the target's tests. A fragment that
  names nothing anywhere leaves the target alone on the line, so 3.7/3.8
  say "You can't do that!" ahead of the container refusals: `put zzz in
  statue` / `in coin` / `in chest` (open or shut). An object named but out
  of reach still counts, so the target-first order measured on p38DARK
  stands. insides() has no exception list before 4.0: `put all
  except/but X in/on Y` is `put all in/on Y` at 3.7, 3.8 and 3.9, and X is
  put too. With nothing held that gives the all arm's empty answers (3.7
  "You have nothing to put inside the cupboard.", 3.8 "You are not
  carrying anything.", 3.9 "Nothing will fit inside the cupboard.").
  run370's openclose does not list a static container either: `open
  chest` is the bare "You open the chest.", where run380 lists. drops()'
  all arm skips only a name after " but " (run380 438793, run390 4456AB;
  run370 has no "but"), and nothing left is " not carrying anything."
  (run380 4388E6), never the exception's "don't have". Already right and
  re-confirmed: 3.7 bare take from a held or static container is "Take
  what?", 3.8's takes it; put into an open static container; the closed
  static refusal; `put coin on table` with the table a dynamic surface
  on the floor is "You are not holding a table.". `[3.7/3.8/3.9]` probes
  p37PUT/p38PUT/p39PUT (`make_3738_putprobe.py`), run370x/run380x/run390x
  Adrift_154_p37put2.rtf, Adrift_155_p38put2.rtf, Adrift_154_p39put.txt
  (`lib_put_co_count_pre390`, `lib_cmd_put_in_except_multiple`,
  `lib_cmd_drop_except_multiple`, `lib_cmd_open_object`, 2026-09-19)
- **3.9 take all sweeps open containers; "and" beats "all" in put.**
  After the floor, run390 takes() (4558CE-455B28) sweeps every open
  container or surface lying directly in the room through the take-from
  arm: `take all` with six objects in the open static cupboard is "You
  take the coin, ... and the table from the cupboard." (a first-take
  sentence is joined with two spaces). insides() sets var_CC = 1 on
  c("all") and then 2 on c("and") (4618D4-46190C), so `put all except
  coin and stone in cupboard` is the and-arm: only co()-named objects
  held or loose in the room count, and with both on the held table it is
  "Nothing will fit inside the cupboard." The capacity arms of the sweep
  (insides() = 2, 455A68) and the and-arm's worn objects are not
  modelled. `[3.9]` p39PUT T23-30 (Adrift_154_p39put.txt), probes
  Adrift_p39takeall.txt / Adrift_p39takeall2b.txt
  (`lib_take_all_sweep_390`, `lib_put_all_common`, 2026-09-19)
- **Pre-4.0 drop "and" arm walks, never parses; `everything` = `all`;
  3.7/3.8 absent static target can't see.** drops()' "and" arm (run390
  4457A0-445813) marks every object held or worn directly whose name
  co(obj, 0) finds, so `drop foo and bar` is "You are not carrying
  anything." and `drop coin and foo` is "You drop the coin." `[<4.0]`.
  run390 generaltasks rewrites "everything" to "all" (45F225), so `put
  everything in/on zzz` is the put-all refusal "You can't put anything
  inside/onto that!" `[3.9]` (3.7/3.8 crash, see deviations). The pre-3.9
  whole-game target search finds a static container in another room, and
  insides() then says "You can't see a cupboard." after the not-a-container
  refusal `[<3.9]`; 3.9 never finds it ("Put the coin inside what?").
  p37PUT/p38PUT/p39PUT, `cmdfile_p3738drop.txt` / `cmdfile_p3739drop.txt`:
  run370x Adrift_160_p37drop.rtf, run380x Adrift_161_p38drop.rtf, run390x
  Adrift_160_p39drop.txt (`lib_drop_and_arm_collect_pre400`,
  `lib_put_that_390`, `lib_put_static_absent_pre390`, 2026-09-19)

### NPCs, walks and battle

- **3.9 absent characters, per verb.** `talk to`/`speak to` any named
  character, even an absent or unseen one, gives the "ask X about"
  hint. The hint at 45975C has no room gate, and the ask branch's
  "isn't here!" loses to it. `give obj to <absent npc>` (also `give npc
  obj`) is the object catch-all "I don't understand what you want me to do
  with the stone.". therest's give stays silent at 45D696 once any
  character is named, so the line reaches 46024A before characters(). A
  bare `give stone` echoes "(to <last named>)" (rewrite 45F9D5, "(to " at
  45FAB9) and runs as that. `take <absent npc>` is "Take what?", because
  takes() answers before characters()' "is not here!" (4596E1). run390
  names a character by Name or Alias(0) only. `[3.9]` p39ABSNPC
  (make_39_absnpcprobe.py, Adrift_1206_p39absnpc.txt)

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
  - An event whose clock ROLLS 0 parks for good, at every version: the
    restart (RestartType 1) and the start off a waiting clock both store
    the roll with no +1, the running block decrements first, and the finish
    test is `clock = 0`, so the clock sits at -1: no PrefTime text, finish
    or restart draw ever follows, and the LookText stays. A zero-length
    event is the case that always rolls 0. Only the task start adds 1 (so
    its roll 0 finishes on the start turn). zelda T52-60 (event 5; 468 =
    468 draws); probe pEVROLL events A and B
    (`harness/make_39_evrollprobe.py [out] [38]`, run390x
    Adrift_1200_pevroll39.txt, run380x Adrift_1201_pevroll38.rtf; run370
    events() 431B5D/43247A/432068/432173 read, same shape) (2026-09-19)
  - A restart-after-delay event with an immediate or task starter is a
    one-shot whatever its length, at every version: the finish block (4706BE) sets the state
    to waiting, draws Rnd once (4706CE) and stores
    Int(Rnd * (EndTime - StartTime)) + StartTime, which is 0 because those
    two fields are only read from the taf for a random-delay starter; the
    waiting block (46FD26) decrements before it tests for zero, so the
    clock sits at -1 for good. No StartText, no LookText afterwards. The
    Rnd is still drawn. run390 448E23-448E7F and run370 43249F-4324F4 are
    the same (run390's variant d re-arms because its starter is a random
    delay). Probe EVRS, Adrift_1196 ("R2 FINISH." once; control R1
    restarts every three turns); probe pEVROLL event C, Adrift_1200/1201
    ("C FINISH." on turn 2 only, no "C LOOK." afterwards) (2026-09-19)
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

- **An empty authored PlayerName is "Anonymous" at 4.0.** run400's
  openadv fills the field at load (48F39F), and %player% and the
  third-person pronoun array (48F6F2) both read it, PromptName off or not.
  run390 has no load-time default (unmeasured; Scarier keeps SCARE's
  "Player" before 4.0). `[4.0]` probe ANON, Adrift_1198 ("Anonymous is
  carrying nothing.", "Name is [Anonymous]."); goldens woof, aliasagent,
  greekschool re-blessed (`%player%`, scvars.cpp, 2026-09-19)
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
