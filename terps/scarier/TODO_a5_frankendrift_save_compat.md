# TODO: ADRIFT 5 FrankenDrift-compatible save files (`.tas` interop)

## STATUS: DONE (core + bidirectional cross-runner validation)

Implemented and validated in both directions against real FrankenDrift:

- **Framing** — `a5_deflate` (a5deobf.cpp/.h) writes an RFC-1950 zlib stream
  (`0x78 0xDA`, `Z_BEST_COMPRESSION` == FD's `CompressionLevel.Optimal`);
  `gsc_a5_save` deflates, `gsc_a5_restore` sniffs `0x78` and inflates, else falls
  through to a raw legacy `<SaveState>` (os_glk.cpp). Undo/round-trip keep the
  **uncompressed** blob (compression is os_glk-only).
- **Schema** — `a5run_save` emits FrankenDrift's `<Game>` (keyed
  Location/Object/Task/Event/Character/Variable/Group/Turns, where-fields as
  `.ToString` enum names, full effective property sets so FD does not strip
  properties) **plus** a `<ScarierExt>` child carrying the complete native
  snapshot (RNG, event/walk internals, seen/override/displayed/look sets).
  FD ignores unknown `<Game>` children, so the extension is invisible to it.
- **Restore** — `a5run_restore` sniffs the root: `<SaveState>` = legacy body;
  `<Game>` + `<ScarierExt>` = lossless native restore; `<Game>` alone = the
  `restore_fd_game` foreign reader (keyed, tolerant).
- **Gotcha fixed** — Scarier's `last_se_index == -1` ("no sub-event run") maps to
  a skip-value (≥ sub-event count) in the FD `<Event>` field; FD's only guard is
  `SubEvents.Length > index`, so a literal `-1` made FD do `SubEvents(-1)` →
  `IndexOutOfRangeException`. Foreign read clamps out-of-range back to `-1`.

**Validation:** `a5_save_test` (Scarier↔Scarier round-trip + zlib byte-exactness
+ foreign-read, ASan/UBSan-clean, in `make -f Makefile.headless test`); mid-game
`A5_SAVE_AT` save/restore is byte-identical across all **22** corpus
walkthroughs; **FD→Scarier and Scarier→FD** both confirmed on Grandpa's Ranch via
the FrankenDrift headless runner (`FD_SAVE_PATH`/`FD_RESTORE_PATH` env vars,
`test/frankendrift-headless/Program.cs`) — door-open state + navigation survive
the cross-runner hop; no FD exception across 8 games.

**Known lossy areas (documented, unchanged from the plan below):** `<Displayed>`
on the FD fields (skipped — cosmetic re-show at most once cross-runner; carried
losslessly in `<ScarierExt>`), per-task `Scored` (Scarier has no such flag; FD
defaults false), RNG cross-runner (FD persists none; Scarier keeps it in the
extension), `OnCharacter` character placement (no Scarier producer; read maps it
to at-location best-effort), array-valued variables (`Value_1…` — only `Value_0`
round-trips; no shipped corpus game uses arrays).

## Goal

Make Scarier's **ADRIFT 5** saved games interoperable with **FrankenDrift**
(`/Users/administrator/frankendrift`, `FileIO.SaveState`/`LoadState`,
`clsState.vb`) — the reference v5 runner:

- **Write** a save FrankenDrift's `LoadState` will restore without error.
- **Read** a save produced by FrankenDrift (or, transitively, the official
  ADRIFT 5 Runner, which uses the same on-disk schema) and restore it correctly.

This is the v5 analogue of the already-done ADRIFT 4 work in
`TODO_save_compat.md` (Runner `.tas` interop). It is **not started** — today
Scarier reads and writes its *own* `<SaveState>` dialect, which is structurally
incompatible with FrankenDrift on every axis (root element, framing, schema, and
the object-location model). See "Current state" below.

### Why it matters / priority

Pure interop nicety: carry a save between Spatterlight/Scarier and
FrankenDrift/the Windows Runner, or load saves shipped with a game. It changes
no gameplay, so it ranks **below** any behaviour/walkthrough work. Unlike the v4
job it needs **no reverse engineering** — FrankenDrift is open source and the
save path is ~200 lines of plain XML writing (`FileIO.SaveState` @51,
`LoadState` @876). The cost is entirely in a careful field-by-field mapping
between Scarier's `a5_state_t` and FrankenDrift's `clsGameState`, plus one
data-model impedance mismatch (the object dynamic/static location split, §3).
Estimate ~1–2 focused days.

