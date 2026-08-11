# The Wheels Must Turn — walkthrough

- **Engine:** ADRIFT 3.90 Release 16 (`Wheel105.taf`, 44,666 bytes), version
  1.05, by **Heal Butcher**, 2001 — **second place, Official ADRIFT Spring
  MiniComp 2001**. **5 rooms, 4 NPCs, 134 tasks, 15 events, no variables.**
- **Result:** the game's single ending, at `That is it, Twenty-Three.` — the
  gilded cage. **There is no score** (`score` → *"There is no score for this
  adventure."*, and there is not one `ACT type=4` in the file), and **T41 is
  the only task in the game with an EndGame action**, so the route is maximal
  by construction rather than by argument.
- **Solution:** `goldens/wheels_must_turn_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh` with `SCR_SKIP_WAITKEY=1`). Win marker:
  `That is it, Twenty-Three.`
- **Provenance:** **the author's own walkthrough**, `walkthru.txt` shipped
  inside the game's zip (`~/Downloads/zip_w105/`). It is replayed line for
  line; the analysis below was read back out with `SCR_DUMP_TASKS`.

## Route

```
Walking in the Hamster Wheel   z / z / z / z / z
                               out
In the Warehouse               z
                               take something
                               read book                (starts the clock)
                               ask awning about hedge-clippers   (+1)
                               ask awning about awning           (+2)
                               ask awning about fatigue          (+3)
                               z                                 (+4 collapse)
In the Rubble                  ask devil about dilemma           (+5)
                               ask devil about girls             (+6)
                               ask devil about fatigue           (+7 clippers)
                               take tool                         (+8)
                               down
Below the Warehouse            cut string 23                     (ending)
```

Nineteen commands. Five `<waitkey>` pauses in a full run, all skipped by
`SCR_SKIP_WAITKEY=1`, so one line of the solution is one command.

## It is one fixed clock, not a puzzle

Everything after `read book` is a single event chain. Events 5–14 all carry
`startTask=14` — 1-based, so **T13 `read book`** (`x`/`open`/`look` `* book |
bible | servos | small *`, restricted to holding it) — and fire on absolute
offsets from that command:

| Offset | Event | Runs | Effect |
|---|---|---|---|
| +1, +2, +3 | 5, 6, 7 | — | the rumbles under the floor |
| **+4** | 8 | T17 `!moveawning, player` | the warehouse collapses; Awning vanishes down a rent and **you are moved to room 3, In the Rubble** |
| +5, +6 | 11, 12 | — | the Devil fidgets |
| **+7** | 13 | T19 `!movehedgeclippers` | the Devil dislodges the hedge-clippers from his spine; they **clatter at your feet** |
| **+8** | 10 | T18 `!movedevil` | the Devil levitates away |

So the six conversation turns are **timing, not content**. `ask awning
about …` and `ask devil about …` are unrestricted flavour tasks, and seven
`z` in their place produce the same ending (verified) — which is exactly what
the author's file says, listing five interchangeable topics *"or simply enter
wait"*.

**What does matter is the count.** The clippers drop in turn +7's *event*
phase, after that turn's command, so `take tool` has to be the **+8th**
command. Do it on +7 and nothing is on the ground; you notice only at the
harp, where every `cut` answers *"You can't cut string N with your bare
hands."* Nothing expires afterwards — the clippers stay put once the Devil
has gone.

The opening is its own small clock: events 0–3 fire on turns 2–5, and events
2 and 3 run **T0 `!announcement, move player`**, which walks you out of room 0
into room 1. Both rooms are called *Walking in the Hamster Wheel*, so the
move is invisible — but `o` is **T1**, room-locked to room 1, and that is
what the five `z` are for. Leaving then starts event 4 (`startTask=2` → T1),
which one turn later runs **T15 `!movebook`** and drops the Book of Servos at
your feet: hence the single `z` before `take something`.

## The harp

`cut * N *` is a task of its own for every string, T21 upwards, each
restricted on `RESTR type=0 … obj4=[hedge-clippers]` being held. Only
**T41, `cut * 23 *`**, has an action, and it is `ACT type=6 v1=1`. Twenty-three
is your own number — the intro tells you *"You are Hamster Twenty-Three."*
Strings **6 and 9** answer *"String 6 is already severed."*: those are the two
girls netted and hauled away in the opening scene. Everything else is
*"resistant to your efforts."*

## Why "Better luck next time." never appears

T41's ending is `ACT type=6 **v1=1**` — EndGame **lose** — and
`SCR_TRACE_TASKS=1` duly reports `Task: task 41 action 0 ended game`. The
message is buffered by `task_run_end_game_action()` exactly as it should be.
It is then **erased by the game's own ALR table**, which de-obfuscating the
`.taf` shows in full:

| Runner message | Replacement |
|---|---|
| `Better luck next time.` | `  ` (two spaces) |
| `You scored 0 out of the maximum 0!` | a single space |
| `That is 100% of the game!` | a single space |
| `[Press any key to end]` | `<c>[Game ended]</c>` |
| `^title` | `<b>The Wheels Must Turn</b>` |
| `You can't go in any direction!` | the Master's own refusal |
| `You get no reply from the darkness.` | the butterflies line |
| `Moving to in the warehouse…` / `Arrived in the warehouse.` | two spaces |

Heal Butcher has ALR'd every stock Runner string out of the game so that the
ending is nothing but the gilded-cage passage and the Necronomicon epigraph.
The last line of the transcript is that pair of spaces.

**The portable lesson:** an ADRIFT game can suppress *any* engine message,
including the end-of-game ones, so a harness win marker must be taken from
the game's own text — never from `Congratulations!` / `Better luck next
time.` / `I'm afraid you are dead!` on the assumption that the engine will
print them. This looked like an engine bug for a while; it is not one.

## Notes

- **`down` at the end is a real room exit**, the only one in the game that
  isn't a task moving you. Everything else — 0→1, 1→2, 2→3 — is an event or
  a task.
- **`take something`** works because the book is described as something wet
  nudging your foot before you can see what it is; `take book` works too once
  it has been named.
- Two rooms share the name *Walking in the Hamster Wheel*, which is
  deliberate: the fiction is that you never stop walking.
- The prose is nasty (a slave-labour hamster-wheel factory, hosings,
  netting, hedge-clippers aimed at a child-like narrator's body) but it is a
  literary comp entry, not AIF, so it is committed like the rest of the
  corpus.
