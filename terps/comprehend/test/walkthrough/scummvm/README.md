# Walkthroughs under the ScummVM port

This folder plays the walkthrough scripts in `../scripts` against the
Comprehend port in ScummVM's Glk engine. The port lives on the
`spatterlight-comprehend-port` branch of angstsmurf/scummvm.

    ./run_scummvm_walkthroughs.sh ~/my-CM-fork/scummvm-comprehend

The run never touches the checkout you pass it. It makes a detached worktree of
the checkout's HEAD in `work/wt`, applies `hooks.patch` there, and builds it with
the null backend and AddressSanitizer:

    ./configure --backend=null --disable-all-engines --enable-engine=glk \
        --disable-detection-full --enable-debug --enable-asan

When `work/wt/scummvm` already exists, the run reuses it. Delete `work/` to
rebuild, for example after the branch moves. The worktree is registered in the
checkout, so after deleting `work/` also run `git worktree prune` in the
checkout.

The table prints `PASS` or `FAIL` for each game, plus a count of ASan errors.
The script exits with status 0 only if every game passes with no ASan errors.
Each game's transcript is in `work/run/<label>/out`.

## The hook

`hooks.patch` changes `engines/glk/comprehend/comprehend_io.cpp` so that, when
`COMPREHEND_SCRIPT` names a file:

- `readLine` takes its next line from that file and echoes it to stdout;
- `readChar` answers every key prompt with Return;
- `putBottom` and `putBottomUni` copy the buffer-window text to stdout;
- the engine quits at the end of the script.

The runner removes comment lines and blank lines before passing a script in.

The patch uses stdio, which ScummVM forbids, so it defines
`FORBIDDEN_SYMBOL_ALLOW_ALL`. That is why it lives here and not in the port.

## Seed and timing

ScummVM's RNG is not Spatterlight's, so the scripts only win at chosen seeds.
The runner uses `--random-seed=1234` by default; set `SEED` to try others. The
scripts were tuned to win in both interpreters (Spatterlight 7610615b4).

Two tuning notes:

- **Transylvania's werewolf** has a 25% chance per turn of appearing in its
  rooms. Once it appears, the next command must leave the room or shoot it.
  `SHOOT WEREWOLF` and `SHOW CROSS` do nothing harmful when their target is
  absent, so the script repeats them to stop depending on one exact RNG draw.
  The wagon/coffin step can't be made RNG-free (the gun isn't loaded there),
  so its cycle count (74) stays tuned.
- **Coveted Mirror** repeats `GET SHADOW` the same way. With 10 tries, it wins
  at 19 of seeds 1–20.

Each run gets a fresh rc file containing `enable_unsupported_game_warning=false`.
Without that setting, the warning dialog hangs the null backend. Each run also
gets its own save path.

Under ASan, the slow-draw games take several minutes. `JOBS` (default 5) caps how
many run at once, and each run is killed after 900 s. More than about 6 runs at
a time pushes the slowest ones past that limit.
