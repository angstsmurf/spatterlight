# Quest 5 ground-truth oracle (`qvh`)

A headless driver for **QuestViva** — the cross-platform .NET port of the Quest 5
engine (`github.com/textadventures/quest`, branch `main`, aka "Quest Viva"). It
loads a real `.quest`/`.aslx` game, replays a command script, and prints a
normalised plain-text transcript.

This is the reference oracle for the planned native Geas ASLX engine
(`terps/geas/quest5/TODO-quest5.md`, milestone 6): once that engine exists, diff its
transcript for a given command script against `qvh`'s, exactly as
`test/a5_groundtruth.sh` diffs Scarier against FrankenDrift. Until then it also
serves to capture golden transcripts and to answer "what does real Quest do
here?" questions during the port.

The QuestViva checkout lives **outside** this repo (it is ~42 MB and MIT-licensed
but not something to vendor), just like the FrankenDrift build the a5 oracle uses.

## Requirements

- .NET 10 SDK (`brew install dotnet` — this Mac has 10.0.301, arm64). QuestViva
  targets `net10.0`, so with the .NET 10 SDK the checkout builds unmodified.
- Network access on first build (to clone QuestViva).

## Build

```sh
./build.sh
```

Clones QuestViva into `$ORACLE_HOME` (default `~/questviva-oracle`), applies the
deterministic-RNG patch (below), and builds `bin/Release/net10.0/qvh.dll`. With
the .NET 10 SDK the checkout needs no retargeting. Override the location with
`ORACLE_HOME=/path ./build.sh`.

## Run one game

```sh
dotnet bin/Release/net10.0/qvh.dll <game.quest|game.aslx> [command-script.txt]
```

Transcript → stdout, diagnostics (`[diag] …`) → stderr. With no script it just
prints the opening turn.

### Command-script format

Mirrors QuestViva's own `WalkthroughRunner` step format, so a game's built-in
`<walkthrough>` steps and hand-written scripts drive identically:

| line | effect |
|---|---|
| plain line | `SendCommand` (also answers a pending `get input`) |
| `menu:KEY` | answer a `show menu` (`KEY` is an option key) |
| `answer:yes` / `answer:no` | answer an `ask` |
| `assert:EXPR` | evaluate `EXPR`, record `[assert PASS/FAIL]` in the transcript |
| `label:…`, `delay:N`, `runtime:` | ignored (editor bookkeeping) |
| `event:NAME;PARAM` | `SendEvent` |
| `# …` | comment |

`wait` and `DoPause` ("press any key") prompts are auto-continued and consume no
script line. For human-authored walkthroughs that lack the `menu:`/`answer:`
prefixes, a bare line pending a menu/question is resolved leniently (menu key,
option number, or yes/no).

## Corpus regression

`corpus.tsv` is the curated manifest — 66 driven rows: each of the 26 games with
a walkthrough mapped to its walkthrough file and an extractor mode, plus 37
override-only rows
(walkthrough column `-`) for games with no published walkthrough at all. It exists because filename
conventions don't line up with the `.quest` names (Guttersnipe dash spacing,
`Jacqueline Jungle Queen`, `Hawk … - hints-walkthrough.txt`, bare `.txt`), so a
naive `<Game> - walkthrough.txt` match silently misses several.

`extract_walkthrough.py` has two modes:
- **welbourn** — David Welbourn (Key & Compass): commands are `> `-prefixed and
  packed several-per-line with `. `; parentheticals are prose; `X —or— Y`
  (em-dash) means "either works", and we take the first alternative (missing
  this silently stalls the game — an unanswered numbered menu swallows every
  later command, see below). Also handled, because each silently breaks a run:
  - **inline menu / input answers** — an *unprefixed* line quoting the game's
    prompt and ending in the chosen number, e.g. `...thwarted expectations? 1`
    (a numbered text menu) or `What combination do you enter? 987333` (a
    get-input answer). A real `>` command never contains `?`, so the trailing
    digits are unambiguously an answer to send as its own turn. Missing them
    left St. Hesper's psychiatrist menu pending, swallowing 200+ later commands.
  - **dingbat/emoji annotations** — Welbourn sprinkles `☹`/`☺`/`★` into command
    lines to flag notes (`> x figurehead. take it. ☹ w.`). Left in, `☹ w` is an
    unknown command, so the move silently fails and every later command cascades
    into "I can't see that". The symbol ranges are stripped; the `—or—` dashes
    sit outside them and survive.
  - **keypress tokens** — `SPACE` (Bathhouse only) is Welbourn's "press any key"
    notation; the harness auto-continues those prompts, so a literal `SPACE`
    command is dropped.
