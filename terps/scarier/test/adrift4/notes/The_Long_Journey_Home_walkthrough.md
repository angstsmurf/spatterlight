# The Long Journey Home — walkthrough (**UNFINISHABLE, 30/90 is the ceiling**)

- **Game:** *The Long Journey Home* by **Danny Chabino**, released **20 June
  2001** (`Journ2.taf`, 59,124 bytes). The author credit and the date are in
  the file's own `WINTEXT`, which is never printed — see below.
  `games.manifest.tsv` line 111 records no author and sources the file from
  `https://ifarchive.org/if-archive/games/adrift/journey.zip` →
  `journey2/Journ2.taf`.
- **Engine:** **ADRIFT 3.90** (`SCR_DUMP_TASKS=1` prints `GAME version=390`).
- **Size:** 27 rooms, 89 tasks, 37 objects, 2 NPCs (the Creature, Joy),
  5 events, 6 variables.
- **Result:** **no ending.** The route stops on a `score` check.
- **Score: 30 out of a declared maximum of 90 — and 30 is the ceiling of
  legitimate play.**
- **Harness row:** `journ2_solution.txt|Journ2.taf|Your score is 30 out of a
  maximum of 90.`, **46 commands**, PASSing golden.
- **Source:** none. There is no published walkthrough. The route was derived
  from `SCR_DUMP_TASKS` / `SCR_TRACE_MATCH` plus the file's own author hint
  menu (`HINTQ=` / `HINT1=` / `HINT2=`) — the fourth game in a row to ship one.

You wake in your own bed, walk through the bathroom mirror, and go down a well
into a stone underworld whose rooms are named Sorrow, Despaire, Anger, Rage,
Fear and Terror. Somewhere down there a creature wants to play cards with you,
and past the card game is the way home. You cannot get to the card game, and
if you could, the way home is nailed shut.

## Why 90 is not reachable by anyone

The file contains exactly ten `ACT type=4` awards, summing to 90:

| task | pts | what |
|------|-----|------|
| T69 | +5 | descend the well |
| T10 | +10 | climb out of Despaire (**male**) |
| T11 | +10 | climb out of Despaire (**female**) |
| T12 | +5 | chop the path open |
| T74 | +10 | light the torch (**male**) |
| T75 | +10 | light the torch (**female**) |
| T25 | +10 | release the pressure (**male**) |
| T24 | +10 | release the pressure (**female**) |
| T79 | +10 | turn the cards over |
| T85 | +10 | drop the cards in the Void |

The first move of the game is a gender prompt (room 10, `VAR 1 [gender]`), and
**three of the ten awards are male/female twins**. So 90 is the sum of two
mutually exclusive careers; no playthrough can exceed **60**. Thirty of those
sixty are then behind three separate walls.

### 1. The female half of the game is broken

`T24 #12 release pressure (f)` is `where=0` — `ROOMLIST_NO_ROOMS`, which
`task_where_allows_run()` in `sctasks.cpp` answers with a flat `return FALSE`.
The task is runnable **nowhere**, the same authoring shape that caps *The
Hangover* at 5/7.

The second one is nastier, because it is silent. Compare the two torch tasks:

```
TASK 74 ... RESTR ... type=4 v1=3 v2=2 v3=1     <- gender == male
    ACT type=0 v1=13 v2=0 v3=21  obj26=[King of Spades]
    ACT type=4 v1=10
TASK 75 ... RESTR ... type=4 v1=3 v2=2 v3=2     <- gender == female
    ACT type=4 v1=10
```

T74 drops the King of Spades into the Gnarled Woods; T75 does not. And the
woods' only way back out is

```
EXIT room=20 N -> dest=19 gateTask=74 wantDone=1
```

gated on **T74 specifically**, not on "either torch task". A female player who
walks south into Terror lights her torch, scores her ten points, and can never
leave. **Play male.**

### 2. Rage is a one-way trap, and its ten points are stolen

