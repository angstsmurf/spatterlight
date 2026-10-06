# ScottFree headless test harness

- `Makefile.headless` — builds `scott_hl`, a stdio (CheapGlk) build of the
  interpreter. `printf 'look\nquit\n' | ./scott_hl "<game file>"`.
- `make -f Makefile.headless check` — the regression suite. Replays scripted
  games and diffs the transcripts against golden files:
  - `test/regression.dat` — a hand-written plaintext Scott database that
    exercises the TAKE/DROP ALL skip loop, the `mr.`/`dr.` title parser and the
    last-dictionary-word lookup in a few turns.
  - `UITests/.../adv01.dat` + `ScottFree command script.txt` — the full
    157-command Adventureland walkthrough, which wins 13/13 treasures.
  - `test/parser/parser.dat` — a second hand-written database (three rooms,
    five portable items, one synonym each for a verb and a noun) for the parser
    alone. Four scripts, goldens in `test/parser/expected/`:
    - `all.txt` — TAKE/DROP ALL, the carry limit, ALL EXCEPT/BUT with one or
      several items, by synonym, with an unknown word and with nothing after
      it, ALL in an empty room, and ALL with a verb that does not take it.
    - `chain.txt` — several commands on a line, split by `,` `.` `;` `and`
      `then`, and what happens to the rest of a line after an error.
    - `it.txt` — IT as the noun of the last command: before there is one,
      across lines and within one, with filler words around it, after a
      direction, a synonym, an error and an ALL, in ALL EXCEPT IT, and where
      it is not a noun at all.
    - `words.txt` — the filler words (`the`, `a`, `my`, `quickly`, `room`, ...)
      before and between verb and noun, and everything after the noun, which
      is ignored up to the next command, known word or not, as the original
      interpreters do (`LOOK UP AT THE GREMLIN` is `LOOK UP`). Unknown words
      where the verb or the noun should be are still reported.

    After a deliberate change to the parser, look at the diff and regenerate:
    `SCOTT_SCRIPT_KEYS=1 ./scott_hl test/parser/parser.dat < test/parser/all.txt
    2>/dev/null | grep -v 'Glk library error' > test/parser/expected/all.txt`.
- `make -f Makefile.headless scott_death_test` — loads a real game, jumps the
  player into `MyLoc = GameHeader.NumRooms` (the death/limbo room) and calls
  `Look()`. Run under ASan with a non-zero malloc fill so an uninitialised room
  pointer is a wild pointer rather than a lucky NULL:
  `ASAN_OPTIONS=malloc_fill_byte=170:max_malloc_fill_size=1073741824 \
   ./scott_death_test "<Seas of Blood.z80>"`.
- `make -f Makefile.headless scott_all_audit` — `test/all_audit.c`, which
  reports whether a database contains implicit actions that relocate
  ALL-eligible items.
- `make -f Makefile.headless formatcheck` — the disk and tape image loader
  tests: every container format `DetectGame()` reads, fingerprinted with
  `test/format_probe.c` and compared with goldens. The images are not in the
  repository; see `test/formats/README.md`.

## DETERMINISM is mandatory

`test/headless_stubs.c` forces `gli_determinism`, so the engine takes its
fixed-seed RNG path. Without it the headless build seeds from the clock (as the
app does with the Determinism theme option off) and games with random events
replay differently every run — two runs of the *same* binary diverge, which
silently invalidates any before/after diff. Anything that diffs transcripts
must keep this on.

## HARNESS LIMITATION — combat is NOT exercised

Seas of Blood's dice combat (`roll_dice` in `ai_uk/seas_of_blood.c`) is driven
by Glk timer events, and `HitEnter()` / the dice loop wait on char events.
CheapGlk delivers neither (`glk_request_timer_events` is a no-op), so a
line-input replay stalls at the first battle (`ATTACK BARGE`, ~command 13 of
405) at the `<HIT ENTER>` prompt and EOFs. Combat must be tested in the real
Glk app.

When diffing two builds, verify the two binaries really differ before trusting
the result — a stale build once made a "no change" result out of an unchanged
binary.
