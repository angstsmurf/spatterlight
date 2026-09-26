# CloD Demo -- walkthrough (**full win, 140/140**)

- **Engine:** ADRIFT 4.00. "Clod's Quest: The Dungeons of Zivulda [demo
  version 1.0]" by Duncan Bowsman -- a comedic fantasy dungeon crawl, and a
  **menu-driven demo**, not a free-text parser: every screen shows one
  highlighted option; `\` (or `'`) cycles it forward, `1` chooses it, `2`
  opens/closes the inventory, where `1` uses or equips (and unequips) the
  highlighted item. The highlighted option is kept after a choice, and the
  inventory cursor always opens on SWORD. No published walkthrough exists.
- **Result:** **FULL WIN, 140/140.** The ending is task 741 (#slab_resume,
  Dimly-lit Landing), an EndGame-win action run by task 742 ("Enter the dark
  passage"), which needs the stone slab gone: task 923 (#E2R4) -- emerald in
  the second socket, ruby in the fourth. The other three emerald/ruby layouts
  (tasks 917-922) are fatal. All twelve #score tasks (1223-1234) are
  collected. Wired as
  `cloddemo_solution.txt|clod_demo.taf|You scored 140 out of the maximum 140!|`,
  no env; no RNG is consulted.
- **Dungeon Entrance:** eat the sandwich first (the potion of strength
  refuses an empty stomach). Equip the sword and slice the dead bush
  (reaching in bare-handed gets you bitten by shrub lizards): the lizards
  flee and a stick falls. Equip the stick and poke the dragon's-head statue
  to drop the tarnished key. Drink the potion, lift the portcullis, go in.
- **Entryway Tunnel:** poke the worm-infested skull with the stick for the
  emerald. The ornate archway is an **unconditional death** ("Track mud into
  MY dungeon, will you?" / "*** YOU HAVE DIED! ***") unless "Wipe boots on
  the indoor entry mat" is chosen first in the **same visit** -- leaving
  through the entrance re-dirties the boots.
- **Pedestal Room / Privy:** the key unlocks the heavy wooden door (and is
  discarded). In the Privy, the magnifying glass on the chamberpot shows the
  ruby; the long-glove from the shelf reaches in for it (the glove is
  discarded). Back in the Pedestal Room, emerald into socket 2 and ruby into
  socket 4: the slab vanishes. Two blank lines then answer the slab text's
  own press-enter prompt, and "Enter the dark passage" ends the demo.
- **Runner check:** run400x (runner_transcripts/cloddemo.txt) wins 140/140
  on the same route. The solution's first line is blank so that it, not `\`,
  answers the intro `<waitkey>`; the Wine feed then starts in step. One turn
  differs: the ruby turn's slab text has a real-time `<wait>` between two
  press-enter pauses, the driver answered one pause itself, and the spare
  blank line reached the game as an empty command ("Please type appropriate
  control symbol"). That is a driver timing artefact, not an engine difference.
- **Content note:** no minors appear in any text reached (cast: Clod, a
  disembodied trap-voice; backstory references to a king, wizards, and the
  sorceress Zivulda, all adults).

## The walkthrough

Comment lines from `goldens/cloddemo_solution.txt` are dropped; blank lines
are significant (they answer press-enter prompts; the first answers the
intro `<waitkey>`).

```

1

\
1
2
\
\
\
1

1
2
\
\
1

2
1
2
1

2
\
\
\
1
2
\
\
1

2
\
\
\
1
2
1

2
\
\
1

2
\
\
1

1


2
\
\
1
2
\
\
1

2
\
\
1
2
1

\
\
1

\
\
\
\
1

2
\
\
\
1
2
\
\
\
\
\
\
\
1

1

1

2
\
1
2
\
1

2
\
1
2
\
\
1

2
\
\
\
\
1
2
1

\
1

2
\
\
\
1
2
\
\
\
\
\
1

2
\
\
\
1
2
\
\
1


\
\
\
1

```
