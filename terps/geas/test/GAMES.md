# The game corpora: manifests, fetching, and why the checksums matter

The Quest regression suites here replay real games — 112 Quest 4 `.asl`/`.cas`
files and 89 Quest 5 `.quest` files at the time of writing. Those files are
third-party and copyrighted, so they are **not** committed (`quest4/games/` and
`quest5/games/` are gitignored) and every harness that needs them skips when
they are absent.

What *is* committed is a checksum manifest per corpus:

- `quest4/games.manifest.tsv`
- `quest5/games.manifest.tsv`

and `fetch_games.sh`, which reads them. `quest5/CORPUS.md` is the prose
companion to the Quest 5 manifest — who wrote each game, which ASL version it
declares, which walkthrough drives it.

## Using it

```sh
cd terps/geas/test

sh fetch_games.sh                 # verify what is on disk (default)
sh fetch_games.sh fetch           # download whatever is still online
sh fetch_games.sh manual          # list the rows nobody can download
sh fetch_games.sh verify quest5   # one corpus only

make gamescheck                   # the same verify
make gamesfetch                   # the same fetch
```

`fetch` writes archives to `test/.game-cache/<host>/` (gitignored) and installs a
game only after its sha256 matches the manifest, so a re-uploaded upstream file
can never quietly replace a corpus file. Existing files are never overwritten.
Override the corpus locations with `Q4_GAMES_DIR` / `Q5_GAMES_DIR` and the cache
with `GEAS_GAMES_CACHE`.

`gamescheck` is **not** a prerequisite of `make check`. That suite is
deliberately corpus-free — it is the part worth wiring into CI — and a corpus
check says nothing at all on a machine that has no corpus. Run it when you have
one. It fails only on a **mismatch**, a file that is present but is not the
recorded bytes; missing files are not an error.

## Why sha256 and not just a URL list

Quest games get republished in place, and `textadventures.co.uk` serves every
build of a game from the same URL. The corpus deliberately holds the *older*
build in three cases, all documented in `quest5/CORPUS.md`:

| corpus file | build of | what differs |
| --- | --- | --- |
| `Quest for the Serpent's Eye v1.1.3.quest` | the frozen v1.1.1 copy | prose polish; the frozen script still replays, 16 diff lines |
| `Guttersnipe- The Baleful Backwash (2018 re-release).quest` | the frozen 2018-03-24 copy | renamed objects and a rebuilt trapdoor lock — needs its own script |
| `The Acreage (pub 6.29 revision).quest` | the frozen 5.9.9166 copy | 177 objects against 173, reworked opening — needs its own script |

The last two are the argument for pinning by hash rather than by name: the same
walkthrough does *not* survive the rebuild, so a corpus refreshed by filename
would swap the game under a frozen golden and produce a diff that looks exactly
like an engine regression.

That matters more here than in most corpora, because the Quest 5 goldens in
`quest5/goldens/*.txt` are byte-exact QuestViva transcripts. Pinning the hash is
what makes "you have the wrong game file" distinguishable from "you broke the
interpreter", and it is why `fetch` refuses to install a file that does not
match rather than taking the newest thing upstream offers.

## Manifest format

Tab-separated, one row per game file, `#` comment header:

| column | meaning |
| --- | --- |
| `file` | name inside the corpus directory — what the harnesses look up |
| `size` | bytes |
| `sha256` | the pin |
| `source` | URL to download, or `-` if none is known |
| `member` | path inside that archive when `source` is a `.zip`, else `-` |
| `title` | the game's title, where the corpus records one |
| `note` | when `source` is `-`: where the game can still be got by hand |

Two `quest4` rows carry a directory in `file` (`HauntedHorror/haunted_horror.asl`,
`worldsend/world's end.asl`) because the game's own `.asl` expects to be run from
there; `fetch` creates the subdirectory.

## How the sources were found

A row gets a `source` only when some upstream file or archive member hashes
**identically** to the corpus file — nothing here is a guess from a matching
filename. Coverage as recorded: 105/112 Quest 4 and 74/89 Quest 5 rows are
fetchable, from two places.

**The IF Archive.** `if-archive/games/quest/` is only part of what it holds:
Quest games are also in `games/quest/spanish/`, in `games/springthing/`
(2012, 2013, 2017, 2018, 2020) and in `games/competitionYYYY/quest/<slug>/` for
2001, 2002, 2006, 2007, 2011, 2012 and 2013. The archive's `Master-Index` does
**not** recurse into the competition subdirectories, so those have to be found by
crawling the directory listings at `https://ifarchive.org/indexes/<path>/`. Note
also that ifarchive.org 302s every file URL to `ukrestrict.ifarchive.org`: without
`curl -L` you save a 110-byte "Redirecting to:" stub instead of the game.

**The Wayback Machine, for `textadventures.co.uk`.** Almost every Quest 5 game
was published there and nowhere else, and the site is now behind Cloudflare:
a scripted client gets a 302 to an interstitial and then a 403, so the live site
cannot be fetched at all. The archive can, and the first pass over it needed
**no downloads at all**:

> The CDX API's `digest` field is the **base32-encoded SHA-1 of the archived
> payload**. Hash the local corpus the same way and every archived snapshot of
> every URL can be matched offline, by content, in one pass.

That turns hundreds of megabytes of throttled downloads into a single index
query. The recipe:

