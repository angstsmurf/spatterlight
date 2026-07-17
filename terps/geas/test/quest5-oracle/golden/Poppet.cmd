# Poppet (Bitter Karella, IF Comp 2019, Quest 5.8) — winning script
# RESULT: state=Finished, errors=0 — the "Good night, Poppet." ending (the good ending:
#   shoe in the hole, teeth-n-claw necklace on the boy, untie him; he escapes and Poppet
#   drifts back to sleep with no regrets).
# SOURCE: "Poppet - Walkthrough and map.txt" (David Welbourn / Key & Compass, welbourn
#   format), extracted with extract_walkthrough.py --mode welbourn. The author's own
#   walkthrough is Q&A hints only (and half its prose is recycled from Night House),
#   so the Welbourn solution is the linear source.
# Deviations from the raw Welbourn extraction:
#  - "ask Piroutte about china" -> "ask Pirouette about china": walkthrough typo the
#    parser rejects ("I can't see that").
#  - Inserted "2" after "use claw on shoelace": the loose claw pried off the taxidermy
#    animal is an object literally named "claw", but the animal itself also has
#    alt "claw"/"claws", so the command opens a disambiguation menu
#    ("1: taxidermy animal / 2: claw"); the number answers it. (Welbourn's preceding
#    "pick up claw" resolves to the animal's attached claws and is a no-op — the den's
#    room command for "take claw" already pocketed the loose claw — but it is harmless
#    and kept for fidelity.)
#  - Everything else is the walkthrough verbatim, one command per line. Both mutually
#    exclusive endings exist ("Good morning" = bring the teapot to the kitchen); this
#    script never takes the teapot, so only the good ending is driven.
help
x me
i
s
turn doorknob
s
x bed
x pillow
x window
x wallpaper
x coats
n
x morning coat
search morning coat
s
e
x cat
take whiskers
x carpet
n
x toilet
x bathtub
x perfume
take it
s
s
x drawing
x dummy
x fan
x shelf
x hat box
x machine
x sewing box
n
e
x gap
x bookshelf
x window seat
s
x footprints
x Birgitta
x dollhouse
x facade
take it
x carriage
x mural
n
use facade on gap
d
x cabinet
open it
x buttonhook
take it
x shoelace
take it
x bannister
u
w
s
use buttonhook on sewing box
take scissors
take needle
use perfume on fan
use matchbook on fan
x hat box
use scissors on hat box
x face
n
e
d
s
x painting
x divan
read paper
x fireplace
x ashes
x poker
x curtains
take poker
w
x table
x chairs
x fireplace
w
s
x grate
w
x animal
x chair
x fireplace
x rug
x heads
speak to animal
use needle on animal
take copper key
e
n
n
x can
take it
x graffiti
x door
x tiny door
n
w
unlock door with copper key
w
x figure
x desk
open desk
x envelope
open envelope
x tome
read tome
ask tome about 1
ask tome about 2
ask tome about 3
ask tome about 4
ask tome about 5
ask tome about 6
ask tome about 7
ask tome about 8
x cabinet
open cabinet
take brass key
e
unlock tiny door with brass key
n
x gum
take gum
s
e
u
w
take whisker
e
d
s
w
use poker on fireplace
x concealed shoe
take it
take ashes
w
x pile
x Jingles
speak to Jingles
x Tomas
speak to Tomas
x Pirouette
speak to Pirouette
use gum on face
use face on Pirouette
speak to Pirouette
x sink
open cupboard
x grease trap
x tutu
ask Pirouette about tutu
ask Pirouette about china
w
x Winston
x thyme
x bay
take thyme
take bay
speak to Winston
ask Winston about Polly
ask Winston about Juliette
ask Winston about china
ask Winston about trapdoor
e
use ashes on grease
use whisker on grease
use thyme on grease
x unguent
take it
e
n
w
use unguent on skeleton
ask skeleton about Polly
ask skeleton about Juliette
ask skeleton about me
ask skeleton about skeleton
ask skeleton about Winston
ask skeleton about Pirouette
ask skeleton about Jingles
e
e
u
w
w
use bay on pillow
e
e
d
w
s
s
open grate
x bell
take it
n
w
give bell to Jingles
ask Jingles about Juliette
ask Jingles about Winston
ask Jingles about china
take china
take eye
use muslin on eye
w
d
x hole
x boy
speak to boy
ask boy about boy
ask boy about Juliette
x dress
ask boy about dress
x teapot
x toolbox
open it
take pliers
x pliers
ask Owen about Westside Hawks
ask Owen about spraypaint can
untie Owen
untie Owen
u
e
e
e
n
u
s
speak to Birgitta
n
d
s
w
s
w
take claw
pick up claw
use claw on shoelace
2
use teeth on necklace
e
n
w
w
d
put shoe in hole
give necklace to boy
untie boy
