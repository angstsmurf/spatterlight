#!clock=5
# The Encyclopedia of Elementals (Adam Holbrook, 2013, ASL 540) -- WON
#
# RESULT: state=Finished, errors=0 -- all five sections, ending with the `end`
# command in the End room (the game's only non-death `finish`). The script runs on
# the typing clock (`#!clock=5`: five seconds per typed command, nothing drained)
# because several beats are real-time: the Castle Main Hall rescue window, the fort's
# patrol timers, the Light timer, and the acid run in Final Fire.
#
# SOURCE: no published walkthrough exists anywhere for this game -- derived entirely
# from game.aslx (15617 lines, ASL 540).
#
# CHARACTER CREATION: name Hawk, gender 1=Male, aspect 1=Fire (the Fire timer is then
# harmless and the fire amulet raises damage), input colour 1 (cosmetic).
#
# COMBAT (Sections 2-5): every fight is `slash <enemy>` plus a `get input` defence
# (bare DEFEND/PARRY/DODGE line, no clock tick) chosen from the enemy's attack text:
# slash-type tells -> defend, thrust-type -> parry (90% counter), slam-type -> dodge;
# "subtly readies his attack" is ambiguous and gets defend. The RNG depends on the
# whole command history, so each fight was derived adaptively in order; editing an
# earlier line re-rolls every later fight and the route must be re-derived from there.
#
# SECTION 1 (Castle): nightstand glow powder, slippers, torch-passage, rock-passage,
# "x table" + "take open book" (sets GoBoom), then the Main Hall rescue inside its
# 70-real-second window: x rubble / take metal pole / use pole on beams.
# SECTION 2 (Drensburg): feathers for the cobbler, Sheila's needle via the magnet and
# the mine's odd rocks, the well rescue (glow powder, washtub, rope, winch), suit +
# flowers + roast, Hephaestus' cooking, dinner answers 2/2/1 -> ruby -> sword,
# training fight.
# SECTION 3 (Ruined Castle flashback): six goblin fights; build the cutters, the
# ladder and the board bridge; spellbook from the nightstand; Gust the flames,
# bed plank across the fire, banish the fire elemental, take its amulet.
# SECTION 4 (Druids / Fort): shaman sells Entangle (`buy spell` never deducts the
# 100 gold -- a game bug, harmless here); bedroll scribing menu: scribe Gust (16 of 18
# free pages). Fort: "x battle remnants" + "throw hilt" clears the front gate for
# free; health potion from the shelves; Gust on the crates reveals the invisibility
# potion; surprise-attack each patrolling guard (hallway 1, hallway 2, tower
# midlevel); "push patrolling guard" at the top of the tower and fight the second;
# closet pole torch + glow powder, "wave pole torch" -- this opens the homeland gate
# but respawns every guard, so fight back down (Full Heal and Lesser Healing cast
# between fights) and take the two traffic-gate guards from behind.
# SECTION 5 (Home): the light elemental captures you; nine `wait`s in the cell until
# the force elemental wrecks the prison; "x safe" then take amulet, Lesser Healing
# potion, glow powder, spellbook, armor, and the sword LAST (taking it starts a
# 15-second timer before the knight attacks); fight the silent knight; eleven `run`s
# through the acid siege; brew the dispel potion (salted flour + salted bonemeal +
# boil + the last glow powder; the shed is dark, so throw some glow powder there
# first); pour it on the crystal necromancer and kill him in the Final Fight.
Hawk
1
1
1
get up
open nightstand
take glow powder
look under bed
take slippers
go hallway
go main hall
x throne
x tapestry
go mysterious wing
x torch
pull torch
go secret passage
use glow powder
x loose rock
push rock
go forbidden library
x bookshelves
x table
take open book
x Encyclopedia of Elementals
go secret passage
go mysterious wing
go main hall
x rubble
take metal pole
use pole on beams
go small cottage
go inside the house
go downstairs
speak to man
go outside
speak to man
shake hands
take crank
go inside the house
go downstairs
take rope
go upstairs
go upstairs
look under bed
take pot
go downstairs
go outside
use garden
go town
go cobbler's shop
speak to woman
go outside
go butcher's shop
x bird
take feathers
buy feathers
go outside
go cobbler's shop
put feathers on shoes
speak to sheila
go outside
go tailor's shop
speak to sheila
go outside
go hephaestus' house
go mines
x table
take odd rocks
go hephaestus' house
go town
go tailor's shop
use magnet on mess
give needle to tailor
speak to sheila
go outside
use crank on well
use rope on well
go butcher's shop
take washtub
go outside
use glow powder on well
go bottom of the well
use washtub on rope
use washtub on boy
climb rope
turn winch
speak to sheila
go tailor's shop
speak to tailor
buy suit
wear suit
go outside
buy flowers
go butcher's shop
speak to butcher
buy roast
go outside
go hephaestus' house
go inside the house
go downstairs
give pot to hephaestus
go upstairs
speak to hephaestus
go outside
go town
go sheila's mansion
speak to sheila
2
2
1
go hephaestus' house
give ruby to hephaestus
go inside the house
go downstairs
speak to hephaestus
go upstairs
go outside
go town
speak to sheila
go hephaestus' house
speak to hephaestus
ask hephaestus about training
slash blacksmith
dodge
slash blacksmith
dodge
slash blacksmith
defend
slash blacksmith
speak to hephaestus
speak to hephaestus

