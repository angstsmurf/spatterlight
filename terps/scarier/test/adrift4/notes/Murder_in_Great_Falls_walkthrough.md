# Murder in Great Falls — walkthrough (**WIN, 200/200 — full score**)

- **Game:** *Murder in Great Falls* (`mudergreatfalls.taf`, 59,896 bytes).
  **No author is recorded anywhere**: the file carries no author byte-field,
  and `games.manifest.tsv` line 142
  (`https://www.adrift.co/files/games/mudergreatfalls.taf`) lists only the
  title. The last line of the file's "Wild" trailer dates it **24 Nov 2001**.
- **Engine:** **ADRIFT 3.90** (`SCR_DUMP_TASKS=1` prints `GAME version=390`).
- **Size:** 28 rooms, 68 tasks, 61 objects (48 of them static), 10 NPCs,
  **no events and no variables at all**.
- **Result:** **WIN**, ending on *"Ken is found guilty of triple homicide."*
  and the WINTEXT about the chief's office and its view of the beach.
- **Score: 200 out of a declared 200 — every point in the game.**
- **Harness row:**
  `murder_great_falls_solution.txt|mudergreatfalls.taf|Ken is found guilty of triple homicide.|SCR_SKIP_WAITKEY=1`,
  **103 input lines** (the name, the gender, 98 moves and 3 `score` checks),
  PASSing golden.
- **Source:** none. There is no published walkthrough (Key & Compass, IF
  Archive, CASA, IFDB), and adrift.co serves the bare `.taf` with no package
  around it. The route was derived from `SCR_DUMP_TASKS` / `SCR_DUMP_OBJLOC`
  plus the file's own hint menu — which helped less than usual, see below.

A three-day police procedural. You are a detective in the seaside town of
Great Falls; Chief Branis phones to say **Donald Wisker** has been found dead
in the back of his car by the college. Collect evidence, hand it to Jake at
the evidence dropoff, work the town, and on Day 4 name the killer out of
Rick Wisker, Dr. Ross Hagon and Chief Ken Branis himself.

## 200 is provably the ceiling, and it is exactly reachable

The file contains exactly **32 `ACT type=4` awards** and no other scoring
machinery — there are no variables in the file, so nothing an ALR table could
be counting either. They sum to **200**, which is the maximum the game itself
declares, and the route fires all 32:

| task | pts | what | day |
|------|-----|------|-----|
| T0 | +5 | open the office closet → your gun | 1 |
| T1 | +5 | open the office desk → your ID | 1 |
| T4 | +5 | take the box of baggies off the couch | 1 |
| T7 | +5 | read the crimescene handbook in the kitchen | 1 |
| T9 | +10 | bag the lipstick in Ross's roses | 1 |
| T30 | +5 | open Jay's locker → the camera | 1 |
| T12 | +5 | bag the hair in the car | 1 |
| T13 | +5 | open the glove compartment | 1 |
| T16 | +5 | bag the gum | 1 |
| T31 | +5 | photograph the body | 1 |
| T20 | +5 | take the parcel in the Wisker Motors lobby | 1 |
| T25 | +10 | show the parcel to the guard | 1 |
| T28 | +10 | give the gum to Jake | 1 |
| T29 | +10 | give the hair to Jake | 1 |
| T32 | +5 | give the photo to Jake | 1 |
| T34 | +5 | give the lipstick to Jake | 1 |
| T35 | +10 | ask Ross about the club — **ends Day 1** | 1 |
| T10 | +5 | knock at Ross's door | 2 |
| T44 | +5 | bag the cigarette in Ross's ashtray | 2 |
| T58 | +5 | give the cigarette to Jake | 2 |
| T40 | +5 | ask Jake about the hair | 2 |
| T41 | +5 | ask Jake about the gum | 2 |
| T42 | +5 | ask Jake about the lipstick | 2 |
| T60 | +5 | ask Ken about Maria | 2 |
| T53 | +10 | search Donald's waste-paper basket → the note | 2 |
| T55 | +5 | search Ross's desk → the letter | 2 |
| T56 | +5 | ask Sal about Trey | 2 |
| T59 | +5 | ask Trey about the will | 2 |
| T61 | +5 | ask Ross about the will — **ends Day 2** | 2 |
| T62 | +5 | search Trey's body → the piece of paper | 3 |
| T63 | +10 | report to Ken — **ends Day 3** | 3 |
| T66 | +10 | accuse Ken — **the win** | 4 |

22 fives and 10 tens: `110 + 90 = 200`. The golden's `score` checks read
`110` at the end of Day 1, `175` at the end of Day 2 and `190` two moves from
the end; `accuse ken` is T66's last +10.

Nothing is left on the table. The only tasks with endings on them that the
route does not fire are T64 `accuse rick` and T65 `accuse ross`, which are the
two losing accusations.

