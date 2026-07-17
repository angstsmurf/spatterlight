# Quest 5 ground-truth oracle (`qvh`)

A headless driver for **QuestViva** — the cross-platform .NET port of the Quest 5
engine (`github.com/textadventures/quest`, branch `main`, aka "Quest Viva"). It
loads a real `.quest`/`.aslx` game, replays a command script, and prints a
normalised plain-text transcript.

This is the reference oracle for the planned native Geas ASLX engine
(`terps/geas/TODO-quest5.md`, milestone 6): once that engine exists, diff its
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

`corpus.tsv` is the curated manifest: each of the 24 games with a walkthrough
mapped to its walkthrough file and an extractor mode. It exists because filename
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
**18 games driven** — **16 `Finished`** (genuine wins), **1 `Running`** (I Contain
Multitudes — its ending sits behind a nested-wait continuation the harness cannot
pump; its author warns its time-based events break Quest's own walkthrough runner),
**1 `Wedged`** (Whitefield — a genuine QuestViva-vs-Quest incompatibility, see
below) — plus **6 hints-only** (Q&A/prose, no linear script). Seven of those wins are
driven by curated `overrides/` (see next section); the other nine by the raw
walkthrough via the extractor. See [[quest5-corpus]]. (Dracula is a special case: its
only walkthrough is for the *original 1986 CRL* game, not this 2014 remake, so its
override heavily adapts that solution to the remake's parser — see its header.)

### Curated overrides (`overrides/`)

Some walkthroughs do not reach the ending when run verbatim — a typo the game
rejects, one of several mutually-exclusive endings, a nav error vs the release map,
or an RNG-/timer-specific solve. For each such game, `overrides/<Game>.cmd` is a
hand-authored winning script (with a `#`-comment header documenting every deviation
from the raw walkthrough); `run_corpus.sh` uses it verbatim in place of the
extractor+preamble. `overrides/README.md` tabulates why each of the eight exists
(six now win; I Contain Multitudes and Whitefield are best-effort, documented above
and below). The nine games whose raw walkthroughs already win have no override.

### Golden baseline (committed regression)

`golden/` holds the frozen answer key: for each of the 18 driven games, the exact
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

*Whitefield Academy of Witchcraft* is a real wedge: the *mandatory* Grislewood snare
double-teleports the player into "Inescapable Cage," a room reachable only by that
teleport that never gets map coordinates, so its enter-script throws
`DictionaryItem(coordinates,"x")` ~19× *through `RunScriptAsync`* in one transition
and trips the breaker. In real Quest an enter-script error is non-fatal, so the game
is winnable there — under this oracle's breaker it is not. This is a genuine
QuestViva-vs-Quest incompatibility, not a walkthrough error. Genuinely-finished
games show `ERR=0` except Bony King (`ERR=31`, explained above).

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

- Six games have **hints-only** walkthroughs (Q&A or prose — *Night House*,
  *Poppet*, *What Once Was*, *Hawk the Hunter*, *Eight characters…*, *Quest for
  the Serpent's Eye*): no mechanically extractable linear command script, so
  they carry a `hints` mode in `corpus.tsv` and are reported as skipped.
- *I Contain Multitudes* is the one driven game that does not reach a win. It runs
  cleanly (0 errors) to its final story beat, but the ending `finish` is behind a
  nested `wait{…wait{…SetTimeout(7)…}}` continuation inside a menu response that the
  harness does not pump — so the ending timer is never even created. Its author
  warns its time-based events break Quest's own walkthrough runner too. Left
  `Running`; transcript is still deterministic.
- The other formerly-`Running` walkthroughs now win via `overrides/` (Dream Pieces,
  Jacqueline, Carnival of Regrets, Mechanical Bathhouse, The Mouse, Bony King) — see
  `overrides/README.md` for each one's deviation from its raw walkthrough.
- *The Brutal Murder of Jenny Lee* has a PDF-only walkthrough (no `.txt`).
