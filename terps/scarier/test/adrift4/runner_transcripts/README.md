# Runner transcripts for every wired walkthrough

One transcript per row of `harness/run_v4_walkthroughs.sh`. Each was
produced by the real Windows ADRIFT Runner for the game's file version,
running under Wine with the vbrng xoshiro hook. The feed, seed and popup
answers are the ones the current golden was blessed with, so each file is
the Runner's side of that golden.

- `<tag>.txt`: the live transcript from run400x/run390x (4.00/3.90).
- `<tag>.rtf`: the Save Transcript output from run380x/run370x (3.80/3.70).
  Read it with `textutil -convert txt -stdout`. The 3.7/3.8 Runners cannot
  save after the last command, so that command's output is missing from what
  they write, and a death or end-game modal wipes the scrollback. All 19 of
  those rows have had that last block **grafted** back on from the Runner's
  own window (see "Grafted tails"); their `source` ends `+tail`.
- `manifest.tsv` has one line per row:
  - where the file came from;
  - the file version and the Runner exe;
  - the seed, `PRE` (startup keypresses) and popup answers;
  - the row's Scarier env;
  - whether the row's win marker appears in the transcript;
  - the verdict of `harness/compare_wine_transcript.py` against a Scarier
    replay of the same feed.
- `compare/<tag>.txt` holds the full compare report for every row that
  isn't identical.
- `plan.tsv` holds the per-row inputs, including how many xoshiro draws the
  route makes under Scarier.
- The games whose goldens are gitignored for explicit text keep their
  transcripts and compare reports local too (see `../.gitignore`). Their
  manifest lines stay. `collect` prints `NOT IGNORED` for any such file the
  ignore list misses.

## How a row is driven

`harness/runner_transcripts.py` does all of it:

    python3 harness/runner_transcripts.py plan
    python3 harness/runner_transcripts.py harvest
    python3 harness/runner_transcripts.py jobs      # -> ~/adrift-battle/runner/wine/xoshiro_jobs_runner_transcripts.txt
    cd ~/adrift-battle/runner/wine
    LOAD_SLEEP=600 VBRNG_TRACE_OFF=1 ./xoshiro_par.sh xoshiro_jobs_runner_transcripts.txt 5
    cd -; python3 harness/runner_transcripts.py collect

- **Runner:** chosen by .taf header bytes 8-10.
  - `93 45 3e` = 4.00 → run400x
  - `94 45 37` = 3.90 → run390x
  - `94 45 36` = 3.80 → run380x
  - `94 45 39` = 3.70 → run370x
- **Seed:** `VBRNG_SEED` is the row's `SCR_SEED`, or 1234 where the row has
  none. 1234 is the default for both Scarier's xoshiro and vbrng.
- **Feed:** `harness/make_wine_cmdfile.py` under `SCR_RNG=xoshiro`, which is
  the harness's own default. Its blank lines and `#sleep`s answer the
  `<waitkey>`/`<wait>` pauses that `SCR_SKIP_WAITKEY` skips in Scarier.
  Startup pauses go to `PRE`.
- **Popup answers:** the golden's own answers to the built-in name and gender
  questions. The Runner asks those in dialogs at load, not at the prompt.
- **Env with no Runner equivalent:** `SCR_ASSUME_COMBAT` and
  `SCR_ASSUME_MOVES` exist only in Scarier. They are applied to the
  comparison, not to the drive.
- **`SCR_ASSUME_PATCHES` is different: it is a data change**, so it can be
  driven. `harness/make_patched_taf.py` bakes the `sctafpar.cpp` patch table's
  own edits for that game into a copy of the .taf under `games/patched/`
  (gitignored, rebuilt on demand from the table), and then the drive *and* the
  replay both play that file with the switch off -- so the Runner is reading
  the same broken-field fixes the engine would have applied in memory. The
  copy is a field rewrite only: no task, object, room, NPC or variable is
  added or removed, so the Runner loads it like any other file.
  - On a **3.90/3.80** game a patch that changes the file's byte count moves
    one random draw. The 3.x codec's LCG is advanced once per file byte and
    `vbrnd(load)` reads it, which seeds random event start times. The Runner
    reads the same patched bytes, so Runner and Scarier still agree; it is the
    *golden* (original file + `SCR_ASSUME_PATCHES`) that can diverge. Two rows
    do: `taot3_patched` and `spirits_flight_patched` differ from their goldens
    in random-event timing while still reaching their win markers.
- **A fresh drive can be worse than the one it replaces.** Re-driving a row
  is not free: `hyper_b_s` came back truncated where the archived capture was
  whole. Compare before you keep one, and if the new capture is worse,
  `git checkout -- runner_transcripts/<tag>.<ext>`, hand-restore that row's
  `source` and `win_marker` (a `recompare` rewrites neither), and recompare.

## Grafted tails

