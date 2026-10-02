# Quest 5 corpus — decisions and facts

The per-game data lives elsewhere and is not repeated here:

- `games.manifest.tsv` — every file in the gitignored `games/`, with its sha256
  pin and download source (`../GAMES.md` explains fetching and verifying).
- `harness/oracle/corpus.tsv` — the wired rows the oracle drives.
- `harness/oracle/overrides/README.md` — how each game's route was derived and
  where it ends.

This file keeps what those don't: why some files are in `games/` but not wired,
why two games are wired twice, and what has already been learned about sourcing
games and walkthroughs.

See `~/Downloads/Quest 5 walkthroughs/README.md` for walkthrough sources, the
Wayback workaround, folder layout and known data bugs. That folder's top level
holds only Quest 5 walkthroughs; `_pre-quest5/` (Quest 1–4) and `_not-quest5/`
(QuestJS) are set aside.

## Corrections to `terps/question/TODO-quest5.md`

1. **§7's "free regression scripts" assumption is false.** It says "many `.quest` games ship `<walkthrough>` elements". **None of the games here do.** It is an editor-side testing feature authors essentially never populate. Regression scripts must come from the external walkthroughs harvested here.
2. **§7 names textadventures.co.uk as the corpus source.** Direct access is Cloudflare-blocked (403 to any scripted client), **but the Wayback Machine is not** — see the README for the working recipe. 11 games here exist nowhere else. The corpus is therefore **not size-capped**: IFDB's `system:Quest` filter returns **631 Quest games**, any of which can be pulled the same way.
3. **`The_House_on_Highfield_Lane` is QuestJS ("Quest 6", pure JavaScript), not Quest 5.** No `game.aslx`. TODO-quest5.md explicitly rejects QuestJS, so the planned engine can never run it. Moved to `_not-quest5/`.

## Corpus facts

- **Walkthrough coverage is believed complete.** Of the 631 Quest games on IFDB, exactly 35 have a walkthrough link. Cross-referencing Welbourn's full index (1340 entries) and `if-archive/solutions/` (499 files) against all 631 titles found **zero** additional. Don't re-hunt.
- **plover.net's `idx_quest.html` cannot prune by version string alone**: a bare `Quest` (no number) means "version unrecorded", not "pre-5" — 15 of the first 49 verified Quest 5 games were labelled bare `Quest` there. The pre-5 walkthrough split was done by triangulating bare-`Quest` + index-year (2001–2009) + `.asl`/`.cas` format, with ASL headers as ground truth.
- **`_not-quest5/`** (games folder) holds Quest 3/4 `.asl`/`.cas` games (which belong to the *existing* Question engine) and the QuestJS outlier.
- Every `.quest` here is verified: a real zip containing `game.aslx`. Three deliberate
  version duplicates:
  - `Quest for the Serpent's Eye v1.1.3.quest` (newer release, kept alongside the frozen
    v1.1.1 corpus copy; the frozen override replays on it to THE END with errors=0 —
    16 prose-polish diff lines, same path). NOT separately wired.
  - `Guttersnipe- The Baleful Backwash (2018 re-release).quest` (the author's later build,
    Quest 5.8.6794.18055 / 2018-08-12, shipped as `speakeasy5.quest`, cover
    `backwash512.png`; the frozen corpus copy is Quest 5.7.6404.15496 / 2018-03-24, cover
    `backwashs.png`). Unlike Serpent's Eye the frozen script does NOT replay on it —
    renamed objects and a rebuilt trapdoor lock break the poker key — so it IS separately
    wired, with its own override + golden.
  `speakeasy5.quest` is still present as well and is **byte-identical** to the re-release
  copy (same md5) — a leftover of the rename, not a third build. It is deliberately not
  a corpus row.
  - `The Acreage (pub 6.29 revision).quest` (the author's later build, Quest
    5.10.9635.26171 against the frozen copy's 5.9.9166.36226, 177 objects against 173,
    new opening and much reworded prose). Like Baleful Backwash the frozen script does
    NOT replay on it — Desmond's conversation menu lost a topic and renumbered, and an
    out-of-range menu answer silently swallows the rest of the run — so it IS separately
    wired, with its own override + golden.

  Three games were renamed to their in-game titles when they were wired, since the corpus
  row and the golden are keyed on the file's basename: `behind_the_door_final.quest` →
  `Behind the Door.quest`, `Park.quest` → `All Visitors Welcome.quest`, and
  `The eye of Mandival..quest` → `The Eye of Mandival.quest`.