## Current state (the gap)

Scarier's v5 serialiser is `a5run_save`/`a5run_restore` (`a5run.cpp` ~1729 /
~1915), surfaced to Glk by `gsc_a5_save`/`gsc_a5_restore` (`os_glk.cpp` ~4216 /
~4260). It is **fully functional for Scarier→Scarier** (save, restore, and the
single-level undo built on it, `TODO_a5_undo.md`) but shares nothing with
FrankenDrift:

| Axis | Scarier today | FrankenDrift |
|---|---|---|
| Root element | `<SaveState>` | `<Game>` |
| On-disk framing | **raw uncompressed** UTF-8 XML (`glk_put_buffer_stream`, `os_glk.cpp:4248`) | **zlib** stream (`ZLibStream`, `CompressionLevel.Optimal`, `FileIO.vb:201`) — no header, no XOR obfuscation |
| Entity ordering | model order, **positional** arrays (restored by index) | keyed by `<Key>`, dictionary order, restored **by key** (order-independent) |
| Object location | one `a5_owhere_t` enum + `is_static` flag (`a5state.h:28`) | **two** enums `DynamicExistWhere` + `StaticExistWhere` + `LocationKey` |
| Variables | single value per var | `Value_0`, `Value_1`, … (array-valued) |
| Turns / RNG | `<Turns>`, plus `<Rng>`/`<RngNative>` (Scarier extension) | `<Turns>` only; **no RNG** persisted |
| Displayed / props / seen | flat sets (node pointers / `a5_prop_ov_t` list / player-centric bool arrays) | per-entity `<Displayed>` / `<Property>` / `<Seen>` children keyed by string |

So a FrankenDrift save and a Scarier save are today mutually unreadable, exactly
as v4 was before `TODO_save_compat.md`.

## The FrankenDrift v5 save format (spec, from source)

Reverse-derived from `FileIO.SaveState` (@51) and `LoadState` (@876). **No RE
needed** — this is the authoritative spec.

### Framing

The whole file is a single **zlib stream** (RFC 1950: `0x78` header + DEFLATE +
Adler-32 trailer) of the UTF-8 XML below. **No** 12/16-byte Blorb/`.tas` header
and **no** 1024-byte XOR obfuscation — those apply only to the `.taf` *game*
file (`a5deobf.cpp`), **not** to saves. `LoadState` is reached via
`FileToMemoryStream(bCompressed:=True, …)` → `Decompress` (`FileIO.vb:841`,
`1077`), i.e. plain inflate. Scarier already has zlib both directions (v4
`ser_flush`/`taf_decompress`; a5 blorb inflate in `a5deobf.cpp`), so this is
machinery reuse, not new code.

### XML schema

Root `<Game>`. Children, in FrankenDrift's write order (order is **not**
significant to the reader — it uses `SelectNodes("/Game/<tag>")`):

```
<Game>
  <Location>*   Key, Property(Key,Value)*, Displayed*
  <Object>*     Key, [DynamicExistWhere], [StaticExistWhere], LocationKey,
                Property(Key,Value)*, Displayed*
  <Task>*       Key, Completed, [Scored], Displayed*
  <Event>*      Key, Status, Timer, [SubEventTime], [SubEventIndex], Displayed*
  <Character>*  Key, [ExistWhere], [Position], [LocationKey],
                Walk(Status,Timer)*, Seen*, Property(Key,Value)*, Displayed*
  <Variable>*   Key, Value_0?, Value_1?, … (only non-default indices), Displayed*
  <Group>*      Key, Member*
  <Turns>       integer
</Game>
```

Bracketed `[…]` elements are **omitted when at their default** (and the reader
substitutes the default when absent):

- `<Object><DynamicExistWhere>` omitted ⇔ `Hidden`; `<StaticExistWhere>` omitted
  ⇔ `NoRooms` (`SaveState` @87/@90).
- `<Character><ExistWhere>` omitted ⇔ `Hidden`; `<Position>` omitted ⇔
  `Standing`; `<LocationKey>` omitted ⇔ `""` (@137/@140/@143).
