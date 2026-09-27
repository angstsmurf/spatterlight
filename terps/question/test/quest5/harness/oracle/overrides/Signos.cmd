# Signos -- M4u, 2012 (Quest 5.2, ASL 520) -- WINNING SCRIPT
#
# RESULT: state=Finished, errors=0, 69 steps, MAX SCORE 108/108
# ("CONGRATULATIONS!!! You have completed this adventure and became one with
# the universe!"). The only `finish` in the game is in buda's `give` handler,
# gated on essence.volume = 100.
#
# SOURCE: derived from game.aslx. The game DOES ship its own solution (a
# `WALKTHRU` command, game.aslx:1767) but it is wrong in three places and caps
# itself at "100 points" -- see DEVIATIONS below. This script is not that one.
#
# THE DESIGN. You carry a bottle of essence that starts 100% full; the win needs
# it back at EXACTLY 100. Six "trials" each cost essence, knock you out into the
# Dark place, and reveal one page of your blank book:
#   page 1  eat apple          (Meadow, fakir)          -16
#   page 2  remove mask        (glass room, needs mirror examined)  -17
#   page 3  yell at buda       (garden)                 -16
#   page 4  drink water twice  (by the lake)            -16
#   page 5  sit on bed         (chappel)                -16
#   page 6  wear crown         (by the lake, yogi)      -16
# Burning each page in the cavern gives the essence back (+16/+17) -- the six
# refunds sum to exactly the six costs (97), so 100 -> 3 -> 100. Each of the 12
# events is IncreaseScore(9): 12 x 9 = 108 = the stated maximum.
#
# THE TWO COUNTING TRAPS (both silently unwinnable, no error, no warning):
#  * `sit on bed` and `sleep on bed` are DUPLICATE triggers for page 5
#    (game.aslx:1232 and :1242 are byte-identical bodies). Doing both spends
#    -32 for one +16 refund and scores 9 too many. Do exactly one.
#  * There are 7 loss sites for 6 pages for this reason; every other trial must
#    be done once and only once.
#
# NAVIGATION. Hall is the hub: W=Meadow, E=chappel, S=by the lake, N=glass room.
# The garden only opens after `break mirror with stone`, which creates the
# glass room -> north -> garden exit (game.aslx:949).
#
# THREE FOOTGUNS THIS SCRIPT ENCODES:
#  1. Meadow, chappel and `by the lake` each have a `firstenter` that runs
#     `get input {}` (game.aslx:1117, :1180, :970), which SWALLOWS the first
#     command typed after you arrive. Each of those arrivals is followed here by
#     a throwaway `x <npc>` that exists purely to be eaten. Without it the real
#     next command vanishes and the trial silently fails.
#  2. fakir, yogi, monk and lake all have <hidechildren/>, so their contents are
#     out of scope until the container is examined -- hence `x yogi` before
#     `get crown` and `x lake` before `drink water`.
#  3. The well is a ONE-WAY TRAP. `garden -> in -> well -> down -> bottom` has
#     no return: the well's only exit is `down` (game.aslx:1333). The way back
#     from the cavern is the underwater passage `bottom -> south -> bottom of
#     the lake -> up -> out`. The shipped WALKTHRU's "UP / OUT" strands you.
#
# DEVIATIONS from the shipped WALKTHRU command:
#  * It omits the three swallowed-command slots, so its own trials misfire from
#    the very first room (`sit down` is eaten by the fakir's get input, then
#    `close eyes` answers "Standing up you feel dizzy").
#  * Its return route "UP / OUT" leaves you stuck in the well (see 3 above).
#  * It advertises "Solution (100 points)"; the real maximum is 108 and this
#    script reaches it.

# --- Meadow (west): the apple = page 1 ---
go west
x fakir
sit down
close eyes
lift arm
eat apple
wake up
# --- chappel (east): pray for the cross (fire-steel), then the bed = page 5 ---
go east
x monk
sit down
close eyes
pray
sit on bed
wake up
# --- by the lake (south): meditate the stone loose, then the crown = page 6 ---
go south
x yogi
sit down
close eyes
think
swim
down
get stone
up
out
x yogi
get crown
wear crown
wake up
# --- by the lake again: drink until you drown = page 4 ---
go south
x lake
drink water
drink water
wake up
# --- glass room (north): examining the mirror unsticks the mask = page 2 ---
go north
x mirror
remove mask
wake up
# --- smash through to the garden: yell at the buda = page 3 ---
go north
break mirror with stone
go north
yell at buda
wake up
# --- fetch the nest from the big tree (the fire needs fuel) ---
go north
go north
climb tree
get nest
down
# --- down the well to the cavern and burn all six pages ---
in
down
go north
go north
burn nest
burn page 1
burn page 2
burn page 3
burn page 4
burn page 5
burn page 6
# --- home the underwater way (the well cannot be climbed) ---
go south
go south
go south
up
out
go north
go north
go north
assert:essence.volume = 100
assert:player.score = 108
give essence to buda
