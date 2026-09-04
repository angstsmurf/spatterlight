# The Vampire With A Conscience — walkthrough (**WIN, 100/100 — full score**)

- **Game:** *The Vampire With A Conscience* (`Vampire.taf`, 63,183 bytes),
  version 1.0, by **Ole Olsen**. `games.manifest.tsv` line 225,
  `https://www.adrift.co/files/games/Vampire.taf`.
- **Engine:** **ADRIFT 3.90** (`SCR_DUMP_TASKS=1` prints `GAME version=390`).
- **Size:** 17 rooms, 137 tasks, 49 objects (33 of them static), 11 NPCs,
  11 events, 8 variables. Exactly **one `<waitkey>`**.
- **Result:** **WIN**, through T108's shoot-out text and T135 `end`
  (`ACT type=6 v1=0`).
- **Score: 100 out of a declared 100 — every point the file can pay out.**
- **Harness row:**
  `vampire_solution.txt|Vampire.taf|Now you are the most powerful vampire alive.|SCR_SKIP_WAITKEY=1`,
  **57 input lines** (55 moves, one `score` check and the final `end`),
  PASSing golden.
- **Source:** none. No published walkthrough exists (Key & Compass, IF
  Archive, CASA, IFDB), and adrift.co serves the bare `.taf`. The file *does*
  carry a hint menu, and it is the most cheerfully unhelpful one in the corpus
  so far — six entries that cover the hotel room and end with **"OK, I got
  downtown. What now?" → "Sorry, you're on your own from now on."** The
  second half of the game was derived from `SCR_DUMP_TASKS`,
  `SCR_TRACE_EVENTS` and `SCR_TRACE_TASKS`.

You are **Jean Buffet**, the 500-year-old delegate from Wianna, in room 1502
of the Oslo Plaza for the annual grand vampire convention. It is 22:00, the
clock runs one minute per turn, and the meeting is at midnight. Grand master
vampire **Igor van der Linden** intends to have you killed at that meeting, so
your plan is to be there first with a gun. The title's conscience is the
game's joke: winning requires murdering the one human being who helps you.

## 100 is provably the ceiling, and it is exactly reachable

The file contains exactly **18 `ACT type=4` awards** and no other scoring
machinery:

| task | pts | what |
|------|-----|------|
| T5 | +5 | `call 5550` — the number on the piece of paper |
| T26 | +5 | `n` — drift into the ventilation ducts as mist |
| T27 | +3 | `listen` — Igor is on floor 15 and plans to kill you |
| T28 | +10 | `hitchhike` on ring route 3 |
| T39 | +5 | `buy beer` in the Bozo |
| T40 | +2 | `listen` again — a call girl is ordered for just before midnight |
| T41 | +5 | `put heroin in beer` |
| T42 | +2 | `give roses to anette` |
| T43 | +10 | `buy heroin` from the pusher |
| T46 | +3 | `listen` in the dark hallway |
| T49 | +5 | `give beer to anette` |
| T54 | +5 | `drain jon simonsen` |
| T62 | +5 | `put stick in container` |
| T64 | +5 | `open container` — Jon climbs out undead |
| T84 | +5 | `enter queue` with both companions |
| T94 | +10 | `push 15` — variant A |
| T95 | +10 | `push 15` — variant B |
| T108 | +15 | `west` into the conference room |

They sum to **110** against a declared MaxScore of **100**, and the ten points
of overshoot are not reachable points that the route misses. **T94 and T95 are
the same +10 on the same command**, `push * %number% *` in the elevator, with
the same four actions (`CurrentFloor = %number%`, hide Anette,
`AnetteVar = 30001`, `+10`); they differ only in what they demand first:

- **T94** wants T93 `ask portiere about igor van der linden`;
- **T95** wants both duct `listen`s (T27 *and* T40).

Forward first-match dispatch runs at most one task per command, so whichever
qualifies, exactly one +10 is paid. 110 − 10 = **100**, which is both the
ceiling and exactly what the route below scores. This run qualifies through
**T95** — we never speak to the portiere. (T96–T99 are the unscored fallbacks
for a player who has only some of the prerequisites; T103 is the bare
"the elevator moves" case.)

## Three clocks, all of them events

Nothing in this game is gated on the minute variable except the two deadlines.
The pressure comes from events:

