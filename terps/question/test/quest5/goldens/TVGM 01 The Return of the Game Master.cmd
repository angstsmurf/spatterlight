# TVGM 01 The Return of the Game Master: a menu gamebook. Menus are Core's numbered text menus (bare digits; their order is seed-locked).
# Difficulty Neutral, find Merlin's communicator (pockets, bed, pockets), then Tyson's Friday. Part 2 is a game-side dead end:
# "Ask Lex" calls the nonexistent SetFlagOn, and "Ignore" reaches `list remove (color_list, pokadot_color)` with pokadot_color never
# assigned (the loop assigns rand_color), so the script aborts before the game-choice menu and part2_gaming is never entered.
# `look` re-runs the part-2 description; each path is taken once. Nothing past that is reachable.
2
1
2
2
1
2
look
1
1
look
