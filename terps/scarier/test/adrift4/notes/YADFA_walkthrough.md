# YADFA — walkthrough (**WIN, 243/231 — 105%**)

- **Game:** *YADFA - Yet Another Damn Fantasy Adventure* by **David Whyld**.
  You are a farm boy ("a dung scraper, Your Highness") who sets out from
  Castle Bloodheart after King Bloodheart's daughter, Princess Isabella, is
  kidnapped by the sorcerer Malgor the Mad.
- **Engine:** **ADRIFT 4.00** (`GAME version=400`; `xxd -l 16 games/YADFA.TAF`
  → `3c42 3fc9 …`). **81 rooms, 61 objects, 625 tasks, 16 NPCs, 2 events, 70
  ALRs**, declared MaxScore **231**.
- **Result:** **WIN.** The King's reward scene — *"So it seems your dreams of
  marrying Princess Isabella have not come to fruition after all … but you have
  gained yourself a nice (haunted) castle. Not bad for a day's work."*
- **Score: 243**, i.e. **105% of the declared maximum**, in **341 commands**.
- **Harness row:**
  `yadfa_solution.txt|YADFA.TAF|gained yourself a nice (haunted) castle. Not bad for a day's work.|SCR_SKIP_WAITKEY=1`,
  PASSing golden.
- **Previously** this was the corpus's oldest open partial-progress
  checkpoint: 83/231 after 117 commands, parked at "By a signpost" because the
  session that derived it was cut short. Everything from the Village onwards
  is new.

## The score really can exceed the declared maximum

231 is simply the wrong number. The game contains **93 `ACT type=4`
(ChangeScore) actions summing to 314**, many of them mutually exclusive
(alternate solutions to the same puzzle), some negative (`smash idol` is -15).
Whyld evidently added up one route. The ending text even hedges for it:

> If you achieved less than 231 points you might like to try again. If you did
> better than that you obviously did really well.

So the win marker for this row is the **King's reward line**, not a score line.
(It is matched on its tail only — the sentence wraps in the transcript.)

## There is an ending, and no `EndGame` action anywhere

An earlier pass over the task dump concluded YADFA had no scripted ending
because it contains **no `ACT type=6` (EndGame)**. That verdict was wrong.
**TASK 579** — `e` at ROOM 12 (Outside throne room) while **holding both
Isabella and Otto** — moves the player to ROOM 13 and prints the reward scene,
after which the game only offers restart. That *is* the win; ADRIFT games can
end by walking into a terminal room.

## The critical path

Numbers below are task ids from `SCR_DUMP_TASKS=1`.

### Castle and village (the original 117-command checkpoint)
`gather mud` at the Market → `throw mud at guards` at the Castle gates →
`take needle`; mend the leaky bucket at the Well with needle + twine and
`wash`; sell your father's knife to the elves **refusing their first offer**;
recruit **Otto** in the Inn with a steak and **Grarrrrr** the dog with the
nuts. In the tavern "The Black Midget": `kill nobleman`, then — strictly
between that first blow and the finishing one — `talk nobleman` / `nod` to be
given his shield, then `kill nobleman` again. `bribe lance` to reach the chasm
and valley; `blow whistle`, `x body`, `mend shield` (**T397** — the **mended**
shield obj51 is what the final fight requires; the broken obj20 is instant death);
take the **feather**; Bugha, the shrine `pray`s, the toll booth, the graveyard.

### Money
The knife having been sold early, `cut tree` (+9 gold) is gone forever, and
`give nail` to the witch-burners is mutually exclusive with the trumpet the
witch rescue needs. The remaining gold is `kill creature` in the valley (+4
score, +9 gold) and selling the ring and the gold nugget to **Theef** (+2 and
+6 gold each) — enough for Gribbly's map (5) and key (3).

### The idol and the witch — the spell
`drop flower` at the idol clearing (**T472**, +5). South twice to the
witch-burning: `blow trumpet` (**T227**, +4) scatters the mob and frees the
"witch", then walking `n` (**T231**) triggers her thank-you — she teaches the
one-shot **"abracadabra"**. This is not optional: it is the only way to survive
Gladrin's ambush on the far side of the archway (**T533/T536** are death
without it).

