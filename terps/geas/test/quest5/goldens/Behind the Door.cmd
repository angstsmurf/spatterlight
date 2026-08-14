# Behind the Door (eejitlikeme, 2017, ASL 550) — shipped as behind_the_door_final.quest.
# No published walkthrough exists anywhere; this 48-step script was derived from
# the game source. Best ending: win the chess game, REFUSE the keeper's offer to
# take his place, and walk out carrying both bonuses (a butterfly in the jar and
# the postcards) so the "away" enter-script prints its two optional lines before
# `finish`. (Accepting the offer — menu option 1 — is the other real ending: you
# become the new keeper and the game finishes on the spot, with no walk-out text.)
#
# Route notes, all verified against the source:
#  - `x door` (or `knock door`) is what UnlockExit()s exit_inside; without it the
#    lockmessage is "Maybe you should do some polite things before breaking in?".
#  - The two non-compass exits must be spelled `go inside` / `go leave`: this game
#    inlines its own `go` command whose bare-word alternative matches ONLY the
#    compass list, so a bare alias is "I don't understand your command."
#  - room1 starts dark: `touch wall` reveals the switch, `switch on switch` lights
#    the room and unlocks the north exit.
#  - `x table` / `x fountain` are load-bearing, not flavour: the lookat command
#    clears `hidechildren`, which is what brings the marble ball and the jar into
#    scope. `x box` likewise, before `brush`.
#  - The lions must be distracted TWICE. `use ball` in the hall opens the north
#    exit but parks the ball with the lions; releasing the caught butterfly there
#    (`open jar`) swaps their attention and drops the ball back on the hall floor,
#    which is the only way to get it back for the marble-inlaid box in room2.
#  - The chess arc: speak → `ask1` (the command the keeper's own link fires) sets
#    talk_1; give the chessboard and the queen; speak again to play and LOSE (menus
#    are the game's own numbered text menus, answered with bare numbers); the loss
#    sets keeper.talk_2, which is what turns the `books` command into the 3-title
#    menu whose third entry hands over the chess notes. Speak once more with the
#    notes in hand to win.
#  - Final detour east re-catches a butterfly (the first one was spent on the
#    lions) so the walk-out prints "You let the butterfly free from the jar."
#
# Fully deterministic: no RNG, score or timers anywhere on this path.
# state=Finished, errors=0, 48 steps.
x door
go inside
touch wall
switch on switch
x table
take ball
open cupboard
take chessboard
north
use ball
north
x fountain
take jar
east
catch butterflies
west
south
open jar
take ball
north
west
x box
brush
use ball
take queen
take postcards
east
speak to man
ask1
give chessboard
give queen
speak to man
1
2
south
south
books
3
north
north
speak to man
2
east
catch butterflies
west
south
south
go leave
