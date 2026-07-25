# TODO — implement the "is following you." room line (static `%Player%` followers)

Status: **DONE** (2026-07-23). Fixed in `a5state.cpp` via `resolve_carrier`:
the static On/In-Character loader now normalises a `%Player%`-variable carrier to
`st->player_key`, mirroring the object loader. Barry Leitch resolves to the
player's room and "is following you." prints 1956× exactly as FD does. The
Symphonica64 xoshiro baseline dropped **2116 → 263** (residual = non-follower
RNG/prose noise: Rory's `OneOf` taunt, river-bank prose, Kickstarter flavour).
Golden re-blessed (vanilla still 0), MAP row updated to
`Symphonica64|symphonica.blorb|0|263`. Whole-suite re-run: **0 FAIL, 0
REGRESSION, no other game changed** (Symphonica is the corpus's only static
`%Player%`-carrier follower). a5 unit tests + save/restore round-trip green.
Kept below for the historical root-cause record.

---

Original plan (as implemented):

## Symptom

FrankenDrift prints `1980s Barry Leitch is following you.` in essentially every
room description across the Symphonica 64 transcript; Scarier omits it. That one
character accounts for **1956 of the 2187 FD-only lines (89%)** in the xoshiro
ground-truth diff. No other follower diverges.

## Why FD shows it and Scarier doesn't — it is NOT a special-cased string

"is following you." is **not** hardcoded anywhere (confirmed: no such literal in
`~/frankendrift`). It is ordinary author data:

- Barry is a character with `CharacterLocation = "On Character"`,
  `CharOnWho = %Player%`, and a `CharHereDesc` property whose value is the
  Description `"%CharacterName% is following you."`
  (dump: `A5_DUMP_XML=1 ./test/a5run_dump test/adrift5-games/symphonica.blorb /dev/null`,
  character `Barry` near line 40280 of `/tmp/a5dump.xml`).
- FD's `clsLocation.ViewLocation` (`~/frankendrift/FrankenDrift.Adrift/clsLocation.vb:148`)
  walks `CharactersVisibleAtLocation`, which resolves an `OnCharacter` rider
  through its carrier to the carrier's room, then prints `ch.IsHereDesc`
  (= the `CharHereDesc` property) — yielding "1980s Barry Leitch is following you."

Scarier already mirrors all of this:
- `view_location_impl` (`adrift5/a5text.cpp:3627`) has the identical character
  loop: `a5state_character_visible_at_location` → `char_here_desc`
  (`adrift5/a5text.cpp:3374`, renders `CharHereDesc`, `##CHARNAME##` grouping,
  `is`→`are` pluralisation). The render path is correct and needs no change.
- The runtime On-Character path works: `MoveCharacter … OntoCharacter %Player%`
  resolves `%Player%` via `resolve_specific` (`adrift5/a5run_action.cpp:500`)
  before storing `char_onchar[ci]`, so Rory / Konkey Dong list fine.

## Root cause (single, small)

The **static-model loader stores the carrier key verbatim**:

`adrift5/a5state.cpp:161-164`
```c
else if (streq (cl, "On Character"))
  st->char_onchar[i] = chr_prop (c, "CharOnWho");      // <- "%Player%" literal
else if (streq (cl, "In Character"))
  st->char_onchar[i] = chr_prop (c, "CharInsideWho");  // <- same bug
```

For Barry that stores the literal string `"%Player%"`. Every downstream consumer
then fails to resolve it:
- `a5state_character_location_key` (`a5state.cpp:893`) does
  `a5state_character_index(st, "%Player%")` → **−1** → returns NULL → Barry has
  no location.
- `a5state_character_visible_at_location` (`a5state.cpp:1080`) same lookup → not
  visible → dropped from the room listing.

The object loader already handles exactly this case one screen up
(`a5state.cpp:73-74`): `if (streq (loc->key, "%Player%")) loc->key = "Player";`
The character branch was never given the equivalent.

Only static On-Character followers whose carrier is the *variable* `%Player%`
are affected. Edith's Cats worked because its `CharOnWho` is the literal player
key (`Player`), not the variable. It is a pure loader gap.

## Fix

Normalise the carrier key when decoding the static On/In-Character start state,
mirroring the object path. `st->player_key` is already resolved at this point
(`a5state.cpp:112-120`, above the loop), so resolve to it (do **not** hardcode
`"Player"` — some games rename the player character; the object-path hardcode is
a latent bug we should not copy):

```c
else if (streq (cl, "On Character"))
  st->char_onchar[i] = resolve_carrier (st, chr_prop (c, "CharOnWho"));
else if (streq (cl, "In Character"))
  st->char_onchar[i] = resolve_carrier (st, chr_prop (c, "CharInsideWho"));
```

where a tiny helper (or inline) maps the `%Player%` variable to the real key:
```c
/* Static CharOnWho/CharInsideWho may be the variable "%Player%" rather than a
   concrete character key; the Adrift runner resolves it to Adventure.Player.Key
   in every accessor (clsCharacterLocation.Key). Resolve it once at load so the
   carrier chain (a5state_character_location_key / _visible_at_location) finds it. */
static const char *
resolve_carrier (const a5_state_t *st, const char *k)
{
  return (k != NULL && streq (k, "%Player%")) ? st->player_key : k;
}
```

Belt-and-braces alternative (or in addition): teach
`a5state_character_index` to treat `"%Player%"` as `st->player_key`. That fixes
any other path that reads a raw carrier key, but the loader normalisation is the
minimal, targeted change.

### Also consider (same investigation, likely same fix covers them)
- `%Referenced…%` / other variable carriers in `CharOnWho` — grep the corpus:
  `grep -rl '<Key>CharOnWho</Key>' ` across dumped models. Symphonica only uses
  `%Player%`; if others appear, route them through the same resolver (it already
  passes non-`%Player%` keys through unchanged).
- The FD-format save path `save_fd_game` (`a5run.cpp` ~2782) still writes an
  On-Character follower as `Hidden` (no `OnCharacter` case). Out of scope for the
  room line, but note it — it would corrupt a follower on FD-format export. The
  ScarierExt save path already persists `char_onchar` (committed in `f0eb6fa0`).

## Verification / re-blessing (order matters)

1. `make -f Makefile.headless a5run` from `terps/scarier/`.
2. Re-dump to confirm Barry now resolves to the player's room and the line
   appears once per room:
   `./test/a5run_dump test/adrift5-games/symphonica.blorb test/Symphonica64_walkthrough.txt | grep -c 'Barry Leitch is following you'`
   (expect ~1956, matching FD).
3. `FD_RNG=xoshiro ./test/a5_groundtruth.sh symphonica.blorb test/Symphonica64_walkthrough.txt`
   — the ~1956 follower hunks should vanish, leaving only the residual ~160
   non-follower hunks (Kickstarter flavour text, river-bank prose, inventory
   ordering). Re-measure the exact count.
4. **Re-bless** because Scarier's own output changes:
   - Regenerate `Symphonica64_expected.txt` from the new transcript (Barry lines
     now present) so the strict golden stays vanilla-0.
   - Update the MAP row `Symphonica64|symphonica.blorb|0|<new-xoshiro-count>`.
5. `./test/run_a5_walkthroughs.sh` — **whole suite**, not just Symphonica: this
   loader change touches every game with a static On/In-Character follower.
   Confirm 0 FAIL and no previously-MATCH game regressed. Any game that *gains*
   correct follower lines must be re-blessed too.
6. `A5_SAVE_AT` round-trip must stay byte-identical (Barry's resolved
   `char_onchar` already serialises via the OnChar element).
7. a5 unit tests green.

## Scope reminder

Small, contained loader fix + re-bless. The reason it is a *documented baseline*
rather than already-done is that fixing it re-blesses Symphonica and any other
static-follower game in one pass; do it as its own change so the corpus diff is
attributable.
