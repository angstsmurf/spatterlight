# TODO: Runner-transcript verification of the v4 walkthroughs

Replay a wired walkthrough command for command in the real Windows ADRIFT
Runner under Wine and diff the Runner's own transcript against Scarier's.
Where they disagree, fix the engine, never the walkthrough. Then re-bless the
golden and write the evidence into the game's row comment in
`harness/run_v4_walkthroughs.sh`. Scope is all four file versions (3.70,
3.80, 3.90, 4.00). Only the Runner binary and the capture flow change with
the version.

**Pruned 2026-09-14.** This file had grown to 10,000 lines of dated
per-port write-ups. It now holds the workflow, the open leads, the
deliberate deviations and a one-line index of every ported rule. The full
write-ups (probe tables, Runner addresses, corpus fallout per port) are in
git history:

    git show 72fd5ea08:terps/scarier/test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md   # last full version
    git show 45e20596:terps/scarier/test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md    # before the 2026-09-06 compaction

Comments in `run_v4_walkthroughs.sh` and the probe generators cite sections
by title ("Ported 2026-09-10: the take-from handler's own answers" and so
on). Grep the `72fd5ea08` version for the title. The commit hashes in the
index below lead to the code.

---

## Where things stand (2026-09-14)

- **Goldens:** 428/428.
- **Sweep:** `python3 harness/sweep_wine_turns.py` at 8cf9cce63 covers 272
  archived rows: 80 clean, 160 differing, 32 lost a feed command.
- **Every remaining low-count differing row has been triaged** as one of
  three things:
  - RNG, because the capture predates vbrng.
  - A `<centre>` transcript artefact.
  - A harness artefact: `(Press a key)` eating a bridge token, or a popup
    name.
- **No engine lead is left in the archive.** The sweep reads each game's
  highest-numbered transcript. Outside the 44-game xoshiro batch, that is the
  2026-09-08 corpus capture, which was taken before vbrng existed. So its
  random values cannot be compared.
- **The next source of engine leads** is fresh `run400x`/`run390x`
  `VBRNG=xoshiro` captures of the RNG-divergent rows (see "Driving" below).
- **Transcript directories** under `~/adrift-battle/runner/wine/`:
  - `pfx/drive_c/adrift/`: the live archive. Never `rm` a glob there.
  - `transcripts_v4_corpus_2026-09-08/`: 427 rows, native RNG.
  - `transcripts_v4_xoshiro_2026-09-12/`: 44 rows, xoshiro. The job list is
    `xoshiro_jobs.txt`.

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
  - The last command is missing from it.
  - A death or end-game modal wipes the scrollback.
  - `£` comes out as `Â£`.
- **`DUMP_SCROLLBACK=<file>`** saves the RichTextBox's own text. Use it to
  prove a line break is a transcript artefact.
- **Kill Wine with `pkill -9 -f wine; pkill -f wineserver`.** fast.sh and
  par.sh also reap the orphaned `winedevice.exe` processes. The prefix is
  shared mutable state: never drive it from two sessions at once.

### 4. Compare

    python3 harness/compare_wine_transcript.py --taf G --feed F --runner T \
        --env SCR_RNG=xoshiro --env SCR_SEED=1234 [--popup Hero --popup male]

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
   - the Runner's `[Press any key to end]` tail;
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

None of these blocks a golden. Grouped by what is needed to settle them.

### Owed re-drives (quick)

- **`beer`:** walkthrough line 12 is now `wear woolly jumper`
  (84b0bb73a). It needs a Wine re-drive.
- **`aliasagent`:** the solution gained `x tray` (e478cdbd6). It needs a
  re-drive.
- **`hcw`:** `lower susan into trunk` (line 163) is not measured in run400.
- **`yonastoundingcastle`:** rule 2 trips at feed[170]; re-drive.
- **`thepkgirl` and the rest of the native-RNG corpus:** re-capture under
  xoshiro before reading any value difference as engine.
  - The 2026-09-08 sweep lists event-phase off-by-one rows: forum,
    briefcase, backhome, barneysproblem, zelda, gmylm, silk_noil, lostmines,
    aegis and overtheedge.
  - It also lists lost_souls `open door`, goblin t48, losttomb t85/86 and
    wonderwombat maze moves.
  - The 2026-09-14 triage put these down to RNG or artefacts on the old
    capture. The xoshiro re-capture would confirm it.
- **`shadowpeak`:** a winning comparison needs a xoshiro seed that survives
  Morac.
- **Load failures:**
  - run400 would not load these in the 09-07 re-feed: wonderwombat,
    the_town_of_azra_v390, ecod2, everything, archie, chosen.
  - Six rows raised `evaluate error - Subscript out of range` mid-game in
    the 09-06 corpus batch.
  - TheADRIFTProject crashed with run-time error 401 at command 92.
  - darkness finishes at feed[99] with 11 lines left.

### Engine, needs a probe (4.0)

- **Silent put confirmation for object #1.** run400 prints no `put`
  confirmation when the moved object is dynamic object #1 (Adrift_82-87).
  run390 prints one.
  - The listing does not single out index 0: name_object 46E23C/46E3FA,
    insides 46639C. The next step is a live trace of var_A4/var_A6.
  - No corpus row touches it.