## The row needs `SCR_SKIP_WAITKEY=1`, and it is not optional

The file contains **15 `<waitkey>` tags** and this route walks through **13**
of them (`SCR_MARK_WAITKEY=1` counts them): two inside the opening phone call,
and the rest in the Day 1→2, 2→3, 3→4 and endgame cut-scenes. Without the
variable each of those pauses eats a line of the script.

What makes this one worth writing down is **where the first pause sits**. The
game has two start-up prompts — a name prompt, and then, because the file's
`PlayerGender` is Unknown, the Runner's gender dialog — and the `<waitkey>` is
**between them**:

```
Please enter your name:
>                            <- line 1, the name
                             <- the <waitkey> eats line 2
Please choose the player's gender (male or female):
>                            <- line 3 is offered as the gender
Please answer "male" or "female".
```

So an unset row does not merely shift by one; it never gets past the gender
dialog at all, because every subsequent line is offered as a gender and
rejected. This is the *Far From Home* trap one notch worse — there a
`<waitkey>` sat **in front of** the name prompt, here it sits **between the two
prompts**. `harness/waitkey_audit.py` classes the row IMMUNE now that the
variable is set.

Nothing in the game tests the gender: there is not one gender restriction in
the file. The dialog is unconditional all the same, so line 2 of the solution
has to answer it.

## The opening room is never described — and that is the author's setting

The transcript goes straight from the gender prompt to the first command, with
no *Office* description. That is not a missing look: `Globals/DispFirstRoom`
is **false** in this file, and `run_main_loop()` (`scrunner.cpp`) only calls
`lib_cmd_look()` when it is set. Compare `losttombv2.taf`, which does print
*The Camp Site* on start-up. Typing `look` produces the Office normally.

## The days are task boundaries, not a clock

There are **no events and no variables** in this file, which is unusual for a
game with a day structure. Every "which day is it" test is a task restriction
or a room `ALT` on one of three tasks:

- **T35 `ask ross about club`** ends Day 1,
- **T61 `ask ross about will`** ends Day 2,
- **T63 `ask ken about trey`** ends Day 3.

Each carries an `ACT type=1` that sends the player home, and each is followed
by a cut-scene and a phone call setting up the next day. The room `ALT`s that
change the weather, empty Ross's office, clear the crimescene tape and put
Trey's body on his own floor are all `type=0` conditions on task 36, 62 or 57
(the dump prints `ALT` conditions **1-based**, so those are T35, T61 and T56).

So the route does all of a day's work first and spends its very last move of
the day on the closer.

### All three closers are `where=3` — runnable anywhere

T35, T61 and T63 are `where=3 room=-1`, which in ADRIFT means *any room*. They
will fire with the NPC absent and never met. Probed directly: from a fresh
start, `s` / `d` / `ask ross about club` ends Day 1 in three moves, from the
player's own living room, without ever having left the house.

The committed route walks to Dr. Ross's Office and to Chief Branis's Office
anyway. That is the authored path, it is what the hint menu describes, and it
is what makes the golden worth diffing.

## Two rooms have deadlines, and one of them costs points

- **The Photography Classroom is Day 1 only.** `EXIT room=9 E -> dest=10
  gateTask=35 wantDone=0` — the exit exists *until* T35 runs. Nothing in there
  scores (the binoculars are T39, no award), so this is a curiosity rather
  than a trap.
- **Ross's Living Room is Day 2 only, and it holds 10 points.** Room 21 is
  reachable only through **T10 `knock on door`** at Outside House, whose two
  restrictions are `type=2 v1=36 v2=0` (T35 **done**) and `type=2 v1=62 v2=1`
  (T61 **not** done) — a window exactly one day wide. The cigarette in the
  ashtray there is the only evidence in the game with a deadline: T44 `bag
  cigarette` (+5) and T58 `give cigarette to jake` (+5). Miss the window and
  the run lands on 190/200 with nothing anywhere to say why, which is the
  familiar "short with a clean transcript" signature.

## Every piece of evidence arrives in your hand

All thirteen dynamic objects start at `pos=-1` — nowhere — and are placed by
the scoring task's own `ACT type=0 … v2=4` (*held by the player*). There is
nothing to hunt for on the floor and there is no carry limit. The box of
baggies (T4, on the living-room couch — the game's first hint points straight
at it) matters only as the `RESTR type=0 v1=5 v2=1` *held* check on T44; the
other three bagging tasks are unrestricted, so they work whether you picked
the baggies up or not.

The camera is the one real dependency: **T31 `take picture of body` requires
the camera held** (`RESTR type=0 v1=10 v2=1 obj32=[camera]`), and the camera is
in Jay's locker at the police department, on the far side of town from the
crimescene. The route therefore crosses Great Falls once before going to the
parking lot rather than after.

## Accusing the wrong man is a losing ending

