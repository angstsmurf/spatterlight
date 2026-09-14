# Runner transcripts for every wired walkthrough

One transcript per row of `harness/run_v4_walkthroughs.sh`. Each was
produced by the real Windows ADRIFT Runner for the game's file version,
running under Wine with the vbrng xoshiro hook. The feed, seed and popup
answers are the ones the current golden was blessed with, so each file is
the Runner's side of that golden.

- `<tag>.txt`: the live transcript from run400x/run390x (4.00/3.90).
- `<tag>.rtf`: the Save Transcript output from run380x/run370x (3.80/3.70).
  Read it with `textutil -convert txt -stdout`. The 3.7/3.8 Runners cannot
  save after the last command, so that command's output is missing, and a
  death or end-game modal wipes the scrollback.
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
  command of the feed. Sometimes it adds "apart from" whitespace or the
  Runner's own `[Press any key to end]`.
- **"0 differing turn(s), 1 lost command(s)"** on a 3.70/3.80 row: the
  last command's output is missing from the `.rtf` (see above), and
  nothing else differs. A missing win marker on those rows has the same
  cause.
- **Lost commands at the end of the feed** (`z`, `wait`, `quit`): the
  Runner's game had already ended, so its ending differs from Scarier's.
  Read the differing turns before the losses.
- **Any other differing turn** is a lead to classify with the workflow in
  `notes/WINE-TRANSCRIPTS-TODO.md`. It may be a capture artefact, the
  harness, the RNG or the engine. `compare/<tag>.txt` shows every
  difference.
