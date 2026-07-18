# Welcome to the Paris Hotel! — CentaurOfAttn, 2017 (ASL 550)
# No published walkthrough exists — derived from game source (game.aslx).
# Part of a custom fiction experience: you are Dani, tracking the missing
# spy Marilyn Copeland through the Paris Hotel, Las Vegas.
# Ending: `x paper` (the slip hidden in Room 1988's Gideon Bible) decodes
# Marilyn's letter (TextFX_Unscramble behind a SetTimeout(4), drained by the
# oracle) and calls finish. Chain: pull the Journey slot's gold lever
# (firsttime jackpot) -> voucher -> redemption hub -> two dollars -> buy gum;
# "A" tile from the crepe stand completes the mural's ROT_TING -> ROTATING
# (use tile on space) opening the secret room; chewed gum (wrapper in pocket)
# stuck on the button-on-a-string makes the sticky button that fishes
# Marilyn's ID out of the ski-ball room grate; the guest-services attendant
# is answered via two get-inputs ("Marilyn Copeland", "yes") and hands over
# the spare key (SetTimeout thank-yous, drained); the key talks past the
# elevator guard; `push 19` in the elevator; Room 1987 (the stated room) is
# toured first — the clue is actually next door in Room 1988. No RNG (the
# slot is a firsttime script, not a die roll); no score.
n
n
x slot machines
x journey
pull gold lever
take voucher
n
n
use voucher on redemption hub
x grate
s
x mural
w
x tile
take tile
e
use tile on space
e
take button on a string
w
s
s
w
buy gum
chew gum
use gum on button
e
n
n
n
take marilyn id
s
s
s
e
speak to attendant
Marilyn Copeland
yes
w
n
e
push 19
go to room 1987
x nightstand
open nightstand
x bible
go to the hallway
go to room 1988
x nightstand
x bible
x paper
