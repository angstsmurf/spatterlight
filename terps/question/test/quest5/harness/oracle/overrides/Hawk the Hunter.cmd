# Hawk the Hunter (Jonathan B. Himes, Spring Thing 2020) -- WINNING SCRIPT
#
# RESULT: state=Finished, errors=0 -- genuine ending: the heroic "save Rap" sacrifice.
# Hawk knocks Rap Scallion clear of the Crypt and plunges into the Cauldron himself,
# voiding the portal to Vanya and reuniting with Elian, the White Queen ("There are
# indeed many worlds. You have played a part in saving this one.") -- exactly the
# sacrifice foreshadowed by the Witch's chess vision (pawn sacrificed to save the
# White Queen). Score 60/360 (this game's 360 points are optional side achievements;
# no ending requires them).
#
# SOURCE: the game only ships author hint files ("Hawk the Hunter - hints-walkthrough.txt",
# the FurtherHints.txt from the Spring Thing zip) -- prose hints, no linear script. This
# script was derived from those hints plus the game source (game.aslx), seed 1234.
#
# ENDING CHOICE: the game is open-ended with several mutually-exclusive endings. The
# "best" ending (kill Voltan, 600hp, "destroyed Voltan and his forces") realistically
# needs the full Table of Five companion chain (rescue Gort from Sped via the Devil Stag
# haunch, witch teleports, felling the redwood, the Ancient Workshop river-valve, etc.)
# -- far beyond a linear replay. Chosen instead is the heroic sacrifice ending reached
# via the Voltan-dilemma sequence in the Chapel: Rap and the world are saved at the cost
# of Hawk's life, and Voltan's plan is voided. (A lesser "give Mindsword to the Black
# Wizard" -> dream ending also works but lets Rap die; not used.)
#
# KEY DERIVATION NOTES (all verified against game.aslx):
# * Stats: 6 points; strength to 6 (str>3 is REQUIRED to pull the Mindsword free at the
#   Center of the Lake), rest to stamina (hitpoints = 40+10*stamina).
# * Golden chalice: hidden behind the corner stone in the (dark) Dim Passage of the Keep
#   -- needs the lit Main Hall torch; "x stone wall" then "shift stone" reveals it.
# * The torch must be DROPPED before taking the punting pole in Burnby (both-hands check),
#   and re-fetched from the Main Hall after the lake; outdoors it dwindles to weak, so the
#   kitchen-pantry tinderbox relights it inside Caddonbury Church ("light torch with
#   tinderbox") to bring the basin into scope for "fill chalice with basin".
# * Mindsword: punt the raft east 5x to the Center of the Lake, take mindsword, west 6x
#   back (the pole is lost overboard on arrival -- by design). Returning with the sword
#   aggros the Lake Brethren: Bumbling + Stumbling Acolytes pursue for the rest of the
#   game (movetype Free), so both are killed at the Lake shore ("attack acolyte" alone
#   hits a disambiguation menu -- full names required).
# * Church: "dip finger" (+100 hp), fill the chalice with holy water, and take the
#   wrapped egg + bread from the altar (drop torch/tinderbox first -- 5-item carry limit).
# * FORTRESS ENTRY IS THE CRUX: the River Shale is impassable (the "swift" flag is only
#   cleared by the valve in the Ancient Workshop, across the trolls' gorge). Instead the
#   Weir-maze secret is used THREE times: in the dark path, N,E,W,S (exact counters,
#   deterministic, no RNG events fire before step 5) teleports into the fortress guard
#   room. Trip 1 is the scripted spying vignette: the guard MUST be attacked within 2
#   turns (else alarm = death); he "drops from fright" and Hawk is ejected. Trip 2:
#   the guard is dead, but Great Hall's beforefirstenter re-arms the maze "news" flag,
#   so the pending guard-room timeout still ejects us right after first entering the
#   Great Hall (the author anticipated the exploit) -- this trip "spends" that first-enter.
#   Trip 3: east again -- guard room's onexit clears "news", nothing re-arms it, and the
#   timeout fizzles: we are inside for good.
# * A dip south to the fortress courtyard triggers its beforefirstenter (moves the Black
#   Wizard into the Chapel, stages Rap) before returning to the Great Hall.
# * Font blessing: "wash me" with the holy-water chalice (+200 hp AND "protection", which
#   deflects the Cauldron's -50/-75 blood spray on Chapel entry).
# * The Chapel exit only becomes visible after 6 Minion clones spawn (1/turn while in the
#   Great Hall), and entering with clone count > 9 is required to trigger the Voltan
#   resurrection/dilemma sequence -- hence exactly 6 post-wash turns in the Great Hall
#   (bread +50 and egg +50/+1 agility are eaten during the wait; the egg's 25-turn
#   agility buff covers the whole Chapel fight).
# * Chapel endgame: survive ~15 scripted turns of Black Wizard (green lightning ~10-15),
#   Conclave, Squad and Dark-Sword Voltan (1d20+5) while Voltan monologues; when Voltan
#   holds Rap over the Crypt (the one-turn "dilemma" window), "save rap" wins. The
#   trailing save rap lines are padding: premature ones print a harmless refusal and
#   advance the turn, so the window cannot be missed.
increase strength
increase strength
increase strength
increase strength
increase stamina
increase stamina
north
take torch
west
x stone wall
shift stone
take chalice
east
north
x pantry
take tinderbox
south
drop torch
south
southeast
northeast
take pole
southeast
down
in
east
east
east
east
east
take mindsword
west
west
west
west
west
west
out
equip mindsword
attack bumbling acolyte
attack bumbling acolyte
attack bumbling acolyte
attack stumbling acolyte
attack stumbling acolyte
up
northwest
southwest
northwest
north
unequip mindsword
take torch
south
southeast
southwest
west
south
west
south
light torch with tinderbox
fill chalice with basin
dip finger
drop torch
drop tinderbox
x altar
take egg
take bread
north
east
north
northwest
equip mindsword
north
north
attack ruffian
attack ruffian
attack ruffian
attack ruffian
attack ruffian
attack ruffian
attack ruffian
attack ruffian
north
north
north
east
west
south
attack guard
z
north
north
east
west
south
east
north
north
east
west
south
east
south
north
wash me
z
z
z
eat bread
z
eat egg
east
z
z
z
z
z
z
z
z
z
z
save rap
save rap
save rap
save rap
save rap
save rap
save rap