```sh
# one page; repeat with &resumeKey=<the last line> until it stops
curl -s 'https://web.archive.org/cdx/search/cdx?url=textadventures.co.uk&matchType=domain'\
'&output=text&fl=original,timestamp,digest,length&filter=statuscode:200'\
'&filter=original:.%2A%5B.%5D%28quest%7Casl%7CASL%7Ccas%7CCAS%7Czip%29%24'\
'&limit=20000&showResumeKey=true'
```

The regex filter must be URL-encoded or curl fails outright, and the resume key
is the last line of the page, preceded by a blank line. Sweeping both
`textadventures.co.uk` and `media.textadventures.co.uk` yields ~25 800 rows.
Where several snapshots share the matching digest the manifest records the
**earliest**, and `http://…:80/` is normalised to `https://…/`. Fetch the bytes
with the `id_` timestamp modifier (`/web/<ts>id_/<url>`) for the raw, unrewritten
payload.

Matching by content rather than by name is what recovers the renamed files:
`A Hobbit Trek.quest` is byte-identical to an archived `Kingdom.quest`, and
`Behind the Door.quest` to `behind_the_door_final.quest`.

**The digest index only sees whole payloads, so it misses everything published
inside an archive** — which is how most Quest 4 games were published, and which
is why the digest sweep alone found only 62 of the 112 Quest 4 rows. Downloading
each of the 131 distinct archived `.zip` payloads once (~250 MB) and hashing its
1889 members recovers 44 more, taking that corpus to 105. Those rows carry the
member path in the `member` column, and `fetch` extracts it with `unzip -p`.
A few of them are not named `.zip` at all: the archive holds a zip under
`…/games/annabel.cas`, so the local file's own extension is no guide.

**Wayback throttles hard**: after roughly 20 downloads in quick succession it
refuses TCP connections outright for a while, which curl reports in milliseconds
as `Couldn't connect`, not as an HTTP status. `fetch` therefore spaces
`web.archive.org` requests out and retries connection-level failures with a long
backoff; other hosts still fail immediately.

The Quest 4 half has been run end to end: `fetch` into an empty directory
installed 105 of its 106 sourced rows byte-exactly, including both nested paths
and every zip member, and turned up the one dead capture below. The Quest 5
half was spot-checked on ten randomly chosen rows rather than fetched whole,
because those files run to hundreds of megabytes.

## The rows that cannot be fetched

The remaining 22 rows — 7 Quest 4, 15 Quest 5 — are marked `MANUAL`. None of them
is lost: **every one still has a game page on `textadventures.co.uk`**, and each
note opens with it:

- *browser download only: `<page url>`* — the game's page on the site, verified
  to exist. It cannot be scripted, so `fetch` will never pick these up (below).
  Some carry a parenthetical because the page is not findable from the corpus
  filename: `Beyond Exile 2.2.cas` is filed as *1.4*, `Great Depression Man
  (Middle Class).quest` as *Great Depression Day simulator*, and `Iron John` is
  published under its Greek title, which leaves its page URL with no slug at all.

A note may then add what the archive has, which is never the corpus bytes:

- *different release archived: `<url>` (sha256 …)* — a different build of the
  same game, downloaded and checked to be whole before the note was written.
  Useful for comparison; it will not satisfy `verify`.
- *archived but not replayable: `<url>`* — one row, `ThunderClan mystery 1.asl`.
  The CDX index lists a 200 capture whose digest is exactly our file, but every
  playback URL for it 404s, so the bytes are indexed and not servable. Worth
  retrying some day; the manifest records where.

**Why these are browser-only.** The site's game paths are all behind Cloudflare —
`/games/view/…`, `/games/download/…` and everything on `media.textadventures.co.uk`
answer a scripted client with a 302 to an interstitial and then a 403, browser
user-agent or not — and `robots.txt` disallows `/games/download/` and
`/games/play/` outright, so automating them is not something this repo will do.
The Wayback Machine is no way around it either: all ~20 000 archived
`/games/download/` captures are 302 redirects with no payload behind them.

What *is* fetchable is `https://textadventures.co.uk/sitemap` — 682 KB of XML,
unchallenged, listing every `/games/view/<guid>/<slug>` page on the site. That is
where the page URLs above came from: match the corpus file's title against the
slugs, then confirm the guid against the archived copy of the page (its `<title>`
is `"<Game> - Play online at textadventures.co.uk"`). The XML declares
`encoding="utf-16"` and is actually utf-8; decode it as utf-8 or it will not
parse.

That checking of the *different release* URLs was not a formality: two things in
the archive look like a whole game and are not.

- **Wayback truncates a capture at exactly 1 MiB.** A truncated `.quest` still
  starts with `PK`, so a magic-byte check passes it; opening it as a zip and
  running `testzip` is what catches it. Both `Frankenstein's Tomb.quest` and
  `The Deer Trail.quest` have exactly one archived snapshot each and both are
  1 048 576-byte truncations of a multi-megabyte game.
- **Cloudflare interstitials were archived as if they were files.** They are
  HTML, so a payload that begins `<html` or `<!doctype` is discarded.

## Adding or refreshing a row

Drop the file in the corpus directory, then append a row: `file`, `size` (bytes),
`sha256`, and a `source` URL you have verified hashes to the same bytes (plus
`member` if it lives in a zip). `sh fetch_games.sh verify` is the check.

Anything in a corpus directory that is not a manifest row — a download leftover,
a game's sibling `.txt` metadata, the `_not-quest5/` set-aside — is simply
ignored, by both the script and the harnesses.
