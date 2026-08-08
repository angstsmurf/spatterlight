# The Cellar — walkthrough (**ending reached**, 132 commands, replayed verbatim)

- **Author:** David Whyld, 2007, written for the **H. P. Lovecraft Commonplace
  Book & Exhibit**. The epigraph is the Commonplace Book entry the game is
  built on: *"Man's body dies - but corpse retains life. Stalks about - tries
  to conceal odour of decay - detained somewhere - hideous climax."*
- **Engine:** **ADRIFT 4.00**, self-described as "Release 46"
  (`xxd -p -l 12 games/TheCellar.taf | cut -c17-22` → `93453e`). 5 rooms,
  53 objects, 141 tasks.
- **Result:** the ending is reached. **No score** and **no `ACT type=6`
  anywhere** — the game finishes by setting `VAR 12 [game over]`, so the row's
  marker is the ending line itself.
- **Source:** `downloaded/TheCellar_clubfloyd.html`, the ClubFloyd session of
  12 June 2022. Replayed **verbatim, all 132 commands**. Nothing needed
  repairing.
- Row: `cellar_solution.txt|TheCellar.taf|And so The Cellar has ended. Many thanks for playing.|SCR_SKIP_WAITKEY=1`.

## Two independent sources agree

The game ships **its own walkthrough**, on TASK 1, reachable by typing
`walkthrough`. It is 24 commands and it too replays verbatim:

```
read papers / x table / get wad of cloth / e / n / talk / open fridge /
get egg / d / talk / yes / x chair / e / talk x7 / take hand / talk x3
```

So the solution path is confirmed twice over, from the author's file and from a
group of players who had never seen it. The **ClubFloyd** route is the one
wired up, because it is the downloaded file and because it is far the better
test: 132 commands of typos (`x barreks`), dead ends (`pull box`, `move
floorboard`, `do dishes`, `touch me`) and four `undo`s reach a great deal more
of the 141 tasks than the shortest path does.

The author's own note attached to the built-in walkthrough is worth quoting,
since it explains why the short route feels thin: *"If you find yourself a
little confused when the game concludes, that's probably because you stuck
rigidly to the walkthrough. Try again. There are other things to do."*

## The shape of the game

Father's room → the key is in a box under the bed, and the box cannot be
opened there, cannot be lifted, and the bed cannot be moved far enough. The
key is not in the box at all: `x table` finds a loose leg, and tugging the wad
of cloth out from under it drops the key free. East, then north, then down into
the cellar, and the rest of the game is a conversation.

The thing in the cellar is your Uncle Gavin, back from Africa in a state the
Commonplace Book entry describes exactly. He wants feeding — the kitchen has an
egg in the fridge, a cob in the oven, a sandwich on the stove — and he wants an
audience for a story told in five parts (`VAR 5` through `VAR 10`,
*part 1 done* … *part 5 done*), with a trip back up for his satchel in the
middle of it. `talk` is most of the verb list.

The twist is in the game's own closing text, which asks it outright: *"Did you
expect the twist at the end of the tale — that the corpse referred to in the
introduction was not, in fact, Uncle Gavin, but the player's father?"* The last
line is your father taking off his glasses.

## Two divergences from the ClubFloyd transcript — both ours to be proud of

ClubFloyd's Floyd bot plays ADRIFT games **through SCARE itself** — *"Welcome
to the Cheap Glk Implementation, library version 0.9.0"* is in the log's first
lines. So the log is stock-SCARE output, not run400 output, and it is **not an
oracle**. Both places where our transcript differs are places where scarier has
since been made more faithful:

* **`open fridge` / `open oven`.** The log has the generic *"Inside the fridge
  is an egg."* scarier prints the object's own in-container description
  (*"There is a single egg inside. Your father clearly does not intend you to
  overfeed yourself in his absence."*) and, for a single contained object, uses
  run400's postfixed format. Both rules are recorded as run400-verified in
  `lib_list_in_object()` (`sclibrar.cpp`), arbitrated against the
  *It's Easter, Peeps!* transcript, which exercises the one-, two- and
  many-object cases.
* **The ellipsis.** The game text uses a real U+2026. cheapglk folded it to
  `...`; scarier passes the byte through. Cosmetic.

Word-level agreement between the log and our transcript is **98.3%**, and every
remaining difference is the MUD's 70-column wrap against our 80, the log's
stripped-out `[LINK]` anchors, or one of the two items above.

## Shape of it

`harness/cellar_solution.txt` — a 43-line comment header and then the
ClubFloyd session unedited, from `x me` to the last `talk to dad`.
`SCR_SKIP_WAITKEY=1` is required: the five-part story is full of `[MORE]`
pauses.
