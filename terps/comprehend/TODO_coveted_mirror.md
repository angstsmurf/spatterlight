# The Coveted Mirror — TODO

Status of getting CM (Polarware Comprehend v2, magic `0x9f8b`, 12 banks MA-ML) fully
playable in the comprehend terp. Ground truth = MAME `apple2e -flop1 "…side B (boot).woz"`,
press **S** (standard hi-res), at "TURN THY DISKETTE OVER" `img:load(side A)` via Lua,
then space. Debugger `statesave`/`stateload <name>` = instant reset to the throne room.
Headless harness: `COMPREHEND_SCRIPT=cmds.txt ./comprehend_hl -g covetedmirror "<side B.woz>"`.

---

## 0. Done this session (NOT yet committed, on `glkcontroller`)

- [x] **String banks off by 2** — `_strings2` loaded 2 entries early vs the indices G0
      uses. Fix: 2 leading placeholders for `covetedmirror` in
      `game_data.cpp:load_extra_string_files`. Verified vs MAME: crown→"'Tis studded
      with jewels.", robe→"It's imperial.", broom→"It's like new…", imprisonment line
      decodes at 312.
- [x] **stringLookup missing table bytes `0x03`/`0x04`** — 73 item longStrings → `BAD_STRING`.
      Fix: added `case 0x03`/`0x04` to the fall-through chain in `game.cpp:stringLookup`.
