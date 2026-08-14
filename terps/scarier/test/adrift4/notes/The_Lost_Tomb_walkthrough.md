# The Lost Tomb — walkthrough (**WIN, 175/175 — full score**)

- **Game:** *The Lost Tomb* (`losttombv2.taf`, 56,336 bytes). No author is
  recorded: the file carries no author byte-field, and `games.manifest.tsv`
  line 123 (`https://www.adrift.co/files/games/losttombv2.taf`) lists only the
  title. You have found the tomb of pharaoh Erick after years of being called
  a fool; your funder, **Lord Rupert Mongoose** — monocle, pith helmet, and an
  alarm clock set for tiffin — has decided to come along.
- **Engine:** **ADRIFT 3.90** (`SCR_DUMP_TASKS=1` prints `GAME version=390`).
- **Size:** 19 rooms, 99 tasks, 86 objects, 1 NPC, 13 events, 6 variables.
- **Result:** **WIN**, ending on *"And with that, you and Rupert start the trek
  back to camp. / Congratulations!"*
- **Score: 175 out of 175 — every point in the game.**
- **Harness row:**
  `losttomb_solution.txt|losttombv2.taf|you and Rupert start the trek back to camp.`,
  **105 commands** (104 moves plus one `score` check), PASSing golden.
- **Source:** none. There is no published walkthrough for this game. The route
  was derived from `SCR_DUMP_TASKS` / `SCR_DUMP_OBJLOC` plus the file's own
  author hint menu (see below).

## 175 is provably the ceiling

The file contains exactly **23 `ACT type=4` awards**, and the route fires all
23:

| task | pts | what |
|------|-----|------|
| T53 | +5 | light the lamp |
| T65 | +5 | empty the water bottle |
| T67 | +5 | fill the bottle with sand |
| T60 | +5 | light the dynamite *in the crack* |
| T78 | +10 | put the right item on the altar |
| T8 | +5 | take the emeralds |
| T4 | +10 | open the first sarcophagus with Rupert |
| T6 | +5 | emeralds into the eagle |
| T13 | +5 | find the spear |
| T81 | +5 | dig out the bricks |
| T96 | +10 | take the sun orb |
| T17 | +5 | Rupert's hand in the hole |
| T14 | +10 | jam the spear between the walls |
| T25 | +5 | cork the jackal |
| T23 | +5 | open the second sarcophagus |
| T28 | +10 | rub the inscription |
| T30 | +10 | the pillar puzzle |
| T31 | +5 | tie the rope to the beam |
| T32 | +10 | tie the rucksack to the rope |
| T36 | +5 | Rupert winds the handle |
| T47 | +20 | read the wall through the complete mask |
| T48 | +10 | answer the riddle |
| T49 | +10 | escape |

`10+5+5+5+10+5+5+5+10+10+5+10+5+20+10+10+5+5+5+5+10+5+10 = 175`, which is what
the game itself declares as its maximum. The golden's `score` check two moves
from the end reads `165 out of a maximum of 175`; the winning `up` is T49's
last +10.

## The author shipped the walkthrough inside the file

There is no solution on the web for this game, and it did not matter. Like
*Veteran Knowledge*, **The Lost Tomb carries an author hint menu, and
`SCR_DUMP_TASKS` prints it**: every puzzle task has `HINTQ=` / `HINT1=` /
`HINT2=` fields holding the author's own two-step nudge. That is a complete
puzzle list with answers-by-implication, free, before a single move is played.
Two games in a row now — worth checking for those fields *first* on any
unwired file.

The one puzzle whose hints are deliberately useless is the riddle itself
(`HINTQ=[I love riddles...] HINT1=[...and for this one, you're on your own...]
HINT2=[...Mu ha ha ha ha ha ha!...]`), and the one whose hint points at a
relationship that does not obviously exist is the numeral floor (below).

## The engine find: `v2=2` is *worn*, not *held*

This is the reason the row is worth carrying beyond its 175 points.

