#!clock=5
# Dream of a Tumour (Crawdeloch) — Quest 5, ASL 580, QuestViva oracle
#
# ENDING REACHED: the game's only ending — `go to lie down` in the seventh-floor
#   apartment ("THE END / Dream of a Tumour", `finish`). No published
#   walkthrough; derived from game.aslx. The game says it is "meant to be played
#   via links only", so exits are named, not compass directions: every move is
#   `go to <exit alias>` (`go to to town` is the exit aliased "to town").
#
# ROUTE: flashlight from the fridge; in the bedroom `x hole` unlocks the way
#   back and `x night-table` reveals the copper key, whose taking unlocks the
#   coffin-shaped hatch; the "attic" is a cemetery. The meat scythe can only be
#   taken once the growths have been visited, and only after hiding in the
#   shack's iron cabinet until the `waiting` turnscript (40 % a turn) clicks the
#   doors open. Scythe through the growths to the mortuary, `reach out` to the
#   bronze doors, on to the crossroads, where `x signpost` reveals both roads.
#   The optional old-lake detour is taken first: `walk to tumour` is a 20 % roll
#   and lands on the fourth try at seed 1234; the tumour's timers return the
#   player to the crossroads. Then town, home, seventh floor, lie down.
#
# WHY #!clock=5: the mortuary doors (Screenwait, 2 s) and the tumour
#   (tumour_timer 8 s, then tumour_timer2 5 s) move the player on real interval
#   timers, which only advance on the typing clock.
up
open fridge
take flashlight
turn on flashlight
go to an askew doorway
go to an off-center doorway
x hole
x night-table
take copper key
x small note
go to off-center doorway
go to coffin-shaped hatch
look
go to walk
go to walk back
go to shack
hide iron cabinet
x strange drawing
z
z
z
z
go to leave
take scythe
go to back out
go to walk
go to large building
x double-doors
reach out double-doors
go to somewhere else
x signpost
go to to the old lake
x tumour
walk to tumour
walk to tumour
walk to tumour
walk to tumour
look
x incision
look
z
z
z
look
go to to town
x apartments
go to home
go to seventh floor
go to lie down