`T22 #12 turn valve debris here` and `T25 #12 release pressure` (+10) both live
in room 9 and carry the **identical** four patterns:

```
* turn * valve *   * open * valve *   * release * valve *   * use * valve *
```

`run_game_commands_common()` in `scrunner.cpp` scans tasks forward and the
first match whose restrictions pass wins. T22 is at index 22 with `restr=0`.
So every phrasing, forever, in every game state:

```
>open valve
Through much vain effort, you begin to realize that the valve will not move
with the debris in the way.
```

— with the debris removed and the pipe fittings mounted, which is exactly what
T25's own hint tells you to do (*"Make sure you've cleared the debris and
mounted the pipe fittings. Then, open the valve."*).

**This is an off-by-one in the author's task list, not an engine question**,
and the file proves it three times over. The same room holds two other
success/failure sibling pairs and the author ordered *both* of them correctly:

- **T18 before T19.** T18 is `remove debris` restricted on holding the mast;
  T19 is `remove debris` bare-handed and is `ACT type=6 v1=2` — instant death.
  Under any order but forward-first-match, the Reservoir would kill every
  player who typed the obvious command.
- **T20 before T21.** T20 is `mount pipe` restricted on T23 (close valve)
  being done; T21 is `#12 pipe fitting too hot`, unrestricted. Under any other
  order T21 would always win and the author's hint (*"First, close the valve to
  stop the release of steam. Then, attach the pipe fittings"*) would be a lie.

Forward order is the only reading under which the author's own hint menu
works — and under forward order the valve pair, alone, is backwards.

It also cannot be shrugged off, because **Rage has one exit and it is gated on
the task you can never fire**:

```
EXIT room=9 N -> dest=7 gateTask=25 wantDone=1
```

with T17 belt-and-braces intercepting `n` while `VAR 5 [taskstate] != 1`, which
only T25 ever sets. Boarding the raft in the Reservoir (T13, `* get * raft *`)
is therefore a **soft-lock**: you are blown south into Rage and stay there.
The route below never goes south from the Lair.

*Not cross-checked against a real `run390.exe`.* It costs 10 points inside a
room you can never leave, so it cannot change the verdict either way, and the
internal evidence above is what a Wine session would be testing.

### 3. The card game has no starter, and the ending is sealed regardless

```
TASK 76 where=1 room=1 restr=3 rep=1 mask=[#A#A#] cmd=[#6 start card game]
    RESTR type=0 v1=3  v2=1 v3=0  obj3=[King of Hearts]    <- held
    RESTR type=0 v1=5  v2=1 v3=0  obj5=[King of Clubs]     <- held
    RESTR type=0 v1=13 v2=0 v3=0  obj26=[King of Spades]   <- hidden
```

Its three restrictions are all satisfiable, and easily — the Creature hands you
the King of Hearts, digging in Despaire uncovers the King of Clubs, and the
King of Spades starts hidden. But **nothing can dispatch it**:

- Command[0] is the author's own internal `#N` label and there are **no
  ALTCMDs**, so no English phrase matches it.
- The file contains **zero `ACT type=5`** (execute-task) actions anywhere.
- None of the five events target it: their raw `affTask`s are 33, 85, 82, 83
  and 1, i.e. T32, T84, T81, T82, T0.

And T77 (`give cards`), T78 and T79 (+10) are all `RESTR type=2 v1=77` — "task
76 must be done". So the entire endgame is sealed: rooms 23 (New Lair), 24
(Broad View) and 25 (The Void), the NPC Joy, and T85's +10.

The route ends by walking back into the Lair holding three kings and typing
`give cards`, which is answered by the library:

> Give what?

Even cheating past it gains nothing. The file has exactly **two** `ACT type=6`
actions — T19's death, and

```
TASK 86 where=0 room=-1 restr=1 mask=[#] cmd=[#17 the end]
    RESTR type=2 v1=86  task85=[#18 drop cards]
    ACT type=6 v1=0
```