The endgame is to read the north wall of the Riddle Room through the death
mask. Three tasks compete for `x wall` there — T45 (ruby-only mask), T46
(sapphire-only mask), T47 (complete mask, **+20**) — and all three restrict on

```
RESTR type=0 v1=26 v2=2 v3=0   obj44=[death mask]
```

`v2=2` is **worn by the player** (`screstrs.cpp`, `restr_object_in_place`
case 2/8), not held. T44 is the `v2=8` "*not* worn" counterpart for all three
mask variants, and it prints a perfectly plausible description:

> Hundreds of lines of red and blue are painted all over the north wall.

So walking in *carrying* the finished mask produces a sensible-looking wall
description, no error, no refusal — and quietly forfeits the file's single
largest award. The riddle still answers, the escape still wins, and the run
lands on 155/175 with nothing anywhere to say what went wrong. `wear mask` is
the whole difference.

The author's hint says so, obliquely: *"...maybe the wall will become clear
when looked at with the right attitude... ...or eyes..."*

## Four timing shapes and one losing ending

### 1. The dynamite goes in the hole *first*

T59 is "light the dynamite while holding it", and it starts `EVENT 9
[DYNAMITE BOOM!]`, which two turns later runs T61 — *"BOOM! YOURE DEAD!"*.
T60 (+5) is the in-the-wall version. Two parser notes come with it: the crack
is hidden until `x walls` reveals it, and the object it becomes is aliased
**hole**, so `put dynamite in crack` is not understood. The route is
`x walls` / `take dynamite` / `put dynamite in hole` / `light dynamite`, then
retreat `s` and wait two turns.

### 2. Rupert keeps his own clock

`EVENT 8 [TIFFIN TIME!]` is `starter=2 start=50..80` — a *random* turn between
50 and 80 — and when it fires, Rupert's alarm clock goes off, he stops for tea
and scones, and `VAR 0 [Rupert]` drops to 0 for three turns. Every
`ask rupert to ...` task is gated on that variable, and a request swallowed by
tiffin does not get refused: it falls through to a **flavour** message. Asking
him to open the second sarcophagus mid-tiffin answers

> the lid is too heavy for you

which reads exactly like a wrong solution rather than a bad moment. The route
therefore asks **twice**; under the harness seed the first ask lands inside
the tiffin window.

### 3. The Wall Room is on a two-turn clock and the spear only fits late

Coming back north through the Wall Room, Rupert trips on the loose brick,
pushes it flush, both doors slam, and the east and west walls start closing
(`EVENT 2 [CLOSING WALLS]`, one step every two turns: 12 → 10 → 8 → 6 feet).
Asking him to put his hand in the hole (T17, +5) starts the north door
inching open. `jam spear in walls` (T14, +10) answers

> The walls are too far apart at the moment to do that.

until they reach **6 feet**, which is turn 7 after the ask — hence the nine
`z`s in the script. Two further turns bend the spear and open the door far
enough to squeeze under; `n` then escapes cleanly with both of you.

### 4. The pillar sink takes a turn

`EVENT 6 [PILLAR CHECK]` runs T30 (+10) at the **end** of the turn the fourth
statue is placed, so the ruby the breaking inscription reveals is not in the
room until the turn after. Without a bare `z` first, **`take ruby` does
nothing and says nothing** — and since T40 ("put the ruby in the mask") needs
the ruby held, the mask can never be completed and the run strands at 155/175
with no diagnostic anywhere. This one cost a full replay to see.

The puzzle itself is clued by the rubbing: *"It shows the cat by water, the
snake chasing the sun, the vulture flying above fire, and the dung beetle
hiding in grass"* → cat/blue, snake/yellow, vulture/red, beetle/green. Each
statue must be **taken** before it can be placed (`put cat on blue pillar`
while it is on the floor answers "You are not holding the cat").

### 5. Never climb out of the well holding the mask

