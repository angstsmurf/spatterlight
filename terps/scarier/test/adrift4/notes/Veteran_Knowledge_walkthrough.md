# Veteran Knowledge — walkthrough

- **Engine:** ADRIFT 4.00 (`vetknow.taf`, 52,248 bytes) by **Robert Street**
  (credited as "Robert Rafgon" in this release). **43 rooms, 359 tasks, 83
  objects, 15 NPCs, 38 events, 2 variables.**
- **Result:** ★ **WON, 50/50** — and 50 is provably the ceiling, not an
  assumption: the file has exactly eight `ACT type=4` awards,
  2 + 8 + 10 + 4×3 + 8 + 10 = 50, and the route fires all eight.
- **Solution:** `goldens/vetknow_solution.txt`, 120 commands, golden blessed,
  row in `run_v4_walkthroughs.sh` with `SCR_SKIP_WAITKEY=1`. Win marker
  `AND THE NEW WORLD CHAMPION IS` — **WINTEXT is empty**, so the marker has to
  be prose out of the winning task's own text.
- **Second file:** `vetknow2.taf` (52,290 bytes) is the same game again and is
  wired as its own row, `goldens/vetknow2_solution.txt`. See *Two releases*
  below.
- **Provenance:** no published walkthrough — David Welbourn covered the
  *earlier* game only. Derived from the game's **own hint system**, which
  `SCR_DUMP_TASKS` prints, plus `SCR_DUMP_OBJLOC` and play.

The full-length rewrite of *Veteran Experience* (`veteran.taf`, 12,043 bytes,
4th 1-Hour Comp), already wired as `veteran_solution.txt` — see
`notes/Veteran_Experience_walkthrough.md`. Same author, same premise, same
finish: a washed-up wrestler crowbars his way back to the World title by
putting every other contender in hospital first. Where the one-hour game is
just the arena, this one puts a whole town in front of it and turns the arena
itself into three floors.

## The hint system is the walkthrough

This is the reusable thing here. The game has a `hint` command that gives
**location-specific** hints, and ADRIFT stores those as per-task fields —
which `SCR_DUMP_TASKS` prints as `HINTQ=` / `HINT1=` / `HINT2=`:

```
TASK 122 where=1 room=27 ... cmd=[give*teddy*monster]
    HINTQ=[The Monster's locker room 2]
    HINT1=[You firstly need to distract the Monster. You may have noticed that
           he does not like cute, friendly objects]
    HINT2=[You need to GIVE TEDDY TO MONSTER]
```

`HINT2` is almost always the literal command in capitals. Every load-bearing
step of the route below is quoted from one of these, and the endgame hint is a
complete sentence-by-sentence walkthrough of the last seven moves. **Grep the
dump for `HINT2=` before deriving anything, on any game whose author built a
hint menu** — it is faster and more reliable than play.

Typing `hint` in the game works too, but it prompts `[Y/N]` *and echoes its
own prompt twice*, so it is useless inside a strict-diffed golden. Read it out
of the dump instead.

## Route

Twelve stages, 120 commands.

