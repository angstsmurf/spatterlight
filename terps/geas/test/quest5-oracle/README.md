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

- .NET 9 SDK (`brew install dotnet` — this Mac has 9.0.100, arm64).
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
dotnet bin/Release/net9.0/qvh.dll <game.quest|game.aslx> [command-script.txt]
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
**17 games driven** — **9 `Finished`** (genuine wins), **7 `Running`**
(walkthrough exhausts or drifts from the game version at the tail, which the
oracle faithfully records), **1 `Wedged`** (Whitefield — see below) — plus **6
hints-only** (Q&A/prose, no linear script). See [[quest5-corpus]].

### `Wedged`: telling the error breaker apart from a real win

QuestViva ends the game *itself* once script errors reach `MaxScriptErrors` (20,
`WorldModel.cs`) — a "session is unrecoverably wedged" circuit-breaker — and this
sets `State=Finished` exactly like a real `finish`. The harness counts `LogError`
events (1:1 with the trip) and reports that case as **`Wedged`**, not `Finished`,
so a walkthrough that drifted into a run of failing turnscripts is not miscounted
as a win. *Whitefield Academy of Witchcraft* wedges this way: after the
walkthrough drifts, the CoreGrid map turnscript throws
`DictionaryItem(coordinates,...)` (missing x/y/z) every turn and trips the
breaker at 20 errors. All genuinely-finished games show `ERR=0`.

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
- Some driven games end `Running` because the walkthrough drifts from the game
  version at the tail (e.g. *I Contain Multitudes*, whose author notes its
  time-based events break Quest's own walkthrough runner). Transcripts are still
  deterministic; they just don't reach a formal win.
- *Dream Pieces* plays its whole wordplay puzzle (with the `new game` preamble)
  but ends `Running`: its published walkthrough's last step is `unlock door`,
  which the game rejects with "Try unlocking the lock." Sending `unlock lock`
  instead wins (50/50, "Awake Gamer") — a walkthrough bug, left as faithful
  drift rather than hand-patched into the command stream.
- *Escape From the Mechanical Bathhouse* is a real-time timed game (a matching
  puzzle with a 50-second wall-clock limit); it always drifts at the tail
  regardless of the script, and is reported `Running`.
- *The Brutal Murder of Jenny Lee* has a PDF-only walkthrough (no `.txt`).
