# TODO: Runner-transcript verification of the v4 walkthroughs

Replay a wired walkthrough command for command in the real Windows ADRIFT
Runner under Wine and diff the Runner's own transcript against Scarier's.
Where they disagree, fix the engine, never the walkthrough. Then re-bless the
golden and write the evidence into the game's row comment in
`harness/run_v4_walkthroughs.sh`. Scope is all four file versions (3.70,
3.80, 3.90, 4.00). Only the Runner binary and the capture flow change with
the version.

**Pruned 2026-09-14, twice on 2026-09-19 and again on 2026-09-20.** This
file holds the workflow, the open leads, the deliberate deviations and a
one-entry-per-rule index of everything ported. The evidence behind each
index entry (probe feeds, `Adrift_<N>_<tag>` transcript names, Runner
addresses, corpus fallout) lives in the row comments of
`run_v4_walkthroughs.sh`, in the code comment next to the function the entry
names, in `~/Adrift_decompile/index/annotations.tsv`, and in git history:

    git show c182b2fa6:terps/scarier/test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md   # before the 09-20 prune; full 2026-09-19 entries with transcript names and addresses
    git show aee976374:terps/scarier/test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md   # before the second 09-19 prune
    git show 55dd84ee1:terps/scarier/test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md   # with the 2026-09-14..19 triage write-ups
    git show 72fd5ea08:terps/scarier/test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md   # last full version before the 09-14 prune
    git show 45e20596:terps/scarier/test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md    # before the 2026-09-06 compaction

Row comments and probe generators cite sections by title ("Ported
2026-09-10: the take-from handler's own answers" and so on). Grep the
`72fd5ea08` version for the title; grep `c182b2fa6` for a transcript name.

---

## Where things stand (2026-09-20)

- **Goldens:** 428/428.
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
  column records which archive file each copy came from. Only runs with no
  row keep their archive names: probes and one-off variants such as
  House_sober.
- **Older archives** under `~/adrift-battle/runner/wine/` hold no engine
  lead. `pfx/drive_c/adrift/` is the live archive where every drive lands:
  never `rm` a glob there.

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
  span. Whitespace-only lines go out as bare Returns.
- **Empty solution lines are commands**, not pauses. The Runner answers
  them "Huh?" or similar.
- **A pause marker must be answered by a blank line, never a real command.**
  Without `SCR_SKIP_WAITKEY`, scare's `[Press any key]` reads a whole stdin
  line, so a golden can pass while a pause swallows a command. The Runner's
  pauses never eat a typed line: it runs as a turn, and a `look` there ticks
  events and draws. light_up and mould were repaired this way on 2026-09-19.
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
  (`WM_GETTEXT`): the window itself, no screenshot or OCR. Use it to prove a
  line break is a transcript artefact and to recover what an `.rtf` could
  not hold. `dump_par.sh` drives a job file that way and
  `harness/graft_scrollback_tail.py` appends the difference to the archived
  transcript. On 3.9/4.0 the transcript is live, so an end-of-feed loss
  there is the keypress wait eating a command with no text behind it. See
  `runner_transcripts/README.md`, "Grafted tails".
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
   prompt for every other ending); blank-line counts around an ending
   (goldens run through `cat -s`); a startup echo for a feed's leading blank
   lines (Glum_Fiddle); a `<centre>` join; a `<waitkey>` line join or
   `<waitkey><cls>` butt-join; a wrap inside an unbreakable token; `[MORE]`
   splits; `.rtf` mojibake; the epilogue cut at the final keypress; rule-2
   "lost" lines after an identical ending.
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
- **3.70/3.80 probes:** `make_37_/make_38_*probe.py`, `make_3738_*probe.py`,
  hand-authored. A hand-built .taf must parse to exact EOF in
  `harness/scare`. Every version needs its own probe file (run400 says
  `Incorrect version`).
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
- motion T257-258: the Runner's echo for T258 landed one room block early;
  with whitespace stripped both sides show the same frames in the same
  order. The other reported turns are Scarier's 80-column wrap on long
  `O----` rows.
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
  run-time error 401 at command 92.

### Engine, needs a probe (4.0)