| # | Where | What | Score |
|---|---|---|---|
| 1 | Outside an alley | wait three turns for the flyer, read it | |
| 2 | The alley | garbage can → **crowbar** | |
| 3 | The store | steal **black paint** and **sleeping pills** | |
| 4 | The bar | `put pills in beer` (the Brawler's) | |
| 5 | South end of park | `swim` | **+2** |
| 6 | North end of park | take the **can of oil** off the brats | |
| 7 | High Flyer's backyard | `open gate with crowbar`, push the trampoline, `get trunks` | |
| 8 | Back entrance | garbage can → **paintbrush**; `paint trunks`, `wear trunks` | **+8** |
| 9 | Outside the car park | `pour oil on driveway` | **+10** |
| 10 | Employment agency | `talk to promoter` → **back door key** | |
| 11 | Arena, bottom floor | crate → mugging → hairpin → guard → firecracker | **+4** |
| 12 | Arena, top floor + ring | Monster, High Flyer, Youth, the Star | **+4/+4/+8/+10** |

The eight scoring tasks, in the order the route fires them:

```
T46  swim                room 17  +2   wash off the alley
T49  paint *trunks       anywhere +8   repaint the High Flyer's white trunks
T63  pour *oil *driveway room 5   +10  writes the Brawler out of the show
T111 light*firecracker   room 29  +4   scares the Evil Twins off your crowbar
T124 hit*monster*crowbar room 27  +4   and drops the bottle of acid
T135 get *mask*          room 39  +4   unmasks the High Flyer
T138 throw *acid*youth   room 26  +8   and starts the title match
T260 attack*star*crowbar room 42  +10  the file's only ACT type=6
```

## The four timing traps

Each of these cost a replay.

**1. The flyer lands at the end of turn 3.** `EVENT 1 [Flyer arrives]` is
`start=4..4`, so `take flyer` on turn 1 is "You see no such thing". The three
`z`s at the top of the solution are not padding — they are the wait.

**2. The park is empty until the beer is drugged.** The north end of the park
(room 18) starts with nobody in it; the brats are `startRoom=-1`. `talk to
brats` there does nothing and no amount of waiting helps. What moves NPCs 4
and 5 into it is **T57 `east` out of the bar**, gated on T54 `put pills in
beer` being complete. So the pills have to be stolen from the store and
dropped in the Brawler's beer *before* the walk to the park, which is why the
route crosses town twice and why the pour-oil payoff comes so much later than
the drugging.

**3. `look under ring` is two different tasks and the order is forced.**

```
T126  look under*ring  room 41  RESTR type=2 v1=139 v2=1  (T138 NOT complete)
      -> bag of tacks, ladder
T127  look under*ring  room 41  RESTR type=2 v1=139 v2=0  (T138 complete)
      -> steel chair, crowbar, fire extinguisher
```

So ringside **before** the acid for the High Flyer's props, and ringside again
**after** it for the crowbar. Get it backwards and the High Flyer is
unbeatable, because there is no other source of the ladder or the tacks.

**4. The title match is on a clock.** `throw acid at youth` teleports you into
the ring with *empty hands* and starts a fan of events: `star attack 1..8` at
turns 3–4, 7–8, 11–12 … 29–30, and `star gets out chain` at 36 (T222, which
hands NPC 14 a steel chain). `spray star` sets VAR 0 `blinded`; `EVENT 14..20
[unblind star]` clear it again **four turns later**; and T260 carries
`RESTR type=4 v1=2 v2=2 v3=1` — variable 0 `== 1`. So the crowbar has to swing
on the very next turn. The seven-command finish lands the win on match turn 7.

The author says it plainest, in the Ring's own hint: *"You need to go outside,
look under the ring, get the fire extinguisher and crowbar, go back inside,
spray Star, and hit Star with crowbar."*

## The arena, which is where the game actually starts

Walking in through the back door triggers **T79 `east`** — a white crate falls
and jams the stairway door. **T86 `move crate with crowbar`** levers it clear,
and then the Evil Twins jump you, steal the crowbar and dump you unconscious
in the *Mysterious room* (`ACT type=1 v1=0 v3=28`). That reads like a losing
state and is not — it is the intended way onto the bottom floor:

```
touch east wall   T95 -- a hairpin, found in the dark.  "TOUCH EAST WALL then
                  head WEST to escape"
west              picks the closet lock with the hairpin; you come out in the
                  lower south corridor (29), with the Twins playing cards in
                  the storeroom to the west, using your crowbar as a trophy
talk to guard     T104 -- "He is actually very willing to leave if you TALK TO
                  GUARD".  He goes to the car park for a smoke and takes the
                  red cigarette lighter with him; the fireworks storeroom to
                  the south is now unguarded
pick lock         T106, with the hairpin -- one firecracker
n / e / e        THE LIGHTER.  Stepping back into the north corridor makes the
                  guard bolt for his post, and he DROPS THE LIGHTER in the
                  north end of the car park on the way
light firecracker T111 in room 29, +4 -- the Twins run upstairs and leave the
                  crowbar in the empty storeroom (33) to the west
```

The lighter is the fiddly one: there is **not one task in rooms 36/37**, and
`take lighter` while the guard still has it is "Take what?". Chasing him out
and back is the only route to it.

## Two smaller footguns

**The Monster's room is dark and holding the torch is not enough.** The
flashlight is in the locker in your own locker room (23), and `take
flashlight` before `open locker` is "Take what?" — the locker is a closed
container and the object is not in scope. It then has to be switched on:
`turn on flashlight` fires T67 and, conveniently, also carries the pending
`east`, so the transcript reads *"You move east and turn on your
flashlight."* T99, T116/117, T122, T123/124 and T70 (leaving west) all carry
`RESTR type=0 v1=13 v2=1` — flashlight *held* — on top of their own gates.

**`wear mask` before `throw acid at youth`.** T137 and T138 are the same
command with the mask restriction inverted: T137 (the failure) wants
`v2=8`, T138 (the +8) wants `v2=2 v3=0` — worn *by the player*. Simply
carrying it is not enough. The author's hint: *"THROW ACID AT YOUTH"* but
*"WEAR MASK first"*.

## The game cannot be lost

There is exactly **one `ACT type=6` in the whole file** — T260's win. The Star
pummelling you for eight scheduled attacks and then producing a steel chain is
all texture; nothing in the file ends the game badly. Neither does the
piledriver the Monster gives you in the park, which is the first `<waitkey>`.
Three `(Press a key)` pauses in total (the piledriver, the Promoter's
street-fight offer, and the celebration), which is why the row carries
`SCR_SKIP_WAITKEY=1`. No RNG is consulted on this route, so no `SCR_SEED`.

## Two releases

`vetknow2.taf` (52,290 bytes) is the same game. A zlib-decompress (offset 22)
plus `strings` diff of the two files finds **exactly three changed strings**:

- the author byte-field, `Robert Rafgon` → `Robert Street`
- one added sentence in the ABOUT text: *"Version 2.1 on 11 December 2005
  updates the author name from Robert Rafgon to Robert Street."*
- the build date, `07 May 2005` → `11 Dec 2005`

Not one room, task, object, NPC or event differs, and the same 120 commands
produce a **byte-identical transcript**. Both are wired, with
`vetknow2_solution.expected.txt` identical in content to
`vetknow_solution.expected.txt`; that identity is the reason for carrying the
second row at all. If those two goldens ever diverge, something in the engine
is reading the header when it should not be.
