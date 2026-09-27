# Everyman — Simon Deimel, 2013 (ASL 540, Quest 5.4)
# No published walkthrough exists — derived from game source (game.aslx).
# Ending: the game's only finish — on stage with the microphone switched on,
# `tell joke` (the joke object's tell verb) prints the victory text and calls
# finish ("You needn't be the best to be somebody... Congratulations!").
# Route/gates: pen+hammer in the dresser; towel in the bath room; milk in the
# fridge; knife revealed by examining the silverware; sleeping pill + leaflet
# in the nightstand. `x leaflet` (firsttime) unlocks the front door and hands
# over the application form; `use sleeping pill on milk` (not at the temple)
# mixes the sleeping potion; `use pen on application form` completes the form.
# Leaving the flat is ONE-WAY (the front-door script locks the return exit),
# so everything must be collected first. Give the potion to the temple guard
# (he thinks it is milk) to unlock the temple; `x pews` reveals the stain,
# `use towel on stain` uncovers the code 69105, entered into the electronic
# device as a get-input answer. Frying pan knocks out the goblin guarding the
# holy book of jokes; reading it sets the "joker" flag. Hammer opens the
# fence at the dead end; knife picks the costume-shop keyhole for the clown
# suit. The completed form given to the guy opens the lobby; the suit can
# only be worn in the wardrobe; the stage exit requires suit+joker.
# Flavour included (all deterministic): sign, speak to guard, x number,
# x mirror in the suit. No score, no RNG, no timers.
open dresser
take pen
take hammer
e
take towel
w
s
open fridge
take milk
open cabinet
x silverware
take knife
take frying pan
e
open nightstand
take sleeping pill
x leaflet
use sleeping pill on milk
use pen on application form
w
n
n
w
n
w
x sign
speak to guard
give sleeping potion to guard
w
x pews
use towel on stain
x number
x small door
use electronic device
69105
s
down
use frying pan on goblin
read book
up
n
e
e
e
e
use hammer on fence
e
x back door
use knife on keyhole
e
x leftovers
take clown suit
w
w
w
w
s
s
sw
x guy
give application form to guy
s
s
wear clown suit
x mirror
n
e
switch on microphone
tell joke
