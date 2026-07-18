# Warriors "(Kit version demo)" (anonymous, 2016, ASL 550) — derived entirely
# from the game source: NO published walkthrough exists (corpus.tsv wt="-").
# A Warrior Cats CYOA-style fan game (310 objects, no player object authored
# beyond the room tree; ~all interaction is menus). Every one of its 13
# `finish` sites prints "END OF DEMO." — the richest arc is: character
# creation -> Chapter One "Night play" (wake sibling, fresh-kill snack, play
# with prey, sneak out the dirtplace hole, explore the forest, back to bed) ->
# Chapter One Complete + status/code screen -> Chapter Two "Play time"
# (mouse keep-away with Stonekit/Lilykit) -> the flying-leap toss ending.
# Notes:
#  - TWO menu systems interleave: Core `ShowMenu`/`Ask` render as NUMBERED
#    text menus (answer with the bare number as a command; menuallowcancel
#    false swallows anything else), while `show menu` script blocks are
#    engine menus (answer with menu:KEY). This game uses both, sometimes
#    back-to-back in one scene.
#  - Under erkyrath seed 1234 the RNG chapter-intro cascade lands on sister
#    Fernkit and the "bats you away" wake-up (answer the get-input prompt
#    with "No" -> regex-matched plead), and "find another way out" reaches
#    Camp entrance deterministically for Thunderclan.
#  - The one-color pelt path derefs unset player.colorO in a wait block
#    (author bug -> script error); the two-color path (Grey+Black) is clean.
#  - Compass moves echo doubled in the transcript (oracle artifact of the
#    game's exit handling; harmless, frozen in the golden).
#  - Win: keep-away on Stonekit's team, "Toss the mouse back to Stonekit."
#    -> his flying catch -> finish ("END OF DEMO."), errors=0.
1
1
Spotted
menu:Confirm
menu:Male
menu:Thunder
menu:Two colors
menu:Grey
menu:Black
No
1
menu:Mouse
2
1
2
north
3
south
in
1
1
out
1
1
1
