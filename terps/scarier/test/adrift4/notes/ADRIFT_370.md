# ADRIFT 3.70 games

`games/arlo.taf` and `games/castle.taf` are, as far as a full sweep of the IF
Archive and of every reachable adrift.co / Wayback copy can establish
(2026-08-04), **the only surviving ADRIFT 3.70 games** — and the oldest
surviving ADRIFT games of any version. Both were downloadable from
`www.adrift.co/files/games/<name>` while being absent from the site's own
adventure database, and exist on no other host.

| file | title | author | dated | size |
|---|---|---|---|---|
| `arlo.taf` | Alice's Restaurant Anti-Massacree Adventure | Laura Lee | 18-03-2000 | 71734 |
| `castle.taf` | Castle Quest | Andrew Cornish | 10-06-2000 | 14686 |

Header (first 14 bytes, `"Version 3.70\r\n"` XOR the fixed VB6 keystream):

    3c423fc96a87c2cf 9445 39 61 39fa

Byte 10 is `0x39`. It is *not* `0x35`, which is what interpolating from
3.80 (`36`) / 3.90 (`37`) would suggest — that wrong guess is why an earlier
sweep classified both files as unknown.

**Both are playable in Scarier as of 2026-08-04**, via `V370_PARSE_SCHEMA` in
`sctafpar.cpp` and the version check in `sctaffil.cpp`. The container is
identical to 3.80 (CRLF plaintext XOR'd with the VB6 PRNG from seed
`0x00a09e86`, keystream indexed from file offset 0; no length header and no
password trailer, unlike 4.0). Four layout differences from 3.80:

1. **One extra integer line after the second `**`**, before the
   game-name/author pair — the **0-based index of the winning task**:

        3.70 arlo:  ... 5 **  6 106       7 <title>  8 <author>
        3.70 castle:... 5 **  6 9         7 <title>  8 <author>
        3.80 duck:  ... 5 **  6 <title>   7 <author>

   which is exactly where `taf38schema.py`'s 3.80 walk desyncs
   (`ValueError: line 8 not an int: b'Laura Lee'`).
2. Task movements are pairs `(Var1, Var2)` with no Var3: 3.7 has one flat
   destination list — `0` hidden, `1` held by the player, `2` the player's room,
   `3+n` room n — where 3.8 splits "where" from "how".
3. Tasks have no `BWinGame` flag (see 1).
4. A fixed block of **17 renameable built-in command words** (`north` … `help`)
   sits where 3.80 has its general synonyms table. Renaming one is *additive* —
   the standard word keeps working — so each becomes a 4.0 synonym mapping the
   author's word onto the standard one.

Everything else — rooms, exits, objects, NPCs, and with them the pooled
size/weight burden model — is byte-for-byte 3.80. The one semantic difference is
that an object cannot start out on an NPC: `#Parent` is ignored when it starts
held or worn.

All of the above was measured against the genuine `run370.exe` under Wine rather
than inferred from these two files; probes and results are in
`../../../RUNNER_TESTS_TODO.md` §6.

Both games sit in `games/` with the rest of the corpus, and both are solved:
`goldens/castle_quest_solution.txt` (17 moves, 50/50) and
`goldens/alices_restaurant_solution.txt` (85 moves, 190/190) each PASS against a
blessed golden. See `Castle_Quest_walkthrough.md` and
`Alices_Restaurant_walkthrough.md`.
