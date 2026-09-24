# Hotel con Fuego — walkthrough (WON, full demo arc)

- **Engine:** ADRIFT 4.0. A short comic mystery demo (Joe, a hotel clerk,
  has a premonition that the hotel will burn down and investigates). The
  game's own intro explicitly states it is an unfinished demo ("This demo
  includes ... everything you need to complete the first big puzzle").
  Content-reviewed: light flirtation/innuendo with an adult waitress
  (Millie) and a running gag about a "mysterious man," no sexual content,
  no minors.
- **Result:** **WON**, reaching the full ending the demo was built to
  reach. Confirmed with `SCR_DUMP_TASKS=1` and `SCR_DEBUGGER_ENABLED=1`
  across the room/task/event tables.
- **The two-timer trap this puzzle hides:** the demo has two independent
  timed EVENTs that both funnel into the same `kill_joe` death task:
  - A ~13-turn "watch the whole stage show" timeout (`TASK 92 miss_show`)
    that fires almost anywhere in the theater area, **except** it
    explicitly excludes room 28 (Theater Entrance) and the backstage/prop
    rooms from its `WHERE_ROOMS` trigger list.
  - A drink-and-sober-up timer: buying beer and opening the purse (see
    below) starts a "NICELY TOASTED" clock that automatically sobers Joe
    back up after enough waiting (`TASK 90 sober2`).
  - `TASK 102 go_back_stage` requires Joe to be **sober** — the opposite
    of the drunk state needed for the purse subplot. Naively getting
    drunk, solving the purse puzzle, then waiting *in the theater seat*
    to sober up runs straight into the ~13-turn show timeout. The fix:
    travel back to the Theater Entrance (room 28) — which the show timer
    ignores — and wait there until the sobering EVENT fires, which takes
    long enough that it looks like it should also trip the drink-death
    timer, but empirically it does not: waiting 22 turns to sober up plus
    several more afterward produces no interruption at all before the
    ticket/backstage sequence completes.
  - The purse (dropped by a departing bar patron after the 3rd `buy
    beer`) is not auto-added to inventory; its contents (a mirror, a tube
    of lipstick) must be `take`n directly after `open purse` succeeds for
    the 3rd time.
- **The backstage puzzle:** a "mysterious man" guards the dressing room
  door. In the adjoining Prop Room, opening the ladder and walking under
  it spooks a superstitious black cat; showing the cat its own reflection
  in the mirror (`give mirror to cat`) sends the mirror-holding cat
  fleeing the room entirely, distracting the mysterious man (he chases
  the cat instead of guarding the door). With the door unguarded, Joe can
  enter the dressing room and examine Haldo's trunk, which triggers the
  long ending cutscene (the mysterious man catches Joe, knocks him out
  with some kind of hypnotic glass eye, and the game explicitly ends with
  "Well, that's the end of the demo... Thanks for playing! Magic Dave").
  The Prop Room's separate ladder/lightbulb sub-puzzle (`climb_ladder`/
  `get_bulb`) is unrelated optional flavor content with no other
  reference anywhere in the task table — not needed for this ending.
- **Wired as:**
  `hotelconfuego_solution.txt|1_Hotel_con_Fuego.taf|Well, that's the end of the demo.|`,
  no env.

## The walkthrough

```
[blank]
[blank]
[blank]
[blank]
[blank]
e
get ticket
[blank]
[blank]
[blank]
n
n
buy beer
buy beer
buy beer
open purse
open purse
open purse
take mirror
s
s
w
n
e
wait (x22)
give ticket to millie
n
n
open ladder
walk under ladder
give mirror to cat
s
e
x trunk
```

The leading and mid-script blank lines each acknowledge one `<waitkey>`
("Press any key") pause embedded in the intro cutscene and in the
`get ticket` task's own multi-paragraph text; the trailing ending
cutscene after `x trunk` needs no further blank-line acknowledgments —
`play.sh`'s own auto-appended `quit`/`y` satisfies its remaining
waitkeys.