The death mask is at the bottom of a well whose sides are "smooth and
slippery". Climbing back up with it is **T33, `ACT type=6 v1=1` — a losing
ending**. T34 catches only the *other* case — mask still inside the rucksack
that is tied to the rope — and turns you back ("you notice a lot of tension
and strain is being put upon it… you climb back down"); it does not protect
you from T33. The intended route sends the mask up on its own: into the
rucksack,
`tie rucksack to rope` (T32, +10), `ask rupert to turn handle` (T36, +5). He
winds it up, detaches it, and drops the empty rucksack back down; the mask is
waiting in the Well Room.

## The numeral floor is unclued past the first step

Four rows of five painted Roman numerals stand between the lower corridor and
the crocodile statue:

```
VI  III  IV   X    XI
V   II   VIII IV   V
VI  I    XX   XIV  II
X   XVII V    VII  IV
```

Every panel that is not on the safe path is a task with `ACT type=6 v1=2` —
instant death (T90–T93). The safe path is **X → VIII → XIV → XVII**, and only
the first step is signposted in play: the row-1 `X` is painted red while
everything else is black. The author's hint is

> ...You'll need to find the relationship between them to take a step in the
> right direction... ...maybe the right number has something to do with the
> next row...

but whatever relation that describes, nothing in the game text states it, and
the answers are simply hard-coded as the `ALTCMD` patterns of T86–T89. The
route uses them.

## Two more small things

- **The altar is the Indiana Jones gag.** The Secret Chamber's altar holds a
  moon orb under a hole cut in the ceiling. T78 (+10) wants a *weighted*
  substitute: `empty bottle` (+5) then `fill bottle with sand` (+5) in the
  tent, and `put bottle on altar` before `take orb`.
- **The crocodile eats hands.** T95 (`put hand in mouth` with `VAR 5 [croc]`
  still 0) is `ACT type=6 v1=1`, another losing ending. Jamming the jaws with
  the pencil first (`put pencil in mouth`) snaps the pencil *and* the teeth,
  after which T96 (+10) hands over the sun orb harmlessly. The snapped pencil
  still works for the rubbing later — the game says so explicitly.

## Full command list

```
in / take rucksack / open rucksack / take lamp / take matches / light lamp /
take bottle / empty bottle / fill bottle with sand / take cork / out /
n / n / x walls / take dynamite / put dynamite in hole / light dynamite /
s / z / z / n /
e / put bottle on altar / take orb / w /
n / take emeralds / ask rupert to open sarcophagus / open sarcophagus /
put emeralds in eagle /
n / x walls / n / search reeds / s / dig bricks /
w / x numerals / step on x / step on viii / step on xiv / step on xvii / n /
take pencil / put pencil in mouth / put hand in mouth / s / e /
n / n / ask rupert to put hand in hole /
z / z / z / z / z / z / z / z / z / jam spear in walls / z / z / n /
put cork in mouth / ask rupert to open sarcophagus /
ask rupert to open sarcophagus / open sarcophagus / take sapphire /
nw / take notebook / open notebook / take paper / put paper on inscription /
rub paper with pencil / take cat / put cat on blue pillar / take vulture /
put vulture on red pillar / take snake / put snake on yellow pillar /
take dung beetle / put dung beetle on green pillar / z / take ruby /
se / ne / take rope / tie rope to beam / d / take mask / put mask in rucksack /
tie rucksack to rope / ask rupert to turn handle / u / take rucksack /
take mask / put ruby in mask / put sapphire in mask /
sw / n / wear mask / x wall / drunk / score / u
```

The commented version, with the task numbers each line fires, is
`goldens/losttomb_solution.txt`.

## Nothing is left on the table

Every `ACT type=4` in the file is banked, so there is no honest "unreachable
points" note to make here — unlike *Three Monkeys, One Cage* or *The
Hangover*, this game's declared maximum and its actual maximum are the same
number. The only unfired scoring-adjacent tasks are the death and near-miss
variants (T33, T59, T61, T90–T93, T95), none of which award anything.