- **Scope, unmeasured:** the never-seen "You can't see that." branch at
  471995. (The two-pass `%object%` scope filter itself is measured and
  ported -- see "A task's `%object%` binds only a seen object" in the
  index -- as is the NPC seen gate for `%character%`, under "A task
  command's `%character%` at 4.0".)
- **Second-noun ambiguity:** wording of an instrument ambiguity (sswhore
  `unlock drawer with key`); a tie inside either half of a " with " split;
  lock/unlock with a Key whose left half resolves to nothing; absent
  lock/unlock where the object really is locked.
- **Ambiguity prompts:** co()'s crowded arm (454454) and its -2/-1
  answers; "That wasn't one of the options!" has never been triggered;
  whether an object ambiguity on a task-answered line also suppresses the
  tick. (454454's Prefix contest itself is measured and ported -- see "The
  4.0 Prefix contest" in the index.)
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

- **checkwild, the unported half.** ~~`uip_wildcard_match_pre400` only
  vetoes a tree match.~~ **Closed 2026-09-20** -- checkwild now decides a
  pre-4.0 `*` command outright; see "Before 4.0 a `*` command is decided
  by checkwild" in the index. ~~3.9's %object% substitution (44AAD6) is not
  emulated.~~ **Closed 2026-09-20** -- see "A pre-4.0 task command's
  %object% and %character% are substituted from the line" in the index.
  ~~Group patterns (`[`, `{`) skip the check at every version.~~ **Closed
  2026-09-20** -- see "A task command's GROUP is the LAST thing 4.0 tries,
  and below 4.0 it is not syntax at all" in the index. ~~%number% /
  %t_number% (44ADxx) hand a command back to the tree.~~ **Closed
  2026-09-20** -- see "A task command's %number% is a substitution, and
  below 3.90 it is a literal" in the index. The lead is now closed but for
  two arms nobody has measured: run390's SECOND pair of object loops
  (44ABFE, 44AC98), which repeats the walk against checktask's own `text`
  argument when the first pair bound nothing, and the generic
  `%<variable>%` / `%t_<var>%` arms at 44AF25 and 44AFF8. A command
  carrying any other marker is still handed back to the tree.
- **Put:** the " is full." arm at 461E59 speaks only when something fits
  and the bag is still full, so it is effectively dead; it needs a size-0
  object.
- **Two-object canonical prefixed retry:** the run390 half is not
  re-measured. The 4.0 half is closed.
- **The "With what?" prefix continuation** (45D3E0) is not modelled.

### Engine, needs a probe (3.7 / 3.8)

- **run380's event route** to the task-ran flag (set in tasks() at 44D0BA)
  is unread; the 3.9 rule is ported.
- **Why `attack dave` is DontUnderstand at 3.7/3.8** (battle off) while
  `hit`/`kick dave` get "Dave avoids your feeble attempts." is unread:
  run380's characters() arm ORs c("attack") in with the others, and
  "attack" is in no other string of either exe. Ported as measured
  (`lib_attack_line_pre390`).
- **A task command meeting a comma** at 3.7/3.8 is measured for the
  literal-line cases only (see "A comma in a task command" in the index);
  the library's comma-as-space rule is not known to reach checktask.

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
- **3.7/3.8 Runner crashes ("Run-time error '9': Subscript out of
  range", transcript lost):** `put all in <nothing>`, `put all on
  <nothing>` and `put everything in zzz` on run370x and run380x; bare `eat`
  and `eat <character>` on run370 (run380 answers DontUnderstand). Scarier
  keeps its sane answer in each case (p37PUT/p38PUT, p37NPCAMB, 2026-09-19).
- **SCARE meta-commands** `wait N`, `hist N` and `redo N` exist in no
  Runner. Eleven more inventions are compiled out by
  `SCARIER_NO_ABBREVIATIONS`.
- **Not ported (policy):** `both` right after a `Which <term>. <list>?`
  prompt (3.9/4.0 re-run the saved candidate list as the typed line, run400
  48AE94 / run390 43B4D5, unmeasured; a bare `both` with no prompt is
  measured and ours: "I don't understand.", two turns, p39WITH T25); the
  battle-disabled `status`/`statusline` fallback; the help/about/time/
  version texts; `turns`/`version` version gates.
- **4.0 `status`' table label junk.** run400's battle status table pads its
  labels with visible nonsense: the literals at 47DD0E/47DD67/47DDE1/
  47DEC0/47DF9F/47E07E (and their NPC twins from 47E144) are
  `<0>Stamina: ileeerfeetts</0> `, `Stamina:</c> <0>ileeerfeetts</0>`,
  `<c>Hit strength:</c><0>ilmfeeee</0>`, `<0>iilmee fttts</0>`,
  `<0>iirmttt</0>` and `<0>eeemee rfetts</0> `, confirmed byte for byte in
  run400.exe at 0x12906/0x129B8. `<0>` is not a hide tag -- the converter
  at 47A688 turns it into `<font color=default>` and `</0>` (47A69F) just
  pops the font stack -- so the Runner really prints "Stamina:
  ileeerfeetts", "Hit strength:ilmfeeee" and the rest (p4BATT turn 33,
  Adrift_1208:118-124). All the padding letters are narrow glyphs
  (i l m e f r t s), which is what they were for; the author left them
  visible, and reused the stamina literal as the label of both the header
  row and the "You are wielding ..." row. A display accident no author
  intended, so by the deviation policy scarier keeps its clean headings and
  its own column padding. Not a lead. (2026-09-20)
- **Empty-Prefix double space** in object listings.
- **ALR stack overflows.** House's `%drunk%` ALR loop and a mutual `A -> B`
  / `B -> A` pair overflow the stack in run400, which then prints nothing.
  Scarier's depth cap prints the intended line.
- **run400's carried-weight cycle on dynamic object #1.** The recursive
  weight walk (447680) sums every object whose parent field equals the
  object's index without looking at the position, and an object that has
  never been inside or on anything still has parent 0 = object #1. So the
  first put or drop of object #1 into a never-moved container or supporter
  makes a #1 <-> container cycle: move_object has already written both
  fields, the walk dies of "Out of stack space", evaluate's handler
  swallows it and the turn prints nothing. The object IS moved, and every
  later weight walk that reaches the pair dies the same way, so `take
  <object #1>`, `take <container>` and `put all` are silent no-ops from
  then on. This is also why `put all on <supporter in the room>` used to
  move only the first object (surf3/surf6): `all` moves object #1 first. A
  held supporter is immune because taking it wrote -1 into its parent
  field. run390 has no recursive weight walk and prints normally. Scarier
  prints the confirmation and moves every item. No corpus row reaches it.
  Measured on put7.taf and PSTAT, caught by the vbrng.dll stack sampler
  (`VBRNG_SAMPLE`), 2026-09-19.
- **Undo slots:** Scarier skips administrative lines, which the Runner
  records.
- **`NPCWalkAlert`:** a synthesized task pair with no run400 counterpart.
  It anticipates the ticker's restart by a tick; nothing depends on it.
- **mould `hint`:** run400 has no interactive hints.

---

## Rules measured and ported (index)

One entry per rule: the rule, the game or probe that found it, and the
commit, function or date that ported it. Version gates are in brackets:
`[4.0]` is 4.00 only, `[3.9+]` is 3.90 and 4.00, `[<4.0]` is 3.70-3.90,
`[<3.9]` is 3.70/3.80. No bracket means every Runner. Addresses, feeds and
transcript names are in the code comment next to the named function, in
`annotations.tsv`, and in the `c182b2fa6` version of this file.

### Parser and dispatch

- **`goto <place>` / `go <place>`.** gotoplace runs after the tasks and
  meta commands and before the room refusal and therest. 3.9+ takes `goto`
  anywhere or a line starting `go `; 3.7/3.8 take `goto` or `go to`. The
  name matches exactly, then as a substring. Answers: `Which "x"?`, "You
  are already x!", "You can't get there from here.", "Unknown place.". A
  walk prints "Moving to x...", types each step as a turn of its own, then
  "Arrived x."; the goto line itself is no turn. The route finder tests
  every exit restriction as a task; 4.0 uses visited rooms only. Bare
  `goto` is DontUnderstand. p38GOTO/p39GOTO/p4GOTO (`lib_cmd_go_place`,
  2026-09-19)
- **`goto` with more on the line.** Pre-4.0 walks first and then runs the
  rest of the line in the same turn; run400 empties its split queue at the
  top of every generaltasks, so a goto that walks throws the rest away
  while one that doesn't walk keeps it. Pre-4.0 examines() runs ahead of
  gotoplace and takes any line with one of its entry words, so `go to
  kitchen and read` is "Nothing special.". (`run_goto_rest`, 2026-09-19)
- **3.7 goto.** Answers as 3.8, and run370 also takes the game's own goto
  word (command slot 15) anywhere, cutting its length plus one off the
  front: `rove kitchen` walks, `a rove hall` walks to "blue hall". It is no
  synonym. `[3.7]` p37GOTO/p37GOTOW (`lib_cmd_go_place`, 2026-09-19)
- **3.7 therest refuses an absent object first:** "You can't see the X."
  before any verb arm (go, enter, push, smell, kiss, turn, jump, sing, look,
  climb, sit on, fly). Lines holding an earlier handler's word keep their
  own answer. `[3.7]` p37GOTO (`run_therest_absent_370`, 2026-09-19)
- **`wait` anywhere, every version.** `c("wait")` (or `z` at 3.9+) with an
  empty message answers "Time passes..." and the wait turns. It comes after
  the tasks and the word-anywhere handlers (take, drop, wear, remove,
  sit/stand/lie, open/close, examines, score, swearing) and before whereis,
  gotoplace and therest: `wait stone`, `please wait`, `push stone wait`
  pass time; `waiting` does not. `look wait` passes time at 3.7/3.8 and is
  examines' from 3.9. run370's openclose writes nothing unless the object
  is openable, so 3.7 `open stone wait` passes time. `goto hall wait` is
  "Time passes..." then "Unknown place."; a goto that walks skips the
  message and the end tick, so only WaitTurns - 1 ticks run. p37GOTO,
  p38GOTO, p39GOTO, p4EXAM (`run_wait_anywhere`, 2026-09-19)
  - Lines the gate reaches with an empty message pass time whatever else
    they say: `give coin to bob wait`, `say hello wait`, `i wait`,
    `inventory wait`, `n wait`, `hint wait` and `help wait` are all "Time
    passes..." at every version (the give echo, inventory and hint arms
    write nothing that survives; the direction is never walked). `score
    wait` prints the score, `shit wait` swears, `sit wait` sits, and
    `ask`/`talk to bob wait` speak. `[all]` p37SITN, p38SITN, p39SITN,
    p4SITN (2026-09-20)
- **`score` anywhere, every version.** generaltasks' score arm (run370
  43BB98, run380 4423EC, run390 45F6B5, run400 48A6AE) enters on c("score")
  anywhere, overwrites whatever was written before it (sitstand included:
  `score sit` prints the score and does not sit) and is no turn. Lines
  holding a take/drop/put/wear/remove/examine or swear word keep their own
  answer. `[all]` p37SITN..p4SITN (`run_score_anywhere`, 2026-09-20)
- **Swearing anywhere overwrites.** The profanity arm (run370 43BEB5, run380
  442709, run390 45F8E4, run400 48A976) runs after sitstand and score, so
  `shit sit` is the swear reply and the player stays standing. `[all]`
  p37SITN..p4SITN (`lib_sitstand_anywhere`, 2026-09-20)
- **Unknown verb, pre-4.0 catch-all.** A line no handler answers walks
  every object it names in index order: a seen present object gives "I
  don't understand what you want me to do with X.", a seen absent one
  "<player> must be in the same room as X to be able to do anything with
  it.", an unseen one "What <Short>?". Present beats absent; unseen speaks
  only if nothing else has. 3.8's therest checks only the first present
  object. 3.7 refuses absent objects first, so only its "What X?" arm
  shows. In 3.9 co() matches only present seen objects, so an absent-only
  line is DontUnderstand. The pre-4.0 eat arm speaks only for a present
  object; "I don't understand what you are trying to eat." is 4.0's.
  `[<4.0]` p37EXAM/p38EXAM/p39EXAM/p4EXAM
  (`lib_verb_object_catch_all_pre390`, 2026-09-19)
- **Seen stamp, 3.7/3.8.** The generaltasks pre-pass runs on every line
  naming zero objects or two or more and marks every present object seen
  (held, worn, loose, statics in the room; never container contents). So
  `n` reveals the room you leave and `frob stone` does not. `[<3.9]`
  p37EXAM/p38EXAM (`lib_prepass_seen_3738`, 2026-09-19)
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
    dispatch-by-index walk FAILS, and never cleared. A stale cache can claim
    a line silently and get_piece answers DontUnderstand. `[4.0]` 3monkeys
    T41 (`restr_cache_fallback`, 2026-09-19)
  - A trailing space in an all-literal task command must be typed. sommeril
    (093a12d5e)
  - Before 4.0 a `*` command is DECIDED by checkwild, not merely vetoed by
    it (run390 checktask 44B10D takes its answer as the match flag, and
    44B0E2 compares a starless command for equality): prefix before the
    first `*`, each later piece found with InStr over the whole line and
    nothing cut, the text after the last `*` equal to the line's end. So
    the middle pieces need NOT be in order and one occurrence can satisfy
    a piece twice -- `* king * rose *` runs on `blip rose blip king blip`
    and `* zog * zog *` on `a zog b`, in all three pre-4.0 Runners, where
    4.0's cutting matcher refuses both. run390 pads for a leading "* " or
    trailing " *"; run380/run370 pad nothing. All three substitute the
    command's %object% / %character% from the line first (see "A pre-4.0
    task command's %object% and %character% are substituted from the
    LINE" in this index). `[<4.0]`
    alchemist T300, `[<3.9]` marooned T53, p37/p38/p39/p4WILDORD
    (`uip_wildcard_match_pre400`, `run_match_task_commands`, 2026-09-20)
  - The SYNONYM table is sequential whole-string rewrites. Vardock
  - The 4.0 `*` matcher does not backtrack: each literal piece is found by
    the first InStr and the line is cut past it. run390's checkwild never
    cuts. `[4.0]` the_town_of_azra T13, xfiles T69
    (`uip_wildcard_match_400`, 2026-09-15)
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
    index into the whole object table, so an Obj2 naming a static object
    always fails. `[<3.9]` twilight T48
    (`parse_fixup_v380_objstate_restr`, 2026-09-15)
  - A comma in a task command: 3.7/3.8's matcher wants the literal line
    (`push, stone`, `push stone,` miss the `push stone` task; `push,stone`
    is the library's, the comma being a space to it). 3.9 splits at the
    comma: `push, stone` is "Push what?" then "You push the stone.", the
    checkverb prefix carrying `push` into the split element. `[<4.0]`
    p37TASK/p38TASK/p39TASK (`lib_what`, 2026-09-19)
  - A task's `%object%` substitutes the bare Short or Alias: no Prefix, no
    article, so `pa a brass key` misses a `pa %object%` task and falls to
    the prefix-tolerant library. 3.90 folds case, 4.0 does not; before 3.90
    `%object%` matches nothing. `[3.9+]` p39CASE, p4TAMB
    (`uip_compare_reference_strict`, 2026-08-25)
  - A task's `%object%` binds only a seen object. 4.0 then makes TWO
    passes over the object array -- present-and-seen (458E6C's `arg_14`
    against `obj_indirectly_in_room()` at 44B578), then, only if that bound
    nothing, absent-but-seen (the tail self-call at 458E64) -- and the
    FIRST namesake in index order wins each pass. 3.90 has no scope test
    and no break, so its LAST seen namesake wins wherever it stands; 3.80
    binds an unseen absent object too. `[3.9+]` Glum_Fiddle T16-22,
    p39OBJREF/p4OBJREF (`uip_match_entity`, 2026-09-20)
  - 3.7 writes the `%object%` substitution back INTO the task record and
    restores it only when the line did not match (run370 4332CA/433377,
    restore guarded at 43342B), so the first matching `%object%` line of a
    session spells that task's command for good: a later `nurb rock` runs
    the task with nothing bound and prints the text's `%object%` raw, and
    `nurb coin` no longer reaches it at all. run380 answers every cell
    normally. `[3.7]` p37OBJREF (`run_370_rewrite_task_command`,
    2026-09-20)
  - An unbound `%object%` / `%character%` / `%theobject%` in a task's TEXT
    prints RAW in every Runner, and no Runner carries a binding from one
    typed line into the next -- the forget at the top of a command is not
    the 3.9+ rule we had it for. `[all]` p37/p38/p39/p4OBJREF task 2
    (`var_get_system`, `run_match_task_commands`, 2026-09-20)
- **Word rules:** `take` becomes `get` before parsing `[3.8]` (great); `z`
  means wait only from 3.90 (cave); `again`/`last`/`previous` are tested on
  the whole line before any task, and `g` joins them from 3.90 (shadowpeak
  TASK 404, `run_is_repeat_word`, 2026-09-14).
- **Line splitting:**
  - 4.0 cuts at `,`, `. `, ` and ` and ` then `, suppressed when the tail
    starts with any object's Short, Prefix word or Alias (case-sensitive).
    The four cuts are passes in that order, each on the head the previous
    pass left. `[4.0]` p4AND (dffce55df, `run_find_split_400`, 2026-09-19)
  - Pre-4.0 never looks at the object table. run370/run380 cut only at
    `then`, at the first substring hit (`x athens then look` runs `x a`,
    `s`, `look`). run390 cuts at the first `,`, then `. ` in the head, then
    `then`. No pre-4.0 Runner cuts at a period with no space after it.
    run390 replaces an empty then-head with everything queued behind it;
    3.8 answers it with DontUnderstand. `[<4.0]` p38ASK/p39ASK
    (`run_find_split_pre400`, 2026-09-19)
  - No Runner drops the rest of a line after a DontUnderstand element.
    (2026-09-19)
  - A name followed by punctuation the splitter left still resolves: `,`
    ends a word at 3.7/3.8, `,` or `.` at 3.9. (`uip_is_word_end`,
    2026-09-19)
  - A comma after a word is a space to 3.7/3.8's library: `x, coin`,
    `drop, coin` answer as without it; `x,` and `x,coin` are "Nothing
    special." (the next word must follow a space). `[<3.9]` p37EXAM/p38EXAM
    (`uip_match_whitespace`, 2026-09-19)
  - Pre-4.0 therest() answers "Nothing special." to c("look") anywhere in
    the line (`look,`, `zzz, look`, `look.` at 3.9 only); c()'s FIRST hit at
    a word start decides. `[<4.0]` (`lib_cmd_look_anywhere_pre_400`,
    2026-09-19)
  - therest()'s cascade: every `If c("<verb>")` arm tests the WHOLE line
    and the LAST matching arm wins (`push stone pull` pulls, `stone jump`
    jumps, `please push stone` is "Your kindness gets you nowhere."). Arms
    that need an empty message (talk, block and lock at 3.7/3.8; only talk
    at 3.9) only win when no earlier arm has. Handlers above therest still
    answer first. Scarier moves the winning keyword to the front and
    dispatches again. `[<4.0]` p38ASK/p39ASK, 21/21 cells at 3.8 and 3.9
    (`run_therest_pre400`, 2026-09-19)
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
    whole word, case-insensitive. p39ASK/p4ASK/p38ASK
  - The 3.7/3.8 ask topic matches by substring, and the last match wins
    (wrecked T129/T211). A typed `<subject>` with no topic is "Smart Alec!";
    run390/run400 escape "<" at input. p38ASK
  - The bare `ask` hint has no final full stop in any Runner. (2026-09-19)
- **Case handling.** Every Runner lower-cases input, but the character
  resolver's tail is case-sensitive, so a SYNONYM carrying a capital makes
  its NPC unreferenceable. bandera (0390eb300)
- **Question prefixes, 4.0.** "Wear what?", "Remove what?", the give
  prefix, checkverb on a bare verb and "...with?" store the line as a
  prefix for the next input. The "with?" line is not a turn. `[4.0]`
  (daf951a81)
- **Question prefixes, 3.9.** Every "<Verb> what?" (checkverb's arms and
  the handlers' own rows) stores the line when the typed line IS the verb,
  and generaltasks prepends it to the next line nothing answers: `push` /
  `stone` pushes the stone, `give` / `coin` asks "Give the coin to who?".
  The prefix lives one line. `push zzz` / `stone` stores nothing. Bare give
  stores its completed "give to nobody" line, so the rerun is not echoed
  again. `[3.9]` p39TASK (`lib_what`, `run_get_line_input`, 2026-09-19)
- **`drink` bare is "You can't drink that." below 4.0**, not "Drink what?";
  eat, open, close, read and say leave no prefix. `[<4.0]` p39TASK
  (`lib_cmd_drink_what`, 2026-09-19)
- **A "Which X.  list?" answer is spliced into the line (3.9):** the next
  unanswered line's text replaces the term in the stored line, followed by
  the term itself unless the answer already holds it as a word, and the
  line is rerun: `wear hat` / `red` runs `wear red hat`, `open box` / `hat`
  runs `open hat box` and re-asks from the answer's own namesakes. "That
  wasn't one of the options!" is unreachable in practice. `[3.9]` p39TASK
  (`lib_co_ambiguity_prompt`, `lib_battle_who_store`,
  `lib_battle_who_continuation`, 2026-09-19)
- **3.9 object catch-all is a co(obj, 0) walk in index order:** the first
  present, seen object co() matches is named, so bare `red box` beside the
  blue box is "I don't understand what you want me to do with the red box."
  and bare `box` is the Which prompt. `[3.9]` p39TASK
  (`lib_cmd_verb_object`, 2026-09-19)
- **The "(to Nobody)" echo outlives the Which prompt:** the bare-give
  completion prints its echo as it rewrites the line, and the end-of-turn
  "Which X." replaces only the turn's answer. read, put, eat and the
  checkverb arms on an ambiguous noun are the prompt alone. `[<4.0]`
  p39TASK (`lib_co_ambiguity_prompt`, `pf_leading_reference`, 2026-09-19)
- **The " with " split.** therest resolves both halves: "don't have",
  "Don't be daft!", or a " with <X>" suffix on about thirty verb refusals.
  Corners: open/close/read/fix/clear with, and take looking up only `get
  <name>`. `[4.0]`; run390 twin `lib_with_clause_390` (daf951a81,
  b526c013b)
- **Pre-4.0 "with" lines.** 3.9 therest with-arm: with battle off, a line
  with the word "with" that the 2+-object split did not claim answers "I
  don't understand what you want me to do with <the X>!" for the first co()
  object after "with", else "With what?"; it beats characters(), so `kill
  dave with stone` never reaches the attack arm at 3.9 (battle on keeps
  dobattle). 3.8+ history rewrite: a line starting "with " is prefixed with
  the previous typed line (blank lines count; not run370). `[<4.0]`
  p37/p38/p39NPCAMB (`lib_with_arm_390`, 2026-09-19)
- **Line endings:**
  - A task that ends the game takes ALL of therest off the line: the "You
    can't <verb> X" arms go too, an empty buffer prints DontUnderstand, only
    the catch-all subset is kept. `[4.0]` relojero, easter, iachini T185
    (f038d76bf, STANDARD_ENDED_FALLBACK_COMMANDS, ab85a3e4e)
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
    or more present, seen namesakes skip the tick and print the task's text
    with no question. `[4.0]` cyber2 T14, p4TAMB
    (`lib_co_400_line_leaves_which_pending`, 2026-09-19)
- **Administrative turns and the counter:**
  - An NPC examine and a nothing-found examine are administrative turns;
    so is `read` via examines. `[4.0]` EV14-16, house T150
  - Only examines' "see no such thing" sets the flag; characters()' NPC arm
    sets none, so `x <npc>` whose line has a unique seen-but-absent object
    as the winner is a turn. `[4.0]` humbug T634
  - In run390 hint, help, clear, time, version, save, restore and undo are
    ordinary turns, and the counter counts every line element, so `both`
    counts twice. `[3.9]` p39ADMIN (a211db2f1, b526c013b)
  - There are no administrative turns and no startup tick below 3.9.
  - run380 counts line elements too, at the top of generaltasks, so `turns`
    counts itself. run370 has no counter and answers `turns` "I don't
    understand." (the `turns` gate is policy). `[<3.9]` p38ADMIN/p37ADMIN
- **clear.** Bare `clear`/`cls`/`clr` empties the window and prints "Screen
  cleared.", a turn below 4.0 and administrative at 4.0. Any other line
  holding the word goes to therest's clear arm: "You can't clear the rope."
  / "You can't clear that.", a turn at every version. `[all]`
  p37ADMIN/p38ADMIN/p39ADMIN/p4WITHQ2 (lib_cmd_clear_other)
- **The room refusal** runs inside the library, ahead of therest. `[3.9]`
  (9fbb40881) Before 3.9, drop, put and give refuse ahead of it. `[<3.9]`
  cave, greatc (f83e1cf87)
- **Pre-4.0 give to a present NPC** runs below the room refusal, and
  run390's give writes only into an empty message or one holding " might
  need " / "I don't understand". So a Where=0 task matching the line wins
  with "You can't do that here!". `[<4.0]` the_hangover T42
  (run_standard_give_npc_commands). At 4.0 the give sits above the refusal.
- **No lock handler before 4.0.** Pre-4.0 carries only therest's checkverb
  " can't lock " / " can't unlock "; " is not locked!" and the key messages
  are run400's alone. Every pre-4.0 lock/unlock line is therest's and loses
  to the room refusal. `[<4.0]` thetest_win T68-77 (fe64ab0f3)
- **run380's post-take-from task sweep.** tra `get meat` also runs `get
  *knives*`. The sweep follows every take-from whose source was found,
  refusals and take-all included; only "You can't do that!" skips it. Its
  text joins the refusal's line. run370 has no sweep. `[3.8]` p38TFSW
  (4f79695e4, 2026-09-19)
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
  pair "Nothing special.". `[3.9]` cybercow_win T118
  (`run_note_dispatched_task_ran`, 2026-09-19)
- **Whole-line take or drop with two names and no `and`** (`get coin,
  hat`, kept whole because "hat" names an object): the noun scorer resolves
  the whole fragment; a tie between objects sharing no name is "It is not
  clear which <last tied name> you are referring to." and moves nothing; a
  unique winner is the only object taken. The take scorer's candidates are
  dynamic, seen, visible; the first pass leaves out anything held or worn,
  and a second pass admits them only when the first found no unique winner.
  `[4.0]` p4AND (`lib_take_tie_400`, `lib_take_resolve_400_string`,
  `lib_drop_named_400`, `lib_input_contains_word_400`, 2026-09-19)
- **`get X and Y` with neither present:** "There is nothing worth taking
  here." with no per-object refusal; one present object takes it alone.
  `[4.0]` p4AND (`lib_cmd_take_absent`, 2026-09-19)
- **`x coin and a hat`:** referencedob's prefix-word pass counts "a" for
  both objects, the scores tie, and examine prints "Sorry, I'm not sure
  which object you're referring to." as a turn; `x coin and the hat`
  examines the coin. `[4.0]` p4AND (`lib_examine_referencedob_400`,
  `lib_disambiguate_object`, 2026-09-19)

### Nouns, scope and the seen model

- **The 4.0 Prefix contest.** A namesake crowd is thinned by what the line
  holds of each candidate's own Prefix before any handler sees it: run400
  Splits the Prefix on single spaces, tests each word against the typed line
  with the whole-word helper 454CB0, counts the hits and keeps the strict
  maximum -- 454454 for objects (called twice from co(), 4645C1/46473F),
  450610 for characters (called twice from npc_in_command(), 45E895/45E8BF).
  A unique top score above zero wins outright; a tie at the top is still a
  crowd. Articles are ordinary Prefix words, so p4PFX2's `x the red guard`
  examines Cid ("the red") over Ann ("a big red") and Bob ("a red"), all
  three aliased "guard". The character question still lists every namesake;
  the object question lists only the top scorers. Neither routine ever
  returns -2 -- that is the caller's Me(424) marker. run390 has no Split
  anywhere and no character contest at all; its one Prefix test is
  lastword() (42DA40), the LAST Prefix word only and objects only. `[4.0]`
  p4PFX/p4PFX2 (`lib_npc_400_prefix_score`, `lib_npc_400_prefix_settles`,
  `lib_disambiguate_npc_pick`, `lib_disambiguate_object_common`, 2026-09-20)
- **4.0 binds a trailing `%character%` by containment.** npc_in_command()
  (45E99C) LCase()s whichever of the Name or an Alias answers and returns
  InStr(Me(304), word): whole-word containment anywhere in the line, not at
  the position the pattern has reached, with the Prefix contest picking
  among the hits. run400 answers p4PFX's `x guard blue` with Bob's
  description where a positional match sees no candidate at all. The
  binding is npc_in_command's MODE 0 -- the mode every reference site uses,
  the examine scan at 48B556 included. Its loop at 45E6C5 counts the
  characters answering to the word that stand in the player's room and are
  seen, and 45E740 returns early only for `count > 0 And mode = 3` or
  `count = 0 And mode = 1`, so mode 0 falls to 45E8F4 (one namesake: this
  NPC's own room and seen bytes) or 45E892 (several: the contest). An
  ABSENT character therefore never binds -- mutaydid's synonym table
  rewrites `butcher mystery meat` to `attack mystery meat`, and although
  "meat" is an alias of the absent Mother Meat run400 answers with the
  object arm, "You can't see the mystery meat.". Mode 1 belongs to the
  absent tails (the ask block at 47F8E5, the attack tail at 47F40D), which
  rescan the line themselves, and mode 3, at 48B9A5 alone, answers a raised
  question. `[4.0]` p4PFX (`uip_match_entity`, 2026-09-20)
- **The 4.0 character catch-all names the contest's winner.**
  characters()' tail calls npc_in_command(index, 0) per NPC (4805EB), and
  mode 0 with two or more present, seen namesakes falls to 45E892: `If
  Proc_21_49_450610(word) = index` -- the contest's winner returns the
  containment TRUE and every rival returns FALSE, so the catch-all names
  the winner and prints "I don't understand what you want to do with
  <Name>." Only a tie (450610 = -1) flags Me(424) and leaves the "Which
  <term>." question to generaltasks. p4PFX `blue guard`, a line with no
  verb at all and three characters aliased "guard", is "... with Bob."
  where Scarier printed the bare DontUnderstand. `[4.0]` p4PFX
  (`lib_cmd_verb_npc`, 2026-09-20)

- **Characters have no Prefix forms below 4.0.** Scarier built a "<Prefix>
  <Name>" candidate form for an NPC at every version; run390's characters()
  knows a character by `c(Name) Or c(Alias(0))` anywhere in the line
  (459109) and nothing else, and reads the Prefix only to compose an
  answer -- lastword() (42DA40) is called from co() only, so characters get
  no last-Prefix-word test either. So at 3.9 `x red guard`, `x blue guard`,
  `x a blue guard`, a bare `blue guard`, `talk to blue guard` and `where is
  blue guard` all answer exactly as the bare `x guard` does: the LAST
  namesake in index order, Cid. The containment binding above is therefore
  not 4.0's alone -- what 4.0 adds is the Prefix contest, so 4.0 keeps the
  forms (they settle the same crowd) and pre-4.0 drops them. `[<4.0]`
  p39PFX, p39PFX2 (`uip_build_entities`, `uip_match_entity`, 2026-09-20)
- **A task command's `%character%` at 4.0 is gated by the seen byte
  alone.** The substitution loop at 468DFC walks the NPC array from 469162
  under `CInt(npc.global_26) = 1` -- field 26, the character's own SEEN
  byte, the field npc_in_command() reads as var_DC(26) -- and under nothing
  else. There is no room test, so a character the player has met and walked
  away from still binds. It tries the LCase()d Name (4691A9, compare
  4691D8) and then each LCase()d Alias (4691F8, compare 46922E), and
  rewrites the pattern in place at 469574 with the Name. No Prefix form is
  built, so a typed article or Prefix word kills the match. p4CHREF: with
  Dave present and seen, `frob dave` is "FROBBED Dave." while `frob a big
  dave`, `frob big dave` and `frob the dave` are all "I don't understand
  what you want to do with Dave." -- the character catch-all, because the
  reference missed and the namesake scan still found him. Before `n`, `frob
  eve` (absent, unseen), `frob spook` (her alias) and `frob fay` (nowhere,
  never seen) are all the bare "I don't understand."; after `n` and back
  `s`, `frob eve` and `frob spook` both run the task with Eve absent, and
  so does `frob dave` from the Cave. This closes the "NPC seen gate for
  `%character%`" lead. `[4.0]` p4CHREF, Adrift_chref400b
  (`uip_match_entity`, 2026-09-20)
- **3.9 binds a task's `%character%` by Name anywhere, ungated.** run390's
  checktask loops the NPC array twice (44AD48 and 44B323) with no gate at
  all -- no seen byte, no room, no presence -- and tests `c(Name)` only
  (44AD5C/44B334, compare 44AD8A/44B385), never an Alias. So 3.9 is the
  mirror of 4.0 on both axes: p39CHREF answers `frob fay` "FROBBED Fay."
  with Fay in no room at all and never seen, and `frob eve` likewise before
  she is ever met, while `frob spook` -- Eve's alias -- falls right through
  to the library, "Who?" before she is met and "Eve is not here!" after.
  The article cells match 4.0's: `frob a big dave` and `frob the dave`
  miss. `[3.9]` p39CHREF, Adrift_chref390b (`uip_match_entity`,
  2026-09-20)
- **Below 3.9 a task's `%character%` matches nothing.** The UTF-16 string
  census finds "%character%" in run390.exe and run400.exe and in neither
  older Runner -- "%object%" is in all four -- so run370/run380's checktask
  never rewrites the pattern and it can only meet a line that spells the
  reference out literally. Every `frob` cell of the p4CHREF feed is "I
  don't understand." under both, with Dave standing in the room and seen.
  `[<3.9]` p37CHREF, p38CHREF, Adrift_chref370b, Adrift_chref380b
  (`uip_match_entity`, 2026-09-20)
- **The pre-3.9 `%object%` substitution reaches plain commands too, and
  compares equal.** The one-object-by-the-line rewrite already ported for
  checkwild patterns (see "the pre-3.9 first named object" in this index,
  run380 checktask 43B78B into replaceob 427704) is not a checkwild
  speciality: run370/run380 substitute for any pattern holding "%object%"
  and then compare the rewritten pattern against the whole line, `*`
  wildcards or not. Where there is no `*` the comparison is plain equality,
  so the line must be the pattern with the object's Short in place and
  nothing more. p37CHREF/p38CHREF, task `nurb %object%` over a rock with
  Prefix "a big": `nurb rock` is "NURBED a big rock." and both `nurb a big
  rock` and `nurb big rock` are "I don't understand." -- the Short binds,
  the Prefix never does, and the completion text still prints the full
  Prefix form. marooned.taf (3.80) carries five such commands (`light`,
  `burn`, `throw`, `toss %object%`), so the corpus exercises it. `[<3.9]`
  p37CHREF, p38CHREF (`run_match_task_commands`,
  `run_pre400_substitute_references`, 2026-09-20)
- **A pre-4.0 task command's `%object%` and `%character%` are substituted
  from the LINE, and the first hit spells the command while the last one
  binds.** checktask (run390 44AA5A, run380 43B78B, run370 4332CA) walks
  the whole object array with no break before it tests the command at all:
  every hit stores its index, and Replace() fires only on the first,
  because it leaves no "%object%" behind. So the FIRST namesake the line
  names makes the literal that equality or checkwild then judges, and the
  LAST is the reference the task's text expands. p39WILDREF answers `blip
  zog blip rock blip gem blip` against `* zog * %object% *` with "WILD1 a
  gem." -- the rock matched it, the gem is printed -- and p38WILDREF the
  same. Because the search is over the line and checkwild's pieces are
  order-free, `blip gem blip zog blip` runs the task at 3.7, 3.8 and 3.9
  where the positional tree (and 4.0) refuse it. 3.90 walks twice, Short
  then Alias (44AB65), gating both on the seen byte and on nothing else --
  so `blip stone blip` binds the gem and an absent object binds -- and
  adds %character% at 44AD2A, by Name with no gate whatever: the King
  binds from the Cave he was left out of. 3.7/3.8 know only the Short and
  gate on nothing at all, which is how p38WILDREF binds a coin two rooms
  away and unseen. %number% and %t_number% follow at 44ADxx (see the next
  entry but one); a command carrying any other marker is still handed back
  to the tree. `[<4.0]` p37/p38/p39/p4WILDREF
  (`run_pre400_substitute_references`, 2026-09-20)
- **A task command's GROUP is the LAST thing 4.0 tries, and below 4.0 it is
  not syntax at all.** run400's command loop (45D9FC-45DBA4) tests a
  command three ways and stops at the first that takes: plain equality
  (45DA51), then, for a `*` command, the wildcard matcher (45DA8C ->
  457D68), and only then NewParse's group expansion (45DADB -> 45D940). So
  every group command is matched *literally* before it is matched by
  expansion, and a group inside a `*` command is never expanded at all.
  p4GROUP task `zog [rock/gem]` runs on `zog rock`, `zog gem` **and** `zog
  [rock/gem]`, while `* blip [red/blue] *` runs on `xxx blip [red/blue]
  yyy` and not on `xxx blip red yyy`. Pre-4.0 checktask is that loop with
  the third step missing: it holds no `[`, `]`, `{` or `}` literal anywhere
  (run390 44AA5A-44B6E6, run380 43B6A3-43C51D, run370 433227-433E4A), so a
  group below 4.0 is punctuation the player has to type -- `zog rock` is
  the object catch-all in all three Runners and `zog [rock/gem]` runs the
  task. The literal test is plain equality: reordering the alternatives,
  adding a space inside the group and dropping the brackets are all
  refused, case is folded, and a keyboard line's double space is collapsed
  first (that last cell is the one deviation left -- the Runner does not
  collapse a line it builds itself, and separating the two would buy a cell
  needing a typed double space *and* a bracket). Zero corpus exposure
  either way, measured over every .taf in `games/` and `downloaded/`: all
  6685 group-bearing task commands are in 4.00 files, and no game at any
  version puts a `*` and a group in one command, so the suite is 429 PASS
  before and after. `[all]` p37/p38/p39/p4GROUP (`run_match_task_commands`,
  2026-09-20)
- **A task command's %number% is a substitution, and below 3.90 it is a
  literal.** checktask spells the marker out before it tests the command at
  all -- run390 44ADDF for %number% (numintext, 4332C8) and 44AE8B for
  %t_number% (numintext2, 42946C), run400 the same pair inside
  Proc_19_36_45F268 -- so it is never a positional wildcard, and a `*`
  command carrying one is decided pre-4.0 by *checkwild*, whose pieces are
  order-free. p39NUMREF's `* zog * %number% *` runs on `blip 7 zog`, and
  `blip 9 zog 3 blip` answers "NUM2 [9]." -- numintext takes the LEFTMOST
  digit in the line, not the one the pattern reached; run400 substitutes
  identically and then cuts, so it refuses both. That pair of cells is the
  whole 3.90/4.00 split. numintext is nobody's idea of a parser: leftmost
  digit, the non-space run from there, a "-" if the character before it is
  one, then `Val()` -- which is why `zork 007 apples` and `zork 3x apples`
  match nothing at either version, the command being spelled "zork 7
  apples" / "zork 3 apples". Both markers are replaced with DIGITS while
  the output filter spells %t_number% out, so a %t_number% command can
  match nothing at all (`frob five` and `frob 5` are both refused). The
  number is one Long, written even by a command that does not match (`nurb
  5 blip` runs nothing and the next `zap` prints 5), left alone by a line
  with no digit, and 0 until something sets it -- turn 1's `zap` is "ZAP
  [0] [zero].", not the "[Number unknown]" Scarier used to print. Below
  3.90 none of this exists: run370 and run380 hold no `%number%`,
  `%t_number%` or `%text%` literal anywhere in the exe, so all three are
  text the player has to type and the output filter prints them raw (`zap`
  -> "ZAP [%number%] [%t_number%]."); `%text%` stays a literal at 3.90 too.
  Real corpus exposure, unlike the group lead: 50 3.90 commands in six
  games, several glued to wildcards (druggy_lane `take *%number%*`, Vampire
  `push * %number% *`, circus `turn* lock* %number%`), all six with
  goldens, none of which moved. 428 PASS before and after. `[all]`
  p37/p38/p39/p4NUMREF (`run_substitute_number_references`,
  `var_is_unknown_reference`, 2026-09-20)
- **The pre-4.0 ask block wants the name at column 5.** characters()' one
  position test, and the only one in the handler: 459882 skips the whole
  `c("ask") Or c("talk to")` conversation block unless `InStr(line,
  LCase(Name))` or `InStr(line, LCase(Alias(0)))` is 5 for an ask, 9 for a
  talk to -- the moment "ask " or "talk to " ends. So 3.9 answers `ask blue
  guard about key` with therest's seed, "You can't talk to that.", while
  the talk-to hint arm just above (45975C, no position test at all) still
  answers `talk to blue guard` with 'Use the format "ask Cid about
  [subject]".' run400 47F8F7 has no such test: 4.0 binds by containment and
  the contest, so the same line is "BOB KEY.". `[<4.0]` p39PFX, p4PFX
  (`lib_pre_400_ask_column`, 2026-09-20)
- **3.9's examine is referencedob(), not the parser.** examines() takes
  whatever referencedob() (42DEF8) returns. Pass one counts the objects
  co(obj, 0) accepts -- so the last word of an object's own Prefix settles
  a crowd for examine exactly as it does for take and drop, and p39PFX's
  `x tree red` is "A red tree.". None is "Nothing special." under the
  end-of-turn "Which tree."; one is the answer; MORE than one runs a second
  pass (42DF60) over EVERY object in the game, present or not, seen or not,
  keeping the ones whose Short or first Alias is exactly lastword(line),
  and anything but a single survivor returns &HFE = -2: "Please examine one
  object at a time." (44BFA9), the line a bare `x all` gets. p39PFX2's
  three trees are all Short "tree" and share the last Prefix word "red", so
  `x red tree`, `x a red tree`, `x the red tree` and `x big red tree` are
  all that refusal -- the multi-word form no help at all, where 4.0's
  contest answers "A big red tree.". `[3.9]` p39PFX2
  (`lib_examine_crowded_390`, `lib_disambiguate_object_common`, 2026-09-20)
- **The 3.8 object loop's scope test** is obhere (run380 4272E8): a
  dynamic object is present when held, worn, loose in the room, held or
  worn by a character in the room, or INSIDE (&HF6 only, never on) a
  present parent that is not closed; a static when it is in the room.
  Record only: Scarier's scope already answered the same (`lie on bed`
  from the next room "You can't lie on that.", p3xTKA cells 174/177/180);
  the rule is reused by the 3.7/3.8 take-from container slot
  (lib_obhere_380). `[3.8]` p37TKA, p38TKA (2026-09-20)
- **Nothing is referenceable until something lists it.** The loader seeds
  the seen byte and afteroa sweeps it (statics only at 3.9). A unique
  absent-seen winner answers "can't see X from here!" and ticks; a tie or
  no winner answers "see no such thing" and does not tick. `[3.9+]`
  (386c9c570). A part-of-character static is stamped seen by obhere when
  its holder is the player or a seen NPC in the player's room. `[4.0]`
  humbug T727
- **463640 is the 4.0 noun resolver.** Scoring: Short whole word +1, first
  alias +1, +1 per Prefix word, an empty Prefix counts as `a`. With two
  objects named, a tie gives the game's DontUnderstand. NPCs are never
  candidates. `[4.0]` House throw (0743cefef). therest's absent-seen clause
  scores every object the line names. `[4.0]` warlord, house doors
  (5662e7397)
  - Mode 2 (a plain `drop X`) scores held objects first, then everything
    present. A tie prompts `Which <term>.` only when Me(424) is set, which
    happens by comparing each tied object's Short with the object TWO
    indexes past the previous tie, else "It is not clear which <term> ...".
    `[4.0]` wilkins T110-T117 (`lib_drop_resolve_400`, 2026-09-19)
- **4.0 named take.** The take falls back on every seen object: "nothing
  worth taking here" / "not clear which" (p4TAKE, 7d051a7d7). It runs the
  tasks' `get <the object>` before both refusals (icecream T0, 8f2898bd4).
  A refused take leaves the typed line (take becoming get) to the task
  dispatcher (3eefa06b2). `[4.0]`
- **Auto-"from" take.** 4.0 retakes a seen object "from" its holder, and
  the referenced object is cleared before each typed line, so a type-1
  Var1=0 restriction is silent. `[4.0]` warlord T104 (68bc1382a)
- **4.0 single take of a worn object** counts worn as held: "You are
  already carrying X." `[4.0]` 3monkeys T65 (2026-09-19)
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
    troll T64, secret_of_lost_world T56. `[<4.0]` (2026-09-14)
  - Namesakes in the pre-4.0 handlers. 3.8 and 3.9 put every candidate
    through co() before any held/worn/openness filter, so `wear hat`,
    `remove hat`, `open box`, `close box`, `take box` and 3.8's `drop hat`
    do nothing and the turn is the prompt; 3.9's drops() recounts with mode
    2 (held, worn, or in/on a held parent) and takes() with mode 1 (loose in
    the room). 3.7's handlers never call co(): drops/takes skip every held
    namesake lacking its Prefix's last word unless it is the only one
    ("Drop what?" for two), wears/removes act on EVERY held namesake with
    the last one's message, openclose changes EVERY namesake's state, and
    examines asks "Which hat would you like to examine.  The red hat or the
    blue hat?". `[<4.0]` p37TASK/p38TASK/p39TASK
    (`lib_disambiguate_object_common`, `lib_co_pre400` mode 2,
    `lib_co_note_line_top`, 2026-09-19)
  - The rub arm is 4.0's alone. Below 4.0 `rub coin` that no task takes is
    the object catch-all and `rub,coin` DontUnderstand. `[<4.0]`
    (`lib_cmd_rub_object`, `lib_cmd_rub_other`, 2026-09-19)
  - The pre-4.0 drop "and" arm skips an object inside a held open
    container: `drop nut and coin` with the nut in the held bag drops only
    the coin, `drop nut and stone` is "You are not carrying anything.".
    `[<4.0]` p37TASK/p38TASK/p39TASK (2026-09-19)
  - The article test is case-sensitive: only lower-case `a`/`an`/`some`
    become `the`. p4PFX (602428ad6)
  - A typed look is an exact whole-line list, and a bare `x` exits examines
    (3.9+). The NPC examine overwrites the object's text and still ticks
    (4.0). lair (5662e7397)
  - The examine state line is always " is ". `[4.0]` magicshow T80
    (e0f709c46). Only 4.0 has one: the 3.7-3.9 schemas carry no states, and
    run390's tail is the open/closed line, then whatisinon().
  - Bare examines. At 3.7/3.8 the typed look is the same exact whole-line
    list (`l`, `look`, `x room`, `x location`), so bare `x`, `ex`,
    `examine`, `exam`, `look at`, `read`, `x the`, `examine room`, `look
    around` are "Nothing special." and `l room` is DontUnderstand. At 3.9
    bare `x`/`ex`/`examine` reach checkverb: "Examine what?"; `exam` stays
    "Nothing special.". At 4.0 bare `x`/`ex` is DontUnderstand and `exam`,
    `look at`, `read` are "You see no such thing.". `ex zzz` and `exam zzz`
    are examines' at every version. `[3.7-4.0]` p37EXAM/p38EXAM/p39EXAM/
    p4EXAM (`lib_cmd_look_typed`, `lib_cmd_examine_other`, 2026-09-19)
  - `look X` is no examine at 3.7/3.8. examines enters on c() of x,
    examine, look at, ex, exam, read (3.8 also look in), so `look coin`,
    `look me`, `look zzz` and an absent object reach therest's look arm:
    "Nothing special.". 3.7 has no `look in` and refuses an absent object
    first. `[<3.9]` p37EXAM/p38EXAM (`lib_look_is_not_examine_pre390`,
    `run_therest_pre400`, 2026-09-19)
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
  p38SIT, p39SIT, p4SIT (`lib_stand_sit_lie`, 2026-09-19)
  - No object arm asks whether the player is already there: `sit on stool`
    twice sits twice. Sitting on an object is "sit down on" even from
    lying. Before 3.9 the object is named by its authored Prefix.
  - Bare `stand` at position 0 is "already standing!", even standing on an
    object.
  - Pre-3.9 bare `sit`/`lie` keep the parent and never name it ("You sit
    up.", "You lie down on the ground."); only `stand` clears it. 3.9+
    names it: `sit` standing on O is "sit down on the O"; lying on a
    sittable O is "sit up on the O"; `lie` on a lieable O is "lie down on
    the O", otherwise "on the ground." and the parent goes.
  - 3.9+ `sit on the ground` on the floor is "are already sitting on the
    floor!" (or "ground!") with a literal "are".
  - Pre-3.9 moveroom only looks at the position: standing on an object, a
    move prints no "(Getting off ...)" and the parent survives it.
  - run370's object loop matches Short or Alias with no scope test, the
    last match winning, and runs before therest's "can't see" test: `sit on
    bed` from the next room sits on it (`lib_cmd_sit_scan_370`,
    `lib_sitstand_claims_370`).
  - 4.0 absent sit/lie targets are therest's "You can't see the X."
    (`lib_cmd_verb_absent_400`).
  - `sit`, `lie` or `stand on the ground/floor` is "You can't sit/lie/stand
    on that." when the line names on/in, except 3.9+ sit, which has a
    ground arm (`lib_floor_named`).
  - 3.9+ `get off X` answers "You are not standing on anything!" before it
    looks at X. Before 4.0, takes() runs first and excludes only `get on`
    and `get down`, so `get off stool` is a take; 3.7/3.8 have no get-off.
    In 3.9 a take that happened stands; otherwise sitstand's answer
    replaces it.
  - The 3.7/3.8 static take refusal names the raw Prefix: "You can't take
    a chair."
  - sitstand is blocks entered on c("sit"), c("stand"), c("lie") ANYWHERE
    in the line, run in code order and each overwriting the one message:
    `sit lie` lies, `stand sit` stands, `please sit` and `push stone sit`
    sit on the ground, `sit on chair stand on stool` stands on the chair
    (last object in index order), `x stool sit` examines and still sits
    (`lib_sitstand_anywhere`). Volant `stand your ground` is "You are
    already standing!".
  - `lay` is a lie word in run400 only: 3.7-3.9 `lay down` is "I don't
    understand." and `lay on stool` the catch-all (`lib_lay_pre400`).
  - 3.7/3.8 `x me` with an empty PlayerDesc is one string; "circumstances."
    gets its full stop only before the sitting or lying clause, and standing
    on an object is "...the circumstances  You are standing on a stool."
    with two spaces and no full stop.
  - 3.7/3.8 `x me` never lists what the player wears (run370 435A9B-435BEA,
    run380 43D3EC-43D53B have no worn loop); only `i` does. 3.9+ appends
    "You are wearing ...". p37SITN, p38SITN (2026-09-20)
- **What a sit/stand/lie word does to the rest of its line.** `[all]`
  p37SITN, p38SITN, p39SITN, p4SITN, cmdfile_psitn.txt
  (`lib_sitstand_anywhere`, `run_line_for_sitstand`, 2026-09-20)
  - Everything generaltasks writes before sitstand loses to it: `give coin
    to bob sit` (the give echo arm is later but only writes when the message
    is empty, and gives nothing), `i sit`, `inventory sit`, `hint sit`,
    `help sit` and `say hello sit` all just sit. A direction word is never
    walked: `n sit`, `north sit` sit in place.
  - Everything after sitstand overwrites it and leaves the player standing:
    score, the swear words and characters() at 460675 (`talk to bob sit`
    is the "ask Bob about" hint, 3.7-4.0). The examines only replace the
    text; the move stands (`x stool sit`, above).
  - Pre-4.0 characters() takes `ask` only at column 5 (`InStr(1, line,
    LCase(name)) = 5` at run390 459818): `ask bob about hat sit` answers
    the topic, `sit ask bob about hat` sits. 4.0 speaks from anywhere.
  - takes and drops claim first (`take stool sit` takes; `drop stool sit`
    drops), each with its own summary. wears/removes act and are then
    overwritten: `wear hat sit` puts the hat on and prints "You sit down on
    the ground." (`i` shows it worn), `remove hat sit` takes it off and
    sits, `wear hat lie` wears and lies. 4.0 wears runs its object loop
    the same way.
  - `put coin on stool sit`: 3.8/3.9 refuse the put ("You can't put
    anything on/onto the stool.") and the refusal stands; 3.7 and 4.0 sit
    on the stool with the coin still held. So the port lets put claim only
    when it moved something, at 3.8/3.9 also when it refused.

### Put and take-from

- **Pre-4.0 take "and" with nothing takeable.** When none of the named
  objects is a candidate: "You can't get any of them."; 3.9 says "either of
  them." for exactly two. A candidate is a dynamic object loose in the
  room, or at 3.8/3.9 in or on something loose here or a static here. 3.9
  names with co(obj,1) and wants it seen; 3.8 names with co(obj), no seen
  test; 3.7 names by c(Short) and counts only loose objects. A held object
  is named, never a candidate. `[<4.0]` p39ABSNPC T36, p3xPUT
  (lib_take_and_none_pre400)
- **Pre-4.0 take "and", the main loop** (run380 43D788, run390 454CA3).
  After the zero-candidate summary takes() seeds "<You> pick up " and
  walks the co-named objects in INDEX order: a held one is silent unless
  the message was already overwritten, when "You've already got X!"
  replaces it; a worn one (3.8+) replaces it with "You are already wearing
  X!"; a loose candidate is taken and listed with ", " / " and " / "." by
  the candidates still to come, or the hands-full line is appended; an
  object in or on something (3.8+) while the message is still the seed
  (or ends " from here!") derives its parent: present, the line becomes
  "<line> from <parent>" for insides(), absent "You can't see X from
  here!". A tail still equal to the seed becomes "Please take objects from
  one place at a time." (3.8+; 3.7 leaves it). The rewritten line then goes
  through insides() when the container slot is valid and is silent when it
  is not. Measured: `take table and stone` with the stone held is "You
  pick up the table."; `take hat and stone` with the hat worn is "You are
  already wearing a hat!the stone." (3.9 "the hat!"); `take stone and
  nut` with the nut in a box on the floor is 3.8's dangling "You pick up
  the stone and" (the derived take-from fails on the unheld box and says
  nothing) and "You pick up the stone." at 3.7/3.9; `take nut and key`
  with the box and table on the floor is 3.8's "Please take objects from
  one place at a time." and the zero-candidate "You can't get any of
  them." at 3.7/3.9. `[<4.0]` p3xTKA, p3xTKB (Adrift_205-207_ptka,
  Adrift_207-209_ptkb; lib_take_and_pre400)
- **4.0 take "and" pieces** (get_outer 4582D8 / get_piece 473A34): the
  noun part splits on " and " and each piece resolves on its own; a piece
  in or on something turns the whole line into a take-from of the FIRST
  such parent, otherwise every piece is taken and each static refused in
  turn. `[4.0]` p4TKA, p4TKB (Adrift_208_ptka_4, Adrift_210_ptkb_4;
  lib_take_and_400)
- **Take-from with " and ", by version.** 3.7/3.8 (insides 4468B3):
  the container slot walks the co-named objects in index order and is
  replaced when the current slot is not present (obhere 4272E8) or the
  object's highest name position in the line is beyond the slot's lowest,
  so the LAST present name after or before "from" wins; fewer than two
  matches without "all" is "You can't do that!"; the and-form collects
  only the named contents, the all-form everything, and an empty pick is
  "There is nothing inside <a slot>." (plain: 3.7 nothing-inside, 3.8 the
  bare "You take "). 3.9 (insides 462FD2): named = the first co-named SEEN
  object before "from", container = the LAST co-named after it; fewer than
  two matches, or no named object, without "all" is the named object's own
  answer ("Get <the X> from what?" with the pending slot `get <X> from`
  filled by the next line, "<The X> isn't in or on anything!", "You can't
  do that!"); any "and" collects only the named contents and never
  complains, so `get nut and bolt from box` takes the nut and says nothing
  about the bolt. 4.0 (get_piece 472F1F): the names before "from" resolve
  first (none: "Take what?"), the container is the FIRST clause after it
  (unresolved: "I don't understand where you want to get things from.",
  no turn), then closed / empty / contents; a line naming nothing on a
  surface that is no container is "You can't take anything from X.",
  otherwise "Take what?". A trailing "from" is silent at 4.0, the no-name
  answer below 3.9 and the named answer at 3.9. `[all]` p3xTKA/p4TKA,
  p3xTKB/p4TKB (lib_take_from_and_390, lib_take_from_and_400,
  lib_take_from_slot_pre390, lib_take_from_nowhere_named_390,
  lib_take_from_trailing)
- **3.7's take-from catch-all** runs two passes over the co-named objects:
  one present (a static in the room; a dynamic held, worn, loose here or
  in/on a held parent) and seen, in index order, is "I don't understand
  what you want me to do with X."; failing that the first unseen one is
  "What <Short>?". `[3.7]` p37TKA, p37TKB (lib_take_from_answer_370,
  lib_present_370)
- **Pre-3.9 `i` lists held surfaces like containers:** "  Inside <the X>
  is <list>." (whatisin1, run370 42B78E / run380 42998C) for a surface
  too; 3.8 skips a closed container, 3.7 lists its contents. `[<3.9]`
  p37TKB, p38TKB (lib_list_in_object_pre_390)
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
    still in the buffer. troll T116, losttomb T85, secret_of_lost_world
    T118 (`lib_put_sweep_claims_390`, `run_typed_line_task_commands`,
    2026-09-19)
  - insides() resolves the single put's object with co(obj, 0) over every
    object and moves the first seen, present, held-or-loose one; a prefix
    naming an absent namesake is not heard. `[3.9]` secret_of_lost_world
    T118 (`lib_put_co_resolve_390`, 2026-09-19)
  - insides() answers a put whose line names fewer than two objects BEFORE
    its task look-up: "Put <the X> inside/onto what?" or "You can't do
    that!", then the sweep. `[3.9]` cybercow T62
    (`lib_put_refusal_first_390`, 2026-09-19)
  - The target refusals also come before the task look-up, then the sweep:
    "You can't put anything inside/onto <the X>." (wrong kind), "... as it
    is closed!", and "You can't put anything inside/onto that!" (two or
    more objects named anywhere, none after the preposition, no "all"). A
    failing task with a claimant leaves the refusal; with none the LOUD
    FailMessage replaces it; a passing task replaces it. `[3.9]` pPUTREF39
    (`lib_put_refusal_sweep_390`, `lib_put_that_390`, 2026-09-19)
  - A put that comes to nothing but a size or capacity refusal moves
    nothing, so there is no sweep: the LOUD pass gets the line and a
    matching task's FailMessage replaces the refusal. A matching FAILING
    task never claims a put that fits. `[3.9]` pPUTREF39, pPUTCLM39
    (`lib_put_named_pre400`, 2026-09-19)
  - Capacity decodes the count from the FIRST digit only:
    Val(Left(Format(cap, "00"), 1)) × mult ^ Val(Right(..., 1)), so 100 is
    one size-1 object, not ten. run400 takes Left(s, Len(s) - 1). `[3.9]`
    (`obj_get_container_capacity`, 2026-09-19)
  - The target is chosen after a raw substring search for the preposition,
    so the "in" inside "coin" or the "on" inside "stone" counts: `put coin
    and stone in junk` targets the stone. A valid target gets the other
    named objects put into it. `[3.9]` pPUTREF39 (`lib_put_target_390`,
    2026-09-19)
  - The all/and put counts before it moves: it walks the named objects in
    index order, counts each one whose size fits in the space left, then
    moves the FIRST that many, whatever their sizes. Leftovers add "  You
    can't put any more inside the bag as it is full."; a count of 0 gives
    "Nothing will fit inside the bag."; a single object keeps "can't fit
    ... at the moment". `[3.9]` pPUTREF39/pPUTREF39E (`lib_put_in_backend`,
    2026-09-19)
  - `put all in/on <nothing>`: c("all") skips the two-name test but not the
    target choice, so the answer is "You can't put anything inside/onto
    that!", then the sweep. `[3.9]` pPUTFULL39 (`lib_put_that_390`,
    2026-09-19)
  - A present static named as the object: insides() counts only movable
    dynamics, so the target's refusals come first, then "You can't see
    that." and the sweep. An unreachable object takes the same path.
    `[3.9]` pPUTFULL39 (`lib_put_named_pre400`,
    `lib_put_not_reachable_pre400`, 2026-09-19)
  - Also measured identical on pPUTFULL39 with no code change: an object on
    a floor supporter gives "You can't do that!"; the onto and-arm over
    capacity puts every candidate; a static target in another room gives
    "Put the coin inside what?"; `drop coin in junk` is a drop. Pre-4.0 has
    no lock state, so "locked container" is moot.
  - Pre-4.0 drops() picks its arm by c("all"), then c("and"). The "and" arm
    drops the named held/worn objects that no "drop <Short>" task claims,
    never names a missing one, and with none dropped says "You are not
    carrying anything." `[<4.0]` pPUTFULL39 (`lib_drop_and_arm_pre400`,
    2026-09-19)
  - Player MaxSize/MaxWt use Val(Left(Format(v, "000"), 2)) × mult ^
    Val(Right(v, 1)) in both loaders, which matches the plain decode below
    1000 (the corpus maximum is 994). (`obj_convert_player_limit`,
    2026-09-19)
- **A 3.8 in/on object with an unset parent** goes in the first container.
  (5cf3d7059)
- **The take-from handler's own answers.** The 3.9 insides() decision
  procedure; `empty` is take-all-from in 4.0 only. p39DARK/p4TFROM
  (2ab1a7c5d)
- **Pre-3.9 take-from answers.** In run380, closed overwrites not-holding.
  Nothing to take is "There is nothing inside <raw Prefix> <Short>." even
  for a surface in 3.7. In 3.8 a single named object is a bare "You take ".
  No "is not inside" line exists before 3.9. run370's insides() needs
  c("get")/c("remove") plus c("from"), so `take gem from box` is the
  catch-all; a bare take's rewrite lands only for a `get` line (`get gem`
  is "You are not holding a box.", `take gem` is "Take what?"). `[<3.9]`
  p37TFSW/p38TFSW (`lib_take_from_line_370`, `lib_take_from_answer_370`,
  `lib_take_from_nothing_taken_pre390`, 2026-09-19)
- **4.0 take capacity.** Size is tested first ("<Your> hands are full.")
  and weight second ("<The X> is too heavy for you to carry at the
  moment."), even out of a container the player holds. A player-held object
  loads with its container field cleared, so it never phantom-weighs object
  0. `[4.0]` wilkins, businessasusual, provenance, riding_home
  (`lib_take_over_capacity`, 2026-09-15)
- **3.9 take-from capacity.** Size first, weight second; weight is waived
  when the container is held, size never. A single named take-from refuses
  per object. The all/and forms pre-count the objects that fit: if none,
  "<Your> hands are full." / "That is too heavy."; otherwise one summary
  follows. `[3.9]` alexis T28, alexis_worn_cube T27
  (`lib_take_from_over_capacity_390`, 2026-09-15)
- **4.0 put names a present-but-unseen object by asking the scorer
  directly.** 463640 mode 2 scores every present object regardless of seen
  state, moves it, and leaves it unseen, so the name composer answers
  "that". Fires only when the top parse's failure really is the seen gate.
  `[4.0]` hub T79 (`lib_put_fragment_present_object`,
  `lib_put_print_object_or_that`, 2026-09-19)
- **4.0 `put box in box` takes the box first.** insides tests possession
  before the itself-test, and the take piece has already run: "(Taking the
  box first)" / "You can't put an object inside itself!" and the box is in
  hand. The wording follows the target's flags ("in or on itself!",
  "onto itself!", "inside itself!"), not the preposition. `[4.0]` PBOXBOX
  (`lib_put_in_backend`, `lib_put_on_backend`, 2026-09-19)
- **4.0 put noun = the mode-2 scorer, held first**, the same resolver as
  plain `drop`, no seen gate: `put key in box` with the brass key held and
  the iron key loose puts the brass key with no prompt; both gems loose tie
  in pass 1 and give "It is not clear which gem you are referring to.";
  both coins held tie in pass 0 and prompt "Which coin.". The all/and/
  except forms keep the ordinary parse. `[4.0]` PPUTTIE
  (`lib_put_named_400`, 2026-09-19)
- **4.0 closed container: the take comes first; `put all` counts hands
  before the lid.** `put ring in box` with the box shut prints "(Taking the
  ring first)" / "The box is closed!" and the ring IS taken; the refusal is
  size-like, printed with the line left for the task pass. `put all in
  <X>` with nothing held says "You are carrying nothing!" even against a
  shut container; with X the only thing carried it says nothing and the
  catch-all answers. `[4.0]` PCLOSED (`lib_put_in_closed_400`,
  `lib_put_all_common`, 2026-09-19)
- **Pre-4.0 put leftovers.** A fragment that names nothing anywhere makes
  3.7/3.8 say "You can't do that!" ahead of the container refusals (an
  object named but out of reach still counts, so p38DARK's target-first
  order stands). There
  is no exception list before 4.0: `put all except X in Y` is `put all in
  Y` at 3.7-3.9 and X is put too; with nothing held, the all arm's empty
  answer (3.7 "You have nothing to put inside the cupboard.", 3.8 "You are
  not carrying anything.", 3.9 "Nothing will fit inside the cupboard.").
  run370's openclose does not list a static container (`open chest` is the
  bare "You open the chest."). drops()' all arm skips only a name after
  " but " (run370 has no "but"). `[<4.0]` p37PUT/p38PUT/p39PUT
  (`lib_put_co_count_pre390`, `lib_cmd_put_in_except_multiple`,
  `lib_cmd_drop_except_multiple`, `lib_cmd_open_object`, 2026-09-19)
- **3.9 take all sweeps open containers; "and" beats "all" in put.** After
  the floor, run390 takes() sweeps every open container or surface lying
  directly in the room through the take-from arm ("You take the coin, ...
  from the cupboard."). insides() sets its mode on c("all") and then
  c("and"), so `put all except coin and stone in cupboard` is the and-arm.
  The sweep's capacity arms and the and-arm's worn objects are not
  modelled. `[3.9]` p39PUT T23-30 (`lib_take_all_sweep_390`,
  `lib_put_all_common`, 2026-09-19)
- **Pre-4.0 drop "and" arm walks, never parses; `everything` = `all`;
  3.7/3.8 absent static target can't see.** `drop foo and bar` is "You are
  not carrying anything." and `drop coin and foo` is "You drop the coin."
  `[<4.0]`. run390 rewrites "everything" to "all", so `put everything in
  zzz` is the put-all refusal `[3.9]` (3.7/3.8 crash, see deviations). The
  pre-3.9 whole-game target search finds a static container in another
  room and says "You can't see a cupboard." after the not-a-container
  refusal `[<3.9]`; 3.9 never finds it. p37PUT/p38PUT/p39PUT
  (`lib_drop_and_arm_collect_pre400`, `lib_put_that_390`,
  `lib_put_static_absent_pre390`, 2026-09-19)

### NPCs, walks and battle

- **Namesake characters, pre-4.0: no question, first or last wins.**
  characters() is one loop over every NPC in index order, naming each by
  c(Name) Or c(Alias(0)) anywhere in the line. An arm that assigns the
  message outright leaves the LAST named NPC's answer; one guarded by an
  empty message leaves the FIRST's. With Ann and Bob, both "a guard", here
  and Cora, a third, next door and unseen: last present for `x guard`
  (examine by containment, all versions), `ask guard [about key]`, 3.9
  `take stone from guard`; last named, here or not, for `talk to guard`,
  `where is guard`, 3.7/3.8 `take guard` ("Cora is not here!"); first
  present for `give stone to guard`, 3.9 `take guard`, `hit`/`kick guard`
  and the 3.9 catch-all; first named for 3.9 `kiss guard`. 3.7/3.8 single
  characters: `kiss dave` is therest's "I'm not sure it would appreciate
  that."; `attack`, `hug`, `eat dave` are DontUnderstand; a 3.7 take line
  without get/remove never reaches insides(). 3.9 `take stone from cora`
  (absent) is "The stone isn't in or on anything!". `[<4.0]`
  p37/p38/p39NPCAMB (`lib_disambiguate_npc_pick`, 2026-09-19)
- **Namesake characters with the Battle System ON: dobattle strikes them
  all.** run390's dobattle (44CC1C-44D1D5) has no loop break either, so a
  line naming two present characters by Name or first Alias strikes both
  and there is no question of any kind at 3.9 -- SCARE's "Please be more
  clear, who do you want to attack?" is an invention at every version. The
  refusals inside the loop split: the blows and " can't attack <NPC> with
  <obj>" APPEND (44CDE8), while " can't <verb> with <weapon>!" (44D079,
  4.0 47EED3) ASSIGNS at both versions and " not carrying <weapon>!"
  assigns at 3.9 (44D0E7) where 4.0 appends (47EF41) -- so `hit guard` with
  a chopping sword and `attack guard with club` (on the floor) each answer
  once. At 4.0 the same lines find no target at all (dobattle reads no
  alias), print "Who do you want to attack?", and generaltasks' namesake
  question then replaces it. Also drives run390's Who prefix (44D1F4):
  `attack` / `ann` strikes Ann, and `look` spends it. `[3.9, 4.0]` p39BATT
  Adrift_1207, p4BATT Adrift_1208 (`lib_battle_line_names_many`,
  `lib_battle_400_namesake_tail`, 2026-09-20)
- **4.0's namesake question belongs to generaltasks, not to any verb.** It
  comes after everything the line printed and REPLACES it, whatever
  answered: p4BATT gives "Which guard.  A guard or a guard?" to `x guard`,
  `attack guard`, `attack guard dave`, `status guard`, `talk to guard`,
  `where is guard`, `give stone to guard`, `give club to guard` and the
  bare noun `guard` alike, and the give lines lose a real answer ("You
  don't have the stone!") to it. Only a task that claimed the line escapes
  (`probe` keeps its text; 48B60C) -- and even then the scan has already
  run, which is why such a line is not a turn. So the raise belongs at the
  tail of `run_all_commands()`, gated on 4.0, the game still running and no
  task having run, not in each verb: that alone retired SCARE's invented
  "Please be more clear about whose status you want." and "... about who
  you want to locate.". `[4.0]` p4BATT Adrift_1208, Adrift_1209
  (`lib_npc_400_raise_for_line_string`, 2026-09-20)
- **...and its term is the OBJECT that shares the flagged character's
  index.** Both halves of the block read that one untyped index and the
  object half (48B6B1) gets first refusal: it prints the object's `Short`
  -- replaced by each alias of it that is a whole word of the line, the
  last winning -- over the character list, whenever the index is also a
  present, seen object's and the line names it. The index is the LAST
  present namesake's raw 0-based character index: the scan loop (48B547)
  has no break, the term and list are the first hit's (45E7B5 refuses to
  rebuild a list the term is already in) but 45E8CA overwrites the index
  for every hit in the player's room. p4BATT has Dave 0, Ann 1, Bob 2,
  Cora 3 with Ann and Bob both "a guard", and sword 0, club 1, stone 2, so
  the flagged index is Bob's 2 and `attack guard with stone`, `give stone
  to guard`, `x guard stone` and `x stone guard` are "Which **stone**.  A
  guard or a guard?" where `x guard sword`, `attack guard with club`,
  `give club to guard` and `x guard dave` are "Which guard." -- a plain
  collision, not "the object the handler resolved" nor "the last object
  named". `[4.0]` p4BATT Adrift_1209, now identical on every turn
  (`lib_npc_400_find_namesakes_in` flagged index, 2026-09-20)
- **No answer narrows a term the namesakes share.** The answer re-runs the
  original line with the answer words spliced in BEFORE the term, so in a
  world whose two namesakes share every word `MemVar_4941F0` comes back
  equal to `MemVar_4941F4` whatever you type: p4BATT answers `ann`, `x
  guard`, `club` and `nonsense` to "Which guard." and all four are "That is
  still ambiguous!", the pair then clearing so the next line asks afresh.
  `[4.0]` p4BATT Adrift_1209 turns 15-25 (2026-09-20)
- **3.7/3.8 characters() arms.** `talk`/`speak` anywhere in the line with a
  named character gives the ask hint for the last one named, no room test.
  A present character named in an examine line overwrites the answer (`x
  dave with stone` = "A quiet man."). The attack arm answers "<Name> is not
  here!" for an absent first-named NPC and " don't have <X>!" for a
  not-held with-object. Bare `hit` is the attack arm, not "Hit what?".
  `[<3.9]` p37/p38NPCAMB (`lib_hit_absent_npc_pre390`,
  `lib_talk_hint_anywhere_pre390`, 2026-09-19)
- **Bare verbs below 3.9.** run370/run380 have no checkverb, so a bare verb
  runs its own arm with no object ("You can't lock that.", "You might need
  that.", "You press, but nothing happens.", "I don't think that is for
  sale."); only Take/Drop/Wear/Remove/With and 3.8's Open/Close ask
  "what?". Bare `give` is "(to Nobody) You don't have that.". No touch arm
  below 3.9 and no shake arm in 3.7. fix/repair/mend share one arm, "I
  don't think you can fix <X>.". Rub is 4.0-only, so at 3.9 a bare `rub` is
  left to the pending prefix. `[<4.0]` p37/p38/p39NPCAMB
  (`lib_bare_verb_pre390`, 2026-09-19)
- **3.9 absent characters, per verb.** `talk to`/`speak to` any named
  character, even absent or unseen, gives the "ask X about" hint (no room
  gate; the ask branch's "isn't here!" loses to it). `give obj to <absent
  npc>` is the object catch-all. A bare `give stone` echoes "(to <last
  named>)" and runs as that. `take <absent npc>` is "Take what?", because
  takes() answers before characters()' "is not here!". run390 names a
  character by Name or Alias(0) only. `[3.9]` p39ABSNPC
  (make_39_absnpcprobe.py)
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
    and 4.0 alike; the max attributes are a plain add. The dodge pronoun is
    he/she/it by Gender. wes_ghn, les_feux (4c7c20f64);
    secret_of_lost_world T125-126/T164, spirits_flight T17/T27 (2026-09-19)
  - The attitude action stores Var3 RAW into the NPC's byte (0 neutral, 1
    ally, 2 enemy). deaths T42/T48/T49 (2026-09-19)
  - The 3.9 speed action indexes the NPC by Var2 RAW, no
    referenced-character case. `[3.9]` deaths T35-T49 (2026-09-19)
  - The 3.9 blow has no accuracy roll: hit iff hitstrength >
    armourstrength, damage = max(0, hit - armour); hitstrength = strength +
    best weapon, armourstrength = defence + worn objects' field 76;
    getnexthit at speed 1 draws Int(Rnd*1)+1, the rest are constants.
    Scarier's `battle_legacy` path; read 2026-09-19.
  - Stamina recovery is a per-line pass that revives the dead;
    `battle_select_target` takes 0-stamina targets. (544868698, e78827349)
  - A type-7 stamina action that leaves an NPC at <=0 kills it. The player
    dies from it only in 4.0. `[3.9+]` cybercow_win T103
    (`battle_change_attribute`, 2026-09-19)
- **Task move "to same room as" (Var2 = 2)** names its NPC by RAW array
  index at 3.9, in the NPC arm and the player arm alike. run400 keeps 0 =
  player, 1 = referenced, N = NPC N-2. `[3.9]` fantasyworld task 92
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
    T152-T171 (2026-09-19)
  - An event whose clock ROLLS 0 parks for good, at every version: the
    restart and the start off a waiting clock both store the roll with no
    +1, the running block decrements first, and the finish test is `clock =
    0`, so the clock sits at -1. Only the task start adds 1. zelda T52-60;
    probe pEVROLL (`harness/make_39_evrollprobe.py [out] [38]`) (2026-09-19)
  - A restart-after-delay event with an immediate or task starter is a
    one-shot at every version: the finish block draws Rnd once and stores
    Int(Rnd * (EndTime - StartTime)) + StartTime, which is 0 because those
    fields are only read for a random-delay starter, and the waiting block
    decrements before it tests for zero. No StartText or LookText
    afterwards; the Rnd is still drawn. Probes EVRS, pEVROLL event C
    (2026-09-19)
- **Pre-4.0 ending mid-tick.** The ending (WinText, summary, "[Press any
  key to end]") is composed as the task that armed it finishes, right after
  the action loop, and the tail's ended test was made before characters(),
  so a walk's task that ends the game is followed by events() in the same
  tick. Event texts join pre-4.0 with the two-space separator unless the
  buffer ends in Chr(10) or "  ". `[<4.0]` haunt T84. 4.0 keeps the
  end-of-turn endmessage. (2026-09-19)
- **Completed tasks.** A 4.0 event that runs a completed task still walks
  the task's restrictions, so a failing restriction prints its message and
  a passing one prints nothing. riding_home T47 (2026-09-15)
- **Static objects moved by events.** A 4.0 static's presence is its
  per-room array o(28). The start mover (Obj1) replaces the rooms; the
  finish movers (Obj2/Obj3) only add one, clearing for hidden or held
  alone. `[4.0]` 3monkeys T109 (2026-09-19)
- **The phantom object.** The 3.9/4.0 object array is `0 To count` and
  loaded 0..count-1. The spare slot's zero position reads as "held", so the
  first "all held" move of a game also moves the phantom, and a roomgroup
  destination draws getaroom. `[3.9+]` hhorror T25
  (`task_move_phantom_object`, 2026-09-19)
- **Look text.** An event's look text is gated on the room being described,
  not on the player's room. goldilocks, cybercow
- **RNG parity.** `SCR_RNG=xoshiro` matches vbrng draw for draw, and it is
  the harness default (991a5f8d9, b150980c8). A death prints the end-game
  score summary. (5f20dd8ce)

### Output, wording and the room block

- **An empty authored PlayerName is "Anonymous" at 4.0.** run400 fills the
  field at load, and %player% and the third-person pronoun array both read
  it, PromptName off or not. run390 has no load-time default (unmeasured;
  Scarier keeps SCARE's "Player" before 4.0). `[4.0]` probe ANON; goldens
  woof, aliasagent, greekschool re-blessed (scvars.cpp, 2026-09-19)
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
  - The examine-self full stop is 3.9+.
  - The NPC examine overwrite applies at 3.9 too: a task's text is replaced
    by the NPC's description; the namesake check stays 4.0 only. `[3.9+]`
    cybercow_win T72 (2026-09-19)
  - Pre-4.0 read ends in the examine tail, the openness line and the
    contents. `[<4.0]` cybercow_win T97 (`lib_read_tail_pre400`,
    2026-09-19)
- **Other formatting:**
  - The "<Name> is here." sentence is capitalised only by the 4.0 loader's
    `#` substitution. run390/380/370 append the raw Name, and no Runner
    capitalises an author's own " is here." text. `[4.0]` goldilocks;
    twilight T12-34 (2026-09-15)
  - `isare()` is exact and case-sensitive, and the loader fills an empty
    Prefix with "a". yeh (496c115f2)
  - 4.0 room names take every matching alt's Changed. togetyou (fee19ae2a)
  - Below 4.0 a room's name is always its Short. "Changed" is a field of
    the 4.0 room-alt record only, so every alt this loader synthesises for
    a 3.7/3.8/3.9 game carries an empty one, and the Runners never look:
    run390's viewroom heads the room with the Short alone (447749) and
    run380's (439C08) reaches its alt array only for description text.
    `[<4.0]` read, not probed (`lib_get_room_name`, 2026-09-20)
  - The multi-take line comes before the earlier task text. `[4.0]`
    fullcircle T43 (b6d2f4f1f)
  - The (Getting off X first) bracket line prints on its own line.
    Monsters_r2
  - The score summary prints after every EndGame; NotifyScore defaults to
    OFF.
