# Treasure of the Ghost-King (Craig Dutton, 2012, ASL 550) — a full-size gothic
# treasure hunt (52 rooms, 198 objects). No published walkthrough exists
# anywhere; this 257-step script was derived from the game source and reaches
# the game's only win: `take treasure` in the secret repository (line 3710).
# Everything else that calls `finish` is a death — taking the scythe before the
# landlord has named the Haggwort, taking the Witch's wand while she is alive,
# staking the Prince without the cross (no fear) or without a silver tip,
# re-entering the entrance hall after staking him with his coffin intact, and
# walking into King Obian's tomb without the candlestick wedging the door.
#
# The dependency graph is deep; the spine of it, all verified against the source:
#  - WIN: `say purple` in the tomb (the `say` command, line 3841) creates the
#    east exit to the repository. The riddle is the frieze on the shields --
#    gules/cerise = red, viridescent/malachite = green, alabaster/lily = white,
#    cobalt/cerulean = blue, xanthin/celandine = yellow, chocolate/bole = brown,
#    and byzantine = purple ALONE. "Say what is lost that is simple and true" =
#    name the plain colour that has no partner.
#  - The shields only appear after giving the gold crown to the Ghost of King
#    Obain, who only appears on `x skeleton`, which needs `open sarcophagus`
#    first (it is a closed hidechildren container).
#  - The gold crown comes off the Prince, and killing him needs THREE things at
#    once: a silver-TIPPED stake (a plain one is an instant death), his `fear`
#    flag (set by using the golden cross on him -- and cleared again the moment
#    you leave the sanctum, so cross and stake must be used on the same visit),
#    and his coffin already smashed in the painting-world (otherwise he revives
#    and kills you the next time you cross the entrance hall).
#  - Silver-tipped stake = give the blacksmith the oak stake AND the silver
#    medallion, after reading the blue book; his turnscript then fires while you
#    are still standing in the forge. Stake <- use the axe on the oak trees;
#    axe <- speak to Willheim Bourg TWICE (the second speak is the one that
#    hands it over); Bourg <- wave the Witch's wand at the statue; wand <- the
#    Witch must be dead first. Medallion <- the locked display case <- gold key
#    <- use the BLESSED mirror on Maelok the Ringmaster (he is a demon); mirror
#    <- the four element buttons in the cave behind Crystal Falls; blessing <-
#    the priest, who first needs his faith restored (free his sister Tanith in
#    the family crypt) and needs you to have read the green book.
#  - Killing the Witch is a four-counter (game.witchisdead): open her window,
#    mud her porch, fill her birdbath, and drop the barn seeds INSIDE the
#    cottage. The turnscript only fires while you are standing OUTSIDE, hence
#    the drop-seeds / out / in sequence. Mud is made by emptying a bucket of
#    river water onto the graveyard's pile of soil, then scooping that up.
#  - The bucket itself comes from bribing Gregor with the study's brandy, and
#    the ruby ring that buys the crypt's iron key from Lady Unya needs the
#    maggots (scythe on Haggwort, then scythe on its head) to eat the dead hand
#    in the Doctor's jar so the ring comes free.
#
# Ordering traps the script is built around:
#  - Using the skull on Abbot Unwahn REMOVES the Ghost of Saint Augustine, so
#    the bell must be rung in the ruins and the Ghost heard BEFORE the abbey is
#    revisited for the exorcism -- hence the two trips to Ralton Abbey.
#  - The player has a 10-object inventory limit (feature_limitinventory), so
#    spent items are dropped as soon as their flag is set.
#  - Several containers are `hidechildren`: `x desk`, `x shelves`, `x cart`,
#    `x dining table`, `x hole`, `x recess`, `x display case` and `x straw` are
#    all load-bearing, not flavour.
#  - The way out of the painting is `use hammer on wall` then `enter opening`;
#    there is no other exit from the painted world.
#  - The south tower's rhyme ("As the trees grow, as the tides turn, as the
#    lightning strikes, as the flames burn") is the clue for the cave buttons:
#    earth, water, air, fire. It is read late here only because the tower is
#    deep inside the castle; the buttons are pressed in that order regardless.
#
# Fully deterministic: no RNG, no score, no timers. state=Finished, errors=0.
southeast
in
speak to Erasmus Thorl
out
south
take scythe
use scythe on Haggwort
use scythe on head
take maggots
north
west
southwest
southeast
in
x stone carving
push earth button
push water button
push air button
push fire button
x recess
take mirror
out
northwest
northeast
east
northwest
cross wooden bridge
northeast
north
in
give scythe to Nimon Krene the Farmer
open cupboard
take hacksaw
take hammer
out
west
x straw
take seeds
east
south
open window
southwest
northwest
west
x cart
take shovel
east
north
in
in
east
x desk
take red book
tear red book
unlock library door
open library door
drop bronze key
north
x shelves
take blue book
read blue book
drop blue book
take green book
read green book
drop green book
south
take brandy
west
west
north
give brandy to Gregor the Servant
take bucket
south
east
up
west
use hacksaw on manacle
drop hacksaw
open glass jar
x hand
use maggots on hand
take ruby ring
east
north
give ruby ring to Lady Unya Bloodreich
take iron key
south
down
out
west
unlock gate
open gate
drop iron key
down
speak to Tanith Tallus
up
east
out
south
southeast
use bucket on river
west
in
give mirror to Father Heinrik Tallus
out
empty bucket
use bucket on pile of mud
east
northeast
use bucket on porch
southwest
use bucket on river
northeast
use bucket on birdbath
drop bucket
in
drop seeds
out
in
take wand
out
northeast
push log
take copper key
north
unlock tower door
open tower door
drop copper key
in
up
move shield
x recess
take skull
wave wand
drop wand
speak to Willheim Bourg
speak to Willheim Bourg
take axe
down
out
south
southwest
southwest
cross wooden bridge
southeast
west
west
open abbey door
in
down
use shovel on coalpile
take bell
up
out
east
southwest
southwest
west
ring bell
speak to Ghost of Saint Augustine
drop bell
east
use axe on trees
take wooden stake
drop axe
south
in
use mirror on Maelok the Ringmaster
take gold key
drop mirror
out
east
unlock display case
open display case
x display case
take silver medallion
drop gold key
west
north
northeast
in
give wooden stake to Gunthar Renn the Blacksmith
give silver medallion to Gunthar Renn the Blacksmith
take wooden stake
out
northeast
west
in
north
use skull on Abbot Unwahn
drop skull
speak to Abbot Unwahn
take golden cross
south
out
east
east
northwest
cross wooden bridge
northwest
north
in
in
north
x painting
east
use shovel on vegetable patch
x hole
take ivory key
drop shovel
in
use hammer on coffin
out
southeast
use hammer on wall
drop hammer
enter opening
south
up
south
x small writing on wall
north
north
east
unlock sanctum door
open sanctum door
drop ivory key
north
use golden cross on Prince Elvric Bloodreich
use wooden stake on Prince Elvric Bloodreich
take gold crown
south
west
south
down
west
x dining table
take candlestick
east
out
out
south
southeast
cross wooden bridge
southeast
east
in
open stone door
use candlestick on stone door
east
open sarcophagus
x skeleton
give gold crown to Ghost of King Obain
x shields
say purple
east
take treasure
