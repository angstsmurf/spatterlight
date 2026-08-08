# Panic! — walkthrough (**ending reached**, 69 commands, replayed verbatim)

- **Author:** Stewart J. McAbney (MileOut / MileStyle). You are a stigmatic in
  the cathedral of St. Venerius, and the game is an hour of dread told through
  a hymnbook whose text rewrites itself while you read it.
- **Engine:** **ADRIFT 3.90**
  (`xxd -p -l 12 games/panic.taf | cut -c17-22` → `944537`). 13 rooms, 422
  tasks, 20 events, 5 NPCs — Father Wessels, Amelia, the Ghosts, the Bible Boy,
  and God.
- **Result:** the ending, in 68 turns, with eight scoring beats and the closing
  line **"Your rating is Messiah."** There is no `ACT type=6` — the rating is
  produced by EVENT 14, *The Rating System*, so the row's marker is the rating
  line itself. The author's shortest path is not a full-score path.
- **Source:** `downloaded/Panic_walkthrough.html`, the author's own
  walkthrough from www.mileout.org.uk (via the Shadow Vault mirror). Replayed
  **verbatim, all 69 commands.** Nothing needed repairing. It is a full
  session transcript, which is what made the engine bug below findable.
- Row: `panic_solution.txt|panic.taf|Your rating is Messiah.|SCR_SKIP_WAITKEY=1`.

The first command is `1`: the game opens on a five-item menu (Start /
Instructions / About / Show Intro / Credits) in `ROOM 4 [Menu]` rather than in
a room. `SCR_SKIP_WAITKEY=1` is required — the game is full of "Press any key
to continue" beats, beginning with Father Wessels' entrance.

## The shape of it

Four movements, and the verbs stay small throughout.

**The nave.** `read hymnbook` seven times. Each reading is a different text and
the seventh is the one that matters; the podium and the organ are the room's
other two objects. The organ wants five notes — `press key f`, then `play d#`,
`play a#`, `play g`, `play c` — and pays for them with the candleholder, which
is the only light you will get.

**The cellar.** Down for the rope, the tin of grease and the tins; back up and
east and north for the knife under the seat. `open tin of grease with knife`,
then `use grease on hook` frees the key. The key opens the coffin, and the
coffin holds the hammer.

**The statue.** `throw rope at statue` — EVENT 9, *Does the Rope Catch the
Statue?*, is a one-turn immediate-restart event that arbitrates it — then `u`,
`x eyes`, `roll eyes`, and the eye comes loose. Down, `get eye`, west and down
to `wash eye under drip`, and then `read eye`. `use hammer on wall` opens the
way north.

**The Path to Glory.** Rooms 6 through 9 are the four rows of flagstones, and
the sign tells you what to spell. `x flagstones` between each step, and the
answer is **TRUTH** — `step on t`, `stand on r`, `stand on u`, `stand on t`,
`stand on h`. `step into pool` in the Grace of God is the last command: you
drown, and wake nailed to the wall as the statue you have been climbing on,
watching the Second Coming stumble through the cathedral door.

## What this game found: the immediate-restart fixup was eating StartText

Panic! builds its atmosphere out of always-restarting one-turn events —
`RestartType=1` with `Time1=Time2=1` — and two of them carry a StartText and
**no** LookText (`texts=S--` on scdump.cpp's EVENT line), so nothing in a room
description could be printing them. Against the author's transcript, scarier
printed each of them exactly **once** and then went silent for the rest of the
game:

| line | published | before | after |
| --- | --- | --- | --- |
| "The old priest's sudden rattled cough…" | 66 | 1 | **66** |
| "The palms of your hands begin to ache…" | 9 | 1 | **9** |
| "Siezed by a sudden uncontrollable shaking…" | 13 | 1 | **13** |
| "The translucent shimmer of a wraith…" | 21 | 1 | 24 |

The cause was `evt_fixup_v390_v380_immediate_restart()` in `scevents.cpp`.
For any pre-4.0 taf it restarted such an event by hand — state to `ES_RUNNING`,
clock to one less than a fresh length roll — instead of calling
`evt_start_event()`. The event therefore *cycled* perfectly well
(`SCR_TRACE_EVENTS` shows a finish every turn, and the TaskAffected kept
firing, which is why this survived so long), but its start actions, StartText
included, ran only on the very first start.

Fixed 2026-08-04: the fixup now calls `evt_start_event()` and then takes off
the one turn this version spends silently, rather than open-coding a silent
start. The length roll has to come from `evt_start_event()` **alone** — the
first attempt kept the fixup's own `scr_randomint()` too, and the extra roll
per restart churned the RNG stream enough to break three combat walkthroughs.

**Corpus effect:** 13 of the 203 rows gained text, every one a pure addition of
a line that had been silently swallowed — troll's "Sid sips from his mug.",
haunt's grandfather clock, wrecked's seagulls, spirits_flight's wind, tq3's
rattlesnakes, colony's lightning, twilight's apparition, timmy_reid's Billy on
his bike, cybercow_win's robot, secret_of_lost_world's "It starts to rain." and
"The volcano is erupting.", marooned's rescue ship coming over the horizon, and
the whole paragraph in alices_restaurant where Obie takes your wallet and your
belt at the station. No route broke and no win marker moved. `thetest`,
`gateway` and `inverness` — the games whose always-restarting one-turn events
were probed live in run390 for `RUNNER_TESTS_TODO.md` §2 — are byte-identical.

The full write-up, with the two open probes, is `RUNNER_TESTS_TODO.md` §8.

## One residual divergence, recorded not chased

The wraith fires 24 times for us against the transcript's 21. The three extra
are turns 44–46 (`u`, `x eyes`, `roll eyes`) — the stretch spent up the rope on
the statue. The published run suppresses the event while you are up there and
we do not. That is an event *visibility* question (`evt_can_see_event()`, and
where the player counts as being while climbing an object), not a restart
question, and it wants a live probe against run390.