The game's **one and only win action** is itself `where=0`. The `WINTEXT` —
*"The Long Journey Home / by Danny Chabino / Released June 20, 2001 / Special
thanks to the members of the Adrift Forum…"* — is dead data in the file.

### A note on typing the label

`!goto lair` (T87) and `!random` (T88) are debug tasks the author shipped in
the release, and they **do** fire if you type them — literal ADRIFT command
patterns match literally, and `!goto lair` teleports you to room 1 from
anywhere. By the same mechanism a real Runner player could type
`#6 start card game` by hand and start the card game; only our headless front
end refuses, because `os_ansi.cpp:286` skips any script line beginning with
`#`. That backdoor is worth 20 points (T79 + T85), is not a solution by any
reasonable standard, and still stops dead at T86's `where=0` wall.

## The 30 points that are real

| task | pts | what |
|------|-----|------|
| T69 | +5 | `down` the well, rope tied and dropped |
| T10 | +10 | `climb` out of Despaire, after `dig` cuts a foothold |
| T12 | +5 | `chop brush with shovel` at the edge of the pit |
| T74 | +10 | `strike stones` in Terror, holding shaft **and** stones |

### Four parser notes, one replay each

- **The Creature eats your first move in the Lair.** T5 (male) / T4 (female)
  match all twenty direction words in room 1, fire once (`rep=0`), hand you the
  King of Hearts and do **not** move you. The extra `s` in the script is that
  throwaway move.
- **The King and Queen of Clubs have no `card` alias.** `obj5` and `obj13` are
  the only two of the eight cards without one, so `take card` in Despaire finds
  nothing. `take king of clubs`.
- **The Gnarled Woods is an RNG maze.** T68 moves you to Terror only while
  `VAR 2 [direction] == 2`, and `EVENT 4 [change randoms]` re-rolls it every
  turn through T0; T70 `#14 wrong way` is the fallback message. Under the
  harness's fixed seed it takes **three** `left`s. Compass directions do not
  work there — and T71 `#14 use compass directions` claims the pattern
  `* *e *`, which matches **any word ending in "e" followed by another word**,
  so `take king of spades` in that room is answered with *"You can't seem to
  remember which way that direction is."* `get king of spades` gets through.
- **Terror is two rooms.** The shaft is in room 21 and the stones in room 22;
  `go through door` (T72/T73) shuttles between them and the two descriptions
  are deliberately near-identical. T74 needs both held.

One more shape worth recording: `T6 #8 slipnslide` in room 3 has the ALTCMD
`[*]` — **whatever** you type on the slippery slope slides you down into
Despaire and drops the shovel in after you.

## Full command list

```
male / z ×12 / go through mirror /
n / take rope / tie rope to palm / drop rope in well / down /
s / s /
e / se / take shovel / dig / take king of clubs / climb /
chop brush with shovel / nw /
w / w / s / left / left / left /
take shaft / go through door / take stones / strike stones /
n / get king of spades / n / e /
give cards / i / score
```

The commented version, with the task numbers each line fires, is
`goldens/journ2_solution.txt`.

## What is left on the table

**60 of the declared 90 points are unreachable by anyone**, and 30 of them are
unreachable by a *male* player:

- **30** are the female twins of awards this route already banks (T11, T24,
  T75) — arithmetic, not a bug.
- **+10** T25, the valve in Rage, stolen by T22's earlier index, in a room
  with no way out.
- **+10** T79, turning the cards over, behind a task nothing can start.
- **+10** T85, dropping the cards in the Void, behind T79.

And the game cannot be won at all, by either gender, by any route, because its
only `EndGame(win)` action lives on a task that is runnable nowhere. This is
the second file in the corpus with that exact shape — see
`notes/The_Hangover_walkthrough.md` and the memory note
`adrift4-where-norooms.md`.
