# The Sisters — walkthrough (**WIN, 109/109 — full score**)

- **Game:** *The Sisters* by James Webb, 04 Dec 2006. You come round in a
  crashed car in the Sussex woods, remembering a little girl in a flowery dress
  running into the road. You follow her footprints to Chatfield Manor.
- **Engine:** **ADRIFT 4.00** (`xxd -l 16 games/TheSisters.taf` →
  `… c2 cf 93 45 3e 61 …`). 50 rooms, 123 tasks, 9 events, 46836 bytes.
- **Result:** **WIN**, ending on the epilogue's last line — *"Inside, with hands
  and feet bound and eyes staring vacantly upwards, lay the lifeless body of
  Trisha Seabourne."*
- **Score: 109 out of 109 — every point in the game**, in **151 commands**.
- **Harness row:**
  `thesisters_solution.txt|TheSisters.taf|lifeless body of Trisha Seabourne.|SCR_SKIP_WAITKEY=1`,
  PASSing golden.
- **Source:** `downloaded/TheSisters_walkthrough.txt` — a 10-section prose guide
  that explicitly sets out to reach 100%.

## The transcript proves its own completeness

Every task in this game has `score=0`. All 109 points are carried by
`ACT type=4` add-score actions instead, and a dump of the task table finds
**exactly 38** of them, summing to exactly 109. The golden contains **38**
*"(Your score has increased by N)"* lines. There is nothing left over: no
unreachable award, no ceiling to apologise for.

That is unusual for this corpus, and the credit belongs to the .txt — it is the
only guide here that is both accurate *and* aimed at 100%. Five things still had
to come out of the engine.

## The five gaps in the guide

### 1. Two nouns the parser doesn't answer to

| The guide says | The game wants |
| --- | --- |
| *"a tin of pickled herrings"* | the object is a **`can`** — `get tin` is *"Take what?"* |
| Tabitha *"pulls out a large metal key"* | the object is the **`iron key`** |

`get key` is worse than useless: it is caught by **task 34**, a catch-all
`exam key` task whose ten alt-commands include `get key`, `take key`, `use key`
and `drop key`, and which answers *"You need to be more specific about which key
you mean."* By the music room you are carrying a small key and a doorkey, so
every generic reference to a key lands there.

### 2. `row west` does not parse

The guide says *"Row west to the centre of the lake"*. There is no `row`
movement: the lake squares are ordinary compass rooms. `row east` **is** a task
— task 70, and it exists on exactly one square (39, the east side) to climb back
out onto the jetty.

`go fishing` is task 71, `where=1 room=43`: it scores on **one square only**,
the one where Tabitha and Mercy are standing on the water. Fish anywhere else
and task 72 catches it with *"although this lake hides a secret, this is not the
right location to fish."* The rod itself is `tie twine to hook` then
`tie twine to wood` — twine from the parchment, hook from the cleaned trophy,
wood from the collapsed library shelf, three items gathered floors and hours
apart.

### 3. Close the penknife, or die

The guide flags this in capitals and it is worth repeating, because the engine
implements it as a **pair of tasks with identical command lists**:

```
TASK 12  climb down / down / d / climb decline ...  RESTR penknife open=0  -> move player to room 11
TASK 13  climb down / down / d / climb decline ...  RESTR penknife open=1  -> ACT type=6 v1=2   (death)
```

You open the penknife in the first room of the game to cut the seatbelt and
there is no reason to think about it again for twenty turns.

### 4. The bleeding clock

`EVENT 0 [bleeding] starter=3 startTask=3 … time1=30 time2=30` — thirty turns
from `leave car` (task 2) to death, stopped by `bandage self`. The route spends
eight of them, which is comfortable, but the guide's invitation to wander west
for a first look at the lake is not free.

### 5. `SCR_SKIP_WAITKEY=1` is load-bearing

The collapse at the front door ends in a `[Press any key]`. Without the skip it
swallows the first command issued in the guest room and every command after it
lands one turn early; the run does not win and does not obviously look broken
either — it just quietly stops scoring.

## One authoring curiosity, and one that didn't bite

`open music box` is written **twice** — tasks 74 and 75, identical commands and
identical alt-commands, differing only in the diary restriction (74 wants the
diary in another state, 75 wants it held). Both move the iron key to the music
room and both are unscored, so unlike *Crime Adventure*'s shadowed pairs this
one costs nothing. It reads like the author covering the case where the player
never picked the diary up.

Worth noting for the endgame: task 75 also **removes the diary from your
inventory**, which is Tabitha taking it back. If you never read it, you never
get her parting line — *"I hope you did read my diary. I left a message in there
for you."*

## The route, and where the 109 points are

| # | Section | Awards | Score |
| --- | --- | --- | --- |
| 1 | Inside the car — glovebox, `x dashboard`, `cut seatbelt`, `open door` | 2+5+5 | 12 |
| 2 | The road — `follow footprints` | 5 | 17 |
| 3 | Lost in the woods — first-aid box, `bandage self` | 5 | 22 |
| 4 | To the manor — close the knife, `climb down`, `use knocker` | 5 | 27 |
| 5 | The guest room — sink, mirror, bedside table, desk, newspaper, coathanger→wire, paper under the door, `insert wire`, `pull paper` | 1+2+2+2+5+2+1+2+2 | 46 |
| 6 | The second floor — `put book on bookshelves` (the shelf collapses; take the wood) | 2 | 48 |
| 7 | The first floor — closet, cabinet, `wet flannel`, `read diary`, `clean trophy with flannel` (→ fishing hook), `put trophy in cabinet`, `throw urn over bannister` | 2+1+5+2+3+2+5 | 68 |
| 8 | The ground floor — `take parchment` (→ twine), `open door`, socks & shoes, `open can with tin opener`, `pour oil on mousetrap`, `set trap`, `put cheese in trap`, `put trap on floor`, `put hand in hole` (→ padlock key) | 2+1+1+2+2+2+1+1+3+2 | 85 |
| 9 | The lake — `unlock padlock`, `go boat`, build the rod, `go fishing` (→ music box) | 3+1+5 | 94 |
| 10 | The cellar — `x body` | 5 | 99 |
| — | The guest room again — `smash window` | 10 | **109** |

The urn has to be **thrown over the bannister** to break it open — nothing else
works on it, and the parchment inside is both the story's key document and the
twine for the fishing line. The mousetrap chain is the game's one real puzzle
and the guide gets every step of it right.

The ending prints no score line, so the route runs `score` on the turn before
`smash window`; the golden records **99/109** there and the last 10 arrive with
the win.

## Reproducing

```sh
cd terps/scarier/test/adrift4/harness
sh run_v4_walkthroughs.sh thesisters     # PASS against the committed golden
```