The 3.7/3.8 `.rtf` files stop one command short, so the winning move's output
lives only in the Runner's window. Read the window back exactly -- no
screenshot, no OCR: `DUMP_SCROLLBACK=<file>` makes `drv/drive.exe` ask the
RichTextBox for its own text (`WM_GETTEXTLENGTH` + `WM_GETTEXT`) after the
feed, and `dump_par.sh` next to `xoshiro_par.sh` drives a whole job file that
way.

    python3 harness/runner_transcripts.py dumpjobs   # -> ~/adrift-battle/runner/wine/dump_jobs.txt
    cd ~/adrift-battle/runner/wine && ./dump_par.sh dump_jobs.txt 5
    cd -; python3 harness/graft_scrollback_tail.py --batch ~/adrift-battle/runner/wine/par/dumps
    python3 harness/graft_scrollback_tail.py --batch <dir> --apply <tags...>
    python3 harness/runner_transcripts.py recompare <tags...>

`graft_scrollback_tail.py` appends nothing unless the archived transcript is a
character-for-character prefix of the dump. That check is the point: it is
what proves the fresh drive took the same route as the capture being extended,
so the appended block belongs to this transcript and not to some other run.
A grafted row gets `+tail` on its `source` and a recomputed `win_marker`.

Run on all 19 `.rtf` rows on 2026-09-17: every one gained its last block, none
still loses a command, all now have their win marker, and 16 compare clean --
ten identical on every turn, six apart from whitespace only. `haunt` gained a
real differing turn the missing block had been hiding; `alices_restaurant` and
`marooned` kept the differences they already had.

**The window is not the only thing a dump run brings back.** `dump_par.sh`
drives with `NOKILL=1`, so the Runner outlives the feed instead of being
terminated the moment `drive.exe`'s 1.5 s size-stable wait is satisfied --
and a 4.00 game whose ending lands after that wait then gets to finish
writing it. `suzypowers` is the case: run400 clears its window before
printing the ending, so the scrollback dump held only the last 1 KB and
nothing to graft, while the dump run's own transcript file was the archived
capture byte for byte **plus** the ending block (2026-09-17). When a dump
comes back short, diff the run's `Dump_<n>_<tag>.txt` against the archive
too; a byte-exact prefix is the same proof of route, and the tail appends the
same way. Checked across all 34 dumps from that day, `suzypowers` was the
only row it recovered: the other 15 live transcripts came back identical.

**It recovers nothing else on 3.9/4.0**, whose transcript is written live. Of
the 14 rows whose losses were at the end of the feed, nine dumps held nothing
the archived `.txt` lacked and five were empty -- the game's ending had
already closed the window. An empty dump is the answer, not a failure.

## Reused captures

`harvest` takes an existing transcript instead of a fresh drive only when
it compares identical against the feed rebuilt from the current golden, and
one of these holds:

- **xoshiro:** a Claude session log records `xoshiro_par.sh` driving it with
  the row's current seed.
- **native-0-draws:** it is in the 2026-09-08 native-RNG corpus
  (`~/adrift-battle/runner/wine/transcripts_v4_corpus_2026-09-08/`), and the
  golden's route makes no random draws at all, so the seed cannot reach the
  text.

The `source` column in the manifest says which.

## Patched rows

The 21 `*_patched` rows were driven on 2026-09-26 (`games/patched/*.taf`, feed
and seed from the patched golden). **All 21 reach their win marker in the real
Runner** -- twenty on that drive, and `illegalsocks_patched` on the follow-up
probe drive of 2026-09-27 that settled its weapon phrasing (below;
Adrift_305_socks6.txt, the same route with `with sword2` for `with awesome
sword`, which is now what the golden plays). That is the point of the
exercise: the patch table's field edits are not a Scarier convenience, they fix
the release in any engine that reads the file. Fourteen rows compare identical
(two of those apart from whitespace); the seven that differ do so in classes
that have nothing to do with the patch:

- Five (`spirits_flight`, `tenebraesemper`, `villains_and_kings`, `lockedout`,
  `liqid`) differ only in the Runner text accidents that `4e8ff9752` reverted
  on purpose -- the literal `" is "` in a multi-object "Inside ... is", the
  Runner's literal `Prefix` case ("You take A Flashlight.", "You stab An evil
  witch"), "Which battery." for "Which battery?", "Also here are" for "You can
  also see". Their faithful rows show the same turns when re-compared today;
  the `identical` verdicts stored against them predate that commit.
- `house` shows the same two classes its faithful row already shows (empty
  Runner output on plain movement turns, and the cold-event beats landing a
  turn apart).
