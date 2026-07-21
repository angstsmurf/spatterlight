# Moquette -- Alex Warren, IFComp 2013 (Quest 5.5.5010.16575, ASL 550) -- WINNING SCRIPT
#
# RESULT: state=Running at the ending screen, errors=0, 145 steps. There is no
# `finish` anywhere in game.aslx (the spondre / MOUNTAIN SKI pattern): the last
# `1` on Epilogue Page 300 calls JS.showEndingScreen(), which fades the game out
# and shows end.jpg. Reaching that IS the end of the game.
#
# SOURCE: no published walkthrough exists -- derived entirely from game.aslx.
#
# WHAT THE GAME IS: a whole simulated London Underground. You are Zoran Tharp,
# hung over, skipping work, riding the tube. There are no puzzles and no score;
# the game is a four-act clock. Every turn spent ON A TRAIN increments
# `train.totalturns`, and `game.triggers` (a dictionary keyed by that count)
# fires the plot:
#   T20  Act 1 vision 1 (the smoke)          -- answer `1`
#   T30  Act 1 vision 2                      -- answer `1`, `2`, `3`
#   T37  sets game.triggeract2 (Heather)
#   T55  Act 3 vision (the clay)             -- answer `1` x5
#   T70  sets game.triggeract4 (Private Rod)
# Turns only count while `player.parent = train`, so time spent on platforms,
# in visions or in Act 2 is free. Reaching T70 takes ~24 station-stops.
#
# THE FOUR PLACES THE SCRIPT MUST TYPE SOMETHING EXACT:
#  1. Act 0 is a linear rail of {command:1:...} links, EXCEPT at London Bridge,
#     where `1` changes to the Jubilee line (game.escapepoint=4) and `2` refuses
#     to and jumps the closing doors (escapepoint=5). This script takes `2`:
#     escapepoint=5 is the only value that unlocks Private Rod's better speech
#     in Act 4 Page 610 ("It was your decision that you made at London Bridge
#     this morning... and you've got the bruise to prove it").
#  2. Riding: `wait2` passes a turn; `interact personNN` looks at a passenger,
#     and the reply usually contains a second link word (`muttering`, `roberta`,
#     `bone`, ...) which is a real command and adds that person to
#     game.interactedpeoplehistory -- the list Private Rod replays on his CCTV
#     monitors in Act 4 Page 500. Passengers are drawn at RANDOM (ShuffleTrain /
#     GetRandomPoisson), so every `interact personNN` below is bound to the
#     erkyrath_random(1234) stream: change one turn and the roster shifts.
#  3. Getting off: `unboard` only works on the ONE turn the train is standing at
#     a station (train.counter = 0) -- i.e. immediately after "This is <X>. I
#     could get off here." Miss that window at a terminus and the train becomes a
#     ghost that prints "End of the line" forever and can never be left again.
#     Then `change <platform name exactly as listed>`, `next`, `board`.
#  4. Six JS callbacks have no headless counterpart (the browser calls back into
#     the game via ASLEvent). Without these `event:` steps the game simply stops:
#       FinishAct0Clear (after Act 0's first `1` on the train)
#       StartAct1        (after EndAct0)
#       JSFinish_Clear   (after every `unboard`)
#       JSFinish_HeatherText / JSFinish_Blackout   (Act 2)
#       JSFinish_HeatherTube / JSFinish_Act4Clear  (Act 4)
#
# ACT 2 (Heather) is reached by BOARDING, not by riding: once triggeract2 is set
# at T37, the next InitTrain diverts to Act 2 Page 100 instead of the carriage.
# Ride on and the driver takes the train out of service after three stops ("due
# to a fault with the doors, all change") and forces the issue, which is what
# happens here at Elephant & Castle.
#
# ROUTE: Balham -> London Bridge (Act 0, Northern line) / Jubilee to Baker St /
# Bakerloo south to Elephant & Castle (Act 2 en route) / Northern north to Euston
# / Victoria south -- Act 4 arrives just before Victoria station.

# --- Act 0: Balham -> London Bridge. `2` at London Bridge = escapepoint 5.
1
event:FinishAct0Clear;0
1
1
1
1
1
1
1
1
2
1
1
1
event:StartAct1;0

# --- Act 1: Jubilee line, London Bridge -> Baker Street (end of line).
interact person23
eat
interact person31
attention2
interact person11
muttering
interact person3
interact person4
interact person22
interact person20
interact person19
interact person7
wait2
interact person21
roberta
interact person25
interact person27
interact person6

# --- Baker Street: change to the Bakerloo and head south.
unboard
event:JSFinish_Clear;0
change southbound Bakerloo line
next
board

# --- Bakerloo south. T20 vision (the `1` at line 41), T30 vision (`1 2 3`),
#     T37 sets triggeract2 -- three more stops and the train is taken out of
#     service at Elephant & Castle.
interact person30
baby
1
stare
interact person5
bone
interact person18
interact person13
interact person14
wait2
wait2
interact person26
1
2
3
wait2
wait2
interact person2
attention
wait2
interact person12
hanging
cock
interact person24
you
interact person16
interact person17
wait2

# --- Elephant & Castle: forced off. Boarding again is what starts Act 2.
unboard
event:JSFinish_Clear;0
change northbound Northern line
next
board

# --- Act 2: Heather, the conversation, the blackout, waking up at Borough.
1
event:JSFinish_HeatherText;0
1
1
1
1
1
1
1
1
1
event:JSFinish_Blackout;0
1
1
1
1

# --- Act 3: Northern line north, Borough -> Euston. T55 is the clay vision.
interact person301
jeans
interact person8
tv
interact person9
notes
wait2
interact person10
conversation
interact person29
group
interact person28
paper
interact person300
interact person303
wait2
wait2
wait2
1
1
1
1
1
wait2
interact person1
satisfaction

# --- Euston is the end of this branch: change to the Victoria line south.
unboard
event:JSFinish_Clear;0
change southbound Victoria line
next
board

# --- Ride to T70; Act 4 then diverts the arrival at Victoria.
interact person32
interact person15
interact person302
wait2
wait2
wait2
wait2
wait2
wait2
wait2
wait2
wait2

# --- Act 4: PRIVATE ROD. Then the epilogue at Balham, and the ending screen.
event:JSFinish_HeatherTube;0
1
1
monitors
man
1
1
1
1
1
1
event:JSFinish_Act4Clear;0
1
1
1
