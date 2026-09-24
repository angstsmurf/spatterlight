# Mages — walkthrough (**no formal win; demonstrates a job-serving bug**)

- **Engine:** ADRIFT, second person. An open-ended magic-school RPG sim:
  health/hygiene/hunger/bladder stats, a magic rating and mana pool, and
  three wizard NPCs to chat with. `SCR_DUMP_TASKS` confirms `WINTEXT []` —
  there is no ending to reach at all, by design, not a bug.
- **Result:** the walkthrough tours the NPC chats and the restaurant job,
  then hits a genuine authoring bug in the job itself, and ends by reading
  the game's own stats/money/magic displays. Wired as
  `mages_solution.txt|mages.taf|a magic rating of 20, and your mana=50.`,
  no env.

## The restaurant-job bug

Taking a job from Bartero and waiting triggers "There is a customer here
that you have to serve." `SCR_DUMP_TASKS` shows why serving never works:
TASK 66 moves the Customer NPC into room 9, Bartero's Restaurant (where the
player already is), but TASK 45 ("serve customer") requires both the player
and the Customer to be in room 22, "Behind the counter" — reached by typing
`in` from the restaurant. Going behind the counter and trying to serve
therefore always answers "There is no customer here." — the customer is
never actually moved to the room the serve task checks.

## The walkthrough

```
lie down
sleep
d
s
w
chat with norimar
e
s
w
chat with esnefed
nw
chat with mystiko
e
e
e
w
s
s
e
w
w
w
chat with bartero
work for bartero
e
wait
in
serve customer
out
stats
money
magic
```