| event | starts | length | what it does |
|-------|--------|--------|--------------|
| EVENT 2 `[Nutriton]` | at LOAD (`StarterType=1`) | 63 | runs T80 `-NutritionDeath`, a LOSE. **PauseTask = T54** |
| EVENT 3 `[MistForm]` | when T18 completes | 8 | runs T25 `-BackTask`, i.e. `YesNo = 0` — you stop being mist |
| EVENT 6 `[DeathHeroin]` | when **T51 `drain junkie`** completes | 10 | runs T59 `-HeroinDeath`, a LOSE |
| EVENT 7 `[JonsEscape]` | when T54 completes | 19 | runs T77 `-RaiseEscapes` |
| EVENT 8 `[RaiseJon]` | when T54 completes | 20 | runs T78 `-RaiseJonS` |

Three consequences shape the whole route:

1. **Draining Jon Simonsen is dinner.** EVENT 2's PauseTask is T54, so the
   only thing that stops the 63-turn starvation clock is the murder the plot
   needs anyway. It has to happen by 23:03.
2. **The obvious meal is a trap.** The dark hallway is full of junkies and
   T51 `drain junkie` works — and starts EVENT 6, which kills you ten turns
   later. The game tells you why when you `listen` there (T46, +3): *"To
   vampires heroin is just as bad as getting a stick through the heart."*
   The same fact is the weapon — a gram of it in Anette's beer.
3. **EVENT 7 is defused by an object check, not by timing.** T77 first
   requires that the stick is *not* hidden, and `put stick in container`
   (T62) is authored as `ACT type=0 … v2=0 v3=0`, i.e. it moves the stick to
   room −1. Jam the handles and EVENT 7 fires into a failed restriction.
   Leave the stick alone and Simonsen simply climbs out.

`wait` is worth **three** turns here — the file sets `Globals/WaitTurns` to 3 —
which is why six `wait`s plus the seven other commands between the drain and
`remove stick` are enough to clear a 20-turn event. Four `wait`s is the
measured minimum; three leaves T70 still refusing to pull the stick.

## The route

### Out of room 1502

`call 5550` (T5, +5 — `RESTR type=4 v1=0 v2=2 v3=5550`, i.e. the *referenced
number* must be 5550) reaches Jon Simonsen, a human fixer who will meet you in
the Bozo nightclub with a gun and *carbonated* bullets — no silver. `open
door` opens the hotel-room door so the room's own west exit works (T9 only
supplies the message; the gate is the door object). The vase in the hall holds
ten red roses (T47), which are Anette's price later.

The guard in the hall will not let you into the elevator, and the hint menu
says so flatly: *"You can't do anything to get past the guard. Find another
way out of the hotel room."* The way out is **T18 `change into mist`**, which
is `rep=0` — a **one-shot for the whole game** — and lasts 8 turns. It buys
`n` into the air ducts (T26, +5), two eavesdropping turns, and `n` out of the
hotel back yard (T31). Take the **stick** in the back yard on the way past:
it is the only thing in the game that will hold a garbage container shut.

The two `listen`s in the ducts are both scored and both are T95's
prerequisites, so they are not optional flavour: T27 (+3) places Igor on floor
15, T40 (+2) is the call girl ordered "just before midnight".

`change back` (T23), then **`hitchhike`** on ring route 3 (T28, +10) — the
ride costs fifteen minutes of clock.

### Downtown

`ask simonsen about plan` (T48) is the handover: gun, bullets, and Simonsen
starts following you. `buy beer` (T39, +5) in the Bozo, then east and north to
Egertorget and east again into the dark hallway for `listen` (T46, +3), `buy
heroin` (T43, +10) and `put heroin in beer` (T41, +5).

### The backyard

T54 `drain jon simonsen` runs only in room 11 (the Bozo backyard) and only
with **Anette (NPC 7) and the Bozo Guard (NPC 10) both out of the room**. The
Bozo Guard patrols room 6 ↔ room 11 and Anette does not move at all until she
has the roses — so **drain Simonsen before giving them to her**, and the
ordering solves itself.

The corpse is the danger. T76 `-GuardFindsPlayerKillingSimonsen` checks that
the corpse is *not* inside the container and that the Bozo Guard is in the
room; once it has fired, T74 makes going east a LOSE. So: `open container`,
`put corpse in container` (T113), `close container`, `put stick in container`
(T62, +5, and EVENT 7 is now harmless). T61 then lets you back into the club.

### Anette

