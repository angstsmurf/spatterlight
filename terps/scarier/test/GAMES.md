# The game corpora: manifests, fetching, and why the checksums matter

The ADRIFT regression suites here replay real games — 245 ADRIFT 3.7/3.8/3.9/4.0
`.taf` files and 174 ADRIFT 5 `.taf`/`.blorb` files at the time of writing. Those files
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

`chooseyourown.taf` is the one of those three that has since been resolved: the
22 Aug build survives in the Wayback Machine's copy of `delron.org.uk`, so that
row now has a real `source` and the table entry above is only history. The other
two are still `MANUAL`.

The goldens in `adrift4/goldens/` and `adrift5/goldens/` are byte-exact
transcripts. Replaying one of them against a different release of its game
produces a diff that looks exactly like an engine regression. Pinning the hash is
what makes "you have the wrong game file" distinguishable from "you broke the
interpreter", and it is why `fetch` refuses to install a file that does not match
rather than taking the newest thing upstream offers.

## Alternate builds are pinned as their own rows

Eighteen ADRIFT 5 rows are second (or third) builds of a game that is already in
the corpus. They are not duplicates to be cleaned up — they are the corpus's only
before/after fixtures for *authored-text* drift, which is the one kind of
transcript change our goldens cannot otherwise tell apart from an engine
regression. `MI_v.1.blorb` / `MI_v.3.blorb` is the worked example: the same
walkthrough wins both, and the diff between the two goldens is a bug the author
fixed, visible nowhere else.

| corpus file | build of | what differs |
| --- | --- | --- |
| `AlienDiver_V13.blorb` | `AlienDiver.blorb` (v15) | earlier, 2020-05-15 vs 2020-10-04 |
| `AlyasOfStarhollow.blorb` | `AoS v.4.blorb` | later, 2026-07-16 vs 2026-06-28 |
| `edithscats_upstream.taf` | `edithscats.taf` | same day, eight hours earlier |
| `FBA v.7.blorb` | `FBA v.3c.blorb` | later, 2023-09-21 vs 2023-07-04 |
| `GFS_SourceCode.taf` | `GFS_Frankendrift.blorb` | the ADRIFT source, not a build |
| `GFS_WebRunnerC.blorb` | `GFS_Frankendrift.blorb` | WebRunner build, 2022-05-07 |
| `GFS_Windows.blorb` | `GFS_Frankendrift.blorb` | Windows build, one minute earlier |
| `GrandpaRanchV5.blorb` | `Grandpa_ParserComp_V1.blorb` | the post-comp V5 |
| `GrandpaRanchV5playonline.blorb` | `GrandpaRanchV5.blorb` | play-online build, ten days later |
| `GrandpaV5sourcecode.taf` | `GrandpaRanchV5.blorb` | the ADRIFT source |
| `Halloween.taf` | `Halloween.blorb` | bare `.taf`, 706-byte header, saved 87s earlier |
| `harlot.taf` | `The Drunken Harlot.blorb` | the ADRIFT source, saved 18s earlier |
| `JacarandaJim_2011.blorb` | `JacarandaJim.blorb` | 5.000019 / 2011 vs 5.000029 / 2013 |
| `LMK_IFComp2017.blorb` | `LMKversion3.blorb` | the IFComp 2017 build |
| `MI_v.3.blorb` | `MI_v.1.blorb` | 2023-10-13 rebuild; see the harness comment |
| `Skybreak_1.3.blorb` | `Skybreak.taf` (1.2) | later, 2021-11-14 vs 2020-09-23 |
| `SoC_ifarchive.blorb` | `SoC.blorb` | 5.000026 / Dec 2012 vs 5.000025 / Oct 2012 |
| `TributeReturnToCoS.blorb` | `Tribute.blorb` (`_v2`) | earlier, 2020-03-24 vs 2020-04-03 |