- **commands** — a bare one-command-per-line list (often the tail of an author
  doc after a prose section), possibly indented and using `menu:` directives;
  prose lines are dropped by a shape heuristic.

`corpus.tsv` has an optional 4th column, **preamble** — `;`-separated commands
prepended before the walkthrough. It exists for games whose walkthrough assumes
a title-screen menu was already dismissed: *Dream Pieces* opens on a
`[NEW GAME]` menu, so without a `new game` preamble every command hits the menu
and fails ("I can't see that") — the whole transcript is garbage.

Note: `game.multiplecommands` defaults to **false** in Core and most games leave
it off, so the engine will *not* split a `.`-joined line itself — pre-splitting
in welbourn mode is required, and also yields one deterministic turn per command.

`run_corpus.sh` drives every non-`hints` row of `corpus.tsv`, writing
`out/<Game>.cmd` scripts + `out/<Game>.out` transcripts and printing a coverage
table (ASL version, steps, emits, error count, final state). Current coverage:
**66 games driven** — **56 `Finished`**, **10 `Running`**, **0 `Wedged`**.

`Finished` means Core's `finish` ran. It is the *only* unambiguous win signal, but
its absence is not a loss: **9 of the 10 `Running` rows are genuine wins in games
that simply never call `finish`** — they print their ending and stop (Balaclava,
El asesino durmiente, First Times, Its election time in Pakistan, Medievalist's
Quest, Nearco II, Sueña un pequeño sueño, cuttings, spondre). For several, `finish`
is *provably* unreachable: spondre's inlined `HandleCommand` routes all input to
ResponseLib topic matching, so Core's `quit` is dead code and Running-at-credits is
the authored terminal state. Do not read the 52/62 split as a 10-game shortfall.

The other `Running` row is a real gap:
- **The Last Hero** — the *shipped game* is unwinnable: every `MoveObject` into a
  challenge room misspells the room name. Best-effort script.
- **WAKE** — the *shipped game* is unwinnable: a `ChangePOV`/`player` mix-up strands
  the endgame puzzle out of scope. See "Known gaps" below.

