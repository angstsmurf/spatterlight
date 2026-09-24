# Twisted Tales: The Crimson Path (Craig Dutton, 2013, ASL v540) — WINNING
#   walkthrough, override-only (no published walkthrough exists). A gothic
#   fairy-tale mashup: gather items around a cursed village, rescue a dog
#   from a frozen pond, brave a haunted tower, crack a tumbler-lock altar
#   puzzle in a dark cave, and slay a werewolf that turns out to be your
#   own grandfather. Oracle reports state=Finished errors=0, steps=160,
#   scriptExhausted=True.
# Inventory footgun: the player object has a hard maxobjects=5 carry cap,
#   enforced by the engine before any custom take-script even runs. The
#   route below deliberately picks up single-use items right before they're
#   needed and drops them immediately after (copper key, book, coil of
#   rope, silver key, hairbrush) to stay under the cap while still carrying
#   long-lived items (shoe, candle, metal wolf's head/metal sphere, silver
#   sword) across the whole back half of the game.
# Parser footgun: several objects have an internal name that differs from
#   their <alias> — the parser only matches the alias. "notarys door" must
#   be typed "notary's door"; "metal wolf head" must be typed "metal wolf's
#   head". Also "unlock X with Y" is not supported for these object-keyed
#   locks — use the bare "unlock <door>", which auto-uses the matching
#   carried key.
# Hidechildren footgun: containers with <hidechildren/> (wooden chest,
#   the trapdoor's recess, the recess behind the stone-panel/altar wall)
#   don't reveal contents to the parser just by opening them — an explicit
#   "look at <container>" is required before "take <item>" will work.
#   "look in" is not understood; it must be "look at".
# Route: take the shoe (Offering Ground, the start room — needed later for
#   the altar). To Your House: open the cupboard, take the wine, carry it
#   to the church and give it to Father Abanon, who gets drunk and reveals
#   the copper key. Take the key, unlock/open the notary's door, use the
#   padlock (code 1818, dropped in-fiction by Abanon's drunken ramblings),
#   open the wooden chest, look at it to reveal the book, take and read it
#   (reveals wolfsbane's location later), then drop the now-useless key and
#   book to stay under the carry cap.
# To the Windmill: look at the millstone to reveal a coil of rope, take it.
#   To Frozen Pond: take the black dog (you fall through the ice instead),
#   give the rope to the dog (rescues you to the Campfire), take the dog
#   again (now it follows), drop the rope. Take the dog to Tamris the
#   Blacksmith and give it to him; take the candle from the forge. At the
#   Campfire, light the candle (only works there) — it permanently
#   lights the game's five dark rooms once carried into them.
# At Cluster of Pines: pull the wintergreen shrub to reveal a trapdoor,
#   open it, look at the recess to reveal a silver key, take it. North to
#   the Dark Tower: unlock/open the tower door, up to the Tower Room, turn
#   the mirror (destroys the Lich Queen), drop the silver key (done with
#   it), take the hairbrush. Up to the Tower Roof, push the stone statue,
#   take the metal sphere. Back down and out; at Snowdrift, use the
#   hairbrush on the snowdrift to reveal the hidden path north, then drop
#   the hairbrush.
# Detour to Grandmother's House (via Offering Ground/Edge of the Forest):
#   speak to Grandmother (reveals the Huntsman at the Clearing), speak to
#   the Huntsman (kills Grandmother off-page), return and move Grandmother
#   to reveal the metal wolf's head; take it.
# Back to Snowdrift/Narrow Gorge/Hillock: use the metal wolf's head on the
#   indent to open the stone door. In to the Tunnel (now lit by the
#   candle), east to the Stone Chamber: use the metal sphere on the panel
#   to reveal an altar (remotely, in the Square Chamber) and a recess.
#   West/north/northwest to the Square Chamber: put the shoe on the altar
#   (a wrong offering here is fatal — do NOT use the pearl brooch from
#   Your Bedroom, a decoy item). This reveals the hidden Cavern-Columned
#   Hall exit. Southeast/northeast to the Columned Hall: rotate the seven
#   tumblers to the code (crimson x3, peridot x1, cerulean x3, ochre x2,
#   amaranth x3, maroon x1, olive x1), look at the recess to reveal the
#   silver sword, take it.
# Back out through Tunnel/Hillock/Narrow Gorge/Snowdrift/Cluster of Pines
#   to Marsh1/Marsh2/Bottom of the Hill, up to Top of the Hill: take the
#   wolfsbane, use it on the silver sword. Backtrack to Offering Ground and
#   kill the Werewolf — the silver sword plus wolfsbane wins the fight and
#   ends the game.
take shoe
southwest
south
south
west
open cupboard
take bottle of wine
east
east
east
give bottle of wine to Father Abanon
take copper key
west
unlock notary's door
open notary's door
south
use padlock
1818
open wooden chest
look at wooden chest
take book
read book
drop copper key
drop book
north
west
north
north
west
in
look at millstone
take coil of rope
out
east
northeast
north
northeast
take black dog
give coil of rope to black dog
take black dog
drop coil of rope
south
southwest
south
southwest
south
south
east
northeast
give black dog to Tamris the Blacksmith
take candle
southwest
west
north
north
northeast
north
northeast
north
light candle
south
west
southwest
northwest
pull wintergreen shrub
open trapdoor
look at recess
take silver key
north
unlock tower door
open tower door
in
up
turn mirror
drop silver key
take hairbrush
up
push stone statue
take metal sphere
down
down
out
south
northeast
use hairbrush on snowdrift
drop hairbrush
southwest
southeast
east
south
southwest
in
speak to Grandmother
out
northeast
north
west
northeast
speak to Huntsman
southwest
east
south
southwest
in
move Grandmother
take metal wolf's head
out
northeast
north
west
northwest
northeast
north
northeast
use metal wolf's head on indent
in
east
use metal sphere on panel
west
north
northwest
put shoe on altar
southeast
northeast
rotate crimson tumbler
rotate crimson tumbler
rotate crimson tumbler
rotate peridot tumbler
rotate cerulean tumbler
rotate cerulean tumbler
rotate cerulean tumbler
rotate ochre tumbler
rotate ochre tumbler
rotate amaranth tumbler
rotate amaranth tumbler
rotate amaranth tumbler
rotate maroon tumbler
rotate olive tumbler
look at recess
take silver sword
southwest
south
out
southwest
south
southwest
southwest
west
north
up
take wolfsbane
use wolfsbane on silver sword
down
south
east
northeast
southeast
east
south
kill Werewolf