- `<Task><Scored>` may be absent (defaults false, `LoadState` @935).
- `<Variable>` `Value_i` omitted when the numeric value is `"0"` or the text
  value is `""` (@172).

**Enum values are written as `.ToString()` names** (not integers) and read back
with `[Enum].Parse`:

- `DynamicExistsWhereEnum` (`clsObject.vb:1034`):
  `Hidden, InLocation, InObject, OnObject, HeldByCharacter, WornByCharacter`
- `StaticExistsWhereEnum` (`clsObject.vb:1044`):
  `NoRooms, SingleLocation, LocationGroup, AllRooms, PartOfCharacter, PartOfObject`
- Character `ExistsWhereEnum` (`clsCharacter.vb:1640`):
  `Uninitialised, Hidden, AtLocation, OnObject, InObject, OnCharacter`
- Character `PositionEnum` (`clsCharacter.vb:1727`):
  `Uninitialised, Standing, Sitting, Lying`
- Event `StatusEnum` (`clsEvent.vb:41`):
  `NotYetStarted=0, Running=1, CountingDownToStart=2, Paused=3, Finished=4`
- Walk `StatusEnum` (`clsCharacter.vb`, `clsWalk` @1203):
  `NotYetStarted=0, Running=1, Paused=3, Finished=4` (note: **no** `2`)

Booleans (`Completed`, `Scored`) are VB `CBool`-parseable — write `"True"`/
`"False"` (`.ToString()` of a .NET `Boolean`), which `CBool` accepts.

`<Object><LocationKey>` carries the key whose meaning depends on the two
where-enums: the location for `InLocation`/`SingleLocation`, the container object
for `In/OnObject`/`PartOfObject`, the character for `Held/WornByCharacter`/
`PartOfCharacter`, the group for `LocationGroup`. `AllRooms`/`NoRooms` ignore it.

## The deltas (the actual work)

### 1. Framing + root element

- **Write:** build the `<Game>` document (below), then **zlib-deflate** it to
  the Glk save stream. Route `gsc_a5_save` through the same deflate helper the
  v4 path uses (`scserial.c` `ser_flush`) or a thin wrapper. Do **not** write raw
  XML.
- **Read:** `gsc_a5_restore` currently slurps the file and hands raw bytes to
  `a5run_restore`. Sniff the first bytes: a zlib stream starts `0x78` (`0x78 01`
  / `0x78 9C` / `0x78 DA`). If it inflates to XML whose root is `<Game>`, parse
  the FrankenDrift schema; if the bytes are already `<SaveState…` (legacy raw
  Scarier save), take the current path. One reader, both formats, no flag-day for
  existing user saves (mirrors v4 §5).

### 2. Emit/consume the `<Game>` schema (`a5run_save`/`a5run_restore`)

Replace the `<SaveState>` body with the `<Game>` schema. Key it by `<Key>`, not
position, so ordering is irrelevant. Field mapping from `a5_state_t`:

- **Objects** (`st->obj[i]`): see §3 for the location split. Emit `<Key>` from
  `st->obj[i].key` (fall back to `adv->objects[i].key`).
- **Characters** (`st->char_loc/char_position/char_onobj/char_in`): map
  `char_onobj`+`char_in`+`char_loc` to `ExistWhere`/`LocationKey` — `AtLocation`
  when `char_onobj==NULL` (`LocationKey`=`char_loc`), else `OnObject`/`InObject`
  (`LocationKey`=`char_onobj`, chosen by `char_in`). `Position` from
  `char_position` (`Standing`/`Sitting`/`Lying`). FD's `OnCharacter` has no
  current Scarier producer — handle on read, skip on write until modelled.
- **Variables** (`st->var_num`/`st->var_text`): emit `Value_0` (numeric →
  decimal, omit if `"0"`; text → raw, omit if `""`). If v5 array variables are
  ever modelled, emit `Value_1…`.
- **Tasks** (`st->task_done`): `<Task><Key><Completed>True|False`. `Scored`
  needs a per-task scored flag — Scarier tracks scoring elsewhere; wire it or
  omit (FD defaults false).
- **Turns:** `<Turns>` from `st->turns`.
- **Groups:** `<Group>` per group with a runtime membership override
  (`a5state_object_in_group`), listing effective `<Member>` object keys.

