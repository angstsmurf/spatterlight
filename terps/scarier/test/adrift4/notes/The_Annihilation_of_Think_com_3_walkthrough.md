# The Annihilation of Think.com 3 — walkthrough (best reachable, 0/1, **no win possible**)

- **Engine:** ADRIFT 3.90, `TAOT3.taf`. A sequel to *The Annihilation of
  think.com* (see `Theannihilationofthink2_walkthrough.md`). Adam and David
  go through a portal after Emma is attacked, and they chase Herald's
  "counterpart" Nexus through a ventilation shaft, a security complex, the
  Battlegrounds and a cemetery. Every fight is a numbered or lettered choice
  menu (1/2, A/B). Choices are raw task commands, so there is no Battle
  System.
- **Result:** best reachable. Score **0 of 1**, 34 command lines (9 of them
  blank lines that eat `<waitkey>`s). No env.
- **Row:** `taot3_solution.txt|TAOT3.taf||` (no marker)

## Why it cannot be won

The single point and the only winning EndGame are both in TASK 22 (`2`,
"NOT BILL!!!!!", then the long epilogue and `ACT type=6 v1=0` plus
`ACT type=4 +1`). TASK 22 is `where=1 room=18` ("Small cache"). No exit leads
into room 18 (from the EXIT map, room 17 FINAL RESTING PLACE has only S), and
no task moves the player there: the only `ACT type=1 v1=0` destinations are
rooms 1, 2, 2, 3 and 15. TASK 20 (`B` in room 17) says Nexus "turns and runs
to your left", but it has no actions. `w`, `left` and `follow nexus` do
nothing. Like its prequel, this is an unfinished build. The run stops right
after TASK 20. In room 17 the only other choices are `2` or `A`, and both
kill you (TASK 17 or 19).

## Route

`examine girl` (TASK 0 text; plain `x girl` doesn't match `*examine girl*`),
`x emma` (2 waitkeys) takes you into the Corridor. `2` (ask the creature
politely; `1` then `shoot creature` is death) takes you to the Ritual room.
`put ring on altar` (the worn gold ring; 1 waitkey; `put amulet on altar` is
death) takes you into the ventilation shaft. `n` ×4 to Security lock 3,
`w`, `search body` (1 waitkey) for the bypass key, `e`, `swipe card`, `n` ×4
to the Battlegrounds. `examine nexus`, `1`, `A` (4 waitkeys; `B` and `2` are
`where=0` death tasks, so in fact they can't be triggered). `look` fires
TASK 15, which opens the E exit. `e` to the Cemetary: its room text has a
`<waitkey>` that eats the next line, so a blank line follows. Then `d`, `n`
into the FINAL RESTING PLACE, and `1`, `B`.

## Oddities

- The waitkey after the Cemetary room text is inside the *room description*.
  Without a blank line, the next command (`d`) is swallowed.
- TASK 6's text has a misspelt `<waikey>`, which prints as a literal tag.
  The route never triggers it.
