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
- **Manifest:** 383 identical on every turn, 31 identical apart from
  whitespace, 13 with a report. Every differing row is classified under
  "Open leads" or "Nothing owed" -- and since the silent-task port
  (2026-09-20) none of them is an engine difference.
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
artefact. The 4.0 and 3.9 engine lists are empty: the six 3.9 rows that
were the silent-task deviation (alexis and alexis_worn_cube T99 `open
chest`, everything T38 `read diary`, lifesimulation T6, life `piss`,
the_hangover T34) went identical with that port on 2026-09-20.

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

- **Ambiguity prompts:** co()'s crowded arm (454454) and its -2/-1
  answers; which of them parks 4941EC, the object whose aliases the
  prompt's term is drawn from -- p4CO's trees and keys park one, its
  hut/shed pair does not, and the alias rule is ported off our own tie
  instead (see "A 4.0 answer REBUILDS the typed line" in the index);
  whether an object ambiguity on a task-answered line also
  suppresses the tick. Everything else this lead once listed is closed:
  454454's Prefix contest, the 4.0 state machine and the whole second-noun
  family are in the index ("The 4.0 Prefix contest", "That wasn't one of
  the options!", "A 4.0 question raised by a " with " line comes out of ONE
  half", "A 4.0 lock or unlock never asks which key", "A 4.0 crowd is the
  WHOLE line's, and its first object decides").
- **The library verb is matched anywhere in the line: the REST of it.** The
  verb half is now ported at every version -- see "A library verb is matched
  anywhere in the line" in the index. Measured 2026-09-20 on
  p37REW/p38REW/p39REW/p4REW with `cmdfile_pcasc.txt` (Adrift_250_casc37b
  .rtf, 249_casc38.rtf, 250_casc39.txt, 251_casc40.txt), 34 verb words
  against a nonsense head. What the probe still shows open:
    * A line naming TWO of these verbs is now measured and ported at every
      version -- see "Below 4.0 a two-verb line is decided by the call
      order" and "At 4.0 a two-verb line is decided by a DIFFERENT call
      order" in the index. What both ports leave alone is the same short
      list: a list line, a clauseless `put`, and the handlers whose place
      in the order nothing has measured yet (give, gotoplace, characters,
      dobattle). openclose and whereis are measured and ported -- see
      "openclose ACTS on every line and claims none of them" in the index --
      and the one cell that entry leaves open is `take ask bob about hat`,
      where characters() overwrites takes() at every version with a
      different sentence. gotoplace cannot be driven at all: drive.exe
      fails on a `go to` line at every Runner.
    * The one pre-4.0 cell left over: `x take off hat` with the hat WORN.
      wears and removes are plain `Call`s (run390 45F499/45F49E, run380
      4421FC/442201) and so can never claim, so removes takes the hat off
      and therest's examines arm then overwrites its message -- run370x and
      run390x answer "A felt hat." with the hat off. Scarier keeps the state
      right and prints "You remove the hat."; one line of one contrived
      cell, and the fix wants a two-step dispatch (act, `pf_truncate`,
      answer) that nothing else needs yet.
    * run370's own word for each of take/drop/wear/remove/examine, command
      slots 10-14 (`MemVar_4460FC(&HA)`..`(&HE)`, beside the slot-15 goto
      word `lib_cmd_go_place()` already reads). The probe game defines
      none, so this is unmeasured and unported.
  Nothing in the corpus types a nonsense head, so the suite says nothing
  about any of it; the 428 rows stay green either way.
- **Silent-task test scope, the unported rest.** run400 tests the whole
  turn buffer. Scarier counts anything a task's run adds (baroo) but still
  ignores text written before the dispatch. No corpus row is known.
- **Turn sectioning, the unported rest.** Every join this lead once listed
  is closed; the index carries them ("An event's text joins the turn's
  paragraph, at every version", "A library answer has no terminator of its
  own", "An NPC's blow joins the turn's string", "A walk announcement and
  an exits list end at the full stop", "A task the engine dispatched joins
  its CompleteText onto the turn's string"). What is left is the
  CompleteText of a task the player's own line matched, which the Runner
  REPLACES the turn's string with rather than joining -- deliberately not
  ported, because Scarier's handlers already keep the turn's text where the
  Runner's callers put it back, and nothing measured differs. The corpus
  case would be `thetest` (3.90), whose ALR Originals span exactly such a
  join, and which already matches the Runner on every turn.
  When reading `sweep_wine_breaks.py`, judge by `k1`: 5486 of the 5514
  Scarier-only breaks are the `<centre>` blank-line artefact (`k2`), so the
  total is a poor target on its own.

---

## Deliberate deviations (measured, not ported)

- **`drop X in Y` is not a put below 4.0.** MEASURED 2026-09-20, not
  ported. run390 answers `drop lamp in box`, `drop lamp on table` and the
  `leave` spellings of both with a flat "You drop the lamp.", and the lamp
  lands on the floor: pre-4.0 drops() claims the line on the verb alone and
  never looks for a container clause, so the preposition is just part of
  the rest of the line. Scarier routes them to the put rows instead and
  answers "You put the lamp inside the box." / "You put the lamp onto the
  table.", which then shifts the next `take lamp` to "You take a lamp from
  the box.". p39SURF, `cmdfile_leaveput.txt`, lp39.txt. The friendlier
  behaviour is long-standing and no corpus row turns on it, so the pre-4.0
  put rows keep `drop` and `put down`; the `leave` port deliberately did
  not widen them.
- **run370 double matcher pass** (arlo `get out of bus`).
- **run380's event route to the task-ran flag.** MEASURED 2026-09-20, not
  ported. run380 clears `MemVar_44F12C` at the top of generaltasks (441A28)
  and sets it in tasks() 44D0BA whenever a task actually runs; the
  end-of-turn guard 4431B0 is `(MemVar_44F124 < 0) Or (CInt(MemVar_44F12C) =
  1)`, and the second disjunct is what stops the whole turn being replaced
  by `Which <term>.  <list>?`. An event reaches it: generaltasks calls
  characters() 443179 and events() 44317E BEFORE the guard, and checkevent
  43A753 sets 44F0B0 to the affected task's command and dispatches
  tasks(CByte(1)) at 43A762. run370 has no such flag -- its guard 43C8D3 is
  `If (MemVar_446140 < 0)` alone -- and its two probe files are
  byte-identical. p3xEVQ2 (`make_3738_eventflagprobe.py`: two hats both
  Short "hat", the adjective the last word of the Prefix, plus an immediate
  event that restarts every turn and runs a `zzev` task; each version built
  twice, once with TaskAffected 0):

  | line | run380 control | run380 + event | run370 control | run370 + event |
  |---|---|---|---|---|
  | `poke hat` | prompt | prompt | prompt | prompt |
  | `x hat` | prompt | "Nothing special." + ev | own examine prompt | same + ev |
  | `take hat` | prompt | "Take what?" + ev | "Take what?" | same + ev |
  | `put hat` | prompt | prompt | prompt | prompt |

  `poke hat` and `put hat` keep the prompt everywhere because they leave the
  buffer empty, so 443160 substitutes DontUnderstand and never reaches
  characters()/events() -- no tick, no flag. Gating
  `run_note_dispatched_task_ran()` down to 3.80 on its own makes p38EVQ2
  WORSE, 2/4 cells to 0/4, because suppressing the prompt uncovers three
  answers Scarier does not have: 3.8 examines' "Nothing special.", takes'
  "Take what?" (3.9 too -- run390 gives "Take what?  EVENT TASK RAN." where
  Scarier gives only the event text), and run380's therest, which says
  nothing at all for an ambiguous noun where Scarier says "I don't
  understand what you want me to do with the red hat." All three live in
  `lib_disambiguate_object_common()`'s `kept == 0` path, which returns -1
  with `*is_ambiguous` set so the handler prints nothing; only `examine_390`
  is exempted today. The flag belongs at 3.80; those three go first.
- **3.7 alias namesakes under an unknown verb.** In the p3xEVQ world where
  the Shorts are "red hat"/"blue hat" and the shared Alias is "hat",
  run370 answers `poke hat` with "I don't understand what you want me to do
  with the red hat." where Scarier prints the end-of-turn `Which hat.` 
  prompt -- a 3.70 co()-term split (Short vs Alias) not chased further
  (p37EVQ, Adrift_evq_p37EVQ.rtf, 2026-09-20).
- **3.7/3.8 Runner crashes ("Run-time error '9': Subscript out of
  range", transcript lost):** `put all in <nothing>`, `put all on
  <nothing>` and `put everything in zzz` on run370x and run380x; bare `eat`
  and `eat <character>` on run370 (run380 answers DontUnderstand). Scarier
  keeps its sane answer in each case (p37PUT/p38PUT, p37NPCAMB,
  2026-09-19); and a line starting `with ` whose previous typed line was
  BLANK on run380x (p38WITHPFX, `cmdfile_pwithpfx.txt`, 2026-09-20 -- the
  drive died at that command and the feed was re-cut as
  `cmdfile_pwithpfx2.txt`).
- **A blank previous line under the `with ` prepend.** `eee` /
  <Return> / `with www`, with a task spelled `with www`: run370 never
  prepends and matches it; run380 crashes (above); run390 prints garbage
  -- "Which pearl.  The gem or the rock?", a disambiguation prompt
  naming an object nothing on the line mentions; run400 answers "I don't
  understand.", because its joined line is " with www" with the empty
  history's separating space still on the front and no task matches
  that. Scarier joins the same way but its task matcher does not see the
  leading space, so it matches the task at every version from 3.80. Not
  ported: a blank line is the only way in, two of the three Runners
  answer nonsense, and the fix means making task matching sensitive to
  leading whitespace. p*WITHPFX, Adrift_219_wp390 / 220_wp400
  (2026-09-20).
- **3.90 spells a `%object%` command from a SNAPSHOT of the line, and tests
  it against the rewritten one.** Measured, not ported (p39TEXTSRC,
  Adrift_215_ts390.txt, 2026-09-20). checktask's walk searches
  MemVar_468224, the line as it stood at the end of the synonym pass
  (45F20F); everything generaltasks does to the line below that --
  "everything"->"all" 45F225, "slap"->"hit" 45F246,
  "except"/"apart from"->"but" 45F267/45F288, the `with ` history prepend
  45F2AF -- is invisible to it, while the equality at 44B0E2 and checkwild
  at 44B139 test the rewritten line. With objects named `hit` and `slap`,
  task `zog %object%` is refused by `zog slap` at 3.90 (the walk binds the
  slap, the test is against "zog hit") and taken at 3.80, which has one
  string. The fallback pair at 44ABFE/44AC98 would repair that, but its
  guard is `MemVar_4681A8 = &HFF`, the per-TURN referenced object cleared
  once at 45EC68, so the first task command on the line whose walk binds
  anything consumes it: `nurb except` with an object named `but` is refused
  at 3.90 because task 1's `blip %object%` fell back first. Scarier keeps one
  string -- the rewritten one, since 2026-09-20 at 3.90 too -- so its walk
  binds the `hit` and takes both lines. Porting this means carrying the
  snapshot and the per-turn flag through
  `run_pre400_substitute_references()`.
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

- **A library verb is matched anywhere in the line.** run400's input
  routine enters every one of its library handlers on `c(<word>)` -- the
  whole word, wherever it sits -- so a nonsense head changes nothing:
  `blorp take` is "Take what?", `blorp take coin` "You take the coin.",
  `blorp eat` "I don't understand what you are trying to eat.", `blorp sit`
  "You sit down on the ground." The handlers are put_drop_list 459DB4,
  get_outer 4582D8, wears 463C30, removes 4624B0, sitstand 46BCFC,
  openclose 476468, examines 471F94, give 48A985, whereis 4684E4,
  gotoplace 464E90, therest 489F4C and characters 480674, called from
  48A462/48A46D/48A48C/48A491/48A510/48A515/48A67B/48A985/48ACB0/48ACD7/
  48AFE4/48B56E. Three of the thirty-four words the probe types are NOT in
  any of them and stay "I don't understand." -- `search`, `wave` and
  `throw` (the last only outside dobattle). `blorp give` shows the two
  halves are separate: the "(to Nobody)" echo, then "Give what?". The
  object half needed nothing -- the 4.0 rows already score their noun over
  the whole line, so `take blorp coin` already worked. Ported as
  `run_hoist_verb_400()` in scrunner.cpp: hoist the one verb to the front
  and let the anchored table answer. It stands aside when the line's first
  word is already a table head (so `x light` stays an examine) and when the
  line names two of the verbs, which is not measured -- see the open lead.
  The put/drop pre-pass ahead of the tasks reads the same hoisted line,
  which is what makes `blorp drop coin` "You drop the coin." p4REW,
  `cmdfile_pcasc.txt`, run400x Adrift_251_casc40.txt (2026-09-20): all 36
  cells match, the 428 rows are unmoved and sweep_wine_breaks is identical
  either way (no corpus line types a nonsense head).
  Below 4.0 the same is true, but only five handlers were still anchored in
  Scarier -- everything else already matched its word anywhere
  (`run_therest_pre400()`, `run_therest_absent_370()`,
  `lib_sitstand_anywhere()`), which is why `blorp drink`, `blorp push`,
  `blorp sit` and `blorp read` already agreed. The five and their entry
  disjunctions, all `c()` tests: takes (run380 43D788, run370 435E28) `get`
  / `take` / `pick` -- `pick` only with no "from" in the line -- drops
  (438659 / 430475) `drop` / `put down` / `leave`, wears (432D5C / 42C533)
  `wear` / `put on`, removes (42FD4C / 4295FF) `remove` / `take off`, and
  examines (43C69D / 434E2A) `x` / `examine` / `look at` / `ex` / `exam` /
  `read`, with `look in` added at 3.80 and NO bare `look` at either. So
  `blorp take` is "Take what?", `blorp wear` "Wear what?" and `blorp
  examine` "Nothing special." run370x Adrift_250_casc37b.rtf, run380x
  Adrift_249_casc38.rtf, run390x Adrift_250_casc39.txt (2026-09-20); same
  `run_hoist_verb_line()`, same narrowing. Below 4.0 the hoist runs over
  the PRIORITY pass as well as the library cascade, because the take and
  drop rows live in `PRIORITY_COMMANDS`, not in the cascade. run370's own
  word for each of the five (command slots 10-14) is still open -- see the
  open lead.

- **Below 4.0 a take or a drop names its object over the WHOLE line too.**
  The noun half of the bullet above. Neither takes() nor drops() parses
  the words after its verb: each walks the object table in index order
  asking co() whether the object's Short or an Alias stands anywhere in
  the line -- takes() mode 1 (run390 454E1D, run380 43DFC3, run370
  4364E6), drops() mode 2 (run390 4458CF, run380 438889, run370 430689).
  So `blorp take coin` is "You pick up the coin.", `blorp drop coin` "You
  drop the coin.", and at 3.90 `drop zzz hat` / `take zzz hat` answer the
  same way where Scarier used to say "Drop what?" / "Take what?" (wear and
  remove already agreed, through `lib_absent_named_object_pre_390()`).
  p37REW/p38REW/p39REW with `cmdfile_pcasc.txt` (Adrift_250_casc37b.rtf,
  249_casc38.rtf, 250_casc39.txt, 2026-09-20) and p39WHAT under run390x
  (`Adrift_p39what2.txt`, `cmdfile_p39what2.txt`), 8/8. Every cell the
  cascade transcripts hold now agrees -- 34 at 3.70 and 35 at 3.80 (the
  .rtf drops the last command, as ever), 36 at 3.90 and 4.00 -- but for one
  `look` cell that differs only by Scarier's own auto-break in front of the
  exits list (`pf_buffer_answer_break`, corpus-wide and older than this).
  Beware a cell comparator written against these transcripts: the 3.9/4.0
  `.txt` echoes a command as `> cmd`, the `.rtf` as `cmd`, and a matcher
  that misses the prompt silently compares nothing at all. PORTED
  2026-09-21 as `lib_move_named_whole_line_pre400()` in sclibrar.cpp,
  called from `lib_take_multiple_common()` (mode 1) and
  `lib_drop_multiple_common()` (mode 2). Two deliberate narrowings, both
  in the comment there: it runs only from the parse-failure branch, so
  nothing the positional parse already answers can move, and only when
  exactly ONE object in the whole table answers co(). The Runner walks the
  rejected objects in the same loop and they write their own refusals --
  which of them speaks is unmeasured -- and requiring uniqueness only
  among the objects the caller's filter accepts moved `wrecked` (run380)
  turn 118 `drop room key`, so the whole table it is.

- **Below 4.0 a two-verb line is decided by the call order, never by word
  order.** The third half of the two bullets above. Since every one of the
  five handlers enters on a whole-word c() over the WHOLE line, `x get
  coin` satisfies takes() and examines() alike, and generaltasks calls them
  in ONE fixed order -- takes, drops, inventory, insides, tasks, wears,
  removes, then therest (run390 45F439-45F49E, run380 4421A3-442201,
  run370 43B958-43B98A). takes and drops sit inside `If CBool(...) Then
  GoTo` the turn tail, so one that ACTS claims the line; one that only
  writes a refusal does not, and the next handler overwrites it. wears and
  removes are plain `Call`s and can never claim at all. So the answer to a
  two-verb line depends only on where the object is: coin loose `x get
  coin` "You pick up the coin.", coin held `get x coin` "A gold coin.",
  coin held `wear take coin` "You've already got the coin!", coin loose
  `remove drop coin` "You don't have the coin!" and `remove wear coin` "You
  are not holding the coin.", hat loose `remove take hat` "You pick up the
  hat.", hat worn `take remove hat` "You remove the hat." and `drop take
  hat` "You drop the hat.". With NOTHING named the examine arm only fills
  an empty buffer, so `drop take` is "Take what?" and `examine wear` "Wear
  what?". `take off <loose thing>` is a take at 3.70 and 3.90 ("You pick up
  the coin."), and at 3.80 too by another road -- 441C61 rewrites take->get
  first, so `take off hat` is `get off hat` there and removes never enters.
  p3xREW with `cmdfile_p2verb.txt` (Adrift_251_2v37.rtf, 252_2v38.rtf,
  253_2v39.txt) and `cmdfile_p2verb2.txt` (253_2w37.rtf, 254_2w38.rtf,
  255_2w39.txt); the wearable cells needed a new probe,
  `make_twoverbprobe.py` -> p3xTWO, with `cmdfile_p2verb3.txt`
  (255_2x37.rtf, 256_2x38.rtf, 257_2x39.txt), all 2026-09-21. PORTED as
  `lib_two_verb_line_pre400()` (sclibrar.cpp), which re-spells the line
  around the handler the order leaves speaking; `run_hoist_verb_line()`
  asks it before its own scan, because the head may be a verb itself. The
  three feeds go 11/11/10 -> 4/6/4 mismatches, 428/428 and the Wine sweep
  byte-identical. Of the four leftovers three were single-verb wordings the
  probe turned up on the way, ported straight after in the entry below; the
  last is the one two-verb cell `x take off hat` worn, in the open list
  above. The feeds now stand at 1/0/1.

- **At 4.0 a two-verb line is decided by a DIFFERENT call order, and word
  order still never decides.** The same machine one version up, with
  another cast: takes 46FB8C and drops 46F118 are dead code at 4.0 (the
  call census on the drops annotation), so generaltasks' run is
  put_drop_list 459DB4 (48A462), get_outer 4582D8 (48A46D), tasks 44CCE0
  (48A481), wears 463C30 (48A48C), removes 4624B0 (48A491), examines
  471F94 (48A67B), therest 489F4C (48AFE4), characters 480674 (48B56E).
  put_drop_list, get_outer, tasks and examines sit inside `If
  from_stack_1 Then GoTo loc_48B4E3` and claim; wears and removes are
  plain `Call`s as below 4.0, but unlike below 4.0 examines runs BELOW
  them and overwrites an already-written buffer, while therest runs only
  on an empty one (the 48AFE1 test is `MemVar_4941B0 = ""`). wears' and
  removes' own refusals are additionally guarded by an empty buffer
  (463B8B in front of 463BBC). So the order that answers a line is
  **put/drop, take, examine, wear, remove, therest**, and inside therest
  the LAST arm the line names wins, its cascade being one `If c(...)`
  after another each overwriting the one before. Coin loose unless said:
  `take drop coin` and `drop take coin` are both "You are not holding the
  coin.", `take drop` is "Drop what?"; held `examine take coin`, `wear
  take coin`, `push take coin` and `x get coin` are all the take;
  `examine drop coin`, `remove drop coin`, `push drop coin` the drop;
  `push examine coin`, `wear examine` and `examine wear` the examine
  ("You see no such thing.", not "Wear what?", because examines writes
  unguarded); `remove wear coin` and `wear remove coin` the wear; `push
  pull coin` and `pull push coin` both "You pull the coin, but nothing
  happens.", `kick hit coin` and `hit kick coin` both the hit. A span
  that carries two handlers is read by each of them its own way -- `x
  take off hat` with the hat WORN is "You are already carrying the hat.",
  because get_outer reads only the `take` out of removes' `take off`.
  p4REW with `cmdfile_p2verb.txt` (Adrift_254_2v40.txt) and p4TWO with
  `cmdfile_p2verb3.txt` (Adrift_258_2x40.txt), 2026-09-21. PORTED as
  `run_two_verb_line_400()` (scrunner.cpp), asked from the top of
  `run_hoist_verb_line()` beside its pre-4.0 twin and above the
  HOIST_HEADS_400 bail, because at 4.0 too the head may be a verb itself;
  it hoists the winning handler's own spelling to the front and leaves the
  rest of the line, the other verb word included, exactly as it stands
  (`wear examine` must become `examine wear` and not a bare `examine`, or
  examines' whole-line bare-verb exit at 471340 answers instead). Narrow
  on purpose: a list line (`all`, `and`), a lone span, a handler whose place
  is unmeasured (give, gotoplace, characters, dobattle) and a clauseless
  `put` -- whose 46DC34 branch does not claim either -- are all left alone.
  openclose and whereis have since been measured and ported in their own
  entry below, as passes of their own rather than as rewrites here. The two feeds go 19/8 -> 0/0
  (`cmdfile_p2verb.txt`'s last two cells are unusable: the drive echoed
  `ii` for feed line 52), 428/428 and both Wine sweeps byte-identical.

- **Below 4.0 a therest verb never outranks the five handlers, wherever it
  stands.** The other side of the same coin: therest() is the LAST thing
  generaltasks calls (run390 45D465, below 45F439's run of handler calls),
  so a line naming any of takes/drops/wears/removes/examines is answered by
  that handler and the verb cascade never sees it -- even when the therest
  word OPENS the line and the handler's word comes after it. With the coin
  in hand `push take coin` is "You've already got a coin!"; with it on the
  floor `push drop coin` is "You don't have a coin!"; and `push examine
  coin` is the coin's description at 3.7, 3.8 AND 3.9, where Scarier used
  to answer 3.9's own therest examine arm, "Nothing special.". run370's
  absent-object refusal already knew the rule -- `run_therest_absent_370`
  skips a line holding any earlier handler's word -- it just never reached
  the verb arms below it. p3xREW `cmdfile_p2verb.txt` cells 31, 35 and 39
  (Adrift_251_2v37.rtf, 252_2v38.rtf, 253_2v39.txt, 2026-09-21). PORTED in
  `run_hoist_verb_line()`: below 4.0 a head that is one of therest's own
  arms (`run_therest_arm_at()`, over the tables the winner scan already
  had) no longer stops the hoist, and the arm word goes with the head
  rather than into the object clause, because the handler's c() walk never
  looked at it -- 3.9's drops row wants `drop coin`, not `drop push coin`.
  The three feeds go 4/6/4 -> 1/1/4 mismatches, 428/428 and the Wine sweep
  byte-identical. What is left: the `x take off hat` cell in the open list
  above (3.7 and 3.9), and 3.9's `wear examine` / `examine wear` with
  NOTHING named, which is
  "Nothing special." there and "Wear what?" at 3.7 and 3.8, so 3.9's
  examines arm fills an empty buffer where its elders left the question
  standing.

- **openclose ACTS on every line and claims none of them; whereis speaks
  only where nothing above it wrote.** The two handlers the two-verb ports
  left alone, measured together because the same machine explains both.
  openclose (run400 Proc_19_3_476468, called unconditionally at 48A515;
  run390 45F512) is a plain `Call`, so it can never claim -- but its acting
  arms write the buffer UNCONDITIONALLY: "You open the " (475822), " is
  already open!" (47592D), " as it is locked!" (4757A5) and " not carrying "
  (4758D3) all assign MemVar_4941B0 outright, while only " can't open "
  (4756EA) and " can't see " (475952) are guarded by `= vbNullString`. So
  the ACT survives whatever comes after it and the message does not: a
  handler ABOVE openclose (takes, drops, tasks) claims the line and the box
  never opens, a handler BELOW it (the typed look, examines, score, whereis,
  characters) opens the box and then overwrites its sentence, and therest --
  whose arms are all `If msg = "" Then` -- leaves it standing. Its entry
  words come from c() over the whole line (`open` 475699, `close` 4759C3,
  `lock` 475D71, `unlock` 47612F, `with` 475C73), so the word may stand
  anywhere. With the box shut and held: `x open box`, `open examine box`,
  `open look at box` and `open read box` all print the description (or the
  read refusal) of an OPEN box at every version -- it opened first --
  `push open box` is "You open the box." because therest cannot overwrite,
  `open where is box` is "You are carrying the box!" with the box open
  behind it, `open take box` is a take and the box stays shut, `wear open
  hat` is the wear and `open ask bob about hat` is Bob's answer. The one
  cell the versions split on is `open x box`: pre-4.0 it is the open box's
  description, at 4.0 "You open the box.", because 4.0's examines anchors
  `x`/`ex`/`exam` to the HEAD (Proc_21_37_447B18) where the older ones take
  them with c() anywhere -- the same split "A library verb is matched
  anywhere in the line" already records. whereis (Proc_19_33_4684E4, entered
  on c("where")/c("find")/c("locate") at 467CE5-467D03) sits below examines
  and above therest and behaves like any lower row: `push where is coin` is
  "The coin is lit room." and `x where is coin` the coin's description.
  `make_orderprobe.py` -> p37ORD/p38ORD/p39ORD/p4ORD with
  `cmdfile_p2ord2.txt` (Adrift_263_3o37.rtf, 264_3o38.rtf, 265_3o39.txt,
  266_3o40.txt, 2026-09-21), 24/24/23/18 mismatches -> 4/4/3/3. PORTED as
  `lib_openclose_anywhere()` and `lib_whereis_anywhere()` (sclibrar.cpp),
  asked from `run_all_commands()` right after the sitstand pass through the
  shared `run_line_for_anywhere` re-entry. openclose reads the line as the
  player TYPED it, not the dispatch line, because below 4.0 the two-verb
  rewrite above it has already dropped the losing open word; what it hands
  back to the row below is the dispatch line, cut only where that word is
  still in it. It reads `open` and `close` alone: `lock`/`unlock` are left
  out on purpose, since `pick lock` is a task command in four walkthroughs.
  428/428 and both Wine sweeps byte-identical. What the probe leaves open is
  one family, `take ask bob about hat` -- characters() overwriting takes()
  -- which every version answers differently ("Bob says, 'That is a fine
  hat.'" at 4.0, "I don't think Bob would appreciate being handled." at 3.9,
  "Bob is not carrying the hat!" at 3.8 and the same handled line with an
  EMPTY name at 3.7); it is in the open list above. gotoplace and dobattle
  stay unmeasured: drive.exe cannot type a `go to` line at any Runner (error
  70 below 4.0, SendKeys glue at 4.0) and the probe world has no battle.

- **At 3.70 insides() runs below wears(); from 3.80 it runs above, so a
  put line is never a wear.** All three pre-4.0 Runners enter insides()'
  put branch on the same test -- `c("put")` with `c("inside")`, `c("into")`,
  `c("in")` or `c("on")` (run370 4399CD, run380 445746, run390 460EDC) --
  but they call insides() from different places. run370's generaltasks
  calls it at the BOTTOM, below tasks(0), wears(), removes(), the hints and
  even the screen clear, and as a plain `Call` that can never claim
  (43BA0A). 3.80 hoisted it above tasks(0) and made it claiming, `If
  CBool(insides()) Then GoTo` (4421DA), and 3.90 kept it there (45F471).
  So `put on hat` is wears()' line at 3.70 -- its entry takes `c("put on")`
  outright (42C533) -- and answers "You put on a hat." held, "You are
  already wearing a hat" worn, "You are not holding a hat." on the floor;
  at 3.80 and 3.90 insides() gets there first, the line co() names fewer
  than twice, and all three are "You can't do that!" (445A2A / 461646)
  with no wear attempted. 3.90 answers the trailing spelling `put hat on`
  "Put the hat onto what?" instead, its target pass taking only a name
  that stands after `InStr(line, "on")` (461000). 4.00 gave the wear back:
  `put on hat` wears a held hat and is "You are already wearing the hat!"
  once it is on. p37TWO/p38TWO/p39TWO with `cmdfile_p2puton.txt`
  (Adrift_257_2y37.rtf, 258_2y38.rtf, 259_2y39.txt, 2026-09-21), 0/8/9
  mismatches -> 0/0/0. PORTED 2026-09-21: `lib_wear_is_put_line_380()`
  declines `lib_cmd_wear_multiple` and `lib_cmd_wear_what` from 3.80 up,
  and the clauseless spellings that no put row could match get two new
  STANDARD_PUT_COMMANDS rows on `lib_cmd_put_no_clause_pre400()`. This
  closes the `put on <loose thing>` leftover from the therest entry above.
  428/428 and the Wine sweep byte-identical.

- **A put refusal silences the wear only where it CLAIMS, and from 3.90 one
  of them does not.** The entry above measured the put line with the object
  LOOSE, which is the one state in which wears() has nothing to do; put the
  hat in the player's hands and 3.90 and 4.00 both hand the line back to
  it. wears()' two acting arms -- the put-on move and its report (run400
  463965/4639F2), and "already wearing" (run390 43CF8B, run400
  4638DE/4638FE) -- write their message unguarded and OVERWRITE whatever
  stands in the buffer, while both of its refusals are guarded by an EMPTY
  buffer: run390 43D1EF `If var_18C(22) <> 0 And MemVar_468154 = "" Then`
  in front of " not holding " (43D220) and run400 463B8B the same test in
  front of 463BBC, with " can't wear " behind a buffer that is empty or
  still holds that very refusal (run390 43D188-43D1A2, run400
  463AC6-463B3F). So an unclaimed put refusal survives exactly when the
  wear cannot act.
  At 4.00 put_drop_list's "Where do you want to put <the X>?" never claims
  (46DC34-46DD2C sets 494281 and falls out), so EITHER spelling is a wear
  there. At 3.90 the spelling decides, because insides()' target pass takes
  only a name standing after `InStr(line, "on")` (461000): `put on hat`
  hands it the hat as the CONTAINER, the line names fewer than twice, and
  the "You can't do that!" arm (461646) claims; `put hat on` leaves it no
  target and the object's own "Put the hat onto what?" (461754) does not --
  which is also the only one of the two spellings wears() would enter on
  its third clause, `c("put") And Right(line, 2) = "on"` (43CD0B-43CD29).
  3.80 claims either way (4421DA) and 3.70 never gets that far. With the
  hat held:

        line          3.70            3.80             3.90/4.00
        put hat on    put on a hat    can't do that!   put on the hat
        put on hat    put on a hat    can't do that!   3.90 can't do
                                                       that!, 4.00 wear

  and worn it is "You are already wearing the hat" in the same cells. The
  coin -- held, not wearable -- keeps the put refusal everywhere: `put coin
  on` is "Put the coin onto what?" at 3.90 and "Where do you want to put
  the coin?" at 4.00. p3xTWO/p4TWO with the new `cmdfile_p2puton2.txt`
  (Adrift_259_2z37.rtf, 260_2z38.rtf, 261_2z39.txt, 262_2z40.txt,
  2026-09-21), 0/0/5/1 mismatches, and `cmdfile_p2puton.txt` re-read for
  4.00 (260_2y40.txt), 5 more. PORTED 2026-09-21 in
  `lib_wear_would_act_390()` -- co() over the whole line below 4.0, the
  463640 scorer at 4.0, then the wear and remove filters -- which
  `lib_wear_yields_to_put_390()` puts in front of `lib_cmd_wear_multiple`
  and the zeroed-"on"-split question in `lib_cmd_put_container_400()`. All
  eight feeds identical on every turn, 428/428, and both Wine sweeps
  byte-identical.

- **"Already wearing" is never contracted, and below 4.0 the wear arm
  leaves the sentence open.** Every Runner builds this message the same
  way, `<pronoun> & " " & <are> & " already wearing " & <name>` -- takes at
  run390 454F31, run380 43E0A2, run400 47BE82, all of them ending "!"; and
  wears at run390 43CF8B, run380 432FCB, run370 42C7B6, none of which adds
  a terminator at all, where run400 4638FE alone appends "!". So `wear hat`
  with the hat on is "You are already wearing a hat" with nothing after it
  below 4.0. The name follows the usual split, the plain Prefix & " " &
  Short concatenation below 3.90 and the definite printer from 3.90 on, so
  3.70 and 3.80 say "a hat" where 3.90 says "the hat". Scarier had
  "You're already wearing" -- a form that appears in none of the four
  listings -- a full stop on the wear arm, and the definite name
  everywhere. The third cell is 3.70's takes(), which has NO worn arm:
  it tests position 0 (436561, "'ve already got") and the room constant
  (436585, the pick-up branch) and nothing else, so a worn object falls
  through to the same " can't see <raw> from here!" arm an absent one gets
  (436909/43696A) -- `take hat` with the hat worn is "You can't see a hat
  from here!" there, against "You are already wearing a hat!" at 3.80.
  Measured on p37TWO/p38TWO/p39TWO with `cmdfile_p2verb3.txt` cells 7, 9,
  22 and 29 (Adrift_255_2x37.rtf, 256_2x38.rtf, 257_2x39.txt, 2026-09-21).
  PORTED in `lib_take_backend_common()` and `lib_wear_backend()`;
  `lib_print_object_list()` now takes '\0' for an arm that leaves the
  sentence open. The three feeds go 4/6/4 -> 1/0/1 mismatches, 428/428 and
  the Wine sweep byte-identical.

- **The Runner rewrites the typed line before anything looks at it, at
  every version, and 3.90/4.00 do it by SUBSTRING.** Below 3.90 the
  rewrites are `change()`, a loop over the whole-word matcher `c()`
  (run370 43B41F/43B430: everything->all, slap->hit; run380 441C3F-441C72:
  those two plus take->get and except->but). From 3.90 they are VB
  `Replace(line, old, new, 1, -1, 0)`, a plain substring replace with no
  word boundary at all (run390 45F225-45F288: everything->all, slap->hit,
  except->but, "apart from"->but; no take->get), and at 4.00 every literal
  carries its own spaces (run400 48A30F-48A372: `" everything "`->`" all "`,
  `" slap "`->`" hit "`, and the exclusion word the OTHER WAY, `" but "`->
  `" except "`, `" apart from "`->`" except "`). So an object named
  `exception` cannot be examined at all from 3.90 -- `x exception` reaches
  the parser as `x bution` -- while 4.00 rewrites nothing at the start or
  the end of a line. The line is already lower-cased by here, which is why
  `Replace`'s binary compare still catches a typed `SLAP`. p37REW/p38REW/
  p39REW/p4REW, `cmdfile_prew2.txt`, Adrift_246_rew37b.rtf /
  247_rew38b.rtf / 248_rew39b.txt / 249_rew40b.txt (2026-09-20);
  `make_rewriteprobe.py` carries the 124-cell table, `scprintf.cpp`
  BUILTIN[] and `pf_rewrite_substring()` the port.
- **Below 3.90 an empty-handed drop line is the only one a task can
  claim.** drops() takes every line saying drop / put down / leave (run380
  438659, run370 430475), and generaltasks branches on the handler's
  *handled* flag, not on the message buffer. On the one-object arm that flag
  is only set inside the held-or-worn test at 438E94 (430C55), so with
  nothing carried or worn the line is handed straight on to the ordinary
  tasks(0) pass and the "Drop what?" just written at 438FE6 (430DCF) is
  overwritten by whatever answers next. Carrying anything at all, drops()
  claims, and the line's one chance at a task is 438F4F (430D38), behind
  "field 22 is 0 or &H9C" AND "co() finds this object's name in the line" --
  name nothing held and the matched task never runs, while checktask having
  matched (4386F5 / 430511) has already shut the whole library print loop at
  438AD5 (4308B0), so the answer is the bare "Drop what?". The gate is
  "something the line names is held or worn", not "the task's own object is
  held", and a worn one counts. 3.90 keeps the same walk but runs a matching
  task at the checktask gate above it (44562A), so a task fires with empty
  hands there; its own "Drop what?" (445F0B) is still live for `drop qqq`.
  4.0's drops() still tests `"leave"` (46F15E-46F197), but nothing routes a
  `leave` line to it -- see the `leave` entry below. p3xDROPGATE and p3xEMPTYHAND
  (`make_3738_dropgateprobe.py`), Adrift_dropgate37/38.rtf,
  Adrift_dropgate39.txt, eh37 / eh38 / eh237 / eh238 / eh39d.txt, and
  Alice's Restaurant's `leave station` empty-handed on run370 (arlo37.rtf).
  PORTED 2026-09-20 as `lib_drop_what_pre390()`, called from run_all_commands
  just above the task passes.
- **Below 4.0 `leave` is a third spelling of `drop`; at 4.0 it is not a
  verb at all.** Every pre-4.0 drops() opens on `c("drop") Or c("put down")
  Or c("leave") Or (c("put") And c("down") And <flag> = 0)` -- run370
  430475, run380 438659, run390 44554E-4455B3 -- so a `leave` line enters
  the drop handler and takes whichever arm the rest of the line picks:
  `leave bean` "You drop the bean.", `leave coin` (loose in the room) "You
  don't have a coin!" ("the coin!" at 3.9), `leave qqq` and bare `leave`
  "Drop what?", `leave all` / `leave X and Y` / `leave all except X` the
  drop answers, and `leave everything` empty-handed "You are not carrying
  anything.". `leave north` is "Drop what?" *and the player does not move*,
  because generaltasks calls drops() well above moves() (run370 43B958 vs
  43BAEB) and a claimed line jumps to the turn tail (43C885). run400's
  drops() still holds the word (46F15E-46F197), but nothing routes a
  `leave` line there: run400 answers the same feed "I don't understand." /
  "I don't understand what you want me to do with the bean." throughout,
  which is what Scarier already did at every version. p37/p38/p39/p4DROPGATE
  (`make_3738_dropgateprobe.py`, which now builds the 4.0 world too),
  `cmdfile_leavegate.txt` and `cmdfile_leaveall.txt`, lg37.rtf / lg38.rtf /
  lg39.txt / lg40.txt / la37.rtf (2026-09-20). PORTED 2026-09-20 as
  version-gated `lib_cmd_leave_*_pre400()` rows beside the drop rows in
  PRIORITY_COMMANDS, STANDARD_ABOVE_REFUSAL_COMMANDS and
  STANDARD_FALLBACK_COMMANDS, plus `"leave"` in `lib_typed_verb()`'s
  DROP_FORMS. Only the drop arms take the spelling; the put rows keep
  `drop` and `put down` alone -- see "`drop X in Y` is not a put below 4.0"
  under "Deliberate deviations".
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
- **The `with ` history prepend `[3.8+]`.** A line that starts with
  `with ` is rewritten as `<the previous typed line> <this line>` before
  anything tests it, and the rewritten line reaches the task matcher.
  run370 has no such arm: `aaa` / `with zzz` matches a task spelled
  `with zzz` there and a task spelled `aaa with zzz` at 3.80, 3.90 and
  4.00. It is the previous TYPED line, not the previous element -- one
  line `ccc, ddd` then `with xxx` matches `ccc, ddd with xxx` at every
  version that prepends, although 3.90 splits that line into two elements
  and run390's history counts elements (45EC5B). 3.90 does prepend,
  which the p*TEXTSRC cells had made doubtful; those cells were the
  %object% snapshot rule below and the "With what?" prefix (see "3.90's
  'With what?' leaves a prefix").
  p*WITHPFX (`make_withprefixprobe.py`, `cmdfile_pwithpfx.txt`,
  Adrift_217_wp370 / 219_wq380 / 219_wp390 / 220_wp400, 2026-09-20)
- **The " with " clause runs at every version, not just 3.9+.** run370
  and run380 split a line holding " with " exactly as run390 does -- the
  instrument is the last object named after the split, present if any is
  -- and answer "With what?" for one that is not present, "<You> don't
  have the X." for a dynamic one not held, and put " with <the X>" before
  the full stop of the can't-do and nothing-happens arms otherwise.
  `cut`, `push`, `fix`/`repair`/`mend`, `lock`/`unlock`, `turn` and
  `clear` all carry the suffix at 3.70 and 3.80 (`smell` does not: it
  keeps "The rock smells normal."). `[<4.0]` p*WITHPFX
  (`lib_with_clause_400`'s version gate dropped, 2026-09-20)
- **3.7 makes the " with " split above its absent-object test.** `cut
  rock with pearl` with the pearl in another room is "With what?", not
  "You can't see the pearl." `[3.7]` p37WITHPFX
  (`lib_with_clause_claims`, 2026-09-20)
- **`break` takes the clause's refusals and none of its suffix.** `break
  rock with gem` is "<You> don't have the gem." while the gem is loose
  and "<You> might need the rock." once it is held -- no " with the gem"
  at any version. The arm ends in an exclamation mark below 3.90 ("You
  might need the rock!"), bare or with an instrument. p*WITHPFX feeds 4
  and 8, Adrift_222_ws370..225_ws400 and Adrift_224_wx370..227_wx400
  (`lib_cmd_break_object`, 2026-09-20)
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
    checkverb prefix carrying `push` into the split element. The library's
    comma-as-space rule does NOT reach checktask at any pre-4.0 version,
    and it does not reach it from the TASK side either: `say hello world`
    misses a `say hello, world` task in all three Runners, while the
    comma-for-comma line matches it at 3.7/3.8 -- and at 3.9 nothing can,
    the line splitting before the match (`say hello, world` is the say
    library then "I don't understand."). A `*` command and a `%object%`
    command are no softer: `rub, coin` and `rub,coin` miss `rub *` at
    3.7/3.8 (catch-all, then DontUnderstand for the glued cell), and
    `poke, stone` / `poke,stone` miss `poke %object%` the same way; 3.9
    runs `rub *` on the split's first element and answers the second from
    the catch-all. `[<4.0]` p37TASK/p38TASK/p39TASK,
    `cmdfile_ptaskcomma.txt` -> Adrift_176_ptaskc_37.rtf /
    Adrift_177_ptaskc_38.rtf / Adrift_179_ptaskc_39.txt; all cells already
    matched, the `say` lines apart, those being Int(Rnd*5|6) draws
    (`lib_what`, `run_match_task_commands`, 2026-09-19, closed 2026-09-20)
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
  - **A 3.9 task that RAN and printed nothing claims the line too**, and
    the empty buffer comes out as the game's DontUnderstand text: tasks()
    returns the task and generaltasks skips everything below the
    dispatcher, so the library verb that would have answered never runs.
    The line is administrative (ported 2026-09-19); the claim followed on
    2026-09-20 (`silent_task_390` guards the library block in
    run_all_commands()). It costs six goldens and no win: alexis and
    alexis_worn_cube T99 `open chest`, everything T38 `read diary`,
    lifesimulation T6 `turn off tv` and life `piss` change one line of
    text, and the_hangover T34 also keeps the filing cabinet shut, so
    `take approval form` is "Take what?" -- all six rows went identical to
    their Runner transcripts. 4.0 falls through to the library instead.
    3.7/3.8 are unmeasured and left alone. `[3.9]` p39done
    (make_39_doneprobe.py, Adrift_18/19), alexis, everything,
    lifesimulation, life, the_hangover
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
  the handlers' own rows) stores a line, and generaltasks (4601A5)
  prepends it to the next line nothing answers: `push` / `stone` pushes
  the stone, `give` / `coin` asks "Give the coin to who?". The prefix
  lives one line. Bare give stores its completed "give to nobody" line, so
  the rerun is not echoed again. checkverb's own arms (42A4F4) store only
  a line that IS the verb, so `push zzz` / `stone` stores nothing -- but
  the four object handlers store the line AS TYPED: takes 455890/45589A,
  drops 445F0B/445F15, wears 43D27F/43D289 and removes 439FBF/439FC9 each
  end `If msg = "" Then msg = "<Verb> what?" : MemVar_4681D0 = the line`.
  So `wear zzz` / `hat` wears the hat, `remove zzz` / `hat` takes it off,
  `drop zzz` / `hat` drops it and `take zzz` / `hat` picks it up, while
  `wear zzz` / `wield zzz` answers "Wear what?" a second time and
  `drop zzz` / `look` / `hat` is the catch-all. (The pre-prune note's
  claim that run390's removes sets no prefix is wrong.) therest's give arm
  is a FIFTH handler of that kind: "Give what?" (45D70B) is followed at
  45D712 by the same unconditional `MemVar_4681D0 = MemVar_468118`. What it
  stores is the line the bare-give completion has already finished, so
  `blorp give` stores "blorp give to nobody" and the next line runs on from
  there -- it has its "to", is not completed again, and prints no second
  "(to Nobody)". That is the whole of run390's `blorp give` / `blorp put`
  pair, "Give what?" twice with only one echo: `put` names no route of its
  own, and 45D70B is the only "Give what?" in the run390 P-code. `[3.9]`
  p39TASK, p39WHAT (`make_39_whatprobe.py`, `cmdfile_p39what.txt`), the
  give pair on p39REW with `cmdfile_pcasc.txt` (Adrift_250_casc39.txt)
  (`lib_what`, `run_get_line_input`, 2026-09-20; the give arm 2026-09-21)
- **The 4.0 drop/take setter branches are DEAD CODE.** run400's takes
  (Proc_19_6_47C83C, body 47B60C-47C83B = P32Dasm `mdlSpreadTheLoad.Sub_20_7`)
  and drops (Proc_19_7_46FB8C, body 46F118-46FB8B = `Sub_20_8`) have ZERO
  call sites anywhere in the run400 P-code, where wears (`Sub_20_9`,
  463C30) and removes (`Sub_20_10`, 4624B0) have one each. Their prefix
  setters 47C7F1 and 46FB7D are therefore unreachable and there is nothing
  to port; `drop zzz` / `take zzz` reach the live sites instead
  (name_object 46E179, get_piece 473A25) and store no prefix, which is what
  ptbad measured. The drops-only string " would you like to drop.  "
  (46F583) appears in no transcript. Census by xref table plus a raw P-code
  call count, 2026-09-20; several annotations still credit these two
  procedures with behaviour reached elsewhere. `[4.0]` (no code change)
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
- **3.90's "With what?" leaves a prefix, and only 3.90's.** Both of
  therest's "With what?" answers -- the two-object split's (45D1A0) and the
  with-arm's (45D3E0) -- store `Left(line, InStr(line, "with") + 4)`, the
  line cut just past the word with the instrument half thrown away. The
  NEXT line, if nothing understands it on its own (DontUnderstand or the
  object catch-all), is run again as `<prefix><line>` and through
  **therest only**: the task matcher never sees the joined line, so with
  `fff with ggg` wired as a task, `fff` / `with zzz` / `ggg` is "With
  what?" twice and the task never fires. A line that anything answers --
  therest itself (`cut rock`), a bare verb ("Push what?"), `i`, `look`, a
  task (`probe`) -- spends the prefix, but a retry that ends in "With
  what?" stores it again from the line it just ran, and since the cut is at
  the FIRST "with" that is the same string, which is how `hhh gem` /
  `with zzz` / `ggg` / `rock` still reaches the rock. Inside the retry the
  ordinary 3.9 rules run, and the split's claim counts the objects the
  WHOLE line names, not one per half: `fff with gem rock` is "You don't
  have the rock." although its head names nothing. `[3.90]` p39WITHPFX
  (`make_withprefixprobe.py`, `cmdfile_pwithpfx3.txt`/`5`/`6`/`7`,
  Adrift_222_wr390, 224_wt390, 224_wu390, 224_wv390;
  `lib_with_prefix_390_note`, `lib_with_clause_390`, 2026-09-20)
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
  `[4.0]` p4AND (`lib_take_whole_line_400`, `lib_take_resolve_400_string`,
  `lib_drop_named_400`, `lib_input_contains_word_400`, 2026-09-19)
- **`get X and Y` with neither present:** "There is nothing worth taking
  here." with no per-object refusal; one present object takes it alone.
  `[4.0]` p4AND (`lib_cmd_take_absent`, 2026-09-19)
- **`x coin and a hat`:** referencedob's prefix-word pass counts "a" for
  both objects, the scores tie, and examine prints "Sorry, I'm not sure
  which object you're referring to." as a turn; `x coin and the hat`
  examines the coin. `[4.0]` p4AND (`lib_examine_referencedob_400`,
  `lib_disambiguate_object`, 2026-09-19)
- **No pre-4.0 Runner rebuilds the line from the object's Prefix.** After
  the library has moved something it offers the tasks a second, canonical
  spelling at 4.0 -- the definite form, `put the bean in the jar`. Below
  4.0 there is no such line at all, in any family: run370, run380 and
  run390 all answer the library's own wording to `take pebble` against a
  live task `take a pebble`, to `get stone` against `get a stone`, to `put
  bean in jar` against `put a bean in a jar` and to `drop coin` against
  `drop a coin`, with every one of those tasks printing its CompleteText
  the moment its own spelling is typed. The crossed pairs (`get pebble`,
  `take stone`) miss too, so it is not a canonical-verb rebuild either,
  and `put down coin` misses `drop a coin`. Scarier keeps only the
  prefix-less retry pre-4.0: there the typed line has already been past
  the tasks, so that retry is how an ALIAS reaches one, not a second
  spelling of the noun the player used. `[<4.0]` p37PRETRY / p38PRETRY /
  p39PRETRY (`make_3738_pretryprobe.py`, `make_39_pretryprobe.py`,
  `cmdfile_pfx39.txt`, `cmdfile_pretry3738.txt`/`b`, Adrift_127,
  Adrift_pretry39b, Adrift_pretry3{7,8}{,b}) (`no_prefixed_retry` in
  `lib_try_game_command_common`, 2026-09-20). Closing it also closed the
  last row of the put39 probe: Scarier now matches Adrift_88 on all 12
  lines, where the bean-and-jar turn used to be the one open pre-4.0
  divergence in that table. Corpus 428/428 before and after, so the retry
  was dead code below 4.0 in practice.

### Nouns, scope and the seen model

- **Pre-4.0 never asks "Please be more clear".** That string is in NONE of
  run370.exe, run380.exe, run390.exe or run400.exe (ASCII and UTF-16LE
  searched, 2026-09-20): it is SCARE's own invention, and every pre-4.0
  line that named several objects used to get it. What the Runners really
  do, measured on p*OPENW.taf (a gem, a rock, a static slab, a closed
  chest, the gem held) is settle the line silently, each handler its own
  way -- see the three entries below. `[<4.0]` p*OPENW/p*OPENA
  (`lib_disambiguate_object_common`, 2026-09-20)
- **Pre-4.0 open over several nouns.** openclose() walks the objects the
  line names and acts on every OPENABLE one, the last by index overwriting
  the message: `open rock gem chest` is "You open the chest." at 3.70, 3.80
  and 3.90 alike, and with two closed containers on the line (p*OPENT: a
  box at index 0 and a chest at index 3) `open box gem chest` is "You open
  the chest." with the box left open as well -- so 3.7's every-namesake
  loop is not 3.7's alone, all three versions run it. The test is
  "openable at all", not "in the state the verb wants": with the box
  already open, `open box gem`, `open gem box`, `open gem chest` and `open
  chest box` are all "The <box|chest> is already open!" at every pre-4.0
  version, the openable object winning the line and the handler's own
  already-open wording answering for it. With none openable,
  3.70 has no refusal of its own and the line falls to therest's can't-do
  tail, which names the first object by **word position** and ends in a
  full stop (`open rock gem` -> the rock, `open gem rock` -> the gem,
  `open slab rock` -> the slab); 3.80 gave openclose its own refusal, which
  names the lowest object **index** and ends in a bang, so the same three
  lines are "the gem!", "the gem!" and "the rock!". `[<4.0]`
  p*OPENW/p*OPENT (`lib_disambiguate_object_common`,
  `lib_first_named_pre400`, `lib_cmd_open_object`, 2026-09-20)
- **Pre-4.0 close over several nouns.** Same walk and the same openable
  test (`close chest gem` closes the chest, `close box gem` the box), but
  `close` got no refusal of its own
  before 4.0, so at 3.70, 3.80 AND 3.90 a line with nothing closable falls
  to therest: first name by word position, full stop, and the two-object
  split's " with <the instrument>" suffix kept -- `close rock gem` is "You
  can't close the rock.", `close rock with slab` "You can't close the rock
  with the slab.". 3.70's `open` keeps that suffix too (`open rock with
  slab`); 3.80/3.90's own open refusal drops it. A static instrument falls
  through to the suffix, which settles the "Don't be daft!" cell
  `lib_with_clause_390()` had only read off the listing: 3.9 has no such
  line. `[<4.0]` p*OPENW/p*OPENT (`lib_cant_do_suffix_pre400`, 2026-09-20)
- **Pre-4.0 take/drop over several namesakes.** takes() (run370 436280)
  and drops() (430DDC) walk a crowd the way openclose() does: every
  survivor moves and the last by index overwrites the message. `take red
  pin`, over pins Prefixed "old red" and "new red", is "You pick up new
  red pin." at 3.70 AND 3.80 with `i` listing both, and `drop red pin`
  then drops both; 3.90 keeps only the FIRST by index, so the same line is
  "You pick up old red pin." and `i` lists it alone, and 4.00 asks
  instead ("Which pin.  Old red pin or new red pin?", and `drop red pin`
  "It is not clear which pin you are referring to."). What survives is the
  Prefix contest, and which crowd runs it is the version split: 3.80/3.90
  have co() under every handler, so the crowd is the TERM's present
  namesakes, while 3.70 has no co() at all and its crowd is the objects
  sharing a **Short** the line names -- a rock merely ALIASED "gem" beside
  a gem is no crowd to it, and `take gem` takes both with the rock
  speaking (p37OPENA). takes() walks what is loose and drops() what is
  held, so a namesake in the wrong place is not in the crowd either: two
  orbs on the floor answer `drop orb` with drops' ordinary "You don't have
  a orb!". With the crowd formed and nothing keeping its Prefix word the
  turn is takes'/drops' own "Take what?" / "Drop what?" (the "Which ...
  would you like to take" strings at 430866 are dead code), and the line
  is answered, so no end-of-turn co() question follows it. `[<4.0]`
  p*TAKEP/p*TAKEQ (`make_takeprefixprobe.py`, `cmdfile_ptakepfx.txt`,
  `cmdfile_ptakeq.txt`, Adrift_238_pc370 .. 241_pc400 and 240_pd370 ..
  243_pd400; `lib_disambiguate_object_common`,
  `lib_namesake_crowded_pre380`, `lib_what`, 2026-09-20)
- **Pre-3.9 Prefix words start at the SECOND word.** co() below 3.90 drops
  the Prefix's first word -- whatever it is, article or not -- and then
  takes the last word of what remains; 3.90 and 4.00 read the whole
  Prefix. So a one-word Prefix tells nothing apart before 3.90: `take big
  gem` over gems Prefixed "big" and "small" is "Take what?" at 3.70 and
  co()'s "Which gem.  Big gem or small gem?" at 3.80, exactly as bare
  `take gem` is, and so are `take orb` ("a") and `take cog` ("the"), while
  3.90 answers "You pick up big gem.". Two words leave one: pins Prefixed
  "old red"/"new red" answer `take red pin`. Three words prove it is the
  LAST that is kept and the FIRST that is dropped, not the second that is
  kept nor an article that is dropped: gems Prefixed "a very red" / "a
  very blue" answer `take red gem` but not `take very gem`, and pins
  Prefixed "big red" / "small red" refuse `take big pin`. `[<3.9]`
  p*TAKEP/p*TAKEQ (`lib_co_prefix_word`, 2026-09-20)
- **4.0 take asks with the same pending object drop does.** run400 has two
  answers for a crowded noun -- the question "Which gem.  The gem or the
  gem?" and the flat "It is not clear which gem you are referring to." --
  and takes() picks between them exactly as drops() does, through
  name_object's 463640 and its Me(424) pending object, only in mode 1
  (pass 0 what is NOT held, pass 1 everything present) instead of mode 2.
  The question needs both halves of that model: two tied objects with the
  same **Short** (a pair joined by an alias is always flat), and a tie the
  verb's OWN pass made -- when the verb's side of the room held nothing,
  pass 1 counts more than pass 0 and restores pass 0's empty Me(424), so
  `take pad` over two held pads and `drop gem` over two loose gems are
  flat while `take gem` and `drop pad` ask. That also settles p4OPENA's
  `take gem`: by then the gem and the rock aliased "gem" were both loose,
  so it is the alias cell. Nothing below 4.0 is in this -- see "Pre-4.0
  take/drop over several namesakes" -- and name_object's prompt, unlike
  the generaltasks scan's, prints in full with a question already open:
  p4TAKER's `drop cog` asks and the `drop pad` right after it asks again.
  `[4.0]` p4TAKER/p39TAKER (`make_taketieprobe.py`,
  `cmdfile_ptaketie.txt`, Adrift_242_pe390, Adrift_243_pe400;
  `lib_name_object_resolve_400`, `lib_co_400_raise_common`,
  `lib_disambiguate_object_common`, 2026-09-20)
- **4.0 open/close ask by the same pending object, and its index+2
  quirk.** A crowded `open X Y Z` that openclose cannot settle is usually
  the flat "You can't open that.", but sometimes the ambiguity question:
  `open rock gem chest` and `open chest gem` are "Which chest.  The gem,
  the rock or the chest?". By the INDICES of the objects the line names --
  word order makes no difference -- the measured matrix is `{0,1,3}` the
  question and `{0,1}` `{1,2}` `{0,1,2}` `{0,2,3}` `{1,2,3}` `{0,1,2,3}`
  all flat, in all three probe worlds alike, whichever of them holds the
  openable object. It is not openability, name length or word order: it is
  name_object's 463640 again, in mode 0 (one pass, the co(i, 0) gate), and
  the whole matrix is that walk's Me(424) **index+2** quirk. After a tie at
  index k the result holds -(k+2), so the next tied object's Short is
  compared with the Short of the object TWO past k; `{0,1,3}` ties at 1,
  which points the comparison at object 3 -- the tied object itself, which
  matches itself -- so Me(424) becomes 3 and the question is raised about
  it. Every other set either compares two different Shorts or reads past
  the end of the object table. `[4.0]` p4OPENW/p4OPENA/p4OPENL/p4OPENT
  (`make_openwithprobe.py`, `cmdfile_popenw2.txt`, `cmdfile_popena.txt`,
  `cmdfile_popenl.txt`, `cmdfile_popent.txt`, `cmdfile_popent2.txt`;
  Adrift_233_ox400, Adrift_235_oy400, Adrift_237_oz400, Adrift_239_pa400,
  Adrift_238_pb400; `lib_open_close_tie_400`,
  `lib_name_object_resolve_400`, 2026-09-20)
- **A 4.0 open or close resolves the WHOLE line, " with " tail and all.**
  therest's with-arm ("You can't open the box with the coin.") only ever
  answers the lines openclose let go. openclose scores the whole typed
  line with 463640 in mode 0 (open 4756AB, close 4759D5), so the tail is
  not a barrier -- it is more candidates. With p4LOCK's locked box and its
  key, the held coin: in Alpha both score, the walk ties and therest
  answers; `open box with zzz`, zzz naming nothing, has the box alone and
  opens it; and from Beta, with the box seen but left behind, `open box
  with coin` has the held coin alone and answers openclose's own "You
  can't open the coin!". An empty present pass with a unique seen winner
  still gets the "can't see" refusal (4887A0). `[4.0]` p4LOCK
  (`make_400_lockprobe.py`, `cmdfile_lock3.txt`, Adrift_lock3.txt;
  `lib_open_close_with_400`, 2026-09-20)
- **A 4.0 lock or unlock works on a seen-but-absent object.** The arms cut
  the line at " with " (475D5D), resolve the head with 463640 in mode 0
  (475D91, 476141) and then run in full -- openness refusals, Key test,
  the unlock itself -- without ever asking where the object is. From
  p4LOCK's Beta, with the box locked back in Alpha and the coin in hand,
  `lock box with coin` is "You lock the box with the coin.", `unlock box`
  is "You unlock the box with the coin.", and an unresolved named key
  takes the keyless branch (4763ED). (The `unlock box with zzz` / `lock
  box with zzz` cells of Adrift_lock2 do NOT show that: the box was
  already unlocked by then, so they are openness refusals and prove
  nothing. p4WTIE's feed 3 re-locks between cells and proves it properly
  -- see the next entry.) This generalises the sswhore refusal already
  noted. `[4.0]` p4LOCK (`cmdfile_lock2.txt`, Adrift_lock2.txt;
  `lib_lock_absent_object_400`, `lib_lock_backend`, 2026-09-20)
- **A 4.0 lock or unlock never asks which key, and naming the key is what
  picks it up.** openclose starts var_88 at -1 (475C63) and sets it only
  from a " with " half that 463640 RESOLVES (475CB0), so a half that ties
  is a half that named nothing: with two objects Short "stone" and two
  sharing the alias "gems", `unlock box with stone`, `unlock box with
  gems` and `unlock box with zzz` are all the keyless branch's "You unlock
  the box with the coin.", and `lock box with stone` the lock twin, while
  a half that resolves to the wrong object is the flat "You can't unlock
  the box with the knife.". No question is ever asked, so SCARE's old
  "<verb> that with what?" -- in no Runner's string pool -- is gone; it
  was what sswhore's `unlock drawer with key` invented. The two branches
  differ in one more thing: the NAMED key is picked up (`unlock box with
  coin`, the coin on the floor, is "(Picking up the coin first)" and
  leaves it carried), where bare `unlock box`, `unlock box with stone` and
  `unlock box with zzz` are "You don't have anything to unlock the box
  with!" -- SCARE had the two exactly the wrong way round. Refusals come
  first either way: a wrong named key on the floor is the flat can't-line,
  and "The box is already locked!" precedes both. `[4.0]` p4WTIE
  (`make_400_withtieprobe.py`, `cmdfile_wtie3.txt`, `cmdfile_wtie4.txt`,
  `cmdfile_wtie10.txt`; Adrift_wtie3/4/10; `lib_lock_backend`,
  2026-09-20)
- **A 4.0 question raised by a " with " line comes out of ONE half.**
  therest splits at " with " before any verb test (4883C5) and scores each
  half with 463640, and the candidate list the prompt reads back is the
  one the last half scored -- not the line's. With two objects Short
  "stone", a knife, a box and a rope: a head that TIES parks its own crowd
  and therest leaves (`cut stone with knife`, `cut stone with zzz`, `chop
  stone with knife` all answer "Which stone.  The red stone or the blue
  stone?", the knife left out although the line names it); a head that
  RESOLVES lets the tail be scored and its tie ask instead (`cut knife
  with stone`, `cut rope with stone`, `open box with stone`, `close box
  with stone`); and a head that names NOTHING declines before the tail is
  ever scored, so `chop zzz with stone` is the game's DontUnderstand text.
  Examine is the exception, and only where its own half ties: it sits
  above therest and answers with the whole line's reference set, so `x
  stone with knife` is "Which stone.  The knife, the red stone or the blue
  stone?" and `x stone with box` names the box, where `x knife with
  stone`, `x box with stone`, `x rope with stone` and `x zzz with stone`
  list the two stones alone -- an examine whose head names nothing still
  reaches the tail, where an unhandled verb does not. The tie must be one
  of Short: `open box with gems` and `close box with gems`, tied by alias
  only, raise nothing. A second identical with-line is "That is still
  ambiguous!", and an answer rebuilds the line (`cut rope with stone` +
  `red stone` = "You don't have the red stone."). `[4.0]` p4WTIE
  (`cmdfile_wtie.txt`, `cmdfile_wtie5.txt` .. `cmdfile_wtie9.txt`;
  Adrift_wtie, Adrift_wtie5..9; `lib_with_split_crowd_400`,
  `lib_disambiguate_object`, `lib_cmd_verb_object`, 2026-09-20)
- **A 4.0 crowd is the WHOLE line's, and its FIRST object decides whether
  the question is asked.** Away from examine, the candidates are 463640's
  over the whole typed line -- every object of the top score, a second
  noun included -- and not the `%object% *` reference set a library row
  bound. The walk keeps one best, and only a tie whose Short matches that
  best's parks the pending object (Me(424)) the question is asked from, so
  a namesake pair named AFTER some other object of the same score asks
  nothing at all: `chop stone knife` is the game's DontUnderstand text,
  where `chop stone`, `chop stone with knife` and p4CO's `chop tree rock`
  (the pair first, the odd one after) all ask. A crowd that asks nothing
  leaves the handler with no object rather than a listing, and the command
  goes on to its own `%text%` row: `cut stone knife` is "You can't cut
  that." and `open box knife` "You can't open that.", both counted turns.
  That retires SCARE's "Please be more clear, what do you want to
  <verb>?", which is in no Runner's string pool, from every 4.0 path but
  examine's. `[4.0]` p4WTIE (`cmdfile_wtie6.txt`, `cmdfile_wtie7.txt`;
  Adrift_wtie6/7; `lib_co_400_raise_for_short_tie`,
  `lib_disambiguate_object`, 2026-09-20)
- **"That wasn't one of the options!" is the 4.0 question meeting a SECOND
  element of the same typed line.** generaltasks keeps two things, not
  one: the question (494234, "term|command") and what the last prompt
  offered (4941F4). The question is taken at the top of a typed LINE
  (489FD4) and spent by its first element (48B5FC) -- the queue drain and
  the answer re-runs jump back in below that capture, at 489FEB -- so a
  question raised by an EARLIER element of the line being run is still
  standing at 48B6AE, where the candidate list is not consulted at all:
  "That wasn't one of the options!", and the question is dropped
  (48BB5D/48BB8E). Only then is 4941F4 consulted (48B6F6, 48B80F for a
  character): the same offer again is "That is still ambiguous!" and
  clears both, a different one is a full prompt, and an element that flags
  no ambiguity at all forgets it (48B61F). So on p4CO `x tree and x tree`
  is prompt + wasn't-one-of, `x tree and x tree and x tree` adds
  still-ambiguous, `x tree and x rock and x tree` still says wasn't-one-of
  (the question survives an element that did something), `x tree` / `x
  tree` / `x tree` is prompt / still-ambiguous / prompt, and `x tree` / `x
  keys` gets a second full prompt -- which is what proves the arm is a
  comparison and not a flag. An element that says NOTHING never
  reaches any of this: the answer slot claims it first (48AFF3, gated on
  the reply MemVar_4941B0 being empty), which is why `chop tree and chop
  tree` -- the unhandled-verb tie, whose catch-all the ambiguity holds
  back -- is "That is still ambiguous!" where `x tree and x tree` is not,
  and why the element after an answered one is dropped with the rest of
  the line. `[4.0]` p4CO (`make_400_coprobe.py`, `cmdfile_co7.txt` ..
  `cmdfile_co11.txt`, Adrift_co7..co11; `lib_co_400_begin_line`,
  `lib_co_400_raise_common`, `lib_co_400_take_question`,
  `lib_co_400_raise_for_short_tie`, 2026-09-20)
- **A 4.0 answer REBUILDS the typed line, and what the prompt remembers
  is the candidate LIST.** generaltasks never scores the answer against
  the candidates. The answer slot at 48AFF3 fires when the element said
  nothing (its reply 4941B0 is empty) and a question stands; 48B020 splits
  "term|command", 48B07C asks whether the command holds the term, and
  48B097-48B142 build `Left(command, at-1) & answer`, append `" " & term`
  when the built line does not already hold the term as a word (48B0D8's
  `c()`), then append the rest of the command past the term; 48B152 clears
  the question, 48B158 parks 4941EC at -1, and 48B15B jumps to the element
  top (489FEB) to run the rebuilt line. With no term in the command it is
  just `command & " " & line` (48B15E). So `chop tree` answered `keys`
  runs `chop chop keys tree` -- which names the keys too, and asks a
  SECOND full question; the old "score the candidates" model answered
  "That is still ambiguous!" there (Adrift_co11 turn 15). And 4941F4 is
  the candidate list, not the term: co() accumulates the candidates' names
  into 4941F0 as it walks them (the Which arm 464560), 48BB53 copies it
  and 48B6FF compares the strings, so `x tree` then `x tree rock` -- one
  term, two objects then three -- asks the whole question again, while `x
  tree` twice is still-ambiguous (Adrift_co14). Scarier compares the
  candidate objects, which differs only where two different sets would
  render alike. Last, the term the question names need not be the tied
  Short: an alias the tied objects SHARE, typed as a whole word of the
  line, replaces it (48B73C-48B78C takes Short(4941EC) and walks that
  object's aliases, 4941EC being parked by co() at 46486C), so `chop keys
  tree`, `chop tree keys` and `chop shed keys tree` all ask "Which keys."
  while `chop tree shed`, `chop tree hut` and `chop rock tree` ask "Which
  tree." -- "shed" being the hut's alias but the shed's Short is not
  shared, and `chop keys`, `chop rock keys`, `chop hut keys` are all
  refusals. Why the hut/shed pair parks nothing at 4941EC where the trees
  and keys do is NOT modelled; the aliases are read off our own tie
  instead. And the rebuilt line is a TURN of its own: 489FEB is above the
  stores that mark a line administrative, so the answer is counted by what
  the rebuilt line does, not by the prompt that asked for it -- `cut rope
  with stone` is the question and no tick, and `red stone` runs `cut rope
  with red stone`, says "You don't have the red stone." and ticks, where
  SCARE printed the refusal without spending a turn. A re-run that asks
  again marks itself, and so does the still-ambiguous arm, so only a line
  that did something reaches the clock. `[4.0]` p4CO (`cmdfile_co12.txt`
  .. `cmdfile_co14.txt`, Adrift_co12..co14) and p4WTIE (Adrift_wtie6 turn
  11, Adrift_wtie8 turn 10); `lib_co_400_object_answer_line`,
  `lib_co_400_scan_term_400`, `lib_co_400_raise_common`, 2026-09-20
- **A 4.0 turn SCARE does not count: a take's "Which" prompt is unanswerable
  and TICKS when the " with " half names an object.** `take stone with
  knife` prints "Which stone.  The red stone or the blue stone?" exactly as
  bare `take stone` does, but the two are opposite machines: bare `take
  stone` registers the question and is administrative (4941EC >= 0 at
  48B5B5), where the with-line registers nothing, counts the turn, runs
  the events, and the answer `red stone` is the game's DontUnderstand text
  (Adrift_wtie15). The writer that leaves 4941EC at -1 is NOT in takes():
  it is openclose (476468), which runs unconditionally at 48A515 after the
  take has already printed. Its with-half (475C63-475D6C): `var_88 = -1;
  If c("with") Then var_88 = 463640(Right(line, Len - InStr(line,
  "with")), 0, 0)` -- and the scorer's restart at 4630BC sets Me(424) =
  -1 before it walks -- `If var_88 < 0 Then For each object: If co(obj,
  0) [46486C] Then If InStr(line, Short) > InStr(line, "with") Then var_88
  = obj`. co(obj, 0) re-parks Me(424) for every object whose word the line
  holds: one present seen namesake -> -1, two or more -> the tied index.
  So the tick fits "the tail RESOLVES an object" (`stone|knife`,
  `stone|box`, `stone|rope`, `stone|coin`, `stone|emerald`, `ruby|stone`,
  `emerald|stone`: the scorer settles it at -1 and the co loop finds
  nothing to re-raise, or the last object it names is unique) and "no
  tick" fits "the tail names nothing" (`stone|zzz`, `stone|qqq`, empty
  tail: the co loop walks the line's words and the stone pair re-parks
  the index). Measured with a watchpoint on 4941EC (VBRNG_WATCH, see
  rng/README.md "Watchpoint"): `take stone with knife` ends 5 -> -1 from
  463640 called at 475CB5; `take stone with zzz` ends -1 -> 4 -> 5 from
  co() at 475CF4, then 48BBF3 registers the question. The old fit "the
  head names a pass-0 candidate" was a coincidence of which cells were
  driven. Two more things the port needed to match run400 cell for cell:
  get_piece scores the WHOLE typed fragment with 463640 in mode 1 (473011)
  before any word-order parse, so `take rope with ruby` takes the ruby and
  `take stone with knife` ties the two stones with the knife left out; and
  the Which term is the LAST tied object's raw Short (4733BD:
  Objects(-(var_BA+2)).global_4), where the "not clear which" arm (473336,
  when Me(424) < 0) names the last tied object's typed name. Unmeasured:
  `take stone with gems` (alias tie in the tail; predicted no tick); a
  " with " line on a handler that prompts nothing (`unlock door with key`
  with two keys), where the co loop would leave a tie for the generaltasks
  scan to ask -- the scan model stays NPC-gated; and a drop/put prompt on
  a with-line. Open: `take stone from knife` is "Which stone.  The red
  stone or the blue stone?" with no tick in run400 (get_piece's "from" arm
  472EF1 resolves the container in mode 0 and then scores the piece) where
  Scarier answers "You can't take anything from the knife." (Adrift_wtie11
  turn 16). `[4.0]` p4WTIE (`cmdfile_wtie11.txt` .. `cmdfile_wtie18.txt`,
  `cmdfile_wtiewatch.txt`; Adrift_wtie11..18, Adrift_wtiewatch,
  `wtie_watch_trace.txt`; `lib_openclose_with_half_400`,
  `lib_co_400_named_question_raised`, `lib_take_whole_line_400`,
  `run_all_commands`, 2026-09-20)
- **Pre-4.0 read is examines()' object too.** A `read` line naming more
  than one object is settled exactly as `x` settles it: 3.90 by
  referencedob()'s last-word pass (`read rock gem` -> "You can't read the
  gem!", `read gem rock` -> "the rock!", `read rock with slab` -> "the
  slab!"), 3.70 and 3.80 by examines()' own question, "Which <Short of the
  LAST match by index> would you like to examine.  <the matches, in index
  order>?" -- so `read rock gem`, `read gem rock` and `examine rock gem`
  are all "Which rock would you like to examine.  The gem or the rock?",
  and `read rock with slab` is "Which slab would you like to examine.  The
  rock or the slab?". The question is 3.7/3.8 only; 3.90 replaced it. Only
  a line naming nothing reaches "Nothing special.". `[<4.0]` p*OPENW
  (`lib_cmd_read_other`, `lib_disambiguate_object_common`, 2026-09-20)
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
- **A task command's %<user variable>% is a substitution, and its marker is
  lower-cased while the variable's Name is not.** The last arm of the
  checktask lead. run390 44AF07 walks the variable array, builds `"%" &
  var(n).Name & "%"` (44AF25), and on an InStr hit writes
  `Format(var(n).Value)` back over the command (44AFC2); run400 does the
  same inside the shared substituter Proc_19_36_45F268 (45F105-45F259). So
  a `%foo%` is spelled out before the command is tested, exactly like
  %object% and %number%, and never a positional wildcard the way Scarier's
  pattern tree used to treat it. Four rules, all measured on p39VARREF /
  p4VARREF: **(1)** the value is always the NUMERIC one, so 4.00's string
  variables spell 0 -- `nurb %word%` over word="quux" runs on `nurb 0` and
  refuses `nurb quux`; 3.90 cannot even pose the question, its VARIABLE
  record having no Type field at all (sctafpar.cpp spells it `ZType`, a
  defaulted zero read from nothing), so every 3.90 variable is a number.
  **(2)** The marker is lower-cased and the stored Name is not: `wibb
  %NUM%` reaches `num` and runs on `wibb 7`, while `bork %Big%` and `snib
  %big%` over a variable named `Big` reach nothing at all -- a capitalised
  variable Name is unreachable from any command, which kills Riding_Home's
  `{your/%NewPlayer%'s}` in the real Runner. **(3)** `%t_<name>%` never
  substitutes anything, although the code is there (44AFF8, int2text at
  44B03D; run400 45F1C6): its guarding InStr searches the typed line, not
  the command, so `frob %t_num%` refuses both `frob seven` and `frob 7` at
  both versions -- and the output filter still spells it out, `zap`
  printing "ZAP [7] [seven] [42].". **(4)** A marker naming no variable
  matches nothing, not even typed verbatim (`blip %nosuch%` is refused).
  Pre-4.0 a `*` command carrying a variable is decided by checkwild and its
  pieces are order-free, so 3.90 runs `* zog * %num% *` on `blip 7 zog`
  where 4.0 substitutes and then cuts; that pair of cells is the whole
  version split, and below 3.90 there is no Variables section in the TAF at
  all. Corpus exposure is three commands in two games, both with goldens
  (iachini `push * key * %keynum% *`, whose route types the measured `push
  key 80`; Lair of the Vampire `drop %item%` and `give %item% to
  %character%`), plus the dead `%NewPlayer%`; 428 PASS before and after.
  Commands mixing a variable with `%object%`/`%character%`/`%text%`, or
  carrying an unknown marker (COBL's `[%theobject%/%object%]`), still go to
  the tree, where `uip_match_variable()` now answers the same four rules
  for the marker itself. `[>=3.90]` p39/p4VARREF
  (`run_substitute_variable_references`, `var_get_command_number`,
  2026-09-20)
- **A task command's reference marker is matched without regard to case.**
  checktask looks for a marker with `InStr(1, cmd, "%object%", 0)` (run390
  44AAC0, the command read straight out of the task record at
  44AAA5-44AAB5), and the 0 is vbBinaryCompare; the same shape guards
  %character% (44AD2A), %number% (44ADDF) and the variable arm (44AF43).
  The exe's literals are lower case, so on a binary compare a command
  spelled `frob %Object%` ought to carry no marker at all -- and it does.
  p37/p38/p39/p4CASEREF put the four known markers side by side with their
  lower-case twins (`frob %Object%`, `nurb %CHARACTER%`, `blip %Number%`,
  `murg %TEXT%`, fed `frob rock`, `nurb fay`, `blip 7`, `murg quux`) and
  every capitalised one runs its task, in every Runner that knows the
  marker at all: %object% everywhere, %character% and %number% from 3.90,
  %text% at 4.00, and below that both halves are literals and both cells
  answer DontUnderstand. So the COMMAND is lower case before any matcher
  sees it -- checktask holds no LCase above 44B0BA, so the fold is at load,
  and the site is not located in the listing. p*VARREF had already said so
  sideways and fixes which side folds: `wibb %NUM%` reaches a variable
  named `num` while `bork %Big%` *and* `snib %big%` over a variable named
  `Big` reach nothing, which only a folded command against an unfolded
  stored Name explains. Scarier folds the markers and nothing else, since
  everything else in a command is already compared case-insensitively (the
  equality LCase()s both sides at 44B0BA/44B0DA, and checkwild and 4.0's
  wildcard matcher fold too -- p*CASEREF's `* Zag * GEM *` runs on `xxx zag
  xxx gem xxx` everywhere, and did here before this). Corpus exposure is
  two games: X-Files task 30 `Molest *%Character%` and its four alternative
  commands become live %character% commands, and Riding_Home task 78
  `knock {on} {your/%NewPlayer%'s} {door}` stays dead, the fold being on
  the command and not on the Name. 428 PASS, no golden moved.
  `[all versions]` p37/p38/p39/p4CASEREF (`run_lower_command_markers`,
  2026-09-20)
- **A 4.0 GROUP is the Runner's only case-SENSITIVE command test, and
  nothing can feed it a capital.** Of the three tests at 45D9FC, equality
  (45DA51) LCase()s both sides and the `*` matcher (457D68) lowers the
  pattern alone (457B17), which is invisible because the typed line was
  lowered at read (45C5DC); NewParse (45D940) folds neither side. Its two
  compares are `EqStr` at 0005D7FA and `EqVar` at 0005D835 in
  run400.p32dasm.txt, and a census of that whole dump finds 551 `EqStr`,
  178 `NeStr` and no text-compare opcode at all, so every module in the exe
  is Option Compare Binary. Two things could hand it a capital, and neither
  does. (1) The author: p4GRPCASE's `zog [Rock/Gem]` runs on `zog rock` and
  on `zog gem`, and `nurb {The} rock` on `nurb the rock`, so the load-time
  fold of the task command reaches inside a group -- the first DIRECT
  measurement of that fold, which p*CASEREF above could only infer from its
  markers. (2) A substitution: the %object% matcher 458E6C splices the
  Short or Alias RAW and routes a group pattern to 45D940 (458D11/458E29),
  so `frob [%object%/zzz]` runs on `frob rock` and is refused on `frob gem`
  where the Short is authored `Gem` -- exactly as the group-free `wibb
  %object%` is refused, the p4BURN rule reaching the group path unchanged
  -- while the %character% twin 4696A4 lowers the Name (4691B4) and every
  Alias (469207) before splicing and routes groups the same way
  (468FBB/46912D/4692E8/469542), so `blip [%character%/zzz]` runs on `blip
  fay` with the Name authored `Fay`. Thirteen cells, and Scarier already
  answers every one of them the way run400 does. This closes the last arm
  of the old "output filter" lead; its other two are closed too -- the ALR
  pass's trailing spaces are the Runner's pspace() paragraph ends, modelled
  in `pf_replace_alrs()` since 2026-09-13, and the drop rebuild at 46F33B
  sits inside drops 46FB8C, which the call census found to be dead code
  (see "The 4.0 drop/take setter branches are DEAD CODE"). `[4.0]`
  p4GRPCASE (`make_grpcaseprobe.py`, `cmdfile_pgrpcase.txt`,
  `Adrift_grpcase400.txt`, run400x) (no code change, 2026-09-20)
- **A pre-4.0 task command's %object% walk takes an object's Short and its
  Aliases together.** 3.90's walk is ONE loop over the object array, not
  two: `For var_138 ... Next var_138` with the Next at 44ABE5, testing the
  Short (44AAEA, seen 44AAFD, store 44AB0D, Replace 44AB40) and then that
  same object's Aliases (44AB6D, seen 44AB80, store 44AB90, Replace 44ABC3)
  before it moves on. The Alias arm substitutes the ALIAS, `.global_8`, and
  not the Short. With the first-hit-spells / last-hit-expands rule that
  makes the order obj0.Short, obj0.Alias, obj1.Short, obj1.Alias, ..., and
  a line naming one object's Short and an earlier object's Alias is spelled
  with the alias and expands to the later object: with a gem (index 0,
  alias "stone") and a rock (index 1), 3.90 answers BOTH `zug rock stone`
  and `zug stone rock` against "* zug * %object% *" with "WILD [a rock]."
  -- the gem's alias makes the string "* zug * stone *" that checkwild
  finds, the rock's Short binds after it. 4.00 is the version that walks
  twice, every Short and then every Alias, and answers both lines "WILD [a
  gem]." Below 3.90 there is no Alias arm at all, which the same probe
  shows from the other side: `blip stone` against "blip %object%" is the
  object catch-all at 3.70 and 3.80 although co() finds the alias for it.
  Scarier had 4.0's shape everywhere below 4.0. Zero corpus exposure (428
  PASS before and after) -- it needs one line naming two objects, one of
  them by an alias, against one reference-bearing command. `[<4.00]`
  p37/p38/p39/p4TEXTSRC (`run_pre400_substitute_references`, 2026-09-20)
- **The 3.7/3.8 object catch-all answers in index order, aliases
  included.** run380 442F5D asks co() about EVERY object, so every object
  whose Short or one of whose Aliases is in the line is a candidate and
  index order alone decides which one speaks -- not the order the names
  appear in. With a gem at index 0 aliased "stone" and a rock at index 1,
  both `zug rock stone` and `zug stone rock` are "I don't understand what
  you want me to do with the gem." at 3.70 and at 3.80, where Scarier
  answered the rock for the first line: our references bind one candidate
  off the line, which puts the answer in line order. 3.70 resolves the
  alias here although its takes() test is a bare c(Short) -- `blip stone`
  is the gem's catch-all in both older Runners -- so the scan reads the
  Aliases at both versions, unlike `lib_co_pre400()`. 3.90's walk already
  had this shape (run390 46024A, `lib_co_pre400` mode 0 per object) and
  answers the gem for both lines; 4.00 resolves with the 463640 score and
  answers neither. Zero corpus exposure, 428 PASS before and after -- it
  needs an unhandled line naming two present objects, one of them by an
  alias or by a later-indexed Short. `[<3.90]` p37/p38TEXTSRC
  (`lib_catch_all_names_pre390`, 2026-09-20)
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
  - **run390's " is full." arm wants a size-0 object.** 461E59 sits after
    the count and its refusals and before the move: a container already
    holding exactly its capacity, with something on the line that still
    fits, answers "<The X> is full." and moves nothing. Something fitting
    into a full container means a size of nil, and size is SizeMultiple
    raised to the SizeWeight's tens digit, so it takes a SizeMultiple of 0
    -- which the ADRIFT editor never writes, which is why the arm read as
    dead. With one: `put feather in bag` (bag capacity 2, a coin and a
    stone inside, feather SizeWeight 10) is "The bag is full." and the
    feather stays in hand; a size-1 pebble on the same line is the
    ordinary "The pebble can't fit inside the bag at the moment.", so the
    per-object refusal still comes first; `put all in bag` with two size-0
    objects held is "The bag is full." and not "Nothing will fit inside
    the bag."; and a surface takes all three (the arm's third test is
    var_E0 = "inside"). The same probe is the first measurement of
    obj_scale()'s zero multiple. `[3.9]` pPUTZERO39
    (`make_39_putzeroprobe.py`, `cmdfile_pputzero.txt`,
    Adrift_pputzero39.txt; `lib_put_in_backend`, 2026-09-20)
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
- **Pre-4.0 attack arm hides behind ", but nothing happens.", so a bare
  `attack` line is DontUnderstand.** generaltasks' turn tail runs
  characters() only when therest left a message; the attack arm (run380
  440260, run370 4383CD) then needs c(hit|kill|kick|punch|attack) -- 3.8
  also "no task ran" -- and a buffer that is empty or ends ", but nothing
  happens.", which only therest's hit/push/pull/press/kick arms leave.
  therest has no attack arm, so `attack dave` reaches the tail empty:
  "I don't understand.", no tick. c("attack") fires only behind one of those
  verbs: `push attack dave`, `attack dave push`, `pull attack dave` are all
  "Dave avoids your feeble attempts.", `push attack cora` (next door) is
  "Cora is not here!"; `kill`/`punch dave` keep therest's own "Now that
  isn't very nice." / "Who do you think you are, Mike Tyson?" because those
  do not end ", but nothing happens.". The with-loop takes the LAST named
  object whose raw Short/Alias(0) sits after "with" (binary InStr against
  the lower-cased line): not held "You don't have the stone!", Weapon "You
  swing at Dave with the stone, but you miss." (comma, all Runners), else
  "I don't think the stone would be a very affective weapon!"; `attack dave
  with stone` is the stone's catch-all. Identical at 3.7 and 3.8. `[<3.9]`
  cmdfile_pattackarm.txt on p37/p38NPCAMB, run370x Adrift_242_pattackarm37,
  run380x Adrift_243_pattackarm38 (`lib_hit_arm_pre390` under the
  hit/kick/push/pull/press object and other handlers, 2026-09-20)
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
- **A task's AdditionalMessage joins the turn's string at every version.**
  It goes on after the two-space separator, not on a line of its own:
  run370 441C78 appends "  " and the message outright, run380 44D03F guards
  that with `Right(out, 2) <> "  "` (which is the superliam suppression, see
  `task_suppresses_additional_message`), run390 43F1C5 calls pspace() at
  43F1C8, and run400 does the same. troll t17 (3.90) is the corpus case:
  `get in coach` ends with the coach room's block and then "The sun is
  creeping up in the morning sky..." on the same line
  (runner_transcripts/troll.txt). `[all]` (`sctasks.cpp`, 2026-09-20; was
  4.0-only; 16 goldens moved, whitespace only; sweep_wine_breaks
  5694 -> 5693, k1 187 -> 186, runner-only unchanged at 2)
- **A library answer has no terminator of its own.** The Runner's library
  handlers append their sentence to the turn's one string and stop there;
  the break after it is ours, not theirs, so whatever the turn prints next
  -- an event's text, an NPC's walk line, a battle strike -- follows after
  pspace()'s two spaces. troll (3.90) is the plainest: `drop tankard` is
  "You drop the tankard.  Your guts rumble.  Your throat is sore." on one
  line (runner_transcripts/troll.txt t29), and t31, t37, t43, t45, t57 say
  the same after `drop`, `get` and `take ... from`. Two of the library's
  newlines are NOT ours and keep their own spelling: the room heading's,
  which the Runner stores too (`lib_print_room_name`,
  `pf_buffer_hard_break`), and the room description block's, already noted
  where `lib_print_room_contents` takes it back for its own "  Also here is"
  -- marking either as an answer break makes the contents list run onto the
  heading, which the archive catches at once (6 rows, 14 new runner-only
  breaks). `[all]` (`pf_buffer_answer_break`, 2026-09-20; 39 goldens moved,
  whitespace only; sweep_wine_breaks 5735 -> 5694 Scarier-only breaks, of
  which the meaningful single-newline kind 221 -> 187, runner-only unchanged
  at 2, no row worse, 17 better)
- **Below 3.90 the WinText joins with ONE space.** run380 tasks() builds the
  win branch as `out = out & " " & WinText` (44E33C); 3.90 alone routes it
  through pspace() (run390 loc_43F255) and 4.0 opens a paragraph. Scarier
  had a butt-join here, from a whitespace-normalised note of the
  microwaveman measurement -- the .rtf itself has the space. Nine pre-3.9
  rows carry the join and all nine agree, including the four whose text
  already ended in spaces, where the extra one lands on top: "You win the
  game. You have destroyed Coffee Man..." (microwave_man.rtf), "You read the
  parchment aloud. Suddenly..." (cave.rtf), "...safe and sound. Well there
  you have it..." (crime_adventure.rtf), "Ypu ask her out You win"
  (life_of_mike.rtf), "...Thanks for getting us back home!". Your joyful
  reunion..." (timmy_reid.rtf), "you walk up the stairs. "hello tom ..."
  (tom_ceader.rtf), "...it really is you!" + FOUR spaces + "As Martha
  ushers you..." (akron.rtf), "and go outside." + three + "You've done it!"
  (haunted_house.rtf), "Congratulations!!!" + five + "You stand in a
  sparkling room." (super_liam.rtf). NOT ported alongside it: below 4.0 a
  task that runs flushes its two-space separator into the turn's string
  BEFORE it tests CompleteText, so a task with no text still leaves "  "
  behind for whatever butts on next. haunt's win task 23 is `- win` with no
  text at all, which is why the Runner reads "Horace lurches in from
  above.   You drop down into the laboratory," -- two spaces from the empty
  task, one from here -- where Scarier prints one. Nothing else in the
  engine butts on without a separator of its own, and every compare
  normalises runs of whitespace, so no measurement sees it. `[<3.90]`
  (`task_print_end_game_message`, 2026-09-20; 9 goldens, whitespace only)
- **A walk announcement and an exits list end at the full stop.** Neither
  carries a terminator of its own: the walk announcement's separator is the
  two spaces the Runner puts in FRONT of it, guarded on the buffer not
  already ending in a newline (run380 441740-44174A, the text then appended
  at 4417B0 and the bare "." at 4417C2; run370 loc_439360, run390
  loc_45A99B, run400 @468A5D, and the hidden form at run370 loc_4397A3 /
  run400 loc_468CF9), and the exits list is an ordinary library answer that
  the room builder appends to the one string (run400 472C64, run390
  44813D). So whatever the tick prints next runs on from them. shadowpeak
  (4.00): "You can move north, east and west.  Seeker hums!"; timmy_reid
  (3.80): "...wafts towards you from the west.  Electricity rips through
  your spine..."; haunt (3.80): "You can only move up.  Horace lurches in
  from above." (runner_transcripts/shadowpeak.txt, timmy_reid.rtf,
  haunt.rtf). `[all]` (`npc_announce`, `npc_announce_hidden`,
  `lib_print_exits_list`, `pf_buffer_answer_break`, 2026-09-20; 37 goldens
  moved, whitespace only; sweep_wine_breaks 5662 -> 5636 Scarier-only
  breaks, single-newline kind 158 -> 132, runner-only unchanged at 2, no row
  worse, 11 better)
- **A task the engine dispatched joins its CompleteText onto the turn's
  string.** execute_task takes a mode argument, and that argument is the
  whole of the difference between the two kinds of task run. With mode 1 it
  calls pspace() and appends: `out = out & CompleteText` (run390
  43F106-43F132, run400 45A239-45A265). With any other mode it REPLACES the
  turn's string with the text (run390 43F15F-43F172, where the write is
  plainly `MemVar_468154 = CompleteText`; run400 45A27D-45A2AD, where the
  same write reads `var_AC & CompleteText` with var_AC never assigned
  anywhere in the body, so `""`). The caller is what preserves the turn:
  run390's `inventory` saves `out & "  "` into var_110 at 439B55-439B5C
  before calling execute_task(0, ...) at 439B72 and puts it back afterwards
  -- pointless unless mode 0 clears the buffer. Mode 1 is what Sub_20_22 /
  Proc_19_21_45FB78 passes at run400 45FA66 (our run_task_run_by_index: an
  "execute task" action, an event's TaskAffected, a walk's CharTask and
  ObjectTask, a battle task), what run400's `inventory` passes at 45C2FD,
  and what checkevent passes when it dispatches by command text (run390
  42D3F5, run380 43A762); the typed line's own matcher passes 0 and its
  fallback pass 2 (run390 generaltasks 45F48B, 460584). Only the join is
  ported: Scarier's handlers already keep the turn's text exactly where the
  Runner's callers put it back, so the replacement changes nothing
  measurable. shadowpeak (4.00): "You take the pair of leather gloves.  It
  starts to rain...  You hear the flutter of batlike wings." and "You open
  the vial.  The group of fairies seem to have cast some type of magical
  spell...  The open vial starts to pulsate..."; timmy_reid (3.80):
  "...shuffles towards you from the north.  Your grandfather eye's you and
  says, ..."; pyramid (4.00): "You put the golden beetle inside the
  depression.  The beetle presses home into the depression..."; viewtohome
  (4.00): "You hear the sound of water running.  Congratulations! You have
  collected all three medals!" (runner_transcripts/shadowpeak.txt:82,382,
  timmy_reid.rtf:147, pyramid.txt:45, viewtohome.txt:503). `[all]`
  (`task_push_dispatched_run`, `task_in_dispatched_run`,
  `run_task_run_by_index`, `run_task_command_dispatch`, 2026-09-20; 76
  goldens moved, whitespace only -- two of them, pyramid and viewtohome,
  needed their suite win marker shortened because the join rewraps the line
  it sat on; sweep_wine_breaks 5636 -> 5514 Scarier-only breaks,
  single-newline kind 132 -> 28, runner-only unchanged at 2, no row worse,
  44 better -- and paint, fullcircle, thelasthour and spot_of_bother now
  match their Runner transcript with no break at all)
- **An NPC's blow joins the turn's string.** chardohit -- the NPC's blow --
  calls pspace() at the head of every one of its four printing branches, so
  the sentence runs on after whatever the turn has already said: run390
  442C7C at 4424E6 (bare-handed, landed and no damage), 442610 (armed, both
  outcomes), 4427EF and 442911 (the same two against the other class of
  target), and run400 4654F8 at the matching 465072, 46513D, 4651EF and
  46537F. killchar does it too, ahead of " falls down, dead." (run400
  44B105). The PLAYER's blow does not: dohit (run390 438B50, run400 45E578)
  writes `MemVar_4941B0 = MemVar_4941B0 & Ary(0) & " hit " & ...` with no
  separator at all -- the same bare concatenation that glues one line's
  several strikes together -- so only an NPC attacker joins. trabula (4.00)
  is byte-exact either way: `attack soldier` is "You stab a soldier with the
  sword.  Soldier falls down, dead." and the Middle Bridge arrival is "A
  soldier is here.  A soldier attacks you with the rapier, but you manage to
  avoid it." (runner_transcripts/trabula.txt, lines 48 and 62). `[all]`
  (`battle_blow_join`, `pf_buffer_join_open`, 2026-09-20; 29 goldens moved,
  whitespace only; sweep_wine_breaks 5693 -> 5662 Scarier-only breaks, of
  which the meaningful single-newline kind 186 -> 158, runner-only unchanged
  at 2, no row worse, 9 better)
- **An event's text joins the turn's paragraph, at every version.**
  checkevent puts the two-space separator ahead of every one of its texts --
  inline in run370 (431CA5, 432143, 4321FE) and run380 (439F69-439F86,
  43A401, 43A50A), pspace() in run390 (4484A7, 4489AC, 448AA4) and run400
  (44A9F4 at 46FECB StartText, 4701BF PrefText, 47028A FinishText, 470633
  the restart's) -- so the event's text is part of the turn's ONE string and
  the ALR pass walks the join. Where the Runner's own string really stops at
  a newline the separator adds nothing and the text starts a fresh line
  anyway: 4.0's "Time passes..." carries a vbCrLf of its own (48ABDA). Where
  it stops unterminated the two spaces go in, as after the ending's bare
  "[Press any key to end]" (haunt T84). Probe p4ALRSRC
  (`make_400_alrsrcprobe.py`), Adrift_10/11_p4src.txt: `xray`, a task whose
  CompleteText is "X." starting an event whose StartText is "EV ball.", is
  "X.  EV qball." on one line, while the FinishText two `wait`s later is
  "Time passes..." / "FIN qball." on two. Below 4.0 the corpus says it,
  troll (3.90) most plainly: its three hunger/thirst events print onto the
  turn's line and onto each other, "You move northeast.  ...work of shadows
  upon the ground.  You feel hungry." and "...Your stomach growls. Your
  throat is sore." on one line each (runner_transcripts/troll.txt), where we
  broke before every one. `[all]` (`evt_buffer_text`, and the pspace() the
  join falls back on in `pf_buffer_join_line`, 2026-09-20; 30 goldens moved
  on top of the 4.0 half's 52, whitespace only; sweep_wine_breaks
  5892 -> 5735 Scarier-only breaks, runner-only unchanged at 2, no row worse
  on any axis, three better: troll 90 -> 60, wonderwombat 219 -> 213,
  the_town_of_azra_v390 38 -> 37)
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
