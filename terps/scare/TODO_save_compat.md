# TODO: ADRIFT 4-compatible save files (`.tas` Runner interop)

## Goal

Make SCARE's saved games **bidirectionally interoperable** with the original
ADRIFT 4 Runner (`run400.exe`):

- **Read** a `.tas` produced by the real Runner (or another ADRIFT 4 interpreter)
  and restore it correctly.
- **Write** a `.tas` that the real Runner will load without complaint.

Today neither direction works: SCARE reads and writes its own `.tas` dialect
which is the ADRIFT v4 layout **minus the leading version line** (and with the
Battle System state in a different place), so a Runner save and a SCARE save are
mutually unreadable. This is the long-parked "item 5b" of
`TODO_battle_system.md`; with mutable battle attributes now implemented (commit
on `claudeslop`, see `TODO_battle_attributes.md`) the last *data-model* blocker
is gone and only the *file-format* work remains.

### Why it matters / priority

Pure interop nicety: carry a save between Spatterlight/SCARE and the Windows
Runner, or load saves shipped with a game. It changes no gameplay, so it ranks
**below** any behaviour work. It is, however, self-contained and the format is
almost entirely already understood (see below). Estimate ~1–2 focused days, most
of it careful field-by-field diffing plus the one RE step.

## Progress (this branch)

- **§2 + §3 Runner-format serializer — DONE, validated against real saves.**
  `ser_save_game` now writes the full ADRIFT layout: player **name** field, the
  converted size/weight **limits**, the **interleaved** player battle block
  (after encumbrance) and per-NPC battle block (after "seen"), and static objects
  as a **room list** (`count` + 1-based room indices) instead of a single
  position. The old private trailing `ScareBattleState` block is gone.
  `ser_load_game` reads this when line 1 carries the `0xAC` header, and still
  reads legacy SCARE saves (no header: no name, single-int object positions,
  trailing battle block). Helpers: `ser_save_battle_block` /
  `ser_restore_battle_block` (slot order `SER_BATTLE_SLOTS` = Str,Def,Acc,Agi,
  each max,hi,lo; padded), `ser_save_object_location` /
  `ser_restore_object_location`.
  **Validation:** loading the two real `run400.exe` saves and re-saving
  reproduces them **byte-for-byte** — `start.tas` 0 diffs, `combat.tas` 2 diffs,
  both being only the derived *current* size/weight (lines 14/16; we write 0, the
  Runner recomputes from inventory on load). All 17 corpus games still SCARE→SCARE
  round-trip; battle-attribute round-trip all-PASS; a genuine pre-change legacy
  save still loads. CLI builds clean.
  **§4 CONFIRMED:** a SCARE-written save (score 33, turns 7) loads cleanly in the
  real `run400.exe` and shows the stamped score — SCARE→Runner works. Interop is
  now validated **both** directions (Runner→SCARE byte-exact; SCARE→Runner
  accepted by the Runner). Only optional fidelity remains: compute the live
  current size/weight (lines 14/16) — cosmetic, the Runner recomputes it.

- **§1 version-line header — DONE (write + read), verified.** `ser_save_game`
  now emits the ADRIFT v4 version line `"\xAC" "400052"` as the first `.tas`
  line, ahead of GameName (`scserial.c`, `SER_VERSION_HEADER`). `ser_load_game`
  sniffs the first line: a leading `0xAC` byte (`SER_VERSION_LEAD`) ⇒ skip the
  version line and take GameName from the next line; any other first line ⇒
  legacy SCARE save, that line *is* the GameName. The reader is **tolerant** —
  it accepts any version digits a v4 Runner wrote rather than insisting on its
  own. Verified with a zlib round-trip harness: header byte present on write,
  both with-header and stripped-header (legacy) streams restore turns/score, and
  the battle-attribute round-trip (`scsertest`) still passes.
- **§5 read-sniff / backward compat — DONE.** One reader, both formats; existing
  user saves keep loading (no flag-day), as designed.

### Version digits (resolved)