### Gribbly, Erik and the rocket launcher
`buy map` (**T248**, +2, 5 gold) then `x map` (**T252**, +1) — the latter is
what opens the Temple entrance (`EXIT room=54 IN → 59 gateTask=252`).
`buy key` (**T249**, +2, 3 gold) is the only key to the room-62 door.
Mad Old Erik, in the ruined city, wants **five pieces of junk**
(`give <junk> to erik`, **T261**, each +1 to his counter; he **refuses the bent
nail**, so pick five others) before `talk erik` (**T259**, +5) will tell you
the plot. Only then does `say to gribbly erik` (**T224**, +5) yield the
**rocket launcher** — whose single shell must be saved for Malgor.

### Dungeon: let the prisoner kill the ogre
`get torch` at the junction (**T502**, +2), `open door` at the End of passage
(**T253**, +1, Gribbly's key), `x bones` in the Cell (**T508**, +5) — a mad
prisoner leaps out and charges off after the ogre. Now simply walk `s` from the
End of passage (**T515**) and he kills it offstage for you.

This matters. The alternatives all cost you something you need:
- `kill ogre` yourself (**T513**) works but **removes both Otto and Grarrrrr**,
  and the game's own hint (**T582**) says the maximum score requires both
  following you. Otto is also required by the ending (**T579**) *and* by the
  best `kill malgor` branch (**T568**).
- `fire rocket launcher` at the ogre (**T557**) spends the only shell.
- `abracadabra` at the ogre (**T524**) spends the spell, which then can't save
  you from Gladrin.

### The pit and the balcony
`jump` from the bridge holding the feather (**T246**, +5) → Bottom of pit →
`n` to the Circular chamber. Detour `e` to the Balcony and `list` (**T614**,
**+10** — the single largest award in the game, for eavesdropping).

### The timed stairs sequence
This is the one genuinely sequenced puzzle, driven by `var37`:
1. `n` to the Top of stairs (**T540**) — something big starts coming up.
2. `drop torch` (**T546**, +5) — the torch lands on Malgor's bodyguard and
   kills it. Skipping this makes the next `wait` fatal (**T542**).
3. `wait` (**T543**, needs Otto) → var37 = 1.
4. `s` back to the chamber (**T550**) → var37 = 2.
5. `nw` to the First archway (**T553**) → var37 = 3.
6. `enter archway` (**T534**, +5): you emerge surrounded, and the witch's
   spell fires automatically, blasting Gladrin's mob to ash. Any other move at
   var37 = 3 (`se`, `sw`, `w`, `d`) is death.

### Malgor
`n`/`u`/`s`/`w` back through the chamber (all safe once var37 = 4), `n` to the
Top of stairs, `d` to the Bottom, `s` into the Sacrificial chamber (**T561**,
with Otto). Malgor's `var40` starts at 0 and `kill malgor` below 6 is death:
- `fire rocket launcher` (**T558**) → +3
- `use mirror` (**T566**) → +3 (the *second* `use mirror`, **T567**, is death)
- `kill malgor` (**T568**, **+10**) with Otto following **and the mended
  shield** — with the broken shield instead (**T569**) a gap in it lets Malgor
  kill you.

Then `untie isabella` (**T572**, +2), `untie princess` (**T576**, +1 — a
*separate* task, both fire), and `isabella follow me` (**T577**, +1).

### Home
`n`, `u`, `s`, `w` to the shimmering gateway, `enter gateway` (**T251**) →
back to the Temple; `out`, `n`, `n`, `out` to leave the ruined city, then the
long walk back — `n w w nw nw nw nw nw n` — to the Castle gates (Otto's
presence gets you past the guards, **T580**), `e` to the Courtyard, `e` to
Outside the throne room, and `e` for the ending.

## Red herrings confirmed

The game's own hint **T611** says it outright: *"The game is littered with
items that don't do anything."* Confirmed unused: the scrap of paper's password
(**"Janrat"** appears nowhere in any task pattern in the file), the medallion,
the remote control, the club, the straight nail, the fish, the twine and needle
after the bucket, and the bodyguard's journal after its five `read journal`
pages. Four of those make perfectly good Erik-junk.

## Engine notes

- `SCR_SKIP_WAITKEY=1` is required — the game uses `<waitkey>` freely.
- `RESTR type=4 v1=N` and `ACT type=3 v1=M` address the same variable with
  **`N = M + 2`** (e.g. T227's `ACT type=3 v1=26` sets what T231's
  `RESTR type=4 v1=28` reads). Worth remembering for any dump-driven
  derivation.
- `ACT type=1 v1=0 v2=0 v3=6` (move player to ROOM 6, *"A dark, dark
  place…"*) is this game's death branch — 20-odd tasks end that way.
