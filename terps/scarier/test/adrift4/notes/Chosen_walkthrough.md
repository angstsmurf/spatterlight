# Chosen — walkthrough (**WON 300/300**, 51 commands)

- **Author:** Ryan J. Bury — the title page gives *"a 15-room adventure for the
  ADRIFT MiniComp 2001"*. You are Captain Johnson, lifted out of your bed and
  dropped into an underground complex you have to work your way out of.
- **Engine:** **ADRIFT 3.90**
  (`xxd -p -l 12 games/Chosen.taf | cut -c17-22` → `944537`).
- **Result:** **WON, 300/300** — `score` in the door room reports
  *"out of a maximum of 300"*, and the route fires all sixteen `ACT type=4`.
- **Source:** `downloaded/Chosen_walkthrough.txt`, the author's own eight-line
  note. It is **prose hints, not a command list** — *"Explore all the rooms",
  "You'll soon work out how they work", "I won't bother explaining the obvious
  puzzles"* — so the route below is derived from the game and then run.
- Row: `chosen_solution.txt|Chosen.taf|You plug the T-shaped block into the final socket in the door.|SCR_SKIP_WAITKEY=1`.

## What the complex is

Six metal blocks, lettered, and a door with six sockets. They spell **ADRIFT**,
and the people on the other side of it are the Drifters — an elite squad that
has been quietly winning other people's battles since the forties, and the
whole complex is their entrance exam. The last line of the game is *"Allow me
to introduce myself. The name's Wild. Campbell Wild."*, which is the author
handing the joke to ADRIFT's own author.

## The map

15 rooms. The two glowing alcoves are teleporters, and they are what makes the
map an awkward shape rather than a tree.

```
  0 dark room  --N-->  1 corridor  --W-->  3 darkened room (the metal box)
                       1  --E-->  5 ice room (the wall box)
  attach belt to cable, in room 1  ->  2 alcove
  2  --D-->  4 THE DOOR ROOM (six sockets)
     TASK 2  `west` in rooms {4,13}  ->  room 0
     TASK 3  `east` in room 4        ->  room 6
  6 round room --N--> 7 cage   --W--> 8 lever   --E--> 9 corridor
  9 --N--> 11 supply room   --S--> 10 shooting gallery   --E--> 12 edge of pit
  12 --E--> 13 alcove above the spikes    (only after TASK 13)
  13 --D--> 14 the pillar room
```

The belt-and-cable descent is repeatable (`rep=1`), which is what lets the
route come back for the last two blocks and then drop into the door room a
second time.

## The three things that kill you or dead-end you

**`pull lever` in room 8 before room 7.** Room 7 has the caged tiger and the
A-block. The winning task is

```
TASK 7  where=1 room=7  cmd=[pull*lever*]
    RESTR type=2 v1=9 v2=0   task8=[pull*lever*]     # TASK 8 must be done
    ACT type=0 -> A-shaped metal block into the room
    ACT type=4 v1=30
TASK 9  where=1 room=7  cmd=[pull*lever*]            # no restrictions at all
    ACT type=6 v1=2                                  # death
```

Same command, same room, and TASK 9 is unguarded — so it is purely TASK 7's
restriction that decides which one you get. The lever in room 8 answers *"You
pull the lever, but nothing interesting seems to happen."* That **is** the
success response; it is electrocuting the tiger offstage.

**Any form of `take block` in room 14.** The R-block sits on a pillar covered
by a gun:

```
TASK 14  where=1 room=14  cmd=[take*block*pillar*]
    ALTCMD[1]=[get*block*]   ALTCMD[2]=[take*block*]
    ACT type=6 v1=2
```

Every reasonable phrasing is trapped. The block comes off via `tie string to
block` (TASK 15, +15) and then `up` (TASK 16, +15) — and TASK 16 both puts the
R-block in your hand and moves you back to room 13, so you never stand in the
pillar room holding it.

**The block order.** TASK 18 through TASK 22 each carry a `RESTR type=2` on the
previous one, so the sockets open one at a time and the door has to be spelled
out: **A, D, R, I, F, T**. TASK 22 is the win (`ACT type=6 v1=0`) and carries
100 of the 300 points.

## Two smaller repairs the route needs

* **`take belt` first.** The belt starts *inside the trousers*
  (`parent=1 pos=-20`), and TASK 0's `RESTR type=0 v2=1` wants it held, so
  `attach belt to cable` on turn one answers "You do not have your belt."
* **Full block names only.** `take block`, `get a block` and
  `take a-shaped block` all get "Take what?". It has to be
  `take a-shaped metal block`.

## A decoy

`break ice` (TASK 6, ALTCMD `smash ice`) carries `RESTR type=2 v1=6 v2=1` —
TASK 5 must **not** be done. So it is only available before you solve the room,
and the author's note says outright what the room actually wants: *"To melt the
ice, you'll need to find a battery to plug into the box on the wall."* That is
TASK 5, +20, and it produces the T-block.

The other death not on the route is TASK 23, `[jump*off*]` in rooms
{1, 2, 12, 13} — every ledge in the game is real.

## Where the three hundred points are

| task | command | points |
| --- | --- | --- |
| TASK 0 | `attach belt to cable` | 20 |
| TASK 7 | `pull lever` (room 7) | 30 |
| TASK 11 | `reload gun` (bullets) | 10 |
| TASK 10 | `shoot targets` | 10 |
| TASK 12 | `reload gun` (harpoon) | 10 |
| TASK 13 | `fire gun at target` | 10 |
| TASK 15 | `tie string to block` | 15 |
| TASK 16 | `up` (holding the string) | 15 |
| TASK 1 | `unlock box` | 10 |
| TASK 5 | `plug battery into box` | 20 |
| TASK 17–21 | `plug A/D/R/I/F block` | 10 each |
| TASK 22 | `plug T block` | 100 — and the win |

`SCR_SKIP_WAITKEY=1` is required for the row: the ending is a long scene that
pauses.

## Shape of it

`goldens/chosen_solution.txt`, 51 lines:

```
n / take belt / attach belt to cable          the descent -> alcove
take key / d                                  the door room
e / w / pull lever                            room 8: electrocute the tiger
e / n / pull lever                            room 7: the cage, +30
take a-shaped metal block                     A
s / take battery
e / n / take bullets / take harpoon
s / s / reload gun / shoot targets            the gallery
take f-shaped metal block                     F
n / e / reload gun / fire gun at target       the harpoon opens 12 <-> 13
e / take i-shaped metal block / take string   I
d / tie string to block / up                  R, without standing under the gun
w                                             teleport back to room 0
n / w / unlock box / take d-shaped metal block          D
e / e / plug battery into box                 melts the ice
take t-shaped metal block                     T
w / attach belt to cable / d                  back to the door
plug a/d/r/i/f/t block                        -- WIN, 300/300
```
