# The game corpora: manifests, fetching, and why the checksums matter

The ADRIFT regression suites here replay real games — 195 ADRIFT 3.7/3.8/3.9/4.0
`.taf` files and 152 ADRIFT 5 `.taf`/`.blorb` files at the time of writing. Those files
are third-party and copyrighted, so they are **not** committed (`adrift4/games/`
and `adrift5/games/` are gitignored) and every suite that needs them SKIPs when
they are absent.

What *is* committed is a checksum manifest per corpus:

- `adrift4/games.manifest.tsv`
- `adrift5/games.manifest.tsv`

and `fetch_games.sh`, which reads them.

## Using it

```sh
cd terps/scarier

sh test/fetch_games.sh                 # verify what is on disk (default)
sh test/fetch_games.sh fetch           # download whatever is still online
sh test/fetch_games.sh manual          # list the rows nobody can download
sh test/fetch_games.sh verify adrift5  # one corpus only

make -f Makefile.headless gamescheck   # same verify, wired into `make test`
make -f Makefile.headless gamesfetch   # same fetch
```

`fetch` writes archives to `test/.game-cache/<host>/` (gitignored) and installs
a game only after its sha256 matches the manifest, so a re-uploaded upstream file
can never quietly replace a corpus file. Existing files are never overwritten.
Override the corpus locations with `V4_GAMES_DIR` / `A5_GAMES_DIR` and the cache
with `SCARIER_GAMES_CACHE`.

`gamescheck` runs as part of `make -f Makefile.headless test` (the default
isolation suite; no FrankenDrift). It fails only on a
**mismatch** — a file that is present but is not the recorded bytes. Missing files
are not an error, because an empty corpus is a legitimate state. Bypass it with
`SCARIER_SKIP_GAMESCHECK=1`.

## Why sha256 and not just a URL list

ADRIFT games are republished in place. Ten files in these corpora (eight ADRIFT 4,
two ADRIFT 5) no longer match the file their own name serves upstream. Unpacking
three of the ADRIFT 4 ones shows what the difference is — same game, same author,
a different build date stamped in the `.taf`, i.e. we hold the *earlier* release:

| corpus file | upstream today | build date, ours vs theirs |
| --- | --- | --- |
| `ADRIFTMaze.taf` | `adrift.co/files/games/adriftmaze.taf` | 01 Mar 2004 vs 02 Mar 2004 |
| `chooseyourown.taf` | `ifarchive.org/…/ChooseYourOwn.taf` | 22 Aug 2004 vs 16 Sep 2004 |
| `TheADRIFTProject.taf` | `adrift.co/files/games/theadriftproject.taf` | 05 Aug 2004 vs 31 Aug 2004 |

The goldens in `adrift4/goldens/` and `adrift5/goldens/` are byte-exact
transcripts. Replaying one of them against a different release of its game
produces a diff that looks exactly like an engine regression. Pinning the hash is
what makes "you have the wrong game file" distinguishable from "you broke the
interpreter", and it is why `fetch` refuses to install a file that does not match
rather than taking the newest thing upstream offers.

## Manifest format

Tab-separated, one row per game file, `#` comment header:

| column | meaning |
| --- | --- |
| `file` | name inside the corpus directory — what the harnesses look up |
| `size` | bytes |
| `sha256` | the pin |
| `source` | URL to download, or `-` if none is known |
| `member` | path inside that archive when `source` is a `.zip`, else `-` |
| `title` | canonical title (ScummVM's ADRIFT detection tables, else adrift.co) |
| `note` | why `source` is `-`, when we know |

Rows were built by hashing the local corpus and content-matching every byte
against a full mirror of adrift.co (619 games, including zip members) and the IF
Archive's `if-archive/games/adrift/` (194 files). A row gets a `source` only when
some upstream file or archive member hashes **identically** — nothing here is a
guess from a matching filename.

Coverage as recorded: 131/195 ADRIFT 4 and 128/152 ADRIFT 5 rows are fetchable.
The remaining 88 are marked `MANUAL`. Their notes distinguish two cases:

- *different release upstream: `<url>` (sha256 …)* — the game is still online, but
  not these bytes. Anyone who finds the original release can drop it in; the
  manifest will confirm it.
- *no online source found* — not on adrift.co and not in the IF Archive's adrift
  directory under any name (many are pre-2005 games that only ever circulated on
  the now-dead delron.org.uk, whose game files did not survive in the Wayback
  Machine).

Two provenance quirks worth knowing:

- `PervertActionCrisis.blorb` is adult-flagged, so adrift.co omits it from the
  game listing entirely. It has an unlisted direct mirror (`…/files/games/pac.blorb`,
  which is what `/cgi/download.cgi?1358` redirects to) and that is what the
  manifest records.
- ScummVM's `detection_tables.h` md5s only the **first 5000 bytes** of a file, so
  its hashes are not comparable with these; it is used here only for titles.

## Adding or refreshing a row

Drop the file in the corpus directory, then append a row: `file`, `size` (bytes),
`sha256`, and a `source` URL you have verified hashes to the same bytes (plus
`member` if it lives in a zip). `sh test/fetch_games.sh verify` is the check.

Anything in a corpus directory that is not a manifest row — a download leftover,
a game's sibling `.xml`/`.txt` metadata — is simply ignored, by both the script
and the harnesses.
