# Short-lived — walkthrough (joke game; instant death by design)

- **Engine:** ADRIFT 4.00, `shortlived.taf` (195 bytes), Duncan Bowsman,
  27 Jul 2008. The whole game is one room with a blank description and one
  task, TASK 0 `*` (`where=1 room=0`), whose only action is `ACT type=6
  v1=2`: "I'm afraid you are dead!". Any command you type ends the game.
  MaxScore 0.
- **Result:** unwinnable by design. The solution answers the 4.00 name and
  gender prompts (`Player`, `male`) and types `look`. The game prints the
  death summary ("You scored 0 out of the maximum 0! That is 100% of the
  game!").
- **Row:** `shortlived_solution.txt|shortlived.taf||`

A blank line as the first command also ends the game, but it prints the
game's "I don't understand what you mean!" before the death text. `look`
gives the cleaner transcript.
