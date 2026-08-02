# Choose Your Own… — walkthrough

- **Engine:** ADRIFT 4 (`chooseyourown.taf`), David Whyld, Summer Comp 2004.
  449 tasks, **one** room, no objects worth the name — a gamebook implemented
  as a state machine.
- **Result:** the *"do nothing"* ending — Carl Jacobson refuses von Lastmere,
  survives the psychic duel by standing still, kills him, and three months
  later is on a balcony with Sharon Elson. This is the deepest ending in the
  file: the only one that reaches von Lastmere's quarters *and* leaves both
  of you alive.
- **Solution:** `harness/chooseyourown_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`; win marker
  `"A hunch," you say. You link arms with Sharon Elson.`).
- **Provenance:** derived here from the `SCR_DUMP_TASKS` page graph (there is
  no author walkthrough — the Plover index carries the game but not a
  solution, which is fair enough for a gamebook).

## How the game is built

Every "command" is a menu option number. There are no verbs, no nouns, no
inventory: the parser only ever sees `1`…`4`.

- The current **page** is a variable (`ACT type=3 v1=3` writes it,
  `RESTR type=4 v1=5` reads it). A task is "on page *N*" purely by having a
  page-equals-*N* restriction; there is exactly one room and you never leave
  it.
- Three more variables carry state across pages: **gender** (`ACT` v4 /
  `RESTR` v6 — 1 = Carl Jacobson, 2 = Rita Monroe, selected by the
  undocumented option `222` on page 2), a **"talked to the man in the hotel
  room"** flag (`ACT` v7 / `RESTR` v9), and **Sharon's state** (`ACT` v8 /
  `RESTR` v10).
- **There is no EndGame action anywhere in the file.** All 53 endings run
  `TASK 38 '- end game'` (there is also a `TASK 287 '$$$$$ END GAME $$$$$'`),
  which prints the closing paragraph and *keeps prompting*. That is why the
  regression row's win marker is a line of ending prose rather than a score
  or a "you have won" banner.
- Pages numbered **2000+** are die rolls: the game prints
  `Press either [1] or [2]` and treats your keypress as "rolled 1–3" or
  "rolled 4–6". They are ordinary menu picks as far as the engine is
  concerned, so the run stays deterministic — no RNG is involved.
- The author left his debug commands in: **`loc`** (TASK 51) prints
  `location = <page>`, and **`a: <number>`** (TASK 73) jumps straight to a
  page. `loc` is what makes deriving a route through this game tractable —
  it does not advance the page, so you can single-step the state machine.

## Route

52 choices. Each line is `page: choice -> page reached`.

