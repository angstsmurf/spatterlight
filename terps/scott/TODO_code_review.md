# ScottFree (terps/scott) code-review findings — 2026-07-13

High-effort review of the whole `terps/scott` tree (core engine + `ai_uk` /
`saga` / `ti994a` loaders). Ten findings, ranked most-severe first. **All ten
are now fixed**, plus one more (11) found while building the regression
fixture. The death-path memory bug was reproduced and confirmed fixed with a
dedicated ASan reproducer; findings 6, 9 and 10 were reproduced in a synthetic
database and confirmed fixed by transcript diff (see "Verification").

## Verification harness

- `Makefile.headless` — builds `scott_hl`, a stdio (CheapGlk) build of the
  interpreter. `printf 'look\nquit\n' | ./scott_hl "<game file>"`.
- `make -f Makefile.headless check` — the regression suite. Replays two scripted
  games and diffs the transcript against a golden file:
  - `test/regression.dat` — a hand-written plaintext Scott database that
    reproduces findings 6, 9 and 10 in a few turns (see below).
  - `UITests/.../adv01.dat` + `ScottFree command script.txt` — the full
    157-command Adventureland walkthrough, which wins 13/13 treasures.
- `make -f Makefile.headless scott_death_test` — the finding-3 reproducer. Loads
  a real game, jumps the player into `MyLoc = GameHeader.NumRooms` (the
  death/limbo room) and calls `Look()`. Run under ASan with a non-zero malloc
  fill so an uninitialised room pointer is a wild pointer rather than a lucky
  NULL:
  `ASAN_OPTIONS=malloc_fill_byte=170:max_malloc_fill_size=1073741824 \
   ./scott_death_test "<Seas of Blood.z80>"`.

DETERMINISM: `test/headless_stubs.c` forces `gli_determinism`, so the engine
takes its fixed-seed RNG path. Without it the headless build seeds from the
clock (as the app does with the Determinism theme option off) and games with
random events replay differently every run — two runs of the *same* binary
diverge, which silently invalidates any before/after diff. Anything that diffs
transcripts must keep this on.

HARNESS LIMITATION — combat is NOT exercised. Seas of Blood's dice combat
(`roll_dice` in ai_uk/seas_of_blood.c) is driven by Glk timer events, and
`HitEnter()` / the dice loop wait on char events. CheapGlk delivers neither
(`glk_request_timer_events` is a no-op), so a line-input replay stalls at the
first battle (`ATTACK BARGE`, ~command 13 of 405) at the `<HIT ENTER>` prompt
and EOFs. Combat must be tested in the real Glk app.

## Regression evidence for this pass (findings 6, 9, 10, 11)

Two builds — pristine HEAD vs fixed — were each run over the corpus below and
the transcripts diffed. (Verify the two binaries really differ before trusting
such a diff: a stale build once made a "no change" result out of an unchanged
binary.)

- Seven real walkthroughs, byte-identical: Adventureland (plaintext, wins),
  Hulk (ZX .tap, 1727 lines), Gremlins (ZX + C64 — wins), Sorcerer of Claymorgue
  Castle (Apple II .woz), The Count (C64), Voodoo Castle (C64).
- 16 games × 2 scripts (one of them a TAKE ALL / DROP ALL / EXCEPT + parser
  script) spanning ZX (.z80), C64 (.d64), Apple II (.dsk/.woz), Atari 8-bit
  (.atr), TI-99/4A (.rpk/.dsk) and plaintext (.dat). The *only* differences
  anywhere are the intended ones from finding 9: `"dr"` → `"dr."` and `"mr"` →
  `"mr."` in the "I don't know what a ... is" message.
- ASan + UBSan clean on plaintext, C64, TI-99 and ZX games and on the synthetic
  database. Note ASan cannot see finding 11 on its own: item 9 of a short
  database lands inside a neighbouring heap chunk, not a redzone.

---

## FIXED