go castle entrance
go dungeon
listen
use glow powder
slash goblin on the left
slash goblin on the left
defend
slash goblin
defend
slash goblin
defend
slash goblin
go torture chamber
knock on brass bull

slash goblin
defend
slash goblin
parry
slash goblin
take metal nails
go up the stairs

go west wing
take ten foot pole
go forbidden library

slash goblin
defend
slash goblin
x chandelier
take metal shards
go west wing
go main hall
go royal chambers
look under bed
take metal beam
take wooden shafts
go main hall
go kitchen

slash goblin
defend
slash goblin
defend
slash goblin
defend
slash goblin
parry
take broken ladder
go cellar

slash goblin on the left
dodge
dodge
slash goblin on the left
dodge
defend
slash goblin on the left
defend
slash goblin
defend
slash goblin
take hinge
use hinge on metal shards
use wooden shafts on cutters
use ten foot pole on broken ladder
go kitchen
go main hall
use ladder on ruined staircase
climb ladder
take large board
use metal beam on large board
use metal nails on large board
use large board on gap
go across the gap
use cutters on metal beams
go your bedroom
x nightstand
take spellbook
go hallway
go main hall
open spellbook
2
3
cast at flames
go torture chamber

slash goblin
dodge
slash goblin
parry
slash goblin
go dungeon
x middle cell
take bed plank
use bed plank on flames
go outside
banish fire elemental
take amulet
speak to strangers

go outside
go garden
speak to gardener
go town center
go shaman's hut
speak to shaman


ask shaman about magic
buy spell
go outside
go hut
sleep in bedroll
1
2
4
3
1
sleep in bedroll

x battle remnants
throw hilt
go traffic gate
x shelves
take health potion
open spellbook
2
3
cast at crates
take potion of invisiblity
go north hallway
attack patrolling guard

slash guard
slash guard
defend
slash guard
defend
slash guard
defend
slash guard
go around the corner
attack patrolling guard

slash guard
slash guard
parry
slash guard
defend
slash guard
go up the stairs
attack guard

slash guard
slash guard
defend
slash guard
defend
slash guard
dodge
slash guard
go top of the tower
push patrolling guard

slash guard
slash guard
defend
slash guard
dodge
slash guard
parry
slash guard
open closet
take pole torch
use glow powder on pole torch
wave pole torch
go down the stairs
attack guard

slash guard
slash guard
defend
slash guard
parry
go down the stairs
attack guard

slash guard
slash guard
dodge
slash guard
defend
slash guard
dodge
slash guard
open spellbook
2
1
go traffic gate hallway
attack guard

slash guard
slash guard
defend
slash guard
defend
slash guard
open spellbook
2
1
drink health potion
go traffic gate
fight guards

slash guard on the left
slash guard on the left
parry
dodge
slash guard on the left
parry
parry
slash guard on the left
defend
defend
slash guard on the left
defend
slash guard
defend
slash guard
defend
slash guard
defend
slash guard
go home

wait
wait
wait
wait
wait
wait
wait
wait
wait


x safe
take amulet
take potion of lesser healing
take glow powder
take spellbook
take armor
take sword
fight knight

slash silent knight
defend
slash silent knight
defend
slash silent knight
defend
slash silent knight
defend
slash silent knight
drink potion of lesser healing
go center of town


run
run
run
run
run
run
run
run
run
run
run



go bakery
take sack of flour
x countertops
take bowl of salt
use bowl of salt on flour
go outside
go open shed
use glow powder
take fertilizer
use bowl of salt on fertilizer
go outside
go your house
take experimental health potion
use salted bonemeal on alchemy set
use salted flour on alchemy set
use alchemy set
use glow powder on alchemy set
go outside
drink experimental health potion
use dispel magic potion on necromancer

slash necromancer
parry
slash necromancer
parry
slash necromancer
defend
slash necromancer
parry
slash necromancer
parry
slash necromancer
parry
slash necromancer




end