### 3. Object location: collapse Scarier's one enum onto FD's two (the mismatch)

Scarier's `a5_owhere_t` (`a5state.h:28`) fuses dynamic and static placement into
one field plus an `is_static` flag; FrankenDrift keeps `DynamicExistWhere` and
`StaticExistWhere` separate. Map (write) as:

| `a5_owhere_t` | `is_static` | → Dynamic | → Static | LocationKey |
|---|---|---|---|---|
| `NONE`/`HIDDEN` | 0 | `Hidden` (omit) | `NoRooms` (omit) | (omit) |
| `LOCATION` | 0 | `InLocation` | — | loc key |
| `LOCATION` | 1 | — | `SingleLocation` | loc key |
| `ALLROOMS` | 1 | — | `AllRooms` | (omit) |
| `LOCGROUP` | 1 | — | `LocationGroup` | group key |
| `ON_OBJECT` | 0 | `OnObject` | — | object key |
| `IN_OBJECT` | 0 | `InObject` | — | object key |
| `HELD_BY` | 0 | `HeldByCharacter` | — | character key |
| `WORN_BY` | 0 | `WornByCharacter` | — | character key |
| `PART_OBJECT` | 1 | — | `PartOfObject` | object key |
| `PART_CHAR` | 1 | — | `PartOfCharacter` | character key |

Read is the inverse: `StaticExistWhere != NoRooms` ⇒ `is_static=1`, decode from
the static enum; else decode from the dynamic enum. Note `InLocation` (dynamic)
and `SingleLocation` (static) both land on `A5_OWHERE_LOCATION`, discriminated by
`is_static` — verify against how `a5state`/`a5run` already distinguishes a
dynamic object dropped in a room from an author-placed static one.

### 4. Per-entity grouping of the sparse sets (Property / Displayed / Seen)

FrankenDrift nests these under their owning entity; Scarier keeps them flat.

- **Property overrides** (`st->ov`, `a5_prop_ov_t{entity,prop,value}`): group by
  `entity` key and emit `<Property><Key><Value>` under the matching
  `<Object>`/`<Character>`/`<Location>`. Requires classifying each override's
  entity (object vs character vs location) — `a5model_object`/`_character`/
  `_location` lookups. FD **also** persists *location* property overrides;
  Scarier's dynamic location-group props (`a5state_location_group_prop`) may need
  a `<Location>` section it does not currently write.
- **Seen** (`st->char_seen`/`obj_seen`/`loc_seen`): Scarier stores these as
  **player-centric bool arrays**; FrankenDrift stores a per-character `<Seen>`
  key list (`chs.lSeenKeys`) that mixes object/character/location keys the
  character has seen. On write, emit the player's true-flagged keys under the
  player `<Character>`. On read, populate the player arrays from that list. NPC
  seen-sets are not tracked by Scarier — write only the player's, tolerate others
  on read.
- **Displayed** (`<DisplayOnce>`): FD keys retired description segments by the
  description's **string key**, grouped under the owning entity; Scarier tracks
  them by **DOM node pointer** with a global pre-order index
  (`st->disp_once`, `collect_dom_nodes`). Bridging needs each description node's
  FD key + owner. **Recommended: skip `<Displayed>` on write initially** (a
  DisplayOnce segment may re-show at most once after a cross-runner restore —
  cosmetic) and record it as a known-lossy area; do it properly only if a corpus
  game depends on it.

### 5. Scarier-private extensions (RNG, conversation, pronouns, endgame)

FrankenDrift's `LoadState` reads only the elements it queries by name, so
**unknown children of `<Game>` are silently ignored**. That lets Scarier keep its
extras *inside* the FD-compatible document without breaking FD — the clean v5
version of v4's "trailing block" trick, but forward-compatible by construction:

- Emit `<ScarierRng>`/`<ScarierRngNative>`, `<ScarierConvChar>`/`Node`,
  `<ScarierItRef>`/`Them`/`Him`/`Her`, `<ScarierGameOver>`/`EndMessage`/
  `EndDisplayed` as extra `<Game>` children (distinct names so they never collide
  with FD tags). FrankenDrift ignores them; Scarier reads them back for
  identical-replay determinism and conversation/pronoun continuity.
- **Loss when loaded by FrankenDrift:** RNG (FD re-seeds — a game that draws
  randomness right after the restore diverges), the current conversation, last
  pronoun targets, and the mid-endgame flag. All acceptable; document them.

