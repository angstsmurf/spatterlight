# Great Depression Man (Middle Class) -- "Great Depression Day simulator." (Quest 5.8.6794.35054,
# 2019, ASL 580, v1.1) -- WINNING SCRIPT
#
# RESULT: state=Finished, errors=0, 48 steps, ending health 90/100 and $65 of the starting $100.
#
# SOURCE: no walkthrough exists for this game anywhere; derived entirely from game.aslx.
# A school history project ("This is my history project. It is about becoming a snobby teenager
# who has been dropped into the great depression") -- a ten-room 1935 tour with no puzzles and
# no score. The script is a COMPLETE tour: every room entered, every object examined, every
# custom script fired.
#
# HOW THE GAME ENDS. There is no victory text and no explicit win condition -- the terminal
# state is a round trip. Old Fashioned Room's `onexit` prints "When you want to finish the game,
# come back in." and sets the player flag "visitedall"; its `enter` then reads
#     if (GetBoolean(player, "visitedall")) { finish }
# So the flag arms on the FIRST time you step out of the bedroom, and the game `finish`es the
# moment you walk back in. Consequence for scripting: the whole tour must happen between those
# two moves, and the Hallway's `in` exit must never be taken until the very last line -- taking
# it early ends the game on the spot with everything unseen.
#
# THE "WORK FIRST" GATING IS VACUOUS -- and this is what makes the game completable at all.
# Four exits ship `locked` with lockmessages telling you to go and work in the hotel office
# first (Home Street, Storedoor, Hotel Door, Office Door). Nothing in play ever unlocks them:
# the only UnlockExit calls live in the Cubicle Charts object's `_initialise_` script, which
# Quest runs at LOAD time, before the first prompt. Asserted against the oracle at turn 1:
# `Home Street.locked`, `Storedoor.locked` and `Office Door.locked` are all already false.
# Were they honoured, `look at charts` (the intended "work") would not clear them either -- the
# Charts' `look` is plain text -- and the Cubicle, whose only exit is the locked Office Door,
# would be an inescapable dead end. The author's intended gate is dead code; the accident is
# what leaves the game winnable.
#
# THE HEALTH ECONOMY IS THE ONLY REAL HAZARD. `game.onhealthzero` is `finish` -- the intro's
# "try not to go insane, if you do, the game ends". The tour deliberately takes the two
# empathy hits (look at the mob -5, speak to the mob -20) because they are the point of the
# piece, and pays them back with the Jazz Club's firstenter +5 and the mob's `give money`
# +10 (conditional on player.money > 15, comfortably true at $85). Net 100 -> 90. Asserted.
#
# AVOIDED: `attack mob of people` is an instant loss -- its script is `finish` FIRST, then
# "You died because they attacked you back." and DecreaseHealth(100), so the death message is
# printed after the game is already over. It is the only fatal action in the game.
#
# ODDITIES LEFT IN, all the author's:
#  - `eat eggs` / `eat cup of coffee` work without taking them first; both objects are `edible`
#    but not takeable, so the natural `take eggs` answers "You can't take it." (dropped here).
#  - `buy cards` prints nothing at all -- the Cards `buy` script charges $3 for a $2 price and
#    AddToInventory's the deck with no message, unlike Monopoly's "You bought Monopoly".
#  - The Store's firstenter and the Calander both `ClearScreen` and then tell you to "Look at
#    the right side for the things that you can do" -- pane advice that is inert in a
#    transcript, and the reason those two turns look truncated.
#  - The Creaky Shelf's `onopen` reveals Grapes of Wrath, but the shelf ships `isopen`, so the
#    book is visible from the start and the reveal never runs; its `look` script is empty.
#  - "the a Street" (Street sets descprefix "You are in the" while Core still adds the article)
#    and "Calander" are spelled as shipped.
#
# ROUTE: dress in the bedroom -> Hallway -> Living Area (breakfast, Grapes of Wrath, chair) ->
# Street (the mob, rock soup, charity) -> Town Square (the beggar) -> Jazz Club -> Hotel Lobby
# -> Office -> Cubicle (the "work") -> back out to the Store (calendar reveals the year 1935)
# -> home through Street and Living Area -> up to the Hallway -> in.
look at bed
sleep on bed
open closet
look at clothes
take clothes
wear clothes
out
down
look at old table
eat eggs
look at grapes of wrath
read grapes of wrath
look at old wooden chair
sit on old wooden chair
out
look at mob of people
look at rock soup
speak to mob of people
give money mob of people
north
look at beggar
give money beggar
north
look at booth
sit at booth
south
east
up
look at employee
speak to employee
in
look at charts
look at cup of coffee
eat cup of coffee
out
down
west
west
look at calander
look at storekeeper
look at cards
buy cards
buy monopoly
east
south
in
up
in