### 1. `ReadString()` stack buffer overflow — scott.c:~382 (FIXED)
The plaintext-database reader copied a quoted string into `char tmp[1024]` with
no bound on the write index; a string longer than the buffer smashes the stack,
with length and content fully controlled by the file. **Fix:** stop writing once
`ct` reaches `READSTRING_BUFFER_SIZE - 1` while still consuming to the closing
quote so the file stays in sync.

### 2. `PerformLine()` param array out-of-bounds — scott_actions.c:~333 (FIXED)
`int param[NUM_CONDITIONS]` (5 slots) is filled by the condition phase but read
by the opcode phase, where four opcodes can each pop two params (up to 8 reads).
Reads past index 5 pull stack garbage that is then used directly as an `Items[]`
index (OOB read → OOB write). **Fix:** size the array to `NUM_OPCODES * 2` and
zero-init, so over-reads yield a defined item/room 0; guard the condition-phase
write too.

### 3. Compressed room descriptions off-by-one — detect_game.c:~952 (FIXED)
The compressed-text room loop used `ct < GameHeader.NumRooms`, while every other
room loop uses `<=`. It left `Rooms[NumRooms].Text` uninitialised — and
`MyLoc = GameHeader.NumRooms` is exactly where `PlayerIsDead()` and the dark-move
death send the player. First death → `Look()` dereferences a wild pointer.
Reproduced with `death_test` (SEGV at scott_display.c:498 on the unfixed engine;
clean on the fixed one). Affects Seas of Blood (ZX + C64). **Fix:** use `<=`
(room 83 decompresses to valid data — "lagash" — confirmed in-game), *and* switch
`AllocateGameData()` to `MemCalloc` so any field a loader leaves unset reads back
as NULL/0, which the engine already guards against.

### 4. TAKE ALL / DROP ALL skips the last item — parser.c:~1186 (FIXED)
`CreateAllCommands()` looped `i < GameHeader.NumItems`, so the highest-indexed
item was never eligible for ALL (the EXCEPT loop just above already used `<=`).
**Fix:** `<=`.

### 5. `EXCEPT` falls through into `FLICKER` — scott_meta.c:~309 (FIXED)
`case EXCEPT:` had no `break`, so a bare "except"/"but"/"ausser" ran
`FreeCommands()` and then fell into `case FLICKER`, silently switching the
flicker option on and reporting success. **Fix:** add `break;` so it drops to the
"not handled" path and prints "I don't understand". Verified: bare "but" now
rejected; `#flicker` still toggles.

### 6. TAKE/DROP ALL skip loop never re-reads `item` — scott_actions.c:~971 (FIXED — but LATENT, see below)
The loop that skips already-taken/dropped items during ALL advanced
`CurrentCommand` but never re-read `item = CurrentCommand->item`, so its guard
could not change: the first item that had moved fast-forwarded straight to the
`LASTALL` node and returned, discarding the rest of the ALL chain. A loop whose
condition cannot change is a bug by construction, so this was fixed — **but TAKE
ALL was NOT broken in practice, and this fix changes nothing in any real game.**

Reachability, measured rather than assumed:
- The path is only entered when an item is no longer where it was when the line
  was parsed. The chain is expanded once (`CreateAllCommands`) and each node then
  runs as its own turn, so an implicit (vocab 0) action firing *between* nodes,
  or an action firing *for* a node, would have to move an item still queued in
  the chain.
- `test/all_audit.c` (`make -f Makefile.headless scott_all_audit`) shows every
  real game does contain implicit actions that relocate ALL-eligible items
  (Gizmo wanders in Gremlins, Adventureland's mud dries, Claymorgue's stars
  move), so it is reachable in principle.
- But instrumenting the skip path and replaying every walkthrough in the corpus,
  the ALL/EXCEPT scripts, and a script hammering GET ALL / DROP ALL forty times
  in a room holding an item that a 5%-per-turn action removes, produced **zero
  hits**. Transcripts are byte-identical before and after.