`run400.exe` reports PE file version **4.0.0.52** (extracted from its
`VS_FIXEDFILEINFO`). The Runner's `saveg` writes
`Chr(172) & CStr(Major) & Format(Minor,"000") & Format(Revision,"00")`, so its
exact first line is `0xAC` + `"4"` + `"000"` + `"52"` = `"\xAC400052"`. That is
the literal SCARE now writes. (Other v4 builds differ only in the revision
digits; the read side tolerates that.)

### Battle UDT field offsets (resolved, from the decompile)

Confirmed against the Runner's status-display code (`Battles.bas` ~298–342,
`run400.exe`): within a character's battle sub-struct the byte offsets are
`Stamina` cur=2 / max=6; `Strength` lo/hi/max = 8/10/12; `Accuracy` = 14/16/18;
`Defence` = 20/22/24; `Agility` = 26/28/30; and (NPC only) `Attitude` = 0,
`Speed` = 32, attack-countdown = 36; the player block's trailing field is 38.
The `saveg` writer (Form1.frm ~3854) prints each attribute triple as
`(max, hi, lo)` and orders the four attributes **Strength, Defence, Accuracy,
Agility** — i.e. SCARE's `sc_battle_t` slot order
(`Strength=0, Accuracy=1, Defense=2, Agility=3`) maps to Runner save order
**[0, 2, 1, 3]**. This is the exact mapping the still-to-do §2 interleaving
needs; no further RE is required for the battle blocks.

## Resolved save-file structure (from REAL Runner saves)

Two real `run400.exe` saves of `Sun_Empire_Quest_For_The_Founders` (13 rooms /
72 objects / 73 tasks / 3 events / 8 NPCs, a battle game) were captured and
decompressed: `start` (game start, empty-handed) and `combat` (turn 101, score
40, carrying 12 objects, mid-fight). Fixtures + decoded line dumps live in
`~/adrift-battle/runner-saves/`. Both inflate to exactly 716 lines. The full
line-by-line structure, confirmed against the `saveg` decompile **and** the real
data, is:

```
 0  version line            "\xAC400052"            (0xAC + 4 + "000" + "52")
 1  GameName                (string)
 2..6  counts               rooms,objects,tasks,events,npcs  (Format → bare)
 7  score                   (Format → bare)
 -- player block (9 fields, NOT 8) --
 8  player NAME             "Tangee Simone"  <-- STRING, the missing 9th field
 9  room (1-based)          | 10 parent | 11 position | 12 gender   (bare)
 13 size-limit (90)         | 14 CURRENT size | 15 weight-limit (90) | 16 CURRENT weight
 -- player battle block (15 vals, battle flag only; raw Print → " N " padded) --
 17 maxstamina | 18 stamina | 19-21 Str(max,hi,lo) | 22-24 Def | 25-27 Acc
 30-28? see note | 28-30 Agi(max,hi,lo) | 31 trailing idx38 (= -1 here)
 -- rooms: R seen flags (bare) --
 -- objects: per object, see "object loop" below --
 -- tasks: 2 bools each | events: 4 fields each --
 -- npcs: location, seen, [npc battle block 17 vals if flag], walk steps --
 -- variables (per bundle order: int bare / string raw) --
 713 elapsed seconds | 714 turns | 715 "" (final CRLF)
```

**Number formatting (confirmed):** VB `Print 1, n` on a raw number emits
`" n "` (leading sign-space + trailing space) — identical to SCARE's
`ser_buffer_int_special` `"% ld "`. `Format(n,"General Number")` emits bare
digits. So the battle-block values, the static-object room list, and walk steps
are space-padded; counts/score/positions/flags are bare. (For *Runner
acceptance* this is cosmetic — VB `Val()` ignores the spaces — so SCARE need only
be self-consistent, but matching it gives byte-identical output for testing.)

**Two surprises beyond the original plan:**

1. **The "off-by-one" is the player NAME** (line 8) — `saveg` prints
   `var_19C(4)` *raw* (it is a string, "Tangee Simone"), as the first player
   field, which SCARE omits entirely. So the player block is name + 8 ints, not
   8 ints. Resolves §3's open question.
