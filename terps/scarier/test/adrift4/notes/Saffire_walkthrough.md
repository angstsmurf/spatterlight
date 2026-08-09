# Saffire — walkthrough

- **Engine:** ADRIFT 4 (`saffire.taf`, Woodfish, **3rd ADRIFT One-Hour Game
  Competition**, 2003). You are Harry; a gunman is walking towards you, and
  there is nothing to be done about that.
- **Result:** **WON** — the Heaven ending, the intended good one.
- **Solution:** `goldens/saffire_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `you reach heaven`.

## Structural verdict

40 tasks, 10 rooms, no NPCs. **No scoring system.** Four tasks end the game and
all four are `EndGame(win)` — task 36 Heaven, 37 Hell, 38 Reincarnation, 39
Zombification. They differ only in the closing text, so "maximum" here means
the intended good one.

The prologue is on rails. `EVENT 0 [man shoots]` starts at turn 2 and fires
after five or six more, running task 9 (`#Shot`): *-BANG-*, and the corpse-eye
view teleports you to room 1 *[Darkness]*. Nothing you type in room 0 matters —
tasks 0/5/6/7/8 are all polite refusals (hit him, grab the pistol, throw a bin,
shout, talk) and there is no exit.

## Route

```
z
z
z
z
z
        ← blank   (death scene ".. press any key ..")
turn on torch
n
e
take glass
drink water
u
u
s
rub the sapphire
x sapphire
press 1
```

## Notes

- The five `z`s just let the clock run; the blank after them rides out the death
  scene's `.. press any key ..`.
- `turn on torch` (task 13) is defined **only in room 1** — trying it while still
  alive answers *"You can't turn that."*
- Any direction out of room 2 walks you to room 3 *[Wall]* (task 1); `e` or `w`
  from the Wall (task 20) reaches room 5 *[Beside Shelf]*, where the glass of
  water stands.
- `take glass` (task 23) turns the glass of water into the empty glass, and
  drinking is the **sole** restriction on task 26, the staircase — you must have
  drunk before you may climb.
- `rub the sapphire` (task 34) is what makes the sapphire show its menu; tasks
  35–39 all depend on it. `press 1` is Heaven.

## Cosmetic bug worth recording

The Heaven ending closes with `<font face="Wingdings" size=140>V</font>`.
Wingdings is a symbol font, so that byte is a pictogram, not a letter — `0x56`
maps to the glyph named `crossshadow` in the shipped `wingding.ttf`, i.e. a
shadowed Latin cross (U+271E). Scarier's `os_glk.cpp` only carries a **Webdings**
translation table, so it prints a bare `V` instead. Same shape of fix as the
Topaz dove (`scarier-webdings-symbol-font`).
