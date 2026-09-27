# The Will -- walkthrough (**full win**)

- **Engine:** ADRIFT 3.90 Release 19 (`ambrosine@mindspring.com`, 2001).
  A treasure hunt: the player's late uncle's fenced yard leads into the
  house (hallway/foyer/gallery/living room/dining room/kitchen, an upstairs
  and a cellar), and separately into a woods area (shack, hill, tunnels,
  chasm, river) behind the yard. ~50 rooms, 200+ tasks.
- **Result:** **FULL WIN.** All sixteen treasures on the Gallery's oriental
  rug collapse it (+10) and `d` down the revealed stairway (+10) prints the
  WinText. Every scoring task in the data fires -- 27 tasks at +5 plus those
  two at +10 -- for **155 against a declared maximum of 150**, so the summary
  reads "You scored 155 out of the maximum 150." / "You finished -5 points
  short." (how the Runner prints a negative shortfall). Wired as
  `thewill_solution.txt|The_Will.taf|You have completed The Will and inherited a fortune.|`,
  no env; deterministic across seeds.
- **The intro's `<waitkey>`** ("Press any key when ready to play.") eats the
  solution's first line, so line 1 is blank. The old partial solution lacked
  it and its first `w` was eaten.
- **Front Gate:** `unlock gate` (breaks the entrance key off in the lock),
  `oil gate` (the oilcan comes from `search hedges` at the Northwest Corner
  of Yard), `open gate`. The open gate's exit is **up**.
- **Seen objects:** things listed only through `%in_X%`/`%on_X%` room text
  answer "What do you want to take?" until their holder is examined -- `x
  cabinet` (crowbar), `x safe` (mug), `x table` (lamp), `x grate` (carving),
  `x computer` (crystal disk); the wooden pedestal after the sculpture breaks
  needs a `look`. The pocket watch comes straight out after `open clock`.
- **Flashlight:** charge it early (`level cabinet with matchbook` in the
  shack, `open drawer`, `get matchbook`, `put flashlight in charger`), light
  it in the Kitchen and **never switch it off**: after `unlight flashlight`
  (task 103) event 19 clears task 101 ("light flashlight") every turn, and
  the Foot of Hill's east exit is gated on task 101. run390x agrees: an
  off/on cycle right after the first lighting leaves `e` at the Foot of Hill
  refused ("At present your only way out appears to be south."; feed
  cmdfile_willflash.txt, Adrift_304_willflash.txt, 2026-09-26).
- **Other order constraints:** take the carving while the umbrella is still
  hooked on the steam pipe; `feed puppy steak` before `get dogbone`; the
  shovel appears in the Toolshed after `push button` in the Study, and `open
  bag` after digging leaves the rubberband in hand for `wrap band around
  valve` / `turn valve with wrench` at the fountain (pearl).
- **Chasm:** carry the lit flashlight east from the Foot of Hill, take the
  ring, `drop flashlight` (aims it across), go round through the Toolshed
  hole with the oil lamp lit (a lit flashlight burns out going down). At
  the Narrow Squeeze `drop all` (the crack needs empty hands), `n`, take the
  nugget, `throw nugget`, `s`, `get all`, `d`; `extinguish lamp` before `e`
  (a lit lamp or match ignites the gas), then collect nugget and flashlight.
- **River:** `open umbrella` at the Cliff drifts down to River's Edge; swim
  w/s/s/e to the lily and back, `climb up` the vine.
- **"insert" is a game synonym for "put"**, so the boot disk goes in with
  `put disk in computer` ("insert disk" becomes "put disk" and matches
  nothing).
- **`get off pedestal` while standing on it:** the Runner (every version
  below 4.0) takes the pedestal, climbing off first; Scarier dismounts, a
  deliberate deviation (2026-09-27), so the route follows it with `get
  pedestal`.
- **Runner check:** run390x drove the whole 242-command route
  (`runner_transcripts/thewill.txt`, 2026-09-26) and is identical on every
  turn except for the extra `get pedestal` above (the Runner's single
  `get off pedestal` does both). The one earlier difference, the Narrow Squeeze `get all` (task 142
  claims the clover), was a Scarier ordering bug: run390, like run400, puts
  the library's take line before the task's text. Fixed 2026-09-26.
- **Content note:** nothing requiring a check; no minors appear in the game
  text.

## The walkthrough

```

w
n
search hedges
s
e
unlock gate
oil gate
open gate
u
w
n
put umbrella in hole
open umbrella
get necklace
get umbrella
s
e
n
raise flag
open door
n
open clock
get watch
n
move statue
w
get wrench
e
e
move pillows
n
move painting
103221
x safe
get mug
e
open fridge
get meat
n
get flashlight
s
w
w
open window
put watch on rug
put necklace on rug
put mug on rug
drop oilcan
s
u
n
open toilet
stuff will in toilet
flush toilet
get earring
s
w
push armoire
s
get vase
n
e
e
push button
read computer
w
d
n
put earring on rug
put vase on rug
d
w
s
s
s
w
x cabinet
get crowbar
level cabinet with matchbook
open drawer
get matchbook
put flashlight in charger
e
e
get ashes
n
get pickaxe
s
e
climb tree
shake branches
d
get egg
w
w
n
n
n
e
u
put egg on rug
break sculpture with pickaxe
drop pickaxe
look
get wooden pedestal
s
u
stand on pedestal
look at sparkle
get off pedestal
get pedestal
w
throw ashes at mirror
e
d
n
put crown on rug
drop pedestal
e
e
light flashlight
d
put meat on grate
get steak
put umbrella on pipe
open grate with crowbar
x grate
get carving
get umbrella
u
w
x table
get lamp
w
put carving on rug
drop crowbar
s
s
s
open mailbox
get stamp
s
d
e
n
enter tear
get shovel
enter tear
s
w
u
e
dig soil with shovel
open bag
get clover
drop shovel
w
wrap band around valve
turn valve with wrench
x fountain
get pearl
drop wrench
d
w
s
feed puppy steak
get dogbone
s
e
n
e
get ring
drop flashlight
w
s
w
n
n
e
e
n
enter tear
light lamp
d
s
u
e
s
u
get disk
e
d
e
drop all
n
get nugget
throw nugget
s
get all
d
extinguish lamp
e
get nugget
get flashlight
w
s
e
e
open umbrella
w
s
s
e
get lily
w
n
n
climb up
w
n
n
e
u
n
n
n
u
e
put disk in computer
x computer
get crystal disk
w
d
n
put stamp on rug
put dogbone on rug
put clover on rug
put nugget on rug
put pearl on rug
put lily on rug
put ring on rug
put crystal disk on rug
d
```
