# Sword and Spell -- Caleb Wilson, 2015 (Quest 5, ASL 550) -- WINNING SCRIPT
#
# RESULT: state=Finished, errors=0. "Version 1.0 / first part of the game" -- the
# win is reaching the end of Part 1: enter the Portal to valley in the Mysterious
# building, which prints "THIS IS THE END OF \"SWORD AND SPELL,\" FOR NOW."
#
# SOURCE: no published walkthrough exists anywhere; derived entirely from
# game.aslx. The portal's enter script never calls `finish` (Serpent's-Eye /
# Zen-Garden pattern), so the script ends with the game's own `quit`
# (request(Quit)) to settle state=Finished in the oracle.
#
# You are Sam, a modern boy woken in an ancient past, a latent wizard. Spell pages
# each carry one Latin word; the whole of Part 1 is a chain of locked gates, each
# opened by the right spell page or item:
#  * Spell page 1 "Resero" (desk drawer, Bedroom) unlocks the bedroom door.
#  * Spell page 3 "Territo" (under the pile of clothes, Closet) scares off the
#    Unicornium at the north gate -> it drops the KEY and unlocks the way north.
#  * The KEY opens the Family chest (Living Room): choose ONE of four elemental
#    swords -> you become that elementalist and receive the matching element page.
#    ANY sword wins; this script picks Fire (Fire page 1 "Ustulo").
#  * The element page kills the Nantantis lanugo (puff ball) blocking the path to
#    Deseron. (A plain sword swing does nothing -- only the spell page works.)
#  * In Deseron's Medieval Mart, `take map` (you "steal" it) unlocks the east
#    exit "hey" into the Mysterious building.
#  * enter portal -> end of Part 1.
#
# Spell page 2 "Integro" (Mirror, Bathroom) and the town shops (drabbles ->
# knife/vial/beer/bracers/crustulum) are optional colour, not needed to finish.
#
# MAP (house): Bedroom -s-> Hallway; Hallway -ne-> Kitchen -w-> Dining Room
#   -nw-> Living Room; Living Room -s-> Closet, -n-> Front yard; Front yard -n->
#   Clearing -n-> Path through the woods -nw-> Deseron; Deseron -ne-> Medieval
#   Mart, -e-> Mysterious building.
look
x sam
x desk
open desk
take spell page 1
x spell page 1
use spell page 1
south
northeast
west
northwest
south
x pile of clothes
move pile of clothes
take spell page 3
x spell page 3
north
north
open mailbox
x letter
x unicornium
use spell page 3 on unicornium
x key
south
x family chest
use key on family chest
x fire sword
x water sword
x earth sword
x wind sword
take fire sword
x fire page 1
north
north
north
x nantantis lanugo
use fire page 1 on nantantis lanugo
x drabble 3
northwest
northeast
x map
take map
southwest
east
x old man
x portal to valley
enter portal to valley
quit
