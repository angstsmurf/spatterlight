# Newbie — walkthrough (**WIN, no score, 3 moves**)

- **Engine:** ADRIFT 3.90. Two rooms (a Studie and a Cubbord), one NPC (Joe
  Bloggs), one interactive object (a button; a "large gun" is present but
  explicitly "cannot be moved or used because it is rubbish" -- a joke prop).
- **Result:** **WON** in three commands. No score system exists (0 of 0,
  "Well done - you scored maximum points!"). Wired as
  `newbie_solution.txt|newbie.taf|You go throo the wall|`, no env.
- **Source:** self-derived via `SCR_DEBUGGER_ENABLED=1`'s `tasks *` dump and
  `SCR_TRACE_TASKS=1`. No external walkthrough exists for this micro-game.

## The game

```
press button
west
west
```

Pressing the button in the Studie opens a secret exit; walking west moves the
player into the Cubbord, and walking west a second time triggers the game's
ending task. The one wrinkle: the Cubbord's own exit-refusal message ("You
can only move east.") intercepts the movement abbreviation `w` before the
ending task ever gets a chance to match, because the task's command pattern
is the literal word `west`, not the verb's shorthand. Typing `w` twice
therefore just repeats the refusal forever; typing `west` on the second
attempt reaches the task instead. Confirmed via `SCR_TRACE_TASKS=1`: task 2
(`cmd=[west]`, gated on task 1 `press * button` being complete) runs only on
the literal `west` input.

The whole game is a short, self-aware parody of poorly-written amateur
ADRIFT games -- the ending text lampshades bad prose, a pointless "sequel"
announcement, and a deliberately absurd/badly-described death scene for the
player character, played entirely for comedy. No sexual content; the sole
NPC (Joe Bloggs) is an unspecified adult. Clean per the no-minors content
policy.
