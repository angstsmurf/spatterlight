# ARC II (Daniel Gao, 2019/2020, ASL 580) — WINNING walkthrough
# Ending reached: "The End" via `receive message display` in the Command Room
#   (Conor pulls out Chloe's necklace — the best/good ending branch). finish is
#   called; oracle reports state=Finished errors=0 steps=36. Deterministic,
#   reruns byte-identical.
# Route/gates:
#   PART ONE (Afternoon Nap): speak to Sharon (unlocks both Waiting Room doors;
#   press-any-key waits are auto-continued by the harness), east twice (first
#   attempt is a scripted hesitation), speak to Chloe (adds the NECKLACE, needed
#   for the best ending text), west, west (Sharon is gone so the hallway exit
#   moves straight to the Hallway -> Dr. Schneider walks you to the Bedroom),
#   speak to Schneider (adds the BUTTON), then press button THREE times (first
#   two are scripted hesitations; third ends Part One -> wake in New Bedroom).
#   Walking east out of the Bedroom twice instead gives the alternate
#   "walk away" THE END (also finish) — not taken here.
#   PART TWO (The Warden): entering North Hallway arms SetTurnTimeout(8) — an
#   earthquake 8 turns later unlocks the Greenhouse Entrance (and the Bathroom).
#   The sightseeing turns (Warden's Quarters portraits/mural, Schneider's room)
#   burn those 8 turns; all are optional flavour, any 8 non-error turns work.
#   Then north through the greenhouses; in the North Greenhouse TAKE BADGE
#   (lying beside the corpse; gates every remaining door + Star Room gravity),
#   speak to warden -> numbered Core menu, answer "1" (Yes = forgive him; "2"
#   leads to his suicide and the darker Command Room footage, still winnable).
#   north into the Star Room ends Part Two and auto-moves you to the Archive.
#   PART THREE (ARC): use terminal + "3" (ARC Program lore, optional), north to
#   Museum (plain exit; its author-script is disabled via runscript=false),
#   north to Command Room (badge), receive message display -> The End, finish.
# Menu footguns hit: Core ShowMenu wants the BARE NUMBER — the literal option
#   text ("Yes") silently misses and wedges the pending menu. The win verb is
#   `receive message` + object: "receive message display" ("receive message"
#   alone is not understood).
# Zero script errors; no #!errorlimit needed; no tick: lines (the only timer is
#   the turn-based earthquake timeout, advanced by ordinary commands).
speak to woman
east
east
speak to chloe
west
west
speak to schneider
press button
press button
press button
east
north
west
x wedding portrait
x mural
x bookshelf
east
south
east
x picture frame
west
north
north
north
x tombstone
north
take badge
speak to warden
1
north
use terminal
3
north
x statue
north
receive message display
