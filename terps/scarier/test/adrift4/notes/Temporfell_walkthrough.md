# Temporfell, a demo — walkthrough (**tech-demo end, no score**, 159 moves)

- **Game:** `temporfell_demo.taf`, title screen reads "Temporfell" / "Beta
  Release 2" — a sci-fi tech demo, not a finished game. There is no score and
  no traditional win: the game ends on a fixed "Thanks for testing!" screen
  once the demo's one scripted sequence is completed.
- **Result:** demo completed in 159 commands, ending on "Thanks for testing!
  I hope you enjoyed playing. Look for the the final version soon with more
  puzzles, more story and less bugs! [Press any key to end]". Wired as
  `temporfell_solution.txt|temporfell_demo.taf|Thanks for testing|SCR_RNG=xoshiro`.
  `SCR_RNG=xoshiro` is needed for determinism (an NPC-walk/dust-storm RNG
  stream elsewhere in the game).
- **Source:** self-derived by exploring the game live under the headless
  harness (`SCR_DUMP_TASKS=1`/`SCR_DUMP_OBJLOC=1` structural dumps plus
  iterative play), no third-party walkthrough exists for this pre-release demo.

## The route

Wake up on the beach, get rescued, suit up, and make your way to Node (a
friendly robot) via the West Machine Room, doing the panel-fix side quest
along the way (optional, left in as the validated route — it does not gate
anything, it was simply the path taken to explore the game). Node then hands
out a "Remote Command Device," which is the key to the whole demo:

1. Detour from the West Machine Room to the Cluttered Angled Office (via the
   purple-plate corridor Node opens) to fetch the device from a cabinet.
   `open cabinet` then `look in cabinet` is required before `take device`
   will parse the cabinet's contents by name — the object is already
   correctly parented to the cabinet programmatically, but the parser will
   not resolve a held container's contents as nouns until an explicit
   `look in` has been issued, even though the cabinet was already opened and
   its contents already described in the open-cabinet task's own text.
2. Return to the West Machine Room and take the escalator/archive/duct/
   junction route to Bleach White Reception, which a locked door seals off
   from White Lab East (the demo's end room).
3. `activate remote` (while holding the device) to take over the NPC Node,
   who refuses to enter the reception room and is left standing in Bleach
   White Hall next door.
4. Walk Node `west` to rejoin the player's own body in the reception room,
   then `south` — this fires the "You fly Node up and through the open glass
   pane" task, which bypasses the locked door entirely and moves the
   *player* (not just Node) into White Lab East.
5. A final `west` in White Lab East fires the demo's unconditional
   end-of-content task and prints the "Thanks for testing!" screen.

The wire puzzle (a separate side room reached earlier) and the room-41
broken-panel quest are both optional side content — neither gates any exit or
task on the route above.

## Content check

Sci-fi vacation-planet setting; all named characters (Price, the captain,
Node the robot) read as adults; nothing romantic, sexual, or violent beyond a
scripted near-drowning rescue at the start. No minors, nothing to flag.
