# Monster Island -- walkthrough (**full win, 2650**)

- **Engine:** ADRIFT 4.00, first person. A Frankenstein/Dracula
  monster-hunting comic adventure: a wrecked-ship beach feeds a
  jungle/cemetery/village area, a black-powder and lead/silver ball-casting
  puzzle, a generator/gas mechanic, and Frankenstein/vampire-bat/Dracula/
  Wolfman encounters.
- **Result:** **FULL WIN at 2650** -- every ChangeScore task in the game
  (`SCR_DUMP_TASKS`). The game's own `score` reads "maximum of 0", so there
  is no score line to match; the marker is the ending text. Wired as
  `monsterisland_solution.txt|MonsterIsland.taf|Ameila and I rushed to her plane and we flew off to safety.|`,
  no env. Wins on the default seed and on SCR_SEED=1, 2, 3, 7 and 42.
- **Scope:** objects on a surface are out of scope until the surface is
  examined, so the route examines each one first (`x table`, `x bench`,
  `x pit`, `x mold`, ...).
- **Vampire bat:** it circles cemetery -> village -> pier -> dirt road ->
  hidden, one room per turn, and chases the player back to the jungle on
  meeting. Leaving the shack on the turn it is hidden reaches the
  blacksmith's shop (which it never enters) unmet; wearing the garlic
  (+50) then makes it harmless.
- **Black powder (+300):** potassium nitrate from the florist's box,
  charcoal (and the tent stake) from the camp site's fire pit, sulphur
  found by flare-light in the dark lower cave, all in the bowl.
- **Balls:** the boat chest's fishing weights melt in the vat (+100) and
  cast in the mold on the bench (+100); later the castle's silver
  candlestick makes one silver ball (+100, +100).
- **Castle:** eat the banana and drop the peel in front of Frankenstein's
  monster (+500), shoot the drawbridge switch with the lead ball (+200);
  `stake dracula` with stake and mallet (+500) frees the bronze key.
- **Generator:** the blacksmith's apron stays on his body (carrying it makes
  the bucket too heavy); siphon the boat motor's fuel into the bucket with
  the tubing (+100), pour it into the generator and `press start` to power
  the jail cell, then free Ameila.
- **Ending:** freeing Ameila sends the Wolfman to the pier and her
  seaplane; `shoot wolfman` with the silver ball (+500) wins.
- **Runner check:** run400x (`runner_transcripts/monsterisland.txt`)
  matches the whole route, win included, except two "Inside/On X is" list
  lines that Scarier deliberately prints as "are".
- **Content note:** the ending text mentions an NPC's sexual orientation in
  a throwaway line -- no sexual content, no minors anywhere on the route.

## The walkthrough

```
d
x chest
get harpoon
get kit
open kit
get flares
u
e
look
throw harpoon at the creature
look
throw harpoon at the creature
look
throw harpoon at the creature
look
throw harpoon at the creature
look
throw harpoon at the creature
look
throw harpoon at the creature
look
x creature
get key
e
unlock gate
open gate
get banana
s
e
get musket
x table
get bowl
get mold
get notecard
w
s
e
x blacksmith
x bench
get garlic
wear garlic
get mallet
put mold on bench
w
s
s
get box
open box
get nitrate
drop box
n
u
x tent
x pit
get stake
get charcoal
d
n
n
n
w
n
d
twist flare
get deposits
put nitrate in bowl
put charcoal in bowl
put deposits in bowl
u
s
w
d
x chest
get weights
u
e
e
s
s
e
put weights in vat
pour lead into mold
x mold
get balls
load gun with lead ball
w
s
u
eat banana
u
n
drop peel
shoot switch
in
x table
get stick
w
u
open coffin
stake dracula
get key
d
d
twist flare
get tubing
n
unlock door
w
e
s
u
e
out
s
d
d
n
e
put stick in vat
pour silver into mold
x mold
get ball
load gun with silver ball
w
w
get bucket
e
n
n
w
w
siphon gas into bucket
e
e
s
s
s
u
u
n
in
w
d
n
w
pour gas into generator
press start
e
n
press button
in
out
s
s
u
e
out
s
d
d
n
w
shoot wolfman
```