- **name_object's own list loops (46E04E / 46E0B2)** are not rebuilt. That
  is six cells, and the wording must come off the Runner.
- **`put box in box`:** run400 announces the take and prints nothing, where
  Scarier says "can't put an object inside itself!".
- **Scope.** None of these is measured:
  - the never-seen "You can't see that." branch at 471995;
  - the two-pass `%object%` scope filter proper (`SCR_TRACE_SCOPE`);
  - the NPC seen gate for `%character%` (xfiles `look up byers`).
- **Second-noun ambiguity:**
  - The wording of an instrument ambiguity is unmeasured (sswhore `unlock
    drawer with key`).
  - The same goes for a tie inside either half of a " with " split.
  - Also unmeasured: lock/unlock of an object with a Key whose " with "
    left half resolves to nothing.
  - Also unmeasured: absent lock/unlock where the object really is locked
    (the key-check path).
- **Ambiguity prompts:**
  - co()'s crowded arm (454454) and its &HFE/&HFF answers are not modelled.
  - "That wasn't one of the options!" has never been triggered.
  - Unmeasured: whether an object ambiguity on a task-answered line also
    suppresses the tick.
- **Events:** an event with RestartType=2, an immediate starter and a
  non-zero length fires once in run400, but Scarier re-arms it. Corpus
  exposure is zero.
- **Put row corners** (all unmeasured):
  - `(Taking X first)` ahead of a closed-container refusal;
  - `put all in <the container, held alone>`;
  - Glum_Fiddle's `(Taking that first)` / `You put that inside` pronoun
    wording (Adrift_583).
- **Output filter:**
  - Where the ALR pass sees trailing spaces is unlocated.
  - The NewParse `%` pattern binary path is unmeasured.
  - The drop rebuild at 46F33B is unmeasured.
- **Drop/take/wear setter branches** 46FB7D, 47C7F1 (and run390's wears at
  43D289) have been read but not measured.
- **The run400 loader's "Anonymous" fill** for an empty PlayerName with
  PromptName off is unmeasured.
- **Silent-task test scope.** run400 tests the whole turn buffer, Scarier
  tests only the task's own output. They differ only when something wrote
  before the verb dispatch. No corpus row is known.
- **Turn sectioning, the unported rest.** run400 builds the turn as one
  string joined with pspace() and runs the ALR pass over it. Scarier joins
  only the room block (2026-09-07) and, at 4.0, the text of a task run by
  an action plus every AdditionalMessage (`pf_buffer_join_line`,
  2026-09-14). Everything else is still its own section, so an ALR
  Original spanning a join does not match. This is unfinished, not policy.
  - An event's text after a task's text: p4SRC `xray`, where run400 prints
    "X.  EV qball." on one line. Measure it first.
  - The rest of the turn: thetest (a two-sentence Original).
  - Expect a large golden reblessing. The task-text join alone moved 94 rows
    (92 whitespace only), and sweep_wine_breaks still counts 5622
    Scarier-only breaks.

### Engine, needs a probe (3.9)

- **`give <x> to <y>`** belongs in characters(), below the room refusal
  (the_hangover t42). Only the 3.7/3.8 give is ported (f83e1cf87).
- **"Please be more clear, who do you want to <verb>?"** is a SCARE
  invention (alexis_worn_cube t79). For `x berry` with two berries, run390
  answers "I can tell you nothing about that." (cybercow_win T118,
  2026-09-14). The `who` form is still unmeasured.
- **Per-verb absent-NPC branches:**
  - `talk to <npc>` elsewhere (hcw, alchemist): unmeasured.
  - `give obj to npc` elsewhere: unmeasured.
  - run390's take `is not here!` at 4596E1 (Battle System off): unmeasured.
  - The attack branch is ported.
  - kill/kick/punch lines go through other grammar first; unmeasured.
- **run390's Who-prefix consumption** at 460022 is assumed, not measured.
- **Take-from:**
  - the " and " clause picks the last container in 3.9 and the first in
    4.0;
  - 3.9's " and " collection bug;
  - the pending slot after `Get X from what?`;
  - surface-vs-container wording of the parent-derivation arm.
- **Put:**
  - the put parser at 461769 ("can't put anything inside/on that!");
  - "onto";
  - a locked container;
  - a named static;
  - an object on a floor supporter;
  - `put all in <nothing>`.
- **Two-object canonical prefixed retry:** the run390 half is not
  re-measured. The 4.0 half is closed.
- **Examine and the ALR pass:**
  - run390's examine state line and move-object seen stamp have not been
    read.
  - The 3.9 type-7 battle branch has not been read; Scarier keeps the zero
    floors.
  - Ask-topic overwrite: run390 lets the last matching topic win, Scarier
    keeps the first.

### Engine, needs a probe (3.7 / 3.8)

- **Handlers other than take** were measured on single matches only.
- **run380's post-take-from task sweep** after a *refused* take-from is not
  ported.
