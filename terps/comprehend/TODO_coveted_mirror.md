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
- [ ] **Full-transcript diff vs MAME.** Play a scripted walkthrough in both and diff every
      response line. Catch any remaining off-by-N, wrong-table, or `@`-replacement
      (count/noun substitution) mismatches.
  - Walkthrough: `/Users/administrator/Desktop/Comprehend walkthroughs/Mirror.txt`
      (Ambrosine, Classic Adventures Solution Archive).
  - The hourglass timer **does tick per turn even headless** (`>>>SAND'S RUNNING LOW<<<` ->
      `Time's up! You've been caught!` teleports to the throne). So a headless replay is
      possible but only with a strict jailer-bribe cadence; a naive 375-command replay just
      bounces throne<->prison to the 15-strike "THE GAME IS OVER". The non-deterministic
      "keep going S until the barrel" steps also need their counts pinned. Easiest to mirror
      against MAME for the authoritative wording.
  - Banks fully decoded to plain text in this session (standalone ciderpress extractor over
      MIRROR2.DSK): MA-ML, 64 entries each, index read from byte 0. Handy for the diff.
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
  - STILL TODO: full-completion scripts for CM (jailer-bribe cadence) + OO-Topos (alien-pursuit
      cadence) + Crimson/Talisman/Transylvania (route counts + werewolf/eagle hazard handling).

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
- [ ] **DOS NOVEL.EXE registry entries** for CM — none yet (Apple-only so far).

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
