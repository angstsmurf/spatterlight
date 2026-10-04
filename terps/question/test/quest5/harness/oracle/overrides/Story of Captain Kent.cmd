#!clock=5
# Story of Captain Kent (title "A Captain Name Kent") — Quest 5, ASL 550, QuestViva oracle
#
# ENDING REACHED: the WIN, the game's only `finish` — `speak to maple` holding
#   her letter ("The ship is yours. Congratulations! You finished the game!").
#   No published walkthrough; derived from game.aslx.
#
# CHAIN: Bill -> candle; Bella -> 20 coins; the outhouse (key from Kent's house)
#   holds the chest key and melted tar; Mr. Bide opens the watchtower, where
#   `look out` breaks the telescope for a piece of glass; the chest gives
#   parchment paper; `create telescope` on the workbench; telescope -> Bobby ->
#   bag of coins -> Howard's chicken -> Bella's pumpkin -> Tom's glasses; light
#   the candle to enter the governor's house; glasses -> Mason -> the letter.
#
# WHY #!clock=5: entering the outhouse arms SetTimeout(15) — "You fainted from
#   the smell", back to Kent's house. With timers drained on every step the
#   faint lands before anything can be taken, so the chest key and tar are
#   unreachable. On the typing clock the two takes and `out` fit inside the 15 s.
#
# The custom verbs take an object: `unlock outhouse key` (at the farm),
#   `look out broken telescope`, `create telescope workbench`,
#   `buy something howard`.
take outhouse key
out
e
speak to bill
speak to bella
unlock outhouse key
in
take chest key
take melted tar
out
w
w
speak to bide
up
look out broken telescope
down
e
in
unlock chest
take parchment paper
create telescope workbench
out
e
e
speak to bobby
w
w
w
down
buy something howard
up
e
e
speak to bella
up
w
buy glasses
e
light candle
in
speak to mason
out
down
e
speak to maple
