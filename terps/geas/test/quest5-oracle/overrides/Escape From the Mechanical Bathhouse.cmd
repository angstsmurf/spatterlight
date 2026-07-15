# Escape From the Mechanical Bathhouse - winning script (state=Finished)
#
# TIMER MECHANISM: (b) REAL WALL-CLOCK, but HEADLESS = infinite time.
#   The 50-second "matching puzzle" limit is a Quest 5 <timer name="test"> with
#   interval=1 (game.aslx:9674-9695) enabled by "press button" (game.aslx:2970,
#   EnableTimer(test)). Quest timers fire off game.timeelapsed, which QuestViva
#   only advances via WorldModel.SendCommand(cmd, elapsedTime, ...) when
#   elapsedTime>0 (WorldModel.cs:331-333) or via Tick(elapsedTime>0). The headless
#   harness (Program.cs:128) calls SendCommand(cmd) -> elapsedTime=0, and never
#   subscribes to RequestNextTimerTick / never calls Tick. So game.timeelapsed
#   stays 0, TimeElapsed(0) >= trigger(1) is never true, the test timer NEVER
#   ticks, Access Panel.timer stays 50 and never resets. => unlimited turns for
#   the 5 matches. A static script wins.
#
# DEVIATIONS from David Welbourn's walkthrough:
#   - Man wanders RANDOMLY (turnscript "archimedes alive", PickOneUnlockedExit,
#     game.aslx:2585-2623). After gauge=6, wait exactly 7 turns (z x7) so the man
#     is in the Bathroom when we "tell man about bath" (empirically found under
#     RNG seed 1234; 6 or 8 miss him). Once he enters the bath, archimedes is
#     disabled and he stays put, so the rest is deterministic.
#   - Split Welbourn's "> a. b." lines into one command per line; dropped SPACE
#     tokens and pure-prose parentheticals.
#   - "set slider to 1" is a real command here (game.aslx:3173).
#   - Puzzle symbols are RNG-chosen (Panel stuff, PickOneString, game.aslx:9650).
#     Under seed 1234 with this exact prefix the five displayed symbols are:
#       square, square, diamond, arrow, triangle
#     -> insert the OPPOSITE face each time (game.aslx:3009-3085):
#       square->arrow, diamond->triangle, arrow->square, triangle->diamond.
x me
x pyjamas
x man
x serial number
read it
x keyhole
x bathrobe
x pocket
x keyring
x brass key
x card
x clock
x door
x hands
x walls
x nest
turn hands
x cuckoo
take cuckoo
drop cuckoo
z
z
look
x broken chick
search chick
x silver key
insert silver key
turn silver key
e
x attendant
x lockers
e
ask attendant about attire
ask attendant about bathhouse
ask attendant about man
ask attendant about lockers
insert brass key
unlock locker
open locker
take towel
x towel
x doors
take coin
x coin
take keyring
n
x bench
x hooks
remove pyjamas
put pyjamas on hooks
wear towel
s
e
x bath
x taps
x plughole
x gauge
read gauge
x floor
x design
x cube
x well
take cube
put coin in plughole
turn taps on
read gauge
g
g
g
g
turn taps off
read gauge
z
z
z
z
z
z
z
tell man about bath
take cube
take bathrobe
w
n
remove towel
wear bathrobe
s
e
give towel to man
take coin
w
put coin in door
take keyring
e
s
swipe card
s
x walls
stand on bench
remove bathrobe
wear bathrobe
stand on bench
set slider to 1
d
x walls
sw
x recess
press button
insert arrow face
take cube
insert arrow face
take cube
insert triangle face
take cube
insert square face
take cube
insert diamond face
w
