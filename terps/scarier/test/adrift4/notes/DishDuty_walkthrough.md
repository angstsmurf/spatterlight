# Dish Duty — walkthrough (**UNWINNABLE, intro build**)

- **Source:** IntroComp 2009 entry (`dishduty_intro(3).taf`, also historically
  distributed as a duplicate draft "dishdutyintro" — same game file). A
  domestic-horror/glitch teaser: the player's spouse asks them to wash the
  dishes, and the mundane scene steadily unravels into wrongness ("Something
  is wrong. You can't concentrate. Nothing makes sense."), with garbled,
  corrupted-looking object and NPC names lurking in the data.
- **Content review:** unsettling psychological-horror atmosphere throughout,
  but no sexual content and no minors. Clean to wire.
- **Result:** **UNWINNABLE** in this intro build, and deliberately so. Per
  `SCR_DUMP_TASKS`, the `wash dishes` success task (task 30, "Okay, you wash
  the dishes.") carries a restriction requiring marker task 28
  ("---A SECRET?---", an untypable placeholder command with no restrictions
  and no actions of its own) to be marked done. Nothing anywhere in the dump
  — no other task's restriction or action — ever references task 28, so it
  can never become done, and only the sibling failure task (29, "You can't
  wash that.") ever fires, forever. This is the same deliberately-stubbed
  IntroComp pattern already documented for `dbaa` (`DBAA_walkthrough.md`):
  a preview build whose core verb is wired to look reachable but was never
  actually finished for the teaser. Wired as
  `dishduty_solution.txt|dishduty_intro(3).taf|You can't wash that.`, no env.

## The walkthrough

Answers "n" to the spouse's opening "Something wrong with the dishes?"
question, triggering a small argument beat. Two `wash dishes` attempts both
fail with "You can't wash that." — the second one also triggers a scripted
event where the spouse goes pale and flees north, locking the door behind
her. The walkthrough confirms both the north and west exits are now locked,
then waits for the spouse to return with a run of increasingly unsettling
"stepford wife" dialogue lines, and tries `wash dishes` one final time to
show the failure message persists even after that reset — the deterministic
dead end of this preview build.
