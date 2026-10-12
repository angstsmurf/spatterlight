# The Coveted Mirror — TODO

CM (Polarware Comprehend v2, magic `0x9f8b`, 12 banks MA-ML) in the comprehend terp.
Ground truth = MAME `apple2e -flop1 "…side B (boot).woz"`, press **S** (standard hi-res),
at "TURN THY DISKETTE OVER" `img:load(side A)` via Lua, then space. Debugger
`statesave`/`stateload <name>` = instant reset to the throne room. Headless harness:
`COMPREHEND_SCRIPT=cmds.txt ./comprehend_hl -g covetedmirror "<side B.woz>"`.

CM is playable and winnable end to end (`test/walkthrough/` plays it from the throne to
"Congratulations!!"); the hourglass render is pixel-exact vs
`test/cm/fixtures/throne_sand60.page`. The write-up of how it got there is in the git log.

## Open

- [ ] **Full end-to-end transcript replay vs MAME.** Content + `@` rendering are verified;
      what's left is a deep scripted walkthrough diffed line-by-line against MAME to catch any
      remaining *room/message off-by-N*. Needs the jailer-bribe cadence to drive a run past the
      throne loop. Walkthrough ref: `~/Desktop/Comprehend walkthroughs/Mirror.txt` (Ambrosine).
- [ ] **Eyeball the hourglass grain-fall animation in the Spatterlight GUI.** The timer can't
      run in the headless cheapglk build; wiring mirrors the proven slow-draw path 1:1 and the
      *resting* level is validated, but the falling-grain motion (model b) has no MAME ground
      truth and hasn't been seen in the real GUI. (The animation itself is a keeper — see
      below; this item is just "look at it once in the app".)

## Not applicable — the king's face animation

The king's face does **not** animate in the Comprehend version of CM; that and the other
animated effects exist only in the non-Comprehend release of the game. Nothing to port —
do not add them (the colour-flash XOR at $4284/$428e is the *other* release's code).

The hourglass grain fall is **deliberate and stays**, even though it is ours rather than
something the Comprehend release animates: the resting sand level is validated pixel-exact
against `test/cm/fixtures/throne_sand60.page`, and the falling-grain motion rides the proven
slow-draw path. Don't "correct" it away by appealing to the paragraph above.

## Reference (verified)

- VM data loads at **$5a00**; variables base **$5a3f** (16-bit each), flags array base $5b3f,
  current-room byte $5a3d, functions block base $7bec. Hourglass = var 0x11 ($5a61), displayed
  counter @ $42f7. Per-command dispatch $4092; main loop $404c; daemon = function 0.
- startRoom = **0x12 (18)** = throne (S1[67], only exit S→0x11). **Room 1 = prison** (S1[80],
  exits S→6, W→2). Voar = ITEM[159], word 0x32 (`voar`/`king`), room 0x12.
- Bytecode operands are 1-BASED item numbers; PRINT string refs `(idx, 8x)`: 0x82→S2[idx],
  0x83→S2[idx+0x100], 0x84→S2[idx+0x200].
- Mirror pieces: var 7 counts, 4 needed (var 8). Rooms 0x57 / 0x16 / 0x01 / 0x0b (gated by
  flags 0x15, 2/0x1b). Endgame: weight ≤ var9 for the Perilous Pass squeeze, WEAR COAT for the
  summit, plateau 0x51 WAIT×7 + GET MIRROR when var0x18==var0x1b(7).
- Bribe items: BROOM/PICTURE/COOKIE/FLOWER/TELESCOPE/AX/JUG; Bernt's cookies (GIVE MOOSE at
  bakery 0x32) are the renewable bribe. Riddle at Winder's → answer `UNCLE`.