## Read-sniff / dual-format strategy (recommended)

Make the FrankenDrift layout the **canonical** Scarier v5 save and keep a
tolerant reader:

- **Save:** always write zlib(`<Game>…</Game>`) + the `Scarier*` extension
  elements.
- **Restore:** sniff — zlib magic `0x78` and inflated root `<Game>` ⇒ FD format;
  literal `<SaveState…` ⇒ legacy Scarier raw save ⇒ current `a5run_restore`
  path. Existing user saves keep loading.

## Lossy areas / open questions

- **`<Displayed>` mapping** (§4): description-node-pointer ↔ FD string-key +
  owner. Skipped initially; confirm no corpus game needs it before shipping.
- **`Scored` per task** (§2): Scarier must expose a per-task scored flag or
  accept FD's false default (a restored game could re-award a task's score).
- **RNG loss cross-runner** (§5): unavoidable — FD does not persist RNG.
  Scarier→Scarier keeps it via the extension elements.
- **`OnCharacter` / static location group edge cases** (§3): ensure read
  tolerates every FD enum value even where Scarier has no writer yet.
- **Variable arrays**: does any shipped v5 game use array variables? If so,
  `Value_1…` must round-trip, not just `Value_0`.
- **Entity present in save but absent in game (or vice-versa):** FD keys by
  string and simply skips unknown keys; Scarier's positional restore must switch
  to key-lookup so an added/removed entity between versions degrades gracefully.

## Validation plan

FrankenDrift **is** the ground truth (unlike v4, where the Runner needed Wine and
a decompile):

- **Scarier→FD:** save a corpus game in Scarier, load it in the FrankenDrift
  headless runner (`test/frankendrift-headless/`, see `TODO_adrift5_support.md`
  §Ground-truth), confirm room/inventory/score/turns and any mid-game object &
  character positions survive.
- **FD→Scarier:** save in FrankenDrift, restore in Scarier, diff resulting state.
- **Byte-compare (optional):** FrankenDrift indents with 4 spaces
  (`XmlTextWriter.Indentation = 4`) and orders sections
  Location→Object→Task→Event→Character→Variable→Group→Turns; matching that gives a
  near-byte-identical body for the same state (extension elements aside).
- **Round-trip (Scarier→Scarier):** unchanged determinism, including RNG replay
  (extension elements) and single-level undo (`TODO_a5_undo.md` uses
  `a5run_save`/`restore` — keep them working, whichever format they emit).
- **Legacy load:** an existing raw `<SaveState>` save still restores (the sniff).
- **Corpus smoke:** `make -f Makefile.headless test` in `terps/scarier/` — no
  crashes; add a save/restore leg to a couple of walkthroughs.

## References

- **FrankenDrift (spec):** `FrankenDrift.Adrift/FileIO.vb`
  `SaveState` @51 / `LoadState` @876 / `Decompress` @1077; `clsState.vb`
  (`clsGameState` @447, `RestoreState` @241); enums in `clsObject.vb` @1034/@1044,
  `clsCharacter.vb` @1640/@1727/@1203 (`clsWalk`), `clsEvent.vb` @41.
- **Scarier:** `a5run.cpp` `a5run_save` @1729 / `a5run_restore` @1915
  (current `<SaveState>` schema); `a5state.h` (`a5_owhere_t` @28, `a5_state_t`
  @62); `os_glk.cpp` `gsc_a5_save` @4216 / `gsc_a5_restore` @4260 (raw, needs
  zlib framing); `a5deobf.cpp` / v4 `scserial.c` `ser_flush` / `sctaffil.c`
  `taf_decompress` (existing zlib machinery to reuse).
- `TODO_save_compat.md` — the completed **v4** Runner `.tas` interop (parallel
  job; §5 dual-format strategy reused here).
- `TODO_a5_undo.md` — single-level undo built on `a5run_save`/`restore`; must
  keep working across the format change.
- `TODO_adrift5_support.md` — the FrankenDrift-parity map and the headless
  FrankenDrift ground-truth harness used for validation.
- `ADRIFT4_vs_ADRIFT5.md` — v5 container/obfuscation notes (obfuscation is a
  *game-file* concern, not a *save* concern — saves are plain zlib).
