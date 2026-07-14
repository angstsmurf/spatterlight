# HYPER Battle System (preview) — walkthrough

- **Game:** `hyper_b_s.taf` — *"HYPER Battle System" Version 1.1*, Copyright 2002
  Seciden Mencarde.
- **Result:** **WON, full 100/100, deterministic.**
- **Solution:** `harness/hyper_b_s_solution.txt`.

## What it is

Not a story game — a **shareware tech-demo of a home-made ADRIFT combat
engine.** The intro says so outright: *"This is a little preview of my Battle
System… intense limits… this mainly just tests to see that the basics work
right."* The whole game is one fight: punch a Flare Rat to death.

It is a *custom* menu battle system the author built by hand on top of ADRIFT —
room descriptions act as menus, and single-letter "tasks" (`a`, `p`, `d`, `f`,
`m`, `1`, `2`) are the menu selections. It does **not** use the ADRIFT Battle
System (`scbattle.c`); SCARE drives it purely through ordinary tasks/variables,
so no `SC_ASSUME_COMBAT` is needed and the result is faithful.

## Structure (from the `SC_DUMP_TASKS` dump)

- 5 rooms: Lobby (0), Basement Tutorial Lounge (1), Flare Rat room (2),
  Battle Menu (3), Attack Menu (4). Rooms 3/4 are the battle "screens".
- 2 NPCs: the Clerk (flavor) and the **Flare Rat** (starts hidden, 30 HP).
- 11 tasks. The only **ChangeScore** (type-4) and the only **EndGame** (type-6,
  var1=0 = win) actions live in **task 8 "KILLRAT"** — `+100` then win,
  gated on a variable restriction that passes when the rat's HP reaches 0.
  ⇒ **max score = 100**, reached by the single kill.
- 1 event ("DEADRAT") wired to fire the KILLRAT task; the win triggers
  deterministically on the turn the rat's HP hits 0.

## The fight

From the Lobby, go **down** to the Basement Tutorial Lounge. It offers a numbered
menu: `1` = read the tutorial (optional), `2` = **First Battle (vs. Flare Rat)**.

Pick `2` to enter the Flare Rat room, then start combat with **`battle rat`**
(the task accepts `battle`/`fight`/`attack` × `Flare Rat`/`rat`/`rodent`).

This opens the **Battle Menu**. The menu loop is three keystrokes per punch:

1. **`a`** — Attack → opens the Attack sub-menu.
2. **`p`** — Punch → deals **3 damage** (the rat retaliates for 3; you start at
   100 HP, it at 30, so you win the race with margin to spare). Prints a `][`
   "press a key" screen.
3. **(blank line)** — dismiss the `][` screen, back to the Battle Menu.

Repeat **10 times** (30 HP ÷ 3 per punch). On the 10th punch the rat drops to
0 HP: *"(Your score has increased by 100) … The Flare Rat is dead! Mission
complete! Congratulations!"* — game won, **100/100**.

> Menu gotchas: `p`/`w` are only valid on the Attack sub-menu; `a`/`d`/`f`/`m`
> only on the Battle Menu. Skipping the blank-line keypress de-syncs the menu
> (a `p` typed at the Battle Menu is rejected), which is why a naïve
> `a p a p …` only lands a punch every other pair. Defend is auto-`[D]`,
> Magic-Spell is "under construction", and Meditate/Flee are unnecessary here.

## Notes

- Faithful, no engine obstacle and no combat-assist needed: the player and rat
  carry real non-zero stats (3-damage punches both ways), unlike the all-zero
  Battle-System games (Azra / V&K / To-Hell / Mr-Smith).
- Verified deterministic: the banked solution reproduces 100/100 + the
  "Congratulations!" win marker identically on 3 consecutive runs.
- **Cosmetic quirk + Glk auto-assist:** the game's `attack` task carries an
  unset (`Var2=-1`) NPC move with a dangling `Var3`, meant to bring the Flare
  Rat into the Attack Menu; faithfully ignored, the rat is never visibly
  present during its own battle (its "glares menacingly" line shows only in
  the Battle Menu).  The fight itself is variable-driven, so the win is
  unaffected either way.  The Glk port turns move assist on automatically for
  this game (os_glk.cpp `GSC_GAME_ASSIST_TABLE`; note the table key is the
  tag-stripped animated title, "HYPER Battle System Version 1.1Copyright 2002
  Seciden Mencarde"), which summons the rat to the Attack Menu.  The corpus
  golden runs assist-off and is unchanged; see
  `To_Hell_And_Beyond_walkthrough.md` for the full unset-move analysis.
