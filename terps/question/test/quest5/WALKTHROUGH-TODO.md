# Quest 5+ games without a full walkthrough script

Status as of 2026-10-17: 374 game files in `games/`, 364 `corpus.tsv` rows, every
row with a golden. The list counts a game as done when its script reaches a win,
or the game's authored ending, or the furthest state a game with no ending or a
broken ending can reach. Everything else is listed here, numbered straight through.
The per-game reasons come from `harness/oracle/overrides/README.md`.

## A. No script at all (game file present, no corpus row)

Each one loads and answers `look` in `aslx_replay`. Nothing more has been checked.

1. Beyond The Gloom (ASL 600, Quest Viva 6.0.1 save)
2. Chicken Farm
3. Indy Wrestler
4. Mystery Musical
5. RollingDice
6. Star Trek Wrath of the Romulans
7. TVGM 01 The Return of the Game Master

Alternate builds of games that are already wired:

8. Thyme (ASL 600 build). The ASL 580 `Thyme.quest` is wired; this one isn't.
9. Signos.aslx, the browser-download release. The corpus row runs `Signos.quest`.
10. What Once Was.aslx, the browser-download release. The corpus row runs `What Once Was.quest`.

## B. Partial script, and a win exists or hasn't been ruled out

11. A Galaxy Consumed: bounded tour of character creation, quarters, terminal and the first story message. `inventory` errors in the oracle.
12. A Tiny Furry In A Huge World: opening tour only (v0.9, unfinished).
13. The Quest for the Golden Egg: the win is known (destroy the egg, then drop the king's note in the Amphitheater) but not derived. The script is an overworld tour.
14. Rise of Death: bounded opening (tutorial, first fight, two cave steps). The game has several `finish` calls that haven't been reached.
15. The Story of MLJesus: opening only. The long RPG after it hasn't been derived.
16. Pokemon Type Harley Gold And Silver: opening tour only.
17. What Dragons Need: creation, the town and the Library. Nothing past the town gate has been played.
18. Xanadu 3 - The Death of Xanadu: village tour only. No victory exists, but the lake, cliff, crypt and rat dungeon haven't been visited.
19. Detective: the deduction tour stops because at seed 1234 the murder weapon, jar_caviar, can't be taken. Another seed might allow REUNE/ACUSA.
20. Worship the Pig: only the short death route is recorded. The eyes/constellation, Proscenium, Clock tower and AntiBazaar branches haven't been walked.
21. Midnight: upstairs tour only. The downstairs and the timer-driven ending haven't been played.
22. Enter The Double Dare: opening screen only. The doors open on a real-time timer.
23. Fishvale: tour of the opening area. It's a gold and fishing-level grind, and the north and east exits are locked.
24. Giantess-Shrunk Dimensions (W.I.P): bounded tour. Scenes end in death.
25. Giantess Shrinking: bounded tour. Endings haven't been derived.
26. I want to Isekai: opening tour only (a large RPG with vore-themed content).

## C. Partial on purpose (content not played)

27. Vore Inc: ground floor only.
28. So fun to be a Scientist: apartment only.
29. Fantastic Dungeon Extender for American Audiences (TM): room tour only.
30. Window: opening only (child protagonist, abuse themes).
31. Shrunk at High-School: opening screen and one `look`.
32. The Great Something _br_Act I: title and warning screen only.
33. The Lodge: introduction and one `look`.
34. Ursake The Text Adventure: home-only tour. The school half and a win that hangs on `RandomChance(25)` were skipped.

## D. No ending in the game; the script is a bounded tour rather than an exhaustive one

These can't be "won". The open question is how much more of each to cover.

35. Blech- Kogasux edition: every `finish` is a death.
36. Sburb [A Sburb (Homestuck) Game]: stops on reaching the Medium.
37. Fighting Sim: a game-side script error after the first raft tiles.
38. Fatalonia: up to the Secret Temple.
39. House in the Forest v1.4: upstairs only (the living room raises an `Ask()` error).
40. Fishvale Remake: 25-room tour.
41. Summoner: 30-room tour.
42. The Fat House: hub rooms only.
43. Furry diaper adventure: home rooms only.
44. Sword Warrior Adventure: village and surroundings.
45. Great Stories: home and the Watchtower.
46. The 12 x 12 Deadly Rooms: clues and the Blue Key.
47. Shop Shaper: sandbox actions.
48. Deeper Test: creation and the first chamber of the random dungeon.

## Considered complete, so not listed above

These games are unwinnable or have no ending, and their scripts already go as far
as the game allows: WAKE, The Last Hero, GiantKiller Too 2.14, System Restore,
Pixie's Quest, Deeper, Labyrinth, Spectrum, Cloak of Darkness (2013), Quills,
Of Sisters and Thieves, Project Sparkle, minecraft adventure, disneys time machine,
There is no name for such awesomeness as this, Improvised Star Trek, Why Now The
Brown House_. Weekend At Ruby's is counted as complete because it wins, even though
it scores only 16/18.
