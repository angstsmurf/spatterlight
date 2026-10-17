# Detective (Spanish): WIN at seed 1 (Caso cerrado, "Detective Profesional"). The mystery is generated at start from the RNG: at the default
# seed 1234 the murder weapon is the caviar jar (jar_caviar), the one object with no <take/>, so the weapon can never be carried and
# REUNE/ACUSA stay locked (a game bug). `#!seed=1` (both drivers) picks a seed whose weapon is takeable: Mansion 1 layout, the lupa and the weapon
# (the thawing quail) both lie in the Comedor, the murder room is the Baño Principal, the murderer is Lizbeth Slocombe. Those facts were read off
# the game state with asserts while authoring; the script itself plays no interrogations, it just does what a solved case would do:
# menu 1,1; n,n,o to the Comedor; coger lupa; ver con lupa codorniz (blood: the murder weapon); coger codorniz; carry it to the Baño Principal
# (e,s,arriba,n,n,o); reune; acusa a Lizbeth.
#!seed=1
1
1
n
n
o
coger lupa
ver con lupa codorniz
coger codorniz
e
s
arriba
n
n
o
reune
acusa a Lizbeth
