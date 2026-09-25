# Space Mercenary (smercenary) — walkthrough (**best reachable: Lork audience, no win exists**)

- **Engine:** ADRIFT 4.00. "Space Mercenary" v0.1 (13 April 2013) by Duncan
  Bowsman. It is a menu-driven sci-fi sandbox: almost every command is an option number.
- **Result:** there is **no win**. The game has no score actions (max 0) and an
  empty WINTEXT. All 27 EndGame actions (Var1=3) are deaths or failures. The
  furthest story beat is the audience with Lork, the Ghlarskean warlord, which is triggered by turning
  in a 3rd schematic at the IMPERIAL WAR ROOM (task 161/175, GHQ=4). Lork offers a
  menu, "1) Accept politely or indifferently. 2) Remain standing, defiant.", but
  nothing handles it. Both `1` and `2` answer "Please enter the number of an
  available option." This is the unimplemented end of the v0.1 demo.
  The route ends on that menu. It is 63 lines, and three row.sh runs are byte-identical.
- **Proposed row:**
  `smercenary_solution.txt|smercenary.taf||` (empty marker: there is no win).
  If a depth marker is ever wanted, "Remain standing, defiant" (Lork's menu)
  appears exactly once, on its own line, and only at this point.
- **Env:** `SCR_RNG=xoshiro` (row.sh sets it anyway). Starline puzzles are
  random, and so are the rumour replies. The route is tuned to the xoshiro stream, so
  **do not insert or remove any line** before the last job.

## Route by phase

1. **Title/prologue (lines 1–2).** `1` starts the game. A blank line answers the
   begin waitkey.
2. **Orbit (lines 3–8).** `1`, `1` to land at Ghlarsk V. The checkpoint patrol then
   asks what you do. Choose `3` (allow the search: lifesys −25%). Option 2
   (question the authorities) is a death. Three blank lines answer three waitkeys.
3. **Spaceport → Square (line 9).** `1` heads out to the Square.
4. **Rumours (lines 10–20).** `1` opens the rumour menu. Lines 11–17 are seven
   "ask around"/"loiter" tries that all print "You are mostly ignored". They are
   harmless, but they **pad the RNG stream**. Without them the later starline combos
   differ and the route breaks, so keep them. Then use `3` (bribe, 100 merits)
   twice. The first bribe unlocks Dridbel (trade). The second gives the password
   DOWN WITH LORK and unlocks "Find work". `5` leaves the menu.
5. **Job 1: earn the decryptor (lines 21–31).** `3` (find work), `down with lork`,
   `y`, `y`, then a blank for "[press almost any key to begin]". Five rounds of
   "How many spikes?" follow. The answer is the total number of `i` characters in the two
   starline lines. Finishing the job awards the COVERT DECRYPTOR, and a blank answers its waitkey.
6. **Jobs 2–4: three schematics (lines 32–63).** With the decryptor, `3` at the
   Square starts a job immediately (task 131). A perfect five-answer job gives
   "You have gained a schematic!" plus a waitkey blank. Then `4` (War Room),
   `2` (turn in the schematic, +200 merits), blank, and `2` (nothing) returns to the Square.
   The third turn-in triggers "Lork would like a word with you", and its waitkey blank
   leads to the Lork scene and the dead-end menu.

## Engine / authoring oddities

- **`<waitkey>` eats a line.** Every "[press almost any key ...]" needs a
  blank line in the solution, or it swallows the next command.
- **Authoring bug in starline combo (1,2).** The correct answer is 3. Task 354
  (rounds 1–4, WRONG branch) uses the restriction "number < 5", so answer 3 is
  always WRONG, and answers of 5 or more fall through to the "Please enter the number"
  fallback. Task 360 (round 5, no decryptor, RIGHT) tests `== 2` instead of
  `== 3`. A perfect job is therefore impossible whenever combo (1,2) comes up in
  rounds 1–4. The route avoids that through RNG padding (the rumour asks). This is the
  game's own data (see the restrictions in the SCR_DUMP_TASKS output), not a Scarier bug.
- **Stale first display with the decryptor.** Task 131 runs the job initiator
  (337) without the display sequencer. When a decryptor job starts directly, its
  first display can therefore show the previous job's starline. In this route, the answers
  in lines 33–37, 44–49 and 56–61 were verified against the actual transcript.
- **Ghlarskean HQ (room 1) and Zoist HQ (room 4) have no tasks.** There is no Zoist
  progression in v0.1.
- There is no score, so the end-of-game score summary never prints. `quit` ends the run.
