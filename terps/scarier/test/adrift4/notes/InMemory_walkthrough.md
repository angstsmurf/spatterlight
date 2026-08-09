# In Memory — walkthrough (**WIN**, 15 commands)

- **Author:** Jacqueline A. Lott, written for *The Indigo New Language Speed
  IF* (played at ClubFloyd on 1 May 2011). You are Alex, unconscious in a
  hospital bed, and the game is the last seven things you remember.
- **Engine:** **ADRIFT 4.00**
  (`xxd -p -l 12 games/InMemory.taf | cut -c17-22` → `93453e`).
- **Result:** **WON.** No score system (`score` reports 0 of 0).
- **Source:** `downloaded/InMemory_clubfloyd.html`, a session covering all ten
  Indigo speed-IFs. Route derived from the task dump.
- Row: `inmemory_solution.txt|InMemory.taf|had ceased to beep.|SCR_SKIP_WAITKEY=1`.

## What the game actually is

Not a puzzle game. There is exactly one ending task, and it is a counter:

```
TASK 178 where=3 room=-1 restr=1 rep=0 cmd=[EndGameScene]
    RESTR type=4 v1=2 v2=2 v3=7      # variable 0 == 7
    ACT   type=1 v1=0 v2=0 v3=10     # move player -> Unconsciousness <3>
    ACT   type=6 v1=1                # end game
```

`let go` in Unconsciousness \<1\> moves you to Unconsciousness \<2\>, where
seven memory objects float in the indigo light. Naming any of them
(`rabbit`, `desk`, `outfit`, `Sam`, `headphones`, `book`, `vista`) drops you
into that memory's room. Each room then asks you one open question, and has a
swarm of one-shot answer tasks behind it — 10 to 20 per room, each with dozens
of ALTCMDs, so almost any sensible word matches something. Whichever one
fires does the same four things:

```
ACT type=3 v1=N  expr=[Upbeat]      # remember the answer, as a text variable
ACT type=3 v1=0 v2=1 v3=1           # variable 0 += 1
ACT type=0 ...                      # hide that room's focus object
ACT type=1 v1=0 v2=0 v3=1           # walk you back to Unconsciousness <2>
```

Seven rooms, seven increments, and `EndGameScene` fires by itself. The text
variables are never read back for anything mechanical — they only colour the
prose — so **there is no wrong answer**, and the route below simply takes the
first option of each set. Any other legal answer wins too, with different
words on the way.

| room | prompt is about | the answer this route gives |
| --- | --- | --- |
| Holding onto the Bunny | your childhood mood | `happy` |
| School Days | your best subject | `english` |
| Homecoming Night | your outfit's style | `casual` |
| Loving Sam | your favourite thing about Sam | `smile` |
| Lost in Sound | the music in your headphones | `rock` |
| Imagined Realities | the kind of book | `fantasy` |
| The Vista Back Home | the landscape | `mountains` |

## Shape of it

`goldens/inmemory_solution.txt`, 15 lines — `let go`, then seven noun/answer
pairs:

```
let go
rabbit / happy
desk / english
outfit / casual
sam / smile
headphones / rock
book / fantasy
vista / mountains        -- counter hits 7, WIN
```

The ending is not a victory. You surface just far enough to hear Sam and Chris
at the bedside, and then the heart monitor stops; the win marker is the last
line of it, `beep, beep had ceased to beep.`
