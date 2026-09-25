# The World According To CBN — walkthrough (**WIN**)

- **Game:** `The_World_According_to_CBN.taf`, ADRIFT 4.00, David Whyld. A
  Clueless Bob Newbie spin-off: you are Shamus, a student in Bob's dreamworld
  where Newbieism is the state religion, contacted by "the Heretic".
- **Result:** genuine win. TASK 111 moves the player into room 8, whose
  `<victory>` text is *"You've won. You're free of Bob's dreamworld ... Give
  yourself a well-earned pat on the back"*. 38 commands.
- **Score:** none. No `ACT type=4`, no `ACT type=6` ("In a CBN game, no one
  keeps score"); the ending is the terminal room reached by
  `ACT type=1 v1=0 v2=0 v3=8`.
- **Env:** `SCR_SKIP_WAITKEY=1`, like the `CBN.taf`/`cbn2.taf` rows, so every
  `[MORE]` pause is skipped and the script needs no blank lines. Without it the
  `2 2 3 3` class answers hit "Even in a Clueless Bob game, that would have made
  no sense."

## Route

1. **Class** — `z` ×5 (the class timer; the Enforcer arrives on the 5th), then
   the four answers `2 2 3 3`. Each moves the knowledge variable; keep it below 2
   (TASK 40) or at most 5 (TASK 39). 6 or more is death.
2. **Home** — `x mark`, `read book`, `x tv` (flavour), `use computer`, `check
   mail` (the Heretic's e-mail), `1` = Go.
3. **Bridge** — `x tramp`, `z` ×3 to wait for the Heretic, `1` to say the words.
   `z z` more and the corridor timer drops you on the river bank.
4. **River bank** — `stroke frog` (flavour), then `talk to heretic` ×6. TASK 92
   takes you to Bob's home.
5. **Bob's home** — `z z help z help`. The first `help` prints the stock "u dunt
   need elp" joke; on the next turn Bob arrives and the duel starts; the second
   `help` (bob timer >= 4) drops the piano on him: TASK 111, victory.

## Win marker

`a well-earned pat on the back` (the victory text, printed only in room 8).