- [x] **Commit these two fixes** (commit `15245d95`, message: "COMPREHEND: Fix Coveted
      Mirror string-bank alignment and 0x03/0x04 lookup").

---

## 1. HIGH — gameplay blockers

- [x] **Throne-room imprisonment** — SOLVED (not yet committed). The trigger is **engine
      code, not bytecode**, which is why nothing in G0 calls FUNC 000a. RE'd from the live
      Apple II RAM dump (`cm_ram_raw.bin` in Ghidra) at **$4092**, the per-command dispatch:
      ```
      LDA $5a3d ; current room   (header current-room byte, = startRoom 0x12)
      CMP #$12  ; throne?
      BNE normal
      LDA #$05  ; flag 5
      JSR $43b8 ; test_flag (flags array base $5b3f, mask table $43d6)
      BNE normal ; flag 5 set -> ordinary play
      LDA #$0a STA $5b64 / LDA #0 STA $5b65 ; force function $000a...
      JMP $40b1 ; ...run it, BYPASSING normal action dispatch ($470c)
      ```
      So in the throne (room 0x12) with flag 5 clear, the parsed command is **discarded** and
      FUNC 000a runs (insolence line + move to prison room 1). Flag 5 is later set by
      **FUNC 0028** (gated on flag 0x22) once the player progresses, re-opening the throne;
      the daemon FUNC 0000 conditionally clears it each turn.
  - VM facts (for future CM work): the game data loads at **$5a00**; header `addr_vm`=$5a0e
    → functions block base **$7bec** (FUNC 000a body at $7ce1). Function index to run lives
    in **$5b64(hi):$5b65(lo)**; `$5195` walks the null-terminated function list to that index,
    `$51b2` is the bytecode-byte fetcher (zp ptr $fa/$fb), `$50bc`/`$50a5` run it. The daemon
    runs function 0 (chain 0→1→…→9→**0b**→0c→0d/0e→017f; **000a is skipped by the chain**).
  - **Implemented**: `CovetedMirrorGame::handleAction` override (game_cm.cpp) replicates the
    $4092 check: `if (_currentRoom == 0x12 && !_flags[5]) eval_function(0x0a, nullptr)` then
    daemon, bypassing the matched action. Verified headless: `examine crown`, `south`, and
    `xyzzy` as the first command all print the insolence line and land in the prison.
  - **Also fixed (prereq)**: V2 `OPCODE_SET/CLEAR_INVISIBLE` used `get_item_by_noun(noun)`,
    which null-derefs (segfault) when a daemon toggles items by literal index with no noun —
    FUNC 017f does `a9 0e a9 13 …`. Changed to `getItem(instr)` (the operand; noun-forms are
    already noun-substituted into operand[0] above). Latent CM bug independent of the throne
    (017f is reached in rooms 6/9/0e/10/15/24/2d via FUNC 000e too). V1 base handlers left
    untouched; OO-Topos/Talisman/Transylvania-v2 still boot + parse clean.
  - **VERIFIED vs MAME (2026-06-12)**: the doubled "only" ("I'll only allow it **only** 15
    times!") is GENUINE — MAME apple2e prints the exact same wording. Not an `@` artifact;
    no fix needed. (The "@" in template 0x8338 is the count, rendered "1"; the "only…only"
    is just the original author's text.)

- [x] **`look` / `l` / garbage with no object — RESOLVED, no code change (2026-06-12).**
      Verified vs MAME apple2e:
  - Bare **`look`** (full word) → "I don't understand what you want to look at." in BOTH
    MAME and our terp. The original does NOT redraw the room on bare look. FAITHFUL.
  - **`l` and other garbage**: the original engine RE-RUNS THE LAST VERB with no noun when
    the input parses no verb into slot 0 (RE'd in `cm_ram_raw` parser `FUN_4742` @47b0: when
    the freshly-parsed verb slot `DAT_4b91[0]==0` and separator gate `DAT_4b8f==1`, the copy
    into the sentence buffer starts at index 1, leaving the *previous* verb in `DAT_5b5f[0]`).
    So `l` after `look` reproduces look's no-noun message (looks like a "repeat"); `l` after
    `examine` reproduces examine's no-noun message (the generic "I'm sorry, I don't
    understand", so it looks like ordinary garbage). Deterministic, not flaky.
  - **Our terp already deliberately suppresses this** (commit `cbe85e23` "COMPREHEND: Fix
    unintelligible command repeating the last action", `game.cpp:1031-1045`): all-garbage
    input replaces the sentence wholesale instead of keeping the stale verb, so we report
    "not understood" rather than silently re-running the last command. WORKING AS INTENDED.

---

## 2. MEDIUM — message/string correctness

- [x] **Stuck number-mode `@` substitution — FIXED (2026-06-12, not committed).** `examine
      <absent noun>` printed "**1 here.**" instead of MAME's "**There isn't one here.**". Cause:
      `OPCODE_SET_STRING_REPLACEMENT3` (game_opcodes.cpp ~867) selects a *word* replacement
      (here `_replaceWords[2]` = the phrase "There isn't one") but, unlike
      `OPCODE_SET_STRING_REPLACEMENT2`, never cleared `_replaceWordIsNumber`. So the
      number-mode flag left set by the throne-room insolence message
      (`OPCODE_SET_STRING_REPLACEMENT1`, var 6 = count 1) persisted, and the template
      "@ here." rendered `@` as the decimal `1` rather than the word. Fix: add
      `_replaceWordIsNumber = false;` to the REPLACEMENT3 handler (mirrors REPLACEMENT2 and
      the original's single-byte selector whose high bit = number mode). Verified: now prints
      "There isn't one here." headless; OO-Topos (examine door) + Talisman (boot/look/wait)
      unregressed. NOTE: `OPCODE_SET_CURRENT_NOUN_STRING_REPLACEMENT` (the `error("TODO")`
      stub at ~664) would need the same `_replaceWordIsNumber = false` if ever implemented.

- [x] **Validate page 0x84 (banks MI-ML) alignment — DONE (2026-06-13).** Decoded all
      page-0x84 item longStrings (offsets 0x401-0x4c7): 45/46 are coherent English and the
      medallion (ML[5]=711, room 2) / crown / robe / imprisonment all match MAME, so the +2
      shift + uniform-64 stride is correct for page 0x84. Verified vs MAME by poking the
      current-room byte `$5a3d`=0x0a (the throne guard only fires in room 0x12, so this
      teleports freely past the imprisonment loop without solving the early puzzle).

- [x] **BUG: `examine fish` printed "thvg" instead of "Not fit for human consumption!"
      — FIXED (2026-06-13, not committed).** The MA-ML bank files have **no 4-byte header**:
      the first 4 bytes are already `index[0]`/`index[1]` (verified by extracting the banks
      from MIRROR2.DSK: MJ begins `82 00 84 00 93 00 ...`, a monotonically-ascending offset
      table from byte 0). The shared reader did `fb.seek(4)`, skipping the first two index
      entries of every bank and reading two *out-of-range* entries at each bank's tail; the
      global `+2 _strings2` pad cancelled this for mid-bank strings but mis-mapped each
      bank's leading **two** entries (game index N where `N % 64 ∈ {0,1}`) to the previous
      bank's garbage tail. The fish is **MJ[1] @0x0093** (global index 577); the buggy map
      sent 577 → MI local slot 63 → file offset `0x00c8` → "thvg".
  - **Fix** (`game_data.cpp`): for `covetedmirror` only, read the bank index from byte 0
      (`fb.seek(noHeader ? 0 : 4)`) and drop the +2 leading pad. The seek offset and the pad
      cancel for every mid-bank string, so the mapping is *identical* except for the
      `N % 64 ∈ {0,1}` leading pairs, which now resolve correctly.
  - **Verified end-to-end**: dumped live `_strings2` — `S2[577] = "Not fit for human
      consumption!"` (was garbage); known-good mid-bank unchanged (`S2[2]`="'Tis studded
      with jewels.", `S2[10]`="It's imperial."). CM/OO-Topos/Transylvania-1985 all still boot
      and parse clean (OO-Topos/Talisman/Transylvania keep `seek(4)` via the gameid guard).
      Cross-checked the bank decode with a standalone ciderpress-based extractor over the
      same DSK, which is byte-identical to MAME's source data.
  - **Full blast radius measured**: reconstructed the entire `_strings2` array under both the
      old (seek 4 + 2 pad) and new (seek 0 + no pad) schemes and diffed all 768 entries.
      Exactly **24 strings changed = the 12 banks x their 2 leading entries** (global index
      N where `N%64 ∈ {0,1}`); **0 mid-bank strings changed**. Every old value was garbage or
      an empty pad; every new value is coherent. So the fix also repaired other reachable
      bank-leading messages, e.g. the game-over line ML[1] (G=705) "ENOUGH OF THIS! THY QUESTS
      SHALL CEASE! THE GAME IS OVER." (was `<no-string>`) and the death line MH[0] (G=448)
      "No coat! You sink into a frozen slumber...". The fish was just the one we happened to
      notice.
- [~] **Full-transcript diff vs MAME.** Play a scripted walkthrough in both and diff every
      response line. Catch any remaining off-by-N, wrong-table, or `@`-replacement
      (count/noun substitution) mismatches.
  - **`@`-SUBSTITUTION half DONE (2026-06-13) -- verified faithful against the live Apple II
      RAM dump in Ghidra (`cm_ram_raw.bin`), not just MAME screen-reading.** CM has exactly
      **three** templates containing `@` (confirmed by scanning the decoded string tables):
      `S1[25]`="@ here.", `S1[55]`="@ not yours to take.", `S2[312]`= the throne insolence
      count line. The marker is filled from a **replace-word table** the engine parses from the
      game data; dumped it live and found the meaningful entries are RW[0..12]:
      `[0..3]` negative-existence ("She's not"/"He's not"/"There isn't one"/"There aren't any"),
      `[4..7]` subject pronouns ("She's"/"He's"/"It's"/"They're"), `[8..11]` object pronouns
      ("her"/"him"/"it"/"them"), `[12]`="i". Located the SAME table verbatim in RAM at **$b9da**
      (a second copy at $ba84) -- byte-for-byte match, so the table load is faithful.
  - Mechanism confirmed correct: `OPCODE_SET_STRING_REPLACEMENT3` computes `articleNum` (0..3 =
      female/male/neuter/plural) from the noun's `_wordFlags` gender bits and sets
      `_currentReplaceWord = operand[0] + articleNum - 1`; `operand[0]` selects the set base
      (1 -> negatives, 5 -> subject pronouns). So `@ here.` -> "There isn't one here." (neuter,
      RW[2]) and `@ not yours to take.` -> "It's/He's/She's/They're not yours to take." by
      gender. `@`-as-number (`SET_STRING_REPLACEMENT1`) handles the throne count.
  - **Status of the 3 templates**: `S1[25]` both modes confirmed live (renders "There isn't one
      here.", no stale number-mode leak); `S2[312]` count was MAME-confirmed earlier (the
      doubled "only...only" is genuine). **`S1[55]` NOW CONFIRMED LIVE END-TO-END (2026-06-13).**
      Found the real triggers via a static bytecode scan: five take-handler functions (FUNC
      62/88/238/264/373) print `_strings[55]` with a preceding `c5 05` (REPLACEMENT3 **base 5** =
      subject pronouns), reached by verb "take/get" (dict idx 52) + nouns pitcher/book/bottle/
      stick/necklace. FUNC 88 (book) gates on room **0x2e** (the Abbott's); injecting `GET BOOK`
      there in the full walkthrough renders **"It's not yours to take."** -- the neuter article
      (RW[6]="It's", base 5 + articleNum 2). This exercises the base-5 branch that `S1[25]`
      (base-0 negatives) couldn't, so the whole REPLACEMENT3 gender/article path is now verified
      at runtime, not just by inspection. No MAME navigation needed (template text + RW table
      were already byte-matched against the live RAM dump at $b9da).
  - **Dead end ruled out**: `OPCODE_SET_CURRENT_NOUN_STRING_REPLACEMENT` is an `error("TODO")`
      stub, but it is **never mapped to any bytecode byte** in either V1/V2 opcode map -- it is
      unreachable dead code, so it is NOT a latent crash. (error() is non-fatal anyway.)
  - **Minor benign note**: the non-Talisman replace-word parser reads past RW[12] into a
      near-duplicate pronoun block + a few high-bit/binary bytes (RW[13..~45]); CM's table has
      no empty-string terminator before the duplicate. Harmless -- the gender/article path only
      ever indexes RW[0..11]; left as-is to avoid regressing the other games' terminator-based
      parse.
  - STILL TODO for full closure: a real end-to-end transcript replay vs MAME to catch any
      *room/message off-by-N* beyond the (now-cleared) string content and `@` rendering -- needs
      the jailer-bribe cadence to get a deep walkthrough past the throne loop.
  - Walkthrough: `/Users/administrator/Desktop/Comprehend walkthroughs/Mirror.txt`
      (Ambrosine, Classic Adventures Solution Archive).
  - The hourglass timer **does tick per turn even headless** (`>>>SAND'S RUNNING LOW<<<` ->
      `Time's up! You've been caught!` teleports to the throne). So a headless replay is
      possible but only with a strict jailer-bribe cadence; a naive 375-command replay just
      bounces throne<->prison to the 15-strike "THE GAME IS OVER". The non-deterministic
      "keep going S until the barrel" steps also need their counts pinned. Easiest to mirror
      against MAME for the authoritative wording.
  - **JAILER-BRIBE CADENCE worked out (2026-06-13).** Mechanic, fully measured:
      - The hourglass is **variable 0x11** (17). It starts at **74** and the daemon (function
        0) decrements it by 1 each turn. `>>>SAND'S RUNNING LOW<<<` prints at ~27 left; at 0 the
        jailer teleports you to the throne (a "strike", variable 6++). 15 strikes -> game over.
      - **Budget ~= 74 ticking turns** between bribes (measured: warning at turn 47, caught at
        71, plus the 3 escape moves ~= 74). Every recognised command ticks (moves, WAIT, GET,
        LOOK, I); only *unparsed* garbage ticks irregularly.
      - **Bribe** = be at the barrel hub, then `GO BARREL, E, WAIT, GIVE <item>` (jailer Boris
        accepts: "Ye can stay out of prison 'til the hourglass runs empty."), which **resets the
        sand to 74**. Exit back to the world with `MOVE BED, GO HOLE, GO BARREL`. The timer is
        **paused** while inside the barrel/cell bribe scene (those turns don't tick).
      - Bribe items (any one): **BROOM, PICTURE, COOKIE, FLOWER, TELESCOPE, AX, JUG**.
  - **PREREQUISITE FIX (committed 845e9d5d).** The budget was only ~37, not ~74: CM's daemon
      ran **twice per turn** (`ComprehendGame::beforeTurn` + `handleAction`), so the sand drained
      at 2/turn. RE of `cm_main_command_loop` $404c shows the original runs function 0 exactly
      once per turn (in the post-action dispatch `cm_vm_dispatch_thunk` $4f80), with no pre-parser
      daemon pass. Fixed with a CM-only empty `beforeTurn()` override. Now ~74, faithful.
  - **Cadence consequence (measured against Ambrosine):** even at the corrected 74-turn budget,
      Ambrosine's collection loop between the explicit `GIVE AX` and `GIVE TELESCOPE` bribes is
      **>74 ticking turns**, so a non-stop replay is caught ~74 turns after GIVE AX (confirmed:
      first catch at route command #101, ~74 ticks past the bribe -- pure timer, not a nav
      desync). Ambrosine's walkthrough relies on the human "keep an eye on the hourglass" note to
      insert *extra* opportunistic bribes. So a deterministic completion script must **add
      intermediate barrel detours/bribes** wherever a loop would exceed ~70 turns (stay ~5 turns
      under for safety), and pin the non-deterministic steps (MERMAID/Starina/man-appears WAITs,
      "keep going S until the barrel" counts). That scripting is the remaining authoring work --
      now unblocked by the timer fix.
  - Banks fully decoded to plain text in this session (standalone ciderpress extractor over
      MIRROR2.DSK): MA-ML, 64 entries each, index read from byte 0. Handy for the diff.
  - **STRING-CONTENT half DONE offline (2026-06-13).** Rather than wait on the MAME replay,
      dumped the terp's *actual* runtime `_strings`/`_strings2` (via a temp env-gated hook in
      `loadGame`, since reverted/uncommitted) and auto-scanned every entry for control bytes,
      high bytes and consonant-cluster garbage. **All reachable CM strings are clean** -- no
      remaining garble like the fish bug. The only junk is the **unreachable ML bank tail
      S2[745..767]**: bank ML holds just 41 real strings (S2[704..744], last = "There isn't a
      window to look out of here."), and the loader's fixed 64-entry-per-bank read overruns the
      offset table into the string blob, so those surplus offsets decode to fragments of real
      strings (e.g. " in the wall." is a slice of S2[742]). Max item/room reference is S2[736]
      ("Oink!") and no instruction can reference a string past the bank's real count, so the
      tail is never shown. `S2[516]`="zzzzzzzzzz" is a *legit* snore SFX (follows "Don't let the
      bedbugs bite."). Ran the same scan on OO-Topos/Crimson/Talisman/Transylvania: 0 real hits
      (all flags were onomatopoeia -- "Hmmm", "Rrrrrr", "Grrrrrr", "Phhffffff"). So string
      *content* is validated across all five games; what still needs MAME is the **runtime
      rendering** -- the `@` count/noun substitution and template assembly, which a content scan
      can't see.
- [~] **Text regression harness ADDED (2026-06-13): `test/walkthrough/`** (geas-style:
      `run_walkthrough.sh` plays a raw command script through `comprehend_hl` and greps the
      transcript for a win marker; `run_walkthroughs.sh` prints a PASS/FAIL table; games stay
      external). Committed a deterministic, timeout-free CM smoke script
      (`scripts/covetedmirror.txt`) that plays out of the cell, through town, and solves the
      astrologer's constellation puzzle (marker "My horoscope was horrible.") -- exercises
      string-bank decoding, the parser, NPC dialogue and the throne override.
  - **OO-Topos smoke prefix ADDED (2026-06-13): `scripts/ootopos.txt`** (marker "Pushing the
      green button causes the panel to swing open."). Plays out of the prison cell through the
      3-stage BREAK LOCK counter, the OPEN DOOR siren event and the scripted stun-capture that
      drags you to the guard post, then arms up at the button panel. Verified deterministic
      (identical transcript across runs) and wired into `run_walkthroughs.sh` (CM+OO both PASS).
      Stops at the panel **on purpose**: stepping E out of the guard post triggers the alien
      stun that strips every item, and surviving OO-Topos's pursuit needs a turn-by-turn
      cadence still to be pinned against MAME.
  - **Crimson/Talisman/Transylvania smoke prefixes ADDED (2026-06-13).** All three boot and
      parse as Apple woz-a-day disks via the existing gameids:
      - `scripts/crimsoncrown.txt` (`crimsoncrown`, side A) -- party falls through the trap
        door into the crypt and reaches the crystal-ball cave (marker "On a stalagmite rests
        a pulsating crystal ball.").
      - `scripts/talisman.txt` (`talisman`, side 1 Boot) -- WAIT x4 + BOW reprieve cutscene to
        the King's audience chamber (marker "Ye art a guest in the King's audience chamber.").
      - `scripts/transylvania.txt` (`transylvania`, 1985 side A) -- answers the guest register,
        walks to the dark forest, reads the note (marker "Sabrina dies at dawn!"). NOTE: the
        1982/1984 woz and bare gameid otherwise fall through to DOS tr.gda/novel.exe loading;
        1985 side A is the clean Apple boot.
      All verified deterministic (identical transcripts across runs) and wired into
      `run_walkthroughs.sh` -- **the whole table is now 5/5 PASS** (CM, OO, CC, Talisman, Trans).
  - **CM script EXTENDED through the mid-game with the bribe cadence (2026-06-13).**
      `scripts/covetedmirror.txt` no longer stops at the constellation puzzle; it now plays the
      whole early/mid game and **rides the hourglass** -- marker moved to
      `"Ah, thou hast made a friend for life!"` (the innkeeper's grain reward, deep in the
      eastern map). New facts pinned while authoring it:
      - **Barrel hub = "Outskirts of town"** (the south end of the N/S town spine: Northgate ->
        North Castle St -> Town Square -> South Castle St -> Outskirts -> [S] stockade).
        `GO BARREL` only works there. The bribe macro from the hub is
        `GO BARREL, E, WAIT, GIVE <item>, MOVE BED, GO HOLE, GO BARREL` and **returns you to
        the hub**; it resets sand (var 0x11) to 74 and the barrel scene itself is paused.
      - The eastern collection loop **passes back through the hub** (3rd `S` from the
        blacksmith), so a bribe inserts inline without a detour. The committed script bribes
        twice -- `GIVE AX` early, `GIVE TELESCOPE` before the eastern loop -- and never drops
        below a healthy margin (measured: sand stays >=40 the whole run).
      - **Riddle at Winder's is FIXED in this build -> answer `UNCLE`** (the Henry/Margot/Alice
        kinship variant: "Henry is Adam's uncle"). Ambrosine's `MERMAID` is a *different random
        variant* that doesn't occur in the deterministic headless run -- don't copy it verbatim.
      - **Cadence-authoring tool:** drop `if (getenv("CM_DEBUG_SAND")) fprintf(stderr,
        "[SAND=%d room=%02x]\n", _variables[0x11], _currentRoom);` into
        `CovetedMirrorGame::beforeTurn()` for a per-turn sand/room trace (used to author the
        cadence; reverted before commit). `_variables[0x11]` is the live hourglass.
  - **CM command opcode `0xc6` NOW ENABLED (2026-06-13).** It fires at the **Color Fairy**
      `MOVE GLASSES` spell grant (room 0x26). It is `OPCODE_SET_OBJECT_GRAPHIC` (item->_graphic =
      operand[1]; flags UPDATE_GRAPHICS) -- the handler already existed in the shared opcode
      switch but the V2 map left it in an `#if 0` block, so it warned "Unhandled command opcode
      c6". Mapped it in `ComprehendGameV2::ComprehendGameV2()` (one line; left 0x9e/0xf0/0xfc
      `#if 0` since those are unverified for V2). Text play is unchanged (graphic-only); the
      warning is gone; 5/5 walkthroughs still PASS.
  - [x] **CM COMPLETED END-TO-END (2026-06-13).** `scripts/covetedmirror.txt` now plays from the
      throne to **"Congratulations!!"** on the Peak of Shards (the runner's win marker). Required
      RE'ing and implementing three missing ENGINE systems (see section 5 below) plus fixing two
      V2 opcode-fidelity bugs. The 5-game PASS table still holds. Remaining script work is only
      the other games' full completions (OO alien-pursuit cadence, CC/Talisman/Trans routes).
  - Full-game mechanics, all RE'd from cm_ram_raw + the parsed bytecode (operands are 1-BASED
      item numbers; PRINT string refs are `(idx, 8x)` with 0x82→S2[idx], 0x83→S2[idx+0x100],
      0x84→S2[idx+0x200]):
      - **Mirror pieces**: var 7 counts them, 4 needed (var 8). Rooms: maze treasure area 0x57
        (GET MIRROR), colorful art room 0x16 (USE COLOR SPELL, FUNC 151), prison cell 0x01
        (EXAMINE MIRROR; needs flag 0x15 from GIVE HORSESHOE to the WAIT-summoned man in castle
        room 0x0d -- the "piece is right in thy room" riddle), castle chapel 0x0b (EXAMINE
        MIRROR; needs flag 2 from READ BOOK at the Abbott's 0x2e, which needs **WEAR MEDALLION**
        (flag 0x1b) -- the alcove medallion's whole purpose; the Abbott "recognizes your
        medallion and leaves quietly").
      - **Brother Jon**: when var7==var8(4), FUNC 0002 moves him into the tavern 0x24 and removes
        the pickpocket trio; TALK (medallion OFF -- it makes townsfolk avoid you) = the sign
        class + the RING (sets flag 9, thieves return).
      - **Deaf mute**: WAIT at the Forest of No Return's edge 0x42 (E of the well) summons him;
        WEAR RING (flag 0x26, FUNC 147) then TALK sets flag 4; the Impenetrable Mist (0x46,
        FUNC 00f movement handler) then auto-guides you to the bridge.
      - **Endgame**: drop to weight <= var[9] for the Perilous Pass squeeze 0x4c, WEAR COAT
        (from Sue's, GIVE FISH) for the freezing summit, plateau 0x51: WAIT x7 cycles var 0x18
        through the shard slots, GET MIRROR wins when var18==var1b(7) -> "Congratulations!!" +
        SPECIAL 1; a wrong GET = eagle alarm, teleport to Voar, var18=0.
      - **Sand economy**: catch at sand==0 (not 14), warning at 24 (var 0x0b). Bribe values are
        per-item vars (ax 74, telescope 124, jug/picture ~96, broom only ~21, cookies ~72).
        WAIT in the cell summons Boris and ZEROES the sand; two WAITs make him doze off
        ("As the day waxeth late") for a no-item 35-sand refill (var 0x64). **Bernt's cookies
        (GIVE MOOSE at the bakery 0x32, E of South Castle St) are infinitely refillable**
        ("just help thyself") -- the renewable bribe that funds the endgame.
      - **PICTURE "puzzle" is a red herring**: GET PICTURE at Lady Vainly's (0x3c) ALWAYS
        succeeds (FUNC 115 takes it, then prints the "Admire it well" flavor line). Ambrosine's
        "BEAR" step is noise/another variant. Verified in MAME, identical behaviour.

---

## 3. LOW — graphics / presentation

- [ ] **Right panel**: the logo + hourglass are engine-drawn (not in pic data) and not
      implemented in the terp. Decide whether to render or leave blank.
- [ ] **King's face animation / hourglass tick** — event-driven engine effects (brush
      blitter $0FFF, XOR mode $1024 in CM's T2), not in pic streams. Out of scope unless
      a faithful idle screen is wanted.

---

## 4. Robustness / housekeeping

- [x] **Side-A-only crash — FIXED (2026-06-13).** Two parts: (a) `FileBuffer` ctor threw
      `std::length_error` on `resize(f.size()==-1)` — fixed by commit `0cb1e697` (guards the
      failed open + negative size). (b) That left a *second* crash: with `g0` absent the
      empty FileBuffer flowed on through `parse_header` (magic 0000), every non-fatal
      `error()` soft-returned, and parsing empty data tripped `assert(...)` in
      `parse_function` (SIGABRT). Fixed by guarding `loadGameData`: if the main data file is
      `< 4` bytes, print a clear error and `glk_exit()` instead of parsing garbage. Verified:
      isolated `side A.woz` now exits cleanly (code 0, "No usable game data in 'g0' -- is a
      disk side missing?") instead of aborting; CM/OO-Topos with both sides still boot.
- [N/A] **DOS NOVEL.EXE registry entries** for CM — confirmed not applicable (2026-06-13). No
      DOS/PC release of The Coveted Mirror exists (checked the local game corpus; only OO-Topos,
      Transylvania and Talisman ship a DOS `g0`+NOVEL.EXE). `gameIdForMagic`/`guessDosGameId` in
      `apple2.cpp` already map magic 0x9f8b -> "covetedmirror", so if a DOS CM g0 ever surfaces
      with the same magic it is auto-detected with no code change; nothing to add.

---

## 5. Engine systems implemented & opcode fixes (2026-06-13)

- [x] **Wandering-NPC spawn engine** (`game_cm.cpp:spawnWanderingNPCs`, RE'd from the room-entry
      block inside `cm_handle_special_opcode` $4150-$4283): on entering a NEW room (last spawn
      room = $4037) the engine removes that room's wanderers and rolls $5b69 to respawn one.
      Hardcoded: room 0x2b {witch et al 0x16/0x4f/0x18}, **0x34 {0x1f/0x20/0x22 -- 0x22 is the
      catchable SHADOW** (GET SHADOW = FUNC 0ce: needs it present + the vase; "sealed in the
      vase")}, 0x35 {0x23/0x24}, 0x38 {0x27/0x46}, 0x3b {0x2b/0x2c}, else a 9-room table
      ($4254/$425d/$4266: rooms/items/thresholds, appear when rand > threshold). This is the
      "go N,S,N,S until Starina shows up" mechanic; without it the shadow NEVER appears and the
      witch's brew (bones + shadow -> in/visibility spells) is unobtainable.
- [x] **Special opcodes** (`game_cm.cpp:handleSpecialOpcode`, from $4140): 1 = THE END win,
      2 = 15-strike game over (both -> restart/quit prompt; the banks have NO restart string --
      the original freezes -- so `handle_restart` prints a literal), 8 = tavern pickpockets move
      ALL carried items to room 0x1d, 0xc = Voar confiscates carried items to the treasure room
      0x5e (= the maze treasure room; matches the magician's diary), 6/7 = jousting/fishing
      animated side-shows (graphics-only, not implemented), 9 = colour-spell flash (graphics).
- [x] **V2 opcode 0x11 was INVERTED** (upstream ScummVM bug): the original handler ($51d9,
      dispatch table $5021/handlers $4f93) is `CMP #$ff / BNE -> true` = OBJECT_IS_**NOT**_NOWHERE;
      0x51 (|0x40) is the nowhere test. ScummVM's V2 map had OBJECT_IS_NOWHERE (and no V2
      executor case at all, so it errored). Fixed; this is what gates Brother Jon's arrival
      (`51 35` = "Jon is nowhere" in FUNC 0002).
- [x] **V2 opcode 0x15 (INVENTORY_FULL_X) fixed**: the original ($5265) is "total inventory
      weight > variables[operand0]" (vars at $5a3f, 2 bytes each) -- no noun weight, limit from
      operand 0 (the Perilous Pass squeeze is `15 09`). ScummVM added the noun's weight and read
      the limit from operand[1] (always 0 here), which blocked the pass with ANY item carried.
- [x] Misc RE anchors: VM variables base **$5a3f** (16-bit each); VAR_EQ2/VAR_ADD etc. are
      var-vs-VAR (the engine stores constants in variables: var8=4 pieces, var9=squeeze limit,
      var 0x0b=24 warning, var 0x0e=0 catch, var 0x64=35 doze refill, var1b=7 winning shard).
      FUNC 066 = the giant TALK/examine dispatcher; FUNC 00f = the per-room movement overrides
      (mist guide gate, cold rooms 0x4d/0x4e -> throne traps, squeeze, peak exits).

---

## Reference (verified this session)

- startRoom = **0x12 (18)** = throne ("Thou art in the presence of the mighty King Voar.",
  S1[67]); only exit S→0x11.
- **Room 1 = the prison** (stringDesc 0x50 → S1[80] "You are in a dank, desolate prison
  tower."); exits S→6, W→2.
- Voar = ITEM[159], word 0x32 (`voar`/`king` both 0x32), room 0x12, longString 0x0201
  (→ now empty after +2; his "examine" is the imprisonment, not a describe).
- dict: `examine`=0x39 (verb), `voar`/`king`=0x32 (male noun). The action `w46 w32` is
  *move/lift* voar (0x46=`lift`/`move`), not examine.
