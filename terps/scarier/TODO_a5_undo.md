# TODO: Add undo support to the ADRIFT 5 engine

## Background

The ADRIFT 5 engine (`a5run.cpp`) already has full save/restore serialization:

- `a5run_save(run, &length)` — serialises the complete mutable runtime to a
  heap-allocated XML blob (RNG state, entity arrays, task/seen/override/look
  sets, conversation state, etc.)
- `a5run_restore(run, buffer, len)` — applies a blob back to the same run,
  correctly resetting all accumulating fields first

The ADRIFT 4 path in `sclibrar.cpp` (`lib_cmd_undo`) copies the game-state
struct from `game->undo` (snapshotted before each turn in `scrunner.cpp`'s
`run_save_undo`).  The ADRIFT 5 path has no equivalent snapshot or verb handler.

The engine already tells the player undo is available: `emit_endgame()`
(`a5run.cpp:5069`) prints "Would you like to restart, restore a saved game,
quit or **undo the last command**?" — so the omission is currently a lie.

---

## What to do

### 1. Add an undo slot to `a5_run_t`

In `a5run.cpp`, `struct a5_run_s` (line 198), add two fields:

```c
char  *undo_blob;   /* serialised snapshot from before the last turn, or NULL */
size_t undo_len;
```

Initialise both to `NULL` / `0` in `a5run_new()`.

In `a5run_free()` (line 486), add `free(run->undo_blob)`.

### 2. Snapshot before each turn in `gsc_a5_main_game_loop()`

In `os_glk.cpp`, the inner `for (;;)` loop (around line 4563) calls
`gsc_a5_read_line()` then `a5run_input()`.

Just before the `a5run_input(run, input)` call (line 4606), add a snapshot:

```c
/* Snapshot state for single-level undo. */
free (run->undo_blob);
run->undo_blob = a5run_save (run, &run->undo_len);
```

This captures the pre-turn state including RNG, so an undo followed by the
same command replays identically.

Do NOT snapshot inside `a5run_input()` itself — the function already
increments `st->turns` on entry (line 5707) and would snapshot mid-turn state.

### 3. Handle "undo" in the main loop's command dispatch

In `os_glk.cpp`, between the "restore" block and the `a5run_input()` call,
add:

```c
if (gsc_a5_match_command (input, "undo"))
  {
    if (run->undo_blob != NULL)
      {
        a5run_restore (run, run->undo_blob, run->undo_len);
        /* Consume the snapshot so double-undo does not revert to the same
           turn twice. */
        free (run->undo_blob);
        run->undo_blob = NULL;
        run->undo_len = 0;
        gsc_a5_put_string ("The previous turn has been undone.\n");
      }
    else
      gsc_a5_put_string ("You can't undo what hasn't been done.\n");
    continue;
  }
```

`a5run_restore()` resets `st->game_over` to its saved value, so undoing a
fatal move correctly clears the game-over flag.

### 4. Handle "undo" in the endgame loop

The endgame loop (os_glk.cpp around line 4615) currently only accepts "restart"
and "quit".  The prompt already mentions undo, so the loop should honour it:

```c
if (gsc_a5_match_command (input, "undo"))
  {
    if (run->undo_blob != NULL)
      {
        a5run_restore (run, run->undo_blob, run->undo_len);
        free (run->undo_blob);
        run->undo_blob = NULL;
        run->undo_len = 0;
        gsc_a5_put_string ("The previous turn has been undone.\n");
        break;   /* exit the endgame inner loop; outer loop reads next input */
      }
    else
      gsc_a5_put_string ("Sorry, no undo is available.\n");
  }
```

### 5. Expose `undo_blob`/`undo_len` from the header if needed

`a5_run_t` is an opaque struct (`typedef struct a5_run_s a5_run_t`); os_glk.cpp
includes a5run.h but accesses the struct through the API.  The snapshot fields
need to be accessible from os_glk.cpp, so either:

- **Option A** (simplest): add `a5run_snapshot(run)` and `a5run_undo(run)`
  API calls to a5run.h/a5run.cpp that wrap the save+store / restore+clear
  logic, keeping the struct opaque.
- **Option B**: expose `undo_blob`/`undo_len` directly in the header (already
  done for `a5run_state()`/`a5run_is_over()` inline accessors).

Option A is cleaner if the struct needs to stay opaque.

---

## Limitations

- **Single-level undo only** — the ADRIFT 4 path has a 16-slot memo ring
  (`MEMO_UNDO_TABLE_SIZE` in `scmemos.cpp`), but ADRIFT 5 games rarely expect
  multi-level undo and the XML blob is large enough that a ring would be
  expensive.  A single slot matches what most players expect and what
  FrankenDrift implements.

- **Disambiguation and remembered-verb state** — `run->amb_active`,
  `run->amb_keys`, `run->remembered_verb` live in `a5_run_t` outside
  `a5_state_t` and are not serialised by `a5run_save()`.  Undoing during an
  active disambiguation round will clear the disambiguation, which is
  acceptable.

- **Verb matching** — the "undo" check uses `gsc_a5_match_command()` (exact
  case-insensitive word), the same as "save"/"restore"/"restart".  ADRIFT 5
  games may also define their own undo synonym tasks; those would pass through
  to `a5run_input()` and do nothing useful.  The intercept in step 3 must come
  before `a5run_input()` to catch the common case.

---

## Verification

After implementing, test:

1. Play a few turns, type UNDO, verify previous-turn state is restored.
2. Type UNDO immediately after game start — should print "can't undo".
3. Double UNDO (should undo once then fail on second attempt, since the snapshot
   is consumed on undo).
4. Die, then UNDO from the endgame "restart/restore/quit/undo" prompt.
5. Run the full ADRIFT 5 walkthrough corpus (`make -f Makefile.headless test`
   in `terps/scarier/`) — undo is not exercised by the walkthroughs so no
   regressions are expected, but the snapshot overhead per turn should be
   measured: `a5run_save()` allocates and serialises every turn.
