# ADRIFT 4-compatible save files (`.tas` Runner interop) — DONE

## Goal (achieved)

Make SCARIER's saved games **bidirectionally interoperable** with the original
ADRIFT 4 Runner (`run400.exe`):

- **Read** a `.tas` produced by the real Runner and restore it correctly.
- **Write** a `.tas` that the real Runner will load without complaint.

Both directions now work (see "Progress" below). It changes no gameplay, so it
was always ranked below behaviour work; it landed once the mutable-battle-attribute
data-model blocker was cleared (see the `scare-battle-system-port` /
`adrift-battle-formulas-re` memories).

## Progress (this branch) — DONE, validated both directions

- **§2 + §3 Runner-format serializer — DONE, validated against real saves.**
  `ser_save_game` now writes the full ADRIFT layout: player **name** field, the
  converted size/weight **limits**, the **interleaved** player battle block
  (after encumbrance) and per-NPC battle block (after "seen"), and static objects
  as a **room list** (`count` + 1-based room indices) instead of a single
  position. The old private trailing `ScarierBattleState` block is gone.
  `ser_load_game` reads this when line 1 carries the `0xAC` header, and still
  reads legacy SCARIER saves (no header: no name, single-int object positions,
  trailing battle block). Helpers: `ser_save_battle_block` /
  `ser_restore_battle_block` (slot order `SER_BATTLE_SLOTS` = Str,Def,Acc,Agi,
  each max,hi,lo; padded), `ser_save_object_location` /
  `ser_restore_object_location`.
  **Validation:** loading the two real `run400.exe` saves and re-saving
  reproduces them **byte-for-byte** — `start.tas` 0 diffs, `combat.tas` 2 diffs,
  both being only the derived *current* size/weight (lines 14/16; we write 0, the
  Runner recomputes from inventory on load). All 17 corpus games still SCARIER→SCARIER
  round-trip; battle-attribute round-trip all-PASS; a genuine pre-change legacy
  save still loads. CLI builds clean.
  **§4 CONFIRMED:** a SCARIER-written save (score 33, turns 7) loads cleanly in the
  real `run400.exe` and shows the stamped score — SCARIER→Runner works. Interop is
  validated **both** directions (Runner→SCARIER byte-exact; SCARIER→Runner
  accepted by the Runner).

- **§1 version-line header — DONE (write + read), verified.** `ser_save_game`
  now emits the ADRIFT v4 version line `"\xAC" "400052"` as the first `.tas`
  line, ahead of GameName (`scserial.c`, `SER_VERSION_HEADER`). `ser_load_game`
  sniffs the first line: a leading `0xAC` byte (`SER_VERSION_LEAD`) ⇒ skip the
  version line and take GameName from the next line; any other first line ⇒
  legacy SCARIER save, that line *is* the GameName. The reader is **tolerant** —
  it accepts any version digits a v4 Runner wrote rather than insisting on its
  own. Verified with a zlib round-trip harness.

- **§5 read-sniff / backward compat — DONE.** One reader, both formats; existing
  user saves keep loading (no flag-day), as designed.

## Remaining (optional, cosmetic)

- **Live current size/weight (lines 14/16).** SCARIER writes `0`; the Runner
  recomputes both from held objects on load, so this is invisible to the Runner
  and only shows up as the 2 self-diff lines in `combat.tas`. Writing the real
  current totals would make the round-trip byte-identical for testing, but
  changes no observable behaviour.

---

## Reference: resolved save-file structure (from REAL Runner saves)

Kept as the reverse-engineering record behind the serializer above.

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
`" n "` (leading sign-space + trailing space) — identical to SCARIER's
`ser_buffer_int_special` `"% ld "`. `Format(n,"General Number")` emits bare
digits. So the battle-block values, the static-object room list, and walk steps
are space-padded; counts/score/positions/flags are bare. (For *Runner
acceptance* this is cosmetic — VB `Val()` ignores the spaces.)

**Two surprises beyond the original plan:**

1. **The "off-by-one" is the player NAME** (line 8) — `saveg` prints
   `var_19C(4)` *raw* (it is a string, "Tangee Simone"), as the first player
   field. So the player block is name + 8 ints, not 8 ints.
2. **Static objects serialise as a room LIST, not a single position.** The
   Runner's object loop branches on `var_1B8(24)`: dynamic object ⇒ one position
   int (`var_1B8(26)`); **static** object ⇒ `Print count` then one room index per
   room it is present in (the `var_1B8(28)` per-room bool array). The serializer
   emits/consumes the count+list form for static objects to match.

**Encumbrance:** limits (lines 13,15) are constant 90/90; the *current*
size/weight (14,16) are **live** — `0/0` empty-handed in `start`, `4/4` carrying
12 items in `combat`. The Runner recomputes them from inventory on load (verified
by SCARIER→Runner acceptance), so SCARIER's `0` is safe; see "Remaining".

### Version digits (resolved)

`run400.exe` reports PE file version **4.0.0.52**. The Runner's `saveg` writes
`Chr(172) & CStr(Major) & Format(Minor,"000") & Format(Revision,"00")`, so its
exact first line is `0xAC` + `"4"` + `"000"` + `"52"` = `"\xAC400052"`. That is
the literal SCARIER writes; the read side tolerates other revision digits.

### Battle UDT field offsets (resolved, from the decompile)

Confirmed against the Runner's status-display code (`Battles.bas` ~298–342,
`run400.exe`): within a character's battle sub-struct the byte offsets are
`Stamina` cur=2 / max=6; `Strength` lo/hi/max = 8/10/12; `Accuracy` = 14/16/18;
`Defence` = 20/22/24; `Agility` = 26/28/30; and (NPC only) `Attitude` = 0,
`Speed` = 32, attack-countdown = 36; the player block's trailing field is 38.
The `saveg` writer (Form1.frm ~3854) prints each attribute triple as
`(max, hi, lo)` and orders the four attributes **Strength, Defence, Accuracy,
Agility** — i.e. SCARIER's `sc_battle_t` slot order
(`Strength=0, Accuracy=1, Defense=2, Agility=3`) maps to Runner save order
**[0, 2, 1, 3]**. SCARIER's internal attitude encoding is `0=Neutral,1=Ally,2=Enemy`,
which **matches** the Runner's stored value. The wielded weapon is **not** in the
Runner's save — it resets to "none" on load (`global_78 = &HFF`); SCARIER drops it
from Runner-format output.

## References

- `scserial.c` — SCARIER's serialiser (`ser_save_game` ~338, `ser_load_game` ~675,
  `ser_flush` deflate ~93, `SER_BATTLE_MARKER` block).
- `sctaffil.c` — `taf_create_tas` / `taf_decompress` (saved games read as v4.0,
  zlib-inflated); v4.0/3.9/3.8 signatures ~51.
- `adrift-battle-formulas-re` / `scarier-battle-system-port` memories — battle
  sub-struct field indices and the `saveg` @477318 RE; decompiles at
  `~/adrift-battle/decompiled/`, Runner at `~/adrift-battle/runner/run400.exe`.
  (The design docs `TODO_battle_system.md` / `TODO_battle_attributes.md`, which
  carried the parked "item 5b" interop note and the interleaved battle-block
  index orders, were deleted 2026-07-11 once done — in git history if needed.)
- `run400.exe` restore routine `Proc_19_63_472CA4` (address 472CA4) is the
  loader; its body was never in the available decompile, but SCARIER→Runner
  acceptance was confirmed empirically instead, so a fresh decompile is no longer
  needed.
