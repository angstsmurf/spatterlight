# Alan 2 walkthroughs and reference transcripts

Four Michael Zerbo games, all shareware/demo builds, all of them media games that
drive `VIEWER.EXE` and `SBPLAY.EXE` through the A-machine's `SYSTEM` instruction
(see `../../TODO_media.md`). They are the regression corpus for that work: every
one of them draws pictures, and three of them print a room description, show a
picture, and then print the description again — the case the duplicate filter in
`glkmedia.c` exists for.

Each game has two files:

| file | contents |
| --- | --- |
| `<game>_walkthrough.txt` | the commands, one per line, ready to pipe into the interpreter |
| `<game>_expected.txt` | the transcript that walkthrough produces under CheapGlk |

Every walkthrough runs from the first move to the game's shareware wall with **no
refusals at all** — no "You can't do that.", no "I can't see any …", no "Huh?".
A diff against the expected transcript that shows one is a real regression.

## Regenerating

The transcripts come from the headless CheapGlk build of this interpreter
(`-DGLK -DREVERSED -DSPATTERLIGHT`, `-Itest` for `test/glkimp.h` and
`test/headless_stubs.c`, linked against `../../cheapglk/libcheapglk.a`). Run each
one from the game's own directory, since the media lookup is relative to the
`.acd`:

```sh
cd <game directory>
{ cat <game>_walkthrough.txt; printf 'quit\ny\n'; } | alan2 <game>.acd 2>/dev/null \
    | grep -v '^Welcome to the Cheap Glk'
```

CheapGlk fails the `gestalt_Graphics` check, so pictures never draw and the
duplicate-description filter never engages. That is why the transcripts contain
each description twice: the acode idiom is *describe, pause, viewer, describe
again*, and with no picture in between both copies simply print. In the app the
picture lands between them and the second copy is dropped.

## The games

### The Child Murderer (`Cm.acd`, Laser Point 1996, Alan 2.6)

`~/Downloads/The-Child-Murderer_DOS_EN_Shareware/Cm.acd`. Menu-driven parser:
`G`et, `T`alk to, e`X`amine, `Us`e, `L`ook, `R`estore, sa`V`e, `Q`uit, plus the
six compass letters. No score.

22 moves. Act 1 is the Harris mansion: Mr. Harris sends you to fetch his wife,
Mrs. Harris can't get Alice indoors, and the strawberries from the Entrance buy
the child's co-operation. Two things are easy to get wrong — you must **enter
the Parlor first**, or `t mrs harris` only produces small talk about the
weather; and `x desk` in the Den is what gives you the candle (the game hands it
over as a piece of scenery business, there is nothing to `g`et). Act 2 opens
with Alice dead on your bedroom floor: `g alice` shoulders the body, `s` sneaks
you out of town to the Coast, and `us candle` on the Beach both lights the cave
and walks you into it, where `us alice` hides her.

**The wall** is the hansom cab: `n` from the Coast prints the registration
notice and drops you in `Carriage`, a room with no exits at all (all six probed).
Everything the string table promises past that point — the church, the tavern
graverobbers who pay a gold sovereign for the location of a body, the Booth
prostitute, Joe the butcher who grinds the body into sausages, Susan, Devlin's
hideout, and the winning ending where the sovereign buys passage from the
seaport captain — is registered-version content and unreachable here. Going `n`
from the Coast *without* hiding the body first reaches `End_demo` instead, a
loss.

### Inner Demons (`Demons.acd`, Laser Point 1996, Alan 2.6)

`~/Downloads/Bloody-Murder_DOS_EN_Shareware/INNER/Demons.acd`. The same
menu parser, plus `H`it and `I`nventory. No score. (The sibling `CHILD/`
directory in that download is a copy of The Child Murderer above.)

28 moves. `us dresser` opens the drawer, `x clothes` turns up the key. Upstairs,
`us rope` in Hallway3 lowers the attic stairs; the cord is not visible until
`x boxes` reveals it. `us key` unlocks the west-wing door, `x crib` shows you
what is in it, and `us cord` strangles the creature — which teleports you to the
bathroom to be sick, and *that* is what puts the rat behind the toilet. Cleaning
the attic window with the rags and looking through it, and the portrait vision in
Hallway1, are optional but they are two of the game's set-piece pictures.

**The wall** is `us rat` in the Diningroom: dropping the rat to distract the
feeding creatures prints the registration notice and ends the game on the spot.
The bones it would have freed up, the demonic dog they feed, the basement, the
wine-and-corkscrew hallucination sequence and the broken-bottle ending are all
past it. The corkscrew itself is reachable (`x cabinets`, `x utensils`) and is
picked up in the walkthrough for completeness, but there is no wine to open.

### The Hollywood Murders (`Hm.acd`, Laser Point 1996, Alan 2.6)

`~/Downloads/The-Hollywood-Murders_DOS_EN_Shareware/Hm.acd`. A conventional
Alan parser, and the main reference game for the media work. No score.

30 moves. The chain is: the business card is in the waste basket, you can't read
it without the reading glasses, they are in a locked desk drawer, and the key
comes from **`ask julie for key`** in the outer office. `call thea` brings the
client in with the photo, the documents and the bracelet. Julie identifies the
film from the photo, the pawnbroker translates the German documents and gives ten
dollars for the bracelet, and the ten dollars buys the film magazine at Harold's
newsstand that names the studio. `take gun` is mandatory — the game will not let
you leave the building without it.

**The wall** is `drive car`: you arrive at M-G-M and the registration notice
follows immediately. The whole Hollywood half — the studio guard bribe, the
Haswell estate, the flashlight, the oiled window, the sleeping pills in the
chicken, the dog, the bodies on the couch, and the fight at the empty swimming
pool that ends the case — is registered-version content.

Useful media sites in this game, unrelated to the walkthrough: startup runs
`viewer title.pcx` → `sbplay gun/lady` → `viewer share.pcx`; `turn on fan` plays
`switch.wav`; `turn on radio` plays `suspense.wav`; asking Harold about anything
plays `nuthin.wav`, which is a misnamed text file and gets rejected.

### A Matter of Time (`TIME1.ACD`, Michael Zerbo 1995, Alan **2.5**)

`~/Downloads/Alan 2.6/AMattero/TIME1.ACD`. Conventional parser, no score. The
second media reference game, and the one that uses the switch-before-name form
`viewer -f3 title.pcx`. Every room shows a picture and repeats its whole
description afterwards, so simply walking around it exercises the filter harder
than anything else here.

27 moves, and this one is a **maximal** route rather than a route to a wall: the
shareware build is unwinnable by design. `x machine` is mandatory — the greasy
substance does not exist as an object until the time machine is examined — and
bare `take substance` is refused, it must be `take substance with leaf`, the leaf
being up the tree in the Swamp. The iron box is rusted shut until oiled, and the
rusted metal must be washed in the river before it will reflect anything.

**The wall** is a loop, not a missing object. Crossing the river needs the raft,
and launching it drowns you unless you already know where the Professor is; the
only thing that sets that is signalling him with the polished metal from the
Hilltop; and signalling teleports you straight into `End_demo`, a room with no
exits. The registered build of the same game is a separate release
(`~/Downloads/Alan 2.6/A-Matter-of-Time_DOS_EN_v12/Time1.acd`, v1.2), which has
no `End_demo` and uses `showjpg` instead of `viewer`.
