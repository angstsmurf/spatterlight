# How Did I Get Into This? — walkthrough (**no win exists; story ending**)

- **Game:** `hdigit1.taf`, ADRIFT 4.00 (2008). A three-scene vignette: a
  checkout at the grocery store, a drunken night out, and a missile silo where
  "Red" turns the key and you push the button.
- **Result:** the story's own ending, TASK 15: *"Without hesitation, I push the
  button ... We know we are already dead."* 12 turns.
- **No win.** MaxScore 0, no `ACT type=4`. All four `ACT type=6` endings are
  `v1=1` ("Better luck next time."):
  - TASK 14 — push the button before "Now!" (flubbed launch);
  - TASK 15 — push on "Now!" (taken here: the authored conclusion);
  - TASK 18 — `push red` a third time (traitor's trial);
  - TASK 19 — never push (Red shoots you).
  So the row has an empty win marker.
- **Env:** none.

## Route

- Blank line: answers the title `<waitkey>` after *"How did I get myself into
  this?"*. Without it the pause eats `take box` (confirmed with
  `SCR_MARK_WAITKEY=1`: `[WAITKEY ate "take box"]`). The route still works then,
  because `put box on scanner` takes the box implicitly, but the script should
  not lean on a swallowed command.
- `take box` / `put box on scanner`, `take cabbage` / `put cabbage on scanner`,
  `take wine` / `put wine on scanner`. These are the three scanner tasks (3–5);
  the wine one cuts to *At Night*.
- `drink wine`, `yes` (you will drive) — cut to *Missile Control*.
- `z z z` — "Ready...", "Set...", "Now...!", then `push button`.