All eighteen are now wired into `run_a5_walkthroughs.sh` as rows of their own
(the harness's 2026-08-11 comment block has the per-row detail). Sixteen replay
their parent's route verbatim, which is itself the finding: the rebuild changed
no mechanics. (`harlot.taf` was the last one in, and only because its parent
`The Drunken Harlot.blorb` had no walkthrough at all until one was derived for
it; the two files then produced byte-identical transcripts.)

Two needed a route of their own. `SoC_ifarchive.blorb` takes three edits because
a closet that is open in the parent build is shut in this one; `AlienDiver_V13`
takes a wholly re-derived 99-command win, and is the one row that pins a
residual divergence (26 hunks, budgeted) rather than closing one.
`Skybreak_1.3.blorb` gets a second row on top of its verbatim one, because the
full Storyteller win does *not* survive the rebuild — character creation shifts
by one talent index and the last four horror stories have to be gathered
elsewhere.

The row that paid for itself is `JacarandaJim_2011.blorb`, whose route is
verbatim but which did not load at all until the loader learned that a blorb's
obfuscation is decided by its iFiction metadata rather than by its payload
layout. That in turn exposed a general engine bug: pre-5.0.20 games serialise
property restrictions without their comparison operator, so every one of them
had been passing those restrictions blind — `get all` in Jacaranda Jim's quarry
was picking up the glacier.

Two of them — `Halloween.taf` and `LMK_IFComp2017.blorb` — are `MANUAL`: adrift.co
serves only `Halloween.blorb`, and its `LMKversion1`/`2`/`3` URLs all return the
same `LMKversion3.blorb` bytes.

Where a game ships its ADRIFT source alongside the built game, the source is
pinned too: `GFS_SourceCode.taf`, `GrandpaV5sourcecode.taf` and `harlot.taf` are
the author's own `.taf`, distributed in the same zip as the `.blorb`, so a transcript
difference can be traced back to what the author actually wrote rather than
inferred from the compiled game. (`Halloween.taf` is a different thing — the
author's working file, saved 87 seconds before the `.blorb` was cut from it and
carrying a 706-byte header the `.blorb` does not have, not a published source
drop.)

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
Archive's `if-archive/games/adrift/` (194 files), plus, for the Spanish-language
ADRIFT games, `if-archive/games/adrift/spanish/` and the Spanish competition
directories the archive's `Master-Index` points at, plus a Wayback Machine sweep
of four dead ADRIFT/AIF hosts (see below). A row gets a `source` only when
some upstream file or archive member hashes **identically** — nothing here is a
guess from a matching filename.

Coverage as recorded: 211/245 ADRIFT 4 and 152/174 ADRIFT 5 rows are fetchable.
The remaining 56 are marked `MANUAL`. Their notes distinguish two cases:

- *different release upstream: `<url>` (sha256 …)* — the game is still online, but
  not these bytes. Anyone who finds the original release can drop it in; the
  manifest will confirm it.
- *no online source found* — not on adrift.co, not in the IF Archive's adrift
  directories under any name, and not in the Wayback sweep below.

Four provenance quirks worth knowing:

- `PervertActionCrisis.blorb` is adult-flagged, so adrift.co omits it from the
  game listing entirely. It has an unlisted direct mirror (`…/files/games/pac.blorb`,
  which is what `/cgi/download.cgi?1358` redirects to) and that is what the
  manifest records. `PAFv1_2.blorb` (*Pervert Action: Future*, the same author's
  252 MB sequel) is the same story — `?1368` and `?1375` both redirect to it, and
  `PAFv1_1.blorb` serves those identical bytes, so there is no v1.1 to pin.
  Neither file is reachable from any page on the site; **the `download.cgi` walk
  is the only way to find them**, which is the argument for re-running it rather
  than trusting the listing.
- Adult-flagged games are not the only unlisted ones — the listing is simply
  incomplete. `croft.taf` (Lara Croft: The Sun Obelisk) and `chicago.taf` are both
  served from `…/files/games/` but appear under no listing page. Walking
  `/cgi/download.cgi?<id>` with `curl -I` for `id` in 1..1200 and collecting the
  `Location:` headers enumerates 617 mirror URLs, ~30 more than the listing knows
  about. adrift.co answers HEAD without a `Content-Length`, so the file has to be
  downloaded to be identified.
- **The dead hosts are not gone — they are in the Wayback Machine.** Most pre-2010
  ADRIFT games only ever circulated on sites that are now offline, and their game
  files were archived along with the pages linking them. Sweeping four of them
  (`delron.org.uk`, `shadowvault.net`, `adrift.org.uk/ftp/games/`, and
  `aifcommunity.org` plus `newsletter.aifcommunity.org`) recovered 36 rows that
  no live mirror has, including original releases that upstream has since
  overwritten. The recipe: query the CDX API per host
  (`web.archive.org/cdx/search/cdx?url=<host>&matchType=prefix&output=text&fl=original,timestamp,statuscode,length&filter=statuscode:200`),
  keep the **largest** snapshot per path — do *not* use `collapse=urlkey`, which
  returns the first one, often a 404 stub — discard anything under ~3 kB (late
  re-crawls of dead paths are ~1350-byte placeholders), and fetch the survivors
  with the `id_` timestamp modifier for raw, unrewritten bytes. Comp collections
  (`…/comps/downloads/ectocomp-2008.zip`, `newsletter.aifcommunity.org/2013minicomp.zip`)
  are worth more than single-game URLs: one zip often carries several rows.
  **Wayback throttles hard** — after roughly 20 downloads in quick succession it
  refuses TCP connections outright for a while, which curl reports in
  milliseconds as `Couldn't connect`, not as an HTTP status. `fetch` therefore
  spaces `web.archive.org` requests out and retries connection-level failures
  with a long backoff; other hosts still fail immediately.
- ScummVM's `detection_tables.h` md5s only the **first 5000 bytes** of a file, so
  its hashes are not comparable with these; it is used here only for titles.

## Adding or refreshing a row

Drop the file in the corpus directory, then append a row: `file`, `size` (bytes),
`sha256`, and a `source` URL you have verified hashes to the same bytes (plus
`member` if it lives in a zip). `sh test/fetch_games.sh verify` is the check.

Anything in a corpus directory that is not a manifest row — a download leftover,
a game's sibling `.xml`/`.txt` metadata — is simply ignored, by both the script
and the harnesses.
