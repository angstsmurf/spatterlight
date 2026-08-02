# Sommeril — walkthrough

- **Engine:** ADRIFT 4 (`sommeril.taf`, H. Lee Parten, InsideADRIFT Spring Comp
  2004). A surrealist dream-village — you fall out of the sky clutching a book,
  the pages blow away, and the village below is a hallucination stitched out of
  the author's open-heart surgery.
- **Result:** ★ **WON, 85/100 — and 85 is the ceiling.** The remaining 10 points
  are behind a restriction that can never hold; see *Bugs* below.
- **Solution:** `harness/sommeril_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `www.angelfire.com/games5/sommeril`.

## Map

Three parallel north–south streets, each with a Junction / Lower / Middle /
Upper tier, joined at the top by the Park and at the bottom by the
Mist-shrouded junction where you land. Everything indoors is `in` / `out`.

```
                 Inside confessional (26)
                        ^ in
        Citadel interior (24) --w--> Outside confessional (28)
                 ^ in
          Citadel foyer (16)          [in from Citadel stairs, needs the key]
                 ^ in
          Citadel stairs (15)
                 ^ n
Tavern (21) <-in-  Park (14)  -in-> Ye Olde Curiosities & Toys (19)
              sw /  |  \ se
   Upper Nemo (10)  |   Upper Romarico (13)
        |      Upper Faust (4)          |
   Middle Nemo (6)  |   Middle Romarico (12)   [wheel in the gutter]
    in-> Tailor(20) Middle Faust (8) -in-> Village Smythe (18) -n-> Stall (23)
        |           |                      |
   Lower Nemo (7)   Lower Faust (9) -w-> Cafe (17)
        |           |                     Lower Romarico (11) -in-> Town Crier (22)
   Nemo Jct (3)  Faust Jct (5)            Romarico Jct (2)
        \            |                    /
           Mist-shrouded junction (0)  --s--> The Mist (1) --s--> death
```

## Scoring

Sixteen tasks award points; fifteen are reachable, for **85/100**.

| pts | where | what |
|---|---|---|
| 5 | Cafe | `get placemat` — the placemat is one of your pages |
| 5 | Upper Faust | `take silver orb` from the skeleton statue |
| 5 | Upper Faust | `read wet page` (after the fish surfaces it) |
| 5 | Upper Nemo | `give silver orb to gargoyle` — it flies home |
| 5 | Nemo Jct | `read crumpled page` |
| 5 | Romarico Jct | `read muddy page` |
| 5 | Toy shop | `take glass framed page` (only after the gargoyle is freed) |
| 5 | Toy shop | `read bloody page` — it is sealed *inside* the framed page |
| 5 | Citadel stairs | `take ring` — but see *Bugs* |
| 5 | Citadel stairs | `take silver key` — but see *Bugs* |
| 5 | Village Smythe | `give wheel to blacksmith` |
| 5 | Stall | `take rat-chewed page` |
| 5 | Stall | `read rat-chewed page` |
| 10 | Citadel stairs | `unlock door` |
| 10 | Inside confessional | `take mask` — the endgame |
| ~~10~~ | Upper Faust | ~~`take wet page`~~ — **unreachable**, see *Bugs* |

The author's own walkthrough (`springcomp04.zip →
walkthru/walkthru/wt-sommeril.txt`) is only the six-step spine — dust, ring,
bride, key, unlock, mask — and scores **20**. Everything else here came out of
`SCR_DUMP_TASKS` / `SCR_DUMP_OBJLOC`.

## Route

Four blank lines first: the intro has four `<waitkey>` pauses.

```
(4 blank lines)
get book cover
n
n
take fish
w
x placemat
get placemat
e
n
n
take silver orb
put fish in fountain
take wet page
read wet page
n
sw
give silver orb to gargoyle
s
s
s
get crumpled page
read crumpled page
se
ne
get muddy page
read muddy page
n
n
x gutter
get wheel
n
in
ask about glass framed page
take glass framed page
open glass framed page
get bloody page
read bloody page
out
nw
n
x pile of dust
take ring
drop ring
take ring
take silver key
s
se
s
s
in
give ring to bride
take silver key
out
n
n
nw
s
s
in
give wheel to blacksmith
n
take rat-chewed page
read rat-chewed page
s
out
n
n
n
unlock door
in
w
in
score
take mask
```

## Bugs (all reproduced faithfully — SCARE is not at fault)

- **`take wet page` (task 6, the biggest single award at 10) can never fire.**
  Its restriction is *WET PAGE held by NPC 0, the FISH* (`type=0 var1=6 var2=1
  var3=2` → `var3 - 2 = 0`), but the task that spawns the page — `put fish in
  fountain` — drops it *inside the fountain container*, and handing it over
  yourself does not help: `give wet page to fish` answers "FISH examines the WET
  PAGE with little interest, then returns it to you." Reading the page still
  pays its 5. **So the real maximum is 85, not the 100 the status line claims.**
- **`take ring` (task 11) restricts on the ring being loose in the room**
  (`var2=0 var3=16` → position 16 = Citadel stairs), but the ring starts *inside
  the pile of dust*. The first take is therefore the plain library take and
  scores nothing. Hence `take ring` / `drop ring` / `take ring` in the route:
  once the ring is on the floor the task fires and pays its 5.
- **`take silver key` (task 5) is scoped to the Citadel stairs**, not to the
  Town Crier where the key actually turns up — so it hands you a key, and 5
  points, before you have ever met the bride, and prints a stray `Take what?`
  afterwards because the engine still runs the library take on top. It is
  possible to skip the bride entirely: ring, key, unlock, done. The route keeps
  the bride anyway (it is the best scene in the game) — but note that **the
  bride task moves that same key object to the Town Crier**, so it has to be
  picked up a second time there or the Citadel doors stay locked. That is the
  one non-obvious failure in this walkthrough.

## Notes

- `ask tailor about coffins` teleports you into a coffin (room 25), and *any*
  `look` in there ends the game. `s` twice from the Mist-shrouded junction does
  the same via The Mist. Both are avoided.
- The confessional ending: taking the mask *is* the endgame (`ACT type=6 v1=0`),
  so `out` to "Your Fate" (room 27) is never reached and nothing after the take
  gets to the parser. The `score` immediately before it records **75**; the mask
  adds the last 10.
- The pages are the author's account of his own heart-valve replacement; the
  village is the dream he had under it. The `score` line in the golden is the
  only in-transcript proof of the 85.