The only demonstration is the synthetic `test/regression.dat`, which forces it:
`TAKE COIN` fires an action that takes the coin and moves the rope to another
room; `TAKE ALL` (coin, rope, book, gem) then stopped dead at the rope and never
took the book or the gem.

**Fix:** skip just this node and let the chain continue. Advancing to another
node's item inside this turn — what the loop looks like it meant to do — would
be wrong in a second way: the item's name is printed *before* the action scan
(scott_actions.c:~893), so the engine would take the book while the transcript
said "A rope....Taken".

### 7. `LookWithPause()` dereferences before NULL check — scott_display.c:~619 (FIXED)
`char fc = Rooms[MyLoc].Text[0];` ran before the `Rooms[MyLoc].Text == NULL`
test on the next line, making the guard useless. **Fix:** reorder — NULL/`MyLoc==0`
check first, then read `Text[0]`.

### 8. `Look()` leaks buffer + stream on NULL room text — scott_display.c:~494 (FIXED)
On `!r->Text`, `Look()` bare-returned after having MemAlloc'd `buf` and opened
`room_description_stream`; only `FlushRoomDescription()` frees/closes them.
**Fix:** call `FlushRoomDescription(buf)` before returning.

### 9. `MatchTitleWithPeriod()` ignores its `index` argument — parser.c:~597 (FIXED)
It always tested `string[0..2]` instead of `string[index..index+2]`, which broke
the feature in both directions:
- a "mr."/"dr." title is only ever recognised at the *start* of the line, so
  "call dr." tokenises as `call` / `dr` / `.` and the game answers "I don't know
  what a DR is";
- once a line *does* start with "mr."/"dr.", every later word beginning with 'd'
  or 'm' is chopped to three characters — "dr. medal" becomes `dr.` / `med` /
  `al`.

**Fix:** test at `index`. Also tightened `MatchYMCA()`'s bound from `>` to `>=`,
which could read one past the input buffer on a full-length line. Both are
covered by `test/regression.dat` ("call dr." / "dr. medal"); Gremlins (the
`y.m.c.a.` game) replays byte-identically, ZX and C64.

### 10. `WhichWord()` called with `NumWords` instead of `NumWords + 1` — parser.c:~1203 (and ~1118) (FIXED)
The dictionary holds `NumWords + 1` entries and `WhichWord` loops
`ne < list_length`, so the final dictionary word could never match at these two
call sites (every other caller passes `+ 1`). An item whose AutoGet word is the
last dictionary entry got `noun = 0` in its ALL node, so a `TAKE <that noun>`
action never fired for it during TAKE ALL — the engine fell back to a plain
"Taken" instead. Reproduced in `test/regression.dat`, where the gem is the last
dictionary word.

**Fix:** pass `NumWords + 1`. Safe at every call site: all loaders allocate the
dictionary with `NumWords + 2` slots, and the arrays are calloc'd, so entry
`NumWords` is in bounds and reads NULL when unused (`WhichWord` skips NULL).

### 11. `Items[LIGHT_SOURCE]` read without a bounds check — detect_game.c:~674 (FIXED)
The lamp is item 9 by convention (`#define LIGHT_SOURCE 9 /* Always 9 how odd */`)
and the main loop reads `Items[LIGHT_SOURCE].Location` **every turn** without
consulting `NumItems`. Every real database has at least ten items, but the
plaintext loader accepts any count, and a database declaring fewer reads off the
end of the `Items` array (confirmed with a six-item database: the read lands in
an adjacent heap chunk, which is why ASan does not flag it).

**Fix:** always allocate through `LIGHT_SOURCE` in `AllocateGameData()` and in
the two TI-99 loaders (which also switch from `MemAlloc` to `MemCalloc`, so the
padding reads back as a destroyed item). No behaviour change for any real game:
when `NumItems >= 9` the allocation is exactly as before.
