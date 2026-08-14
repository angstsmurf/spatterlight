# Quest 5 (`.aslx`/`.quest`) tests

The aslx engine's test tree. Mirrors `../quest4/`, but the shape of the testing
is different: Quest 5 has a runnable ground truth. QuestViva, the cross-platform
.NET port of the real Quest 5 engine, is built into a headless driver (`qvh`)
and its transcripts *are* the goldens, so a corpus game is checked by
byte-diffing our output against what real Quest prints rather than by looking
for a win marker.

## Layout

- `fixtures/` — hand-written `.aslx`/`.quest` files. Ours, and committed. The
  loader and runtime tests open them by name; a few are for driving by hand
  (see below).
- `goldens/` — the answer keys, `<Game>.cmd` (the frozen command script) plus
  `<Game>.txt` (the QuestViva transcript it produces), 86 games. Both halves are
  committed; the `.cmd` is what everything replays, so a diff here is a change
  in what "real Quest does".
- `harness/` — the native harnesses, plus `harness/oracle/`, the QuestViva
  driver that produces the goldens. Binaries build in `harness/` and are
  gitignored.
- `games/` — the `.quest` corpus: 88 game files (86 wired entries, the rest
  duplicate releases) plus a `MANIFEST.md` recording each one's author, ASL
  version and walkthrough. **Untracked** (`../.gitignore`), like
  `quest4/games/` — third-party game data is never committed. The oracle
  scripts use it automatically when it is here, and fall back to
  `~/Downloads/Quest 5 games` when it is not, so a machine that keeps the
  corpus elsewhere still works (or set `GAMES=`).
- `downloaded/` — the same arrangement for the third-party walkthrough
  documents `extract_walkthrough.py` reads, falling back to
  `~/Downloads/Quest 5 walkthroughs` (or set `WALKS=`). Only the 9
  extractor-driven corpus rows need them; the other 77 are driven by the
  curated scripts in `harness/oracle/overrides/`, which are committed.

## Automatic tests (`make check`, from `../`)

| binary | covers |
| --- | --- |
| `harness/aslx_loader_test` | the `.aslx`/`.quest` loader — XML, includes, the zip container |
| `harness/aslx_runtime_test` | the script/expression runtime, save/restore of native state |
| `harness/aslxglk_link_tests` | the Glk frontend's HTML link parser (`link_action`, file-local in `aslxglk.cc`) |

All three open their fixtures as `../fixtures/<name>`, so run them from
`harness/`:

```sh
cd harness && ./aslx_runtime_test
```

`aslxglk_link_tests` exists because CheapGlk reports no hyperlink support, so
the smoke harness below never registers or clicks a link and cannot reach that
code at all.

## Corpus regression (`harness/oracle/check_golden.sh`)

Replays every frozen `goldens/<Game>.cmd` through QuestViva and diffs the result
against `goldens/<Game>.txt`. A FAIL means the *oracle* drifted — a QuestViva
upstream change, a .NET/RNG regression, a `Program.cs` edit — not that our
engine is wrong. It also cross-checks `oracle/corpus.tsv` against the goldens in
both directions, so a driven row with no golden, or a golden with no row, is an
error rather than a silent gap.

```sh
cd harness/oracle
./build.sh                     # clone + build QuestViva (outside the repo)
./check_golden.sh              # the whole corpus, in parallel
./check_golden.sh "hobbit"     # just the matching games
./update_golden.sh             # re-freeze after an INTENDED change
```

`oracle/README.md` is the long-form document: how a game gets wired, what the
extractor modes mean, and what each curated override in `oracle/overrides/`
deviates from.

## Native replay (`harness/aslx_replay`)

The other half of the same regression: our engine against the same frozen
scripts. Built on demand, since it needs the corpus.

```sh
make quest5/harness/aslx_replay                        # from ../
cd quest5/harness
ASLX_CORE=../../../quest5/aslx-core ./aslx_replay \
    "<game.quest>" "../goldens/<Game>.cmd" | diff "../goldens/<Game>.txt" -
```

`harness/aslx_linkcapture` replays a script the same way but prints the visible
*text* of the command link each step clicks, and
`harness/oracle/save_compat.sh` cross-tests `.quest-save` compatibility in both
directions (we load a save QuestViva wrote; QuestViva loads a save we wrote),
using the first half of each golden script as the setup.

## Glk frontend smoke (`harness/aslxglk_smoke`)

The real `quest5/aslxglk.cc` against the in-repo CheapGlk, driven by piping
commands to stdin. Built on demand; useful for anything that only goes wrong
once the frontend is in the loop.

## Manual fixtures (not run by any script)

A few things only exist on a graphical Glk host, so no in-repo harness can
assert on them — CheapGlk has neither graphics nor hyperlinks. These are driven
**by hand in Spatterlight**, and each carries a header comment listing the
commands to type and what to look for.

| fixture | checks |
|---|---|
| `fixtures/framepicture.aslx` | the frame-picture band: opens, sizes itself to the picture without upscaling, and — via **both** the `SetPanelContents` and Quest 5.0 `RunScript` channels — closes again leaving no gap |
| `fixtures/timertest.aslx` | the prompt-retract/preload path in `aslxglk`'s `read_line` timer arm: a recurring wall-clock timer printing while the parser prompt is up must leave no stranded `> ` above the tick lines |

They are kept here rather than in a scratch directory because the paths they
cover are otherwise reachable only by finding a shipped game that happens to use
them: the picture-clear path went unverified through two commits for exactly
that reason.