- `illegalsocks_patched` was the one row whose marker the 2026-09-26 drive
  missed, and
  it is worth reading in full, because **the patch itself is confirmed there**:
  on the patched file `attack dr myanus hurts` makes run400 answer "What do you
  want to attack Dr Myanus Hurts with?", so the Doctor is a legal target at
  last, which is exactly what taking the "." out of his Name was for. Only the
  weapon phrasing was wrong. Re-probed 2026-09-27, `attack dr myanus hurts with
  sword2` -- the Awesome Sword's unique Alias -- strikes in run400 in one turn
  and prints this row's marker verbatim (`status` then reads "You are wielding
  Awesome Sword", hit 28 (20); Adrift_305_socks6.txt line 119), so the golden
  now uses that command and the row's marker is Runner-measured text.

  The earlier reading of this row ("neither sword is in the player's scope",
  from "You are wielding nothing" and an "x awesome sword" that seemed to find
  nothing) was **wrong**, and the scope lead it suggested does not exist: both
  swords are carried at the Battle Dome in both engines (`i` agrees line for
  line), "wielding nothing" is simply what either engine prints before a first
  successful strike, and run400's `x awesome sword` answers "Which Sword." --
  it finds two. Both of Scarier's battle paths already require
  `OBJ_HELD_PLAYER` anyway.

  What actually differs is **4.0 weapon-term resolution**, and the probe series
  `harness/make_400_battlewith{2,3,4,5}probe.py` has now measured the rule (68
  cases; the fifth generator's docstring is the full record). Two of its parts
  decide this row. First, run400 compares an object's **Short case-sensitively**
  to the lower-cased input, where it compares Aliases and Prefix words without
  regard to case, so a Short stored capitalised can never be typed -- and
  Illegal Socks capitalises everything. Both swords have the Short "Sword", so
  neither answers to "sword" that way; only Cool Sword's Alias "Sword" does,
  Awesome Sword's "Sword2" does not, and a Prefix match alone never makes a
  candidate. Second, the winner has to pass an ambiguity gate, and Cool Sword
  fails it: it matched through an Alias that is another object's name. So
  `attack dr myanus hurts with awesome sword` binds nothing, and the catch-all
  that follows comes from a separate, ordinary name resolver (Short 1, first
  matching Alias +1, +1 per matching Prefix word, a Prefix enough on its own)
  where the two swords tie at 1 -- a tie names no object, which is why the
  message is the *character* catch-all "I don't understand what you want to do
  with Dr Myanus Hurts." and takes no turn (Adrift_306_socks7.txt; the same
  against the one-word Quzar, so duplicate NPC names are not in it). A strict
  winner there that is not a usable weapon gives the *object* catch-all instead
  ("... what you want me to do with Full Suit of Armor."), also without a turn.
  The capitalisation half is independently visible outside the battle path:
  `with armor` finds no object at all although the Leather Armor is worn, while
  `with full suit of armor` reaches the Full Suit through its Prefix words
  (Adrift_305_socks6.txt). `with sword2` is therefore the only phrasing that
  reaches the Awesome Sword, which is what this row's golden plays.

  Scarier asks "Which Sword?  Cool Sword or Awesome Sword?" and swings on the
  repeat, because it matches the Short without regard to case and has no such
  gate: `lib_battle_scan_with()`'s walk-every-named-object/last-weapon-wins is
  measured on thesorc, a **3.90** game, and over-fires at 4.0. That is kept as
  a **deliberate deviation**, documented in the comment above
  `lib_battle_scan_with()`: the 4.0 rule is a parser accident rather than a
  design -- a Short the .taf capitalises can never be typed, and an object
  whose Alias is its neighbour's Short can never be wielded -- so honouring it
  would refuse commands the game was written to accept. The divergence is
  one-directional, Scarier accepting strictly more phrasings and never fewer,
  and this row is the only place in the corpus it shows.

## Rows without a transcript

- `dreamquest`: run400 never opens a game window for Dream Quest. Its
  Empty-Command task stops the load, so the row cannot be measured.

## Reading the verdicts

- **"identical on every turn"**: the Runner and Scarier agree on every
  command of the feed. Sometimes it adds "apart from" whitespace. It used
  to add "apart from the Runner's own `[Press any key to end]`" on 238 of
  the 427 rows; Scarier now writes that prompt itself under `os_ansi`, so
  the clause is rare.
- **"0 differing turn(s), 1 lost command(s)"** on a 3.70/3.80 row: the
  last command's output was missing from the `.rtf` (see above). That
  class is closed -- all 19 rows have been grafted, so a lost command on a
  3.70/3.80 row now means something else.
- **Lost commands at the end of the feed** (`z`, `wait`, `quit`) on a
  3.90/4.00 row: the Runner's game had already ended and it is sitting on
  its `[Press any key to end]` wait, which eats the next keystroke. Read
  the differing turns before the losses. Of the 15 rows re-driven on
  2026-09-17 every tail-loss row came back with the same count, and reading
  the Runner's window itself the same day recovered no text for any of
  them.
- **A lost command in the middle of the feed** is a different animal: the
  Runner really did miss that line, so the capture is suspect. Re-drive it
  before reading anything into the turns around it.
- **Any other differing turn** is a lead to classify with the workflow in
  `notes/WINE-TRANSCRIPTS-TODO.md`. It may be a capture artefact, the
  harness, the RNG or the engine. `compare/<tag>.txt` shows every
  difference.
