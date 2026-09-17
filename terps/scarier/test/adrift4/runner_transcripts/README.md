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