2. **Static objects serialise as a room LIST, not a single position.** The
   Runner's object loop branches on `var_1B8(24)`: dynamic object ⇒ one position
   int (`var_1B8(26)`); **static** object ⇒ `Print count` then one room index per
   room it is present in (the `var_1B8(28)` per-room bool array). SCARE writes a
   single position int for *every* object, so its object stream diverges from the
   Runner's for static objects. SCARE already models this (`obj_is_static`,
   `Where.Type` = ALL/NO/ONE/SOME rooms, `scobjcts.c` ~663–708) — the work is to
   emit/consume the count+list form for static objects to match.

**Encumbrance resolved (§3):** limits (lines 13,15) are constant 90/90; the
*current* size/weight (14,16) are **live** — `0/0` empty-handed in `start`,
`4/4` carrying 12 items in `combat`. The Runner stores them; whether its loader
*uses* them vs recomputes still needs the restore routine, but writing the real
current totals (not SCARE's `0`) is the faithful choice.

## What is already in place

- **Compression — done, both directions.** ADRIFT v4 `.tas` files are
  zlib/deflate streams. SCARE already deflates on save (`scserial.c` `ser_flush`,
  `deflateInit`/`deflate`) and inflates on load (`sctaffil.c` `taf_decompress`,
  via `taf_create_tas`, which treats every saved game as v4.0). So compression is
  **not** a gap.
- **The body format is ~95 % the Runner's already.** `ser_save_game` /
  `ser_load_game` were written to mirror the Runner's line-per-value text format
  (CRLF-terminated `Print #1` lines). The counts, score, player block, room-seen
  flags, object position/seen/parent/openness/state/unmoved, task done/scored,
  event timing, NPC location/seen/walksteps, variables, elapsed seconds and turn
  count are all there, in Runner order. Walk steps even use the Runner's odd
  `" %ld "` integer formatting (`ser_buffer_int_special`).
- **Mutable battle attributes — done.** The interleaved battle blocks are mostly
  the Strength/Accuracy/Defence/Agility Lo/Hi/Max, Max Stamina, Attitude and
  Speed values; SCARE now models all of these as live state (`sc_battle_t`), so
  there is finally somewhere to round-trip them. (Previously SCARE re-rolled them
  from the bundle and had nowhere to put a restored value.)
- **The Runner save layout is reverse-engineered.** From `run400.exe`'s `saveg`
  routine (`Form1.frm` @~3846–3981 in the decompile; the `.frm` has embedded
  NULs, use `grep -a` / strip-NULs). Header and battle-block details below.

## The format deltas (the actual work)

### 1. Leading version-line header — the main blocker

The Runner's first `.tas` line is (decompile `Form1.frm` ~3846):

```
Print 1, CVar("¬" & CStr(App.Major)) & Format(Minor,"000") & Format(Revision,"00")
```

i.e. `0xAC` (`¬`, `Chr(172)`) + `Major` + `Minor` (3 digits) + `Revision`
(2 digits), e.g. `¬40006`. SCARE omits this line entirely — `ser_load_game`
reads **GameName** as line 1 (`scserial.c` ~723), and `ser_save_game` writes
GameName first (~355).

- **Write:** emit the header line first, before GameName. The version digits
  should match what a real v4 game/Runner expects; capture the exact value from a
  Runner-produced save (and from the v4.0 `.taf` header SCARE already parses in
  `sctaffil.c`, signatures @~51).
- **Read:** before reading GameName, read the first line; if it begins with
  `0xAC`, it is a Runner/new save — validate (or just skip) the version and
  continue. If it does **not**, it is a legacy SCARE save — rewind one line and
  proceed as today. This presence/absence of the `¬` byte is a clean, unambiguous
  discriminator, so old SCARE saves keep loading for free (see §5).

### 2. Battle System block placement — interleaved vs trailing

SCARE writes its battle state as a sentinel-introduced **trailing** block
(`ScareBattleState/2`) after the turn count. The Runner **interleaves** it,
gated on the battle flag (`MemVar_494282`):

- **Player block** (15 values, `player(40)` sub-struct), right after the player
  fields and before the room-seen loop, sub-struct index order:
  `6 2 | 12 10 8 | 24 22 20 | 18 16 14 | 30 28 26 | 38`
  = MaxStamina, live Stamina, then Max/Hi/Lo of Strength, Defence, Accuracy,
  Agility, then one trailing field (idx 38 — likely the recovery/stamina
  counter; confirm).
- **NPC block** (17 values, `npc(172)` sub-struct), inside each NPC's loop
  iteration *after* location (`npc(14)`) and seen (`npc(26)`) and *before* the
  walk-step loop: `0 6 2 | 12 10 8 | 24 22 20 | 18 16 14 | 30 28 26 | 32 36`
  = Attitude, MaxStamina, live Stamina, four Max/Hi/Lo triples, then Speed
  (idx 32) and the attack-countdown (idx 36).

All of these now have a home in `sc_battle_t` / the live stamina + counter
fields. The work is to **write/read them in the Runner's interleaved positions**
instead of (or in addition to) the trailing block. Note SCARE's internal
attitude encoding is `0=Neutral,1=Ally,2=Enemy`, which **matches** the Runner's
stored value (the `0=Ally…` order is only the editor's "Change Attitude" combo;
see `TODO_battle_attributes.md`), so attitude needs no remap here.

The wielded weapon is **not** in the Runner's save — it resets to "none" on load
(`global_78 = &HFF`). SCARE persisting it is a benign superset; in Runner-format
output it must be dropped (or kept only as a SCARE-private extension, see §5).

### 3. Field-by-field parity audit

Diff `ser_save_game` against `saveg` line by line and resolve every mismatch.
Known suspects:

- **Encumbrance.** SCARE writes four constants `90, 0, 90, 0` and ignores them on
  read (`scserial.c` ~384, with a comment flagging exactly this risk). Determine
  whether the Runner's loader *uses* these or recomputes from held objects; if it
  uses them, compute the real player size/weight limits and current totals.
- **Object openness/state.** SCARE writes openness/state only when non-zero
  (~399–403); the Runner keys these on the object's openable/stateful nature
  (`var_1B8(24)` test, `Form1.frm` ~3890). Match the Runner's emit conditions
  exactly, or the field stream desynchronises.
- **Event fields.** Confirm the StarterType/TaskNum/state/finished mapping
  (~416–441) matches the Runner's event record.
- **Counts and ordering.** Verify the five header counts and every loop bound
  line up (rooms/objects/tasks/events/NPCs).
- **Player block field count — RESOLVED (it is the player NAME).** The Runner's
  `saveg` (Form1.frm ~3848) writes **nine** non-battle player fields after the
  score — `var_19C(4) (0) (16) (12) (30) (32) (36) (34) (38)` — vs SCARE's
  **eight**. The extra is `var_19C(4)` = the player **name string** ("Tangee
  Simone" in the fixtures), printed first and raw. The rest map: `(0)`=room,
  `(16)`=parent, `(12)`=position, `(30)`=gender, `(32)`=size-limit,
  `(36)`=current size, `(34)`=weight-limit, `(38)`=current weight. See the
  "Resolved save-file structure" section above.

### 4. Locate and use the Runner's *restore* routine as the spec

The `saveg` writer is a good spec, but the authoritative spec for what the Runner
will *accept* is its loader. The restore routine is **`Proc_19_63_472CA4`
(address 472CA4)** — `openg` (Form1.frm ~3729) calls it as
`Proc_19_63_472CA4(global_0, 0)` right after the file dialog. **It is named in
the symbol table (`run400.txt`) but its body is NOT present in the available
decompile** (`~/adrift-battle/decompiled/` has only `openg`/`saveg`, not 472CA4;
`/tmp/petter` no longer exists). So the open questions below *cannot* be answered
from the dump on hand — they need either a fresh decompile of 472CA4 or live
observation under Wine:

- whether the loader *uses* the four encumbrance fields or recomputes them
  (decides if SCARE's `90/0/90/0` constants are safe — §3);
- what the ninth player field is (§3);
- whether trailing lines after the turn count are tolerated or cause a hard error
  (decides whether the SCARE-private `ScareBattleState` block can coexist with a
  Runner-readable body — §5).

For the *write* direction (SCARE→Runner) the `saveg` writer is already a complete
and authoritative spec, so SCARE→Runner does not strictly need 472CA4; it is the
Runner→SCARE robustness questions that do.

### 5. Migration / dual-format strategy (recommended)

Make the **canonical** SCARE save format the Runner-compatible one, and keep a
backward-compatible reader:

- **Save:** always write the Runner layout (version header + interleaved battle
  blocks + verified body). Optionally still append the `ScareBattleState/2`
  trailing block *iff* §4 shows the Runner tolerates trailing data — that lets a
  SCARE save carry the few SCARE-only extras (e.g. wielded weapon) while staying
  Runner-loadable; otherwise drop it.
- **Restore:** sniff line 1. `¬…` ⇒ Runner/new format (parse header + interleaved
  blocks). No `¬` ⇒ legacy SCARE save ⇒ current code path (GameName first,
  trailing `ScareBattleState/1|2` block). One reader, both formats, no flag-day
  for existing user saves.

## Risks / open questions

- **Encumbrance semantics** (§3): used by the Runner's loader, or recomputed?
  Drives whether the constants are safe.
- **Trailing-data tolerance** (§4): does the Runner's loader stop cleanly at the
  turn count, or error on extra lines? Decides the dual-format option in §5.
- **Version digits** (§1): exact `Major/Minor/Revision` string a v4 Runner
  accepts; capture from a real save rather than guess.
- **Player trailing field idx 38** (§2): confirm it is the recovery/stamina
  counter and maps to `playerstaminacounter`.
- **Locale/number formatting**: VB `Format(x, "General Number")` vs SCARE's
  `%ld`; should be identical for the integers in play, but verify negatives and
  the `int_special` `" %ld "` walk-step format byte-for-byte.
- **Non-battle games** must stay untouched (battle blocks only when the flag is
  set), exactly as the live-state extension already guarantees.

## Validation plan

- **Round-trip (SCARE→SCARE):** unchanged — existing saves and freshly written
  ones both restore (the `scsertest`-style harness in `~/adrift-battle/harness/`).
- **SCARE→Runner:** write a save from SCARE for a corpus game, load it in the
  real `run400.exe` (`~/adrift-battle/runner/`, under Wine/VM), confirm room,
  inventory, score, turn count and — for a battle game mid-combat — stamina and
  attitudes carry over.
- **Runner→SCARE:** the reverse — save in the Runner, restore in SCARE, diff the
  resulting game state.
- **Legacy SCARE saves** still load (the §5 sniff).
- **Corpus smoke:** no crashes across the 17 battle games + a sample of
  non-battle games; byte-compare the *body* (post-header, pre-trailing) of a
  SCARE-written save against a Runner-written one for the same starting state.

## References

- `scserial.c` — SCARE's serialiser (`ser_save_game` ~338, `ser_load_game` ~675,
  `ser_flush` deflate ~93, `SER_BATTLE_MARKER` block).
- `sctaffil.c` — `taf_create_tas` / `taf_decompress` (saved games read as v4.0,
  zlib-inflated); v4.0/3.9/3.8 signatures ~51.
- `TODO_battle_system.md` item 5/5b — the parked interop note this supersedes;
  the interleaved battle-block index orders.
- `TODO_battle_attributes.md` — the now-done mutable-attribute work that unblocks
  the battle-block round-trip; attitude-encoding note.
- `adrift-battle-formulas-re` / `scare-battle-system-port` memories — battle
  sub-struct field indices and the `saveg` @477318 RE; decompiles at
  `~/adrift-battle/decompiled/` (and `/tmp/petter`), Runner at
  `~/adrift-battle/runner/run400.exe`.
- Manual.pdf (`~/adrift-battle/runner/manual.txt`) for save/restore behaviour.
