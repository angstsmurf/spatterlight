# Report Espionage — walkthrough

- **Engine:** ADRIFT 3.9 (`report.taf`, 41,801 bytes). *"A dynamic adventure
  set within Piopio College, a school principaled by Brian Tegg."* Reports are
  due, yours is a disaster, and the only way out is to collect all seven parts
  of it and burn them. **25 rooms, 33 objects, 23 tasks, 18 NPCs, no events**,
  one variable (`ldoor`).
- **Result:** ★ **WON, 100/100** — the sum of every `ACT type=4` in the file.
  **All 23 tasks fire on the route**; there is nothing in the game that is not
  worth points.
- **Solution:** `goldens/report_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `Congratulations you have been victorious.`
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS` +
  `SCR_DUMP_OBJLOC`.

## Route

```
Sam / male                             <- the two opening prompts, not commands
Common Rm N   open desk (+2) / take set / take cards
Common Rm S   switch off stereo (+5) / open michaels locker (+3) / take ball
bush          steal lighter (+3) / take stick
Geography Rm  take apple                       (out of the rubbish bin)
main block    throw apple at fan (+5) / take shard
Soccer Field  give ball to michael (+3)        -> Chemistry report
Deck          give set to mrs walsh (+3) / wear earplugs
Common Rm N   smash greg with shard (+8) / take history report
Common Rm S   open gregs locker (+5) / take note
Geography Rm  open door (+1)
Library Foyer take book                        (leave the door open)
Library path  give money to bursar (+3)        -> calculator
              [corridor 14 -> 15 -> 16 -> 7, one way]
Stats class   give calculator to mr cook (+5)  -> Maths report
Homus' Office give book to hamish (+3)         -> the Silmarillion
Outside Staff ring homus (+4)                  -> Biology report, in his office
              take biology report
Library       close door / give cards to gapper (+4) / take lamington
              give silmarillion to liz (+5)    -> English report
Outside Rm 9  give lamington to student (+3)   -> the student, as an object
Main Office   smash glass with student (+8) / switch alarm (+4)
Staffroom     take economics report / take slippers
Common Rm S   give slippers to luke (+3)       -> the letter
Todd's Office give letter to mrs todd (+3)     -> opens Tegg's office
Tegg's Office give stick to shane (+7)         -> PE report
              score
              burn report (+10, EndGame win)
```

## Seven report cards, seven owners

| Report | Held by | Prised loose with |
|---|---|---|
| History | Greg (Common Room N) | `smash greg with shard` |
| Chemistry | Michael (Soccer Field) | `give ball to michael` |
| Maths | Mr Cook (Stats class) | `give calculator to mr cook` |
| English | Liz (Library) | `give silmarillion to liz` |
| Economics | nobody — loose on the staffroom floor | `switch alarm` first |
| Biology | Homus (his office) | `ring homus` from the phone booth |
| PE | Mr Tegg (his office) | `give stick to shane` |

`burn report` (T22) is `where=3` — all rooms — with those seven as held-object
restrictions and nothing else, so once you have the set you can finish on the
spot. The lighter is decorative; T22 never checks for it.

## The library door

Three exits, one reversible task, and they do not agree:

| Exit | Gate |
|---|---|
| `room=4 W -> 5` (into the Foyer) | `gateTask=4 wantDone=1` |
| `room=5 E -> 4` (out of the Foyer) | `gateTask=4 wantDone=1` |
| `room=5 N -> 6` (into the Library) | `gateTask=4 wantDone=**0**` |

So the door has to be **open** to use the Foyer as a corridor and **shut**
before you can walk into the Library proper: `open door` / `w` / `close door` /
`n`, and `open door` again on the way out. Liz gives the hint from inside the
Foyer description — *"Close the door!"*.

T4 is `rep=1` and reversible, and **re-opening does not re-score** — the +1 is
banked the first time and every later toggle is free. That matters, because
the route works the door four times.

## Two gates that are easy to miss

- **`switch off stereo` is the first real move.** `EXIT room=0 E -> 3` is
  `gateTask=1 wantDone=1`: Phil sits in the Common Room doorway until the music
  stops (*"You hear Phil getting rather angry and a chair scraping as he moves
  away from the Common Room door"*). Nothing east of the Common Room exists
  before that.
- **`open gregs locker` is restricted on T8.** You have to flatten Greg with
  the shard first; the locker holds the money that buys the calculator off the
  Bursar.

## Mrs Walsh wanders

She is the only NPC in the game that moves, patrolling the library path (rooms
3, 10, 11, 12, 13), and T10 `give set to mrs walsh` needs her present —
otherwise *"Mrs Walsh is not here! (Thank God)"*. The route hands her the
Correspondence Set **on the Common Room Deck**, on the way back from the soccer
field, because that is where she is standing on that turn; a route that burns a
different number of turns beforehand has to go looking for her.

Her earplugs are not a joke item: **T21 needs them WORN** (`RESTR type=0 v2=2`).
Without them, `give stick to shane` gets *"You can't get close enough to Shane
without being repelled by the monotone of BR!"*.

## The chain into the staffroom

The Economics report and the slippers are lying on the staffroom floor, and the
staffroom door is `gateTask=15`:

```
take cards (Common Room table)
  -> give cards to gapper      (Library)     -> lamington
  -> give lamington to student (Outside Rm 9) -> the Year 8 student becomes an
                                                 OBJECT in your inventory
  -> smash glass with student  (Main Office)  -> "You grab the student by the
                                                 ankles and swing him mightily
                                                 towards the alarm"
  -> switch alarm                             -> the staffroom empties
  -> take slippers             (Staffroom)
  -> give slippers to luke     (Common Room)  -> the letter
  -> give letter to mrs todd   (her office)   -> Tegg's office unlocks
  -> give stick to shane       (Tegg's office)-> PE report
```

`give lamington to student` moves NPC 12 to nowhere and puts object 32,
*"student"*, in your hands; `smash glass with student` destroys the object and
puts the NPC back in the room, dazed. It is the longest single dependency in
the file and it runs from one end of the map to the other twice.

## The one-way corridor

`room=14 N -> 15 -> 16 -> 7` runs from outside the Bursar's office straight
back out behind the main block, and **rooms 15 and 16 have no southward
exits**. It is a shortcut, not a passage: the route uses it once, to get from
the Bursar (money → calculator) to Mr Cook (calculator → Maths report) without
walking the paths again.

## Notes

- **`ACT type=0` destination rooms are 1-based**, as in every other 3.9 game in
  this corpus: T8's `v3=1` drops the History report in room 0, T7's `v3=8`
  drops the shard in room 7, T18's `v3=23` drops the Biology report in room 22.
  All three have to be `take`n afterwards.
- **`open bin` is refused** (*"You can't open the rubbish bin!"*) — `take
  apple` works directly.
- **There are no events and no clock.** Nothing is timed, nothing can be
  missed, and the only moving part in the world is Mrs Walsh.
- **No hint tasks** and **no `<waitkey>`** anywhere (`SCR_MARK_WAITKEY=1`).
- **There is no losing ending** — no `ACT type=6` with `v1` other than 0.
- The ending rolls credits over what became of all eighteen characters:
  *"Sean died of lung cancer aged 24"*, *"Mr Cook managed to lose his
  calculator the next day"*, *"Gapper failed to win any of his games of
  Solitaire."*