Day 4 puts you in a small white room with Rick, Ross and Ken. All three
accusations exist as tasks:

| task | command | action |
|------|---------|--------|
| T64 | `accuse rick` | `ACT type=6 v1=1` — **lose** |
| T65 | `accuse ross` | `ACT type=6 v1=1` — **lose** |
| T66 | `accuse ken` | `ACT type=6 v1=0` +10 — **win** |

These are the file's only three `ACT type=6` actions, so the game cannot be
lost any other way. All three read as a confident arrest for a paragraph
before the trial goes wrong, so the transcript does not tell you which you got
until its last line — which is why the row's win marker is *"Ken is found
guilty of triple homicide."* rather than anything from the arrest itself.

## The hint menu is prose, not commands

This is the third game in a row (*Veteran Knowledge*, *The Lost Tomb*) whose
author shipped a hint menu that `SCR_DUMP_TASKS` prints as
`HINTQ=`/`HINT1=`/`HINT2=`. It is also the first where **grepping `HINT2=` did
not hand over the walkthrough**. All five entries are narrative:

| gate | HINT2 |
|------|-------|
| T16 the baggies | *"Try looking on the couch..."* |
| T25 the guard | *"Maybe the receptionist had something for him..."* |
| T35 Day 1 | *"Dr. Ross might know something, but you'll have to know what to ask him. Talking to everyone usually opens new topics to converse about."* |
| T61 Day 2 | *"You might want to look around the police department for a lead..."* |
| T63 Day 3 | *"You musk speak to Chief Branis"* |

Not one of them names a command. So the menu identified the five gates and the
actual phrasings still came out of the dump's `cmd=` / `ALTCMD=` lines. The
`HINT2=` grep is still the right first move — it saved the search, just not the
derivation.

## Engine notes this game contributed

Two, neither of which changes anything:

- **T47 `turn on tv` is `where=0` — runnable NOWHERE.** That is a third corpus
  witness for `Where`/Type 0 after *The Hangover* and *La hija del relojero*
  (see `adrift4-where-norooms.md`). It is invisible in play, because the
  television is a static object the standard library already has an answer for:
  `turn on tv` in the living room, where the tv is, answers **"You can't turn
  that."** As the note above `run_task_refusal()` records, the Runner guards
  the room refusal with `OUT = "" And FLAG = 1`, so any output at all —
  including a library answer — suppresses it. Costs nothing: the task has no
  actions and no score.
- **The 3.9 room refusal fires here, and it is a second live witness for the
  2026-08-10 port.** `knock on door` typed in the Office (T10 is `where=1
  room=5`) answers **"You can't do that here!"** — the first witness outside
  *La hija del relojero*, whose evidence was a Spanish message replacement
  rather than the English string.

## Author typos worth knowing about

They cost nothing here because the `ALTCMD` lists are generous, but they are
the reason to read the dump rather than guess at phrasings: T2's
`ALTCMD[1]=[turn off the famn]`, T16's `ALTCMD[6]=[use baggie on gun]` (for
*gum*), T31's `ALTCMD[2]=[use camersa]`, T52's `cmd=[open des]`, and the Day 3
hint's *"You musk speak to Chief Branis"*.

## The route

The full commented command list is `goldens/murder_great_falls_solution.txt`.
In outline:

**Day 1 — the crimescene (110).** Office: `open closet` (gun), `open desk`
(ID). Down to the living room for the baggies, east to the kitchen to read the
crimescene handbook. Out to 4th Street and west to Dr. Ross's front garden for
the lipstick in the roses. North across Town Square and up Salson Road to the
police department, east into the locker room for Jay's camera. Back down and
east to the College Parking Lot: bag the hair, open the glove compartment, bag
the gum, photograph the body. Into the Wisker Motors lobby for the parcel on
the reception table, upstairs, and `give package to guard` to reach Rick's
office. Back to the evidence dropoff and hand Jake the gum, hair, photo and
lipstick. Then Dr. Ross's office and `ask ross about club`.

**Day 2 — the town (65).** Ross's door is answerable today and only today:
`knock on door`, take and bag the cigarette from his ashtray. Give it to Jake,
and collect his three lab results (`ask jake about hair` / `gum` / `lipstick`,
each gated on the matching Day 1 exhibit *and* on Day 1 being over). `ask ken
about maria` at the Chief's office. North from the Wisker Motors lobby — an
exit that only exists from Day 2 — into Donald's office to search the
waste-paper basket for the note; Ross's own desk, empty of Ross today, for the
letter. Then the Great Falls Apartments: `ask sal about trey` opens the hallway
north, `ask trey about will` in his room, and `ask ross about will` closes the
day.

**Day 3 and 4 — the second body (25).** Trey has been shot. Back to his
apartment, `search body` for the piece of paper, `ask ken about trey` to report
it. That is Day 3; Day 4 is one command in a small white room, `accuse ken`.