```
The train, and the offer
     0: 1  remain in your seat                            ->    1
     1: 2  remain where you are (drugged)                 ->    2
     2: 1  "Carl Jacobson."                               ->    9
     9: 1  say nothing                                    ->   10
    10: 4  ask about von Lastmeer                         ->   14
    14: 3  shout for help                                 ->   13
    13: 1  say you will do it   -- you are handed the gun ->   26
Globrieska
    26: 1  go and find the train driver                   ->   27
    27: 1  knock on the door                              ->   28
    28: 1  call to the train driver                       ->   29
    29: 1  try and break the door down                    ->   30
    30: 1  leave the train and head into Globrieska       ->   34
    34: 1  head into the hotel                            ->   35
    35: 1  follow the heavyset man into the room          ->   36
    36: 2  "No questions. I'm happy with it."   <- flag   ->   47
    47: 2  the bar: ask who she is                        ->   75
    75: 2  "What words am I supposed to speak?"           ->   50
    50: 3  "Just hand me the device."                     ->   51
    51: 1  follow her                                     ->   52
    52: 1  follow her inside                              ->   53
The gift shop
    53: 1  "I am looking for the blonde woman."           ->   54
    54: 1  "What about the curtained area?"               ->   55
    55: 1  "May I take a look?"                           ->   56
    56: 1  head into the back room                        ->   57
    57: 1  hit the shopkeeper                             -> 2012
  2012: 2  die roll -- you duck the vase                  -> 1000
Back on the train, with Sharon
  1000: 1  ask Sharon about herself                       ->   66
    66: 1  ask what she knows about Gemmin                ->   67
    67: 1  reach out and comfort her                      -> 2013
  2013: 1  die roll -- Gemmin walks in; you are taken     ->  169
The metal-table room
   169: 3  wait                                           ->  170
   170: 3  wait some more -- von Lastmere arrives         ->  171
   171: 1  "Tell him you have."                           ->  172
   172: 1  "To kill you."                                 ->  173
   173: 2  "No."   -- he leaves you locked in             ->  174
   174: 2  pick the lock (you can't)                      ->  178
   178: 1  smash it down                                  ->  179
   179: 1  die roll -- you bounce off                     ->  180
   180: 1  try again                                      ->  181
   181: 2  call for help -- Sharon opens the door         ->  182
Escape, and von Lastmere
   182: 2  question her                                   ->  788
   788: 1  go with her                                    ->  183
   183: 1  ask her to continue                            ->  184
   184: 2  ask Gemmin what is going on                    ->  185
   185: 2  demand to see von Lastmere                     ->  777
   777: 1  listen                                         ->  791
   791: 1  kill him (you punch the wall instead)          ->  187
   187: 1  "How do you do that?"                          ->  188
   188: 2  "Forget it."                                   ->  190
   190: 1  attack him                                     ->  191
   191: 2  die roll -- your own powers blunt the blast    ->  192
   192: 3  DO NOTHING                                     ->  THE END
```

The solution file's blank lines are `<waitkey>` pauses, not commands: two at
the very top (title screen, instructions), fifteen more scattered through
the long passages, and two at the end for the epilogue and `THE END`.

## Notes

- **"Do nothing" is the win.** Pages 190 and 192 offer the same four options
  — *attack / use your own powers / do nothing / flee*. Attacking or fleeing
  gets you swatted; using your powers survives but only prolongs the duel
  (the route takes one round of each before the third). Doing nothing makes von
  Lastmere hesitate, and the hesitation costs him his neck. The epilogue
  hangs a lampshade on it — Sharon asks how you knew, and Carl answers *"a
  hunch"*, which is as close as the game comes to admitting that a gamebook
  protagonist is being steered.
- **The gun is never used.** You have to accept the assassination job on page
  26 to be let off the chair (refusing is one of the 53 endings, and it is a
  short one), and the gun is in your hand when you wake on the train — but
  nothing later in the file ever reads it.
- **The impostor thread is the spine of the route.** Everyone in Globrieska
  mistakes Carl for someone else: the heavyset man with the map, Estelle
  Starnavik with "the device", von Lastmere with "our meeting has been fated
  for a long time". Playing along on page 36 (`No questions. I'm happy with
  it.`) sets the flag that keeps the impostor branch alive; telling any of
  them the truth prunes you into one of the short endings.
- **Two die rolls are crossed.** Page 2012 (the shopkeeper's back room —
  `[2]` ducks the vase) and page 191 (the first psychic blast — `[2]` lets
  your latent powers absorb it). Both are presented as luck but are plain
  menu picks, so the transcript is byte-stable.
- **Mild adult content on page 2013.** Comforting Sharon in the carriage
  leads to a fade-to-black that Gemmin interrupts; it is a page of prose, not
  a branch you can route around if you want the deep ending — the capture it
  causes is what puts you in von Lastmere's warehouse.
- **Gemmin / Gebbin.** The torturer's name is spelled both ways within a few
  paragraphs of each other (as is *von Lastmere* / *von Lastmeer* in the
  opening scene). Both spellings are in the golden; they are the author's,
  not the engine's.
- **Playing as Rita Monroe.** The masked man's *"what should I call you?"*
  (page 2) lists only `[1] "Carl Jacobson."` and `[2] "None of your
  business."`, but TASKs 18–20 answer the unlisted input **`222`** and set
  the gender variable to 2, renaming the protagonist Rita Monroe. It changes
  pronouns and a handful of lines but not the page graph, so it is not worth
  a second regression row.