`0 Wedged`: Whitefield formerly wedged on QuestViva's script-error breaker; its
override's `#!errorlimit=200` directive lifts the breaker past a finite error burst
and the full walkthrough now wins — see below. No hints-only rows
remain: the six games whose walkthroughs are Q&A/prose
hints (Night House, Poppet, What Once Was, Hawk the Hunter, Eight characters…,
Quest for the Serpent's Eye), plus the PDF-only The Brutal Murder of Jenny Lee,
are driven by hand-derived winning scripts in `overrides/` (each linearised from
the hints against the game source). Fifty-four of the 64 rows are driven by curated
`overrides/` (see next section) — all 37 `-` rows plus 17 of the 26 rows that do
have a walkthrough; the remaining nine run the raw walkthrough through the
extractor. See [[quest5-corpus]]. (Dracula is a special case: its only
walkthrough is for the *original 1986 CRL* game, not this 2014 remake, so its
override heavily adapts that solution to the remake's parser — see its header.
The Shack and The Tree are two more: they have no published walkthrough
anywhere, so their overrides were derived entirely from the game source and
their walkthrough columns are `-`.)

### Curated overrides (`overrides/`)

Some walkthroughs do not reach the ending when run verbatim — a typo the game
rejects, one of several mutually-exclusive endings, a nav error vs the release map,
an RNG-/timer-specific solve, or (the seven formerly hints-only games) no linear
walkthrough at all. For each such game, `overrides/<Game>.cmd` is a
hand-authored winning script (with a `#`-comment header documenting every deviation
from the raw walkthrough); `run_corpus.sh` uses it verbatim in place of the
extractor+preamble. `overrides/README.md` tabulates why each exists
(all win except I Contain Multitudes, best-effort, documented above). The nine games whose raw walkthroughs already win have no override.

### `wait` and the turn boundary

Legacy Quest 5 runs the game on its own thread and blocks it inside `wait`, so the
stack survives the prompt and Core's `FinishTurn` — hence every turnscript — runs
*after* the wait callback. QuestViva's async port returns from `wait` immediately
(`WaitScript.ExecuteAsync` fire-and-forgets its continuation), so
`TryFinishTurnAsync` raced ahead of the callback and turnscripts ticked against the
room the callback was about to leave. `patch_questviva.py` section 6 defers the
`FinishTurn` (and pane refresh) of a wait-parked turn until the callback resolves;
the native engine does the same (`finish_turn_deferred_` in `aslx-runtime.cc`).

*The Legend of Robin Hood* is the case that proves it — its win is unreachable under
the racing order (see `overrides/README.md`). Five other goldens moved, all for the
better: *Dracula* and *Dream Pieces 2* stop firing coach/room turnscripts in rooms
the player has already left (Dream Pieces 2 also stops dumping a bogus aggregate
"Lab" room listing every object in the game *after* its "THE END"), and *Dream
Pieces*, *Lost in the Shadows of Time* and *Xanadu — In the Compound* simply move a
nag/quote line to the far side of the transition. No ending, score or error count
changed. The deferral is gated to pre-v580 games — the same gate that decides
whether the engine calls `FinishTurn` at all — so anything authored against modern
QuestViva-era turn semantics keeps the stock path.

### `changed<attr>` fires only on a real value change

Quest 5 (`textadventures/quest` branch `v5`) fires `changed<attr>` scripts from a
subscriber to the `Fields.AttributeChanged` event, and `Fields.Set` raises that
event *only when the value actually changed*
(`changed = value == null ? oldValue != null : !value.Equals(oldValue)`). The
rewrite moved the dispatch into `Element.SetFieldAsync`, which fires it
unconditionally after `Fields.Set` — so a **same-value write recurses forever**.
The common idiom that trips it is an `enter`/`onswitchon` script that re-writes an
attribute already at that value: `Pool.enter → enter_pool → MoveObject(player,
Pool)` in *L Too*, or the three mutually-`SwitchOn`ing game boards in *Eight
characters, a number, and a happy ending*. Both wedge stock QuestViva (the
depth-200 guard never trips — the recursion runs through queued `on ready`
callbacks at depth 0 — so it just spins to the error breaker) but terminate on
real Quest. `patch_questviva.py` section 7 restores the guard at the new dispatch
site; the native engine carries the equivalent guard (`fire_changed_script` in
`aslx-runtime.cc`, gated on a `changed` flag computed the same way).

Five goldens moved, all where the oracle had been *over*-firing and native was
already right: *Dracula* (Van Helsing's `changedparent` printed "follows you"
every turn a turnscript re-`MoveObjectHere`d him, even when he never moved),
*Iron John*, *The Acreage* and *Xanadu — In the Compound* (stock Core
`changedparent → OnEnterRoom` reprinted the whole room description on same-room
moves), and *The Bony King of Nowhere* (the oracle additionally *corrupted* names
— `You can see The  and .` where the fix gives `You can see The Bony King and
Your Dog.`). All five still reach `Finished`; every change is duplicated or
damaged output the oracle no longer emits. With the goldens regenerated, native
replay and the oracle agree on all five byte-for-byte. This also lets *L Too* and
*Eight characters* run their previously-wedging rooms, though L Too still has no
published walkthrough to drive past that point.

### Expression-form prompts (`ShowMenu`, `Ask`)

Two Quest built-ins exist in both a *statement* and an *expression* form, and the
expression form genuinely blocks: QuestViva's `ExpressionOwner.ShowMenu` / `.Ask`
**await** the player's response mid-expression, so a synchronous host has to supply
the answer in place rather than registering a pending callback. The native engine
exposes these as `Interp::menu_provider` and `Interp::ask_provider` — fed by the
next script line in the harnesses, by a nested input loop in the Glk frontend.

*Beowulf* is the corpus's first user of `Ask()` as an expression (`if (Ask("Do you
still go on?"))`); before `ask_provider` existed, the native engine reported
`Unknown function 'Ask'` and its win was unreachable natively even though the
oracle drove it fine. As with the `ask` statement, the caption is *not* printed by
the engine — rendering the yes/no prompt is host presentation.

### Parameterless calls to functions that take parameters

Quest checks a procedure's arity *before* running its body, so a bare
`LockExit` (no argument) fails as `No parameters passed to LockExit function -
expected 1 parameters` rather than dying inside the body on an unbound name.
`WorldModel.RunProcedureAsync` raises it for ASL 520 and later. The native
engine had no such check: it bound nothing, ran the body, and reported
`Unknown object or variable 'exit'` instead — a one-line transcript divergence,
found by *Fountain of Eternal Youth*, whose red pedestal initialises with
exactly that bare call. `Interp::call_function` now mirrors the check. (The
sibling `Too many` / `Too few parameters passed` errors come from
`FunctionCallScript`, i.e. calls written *with* parentheses, and are not
implemented — no corpus game reaches one.)

### Golden baseline (committed regression)

`golden/` holds the frozen answer key: for each of the 66 driven games, the exact
command script (`golden/<Game>.cmd`) and the normalised transcript QuestViva
produces for it (`golden/<Game>.out`). This is the only part of the harness
committed to the repo (alongside `overrides/`) — `bin/`, `obj/`, and the scratch
`out/` are ignored, and the QuestViva clone and the corpus games/walkthroughs all
live outside it.

- **`./check_golden.sh`** replays each committed `golden/<Game>.cmd` through the
  built oracle and diffs the result against `golden/<Game>.out`. It drives from
  the *frozen* `.cmd` (not `extract_walkthrough.py`), so a `FAIL` isolates an
  actual oracle behaviour change — a QuestViva upstream bump, a .NET/RNG
  regression, or a `Program.cs` edit — from a walkthrough-extraction tweak. It
  needs the built oracle and the corpus games; exit status is non-zero on any
  divergence. (Running it also re-confirms determinism: the goldens were frozen
  from one run and every game is re-driven from scratch.)
- **`./update_golden.sh`** refreshes the baseline after an *intended* change: it
  runs the full `run_corpus.sh` pipeline (re-extract → drive → transcript) and
  copies the new `.cmd` + `.out` for every driven game into `golden/`. The git
  diff of `golden/` then *is* the change in "what real Quest does" — review it
  before committing.

This is what makes the oracle a durable reference: the eventual native Geas ASLX
engine diffs its own transcript for `golden/<Game>.cmd` against `golden/<Game>.out`
without needing QuestViva or .NET at all.

### `Wedged`: telling the error breaker apart from a real win

QuestViva ends the game *itself* once its error circuit-breaker trips (20 script
errors, `WorldModel.cs:1195`) — a "session unrecoverably wedged" guard — and this
sets `State=Finished` exactly like a real `finish`. The harness reports that case
as **`Wedged`**, not `Finished`, so a walkthrough that drifted into failing
turnscripts is not miscounted as a win.

The signal for "did the breaker actually fire?" must be the breaker's own private
`_scriptErrorsFatal` flag, read by reflection — **not** the `LogError` count.
Those differ: the breaker's counter increments only on errors thrown through
`RunScriptAsync` (line 1195), but expression-evaluation errors during *output
formatting* (e.g. display-name recursion) are logged via a separate, non-fatal path
(line 1456) that fires `LogError` without feeding the breaker. So the count
over-reports. *The Bony King of Nowhere* is the case that proves it: it reaches its
genuine ending (full "The END" + credits) while emitting **31** display-recursion
errors on the mandatory `attack general`→prison POV-swap render — `errors=31` yet a
true `Finished`, because `_scriptErrorsFatal` never set. The earlier
`errors ≥ 20` heuristic wrongly called that a wedge.

*Whitefield Academy of Witchcraft* was a real wedge: the *mandatory* Grislewood snare
double-teleports the player into "Inescapable Cage," a room reachable only by that
teleport that never gets map coordinates, so its enter-script throws
`DictionaryItem(coordinates,"x")` ~19× *through `RunScriptAsync`* in one transition
and trips the breaker. In real Quest an enter-script error is non-fatal (legacy Quest
has no breaker at all — it prints every script error and carries on), so the game is
winnable there. That legacy behaviour is now reproducible per-game: a
`#!errorlimit=N` first line in a script raises the breaker threshold for that run
(`QVH_ERROR_LIMIT` via `patch_questviva.py` §4; the native engine honours the same
directive). Both Whitefield (34 errors, finite cage burst) and The Acreage (58
errors, finite Port-entry recursion) use it in their overrides and run to their
genuine endings, every error still printed in the transcript. The directive is only
for *finite* legacy-tolerable error storms — an infinite per-turn error loop still
means the game is broken under QuestViva (that stays `Wedged`, whatever the limit).
Genuinely-finished games otherwise show `ERR=0` except Bony King (`ERR=31`,
explained above) and The Shack (`ERR=2` — its own `open drawer`
onopen/changed-isopen recursion hits the 200-depth cap twice on a mandatory step;
output stays correct and the breaker never trips — see its override header).

### Real-time timers: `DrainTimers`

Quest timers and `SetTimeout` fire off `game.timeelapsed`, which QuestViva only
advances when a command supplies `elapsedTime > 0` or `Tick(elapsed)` is called —
the interactive players (WebPlayer/WasmPlayer) do this from a 1-second JS interval
after `RequestNextTimerTick`. A headless run has no wall clock, so without help a
game's `SetTimeout`-gated content never fires. Two corpus games depend on it: *The
Mouse Who Woke Up For Christmas* (a `<dark>` nightclub's "eyes adjust" reveal is on
`SetTimeout(2)`; without it the room stays black and the game is unwinnable) and
*Escape From the Mechanical Bathhouse* (whose 50-second puzzle limit is a real timer
— the oracle simply never advancing it is what gives unlimited turns).

`Program.cs` reproduces the interactive clock deterministically: it subscribes to
`RequestNextTimerTick` and, after each command settles, drains any pending
self-destructing `SetTimeout` timer (named `timeout*` by `SetTimeoutID`) by ticking
exactly its trigger delta. Recurring authored timers are left dormant (they would
loop forever without a real clock). This changed only the three timer-dependent
goldens; every other transcript is byte-identical. (It cannot rescue I Contain
Multitudes: there the ending `SetTimeout` is never even *created*, because it sits
inside a nested `wait{…}` continuation the harness does not pump.)

### Synchronous sounds: `play sound (…, true, …)`

`play sound` takes a *synchronous* flag, and a synchronous play is not a UI hook —
it is a suspend. `PlaySoundScript.ExecuteAsync` claims `WorldModel._waitTcs` (the
same slot `wait` uses) via `BeginPrompt`, signals the turn suspended so the driver
moves on, and awaits the TCS; in a browser the audio's ended-event calls
`FinishWait` and everything after the statement runs. Headless there is no audio,
so without help the rest of that script — very often the ending's closing `msg`
plus `finish` — never runs, while the game keeps accepting commands: a silently
half-dead session that looks like a walkthrough that merely fell short.

`HeadlessPlayer.PlaySoundAsync` therefore flags `IsWaiting` when `synchronous` is
set, handing the resume to `AutoAdvance` like any other wait. This is exactly what
QuestViva's own WebPlayer does when a `WalkthroughRunner` is attached (it forces
`synchronous = false` and calls `Runner.BeginWait()`), so it is the *faithful*
reading rather than a harness liberty. The native `aslx_replay` host installs the
equivalent no-op `play_sound` hook, since the engine treats "the host hook
returned" as "playback finished".