- **Examine:** 3.7/3.8 examines have no bare-verb exit (unmeasured
  corners). run370's sit/stand/lie has no location test (42AEC8,
  unported).
- **Ask-topic overwrite** is unmeasured.
- **Put:**
  - 3.7 static container `open`;
  - 3.7 bare take from a held container;
  - `put X in Y` where X names nothing and Y is a bad container;
  - a supporter that is neither held nor static nor a container;
  - a static container;
  - `except` forms;
  - `put all in <nothing>`.
- **Pre-4.0 room-name alt walk** is unmeasured.
- **run380 442F5D** (the catch-all speaks for the first present, seen
  object) is unmeasured.
- **`clear`** at run370/380/390 is unmeasured.

### Harness and corpus

- **`house`** is comparable only as `House_sober.taf` with
  `cmdfile_house_sober.txt`. That run (`Adrift_128_housesober.txt`) is
  identical on every turn. The original House cannot be driven, because of
  the `%drunk%` stack overflow described below.
- **`motion`:** the minigame's keypresses are its turns. Re-cut the feed
  before reading anything into the row (`Adrift_425`).
- **`mould`** needs an adaptive driver: the imp fight is seed-locked.
- **`sophie`** is measured for its first 50 commands only. `sophie_comp`
  and `plague` desync early.
- **Permanently unmeasurable:**
  - `dreamquest`: run400 cannot load a task with an empty Command vector.
  - `great.taf` car chase.
  - `to_hell_and_beyond` assisted rows: Scarier-only by design.
- **Deferred for rollable events on the route:** `Colony`,
  `Locked_door_with_water_trap`.

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
  - 4.0 task matching is verb-literal. `*`, `[..]` and `{..}` patterns
    compare binary. A rebuilt line keeps its capitals, so a pre-match can
    fail to dispatch and fall to DontUnderstand. `[4.0]` hcw T162
    (b6d2f4f1f, 5fc9ef8d1)
  - The task pre-matcher is restriction-aware. The fallback pass wants a
    FailMessage or a spent RepeatText, and a fallback hit is silent. `[4.0]`
    House task 60 (c38297f1e, 5fc9ef8d1)
  - A trailing space in an all-literal task command must be typed. sommeril
    `get placemat ` (093a12d5e)
  - The SYNONYM table is sequential whole-string rewrites. Vardock
- **Word rules:**
  - `take` becomes `get` before parsing. `[3.8]` great
  - `z` means wait only from 3.90. cave
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
  - The catch-all tests the line-top object's presence after the task, and
    that answer is a turn. `[4.0]` seaside `do form` (24dcc8e5a)
  - A line a task answered that names a term two present NPCs share is not
    a turn. `[4.0]` sun_empire, salutations (8c4d3260b)
- **Administrative turns and the counter:**
  - An NPC examine and a nothing-found examine are administrative turns;
    so is `read` via examines. `[4.0]` EV14-16, house T150
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
- **463640 is the 4.0 noun resolver.**
  - Scoring: Short whole word +1, first alias +1, +1 per Prefix word, and an
    empty Prefix counts as `a`.
  - With two objects named, a tie gives the game's DontUnderstand.
  - NPCs are never candidates. `[4.0]` House throw (0743cefef)
  - therest's absent-seen clause scores every object the line names.
    `[4.0]` warlord, house doors (5662e7397)
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
  - 3.7/3.8 co() has its own prompt, which replaces the output while the
    action still happens. `[<3.9]` mikes
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
- **Darkness** is the condition AND HideObjects, and it gates the seen
  flag. A dark examine answers "can't see X very clearly." `[<4.0]` p39DARK
  (9aff3fda9)
- **Absent-object refusals:**
  - co() matches anywhere; the handlers refuse with Prefix + Short, and in
    takes() the last match speaks. `[<3.9]` p37EXAM/p38EXAM (b526c013b)
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
  - `turn on/off` refusals append the particle. `[4.0]` thepkgirl
  - `kiss` answers "I'm not sure she would appreciate that!". `[3.9+]`
    (07bbd664d)
  - sit, stand and lie need the object on the room floor. `[3.8+]` house
    T124 (4e7df6dff)

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
- **A 3.8 in/on object with an unset parent** goes in the first container.
  (5cf3d7059)
- **The take-from handler's own answers.** The 3.9 insides() decision
  procedure; `empty` is take-all-from in 4.0 only. p39DARK/p4TFROM
  (2ab1a7c5d)

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
  - Pronoun echoes use round brackets, but the article rule is 4.0 only.
  - The examine-self full stop is 3.9+.
- **Other formatting:**
  - `isare()` is exact and case-sensitive, and the loader fills an empty
    Prefix with "a". yeh (496c115f2)
  - 4.0 room names take every matching alt's Changed. togetyou (fee19ae2a)
  - The multi-take line comes before the earlier task text. `[4.0]`
    fullcircle T43 (b6d2f4f1f)
  - The (Getting off X first) bracket line prints on its own line.
    Monsters_r2
  - The score summary prints after every EndGame; NotifyScore defaults to
    OFF.
