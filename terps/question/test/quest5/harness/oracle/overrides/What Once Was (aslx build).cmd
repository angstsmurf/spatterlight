# What Once Was (Luke A. Jones, IF Comp 2017; Quest 5) — winning script
# RESULT: state=Finished, errors=0 — "Congratulations, you have completed the game! (Ending 1 of 2)"
#   (the Redeemed Dean ending: trust Dr Alan Berthill in 1997; Prof meets the reformed Dean in the pub)
#   + full credits via the auto-continued "Press any key" wait. Verified deterministic (seed 1234, two runs byte-identical).
# SOURCE: "What Once Was - walkthrough.txt" (author's own hints file). It is HINTS-ONLY —
#   per-puzzle solutions, no linear command list — so this entire script is derived from
#   the hints plus the game source (game.aslx) for exact vocabulary, routes and gating.
#
# Derivation / deviation notes:
#  - Ending choice: the fate-of-Alan menu offers Kill (Ending 2 path variant) vs Trust.
#    Trust yields "Ending 1 of 2" with the Redeemed Dean visible in the pub — the best/canonical
#    ending — and is the one this script drives. That menu is Core's numbered TEXT menu
#    ("1: Kill / 2: Trust", menuallowcancel=false), so it is answered with the bare number "2",
#    NOT a menu: directive (a menu: line there is silently swallowed and wedges the run).
#  - get-input answers sent as plain lines: lab keypad code "2444666668888888" (after "use keypad"),
#    Greggs order "cheese and onion bake" (after "speak to shopkeeper"), and Phil's quiet-room
#    answer "profs office" (after "give journal article to phil").
#  - "give marker to phil" must come BEFORE "give cartridge to phil": once the cartridge is handed
#    over Phil's "Scribbled note from Phil" enters inventory and "phil" becomes ambiguous
#    (disambiguation text-menu would swallow the next commands). Marker-first hits the same
#    phil_pen flag via its no-cartridge branch ("Thanks, they always come in handy").
#  - Parser exactness (QuestViva matches contiguous substrings of the alias, or of the name only
#    when no alias exists): "x pigeon-hole cabinet" needs the hyphen (alias "Pigeon-hole Cabinet");
#    the librarian's photocopy is "journal article" (its alias — the internal name "journal paper"
#    does not resolve); the replacement cartridge needs its full "new temporal algorithm cartridge"
#    ("new cartridge" is a non-contiguous word pair and fails).
#  - The 1997 finale cutscene is three chained SetTimeout(5)s fired by the harness's DrainTimers;
#    the pub ending is a SetTurnTimeout(7) turn-based timer, so exactly 6 "wait" turns after
#    entering the Cross Keys Inn bring it due (the pub's onexit blocks leaving meanwhile).
#  - Time Suit and Guard Uniform share the body wear-slot: the suit is removed for the 2037
#    guard-uniform excursion and re-worn before re-entering the temporal field.
#  - In the 2017 lab the field is entered with "step into temporal field" ("use" on that side is a
#    no-op author bug — it moves you to the room you are already in); on the 2037 side "use
#    temporal field" is the correct return path.
#  - Optional flavour kept: "read email" (opening story beat). Easter eggs (Doolittle headset,
#    pigeon conversations) are skipped.
read email
open drawer
take passport
out
e
x pigeon-hole cabinet
w
w
open remote control
take battery
e
down
down
w
give battery to porter
e
use keypad
2444666668888888
e
take cartridge
take broken power unit
w
down
w
x shelf
take white spirit
x mousetrap
fill zippo
e
up
up
up
e
unlock cupboard
open cupboard
take paper
take marker
put paper in photocopier
photocopy passport
w
down
north
north
speak to librarian
south
put paper on boulder
down
use zippo
open filing cabinet
take incriminating evidence
up
west
southwest
give form to alex
give note from professor weck to alex
northeast
south
speak to shopkeeper
cheese and onion bake
north
north
give cheese and onion bake to phil
give marker to phil
give cartridge to phil
south
east
north
speak to librarian
give note from phil to librarian
south
west
north
give journal article to phil
profs office
south
east
south
west
speak to phil
east
north
east
north
give incriminating evidence to secretary
north
speak to dean
south
south
east
give purchase order to clerk
west
west
south
down
down
east
give season ticket to barry
give broken power unit to barry
speak to yi lu
west
up
up
use vending machine
drink coke
crush can
down
east
put new temporal algorithm cartridge in slot
put crushed can in slot
put fixed power unit in socket
wear time suit
turn on integrator
west
up
up
in
put cheese in small hole
out
down
give cheese to gerald
up
west
unlock locker
open locker
take guard uniform
remove time suit
wear guard uniform
east
down
north
west
south
take prospectus
north
east
south
down
down
east
take magnetron
west
up
east
remove guard uniform
wear time suit
use temporal field
west
up
up
west
fix microwave
use microwave
east
down
down
east
step into temporal field
give ptfm to professor weck
give prospectus to professor weck
speak to professor weck
west
up
north
east
east
give note to alan
west
west
south
down
east
2
west
up
north
west
west
wait
wait
wait
wait
wait
wait