Three corpus rows turn on it. **HMS Victory** is the clean case — its win is
`play sound ("Eight bells.wav", true, false)` immediately before THE END and
`finish` — and **Nearco II** is the same shape. **I Contain Multitudes** was the
corpus's long-standing "harness cannot reach this win" row and is now `Finished`.
Games whose parked tail *is* claimed later (The Tree's `x tube` holcast) are
unaffected: resuming inline and resuming at the next claim produce the same
transcript when no output sits between the two points.

### Two menu systems (why a bad answer used to freeze the game)

Core has two: the engine `show menu` script → `IPlayer.ShowMenu` → answered by
`SetMenuResponse` (the driver's `PendingMenu` path); and Core's ASLX `ShowMenu`
function, which prints a numbered text menu (`1: Yes` / `2: No`) and is answered
by sending the **number** as a normal command (`HandleMenuTextResponse` accepts
integers only). If a text menu is pending and the sent line isn't a valid number
and `menuallowcancel` is false, `CoreParser.HandleCommand` marks it handled and
prints nothing — so every subsequent command is silently eaten. That is exactly
what `1 —or— 2` (sent verbatim) triggered before the extractor fix.

## Two gotchas the harness handles

1. `Begin()` is fire-and-forget internally, but returns a task that completes
   when the turn **suspends** — await it (and `SendCommand`) rather than polling
   for output to go quiet.
2. Quest 5 (ASL ≥ v540) delivers game text as JS calls: `JS.addText(html)`
   arrives as `IPlayer.RunScriptAsync("addText", [html])`, *not* through
   `PrintText`. The harness captures both channels (legacy path for ASL < v540).

## Determinism (RNG)

The oracle replaces QuestViva's `Random` with `ErkyrathRandom` — a C# port of
Spatterlight's `erkyrath_random()` (`terps/common_utils/randomness.c`): xoshiro128**
seeded via SplitMix32, verified byte-identical to the C generator. `build.sh`
injects it via `patch_questviva.py` (idempotent; touches only the three RNG lines
in `ExpressionOwner.cs` plus a new `ErkyrathRandom.cs`), so the clone stays
otherwise pristine.

Seed defaults to **1234** (Spatterlight determinism convention); override with
`QVH_SEED=<n>`. `GetRandomInt(min,max)` uses the inclusive-range mapping from
`terps/scarier/a5rand.cpp` (`span = max-min+1; min + rand % span`); the native
Geas engine **must replicate this** for its transcripts to match the oracle.
Result: games using `GetRandomInt`/`GetRandomDouble` now produce identical
transcripts across runs.

## Known gaps

- There is no longer any driven game whose win the **harness** cannot reach. The
  two remaining non-wins, The Last Hero and WAKE, are unwinnable *games*.
  *I Contain Multitudes* used to hold this slot, and its recorded diagnosis was
  wrong: the ending was blocked not by a nested `wait{…}` continuation but by the
  synchronous `play sound` immediately before it (see "Synchronous sounds" above).
  It now reaches its full "Ending." credits, `Finished`, 0 errors.
- *WAKE* **cannot** be won. It is the second shipped-broken game in the corpus —
  The Last Hero is the other (its challenge-room `MoveObject` calls all misspell
  the destination) — but the two fail differently: The Last Hero's script still
  reaches a best-effort end state, while WAKE's endgame puzzle is unreachable
  outright. It is a 2013 demo whose final puzzle is unreachable: `Activate MARS`
  does `ChangePOV(RCSR 02)`, but the Reactor Core's `attemptreset` then does
  `MoveObject (player, ReactorPuzzleRoom)` — moving the **`player` object, not
  `game.pov`**. The ten fuel rods land in a room the point of view is not in, and
  `ReactorPuzzleRoom` has no exits, so `shift rod 1a` is permanently out of scope.
  Real Quest 5 resolves scope from `game.pov` exactly as QuestViva does, so this
  is faithful, not an oracle artifact — and the author's own win branch says so:
  *"That did it! … (Placeholder message. Need bunker door to be unlocked)"*.
  Solving it would only unlock `MilitaryBunker`, a room whose description is
  `msg("")`; `SubwayStation`/`TracksSequence`/`BunkerHUB` are orphaned. The
  override drives to the furthest reachable state (the reactor diagnostics) plus
  all optional content, `errors=0`, and documents the rod puzzle's paper solution.
  Distinct from *spondre*, which is `Running` **at its credits** and is a genuine
  win. See `overrides/WAKE.cmd`.
- The formerly-`Running` walkthroughs now win via `overrides/` (Dream Pieces,
  Jacqueline, Carnival of Regrets, Mechanical Bathhouse, The Mouse, Bony King) — see
  `overrides/README.md` for each one's deviation from its raw walkthrough — and the
  six formerly hints-only games plus the PDF-only *The Brutal Murder of Jenny Lee*
  are driven by winning scripts derived from their hints against the game source
  (same table).
- *Eight characters…*: switching on the game board is avoided by its override —
  the three board objects mutually `SwitchOn` each other and QuestViva's
  changed-script dispatch recurses to the 200-depth cap, tripping the 20-error
  breaker (a real QuestViva-vs-Quest incompatibility, like Whitefield's).