`give roses to anette` (T42, +2) sets `AnetteVar = 25201` and starts her
following; `give beer to anette` (T49, +5) gets the spiked beer into her.
Note that T53 `drain anette` — the other thing a vampire might do with a girl
who follows him into a dark yard — is `ACT type=6 v1=1`, an immediate LOSE.

Wait out EVENT 8, then `remove stick` (T71; T70 is the refusal that runs while
T78 has not) and `open container` (T64, +5). Jon comes out undead and follows
you again, which is the whole point: the conference-room door is screened by
mirrors, and an undead assistant walks through them.

### Back to the Plaza

`enter queue` at Oslo Central Station is four different tasks. **T84** is the
scored one (+5) and needs **both** Anette and Simonsen present; T85, T86 and
T87 are the same taxi with one companion, the other, or neither, and pay
nothing. All four force the clock to 23:45. Do it before **23:40**, or T83
takes over and loses the game.

In the elevator, `push 15` (T95, +10) sends Anette off to Igor's floor —
`AnetteVar = 30001`, and EVENT 9 hides her via T106. You do **not** want to
follow: `push 0` first, because T104 only lets you west when
`CurrentFloor == 0`. (`push 16` is T100, a LOSE — there is no sixteenth
floor.)

Then west to the lobby, west to the hall outside the conference room, and
west again. **T108** is the win, and it is the most heavily gated task in the
file — seven restrictions:

1. Simonsen in the room,
2. T54 done (he is undead),
3. `AnetteVar == 30001` (she is out of the way),
4. T49 done (she drank the beer),
5. T41 done (the beer was spiked),
6. Anette *not* in the room,
7. **the gun held.**

T109–T112 are the same `west` with progressively fewer of those satisfied, and
all four are `ACT type=6 v1=1` — losses. Finally `end` (T135) is the winning
endgame.

## Two things that look essential and are not

- **The earth from Wianna** in the coffin (T50 `take earth`). No restriction
  anywhere in the file mentions it.
- **The shaving foam** in the bathroom (T130 `push foam`, T131 `smear foam on
  %object%`, with `foamLeft` counting the squirts). It is there for the two
  mirrors in front of the conference-room door — but the mirrors only ever
  matter to a *living* companion, and by the time you reach them Simonsen is
  undead. A genuine alternate-solution shape that the winning route never
  needs; worth a second look if anyone ever wants to know whether the game has
  a "take Anette in alive" branch (it does not — T108 requires her absent).

## Losing endings, for the record

`ACT type=6 v1=1` on: T53 `drain anette`, T59 `-HeroinDeath` (ten turns after
draining a junkie), T74 `east` after the Bozo Guard has seen the corpse, T80
`-NutritionDeath` (63 turns without feeding), T83 `enter queue` from 23:40,
T100 `push 16`, T109/T110/T111/T112 the four under-qualified `west`s, T118
`taste heroin`, and T119 `-ToLate` at 24:00.

## Runner measurement (2026-08-31, run390 under Wine)

The route was replayed command-for-command in the genuine `run390.exe`
(feed `cmdfile_w_vampire.txt`, 84 commands; transcript
`~/adrift-battle/runner/wine/pfx/drive_c/adrift/Adrift_3_vampire.txt`,
every command echoed).  Result: **the real Runner cannot win this game.**

T61 (`east/e/go east/go e` in the Bozo backyard, `Repeatable=0`,
`RepeatText=' '`) fires on the first exit after stashing the corpse.  When
the route comes back to raise Jon and leaves again, the spent task claims
`e` under the pre-4.0 spent-task rule and prints its RepeatText -- a single
space, so the turn is blank -- and does so forever.  The backyard's only
exit is east (no in/out), T61's patterns cover every phrasing, and T114
(the guard's "into the club with you" bounce) is a `-` system task, only
event-fired.  The Runner therefore walls at **70/100**, the exact sum of
the awards on the near side of the second exit.

Our 100/100 is real under Scarier's documented deliberate deviation (the
pre-4.0 spent-task claim is not imported; see
`adrift4-spent-task-vs-restrictions` and Journ2's Lair brick, the same
family).  New Runner fact from this measurement: the spent-task claim
prints the task's RepeatText when non-empty, and the default "You have
already done that." only when it is empty.  The only other divergences
were RNG scheduling (the green-porche traffic event; Jon Simonsen's club
arrival one turn apart).
