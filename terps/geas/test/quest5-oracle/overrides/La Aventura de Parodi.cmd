# La Aventura de Parodi (Lucas Cavanna Quero, "Luquitash que Dioz lo vEndiga",
# 2014, ASL 550, Spanish) — derived entirely from the game source: NO published
# walkthrough exists (corpus.tsv wt="-"). A 7-room joke game built on an
# embedded Spanish translation library; the win is simply `matar panchi`
# (kill Panchi el Magnífico, alias "Panchi") in "el final" — the kill script
# calls `finish` unconditionally. The minimal win is 6 moves + 1 kill; this
# script also exercises every authored response along the way (firsttime/
# otherwise use scripts, container open/close, switchable on/off, eat/sit
# MakeObjectInvisible scripts, picture() looks, drop=false takemsg/dropmsg).
# Notes:
#  - The game has NO <object name="player"> element at all; the engine's
#    implicit default player starts in the first room (el cuarto de Rapodi).
#  - Spanish ParserIgnorePrefixes lacks "a", so it must be `matar panchi`,
#    not `matar a panchi`.
#  - The sit verb's alternatives are tried in order, and `sientate #object#`
#    precedes `sientate en #object#` — so `sientate en lucas` misparses
#    (object "en lucas"); use `sientate lucas`.
#  - Street-Panchi is killed twice (firsttime revive, otherwise
#    MakeObjectInvisible) BEFORE reaching the boss, so the boss's bare
#    "Panchi" alias can never be shadowed by a carried namesake.
mirar
examina cama
dormir cama
examina chaqueta
coge chaqueta
deja chaqueta
usa chaqueta
usa chaqueta
norte
examina mesa
examina jesús
habla con jesús
matar jesús
come mesa
abajo
examina vagabundo
abre vagabundo
cierra vagabundo
examina crucifijo
coge crucifijo
enciende crucifijo
apaga crucifijo
usa crucifijo
deja crucifijo
oeste
examina panchi
coge panchi
deja panchi
matar panchi
matar panchi
examina pancito
coge pancito
come pancito
deja pancito
norte
examina lucas
sientate lucas
noroeste
examina espada
coge espada
deja espada
examina panchi
matar panchi
