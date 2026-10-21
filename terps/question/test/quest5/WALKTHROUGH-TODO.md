# Quest 5+ games without a full walkthrough script

Status as of 2026-10-21: 377 game files in `games/`, 374 `corpus.tsv` rows, every
row with a golden. The list counts a game as done when its script reaches a win,
or the game's authored ending, or the furthest state a game with no ending or a
broken ending can reach. Everything else is listed here, numbered straight through.
The per-game reasons come from `harness/oracle/overrides/README.md`.

## A. No script at all (game file present, no corpus row)

None left (Mystery Musical, the last one, was wired on 2026-10-21).

Alternate builds (Thyme ASL 600, Signos.aslx, What Once Was.aslx) are wired as `(ASL 600 build)` / `(aslx build)` rows; the aslx ones use local symlinks in `games/`.

## B. Partial script, and a win exists or hasn't been ruled out

4. A Galaxy Consumed: bounded tour of character creation, quarters, terminal and the first story message. `inventory` errors in the oracle.
5. A Tiny Furry In A Huge World: opening tour only (v0.9, unfinished).
8. The Story of MLJesus: opening only. The long RPG after it hasn't been derived.
9. Pokemon Type Harley Gold And Silver: opening tour only.
11. Xanadu 3 - The Death of Xanadu: village tour only. No victory exists, but the lake, cliff, crypt and rat dungeon haven't been visited.
13. Worship the Pig: only the short death route is recorded. The eyes/constellation, Proscenium, Clock tower and AntiBazaar branches haven't been walked.
15. Enter The Double Dare: opening screen only. The doors open on a real-time timer.
16. Fishvale: tour of the opening area. It's a gold and fishing-level grind, and the north and east exits are locked.
17. Giantess-Shrunk Dimensions (W.I.P): bounded tour. Scenes end in death.
18. Giantess Shrinking: bounded tour. Endings haven't been derived.
19. I want to Isekai: opening tour only (a large RPG with vore-themed content).

## C. Partial on purpose (content not played)

20. Vore Inc: ground floor only.
21. So fun to be a Scientist: apartment only.
22. Fantastic Dungeon Extender for American Audiences (TM): room tour only.
23. Window: opening only (child protagonist, abuse themes).
24. Shrunk at High-School: opening screen and one `look`.
25. The Great Something _br_Act I: title and warning screen only.
26. The Lodge: introduction and one `look`.
27. Ursake The Text Adventure: home-only tour. The school half and a win that hangs on `RandomChance(25)` were skipped.

## D. No ending in the game; the script is a bounded tour rather than an exhaustive one

These can't be "won". The open question is how much more of each to cover.

28. Blech- Kogasux edition: every `finish` is a death.
29. Sburb [A Sburb (Homestuck) Game]: stops on reaching the Medium.
30. Fighting Sim: a game-side script error after the first raft tiles.
31. Fatalonia: up to the Secret Temple.
32. House in the Forest v1.4: upstairs only (the living room raises an `Ask()` error).
33. Fishvale Remake: 25-room tour.
34. Summoner: 30-room tour.
35. The Fat House: hub rooms only.
36. Furry diaper adventure: home rooms only.
37. Sword Warrior Adventure: village and surroundings.
38. Great Stories: home and the Watchtower.
39. The 12 x 12 Deadly Rooms: clues and the Blue Key.
40. Shop Shaper: sandbox actions.
41. Deeper Test: creation and the first chamber of the random dungeon.

## Considered complete, so not listed above

These games are unwinnable or have no ending, and their scripts already go as far
as the game allows: WAKE, The Last Hero, GiantKiller Too 2.14, System Restore,
Pixie's Quest, Deeper, Labyrinth, Spectrum, Cloak of Darkness (2013), Quills,
Of Sisters and Thieves, Project Sparkle, minecraft adventure, disneys time machine,
There is no name for such awesomeness as this, Improvised Star Trek, Why Now The
Brown House_. Weekend At Ruby's is counted as complete because it wins, even though
it scores only 16/18.
