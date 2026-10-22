# TADS test-game corpora

Two corpora, both pinned by sha256 in a committed manifest. The games themselves
are third-party and are never committed (`*/games/` is gitignored).

| corpus     | manifest                        | rows  | what it is |
|------------|---------------------------------|-------|------------|
| `htmltads` | `htmltads/games.manifest.tsv`   | 306   | 235 released TADS 2 / TADS 3 games that use HTML TADS features (206 .gam, 29 .t3), plus their resource and media files, about 560 MB |
| `xtads`    | `xtads/games.manifest.tsv`      | 2,513 | XTads's `testgames/` folder: small feature tests, sources and builds, about 43 MB |

`htmltads/CENSUS.tsv` has one line per game: kind, TADS version, category,
media resource count and types, external media count, number of `\H+`
switches, and a count of each HTML tag the game's strings use.

## Usage

    sh fetch_games.sh                  # verify both corpora (the default)
    sh fetch_games.sh fetch            # download what is missing
    sh fetch_games.sh manual           # list rows that have no online source
    sh fetch_games.sh fetch htmltads   # just one corpus
    XTADS_SOURCE_DIR=~/Downloads/testgames sh fetch_games.sh fetch xtads

Verify fails on a sha256 mismatch but not on a missing file. fetch never
installs a file whose hash doesn't match. The hash is of the extracted
file, not the archive, so any archive that contains an identical copy is
a valid source. Downloaded archives are cached in `.game-cache`
(`TADS_GAMES_CACHE`).

Manifest columns: `file size sha256 source member title note`. `source` is a
URL, or `-` when there is no online copy. `member` is the path inside the
zip, with `|` separating nested zips.

The pins matter because TADS games get re-released in place on the IF Archive.
The old build moves to `games/tads/old/` under a new name and the new build
takes over the old name, so a URL alone does not identify a build.

## Walkthroughs

`htmltads/goldens/<stem>_walkthrough.txt` holds command scripts for 45 of
the corpus games, one command per line. `<stem>` is the census path with `/`
as `__` and without `.gam`/`.t3`. They were derived from the solutions on
David Welbourn's Key & Compass (`plover.net/~davidw/sol`); the first line of
each script names its page. `run_traces.py --walk` types them in place of
the default script. No expected transcripts are stored: compare the traces
of two builds instead.

## What counts as an HTML TADS game

A game is in the corpus if it falls into at least one of these categories (the
`note` column and CENSUS give the category):

- **markup** (126 games): a TADS 2 game that switches on HTML mode with `\H+`.
  These games have no media, but they use tags, tables, banners and so on.
- **embedded** (85+): images or sounds are stored as HTMLRES resources in the
  .gam, or as MRES resources in the .t3.
- **rsN** (11+): the media is in separate `.rs0`/`.rs1` or `.3r0` resource
  files, which are listed as companion rows in the same directory.
- **external** (6+): the game loads loose image or sound files by relative
  path. These files are rows too, in the directory layout the game expects.

A T3 game with no media at all is excluded. Every T3 game can use HTML, so
being T3 is not enough to qualify.

### Choosing among builds

1. Files are deduplicated by sha256.
2. They are grouped by IFID. When there is no IFID, they are grouped by the
   file stem with any version suffix removed. A version suffix has a
   separator, a `v`, or two digits, so `exhibit21` counts as version 2.1. A
   single bare digit means a sequel instead (`gift3`, `ptbad5`, `speed5`).
   Bundled and unbundled builds are grouped separately, so a game can
   appear as both `foo/foo.gam` + `foo.rs0` and `foo_bundled.gam`.
3. Within a group, the source rank decides:
   `games/tads` < `games/tads/old` < competition directories < nested zips <
   old competitions < IFComp bundles < local copies. Ties go to the highest
   version number and then to the smallest archive.

## Sources

- **IF Archive** (`ifarchive.org`). The scan crawled `games/tads`, the
  competition directories, the mini-comps and the yearly IFComp bundles. No
  row needs a 1–1.6 GB IFComp bundle. The members were range-read out of those
  bundles while scanning.
- **Wayback copies of delron.org.uk/AIF** (8 games). The old adult-IF
  archive is offline. Its 2009–2010 and 2023 captures still hold the real
  zips, while the 2024 captures are 1.3 KB stubs.
- **Local copies** (53 game rows with source `-`). These games aren't online
  anywhere we found. They came from the AIF archive dump
  ("the AIF motherload" on the external disk), the adult-IF minicomp folders
  and a few RAR files. `fetch_games.sh manual` lists them, and the `note`
  column names the container each one came from.

### IFDB cross-check

IFDB's API was used for two checks:

- A tag search (`system:TADS tag:graphics`, `…sound`) found no game the
  scan had missed.
- An IFID lookup (`viewgame?json&ifid=`) matched 151 of the 235 games and
  supplied their titles.

The remaining titles come from GameInfo, the Master-Index or a hand-made
override table. *Love Lost: Final Week* is on IFDB, but no copy of it
turned up.

## Rebuilding the manifest

The crawl, the scan and the selection scripts live outside the repo, in
`.work/` (a symlink to `/Volumes/Extern/htmltads-work`; the scan data is
several GB):

| step | script |
|------|--------|
| list archives, range-read zips | `bin/crawl.py`, `bin/ziplist.py`, `bin/zipget.py` (Zip64-aware, copes with servers that ignore Range) |
| download and unpack candidates | `bin/getall.sh`, `bin/get1.sh`, `bin/nest1.sh` |
| scan each .gam/.t3 (resources, HTML tags, GameInfo, IFID) | `bin/tadsscan.py` |
| group and choose builds | `bin/pick.py` → `sel/chosen_full.json` |
| IFDB titles | `bin/ifdbtitles.py` → `ifdb/titles.tsv`; overrides in `ifdb/title_overrides.tsv` |
| write manifest and census | `bin/manifest.py . sel/out` |
| install the local-only rows | `bin/installlocal.py . MANIFEST GAMESDIR` |
| xtads manifest | `bin/xtmanifest.py SRCDIR SCAN.jsonl OUT.tsv` |

Run the scripts with `python3 -I`, from `.work`. Without `-I`, Python puts
the script's directory first on the import path. Use absolute paths for
anything outside `.work`, because `..` there resolves on the external disk.
