# CloD Demo — walkthrough (**best reachable state**)

- **Engine:** ADRIFT. "Clod's Quest: The Dungeons of Zivulda [demo version
  1.0]" by Duncan Bowsman — a comedic fantasy dungeon crawl, and a
  **menu-driven demo**, not a free-text parser: every screen shows one
  highlighted option; `\` (or `'`) cycles it forward, `1` chooses it, `2`
  opens/closes the inventory. There is no published walkthrough anywhere
  (checked IF Archive/CASA) and this is a promotional demo, so there is no
  guarantee a full win exists within the shipped content.
- **Result:** best-reachable checkpoint, not a win. The route eats the
  sandwich and drinks the strength potion (both one-shot inventory items),
  lifts the portcullis, enters the dungeon, survives the Entryway Tunnel's
  boot-wipe puzzle, and passes safely into the Pedestal Room — the game's
  second area. Wired as `cloddemo_solution.txt|clod_demo.taf||`, no env, no
  win marker.
- **The one genuine trap found:** the Entryway Tunnel's "Pass through the
  ornate archway" option is an **unconditional, deterministic death**
  ("Track mud into MY dungeon, will you?" / magic bolt / "*** YOU HAVE
  DIED! ***", losing a life and respawning in the same room) *unless*
  "Wipe boots on the indoor entry mat" — an easy-to-miss option in the same
  menu cycle — is selected first, in the **same visit**. Leaving and
  re-entering the dungeon re-dirties the boots ("Leave through the
  entrance" always steps in mud again), so the wipe must happen
  immediately before the archway is tried. Two other options in that room
  ("Remove the exit sign", "Clean out the worm-infested skull") are pure
  refusal red herrings.
- **Getting to the archway at all** requires lifting the iron portcullis at
  the Dungeon Entrance, which only succeeds while under the effect of the
  one-shot POTION OF STRENGTH (drunk via the inventory); eating the CHEESE
  & PICKLE SANDWICH beforehand is not required for this but was done along
  the way since it's the same "consume before it matters" pattern.
- **Left unsolved, for a future attempt:** the Pedestal Room's puzzle.
  Equipping the MAGNIFYING GLASS (inventory → equip) and examining objects
  reveals: the pedestal carries a poem — "FIND YE THE GEMSTONES / TO
  REFLECT THE LIGHT. / PLACE THEM CORRECTLY / BUT D..." with its last line
  illegible — confirming a find-4-gemstones/place-in-4-sockets puzzle; and,
  back at the Dungeon Entrance, the dragon-head statue has "a tarnished key
  stuck in the dragon's teeth" (neither bare hands nor the sword can
  retrieve it). The dead bush at the Dungeon Entrance bites back if reached
  into bare-handed (shrub lizards) but can be safely "sliced" with the
  sword equipped; whether anything is left to collect afterward wasn't
  confirmed. In the Pedestal Room itself, the door is locked and the stone
  slab won't budge — both presumably wait on the gemstone puzzle and/or the
  statue's key. None of this was solved within this investigation's scope;
  it's recorded here as a lead rather than chased further, per the
  Sandy/Penrhyn/TheWill precedent for an honest best-reachable checkpoint.
- **Content note:** no minors appear in any text reached (cast: Clod, a
  disembodied trap-voice; backstory references to a king, wizards, and the
  sorceress Zivulda, all adults).

## The walkthrough

```
\
1

\
1
2
\
\
\
1

\
\
1

2
1

1

\
1

\
1

\
\
1

\
\
\
1

\
1

\
\
\
\
\
\
1
```
