# What the oracle found

Every game in `../run_walkthroughs.sh` replayed through both engines from the
same command script at the same seed (`GEAS_SEED=1` / `QVH_SEED=1`), diffed
after the normalisation described in `README.md`:

```
40/111 identical, 68 differ, 3 skipped/failed
```

The three that are not compared are World's End (its transcript is a
`--save-scum` replay, so it is a function of the runner rather than the game)
and two games QuestViva itself cannot run — see *QuestViva's own defects*.

`triage.py` attributes every one of the 20 348 diff lines to a cause, and
`firstdiff.py` prints each game's *first* divergence — the only one with a cause
of its own, since everything after it is cascade.  Run `python3 triage.py` for
the current numbers; the "first" column below is the third one it prints, and
sums to the 68 games that differ.

Once a game's two transcripts stop describing the same world — a lookup that
succeeds on one side only, a timer a turn out — nothing after that point is
independent evidence, so those runs are credited whole to the divergence that
parted them.  Those are the `desync:` rows; between them they account for
16 670 of the 20 348 lines, with a further 2 761 after a QuestViva internal
error, and nine games (all of finding 14's) for 10 489.

| divergence | diff lines | games | first in |
| --- | ---: | ---: | ---: |
| desync: 14, names matched too loosely | 10489 | 9 | 1 |
| desync: 22, lookup not scoped per verb | 3022 | 1 | – |
| after a QuestViva internal error | 2761 | 1 | – |
| desync: 45, `open` marks a container seen | 1010 | 1 | – |
| desync: timer armed after the turn's tick | 989 | 5 | 1 |
| desync: 47, `exec` re-runs the turn scripts | 615 | 1 | – |
| exits: out and dirs split | 369 | 6 | 5 |
| desync: 37, an object name is trimmed | 338 | 1 | – |
| take: implied remove (QV defect) | 116 | 14 | 4 |
| desync: 54, no pre-2.80 room display | 104 | 3 | 1 |
| cp1252 text not transcoded | 92 | 13 | 9 |
| 14, names matched too loosely | 75 | 8 | 5 |
| container contents not listed | 74 | 18 | 15 |
| desync: 52, `set interval` applies a cycle late | 46 | 1 | – |
| disambiguation menu wording | 39 | 9 | 6 |
| desync: 53, place alias not version-gated | 37 | 1 | – |
| desync: 72, `action <drop>` runs where Quest drops | 20 | 1 | – |
| QuestViva defect | 18 | 7 | 2 |
| 56, type applied out of source order | 16 | 1 | 1 |
| 57, timers tick at the wait | 14 | 2 | 1 |
| 22, lookup not scoped per verb | 12 | 3 | – |
| room display: no pre-2.80 path | 10 | 4 | – |
| picture caption printed as text | 10 | 3 | 3 |
| 65, inventory order below 2.80 | 10 | 4 | 2 |
| tag value truncated at `;` | 10 | 1 | 1 |
| menu skipped, choice typed as a command | 6 | 2 | – |
| `error <defaultake>` is spelt with one t | 6 | 1 | – |
| `oops`/`the` answered where Quest is silent | 5 | 1 | 1 |
| 62, pre-280 `look` name lookup | 4 | 1 | 1 |
| 47, `exec` runs the turn scripts again | 4 | 2 | – |
| command echo realigned by a shifted cut-scene | 4 | 1 | 1 |
| `<ERROR>` not produced at load time (known) | 4 | 2 | 1 |
| 63, `place locked` exit dropped | 4 | 1 | 1 |
| 64, `\|w` does not end the line | 3 | 1 | – |
| exits: 4.10 single line | 3 | 2 | 1 |
| 37, `use on <name >` not trimmed | 2 | 1 | 1 |
| `quest.formatobjects`: flat comma list | 2 | 1 | – |
| 43, `property <obj; name=value>` true in geas | 1 | 1 | 1 |
| 46, empty `place <>` dropped | 1 | 1 | 1 |
| 40, `outputoff <>` obeyed | 1 | 1 | 1 |
| 15, `quest.objects` joins with `" and "` | 1 | 1 | – |
| 42, `msg nospeak` dropped | 1 | 1 | 1 |

Each finding below cites the line in both engines, so it can be checked
without re-running anything.

## Confirmed geas bugs

### 1. The default room line has no full stop

**Fixed.**  `geas-runner.cc:2091-2096` now prints the stop, unconditionally, so
One Robot really does say `You are in Cheese Blvd..`.  The `indescription`
branch above it was already right: a trailing colon takes the room name and a
stop, anything else is printed verbatim (`V4Game.Part2.cs:3746-3759`), which is
why King's Quest V's `indescription <You are in the>` still ends bare.

`geas-runner.cc:2001-2002` prints `You are in <name>`; Quest prints
`You are in <name>.` (`V4Game.Part2.cs:3764`, and the pre-2.80 path at 1817).

The single largest divergence in the corpus, and the one that makes 49 games'
diffs unreadable past their first room.

The full stop is unconditional, not "added if missing": One Robot's
`define room <Cheese Blvd.>` comes out of Quest as `You are in Cheese Blvd..`,
the corpus's only double stop.

### 2. A bare `wait` prints nothing

`st_wait` (`geas-runner.cc:6176-6193`) passes the message to
`gi->wait_keypress (tok)`, which prints nothing when `tok` is empty.  Quest's
`ExecuteWaitAsync` (`V4Game.cs:6103-6118`) prints `Press a key to continue...`
below ASL 4.10, and the overridable `defaultwait` from 4.10 on.

```
> ping            (a command that does: msg <before> / wait / msg <after>)
geas:  before                              qv4:  before
       after                                     Press a key to continue...
                                                 after
```

### 3. geas invents a `wait` *player command* that Quest 4 does not have

`geas-runner.cc:4369-4375` answers a typed `wait` with the `defaultwait` text.
Quest has no such command — `wait` exists only as a script statement
(`V4Game.Part2.cs:6037-6040`), so a bare `wait` at the prompt is a
`badcommand`:

```
> wait
geas:  Time passes...
qv4:   I don't understand your command. Type HELP for a list of valid commands.
```

Both corpus games whose walkthrough types `wait` (Adventure, Ghost Light)
define their own `command <wait>`, so this shows up only under probe.

### 4. Error-message table gaps

**Fixed**, and not by patching the five bullets one at a time: geas now has
Quest's mechanism.  `error_table[]` (`geas-runner.cc:1274-1332`) is Quest's
38-entry array in its `PlayerError` order, with Quest's texts;
`init_error_messages` (`:1352-1402`) copies it and then walks the game block's
`error <name; text>` lines over the top of it, and `display_error` (`:1404`)
reads the result.

Three quirks of `SetUpUserDefinedPlayerErrors` came with it, each pinned by a
fixture (`errornames.asl` at 4.10, `errornames280.asl` at 2.80, both matching
qv4 line for line):

* the name is compared **untrimmed and case-sensitively**, because
  `GetParameter` does no trimming (`V4Game.cs:1858-1893`);
* **an unrecognised name leaves `currentError` at 0, so the line redefines
  `badcommand`** — which is exactly what a game that writes `defaulttake`, the
  spelling geas's own call sites use, gets (`asl_error_index`, `:1334-1350`);
* the loop **overwrites as it goes**, so the last matching line wins where geas
  returned on the first.

Two names are version-gated: `defaultake` is `BadTake` at 2.80 and below and
`DefaultTake` above it, and `badexamine` is only recognised from 3.10
(`V4Game.Part2.cs:1466-1517`).  A `defaultlook` line also sets
`DefaultExamine`, unless a `defaultexamine` line came first.

`defaultverb` has no Quest counterpart and could not simply be dropped, since
geas's generic verb dispatch needs a refusal: it is now a literal in
`display_error`, and a game's `error <defaultverb; …>` line lands on
`badcommand` like any other unknown name.

`geas-runner.cc:1324-1364` against `SetDefaultPlayerErrorMessages`
(`V4Game.Part2.cs:426-470`) and the name switch at `V4Game.Part2.cs:1412-1611`:

* **Missing entirely**: `badput`, `cantput`, `alreadyput`, `alreadytaken`.  A
  game that overrides one of these is ignored by geas.
* **`defaulttake` is spelled wrong.**  Quest's own name is `defaultake`
  (`V4Game.Part2.cs:1472`) — a typo in the VB6 original that Quest is stuck
  with.  A game writing the name Quest honours is invisible to geas, and vice
  versa.
* **`defaultput` carries the wrong text.**  Quest's is `Done.`; geas's is
  `You can't put #quest.error.article# there.`, which is Quest's *CantPut*.
* **`defaultwait` carries the wrong text**: `Time passes...` against Quest's
  `Press a key to continue...`.
* geas has a `defaultverb` entry with no counterpart in Quest's table.

### 5. `badthing` / `baditem` are hard-coded

**Fixed.**  The unresolved-`#@var#` path now calls
`display_error (asl_version_ >= 391 ? "badthing" : "baditem")`
(`geas-runner.cc:3398`), matching `MatchCommand`'s own version split
(`V4Game.Part2.cs:2490-2500`), and the take handler's already-held branch picks
between `alreadytaken`, `badthing` and `baditem` at 4.10 / 3.91 / below
(`geas-runner.cc:4263-4265`), which is `ExecTake`'s ladder
(ibid. 5126-5170).  Both go through the overridable table from finding 4.

`geas-runner.cc:3190` prints a fixed `You don't see any <noun>.` when the noun
does not resolve.  Quest returns -1 from `Disambiguate`
(`V4Game.cs:4859-4861`) and leaves the caller to print the overridable
`PlayerError.BadThing` (`I can't see that here.`) or `BadItem`
(`I can't see that anywhere.`).  So geas both prints the wrong wording and
ignores a game's own override.  10 lines across 6 games.

### 6. Inventory is a bare list, not Quest's prose sentence

**Fixed.**  `geas-runner.cc:4585-4628` builds Quest's sentence: entries are
`prefix + |b name |xb [+ " " + suffix]`, joined with `", "`, the last comma
becomes `" and"`, and the whole thing is printed as
`"You are carrying:|n" + … + "."`.

The capital is the part worth writing down.  Quest applies
`UCase(Left(invList, 1))` to the assembled string *including its markup*
(`V4Game.Part2.cs:4538`), so when the first entry has no prefix the character
it reaches is the `|` of that entry's own bold tag and the name is left exactly
as the game spelt it — `You are carrying:` / `coin.`, not `Coin.`.  Emitting
the `|b…|xb` geas had been leaving out is what makes `pcase` land in the same
place.  Below 2.80 Quest lists bare items with neither bold nor affixes, and
there the capital does reach the name.

What is **not** fixed is which things are listed, and in what order: that is
finding 65.

Quest builds one sentence with each object's prefix and suffix, upper-cases the
first letter, joins with `", "` and `" and "`, and ends with a full stop
(`V4Game.Part2.cs:4496-4557`); the empty case is
`You are not carrying anything.` (ibid. 4561).  geas prints
`You are carrying nothing.` or one bare name per line
(`geas-runner.cc:4358-4366`).

```
geas:  You are carrying:            qv4:  You are carrying:
       "Get well soon!" Card              "Get well soon!" Card and Box of Cigars.
       Box of Cigars
```

geas also lists objects inside carried containers; Quest's loop takes only
objects whose `ContainerRoom` is `"inventory"` (ibid. 4502).  26 games.

### 7. Exit lines come out in the wrong order

**Fixed.**  `geas-runner.cc:2130-2150` puts the places line first from 2.80 on
and keeps the pre-2.80 out / directions / places order below it.  Finding 8 is
deliberately left alone: out and the directions are still two lines.

For ASL 2.80 and up, `ShowRoomInfo` appends the places line to
`roomDisplayText` (`V4Game.Part2.cs:3838-3874`) and prints the `out`/
directions string *after* it (ibid. 3948-3955).  geas prints out, then
directions, then places (`geas-runner.cc:2030-2040`) — which is the *pre*-2.80
order (`ShowRoomInfoV2`, `V4Game.Part2.cs:2022`, 2056, 2092), applied to every
version.

```
geas:  You can go west.                    qv4:  You can go to your bed or the bathroom.
       You can go to your bed or the …           You can go west.
```

22 games.

### 8. `out` and the directions belong on one line

`UpdateDoorways` builds `You can go out to <place>.` and `You can go <dirs>.`
into a single returned string separated by a space
(`V4Game.Part2.cs:7306-7357`), and `ShowRoomInfo` prints it with one `Print`.
geas prints two lines (`geas-runner.cc:2033-2038`).  4 games.

### 9. ASL ≥ 4.10 still gets the pre-4.10 exits block

**Fixed.**  From 4.10 `look` prints the single `You can go #quest.doorways#.`
line and nothing else (`geas-runner.cc:2122-2127`); the pre-4.10 block is now
reached only below that version.  Both sub-cases went with it: `out` is a bare
compass word inside that line, and a place carries its *destination room's*
prefix (`geas-runner.cc:2840-2851`, against `RoomExit.cs:231`).

`quest.doorways.dirs`/`.out`/`.places` are still set at 4.10 — that is
finding 25, and left alone.

From 4.10 Quest folds out, directions *and* places into one
`You can go …` line built by `RoomExits.GetAvailableDirectionsDescription`
(`RoomExits.cs:304-353`); `quest.doorways.dirs`/`.out`/`.places` stop being set
at all.  geas keeps printing the three-line pre-4.10 form — 417 lines across 23
games in the `exits: 4.10 single line` bucket, and the variables it keeps alive
are finding 25.

A visible sub-case, counted separately as `exits: place prefix at 4.10`
(28 lines, 3 games): at 4.10 a place exit is rendered as
`to <destination room's prefix> <display name>` (`RoomExits.cs:514-530`, with
the prefix taken from the destination room by `RoomExit.cs:231`).  geas's
`get_places` (`geas-runner.cc:1402-1450`) only ever uses a prefix written on
the `place <the; X>` line itself, which is the pre-4.10 rule
(`GetGoToExits`, `V4Game.Part2.cs:7799-7837`).  ThunderClan Mystery 1 has
`place <ThunderClan Clearing>` and `prefix <the>` on the room, so Quest prints
`the ThunderClan Clearing` and geas prints `ThunderClan Clearing`.

A second sub-case, and the larger one: at 4.10 `out` is an ordinary compass
direction, printed as the bare word.  `RoomExits` gives it a `Direction.Out`
enumerator with the token `o` and the name `out`
(`RoomExits.cs:361-364`, `:465-468`), and `GetDirectionNameDisplay` takes the
`GetDirection() != None` path for it, returning just `|b out |xb` with no
destination (`:514-521`) — only a *place* gets the `to …` treatment.  geas keeps
writing the pre-4.10 `out to <room>`:

```
> up
-You can go out to Skarro.
-You can go down.
+You can go out or down.
```

50 lines across 11 games — Awakening Dead 2, Briny Blue, Exits of The World,
Freshman Fantastic, GhostLight, On The Far Blue, Shipwrecked, SkateUrAssOff,
What Do You Do, Wizard and ZombiesAttack.

### 10. Oxford comma in the places list

**Fixed**, but only from 2.80 up (`geas-runner.cc:2805-2814`).  The comma is
not a geas invention: `ShowRoomInfoV2` splits at `Left(places, lastComma)` and
so *keeps* it (`V4Game.Part2.cs:2092`), where `ShowRoomInfo` splits at
`lastComma - 1` and drops it (ibid. 3862).  Below 2.80 `a, b or c` is wrong and
`a, or b` — comma and all, even with only two places — is right.

geas joins the last two places with `", or "`
(`geas-runner.cc:2622-2634`); Quest inserts `" or"` before the last comma and
keeps the comma (`V4Game.Part2.cs:3849-3866`), giving `a, b or c`.  14 games.

### 11. No characters line below ASL 2.80, and characters leak into the objects line

**Fixed.**  `regen_var_objects` (`geas-runner.cc:2605-2688`) now routes a
`character` block out of the objects list and into a list of its own, sets
`quest.characters` from it — the comma form, since Quest sets the variable
before it rewrites the last comma — and builds the sentence into
`characters_display_`, which `look` prints above the objects line
(`:2110-2111`).

The two version gates really do differ by one, and geas now differs by one too.
The loader fills `_chars` at `ASLVersion <= 280` (`V4Game.Part2.cs:3571`), but
the room display that reads it is `ShowRoomInfoV2`, entered only at
`ASLVersion < 280` (ibid. 3690).  A character in a game declared at exactly
2.80 is therefore listed *nowhere at all*: it is out of the objects loop and
there is no characters line to put it in.

Quest's `ShowRoomInfoV2` prints `You can see … here.` or `There is nobody
here.` and sets `quest.characters` (`V4Game.Part2.cs:1808-1856`); geas prints
neither and never sets the variable.

That is only half of it.  Below 2.80 Quest keeps two arrays and walks them in
turn — the characters loop over `_chars` (`V4Game.Part2.cs:1824-1834`), then
the objects loop over `_objs` (ibid. 1876-1886) — so a character appears in the
first line and *never* in the second.  `_chars` is filled by the `define
character` branch of the loader, which is itself version-gated:

```csharp
else if ((ASLVersion <= 280) & BeginsWith(_lines[j], "define character"))
```
(`V4Game.Part2.cs:3571`)

geas has one object table for both: `GeasState::GeasState` appends every
`character` block to the same `objs` vector it built the `object` blocks from
(`geas-state.cc:625-639`), with the comment that characters are "placed in the
world like objects so they can be looked at, spoken to, etc.".  That is right
for ASL 2.80 and up, where Quest has no `_chars` either, but below it the
character comes out in geas's `There is … here.` line — the one place Quest
guarantees it will not be.  Dream Weaver (217) is the one-name case,

```
> n
-You are in Concack Beach (north)
-There is Odd looking fish here.
+You are in Concack Beach (north).
+You can see Odd looking fish here.
```

and Uranus (210) the mixed one, where the three squad members are pulled out of
geas's objects line and Quest's characters line is printed above it:

```
-There is Your Ship, Fighting for Newbies Book, Lt. Simon, Sgt. Tads and Pvt. Hugo here.
+You can see Lt. Simon, Sgt. Tads and Pvt. Hugo here.
+There is Your Ship and Fighting for Newbies Book here.
```

Eleven of the thirteen pre-2.80 corpus games define characters (all but Easy
Money and Koww), Magic Sword with 57 of them and Devil's Bargain with 39.
310 lines, 12 games.

### 12. Looking at (or opening) a container does not list its contents

From ASL 3.91 on, *every* object description ends with the object's contents
listing; geas produces one only for the separate `look in` verb, so both
`look at`/`x` and `open` come out short:

```
EscapePrison   > look at leather pouch
                 It is a pouch made of deer hide. It smells.
               + It contains lock pick.
CellPhone      > open fridge
               + It contains a can of pepsi and an apple.
Things         > open cereal box
               + The box contains Corn flakes, your favorite.
```

`DoLook` (`V4Game.cs:2136`) runs the object's `look` action/property/line and
then, at 2203-2209, `if (ASLVersion >= 391) objectContents = await
ListContents(id, ctx);`, printing it at 2233-2236. `open` goes through the same
path: `DoOpenClose` sets `opened` and calls `DoLook(id, ctx,
showDefaultDescription: false)` (ibid. 2240-2249), which is why opening a
container shows what is in it.

That call is the whole of `DoLook`, not just its contents half, so `open` also
prints the object's *description* — which geas's open path
(`geas-runner.cc:3541-3571`) never reaches either.  Rainbow Room's wardrobe
carries both a `look` and a bare `open`:

```
> open wardrobe
 You open it.
+It's a wooden wardrobe.
+It contains a Ballerina outfit.
```

`showDefaultDescription: false` is why an object with no `look` stays quiet;
Rainbow Room's kitchen drawer and cupboards are two more of the same, and are
that game's only unclassified lines.  Note also that Quest reaches `DoOpenClose`
only down the *property* branch (`V4Game.cs:2828-2847`): an object with a real
`action <open>` runs its script and shows neither description nor contents.

The look and the contents are also coupled at 2225-2231 — the
`defaultlook` / `defaultexamine` error is suppressed when there *is* a contents
listing, so an undescribed container answers with its contents instead of
"Nothing out of the ordinary".

`ListContents` (ibid. 3298-3431) is the whole format:

- not a `container` → empty string;
- shut, and not `transparent` or a `surface` → the `list closed` action or
  property, and nothing else;
- otherwise the children (objects whose `parent` is this one, `Exists &&
  Visible`) under a header taken from the `list` property: a header ending in
  `:` is a prefix, a header not ending in `:` is printed *instead* of the list,
  and with no header at all it is `UCase(Left(article,1)) + Mid(article,2) + "
  contains "` — the default `article` being `it`, hence `It contains …`.  Items
  are bolded and joined `, ` … ` and `, and the line ends in a full stop;
- empty container → the `list empty` action or property.

geas has the container model — `container`, `surface`, `opened`,
`transparent`, `parent` are all recognised (`geas-runner.cc:545-556, 748-750,
888-892`) — but the listing is reachable only from `look in`
(`geas-runner.cc:3581-3617`), and it is written in wording of geas's own:
`Inside you see a and b.`, `It is closed.`, `It is empty.` for the three cases
Quest renders as `It contains a and b.`, the `list closed` text, and the `list
empty` text. The `list` property, its colon rule and the `article`-derived
header are not implemented at all, so a game that sets `list <The box
contains>` (Things) has nowhere for that string to come out.

17 games touched, 49 lines; first divergence in Barbarian and Escape Prison.
Note that this is engine behaviour, not the type library: only two corpus games
`!include` typelib.qlb at all (finding 19), and none defines a `TLTcontainer`.

### 13. `.asl` / `.cas` text is not transcoded from Windows-1252

Quest's `Chr`/`Asc` go through cp1252, so `©`, `£`, curly quotes and accented
letters come out right; geas passes the bytes through and they land in the Glk
buffer as mojibake.  14 games — Nearco, Something 'Bout A Hex, Green Light,
The Devil's Bargain and others.

(The oracle harness needed the same fix on the QuestViva side; it is section 9
of `../../quest5/harness/oracle/patch_questviva.py`.)

### 14. Object names are matched too loosely

Quest matches a typed noun against the object's `alias` and its alt-names only
(`V4Game.cs:4704-4767`).  geas's `match_object` (`geas-runner.cc:3123`) also
word-matches the raw ASL *name*, so it accepts nouns Quest rejects:

| game | object | typed | geas | qv4 |
| --- | --- | --- | --- | --- |
| Treasure Hunt | `define object <Silver Bracelet>` / `alias <Silver>` | `take silver bracelet` | takes it | `I can't see that anywhere.` |
| YOU ARE A TIGER | name `Phone`, alias `iPHONE` | `look at phone` | resolves | `I can't see that here.` |
| Something 'Bout A Hex | `9volt battery` vs `9-Volt battery` | | resolves | rejects |
| Nearco | name `Prisma`, alias `piedra tallada` | `use casco on prisma` | resolves | `Ese objeto no existe.` |
| Social Studies | name `Letter`, alias `Envelope` | `open letter` | opens it | `I can't see that here.` |

Every one of those walkthroughs depends on the loose match, so each of those
games diverges for the rest of the run.  Nearco's is the sharpest: the prism is
made by `use piedra on roca`, and every later puzzle needs it, so a single
alias lookup decides the whole game.  Social Studies shows the quieter shape:
the failed `open letter` skips a `show <Note>` / `reveal <Note>`, so the Note
never joins the Lobby's object list and every later description of the room is
one object short in qv4.

The exact-match half of geas's `match_object` is right — with an alias present,
`alias` is what `text` is compared against — so this is entirely the *partial*
pass.  `word_match (text, alias) || word_match (text, name)`: the second half of
that disjunction is only ever reached when the object has an alias, and it is
the whole bug.  Quest's abbreviation pass takes the same fallback in the other
direction, `thisName = LCase(_objs[i].ObjectAlias)` when an alias exists
(`V4Game.cs:4737-4741`), and only `ctx.AllowRealNamesInCommand` — set for the
object of an `exec`, so the player is not asked to disambiguate twice — lets the
real name through at all (`V4Game.cs:4638-4650`).

A probe, at 4.10 and with no `exec` in sight:

```
    define object <bartender>
        alias <Barkeep>
        look <He is cleaning the glasses.>
    end define
```

```
 > look at bartender
-He is cleaning the glasses.
+I can't see that here.

 > look at barkeep
 He is cleaning the glasses.
```

Nine further corpus games type an aliased object's ASL name — after discounting
the ones Quest still reaches as a word-initial abbreviation of the alias
(finding 48), which is how `post it` finds `post it note` and `woodsman` finds
`Steven the woodsman`: The Birthday Assignment (`bartender`, `kevin`), Briny
Blue (`rock pile`), Enterprising (`useless card`), Pure Chaos (`box2`, `dws`),
Shadow Masters (`door to hotel`, `door to men's restroom`), Social Studies
(`letter`), Sutekh (`sute stonehouse`), Treasure Hunt (`silver bracelet`) and
YOU ARE A TIGER (`phone`).  Most of those lose a turn each.  The Birthday
Assignment loses the game: the bartender and the homeless man are its two
conversation trees, and `punch bartender` — the script that walks the player out
of the tavern — never runs, so Quest is still in the Prospect Inn while geas is
three rooms away, and the walkthrough's menu answers are read as commands
("`> 2`", "I don't understand your command").

### 15. `quest.objects` joins with `" and "`

Quest uses `", "` throughout `quest.objects` and puts `" and "` only in
`quest.formatobjects` (`V4Game.Part2.cs:3770-3815`, 1860-1910); geas puts
`" and "` in both.  Nearco, which prints `#quest.objects#` itself, comes out as
`un hueso, una piedra and una roca`.

`quest.formatobjects` is not reliably the `" and "` list either.  Three routines
write it, and only two of them punctuate: `ShowRoomInfo`
(`V4Game.Part2.cs:3806-3823`) and `ShowRoomInfoV2` (`:1892-1920`) put `" and "`
before the last object, but `UpdateObjectList` — the pane refresh — rebuilds it
as a flat `", "` list (`:7515-7545`).  `UpdateObjectList` runs after every
`show`, `hide`, `create`, `move` and `take` (`SetAvailability` at `:3087-3088`,
and eleven other callers), so any script that changes what is visible and then
prints `#quest.formatobjects#` reads the unpunctuated list.  geas has one
generator with `" and "` in it (`geas-runner.cc:2503-2515`) and regenerates on
read, so it always punctuates.

Sleepover's `define room <bed2>` description does exactly that — `show <outlets>`
and `show <vent2>`, then the list:

```
> se
-You can see your bed, …, a bust of Mozart, electrical outlets and the heating vent.
+You can see your bed, …, a bust of Mozart, electrical outlets, the heating vent.
```

One line; of the eight corpus games that print `#quest.formatobjects#` it is the
only one that changes visibility first.

### 16. `set numeric` below ASL 3.91 evaluates the whole expression

**Fixed.**  `eval_set_numeric` (`geas-runner.cc:2091-2106`) splits on the
game's version: 3.91 and up keep the full evaluator, and below it
`eval_double_pre391` (`geas-util.cc:101-153`) is Quest's one-operation reader —
the text trimmed, then the first `+`, else `*`, else `/`, else `-` at position
≥ 2, with `vb_val_num` (Quest's `Conversion.Val`) on each side.  A zero divisor
answers 0 and logs Quest's "Division by zero" line to the debug channel.
Fractions print through `fmt_double`, VB's `Str` — full precision, no leading
zero — where `fmt_double_net` (.NET `Double.ToString`) is used elsewhere
(`geas-util.hh:72-95`).  `fixtures/setnum350.asl` and `setnum410.asl` pin the
two bands against the oracle; Kingdom, KQ5 and Teacher all lost the arithmetic
half of their divergence.  Two of Quest's own quirks in that reader are
deliberately not reproduced — see findings 68 and 69.

`ExecSetVar` (`V4Game.cs:7252-7350`) uses `ExpressionHandler` only from ASL
3.91.  Below that it finds the *first* `+`, else `*`, else `/`, else `-` at
position ≥ 2 and computes exactly one binary operation, with `Conversion.Val`
— which stops at the first non-numeric character — on each side.  So
`set numeric <x; 10*3/4>` is `10 * 3` = **30** in Quest and `7.5` in geas.

Fractional results also render differently: VB's `Str` omits the leading zero
and keeps full precision (`.3333333333333333`), geas prints `0.3333333333`.

Corpus: Kingdom (ASL 300), KQ5_Full_final_1124 (350), Teacher (350).

### 17. `define variable … value <…>` is not substituted

**Fixed.**  `set_game` (`geas-runner.cc:2482-2549`) now throws away the raw
ivar/svar records the variable blocks left behind and rebuilds them in source
order, each initial value passed through `eval_string` first, so `#`, `%` and
`$…$` are expanded once at load and `$rand(…)$` draws in definition order.  The
rebuild writes the state directly instead of going through
`set_svar`/`set_ivar`, because a variable's `onchange` script must not fire for
the value it is defined with.  Kingdom's 23 `value <$rand(a;b)$>` stats now
start where Quest's do and its RNG stream stays in step; the game is identical
to QuestViva end to end.  Finding 49 is the same fix.

`static_svar_lookup` and `static_ivar_lookup` (`geasfile.cc:836`, `:878`) both
store `param_contents(tok)` raw.  Quest runs the initial value through
`GetParameter` at load (`V4Game.Part2.cs:744`), so `#`, `%` and `$…$` are all
expanded — and `$rand(…)$` *consumes RNG draws*, in definition order.

Kingdom is the whole-game case: 23 of its variables are initialised with
`value <$rand(a;b)$>`, so under geas every village stat starts at 0 and the RNG
stream is offset against Quest's from the first turn.  (23 lines, all in
`Kingdom.asl`; the `.cas` files were not scanned.)

### 18. `quest.lookdesc` is stale after `property <room; look=…>`

**Fixed.**  `look()` calls `regen_var_look()` itself
(`geas-runner.cc:2121`, the helper at `:2894-2900`), before the description tag
is found, so every room display reads the property as it stands at that moment
— and a description that changes the property still sees the text the display
began with, as in Quest.  The four movement/startup calls stay where they are;
they are what fills the variable for a game that prints it outside a room
display.  `fixtures/lookprop.asl` covers it, and Beam's Inside Machine now
matches.  Finding 50 is the same fix.

geas caches the room's look text and refreshes it only in `regen_var_look()`
(`geas-runner.cc:2542-2548`), which is called from four movement/startup sites
(1232, 2099, 2176, 2418); the room display then reads the cached svar
(2043-2050).  Quest recomputes `GetObjectProperty("look", …)` inside *every*
`ShowRoomInfo` (`V4Game.Part2.cs:3879-3893`).  So after a script sets the
room's `look` property, geas keeps showing the old text until the player leaves
and comes back.

Found via Beam, whose `!intproc116` does `property <Inside Machine; look=…>`;
reproduced with `lookprop2.asl` / `lookprop3.asl`.  (`properties`, plural, is
honoured by neither engine.)

### 19. The bundled type library's `!addto game` block is dropped

**Fixed.**  `typelib_builtin.hh:52-73` carries the block verbatim now, and
`readfile.cc` grew the machinery Quest merges it with: `preprocess()` splices
an `!addto game` body into the game block just before its `end define`, and
`mark_library_addtos()` prefixes the lines Quest prefixes on the way in —
`startscript` from a library at ASL 3.11 and `command`/`verb` from 3.92, the
version coming from `library_asl_version()` (`!library` with no version counts
as 200, a file that does not open with `!library` as 100).  That prefix is what
makes a library's contribution lose to the game's own: `ExecCommand` runs the
game's commands, then its verbs, then the `lib command`s, then the `lib verb`s,
and `cmd_entry::is_lib` (`geasfile.hh:83-86`) carries the tag through to the two
passes in `geas-runner.cc:3771` and `:3810-3813`.  The `description` override
binds through the game-block fallback in `look()` (`geas-runner.cc:2131-2159`),
so King's Quest V's rooms are rendered by `TLProomDescription` — description
first, then `You can move …` — and its 142 diff lines are gone.

geas ships MaDbRiT's Type Library as a built-in string so that a game which
`!include`s it can still load (`typelib_builtin.hh`, spliced in by
`readfile.cc`).  The copy is verbatim except for one omission, recorded in its
own header comment (lines 9-16): the library's `!addto game` block
(`Libraries/Typelib.qlb:26-44`) is cut down to its `startscript` line
(`typelib_builtin.hh:51-53`), because geas "already implements those verbs
natively".  The block does two things, and dropping it changes behaviour:

```
!addto game
   command <look at #TLSdObj#; look #TLSdObj#; l #TLSdObj#> do <TLPlook>
   command <examine #TLSdObj#; inspect #TLSdObj#; x #TLSdObj#> do <TLPexamine>
   command <open #TLSdObj#> do <TLPopen>
   ...
   description {
      do <TLProomDescription>
   }
```

The `description` override is the one this corpus catches.  King's Quest V
includes typelib, so under Quest its rooms are rendered by
`TLProomDescription`, which prints the description first and then its own exit
lines in its own wording — `You can move …` for directions
(`typelib_builtin.hh:211-213`), `You can go to …` for places, `You can go out
to |b…|xb.` for `out`.  geas keeps the procedure but never binds it, so the
native renderer runs instead and puts `You can go north, south, east or west.`
*above* the description:

```
> w
-You are in the desert
-You can go north, south, east or west.
+You are in the desert.
 Endless sand in all directions.
+You can move north, south, east or west.
```

142 diff lines — 71 room renders, both halves of each swap — all in
KingsQuestV.  The command half of the block has no visible symptom here:
the procedures survive as dead code (`TLPlook`, `TLPexamine` →
`TLPexamineContainer`, `TLPopen`, `TLPtake`, … are defined and never named
again), and none of the library's default messages — `noTake`, `closedDesc`,
`isOpenedDesc` — appears on either side of any diff.  Only two corpus games
include the library at all (KingsQuestV and Drom Bennacht, the latter one of
QuestViva's own crashes), so this is a small finding by line count and a large
one by surface area.

### 20. Directions are listed in geas's compass order, not the room's

**Fixed.**  `regen_var_dirs` (`geas-runner.cc:2909-2948`) keeps the fixed
compass table for 2.80-4.09 only.  Below 2.80 it walks `exit_dir_order()`
(`:1718`), the room block's own tag order, skipping `up`, `down` and `out`
because `ShowRoomInfoV2` has no branch for them; from 4.10 the folded
`quest.doorways` list is built from the same walk (`:3068-3098`), which is what
`AllExits()` gives Quest.  Fade To White's swapped diagonals and Koww's
reversed pair are gone, and `fixtures/doorways210.asl`, `doorways400.asl` and
`doorways410.asl` pin the three bands.

geas builds `quest.doorways.dirs` by walking its own fixed `dir_names[]`
table — north, south, east, west, northeast, northwest, southeast, southwest —
and taking the exits that exist (`regen_var_dirs`, `geas-runner.cc:2549-2571`).
Quest does that for exactly one version band.  Below ASL 2.80 `ShowRoomInfoV2`
walks the room block's *source lines* and appends each direction as it meets it
(`V4Game.Part2.cs:1935-1980`); from 4.10 on `GetAvailableDirectionsDescription`
walks `AllExits()`, a dictionary filled by `AddExitFromTag` as those same lines
are read, so again source order (`RoomExits.cs:304-353, 545-573`).  Only
2.80-4.09 uses the fixed order geas always uses (`UpdateDoorways`,
`V4Game.Part2.cs:7229-7286`).

QDK writes rooms with the diagonals in the order NW, NE, SW, SE, so the
signature is a swapped diagonal pair:

```
Wizard      -You can go north, northeast or northwest.
            +You can go north, northwest or northeast.
FadeToWhite -You can go north, south, east, west, northeast, northwest, southeast or southwest.
            +You can go north, south, east, west, northwest, northeast, southwest or southeast.
Koww        -You can go north or south.
            +You can go south or north.
```

232 lines across 10 games, and the version split holds in every one: MagicSword
217, FadeToWhite 210 and Koww 200 are the pre-2.80 cases; GhostLight,
Shipwrecked, Wizard, OperationRisingStar, Ponyville, BrinyBlue and
HospitalVisit are all 410.  No 2.80-4.09 game in the corpus shows the
divergence.  What Do You Do (410) is an eleventh, counted under finding 9
instead because its line also folds in an `out` exit and so has no matching
direction list to pair with:

```
-You can go out to Street.
-You can go north, south, east, west, northeast, northwest, southeast or southwest.
+You can go out, north, south, east, west, northwest, northeast, southwest or southeast.
```

geas already has the machinery: `exit_dir_order()` (`geas-runner.cc:1577-1602`)
reads the room block in source order, and the 4.10 `quest.doorways` svar is
built from it (2654-2660).  It is only the printed line that does not use it.
Fixing finding 9 would fix the 4.10 half of this by construction; the pre-2.80
half needs `regen_var_dirs` to take the same route.

### 21. The disambiguation menu is worded differently, and its options lose the prefix

geas asks `Which <noun> do you mean?` (`geas-runner.cc:3293`).  Quest asks
`Please select which <noun> you mean:`, printed italic behind a `- ` marker
(`V4Game.cs:4802-4803`), and echoes the chosen option back as `- <option>|n`
afterwards (ibid. 4854).

The options differ too.  Both engines prefer the object's `detail`, but where
there is none Quest falls back to `Prefix + ObjectAlias` (ibid. 4810-4820) and
geas to the alias or raw name alone (`geas-runner.cc:3274-3277`):

```
OneRobot   > use chain gun on thug
           -Which thug do you mean?
           +Please select which thug you mean:
            1) Thug
            2) Thug
BrinyBlue  > look at bed
           -Which bed do you mean?
           +Please select which bed you mean:
            1) bed
           -2) bed
           +2) a bed
```

70 lines across 13 games.  (The harness already normalises the *shape* of a
menu — caption, numbered options, chosen answer — into geas's layout, so what
is left is only the text.)

### 22. Object lookup is not scoped per verb

geas resolves every noun against the same two places, inventory and the current
room (`dereference_vars`, `geas-runner.cc:3157-3162`).  Quest passes a scope to
`Disambiguate` per call site, and several verbs name one place only:

| verb | Quest's scope | site |
| --- | --- | --- |
| `take` | current room | `V4Game.Part2.cs:5132` |
| `drop` | inventory | `V4Game.cs:5531` |
| `give <item>` | inventory | `V4Game.Part2.cs:4673` |
| `give … to <who>` | current room | ibid. 4723 |
| `use <item>` | inventory | ibid. 5305 |
| `use … on <target>` | current room, then inventory | ibid. 5396-5403 |

The inventory is reachable from `take` only afterwards, and only from ASL 4.10,
and only to choose between `alreadytaken` and `badthing` (ibid. 5143-5167).
Below 4.10 there is no already-have answer at all: `take <something you are
carrying>` is `badthing` at 3.91 and up and `baditem` below that.  geas answers
it at every version with a hard-coded `You already have it.`
(`geas-runner.cc:4032-4035`) — wrong text as well as wrong version gate, since
Quest's `AlreadyTaken` reads `You already have that.` and is overridable
(finding 4):

```
FreshmanFantastic  > take tool        -You already have it.   +You already have that.   (4.10)
KingsQuestV        > take hair pin    -You already have it.   +I can't see that anywhere. (3.50)
```

So a noun that matches both a carried object and one on the floor is ambiguous
to geas and unambiguous to Quest.  The Wizard's yellow beads are the clean case
— three already carried, one just found:

```
> examine beds
You search all the beds and find a small yellow bead.
> take yellow bead
-Which yellow bead do you mean?
-1) Yellow Bead … 4) Yellow Bead
-[choice] 4
 You pick it up.
+> 4
+I don't understand your command. …
```

Quest takes the one in the room without asking; geas offers four, so the next
script line — the menu answer — is consumed by geas and left over for Quest.
Both transcripts are still in step at the `> take yellow bead` above (it is a
diff context line, not a change), and from there on Wizard's replays run a turn
apart, which is why that one game holds 109 of the corpus's 174
implied-removal lines.

### 23. Below ASL 3.11 a place is named by its tag, not the room's alias

`place <Science Lab>` in a room whose target is `define room <Science Lab>` /
`alias <Sciece Lab>` should print — and answer to — *Science Lab* at ASL 2.10.
Quest gates the alias substitution on the version:

```csharp
// GetGoToExits, V4Game.Part2.cs:7811
if ((ASLVersion >= 311) & string.IsNullOrEmpty(Places[i].Script))
    shownPlaceName = _rooms[GetRoomID(Places[i].PlaceName, ctx)].RoomAlias;
else
    shownPlaceName = Places[i].PlaceName;
```

and `PlaceExist` repeats the same test for *input* matching
(`V4Game.Part2.cs:6570`).  `get_places` (`geas-runner.cc:1402-1525`) has the
script half of the gate but not the version half, so it always prefers the
alias.  Space (ASL 2.10) is the clean case — the room's alias is a typo:

```
> out
-You can go to Stasis Room, Sciece Lab, Medilab, or the Bridge.
+You can go to Stasis Room, Science Lab, Medilab, or the Bridge.
> go to sciece lab
-You are in the Sciece Lab
+You can't go there.
```

That second line is a walkthrough-breaking divergence, not cosmetic: the rest
of the Space replay happens in the wrong room.

Eleven corpus games are below 3.11 and define places: Broken Mirror,
Fade To White, Koww, Space, Uranus, Blade Sentinel, Romantic Music, Green
Light, Legend of Cyrn, Devil's Bargain and Magic Sword.

### 24. `quest.doorways` omits `to ` and prefixes places from the wrong tag

**Fixed.**  The 4.10 branch of `regen_var_dirs` (`geas-runner.cc:3068-3098`)
builds a place as `to [destination-room prefix ]|bname|xb`, reading the prefix
off the destination room rather than off the `place` tag, and folds it into the
same list as the directions with Quest's own `", "`/`" or "` joining.  Things'
`to A Work Truck` now matches, as do `to A Screened Cage`, `to The Nickel
Building` and `to the ThunderClan Clearing`; `fixtures/doorways410.asl` is the
regression test.

At ASL 4.10 and up Quest builds the exits sentence from
`GetAvailableDirectionsDescription`, and a place contributes

```csharp
// GetDirectionNameDisplay, RoomExits.cs:522-528
return "to " + prefix + " " + "|b" + displayName + "|xb";
```

where `prefix` is the *destination room's* `prefix` tag, copied in
`RoomExit.UpdateObjectName` (`RoomExit.cs:225-236`).  geas pushes the place
verbatim (`geas-runner.cc:2655-2662`): no `to `, and its prefix is the one
written in the `place <prefix; dest>` tag, which is usually absent.  The two
differ whenever the destination room carries a `prefix`:

```
Things   define room <Work Truck>  prefix <A>
         define room <Vehicle Maintenance>  place <Work Truck>
> look
-You can go northwest or Work Truck.
+You can go northwest or to A Work Truck.
```

Also seen as `to A Screened Cage`, `to The Nickel Building` and `to the
ThunderClan Clearing`.  Games that print `#quest.doorways#` themselves — Things
does, from its own `msg <You can go #quest.doorways#.>` — show it directly;
elsewhere it is folded into finding 9.

### 25. `quest.doorways.dirs`, `.places` and `.out` are still set at ASL ≥ 4.10

**Fixed.**  `const bool old_vars = (asl_version_ < 410)`
(`geas-runner.cc:2920`) gates `.dirs`, `.out`, `.out.display` and `.places`, and
`quest.doorways` is written only from 4.10 (`:3068`), so exactly one of the two
forms is live at a time.  Nearco's `Puedes ir al: |b#quest.doorways.dirs#|xb`
is the bare label Quest prints.

The other half of finding 9.  `UpdateDoorways` only fills those three variables
below 4.10 (`V4Game.Part2.cs:7190-7365`); from 4.10 the whole block is replaced
by `GetAvailableDirectionsDescription`, which sets `quest.doorways` alone.  A
4.10 game that still prints one of the old variables gets nothing from Quest
and a filled list from geas.  Nearco (ASL 4.10, line 2308) is the case:

```
-Puedes ir al: north, south, east, west, northeast, northwest, southeast or southwest
+Puedes ir al:
```

(Its `msg <… |b#quest.doorways.dirs#|xb>` at line 2308 is the first divergence
in that game.  Later `Puedes ir al: Sur.` lines in the same transcript are
literal room text, not the variable.)

### 26. `take` has no container-accessibility gate

**Fixed.**  `player_can_access` (`geas-runner.cc:922-947`) walks the parent
chain and stops at the first container that is neither a `surface` nor
`opened`, reporting `"inside closed " + displayed_name (parent)` — the
container's alias where it has one, and pointedly not asking about
`transparent`, which makes a thing visible without making it reachable.  The
take handler consults it from ASL 3.91 (`:4508-4517`) and answers
`display_error_info ("badtake", …)`, so Pyramid of Terror's shut goth and shut
cabinet of concoctions refuse.  `fixtures/takeaccess.asl` is the fixture; there
is no sub-3.91 companion, because below that version Quest stops sweeping
container visibility altogether and the same game answers "I can't see that
anywhere" instead.

From ASL 3.91 Quest refuses to take anything whose parent chain passes through
a container that is neither a `surface` nor `opened`:

```csharp
// ExecTake, V4Game.Part2.cs:5169-5174
var canAccessObject = PlayerCanAccessObject(id);
if (!canAccessObject.CanAccessObject)
    await PlayerErrorMessage_ExtendInfo(PlayerError.BadTake, ctx, canAccessObject.ErrorMsg);
```

`PlayerCanAccessObject` (ibid. 7727-7795) walks `parent` upwards and reports
`"inside closed " + <parent's display name>`.  geas goes straight to the take
script (`geas-runner.cc:4037`).  2 lines in Pyramid of Terror:

```
+You can't take it - inside closed goth.
+You can't take it - inside closed concoctions.
```

### 27. An object inside a carried container is already taken (ASL ≥ 4.10)

**Fixed.**  `held_in_container` (`geas-runner.cc:909-919`) walks the parent
chain to the inventory, and `is_held` consults it from ASL 3.91 (`:876`), which
is where `MoveThing` starts recursing into a container's children.  A take of
something already up that chain is refused before any script runs
(`:4498-4506`) — `alreadytaken` from 4.10, `badthing` from 3.91, `baditem`
below.  TARDIS Escape's sonic screwdriver in the trenchcoat pocket now gets its
"Look in your inventory, you greedy pig!".  Finding 61 is the same fix.

Quest keeps a contained object's `ContainerRoom` in step with its container —
`DoAddRemove` copies it on `add` (`V4Game.cs:2118`) and `MoveThing` recurses
into every child from ASL 3.91 (`V4Game.cs:6643-6653`) — so once you pick up
the container, its contents are in `inventory` as far as `Disambiguate` is
concerned.  `take <content>` therefore misses the room scope, hits the
inventory scope, and answers `alreadytaken` (`V4Game.Part2.cs:5143-5153`).

geas models containment the other way round: the contained object's location
*is* the container (`move`, `geas-runner.cc:813-831`), so `is_held` is false
and the take proceeds.  TARDIS Escape (ASL 4.10) has a pocket parented to a
worn trenchcoat, and overrides `alreadytaken`:

```
> take sonic screwdriver
-(first removing it from pocket)
-*
-You acquired the Sonic Screwdriver! Fantastic!
+Look in your inventory, you greedy pig!
```

(The `*` is the pocket's own `remove` text, so the whole three-line answer is
geas running a turn Quest declines to run.)

Distinct from finding 22: there the object is directly in the inventory and
only the wording differs; here geas runs a take script that Quest never runs.

### 28. An unset string or out-of-range array element reads as `"!"`

**Fixed.**  The array accessor answers `""` for an index past the end
(`geas-state.hh:86-94`), which is what `get_svar` already did for a variable
that was never defined, so Wizard's twelve-slot spell menu leaves its empty
slots empty.  The `"!"` a missing *property* reads as is deliberately kept
(`geas-runner.cc:8126-8131`): that one is Quest's too.

`geas-state.hh:89` and `:229` both answer `"!"` — for an index past the end of
a string array, and for a string variable that was never defined.  Quest's
`GetStringContents` logs an ASL error and answers `""` in both cases
(`V4Game.Part2.cs:2559-2653`).  The sentinel is not an internal marker: it goes
straight into game text.  Wizard's spell menu has twelve slots and three spells:

```
 3) Fireball
-4) !
+4)
…
-12) !
+12)
```

308 lines, all in Wizard, but the sentinel would surface in any game that reads
a slot it has not filled.

### 29. An `action <verb>` with an empty script shadows the same-named property

**Fixed.**  `geasfile.cc:279-296` drops an action whose script is empty after
trimming, exactly as `AddToObjectActions` does, so the object's
`properties <verb = …>` text stays visible to `ExecVerb`.  Sutekh's television
answers from its `watch` property again, and the rewrite `action <switch off>`
performs on it is visible.  Finding 60 is the same fix.

Quest discards an action that has nothing after the closing `>`:

```csharp
// AddToObjectActions, V4Game.cs:3935-3940
var ep = Strings.InStr(actionInfo, ">");
if (ep == Strings.Len(actionInfo))
{
    LogASLError("No script given for '" + name + "' action data", LogType.WarningError);
    return;
}
```

so the object's `properties <verb = …>` text is still what `ExecVerb` finds.
geas registers the empty action anyway (`geasfile.cc:275-285`), `block_action`
returns true with an empty script, and the verb prints nothing.  *Sutekh Is
Hiding In Your Priory* has both a `watch` property and a bodyless
`action <watch>` on its television:

```
> watch television
+You watch as a woman in a white dress paces purposefully about in a priory garden.
> switch off television
 You switch off the vintage viewer.
> watch television
+You regard the dot for a while, fancying as you do that a leaf flutters to the ground outside. Well, it is autumn.
```

(The second line is the property that `action <switch off>` rewrote, so the
whole rewrite mechanism is invisible in geas.)

### 30. The take-success error key is spelt `defaultake`, with one t

What finding 4's spelling bullet costs, measured.  Quest's error-key table has
the typo (`V4Game.Part2.cs:1472`), and QuestViva
reproduces it, so `error <defaultake; …>` is what a game must write to override
`You pick #quest.error.article# up.`.  geas registers the key under the
correct-looking spelling instead (`geas-runner.cc:1336`).  Five corpus games —
Briny Blue, Certain Oscar, Mansion, Mitchell Quest and Nearco — write
`defaulttake`; none writes `defaultake`.  geas honours all five; real Quest
honours none of them.  Only Mitchell Quest's replay actually takes anything —
21 lines — so the bucket's 22nd line is Wizard's, credited by the heuristic but
really finding 22 cascade:

```
MitchellQuest  error <defaulttake; Consider it tooken......... er, I mean taken.>
> take crowbar
-Consider it tooken......... er, I mean taken.
+You pick it up.
```

Quest also maps the key to `BadTake` rather than `DefaultTake` at ASL ≤ 2.80
(ibid. 1474-1481); geas has no such version split.

### 31. `picture <file; caption>` — the caption is never split off the filename

Quest's `ShowPicture` splits the tag's contents on `;` for a caption and on `@`
for a display size, prints the caption as ordinary game text, and then shows the
image (`V4Game.Part2.cs:3660-3686`):

```csharp
var caption = "";
if (Strings.InStr(filename, ";") != 0) {
    caption  = Strings.Trim(Strings.Mid(filename, Strings.InStr(filename, ";") + 1));
    filename = Strings.Trim(Strings.Left(filename, Strings.InStr(filename, ";") - 1));
}
if (Strings.InStr(filename, "@") != 0)
    filename = Strings.Trim(Strings.Left(filename, Strings.InStr(filename, "@") - 1));
if (caption.Length > 0) await Print(caption, _nullContext);
await ShowPictureInTextAsync(filename);
```

geas's `st_picture` (`geas-runner.cc:5753-5772`) splits only `@`, so
`picture <Rug.gif; Oriental Rug>` reaches `show_image` with the whole string
`"Rug.gif; Oriental Rug"` as the filename — the image can never load, and the
caption is never printed.  The interface even has the parameter for it
(`GeasRunner.hh:132`), but the call site always passes `""` and `geasglk.cc:1350`
names it `/*caption*/` and drops it.

Six corpus games write captions on 20 tags — Assassin, Burglary, Defenders of
Gondor, King's Quest V, MetroidLite and The Lazst Resort — and the replays reach
10 of them:

```
Burglary  picture <Rug.gif; Oriental Rug>
> look under rug
+Oriental Rug

DefendersOfGondor  picture <river1.jpg; The Great River, looking eastwards towards Mordor.>
+The Great River, looking eastwards towards Mordor.
```

Whether the caption ought to be *printed* is arguable — QuestViva's own comment
says Quest 4.x put the image in a popup window and that showing it inline is a
QuestViva decision — but splitting it off the filename is not: as things stand
none of these six games can display any captioned image at all.

Same statement, second half: geas has **no version gate for `picture popup`**.
Quest dispatches `picture popup ` to `ShowPicture` at ASL ≥ 3.90, `picture ` to
`ShowPicture` at 2.82-3.90, `show ` to it below 2.82, and `picture ` to
`ShowPictureInTextAsync` at ≥ 3.90 (`V4Game.Part2.cs:5830-5843`).  geas
registers one handler, `{"picture", st_picture}` (`geas-runner.cc:4590`), which
calls `next_token`, sees `popup` where it wants a `<…>` parameter, and returns
silently.  MetroidLite, Michael's Game and Wizard use the popup form; none of
the three reaches one of those tags in its replay, so this costs no diff lines
today.

### 32. `##` and `%%` escapes are dropped at load time

**Fixed.**  The `##`/`%%` half went with finding 67, which gave `static_eval`
the empty-name case in both branches (`geasfile.cc:930-941`, `:991-995`), so
RiddleRun's `alias <Room ##1>` prints its hash.  The `$` half is fixed here:
`eval_string`'s `$` arm (`geas-runner.cc:8221-8232`) emits a literal `$` for an
empty body instead of calling a nameless function, and `static_eval` has a `$`
arm of its own.  Pure Chaos's `$$1000000` now reads as money.
`fixtures/dollarpair.asl` covers both paths.  Finding 51 is the same fix for
`$$`; its second half, an unknown function name inside `$…$`, is still open.

`#` and `%` introduce a variable substitution, and both engines let a doubled
sigil stand for a literal one.  Quest does it in the single routine that walks
the string, `ConvertParameter` (`V4Game.cs:6671-6733`):

```csharp
if (string.IsNullOrEmpty(varName)) { result = result + convertChar; }
```

geas has two such routines.  The runtime one, `eval_string`, gets it right
(`geas-runner.cc:7581-7582` for `#`, `:7626-7627` for `%`, `:7644-7645` for
`~`).  The load-time one, `GeasFile::static_eval` (`geasfile.cc:893-987`), has
no empty-name case at all: `##` falls into `static_svar_lookup("")` and `%%`
into `static_ivar_lookup("")`, both of which answer nothing, so the escape
vanishes before the game ever runs.

RiddleRun (ASL 400) aliases its five rooms `alias <Room ##1>` … `<Room ##5>`,
which is exactly the load-time path:

```
> look
-You are in Room 1
+You are in Room #1.
```

All ten of RiddleRun's diff lines are this (five rooms, both sides).  The
corpus census is small: `##` appears only in RiddleRun and ZombiesAttack — the
latter inside a `command <…>` pattern, a different parser — and `%%` only in
Adventure (a `msg`, so the correct runtime path) and in four games'
`display <…>` status strings, which the harness normalises away.  RiddleRun is
the whole visible cost, but the missing case is in the shared load-time
evaluator, so any game that aliases or describes with a literal `#` hits it.
(RiddleRun's ten lines are already counted under *room line: no full stop*,
since both sides of each pair also disagree about the trailing dot.)

The fourth sigil, `$`, is missing the escape on **both** paths: `eval_string`'s
`$` arm (`geas-runner.cc:7650-7660`) computes the empty body and evaluates it as
a function rather than emitting a literal `$`, and `static_eval` has no `$` arm
at all.  Pure Chaos writes `msg <You search through the $$1000000 and find a
key…>`:

```
> search money
-You search through the 1000000 and find a key. How did that get here?
+You search through the $1000000 and find a key. How did that get here?
```

Three corpus games write `$$` — Dark Hills, Pure Chaos and Shadow Masters — and
only Pure Chaos's is in text the replay reaches.

### 34. An unmatched `$` does not yield `<ERROR>`

The three other conversion characters already follow Quest: `eval_string`
returns the literal string `<ERROR>` for the whole parameter when `#`, `%` or
`~` has no partner (`geas-runner.cc:7573-7577`, `:7620-7624`, `:7637-7641`),
matching `ConvertParameter`'s

```csharp
if (nextPos == 0) {
    LogASLError("Line parameter <" + parameter + "> has missing " + convertChar, …);
    return "<ERROR>";
}
```

(`V4Game.cs:6695-6700`).  The `$` arm was left as it was and passes the rest of
the string through instead:

```cpp
std::string::size_type j = s.find ('$', i + 1);
if (j == string::npos)
  {
    gi->debug_print ("Unmatched $s in " + s);
    return rv + s.substr (i);
  }
```

Quest applies `ConvertParameter` once per character — `$`, `#`, `%`, `~` — with
the same body, so all four behave alike.  This one should return `"<ERROR>"`.

No corpus game shows it on the runtime path: Pure Chaos's `look <It's about
$1000000.>` is a *load-time* `look`, which goes through `static_eval` and hits
the documented limitation below instead.

### 33. An unrecognised `|` code eats the character after it

**Fixed.**  `print_formatted` keeps a `bool literal_bar`
(`geas-runner.cc:8429`) that every dead end sets — the `default` arm, a
malformed `|sNN`, `|jx`, `|xz` — and on it prints `"|"`, resets `i` to the bar
and `continue`s, so the loop's own `i++` lands on the character after the bar
and the text branch prints it (`:8540-8547`).  The Lazy Gun Cult's `|K|` sigil
and `|"…"|` quotes survive whole, and Uranus's stray `|s9|` is printed as
written.  `fixtures/barcodes.asl` walks every code, live and dead.

Quest's `TextFormatter` tries the two-character codes, then the one-character
codes, then `|sNN`; if none matches it emits a literal `|` and leaves the read
position immediately after the bar, so the next character is printed as
ordinary text (`TextFormatter.cs:187-190`).  geas's `print_formatted` instead
falls off the end of its `switch` with the index already advanced past the
code character (`geas-runner.cc:7884-7887`), and the loop's own `i++` then skips
it:

```cpp
default:
  GEAS_DBG << "p_f: Fallthrough " << s[i] << std::endl;
  changed = false;
```

So `|` plus one character disappears.  The `case 's'` and `case 'j'` arms have
the same shape — a malformed `|s9` or `|jx` `continue`s out with the index past
the digits — where Quest falls through to the literal `|`.  (`case 'c'` is the
one arm that was already right, and its `--i` is not the bug it looks like:
`|c` never reaches `TextFormatter`, because `Print` intercepts it, and for any
following character that is not a colour code it clears the screen and swallows
both (`V4Game.Part2.cs:6740-6790`) — which is exactly what geas does.)

The Lazy Gun Cult uses `|K|` as its cult's sigil and `|"…"|` to set off a quoted
page, and every one of them is mangled — including two characters' worth per
bar, so words run together:

```
> x arrow
-The letter has been etched in red onto the head of the arrow.
+The letter |K| has been etched in red onto the head of the arrow.

> look
-… There is a bronze laqueon the wall beside you.
+… There is a bronze |plaque| on the wall beside you.

> x guest book
-Tuesday IV Sept:
+|"Tuesday IV Sept:
-Markus Turnwire"
+Markus Turnwire"|
```

(`|plaque|` swallows `|p` and then `| `, which is why `laque` and `on` collide.)
Uranus opens with a stray `|s9|`, malformed because the size code wants two
characters — see finding 71 — and Quest prints it verbatim where geas swallowed
it.  11 diff lines across 2 games, and the explanation for a `|"` line that sat
unclassified for a long time.

### 35. `place locked <room; message>` is thrown away

`RoomExits.AddExitFromTag` strips the `locked ` keyword *before* it looks at
what kind of exit this is, so a place gets the same treatment as a compass
direction (`RoomExits.cs:145-149`):

```csharp
if (_game.BeginsWith(tag, "locked "))
{
    await roomExit.SetIsLocked(true);
    tag = _game.GetEverythingAfter(tag, "locked ");
}
```

and the two-field parameter then yields the destination and the lock message
(`:186-191`).  geas's `get_places` (`geas-runner.cc:1415-1421`) reads the token
after `place`, finds the bare word `locked` where it wants a `<…>`, logs
*Expected parameter after 'place'* and skips the line entirely — so the place is
neither listed nor reachable, and `go to` it answers with the generic refusal.
`exit_declared_locked`'s own comment already records the gap: "geas never locks
places" (`:1911`).

SkateUrAssOff (ASL 410) is the corpus's only `place locked`:

```
> go to half pipe central
-You can't go there.
+You have yet to unlock this Skate Zone.
```

Its two exits lines differ for the same reason, but those are already counted
under finding 9.  Below ASL 4.10 the tag is not supported by Quest either — the
pre-4.10 parser calls `GetParameter` straight past the keyword and then reads
the destination as a *prefix* and the lock message as the room name
(`V4Game.Part2.cs:1216-1234`) — so this is a 4.10-and-up statement.

### 36. A `type <…>` line does not override the properties written above it

Quest reads an object block one line at a time and applies each line as it comes
to it, and a `type <…>` line is applied by pushing the type's whole property list
through the same `AddToObjectProperties` that a `properties <…>` line uses
(`V4Game.Part2.cs:3398-3405`):

```csharp
else if (BeginsWith(_lines[j], "type "))
{
    …
    var PropertyData = await GetPropertiesInType(await GetParameter(_lines[j], _nullContext));
    await AddToObjectProperties(PropertyData.Properties, _numberObjs, _nullContext);
```

`AddToObjectProperties` overwrites a property of the same name in place
(`V4Game.cs:4042-4069`) and re-runs the side effects, so `prefix` from the type
lands back in `o.Prefix` (`:4093-4106`).  A property line therefore only survives
if it comes *below* the `type` line that also sets it.

geas resolves the two in a fixed order instead, types first and the object's own
properties second, whatever the source order (`geasfile.cc:435-448`):

```c++
  /* A property set directly on the object always wins over one it inherits from
   * a type, as in Quest -- regardless of whether the `type <...>` line precedes
   * or follows the property line in the source. */
  for (const GeasBlock::dir &d: block.obj_prop)
    if (d.is_type)
      get_type_property (d.a, propname, bool_rv, string_rv);
  for (const GeasBlock::dir &d: block.obj_prop)
    if (!d.is_type && ci_equal (d.a, propname))
```

The comment's claim is the bug: for the common type-first ordering the two are
identical, but with the property first geas keeps a value Quest has thrown away.
A single in-order pass over `obj_prop` would match Quest for both orderings.
(`get_type_property` just below it already makes that single pass, which is why
nesting *inside* a type is right.)

Sleepover (ASL 350) is the only corpus game that writes a property above its
`type` line — eight sites, six of them `prefix`; `define object <sweater>` has
`prefix <a black>` on one line and `type <jacket>` on the next, and
`define type <jacket>` sets `prefix = a`:

```
> look
-There is Mom, Dad, the television, a knitting needle, a spool of yarn and a black sweater here.
+There is Mom, Dad, the television, a knitting needle, a spool of yarn and a sweater here.
```

The other five reach the transcript through the end-of-game inventory, where the
type's `prefix` is not merely shorter but different — `undies` sets
`prefix = a pair of`, so red panties become a pair of panties:

```
-You were wearing cow jammies, a Supergirl cami, red panties.
-You were carrying: a black sweater, sewing kit, lighter, cookies, a pair of red shorts, screwdriver.
+You were wearing cow jammies, a cami, a pair of panties.
+You were carrying: a sweater, sewing kit, lighter, cookies, a pair of shorts, screwdriver.
```

16 lines, all in Sleepover.  The remaining two sites set `article`, which nothing
in the replay prints.

### 37. `use on <…>` keeps a trailing space the object's name has lost

geas trims the name in a `define` header (`readfile.cc:202-208`) because so much
else about a name arrives trimmed:

```c++
      /* Trim the name: a stray space in "define room <Foo >" would otherwise
       * leave the block registered as "Foo " while every exit destination is
       * trimmed to "Foo" ... */
      out_block.name = trim (param_contents(name));
```

but the rewriter that turns a `use on <…>` line into an `action <…>` does not
(`readfile.cc:371-374`), and neither does the `give to <…>` arm just below it
(`:402-403`):

```c++
			      else if (is_param (rest))
				{
				  line = lhs + "on " + param_contents(rest) + "> " + rhs;
				}
```

So an object written `<Warrior >` is registered as `Warrior` while its partner's
handler is filed under `use on Warrior `, `get_obj_action` misses, and the
`defaultuse` error runs instead of the script.  Quest never trims either side —
`ObjectName` and `UseData().UseObject` are both raw `GetParameter` results
(`V4Game.Part2.cs:3229`, `V4Game.cs:4257-4283`, and `GetParameter` at
`V4Game.cs:1858-1890` does no trimming) — so its two spellings agree and the use
succeeds.

Londe Perplex (ASL 410) has `define object <Warrior >` in West War and
`use on <Warrior >` on the Rage:

```
> use rage on warrior
-You can't use that here.
+You have used your rage against the defenceless warrior. …
```

2 lines, one game.  Nine corpus games declare a name with a trailing space —
eight of them rooms, where the trim is what makes the exits work — and Londe
Perplex is the only one that also writes that name into a `use on` or `give to`
tag.  Trimming in those two arms, like the `define` header already does, is the
matching fix.

The same trim cuts the other way on an *object*, where it is geas that accepts
too much.  `ObjectAlias` defaults to `ObjectName` (`V4Game.Part2.cs:3229-3230`),
both of them raw, so a padded declaration leaves the object answering only to a
name the player cannot type.  Metal Sonic's Quest (ASL 410) has

```
define room <Campaign Mode- Indoors>
	define object < door>
		take goto <Campaign Mode- Tails' room>
	end define
end define
```

and `door` is the only way on:

```
 > take door
-You are in Campaign Mode- Tails' room
-M: Here I am..... gotta make sure I have my weapon!
+Do I need glasses? No. But I can't see that! Then it must NOT EXIST!
```

Quest is stuck in the room for the rest of the run — 245 diff lines, the whole
of the game's campaign half — while geas plays on.  That is the sharper end of
the trade: trimming lets a broken game be finished, but it is not what Quest
does, and a walkthrough written against geas will not replay on the real
engine.

### 38. The abbreviation pass needs a whole word, where Quest needs only a word start

Quest's second disambiguation pass — the one gated on ASL ≥ 3.91 and
`abbreviations` — is a single `InStr` against the object's display name with a
space glued to the front of both sides (`V4Game.cs:4760-4766`):

```csharp
                    if (Strings.InStr(" " + thisName, " " + Strings.LCase(name)) != 0)
```

so the typed text has to *begin* at a word boundary and may end anywhere: `post`
matches `poster`, `grave` matches `gravestones`, `lan` matches `lantern`.  geas's
`word_match` (`geas-runner.cc:3048-3066`) additionally requires the run to end at
a word boundary:

```c++
      bool left_ok = (pos == 0) || t[pos - 1] == ' ';
      std::string::size_type end = pos + x.length();
      bool right_ok = (end == t.length()) || t[end] == ' ';
```

Everything else about the pass is already faithful — the gating on
`use_abbreviations_` and 3.91, the exact-first ordering, the prefix and suffix
handling — so this one condition is the whole difference, and it makes geas
*stricter* than Quest rather than looser (the opposite direction from finding
14).  Dropping `right_ok` would match Quest.

Both corpus cases turn a Quest disambiguation menu into a silent geas match, so
the walkthrough's next line is eaten as a menu answer on the QuestViva side and
the two runs part company for good:

```
> use chalk on grave                      GhostLight, gravestones + laser marked grave
-You mark the grave with the chalk, and not a moment too soon as the red laser light disappears.
+Please select which grave you mean:
+1) several gravestones
+2) a laser marked grave
+[choice] examine grave
```

```
> take post                               Sir Loin 2, Poster + Broken post
-You pick it up.
+Please select which post you mean:
+1) a Poster
+2) a Broken post
```

These two are the only places in the corpus where QuestViva raises a menu geas
does not.  The remainder of both games is cascade — GhostLight's laser pen stays
on its tripod on the geas side, Sir Loin 2's poster is read instead of the post
being taken.

### 39. A room tag containing a semicolon is truncated at every ASL version

geas rewrites each recognised tag line into a property line
(`readfile.cc:350-353`):

```c++
	  else if (props[ltok] && is_param(rest))
	    {
	      line = "properties <" + ltok + "=" + param_contents(rest) + ">";
	    }
```

and `ensure_cached` then splits a property line on `;` (`geasfile.cc:243-271`),
so `look <A; B>` becomes the property `look=A` plus a stray flag named `B`.

Quest reaches the same result, but only from ASL 3.50, and by a different route:
a room's `look`, `alias`, `description`, `indescription` and `prefix` go into
dedicated `_rooms[]` fields *and*, from 3.50 on, into the mirrored object
properties (`V4Game.Part2.cs:990-1000`, `:1186-1193`), where
`AddToObjectProperties` splits them.  The printed text prefers the property and
falls back to the field (`:3879-3891`):

```csharp
        objLook = GetObjectProperty("look", _rooms[id].ObjId, logError: false);
        if (string.IsNullOrEmpty(objLook))
        {
            if (!string.IsNullOrEmpty(_rooms[id].Look))
            {
                lookDesc = _rooms[id].Look;
            }
        }
```

so below 3.50 there is no property to prefer and the whole string survives —
below 2.80 `ShowRoomInfoV2` reads the `look` line straight out of the source
(`V4Game.cs:2185-2187`) and never builds a property at all.  For *objects* the
boundary is 3.11 instead, and above it Quest splits exactly as geas does
(`V4Game.Part2.cs:3349-3363`).

Magic Sword Part 1 (ASL 217) is the corpus's only tag below the boundary:

```
> northwest                              look <…enemys; using weapons.|n And to the northeast…>
-To the northwest is a room where you fight magical replicas of enemys
+To the northwest is a room where you fight magical replicas of enemys; using weapons.
+And to the northeast is the room where you fight replicas of enemys; using spells.
```

6 lines, one game.  Two ASL 350 games — One Robot (`Cheese Street runs northeast
to west; Milk Avenue…`) and The Former (`…stands behind the counter; if you…`) —
have the same shape of tag and are *identical* on both sides, which is what
pins the boundary at 3.50 rather than at "always".

### 40. `outputoff <>` is obeyed, where Quest wants the bare word

geas dispatches a script line on its first token and ignores whatever follows
(`geas-runner.cc:4601-4602`), so `st_outputoff` and `st_outputon`
(`:5720-5730`) fire on any line that *starts* with the word.  Quest tests the
whole line:

```csharp
            else if (Strings.Trim(Strings.LCase(scriptLine)) == "outputon")
…
            else if (Strings.Trim(Strings.LCase(scriptLine)) == "outputoff")
```

(`V4Game.Part2.cs:6049-6058`.)  A parameter makes the line unrecognised, and it
falls off the end of the chain into `LogASLError` — logged, nothing printed,
output left on.

Mysts (ASL 310) is the one corpus game that writes the parameterised form.  Its
Cave turns the player straight back out again (`Mysts.asl:136-142`):

```
description if got <frightened> then {
        msg <On no way should you enter this vile cavern.>
        outputoff <>
        goto <Plateau>
        msg <>
        outputon <>
}
```

The author meant the return trip to be silent; in Quest it is not.

```
> west
 On no way should you enter this vile cavern.
+A small plateau in the side of the cliff. To the south is a path leading to
+the beach and to the west is the enterance to a small cave. …
```

1 line, one game — and the only unclassified line Mysts has.  The other five
corpus games that use the statement (Devil's Bargain, King's Quest V, Dark
Hills, Romantic Music, Sleepover) all write it bare and agree on both sides.
The same first-token dispatch accepts a parameter on every statement Quest
matches whole: `clear`, `helpclear`, `helpclose`, `nointro`, `stop`,
`dontprocess` (`V4Game.Part2.cs:5798`, `:5814`, `:6000-6020`, `:6083`).

### 41. Below ASL 2.80 there is no item table: `give` invents items, in call order

Quest builds the item table once, at load, from the `items`/`possitems` line of
the `define game` block (`SetUpItemArrays`, `V4Game.Part2.cs:6955-7008`), and
`give`/`lose` below 2.80 only set the `Got` flag on an entry that is already
there:

```csharp
            for (int i = 1, loopTo1 = _numberItems; i <= loopTo1; i++)
            {
                if ((_items[i].Name ?? "") == (item ?? ""))
                {
                    _items[i].Got = got;
```

(`V4Game.Part2.cs:6676-6684`.)  geas keeps no table at all: `st_give` appends
the name to `state.items` if it is not there already
(`geas-runner.cc:5465-5472`), and `get_inventory` walks that vector
(`:6969-6982`).  Three consequences, all visible in the corpus:

**The list comes out in a different order.**  Quest's is the declaration order
of the `items` line; geas's is the order the player picked things up.  Romantic
Music declares `items <balls; hat; romantic music; steps; socks; shoes; packet
of condoms; red book; green book; everyday clothes; money; condom machine>`:

```
> i
-everyday clothes  red book  green book  socks  shoes  balls  money
+Balls, socks, shoes, red book, green book, everyday clothes and money.
```

Devil's Bargain is the same story (`Map, police pass, photos and monkey
drawing.` against `map, police pass, monkey drawing, photos`) — the `items`
line puts `police pass` before `photos` before `monkey drawing`, the player
finds them in the other order.

**A `give` of a name that is not on the `items` line does nothing at all.**  No
item, no error — the loop simply finds no match.  Of the 13 pre-2.80 corpus
games, four write such a `give`: Hobbit (`ring`, `flower`, `Goblin knife`),
Lovesong (eight of them), Devil's Bargain (`knife`, `police_phone_number`) and
Space (`Jar with Strange Liquid.`, the declared name plus a full stop).

**The `items` parser drops the first name after a leading separator.**
`SetUpItemArrays` looks for the next separator with `InStr(pos + 1, …)`, so at
`pos = 1` a separator in column 1 is skipped and swallowed into the first name.
Hobbit's `items <; map; key; pipe; …>` therefore declares an item called
`"; map"`, and `give <map>` matches nothing for the whole game.

All three at once, at the end of Hobbit's walkthrough:

```
> inventory
 You are carrying:
-pipe  map  key  orcrist  sting  treasure  food  Goblin knife  ring
+Key, pipe, food, orcrist, sting and treasure.
```

Quest also compares item names case-sensitively (the `==` above) where geas
uses `ci_equal`; no corpus game depends on it.

Nothing in the corpus tests `if got <…>` on an item Quest never gave, so the
divergence is display-only here — but it need not be: the same table backs
`got`, so a game that gates a puzzle on an undeclared item is winnable in geas
and stuck in Quest.

### 42. `msg nospeak <…>` prints nothing

`nospeak` is an ASL keyword (`Libraries/quest.dat:204`) that suppresses the
text-to-speech reading of a message.  Quest ignores it when printing, because
`GetParameter` simply takes what lies between the first `<` and the first `>`
of the line (`V4Game.cs:1858-1874`) — nothing between `msg ` and the `<` is
looked at at all.

geas reads the statement with `next_token`, and requires the very next token to
be the parameter:

```c++
  case st_msg: case st_helpmsg:
    {
      string stmt = tok;
      tok = next_token (s, c1, c2);
      if (is_param (tok))
	print_eval (param_contents(tok));
      else
	gi->debug_print ("Expected parameter after " + stmt + " in " + s);
```

(`geas-runner.cc:5709-5717`.)  `nospeak` is not a parameter, so the message is
reported as a script error and dropped.  The word is in `readfile.cc:706`'s
keyword list, so it survives loading intact — nothing else strips it.

Five sites in three games (The Former ×2, Burglary ×2, King's Quest V), and the
replays reach one:

```
> what model did this motherboard chip come from?
+That? Hmm.... Well, it has Dectyne-type wiring, so this had to have come from a 7.
 You have found a clue.
```

The Former's line is that game's only unclassified line.  Note the general
shape: any modifier word written between a statement and its `<…>` is invisible
to Quest and fatal to geas — `picture popup <…>` is the same bug in another
statement (finding 31).

### 43. `if property <obj; name=value>` can come out true, where Quest always says no

A Quest property is a (name, value) pair split at the first `=`, at load time
and at run time alike:

```csharp
var ep = Strings.InStr(info, "=");
if (ep != 0)
{
    name = Strings.Trim(Strings.Left(info, ep - 1));
    value = Strings.Trim(Strings.Mid(info, ep + 1));
}
```
(`AddToObjectProperties`, `V4Game.cs:4021-4026`)

so no property name ever contains an `=`.  The `property` *condition* tests
only for a name — `ExecuteIfProperty` ends `return GetObjectProperty(
propertyName, id, true) == "yes"` (`V4Game.cs:5991`), and `existsOnly` compares
against `PropertyName` alone (ibid. 6223-6241).  `if property <obj; aggr=true>`
therefore asks whether the object has a flag literally called `aggr=true`, and
the answer is always no — Quest gives an author who meant "is aggr set to
true?" a silent false.

geas splits the pair the same way for properties written in the block
(`GeasFile::block_property`, `geasfile.cc:431-452`, matches `d.a` against the
name), so a statically declared `properties <aggr=false>` behaves identically.
Runtime assignments are different: `set_obj_property` stores the *raw text* of
the assignment as one string (`geas-runner.cc:731-733` →
`GeasState::add_prop`, `geas-state.cc:379-386`), and `runtime_prop` compares
that whole string against the query before it tries to split it:

```c++
  string is_prop = "properties " + prop;
…
	if (ci_equal (dat, is_prop))
	  {
	    string_rv = "";
	    value = true;
	    return true;
	  }
	std::string::size_type index = dat.find ('=');
	if (index != string::npos && ci_equal (trim (dat.substr (0, index)), is_prop))
```
(`geas-runner.cc:668-701`)

The first test is what makes a bare flag (`property <obj; hidden>`) work, but
it also matches when the query text and the stored text are the same
`name=value` string.  So the condition is false until the script assigns
exactly that pair, and true from then on.

Blade Sentinel is the only corpus game that writes the form, and it writes it
eleven times.  Ten are harmless — the pair asked about is never the pair
assigned — but Skippy's computer is the exception:

```
define object <Skippy's computer>
        properties <password=false; aggr=false>
…
afterturn if property <Skippy's computer; aggr=true> then msg < A man voice says: "Hey Skippy, will you open for me?">
```
(`BladeSentinel.asl:406, 436`)

and the menu that arms the droids runs `property <Skippy's computer;
aggr=true>` (ibid. 948).  From that turn on geas's `afterturn` fires:

```
 You set the droids to aggressive mode and can't wait to get the party started.
 You have reached your goal. Your rival's office is ahead of you, to the north.
 As you sneak to your target, you can hear the havoc caused by your little trick. …
-A man voice says: "Hey Skippy, will you open for me?"
```

Only one line here, because the same menu choice does `goto <Outside Torenti's
office>` and the afterturn belongs to the room the turn *started* in — Quest
captures `roomID` once at the top of `ExecCommand` (`V4Game.Part2.cs:4132`) and
uses it for the afterturn at the bottom (ibid. 4578-4597), so geas agrees about
which room's script runs; it only disagrees about the condition.  That is Blade
Sentinel's only unclassified line.

### 44. `oops` and `the` with nothing to correct should print nothing at all

Quest's last two command branches, immediately before the bad-command error:

```csharp
else if (CmdStartsWith(input, "oops "))
{
    await ExecOops(GetEverythingAfter(input, "oops "), ctx);
}
else if (CmdStartsWith(input, "the "))
{
    await ExecOops(GetEverythingAfter(input, "the "), ctx);
}
else
{
    await PlayerErrorMessage(PlayerError.BadCommand, ctx);
}
```
(`V4Game.Part2.cs:4564-4575`)

and `ExecOops` is one `if` with no `else`:

```csharp
private async Task ExecOops(string correction, Context ctx)
{
    if (!string.IsNullOrEmpty(_badCmdBefore))
    { … }
}
```
(`V4Game.cs:4410-4423`)

So *any* unrecognised command starting with `the ` is swallowed in silence, and
so is `oops …` when no command is waiting to be corrected.

geas gates the two words differently (`geas-runner.cc:2878-2896`):

```c++
    if (s.compare (0, 5, "oops ") == 0)
      { corr = trim (s.substr (5)); is_oops = true; }
    else if (oops_ready && s.compare (0, 4, "the ") == 0)
      { corr = trim (s.substr (4)); is_oops = true; }
    if (is_oops)
      {
	if (oops_ready && corr != "")
	  run_command (oops_before + corr + oops_after);
	else
	  print_formatted ("I don't understand your command. "
			   "Type HELP for a list of valid commands.");
	return;
      }
```

`the …` is recognised only while a correction is pending, so otherwise it falls
through to ordinary parsing and ends at the bad-command error; and `oops …`
with nothing pending prints that error outright.  Both cases print where Quest
prints nothing.

The Detective is the one replay that reaches it — five turns whose text happens
to begin with "the", each a geas-only line:

```
 > the pizza detours through the Pizza Hut, where all three toppings lead to
-I don't understand your command. Type HELP for a list of valid commands.
```

(Those turns are themselves a harness accident; see below.  The divergence is
not.)  Note that Quest's silence here is reachable in ordinary play too — a
player who types `the door` at a game with no `the` command gets no answer at
all.

### 45. `open` and `close` mark a container `seen`

Everything parented inside a container is out of scope until the container is
both open (or transparent, or a surface) *and* `seen`:

```csharp
// UpdateVisibilityInContainers, V4Game.Part2.cs:7710
if (parentIsSurface | ((parentIsOpen | parentIsTransparent) & parentIsSeen))
    await SetAvailability(_objs[i].ObjectName, true, ctx);
else
    await SetAvailability(_objs[i].ObjectName, false, ctx);
```

Quest sets `seen` in exactly two places: `DoLook`, from ASL 3.91
(`V4Game.cs:2141-2147`), and `DoAddRemove`'s `add` half, from ASL 4.10 (ibid.
2125-2131).  Opening a container is not one of them — `SetOpenClose` changes
`opened`/`closed` and nothing else.  So a container that the game reveals with
`show <…>` and declares `opened` still hides its contents until the player
*looks at* it.

geas marks it `seen` on the way into the open/close dispatch, before it even
knows whether the object can be opened:

```c++
	    /* Opening or closing a container discovers it (seen gate), even when
	     * the game scripts its own open/close action. */
	    if (key == "open" || key == "close")
	      set_obj_property (obj, "seen");
```
(`geas-runner.cc:3540-3542`; `open_container` sets it a second time at
`:1116`.)  A refused `open` — `It is already open.` — is enough, and so is
`close`.

`recess.asl`, an ASL 4.10 probe with a hidden `container`/`opened` recess and a
`bronze key` parented to it:

```
                     geas                        qv4
> reveal             A secret recess appears.    A secret recess appears.
> probe              KEY HIDDEN / RECESS UNSEEN  KEY HIDDEN / RECESS UNSEEN
> open recess        It is already open.         It is already open.
> probe              KEY VISIBLE / RECESS SEEN   KEY HIDDEN / RECESS UNSEEN
> take bronze key    (first removing it …)       That doesn't work.
```

The Blight of Elantria is the corpus case, and it is a whole-game one: its
temple recess is revealed by filling a bowl with salt, the walkthrough opens it
rather than looking at it, and geas hands over the bronze key and the ruby that
Quest refuses with the game's own `badthing` text.  The two runs never come
back into step — 995 diff lines.

```
 > take bronze key
-(first removing it from recess)
-Done.
-You pick it up.
+That doesn't work.
```

### 46. An empty `place <>` is dropped

geas throws an empty place away as it reads the room:

```c++
	  string dest_param = eval_param (tok);
	  if (dest_param == "")
	    {
	      gi->debug_print ("Parameter empty in " + line);
	      continue;
	    }
```
(`geas-runner.cc:1417-1427`.)

`GetGoToExits` has no such test (`V4Game.Part2.cs:7799-7836`): the empty name
goes through `GetRoomID`, which returns 0, so Quest logs `No such room ''` and
then lists the place under its own — empty — name.  The exits line comes out
with nothing after the `to`.

Metal Sonic's Quest has `place <>` in Egg Base- Lab later, the only one in the
corpus:

```
 There is desk, cupboard and vault here.
+You can go to .
```

1 line, one game.  geas's answer is the better one to read; it is not the one
Quest gives.

### 47. `exec <…>` runs the turn scripts again

A turn in Quest is not "one player command"; it is "one call to `ExecCommand`".
The `beforeturn` and `afterturn` blocks live at the bottom of that routine and
nothing suppresses them on re-entry — `skipAfterTurn` is declared at
`V4Game.Part2.cs:4127` and is never once set true:

```csharp
        if (!skipAfterTurn)
        {
            // Execute any "afterturn" script:
```
(`V4Game.Part2.cs:4578-4604`; the `beforeturn` block above it has no guard at
all.)

`exec` re-enters that routine.  Both arms of `ExecExec` do
(`V4Game.Part2.cs:2202-2251`):

```csharp
                await ExecCommand(execLine, newCtx, false);
...
                await ExecCommand(ex, newCtx, false, false);
```

so a game whose `command <…>` wraps the standard handling in `exec <…; normal>`
gets its room and game turn scripts run twice for the one thing the player
typed.  geas runs them exactly once, from `run_command`, around the whole turn.

The probe is small enough to read whole:

```
define room <Hall>
 look <The hall.>
 beforeturn msg <[beforeturn]>
 afterturn msg <[afterturn]>
 define object <coin>
 end define
 command <plain> msg <PLAIN>
 command <outer> { msg <OUTER> exec <inner> }
 command <inner> msg <INNER>
 command <take #@ob#> { msg <WRAPPER> exec <take #@ob#; normal> }
end define
```

```
        geas                        QuestViva
> plain
        [beforeturn]                [beforeturn]
        PLAIN                       PLAIN
        [afterturn]                 [afterturn]
> outer
        [beforeturn]                [beforeturn]
        OUTER                       OUTER
        INNER                       [beforeturn]
        [afterturn]                 INNER
                                    [afterturn]
                                    [afterturn]
> take coin
        [beforeturn]                [beforeturn]
        WRAPPER                     WRAPPER
        You pick up the coin.       [beforeturn]
        [afterturn]                 You pick up the coin.
                                    [afterturn]
                                    [afterturn]
```

Note where the second `[beforeturn]` lands: not before the wrapper, but between
the wrapper and the inner command, which is what gives it away in a transcript.

The cheapest corpus case is The Quest to find The Dark Hills, whose living room
offers a save the first time you walk in (`DarkHills.asl:1812`):

```
	if ask <Do you want to save?> then exec <save; normal>
```

`save` prints nothing under either harness, so the only trace it leaves is the
room's own `beforeturn` firing a second time, mid-turn, after the answer:

```
 [choice] no
+A new exit has become available! You can now go East.
 > search sofa
 A new exit has become available! You can now go East.
```

(The walkthrough's "no" is not a number, so both harnesses read it as Yes —
`Program.cs:196`, and geas's `atoi` the same way — and the `exec` runs.)

Things is the corpus case that matters.  It rewrites every `take`
(`Things.asl:73-89`):

```
    command <take #@ob#> {
        ...
            else exec <take #@ob#; normal>
```

and two of its rooms hang messages off the turn scripts — `West Security
Courtyard` has `afterturn` prose about the vine, `Smelting Plant` has
`beforeturn if exists <Huge Spider> then msg <…clicking noises…>`.  So every
`take` in those rooms doubles:

```
+You can hear some strange clicking noises coming from within the plant!
 You can hear some strange clicking noises coming from within the plant!
```

Not all of it repeats, either — an `afterturn` that advances a state machine
says something different the second time round.  Things wraps `spray` the same
way (`command <spray #@ob#> { … exec <use extinguisher on #@ob#> }`), and the
bear fight steps twice per turn:

```
 > spray bear
 You blast the bear's claw with the extinguisher! …
 The bear tries to get you with it's other claw!
+The bear rakes you with it's claw and draws it back for another strike!
```

The printed output is the small half of it, though: a turn script that counts
turns or ticks a countdown counts twice per player command in Quest and once in
geas, and nothing in the transcript says so.  Two engines, two different clocks.

Things is where the two clocks decide the game, and it is worth following in
full because it shows how little it takes.  `shine light on spider` is another
wrapper:

```
    command <shine light on #@ob#> {
        flag off <creature1>
        flag on <spider4>
        exec <use flashlight on #@ob#>
    }
```

`spider4` is the spider's stun flag.  The Smelting Plant's `afterturn` moves the
spider one square toward the player unless `spider4` is set, and clears it
through a two-step handshake — first run sets `spider5`, second run sees
`spider5` and clears both:

```
                else {
                    if flag <spider5> then {
                        flag off <spider4>
                        flag off <spider5>
                    }
                    else flag on <spider5>
                }
```

So the author's `flag on <spider4>` buys the player two still turns.  Under the
`exec` the handshake runs twice inside the one turn — set `spider5`, then clear
both — and buys one.  The spider gets a free move, and the corpus walkthrough,
which is written to the two-turn margin, loses by exactly that move:

```
 > look at large ladle
 A large black ladle used for pouring out molten material. …
 The spider moves toward you!
-The spider is now in the western part of the room.
+Oh no! The spider has caught up to you!
+The spider has caught you! It wraps you up in a web cocoon …
```

Quest then bounces the player back to the Smelter Lobby and plays out the rest
of the walkthrough in the wrong room; 500-odd diff lines, all of it downstream
of one flag cleared a turn early.

geas's reading is the sane one and the one a walkthrough author would expect —
Things is only winnable under it.  It is still a divergence, and the sharpest
one in the corpus: nothing about the transcript before the death hints that the
two engines are counting turns differently.

### 48. An abbreviation is a word-initial prefix, not a whole word

**Fixed.**  `word_match` grew a `need_right_boundary` parameter, false on the
abbreviation pass (`geas-runner.cc:3067-3090`, `:3145-3146`), so geas now
matches word-initially like Quest.  Three walkthroughs relied on the kinder
rule and had to be re-written to name the object in full — Sir Loin 2's
`broken post`, Ghost Light's `laser marked grave` and Blight of Elantria's
`scroll of translation` — which is what a player of Quest has to type too; qv4
confirms Quest asks the same disambiguation question of the short form.

Both engines resolve a noun in two passes: exact first, and if that finds
nothing, a loose pass gated on ASL 3.91 and on `define options / abbreviations`.
They disagreed about what the loose pass accepts.

geas took a run of whole words:

```c++
  if (allow_partial && (word_match (text, alias) || word_match (text, name)))
    return true;
```
(`geas-runner.cc:3119-3123`.)

Quest takes anything that starts at a word boundary, whole word or not —
`abbreviations` is meant literally:

```csharp
                    if (Strings.InStr(" " + thisName, " " + Strings.LCase(name)) != 0)
```
(`V4Game.cs:4759`; from 4.10 `thisName` has the object's prefix and suffix glued
on either side first, ibid. 4745-4757.)

So `kni` finds a Knife in Quest and nothing in geas, while `ost` finds nothing
in either — the match has to begin a word, it just need not finish one:

```
> look at kni
-You don't see any kni.
+A knife.
> look at ost
 You don't see any ost. / I can't see that here.
```

The half that matters is not the extra reach but the extra *ambiguity*.  Where
one object's name is another's prefix, Quest sees two candidates and geas saw
one, so Quest stopped to ask a question geas never asked — and in a scripted
replay the menu swallows the next command.  Two corpus games turned on it.
Sir Loin 2 has a `Poster` and a `Broken post` in the same field:

```
 > look at post
-The broken post was part of the coconut shy post, unti the cannon brkoe it. …
-> take post
-You pick it up.
+Please select which post you mean:
+1) a Poster
+2) a Broken post
+[choice] take post
```

Ghost Light has `several gravestones` and `a laser marked grave`, and the
question lands on the puzzle move itself:

```
 > use chalk on grave
-You mark the grave with the chalk, and not a moment too soon as the red laser light disappears.
+Please select which grave you mean:
+1) several gravestones
+2) a laser marked grave
+[choice] examine grave
```

Both games were winnable under geas and not under Quest, which is the usual
shape of these: geas's rule was the kinder one, and the divergence only showed
up when two objects in a room share a stem.  Both scripts now say which object
they mean, and both engines take them.

### 49. A variable's initial `value <…>` is never evaluated

**Fixed** with finding 17: `set_game` (`geas-runner.cc:2482-2549`) rebuilds
every variable's initial value through `eval_string` at load, in source order,
without firing `onchange`.

Quest reads a variable's initialiser through `GetParameter`, the same routine
every other parameter goes through, so `#…#`, `%…%` and `$…$` are all
substituted as the block is loaded:

```csharp
                    else if (BeginsWith(_lines[i], "value "))
                    {
                        variable.VariableContents[0] = await GetParameter(_lines[i], _nullContext);
                    }
```
(`V4Game.Part2.cs:745-748`.)

geas takes the text as it stands:

```c++
      if (vartype == "" || vartype == "numeric")
	{
	  IVarRecord ivr;
	  ivr.name = go.name;
	  ivr.set (0, parse_int (value));
```
(`geas-state.cc:722-728`.)

`parse_int` of a function call is 0, and a string variable keeps the source text
verbatim:

```
define variable <n1>  type numeric  value <$rand(212;252)$>
define variable <s1>  type string   value <$lcase(HELLO)$>

> show
-n1=0 s1=$lcase(HELLO)$
+n1=214 s1=hello
```

Three corpus games have a computed initialiser, all of them `$rand(...)$`:
Kingdom (four), Sir Loin 2 (one) and Sir Loin 3 (two).  Sir Loin 3's is a puzzle
answer — the Steppes have a random number of steps, and the number is also the
combination of a lock further on:

```
 > count steps
-After spending some time counting the steps you find that there are 0 steps
+After spending some time counting the steps you find that there are 242 steps
```

Under geas the answer is 0 every time, which is at least self-consistent — the
lock takes 0 as well — so the game stays winnable and the divergence reads as
cosmetic.  It is not: the variable is meant to be random per playthrough, and
geas makes it a constant.

### 50. A room's look text, name and prefix are cached on arrival

**Fixed** with finding 18.  `look()` calls `regen_var_room()` and
`regen_var_look()` at the top of every room display (`geas-runner.cc:2124-2125`),
so `quest.formatroom` and `quest.lookdesc` are both rebuilt where Quest rebuilds
them and a `property <room; alias=…>` or `; look=…>` shows on the very next
display.  Beam's Inside Machine crabs around once.  The probe below also turned
up something the finding did not claim: geas hung a room's `suffix` off the end
of the displayed name, and Quest has no room suffix at all — see finding 73.

`look()` prints the room's look text out of the `quest.lookdesc` string variable,
and the room's name out of `quest.formatroom`:

```c++
  if ((!described || asl_version_ < 310)
      && (tmp = get_svar ("quest.lookdesc")) != "")
    print_formatted (tmp);
```

(`geas-runner.cc:2049-2051`).  Both are written by `regen_var_look` and
`regen_var_room`, and both of those run only when the player *enters* a room —
`geas-runner.cc:1230`, `2097`, `2174` and `2415` are every call site, and each
one is a `goto` or the start of the game.  Quest rebuilds them where it prints
them, inside `ShowRoomInfo`:

```csharp
        objLook = GetObjectProperty("look", _rooms[id].ObjId, logError: false);

        if (string.IsNullOrEmpty(objLook))
        {
            if (!string.IsNullOrEmpty(_rooms[id].Look))
            {
                lookDesc = _rooms[id].Look;
            }
        }
        else
        {
            lookDesc = objLook;
        }

        await SetStringContents("quest.lookdesc", lookDesc, ctx);
```

(`V4Game.Part2.cs:3879-3893`; the room name, prefix and suffix are assembled a
few hundred lines earlier in the same function).  `GetObjectProperty` is the
runtime property store, so a `property <some room; look=…>` takes effect on the
very next room display.  In geas the assignment lands in the state — reading
`#quest.lookdesc#` straight after it still gives the old text, because nothing
has regenerated it — and stays invisible until the player leaves and comes back.

A probe with a room that rewrites its own look text, alias and prefix:

```
define room <Hall>
    look <Original hall look.>
    command <setlook> property <Hall; look=Changed hall look.>
    command <setalias> property <Hall; alias=Great Hall>
    command <setpre> property <Hall; prefix=the>
    command <show> msg <lookdesc=#quest.lookdesc# formatroom=#quest.formatroom#>
end define
```

```
 > look
-You are in Hall
-Original hall look.
+You are in the Great Hall.
+Changed hall look.

 > show
-lookdesc=Original hall look. formatroom=Hall
+lookdesc=Changed hall look. formatroom=the Great Hall
```

Note that `description` is *not* affected: `look()` reads that one through
`get_room_property` at the moment it prints, so it picks the change up.  It is
only the two cached variables that go stale.

One corpus game does this, and it does it five times: Beam.  Crawling into the
container docking machine is a `goto <Inside Machine>` immediately followed by

```
            property <Inside Machine; look=You are inside the container docking machine under a mess of unidentifiable equipment in a crawlspace that probably was not intended as such. There is an opening near your head.>
```

so the arrival description ("…just enough room to crab around — bumping your
face and every possible appendage — until your head is facing the opening
again") is meant to be a one-off and every later `look` is meant to be the short
form.  In geas the player crabs around for all sixty-seven of them:

```
  > look
 -You are inside the container docking machine … until your head is facing the opening again.
 +You are inside the container docking machine … There is an opening near your head.
```

### 51. `$$` and unknown functions in `$…$`

**Half fixed.**  `$$` is a literal `$` on both evaluation paths now — see
finding 32.  The other half is untouched: an unknown function name inside
`$…$` still evaluates to the empty string in geas, where Quest prints
`[ERROR]`.

Two more gaps in the same `eval_string` arm as finding 34.  `ConvertParameter`
runs the identical body for each of `#`, `%`, `~` and `$`, and one of its cases
is an *empty* name — two conversion characters in a row — which stands for a
literal one:

```csharp
                if (string.IsNullOrEmpty(varName))
                {
                    result = result + convertChar;
                }
```

(`V4Game.cs:6704-6708`).  geas has that case for the other three
(`geas-runner.cc:7563`, `:7610`, `:7644-7645` — `if (j == i + 1) rv += "~";`)
and not for `$`, which instead runs the empty string as a function name and
gets nothing back.  Pure Chaos's safe holds `msg <You search through the
$$1000000 and find a key…>`:

```
 > search money
-You search through the 1000000 and find a key. How did that get here?
+You search through the $1000000 and find a key. How did that get here?
```

The second gap is what a *failed* function call yields.  Quest returns a
marker:

```csharp
            intFuncResult = await DoInternalFunction(name, parameter, ctx);
            if (intFuncResult == "__NOTDEFINED")
            {
                LogASLError("No such function '" + name + "'", LogType.WarningError);
                return "[ERROR]";
            }
```

(`V4Game.cs:6763-6771`).  geas returns the empty string
(`geas-runner.cc:6956-6957`), so the text closes up over the hole instead of
showing it.  Both engines agree on which names exist — the built-in list is
matched case-sensitively on either side — so this only shows on a name the
author got wrong, and no corpus game has one on a reachable line.  A probe:

```
 > d                       ($nosuch(x)$)
-one  two
+one [ERROR] two

 > e                       (cost is $5 and $6)
-cost is 6
+cost is [ERROR]6
```

The second line is worth reading twice: `$5 and $` is a well-formed conversion
as far as either engine is concerned, so both call a function named `5 and `,
and the difference is only in what a miss prints.  Note also that the *earlier*
`[ERROR]` entry in the QuestViva-defects list below was wrong — `[ERROR]` is
Quest doing what Quest does, and the empty string is ours.

### 52. `set interval` does not affect the cycle already in flight

geas counts a timer *down* and re-arms it from `interval` at the moment it
fires:

```c++
      if (tr.timeleft != 0)
	tr.timeleft --;
      if (tr.timeleft == 0)
	{
	  …
	  tr.timeleft = tr.interval;
	  const GeasBlock *gb = gf.find_by_name ("timer", tr.name);
```

(`geas-runner.cc:7740-7752`).  Quest counts *up* and compares the accumulated
count against the interval as it stands on each tick:

```csharp
                    _timers[i].TimerTicks = _timers[i].TimerTicks + elapsedTime;

                    if (_timers[i].TimerTicks >= _timers[i].TimerInterval)
                    {
                        _timers[i].TimerTicks = 0;
                        timerScripts.Add(_timers[i].TimerAction);
                    }
```

(`V4Game.Part2.cs:126-131`).  For a fixed interval the two are the same clock.
They part as soon as `set interval` — which on both sides writes nothing but
the interval itself (`geas-runner.cc:5987`) — moves it while a cycle is in
flight, and they part in two different ways.

*From inside the timer's own action*, geas has already re-armed by the time the
action runs, so the new interval is picked up only from the cycle **after**
next.  A timer at interval 3 whose action sets interval 2, armed by `go` and
then driven by bare turns:

```
        geas   qv4
TICK      3     3     both fire on the third turn
TICK      6     5     geas re-armed from the old 3 before running the action
TICK      8     7
TICK     10     9     from here on the two run in parallel, one turn apart
```

*From outside*, the difference is larger, because geas's countdown was seeded
from the old interval and nothing rewinds it, while Quest's accumulated count
survives and is immediately compared against the new, shorter interval.  A
timer at interval 10, armed by `go`, with `fast` (`set interval <t; 2>`) typed
on the fifth turn:

```
 > fast
+TICK                 Quest: 5 ticks banked, 5 >= 2, fires on this very turn
 > z
 > z
+TICK                 …and every 2 turns from here
 > z
 > z
 ⋮
 > z
-TICK                 geas: still counting down the original 10
 > z
 > z
-TICK                 only now is the new interval in force
```

So a shortened interval cannot take effect early in geas at all, and a
lengthened one takes effect a cycle late.

Seven corpus games call `set interval`: Barbarian, Beam, King's Quest V,
Michael's Game, The Pilgrim's Progress, Shipwrecked and ThunderClan.  All of
them do it from inside the timer's own action, so all of them are exposed to
the milder half; Pilgrim's Progress is where it shows.  Its `sinking in mire`
timer starts at `interval <15>` and steps itself down to 10 and then to 5 as
the `sinking` counter runs out, so geas serves each of the slough messages a
whole cycle late and the rescue — "At that moment you see a man coming, whose
name is Help." — arrives four turns after Quest's.  The walkthrough's last four
`struggle`s land in the bog in geas and on dry ground in Quest.  (The very
first message is one turn out for the unrelated reason that `timeron <sinking
in mire>` follows a `pause <4000>`: the timer-armed-after-the-tick family.)

A faithful fix is to store the count-up form — an `elapsed` that resets to 0 on
firing, tested `elapsed >= interval` — which is also what Quest saves and what
its `RaiseNextTimerTickRequest` subtracts (`V4Game.Part2.cs:8149`).  geas's own
save format writes `interval` and `timeleft` (`geas-state.cc:138-139`), so the
change is not free, but the two are trivially interconvertible.

### 53. A place is named by its destination's alias in every ASL version

`place <Cellar>` in a room whose `Cellar` is `alias <Basement>` is listed, and
answers to, "Basement" — but only from ASL 3.11 on.  Quest gates both halves on
the version:

```csharp
            if ((ASLVersion >= 311) & string.IsNullOrEmpty(_rooms[roomId].Places[i].Script))
            {
                var PlaceID = GetRoomID(_rooms[roomId].Places[i].PlaceName, ctx);
                …
                else if (!string.IsNullOrEmpty(_rooms[PlaceID].RoomAlias))
                {
                    shownPlaceName = _rooms[PlaceID].RoomAlias;
                }
```

— the listing (`GetGoToExits`, `V4Game.Part2.cs:7806-7818`), and `PlaceExist`
(`ibid. 6584-6594`), which is what decides whether GO TO gets in.  geas applies
the substitution unconditionally (`geas-runner.cc:1439-1448`); the no-script
half of the same condition is already there, the version half is not.  A probe,
the same file twice with only the version line changed:

```
asl-version <210>              asl-version <311>
 > look                         > look
-You can go to Basement.        You can go to Basement.
+You can go to Cellar.
 > go to cellar                 > go to cellar
-You can't go there.            You can't go there.
+You are in Basement.
 > go to basement               > go to basement
-You are in Basement.           You are in Basement.
+You can't go there.
```

The room *name* line is unaffected — an alias always names the room you are in.

Two corpus games are in the window where this matters on its own — 3.11 is the
cutoff, and below 2.80 finding 54 takes over the whole display: The Broken
Mirror (3.10) and Green Light (3.10).  The Broken Mirror is a whole-game
desync: `place <THE ENGLISH>` leads to a room aliased "The english pub", the
walkthrough types the alias, and Quest never lets it into the pub.

### 54. Games below ASL 2.80 have their own room display, which geas does not have

`ShowRoomInfo` hands anything under 2.80 straight to a separate routine:

```csharp
    private async Task ShowRoomInfo(string room, Context ctx, bool noPrint = false)
    {
        if (ASLVersion < 280)
        {
            await ShowRoomInfoV2(room);
            return;
        }
```

(`V4Game.Part2.cs:3688-3694`).  `ShowRoomInfoV2` (`ibid. 1723-2178`) is 450
lines of Quest 2.x, and it differs from the modern path in nearly every line it
prints:

* **Characters get their own sentence**, and get one even when there are none:
  "You can see Simon, Hugo and Tads here." / "There is nobody here."  The
  objects then get a second, separate "There is Nuke Buggy here."  geas prints
  the 2.80+ single merged list.
* **The exits keep their comma.**  Both paths join the place list with "or"
  before the last item, but the old one keeps the comma in front of it —
  `Left(places, lastComma)` at `:2091` against `Left(placeList, lastComma - 1)`
  at `:3865`.  So "You can go to the Inn, or the village House." where geas
  says "the Inn or the village House."  (The *directions* line drops the comma
  in both.)
* **The exits come out in source order.**  V2 walks the room block's lines and
  appends each direction as it meets it (`:1932-1978`); the modern path emits a
  fixed n/s/e/w/ne/nw/se/sw order.  Magic Sword's training room lists
  `northeast` before `northwest` in geas and the other way round in Quest.
* **`out` prints whatever its line's first `<…>` holds.**  V2 takes
  `doorways = GetParameter(line)` with no regard for a script (`:1935-1938`),
  looks that up as a room name, and prints the name back when there is no such
  room.  Fade to White has `out` behind a guard and Quest duly says "You can go
  out to You need to get up first.."
* **No place aliasing** (see finding 53) and **no `quest.lookdesc`**: V2 reads
  `alias`, `prefix`, `indescription`, `description` and `look` out of the room
  block's source lines each time it runs, so none of finding 50's caching
  applies either, and a `look` value is taken whole — see finding 55.

Thirteen corpus games declare a version below 2.80: The Black Forest, A Certain
Oscar, Devil's Bargain, Dream Weaver, Easy Money, Fade to White, The Hobbit,
Koww, Lovesong, Magic Sword, Romantic Music, Space and Uranus or Bust.  Three
of them desync outright, all on the missing place aliasing: Uranus chains five
rooms called some spelling of "Asteroid Surface" and never gets off the first
one in Quest; Space's `place <Science Lab>` leads to a room the author aliased
"Sciece Lab"; Magic Sword loses a move in a fight because its exits differ.

### 55. A tag value is cut off at its first semicolon

geas rewrites every tag-property line into the object-property form as it reads
the file — `look <alpha; beta>` becomes `properties <look=alpha; beta>`
(`readfile.cc:349-353`) — and a `properties <…>` list is `;`-separated, so the
value ends at the first semicolon and " beta" becomes a stray flag.

Quest does the same rewrite, and truncates identically — but only from the
version that introduced it: 3.11 for an object's `look`, `examine`, `speak` and
`displaytype` (`V4Game.Part2.cs:3349-3364`), 3.50 for a room's `look`,
`indescription`, `prefix` and the direction tags (`ibid. 1178-1200`).  Below
that the raw field is what gets printed — `_rooms[id].Look` is the fallback
when the property is absent (`ibid. 3879-3890`) — and it still has the whole
value in it.  The probe, one room with `look <alpha; beta>` and one object with
`look <shiny; smooth>`:

```
          x rock          look
v300      shiny; smooth   alpha; beta      (geas: shiny / alpha)
v349      shiny           alpha; beta      (geas: shiny / alpha)
v350      shiny           alpha            (geas: shiny / alpha)
```

Two corpus games have a semicolon inside such a tag: Koww (2.00, two object
`look`s the walkthrough never reaches) and Magic Sword (2.17), whose training
room is

```
    look <To the northwest is a room where you fight magical replicas of enemys; using weapons.|n And to the northeast is the room where you fight replicas of enemys; using spells.>
```

of which geas prints the first eight words and stops.

### 56. A `type <…>` line is applied where it stands, not before the object's own tags

geas resolves an object's inherited types first and then lets the object's own
properties override them, whatever the source order.  The comment in
`block_property` says so, and claims it is what Quest does:

```c++
  /* A property set directly on the object always wins over one it inherits from
   * a type, as in Quest -- regardless of whether the `type <...>` line precedes
   * or follows the property line in the source.  So resolve inherited types
   * first, then let the object's own properties override.  (For the common
   * type-first ordering this is identical to a single in-order pass.) */
  for (const GeasBlock::dir &d: block.obj_prop)
    if (d.is_type)
      get_type_property (d.a, propname, bool_rv, string_rv);
  for (const GeasBlock::dir &d: block.obj_prop)
    if (!d.is_type && ci_equal (d.a, propname))
      { string_rv = d.b; bool_rv = d.bv; }
```

(`geasfile.cc:431-452`, and `block_action` at `:587-607` does the same for
actions.)

Quest has no such precedence.  It reads a `define object` block line by line and
writes each property as it comes; a `type <…>` line is a property source like
any other, expanded in place —

```c#
else if (BeginsWith(_lines[j], "type "))
{
    …
    var PropertyData = await GetPropertiesInType(await GetParameter(_lines[j], _nullContext));
    await AddToObjectProperties(PropertyData.Properties, _numberObjs, _nullContext);
    for (int k = 1, loopTo4 = PropertyData.NumberActions; k <= loopTo4; k++)
        await AddObjectAction(_numberObjs, PropertyData.Actions[k].ActionName,
            PropertyData.Actions[k].Script);
```

(`V4Game.Part2.cs:3398-3421`), and both `AddToObjectProperties` and
`AddObjectAction` (`V4Game.cs:3784-3816`) overwrite an entry of the same name.
So the last writer wins, and a `type` line *below* a tag beats it.

The probe — one type `jacket` with `prefix = a`, two objects that name it on
either side of their own `prefix <a black>`:

```
                                        geas                    qv4
prefix then type   (sweater)   a black sweater         a sweater
type then prefix   (coat)      a black coat            a black coat
```

It is not a prefix rule; a plain `properties <colour=black>` before
`type <jacket>` (which sets `colour = grey`) reads back `black` in geas and
`grey` in Quest.  Actions behave the same way: an `action <rub>` written above
`type <shiny>` runs the type's script in Quest and the object's in geas.

One corpus game does this.  Operation: Sleepover (3.50) has a wardrobe of
garments that set a prefix or an article and then include a clothing type that
sets its own, eight of them in all:

```
    define object <redtrunks>
        alias <shorts>
        prefix <a pair of red>
        type <tights>          ← prefix = a pair of
```

so its object lists and its inventory lose an adjective each time — `a black
sweater` → `a sweater`, `a Supergirl cami` → `a cami`, `red panties` → `a pair
of panties`, `a pair of red shorts` → `a pair of shorts`.

The fix is a single in-order pass in both `block_property` and `block_action`,
taking `is_type` entries as they come; the two-pass form stays correct for the
type-first ordering, which is what every other corpus game writes.

### 57. Timers tick at the turn's first `wait`, not at the end of the turn

`SendCommand` starts the turn, waits for it to *suspend*, and only then ticks:

```c#
_turnSuspendedTcs = new TaskCompletionSource();
_ = HandleCommandAsync(command);
await _turnSuspendedTcs.Task;
if (elapsedTime > 0) await Tick(elapsedTime);
else RaiseNextTimerTickRequest();
```

(`V4Game.Part2.cs:78-88`).  A turn suspends when it finishes — or at its first
`wait` or `pause`, which is what those do.  So in a turn that waits for a
keypress the timers fire *at the wait*, and the rest of the turn's script runs
after them.  geas has no suspension: `wait_keypress` returns and `tick_timers`
runs when the whole turn is over (`geas-runner.cc:7725-7770`).

```
> arm
> z
TICK
> go
A                        A
press any key            press any key
TICK                     B
B                        TICK
```

There is a second, quieter half.  `Tick` skips a timer on the turn it was armed:

```c#
if (_timers[i].BypassThisTurn)
{
    // don't trigger timer during the turn it was first enabled
    _timers[i].BypassThisTurn = false;
}
```

(`ibid. 118-123`), and geas has the same rule.  But a `timeron` that runs *after*
the turn's `wait` has already passed that turn's `Tick`, so the flag survives
into the next turn and is spent there instead — the timer starts one whole turn
later than it does in geas.

King's Quest V is built out of exactly this pattern.  Its rooms arm a countdown
from a `script` block that opens with `wait <press any key>`:

```
	script {
		if not flag <bandit_temple_first> then {
			wait <press any key>
			do <desert bandit>          ← timeron <bandit_temple>
		}
```

so every cut-scene it drives runs one turn earlier in geas than in Quest, and
the walkthrough's `look`s shuffle around it:

```
 > hide
 Graham hears the approaching horses and decides to hide behind the boulder.
 > look
 …
+> look
+…
 Two bandits riding horses approach the opening in the cliff wall.
```

The bandits, the rat that gnaws Graham free in the inn basement, and the wolf
that takes Cedric on the snowy slope all move; the wolf ends up *inside* the
harp scene in Quest, printed at one of its `wait`s.  17 diff lines, one game.
Eleven corpus games write a `timeron` within a dozen lines of a `wait`, King's
Quest V by far the most (11 of them); the rest arm timers the walkthrough never
lets fire, or fire them in turns with nothing else in the way.

**Fixed**, and as a change to `run_command`'s shape rather than to
`tick_timers`.  `run_command` now takes the host's `elapsedTime > 0` as a
`tick_this_turn` flag, arms `turn_tick_pending` with it, runs the turn body and
calls `suspend_turn` at the close; `suspend_turn` ticks only if the flag is
still set and clears it, so the first suspension of the turn spends the tick and
the rest are no-ops — exactly what `TrySetResult` on a completed
`TaskCompletionSource` is.  The suspension points are `wait` (`geas-runner.cc`,
before the keypress clears the screen, so the tick lands while the game still
sits at the prompt), `pause`, a synchronous `playsound`, and the `|w` code, which
is printed by `GeasInterface::print_formatted` on the far side of the engine and
so reaches the tick through the new `GeasRunner::turn_suspended` hook.  King's
Quest V's 17 diff lines are gone and the wolf now prints inside the harp scene's
`press any key`, as it does in Quest; `Beam` and `SirLoin3` came out byte-identical
too.  `fixtures/timerwait.asl` covers both halves.

The three remaining suspension points — a yes/no `ask`, a menu and `enter` — are
deliberately left un-ticked.  They differ from `wait` in that they *print* while
they wait, and the two engines print at different moments: Quest hands the
question to the host and suspends, so its tick lands *before* the prompt, while
geas's `make_choice` and `choose_yes_no` print the prompt and read the answer in
one call, leaving nowhere to put the tick but after it.  Neither order can match
the other; the semantic difference is confined to a script that answers a
question and then reads a timer it armed in the same turn; and ticking there cost
three corpus walkthroughs their timing for no measurable gain.

### 58. `stdverbs.lib` is skipped, and geas does not in fact implement it

**Fixed.**  The library is bundled and spliced in the way `typelib.qlb` already
was — `stdverbs_builtin.hh` (93 lines, verbatim), `is_stdverbs` and the
`handle_includes` arm that recurses into it (`readfile.cc:955-960, 1083-1090`).
Getting it to behave took eight further engine fixes, listed at the end of this
entry; what follows first is what was wrong.

`handle_includes` threw two of Quest's stock libraries away rather than
loading them:

```c++
/* The standard Quest verb libraries are built into Geas; don't try to
 * load them from disk. */
if (is_builtin_quest_library (param_contents (tok)))
  continue;
```

with

```c++
return base == "stdverbs.lib" || base == "net.lib";
```

(`readfile.cc:939-990`, as it was).  They were not built into geas.
`stdverbs.lib` is 61 lines of `!addto game` (`Libraries/stdverbs.lib:12-60`)
and geas answered barely half of it, in its own words.  The whole library
against a bare room with one takeable rock in it, before the fix:

```
> wait          -Time passes...                    +Time passes.
> jump          -I don't understand your command.  +You jump, but nothing happens.
> listen        -I don't understand your command.  +You can't hear much.
> xyzzy         -I don't understand your command.  +Surprisingly, absolutely nothing happens.
> sleep         -I don't understand your command.  +No time for lounging about now.
> push rock     -You can't do that.                +You can't push it.
> read rock     -Nothing out of the ordinary.      +You can't read it.
> smell rock    -You can't do that.                +You sniff, but it doesn't smell of much.
> turn on rock  -You can't do that.                +You can't turn it on.
> ask rock about thing
                -I don't understand your command.  +You get no reply.
> take rock. drop rock
                -You don't see any rock. drop rock.
                                                   +You pick it up.
                                                   +You drop it.
```

The last row is the library's other job: its final two `command` entries split a
line on `.`, `,`, `then`, `and then` and `exec` each half, so every
stdverbs game accepts chained commands and geas accepted none of them.  The
splitter is blind to what the full stop is doing, so it also chops a command
that merely contains an abbreviation:

```
	command <#stdverbs.command#. #stdverbs.command2#; _
```

(`Libraries/stdverbs.lib:51`).  Ponyville's walkthrough types `speak to mr.
cake`, which Quest runs as `exec <speak to mr>` — that resolves Mr. Cake by
abbreviation and prints his speech — followed by `exec <cake>`, which is
nothing, so the turn ends with an unprompted

```
I don't understand your command. Type HELP for a list of valid commands.
```

`give jar of something to mr. cake` splits the same way.  Both diverged in
geas's favour, which was worth knowing before the library was bundled:
shipping `stdverbs.lib` makes two Ponyville turns read *worse*, and it does so
correctly, because that is what Quest does.

The library's twenty-nine `verb <…>` defaults also read
`#(quest.lastobject):article#`, so Quest names the object where geas said
"that".

39 of the 111 corpus games `!include <stdverbs.lib>` — a third of the corpus,
and by far the widest surface of any finding here.  The walkthroughs are
written to win, so they rarely type a verb that only has a default answer;
what the diffs actually caught was Ponyville's five `wait`s and its two split
commands (12 lines).  A game
that leans on the library — one that expects `x rock. take rock` to work — was
not playable in geas at all.

`net.lib` was skipped on the same line, and still is.  It is the multiplayer
library: it
defines one type, `giveable`, whose `give to anything` action is entirely
`msgto`/`player%userid%` machinery that means nothing single-player, so
skipping it costs nothing; three corpus games include it (Assassination, Get
Out Of The House, Metal Sonic's Quest) and none defines an object of that type.

The other three libraries a corpus game names — `standard.lib` and
`movecont.lib` (A Certain Oscar), `q3ext.qlb` (Get Out Of The House) — are not
on the skip list, so geas tries to read them from disk, fails, and includes
nothing.  QuestViva ships `standard.lib` and `q3ext.qlb` and does load them.

#### What bundling it took

The library is shipped the way `typelib.qlb` is, as a bundled string spliced
through the same include pipeline.  Splicing it in is one line; making it
*behave* like Quest's copy needed eight more fixes, every one of them a real
divergence the empty stub had been hiding.  Fixtures `stdverbs.asl` /
`.cmd` / `.expected` cover them, cross-checked against qv4.

* **A library's `!addto game` lines go at the end of the block, not the
  front.**  `preprocess` pushed them out right after the `define game` header;
  Quest splices them in immediately before the block's `end define`
  (`V4Game.cs:1497-1568`).  That is not cosmetic — within a block Quest matches
  commands and verbs in file order, so at the front the library's would beat
  the author's own.  `readfile.cc:1168-1210`.
* **The `lib` tag.**  A library whose own asl-version is ≥ 3.11 gets `lib `
  prefixed to its `startscript` lines, and ≥ 3.92 to its `command` and `verb`
  lines (`V4Game.cs:1445-1470`, 1553-1567).  `readfile.cc:library_asl_version`,
  `mark_library_addtos` (`:986-1049`).
* **Two more dispatch passes.**  `ExecCommand` tries the game's commands, then
  the game's verbs, then the `lib` commands, then the `lib` verbs
  (`V4Game.Part2.cs:4224-4241`); geas ran one undifferentiated pass in file
  order.  With the library at the end of the block that is nearly the same
  thing — but only nearly, and `command <#stdverbs.command#.>` matches
  *anything* ending in a full stop, so on a single pass it eats a game's own
  commands whenever the player types a stop.  `GeasBlock::cmd_entry::is_lib`
  (`geasfile.hh:83-86`), `run_commands`/`try_game_verb` (`geas-runner.cc:3367`,
  `:3409`) and the two extra calls in `try_match` (`:3519-3524`).
* **`lib startscript` runs first, and in reverse order,** for ASL ≥ 3.11 — the
  innermost library initialising first (`V4Game.Part2.cs:7967-7997`).  Below
  3.11 the tag means nothing and they run in place.  `geas-runner.cc:2400-2427`.
* **`quest.lastobject` was never set.**  All twenty-nine verb defaults read
  `#(quest.lastobject):article#`.  Quest clears the variable on entry to
  `Disambiguate` and writes the resolved name into it before returning
  (`V4Game.cs:4623, 4646-4689`).  `geas-runner.cc:3199-3243`.
* **A property the object hasn't got expands to `!`, not to nothing.**
  `GetStringContents` reaches `GetObjectProperty` with its default
  `logError = true`, which logs the miss and returns `"!"`
  (`V4Game.Part2.cs:2581`, `V4Game.cs:6249-6252`); geas expanded it to the
  empty string.  Invisible until the library made every unhandled verb print
  an object's `article`: an object that has none answers `You can't buy !.` in
  Quest.  `geas-runner.cc:7690-7712`; two fixtures re-blessed (`[]` → `[!]`),
  oracle-confirmed.
* **`exec` has to answer a command that matches nothing.**  `ExecuteExec` hands
  the line to the whole of `ExecCommand`, error path included
  (`V4Game.Part2.cs:2229, 4574`); geas silently dropped it.  The splitter turns
  Ponyville's `speak to mr. cake` into `speak to mr` plus a stray `cake`, and
  Quest answers the stray half — which is the "worse" Ponyville reading above.
  `geas-runner.cc:5354-5374`.  Two riders: an *empty* exec is not an error and
  does not even end a paragraph, because `ExecCommand` returns before both the
  error path and its closing `Print("")` (`:4134-4137`; The Quest to find The
  Dark Hills execs one from a `choice` whose "no" branch does nothing —
  `geas-runner.cc:4672-4678`), and every `ExecCommand` ends with that
  `Print("")` (`:4606`), which geas emitted at the top of `run_command`
  instead, so an exec'd command was butted up against whatever followed
  (`geas-runner.cc:5375-5382`).
* **`save` is a built-in command of the engine's** (`V4Game.Part2.cs:4468`),
  even though the dialog belongs to the frontend.  A typed SAVE never reaches
  the engine — geasglk intercepts it (`geasglk.cc:316`) and so does the
  runner's `--save-scum` — but `exec <save; normal>` does, and Dark Hills
  offers exactly that from a `choice`.  Without it the turn ended in
  `I don't understand your command.`  `geas-runner.cc:4500-4510`.

Ponyville's five `wait`s now read `Time passes.`  The library defines its own
`command <wait>`, and a lib command is tried before the engine's built-ins, so
it wins over geas's invented `wait` command (finding 3) and its `defaultwait`
text (finding 4) — which is why that golden changed and why the change is not
either of those findings being fixed.  All 39 stdverbs games were re-blessed;
the 12-line row for this finding is gone from the table above.

### 59. `lose <obj>` only drops the object if it is being carried

**Fixed.**  The `get_obj_parent` test is gone (`geas-runner.cc:5750-5760`);
fixture `loseanywhere.asl` / `.cmd` / `.expected`.

`PlayerItem` takes the object of that name and moves it, with no interest in
where it was:

```csharp
else
{
    MoveThing(_objs[objId].ObjectName, _currentRoom, Thing.Object, ctx);

    if (!string.IsNullOrEmpty(_objs[objId].LoseScript))
    {
        await ExecuteScript(_objs[objId].LoseScript, ctx);
    }
}
```

(`V4Game.Part2.cs:6652-6660`).  `lose` is not "drop what you are holding", it is
"move this object to the room I am standing in", and it runs the object's `lose`
script whether or not the player ever had it.  geas puts a condition in front of
both:

```c++
bool is_object = (asl_version_ >= 280 && state.obj_records (tok) != NULL);
if (is_object && ci_equal (get_obj_parent (tok), "inventory"))
```

(`geas-runner.cc:5670-5671`, as it was).  The guard was original geas — its
first comment read "If a real object of this name was being carried, drop it to
the room" — and it has no counterpart in Quest.  `give`, three hundred lines
up, has never had such a guard, so the pair was asymmetric inside geas too.

A room with the object somewhere else entirely:

```
define room <shed>
    define object <spade>  prefix <a>  lose { msg <The spade clatters down.> }  end define
    define object <fork>   prefix <a>                                          end define
end define
define procedure <p1>  lose <spade>  end define
```

| in `hall` | geas (before) | qv4 |
| --- | --- | --- |
| `probe one` | *(nothing)* | `The spade clatters down.` |
| `look` | `There is a rock here.` | `There is a rock and a spade here.` |
| `probe two` | *(nothing)* | *(nothing)* |
| `look` | `There is a rock here.` | `There is a rock, a spade and a fork here.` |

Sutekh turns on this.  Kissing the statuette in the orchard runs

```
action <kiss> {
    msg <...you find yourself transported...to the renowned Chelsea Physick Garden...>
    lose <brass lantern>
    goto <Chelsea Physick Garden>
}
```

and the brass lantern is not in the player's hands, it is lying on the eastern
lawn two rooms away.  Quest yanks it into the orchard, which is the point — the
lantern's `rub` action is the way out of the locked-off part of the map, and the
author is putting it where the player will trip over it.  geas left it on the
lawn, so the orchard listing was short by one object, the lawn listing long by
one, and `take brass lantern` succeeded on the lawn where Quest answers `I
can't see that here.`

54 of the 111 corpus games use `lose <…>`; the great majority of those calls
really are dropping something held, so only Sutekh showed it (4 diff lines).
The test is deleted and the lose script now runs unconditionally, exactly as
`st_give` always did; Sutekh's golden was re-blessed and its row is gone from
the table above.

### 60. `action <name>` with no script is registered instead of rejected

**Fixed** with finding 29: `geasfile.cc:279-296` drops an action whose script
is empty after trimming, so the same-named property is what the verb finds.

```csharp
var name = Strings.LCase(await GetParameter(actionInfo, ctx));
var ep = Strings.InStr(actionInfo, ">");
if (ep == Strings.Len(actionInfo))
{
    LogASLError("No script given for '" + name + "' action data", LogType.WarningError);
    return;
}
```

(`V4Game.cs:3934-3940`).  A line that is nothing but `action <watch>` never
becomes an action at all; it is an authoring error and Quest says so in the log.
geas stores it with an empty script:

```c++
b.obj_act.push_back ({false, name,
                      (c2 + 1 < line.length ()) ? line.substr (c2 + 1) : "",
                      true});
```

(`geasfile.cc:280-283`).

That empty action then swallows the verb.  `ExecVerb` looks for an action of the
verb's name first and only reads the like-named property `if (!foundAction)`
(`V4Game.cs:3025-3050`), so in geas the action is found, the empty script runs,
and nothing is printed — no property text, and no verb default either:

```
verb <watch> msg <You can't watch that.>
define object <telly>  properties <watch = You watch the telly.>  action <watch>  end define
define object <radio>                                            action <watch>  end define
```

| | geas | qv4 |
| --- | --- | --- |
| `watch telly` | *(nothing)* | `You watch the telly.` |
| `poke telly` | `You poke the telly.` | `You poke the telly.` |
| `watch radio` | *(nothing)* | `You can't watch that.` |

Sutekh's television is the corpus case: it carries `properties <watch = You
watch as a woman in a white dress paces purposefully about in a priory
garden.>`, its `switch off`/`switch on` actions rewrite that property as the
picture changes, and a stray `action <watch>` on the line between them makes
geas print none of it (3 diff lines).  Thirteen corpus games have at least one
bare `action <…>` line — Awakening Dead 2, Annabel, Get Out Of The House, Dark
Hills, Escape House, Ghost Light, Red Sauce Office, Shadow Masters, Michael's
Game, Red Sauce Monday, Sutekh, The Lazst Resort, Zombies Attack — so the same
verb goes quiet in all of them, whether or not the walkthrough types it.

The fix is one condition at the push site: an `action` line whose script is
empty is dropped, and `debug_print`ed, rather than stored.

### 61. `is_held` tests the direct parent, so `take` misses what is in a carried container — and cannot say `alreadytaken`

**Fixed** with finding 27: `held_in_container` (`geas-runner.cc:909-919`)
walks the parent chain, `is_held` consults it from ASL 3.91, and the refusal is
`alreadytaken` from 4.10, `badthing` from 3.91 and `baditem` below
(`:4498-4506`).

```c++
bool geas_implementation::is_held (const string &name) const
{
  if (const vector<size_t> *v = state.obj_records (name))
    for (size_t idx: *v)
      if (ci_equal (state.objs[idx].parent, "inventory"))
	return true;
```

(`geas-runner.cc:858-863`).  Three thousand lines down, `got` does the same job
with `room_of_parent (o.parent)` and a comment explaining exactly why:

> `room_of_parent` walks the container chain, which is how Quest's separate
> ContainerRoom field behaves: DoAddRemove copies the container's ContainerRoom
> onto whatever is put inside it (`V4Game.cs:2117-2121`), so something in a
> carried container still counts as got

(`geas-runner.cc:6395-6399`).  `MoveThing` does the same thing recursively for
every child of what it moves, from ASL 3.91 on (`V4Game.cs:6643-6653`).  So in
Quest, picking up a coat puts everything in its pockets in the inventory too,
and `is_held` is the one place in geas that does not know it.

What that costs is the `take` fast path:

```c++
      /* Already carrying it? ... */
      if (is_held (object))
	{
	  print_formatted ("You already have it.");
	  return true;
	}
```

(`geas-runner.cc:4029-4035`).  Quest's `ExecTake` scopes its lookup to the
current room, and when that fails, at ASL ≥ 4.10, retries in the inventory and
answers `AlreadyTaken` (`V4Game.Part2.cs:5132-5155`).  For anything nested in a
carried container the two disagree: Quest says you already have it, geas
believes it is still out there and runs the implied-removal dance instead.

TARDIS Escape:

```
> take sonic screwdriver
-(first removing it from pocket)                +Look in your inventory, you greedy pig!
-*
-You acquired the Sonic Screwdriver! Fantastic!
```

The screwdriver's `parent <pocket2>`, the pocket's `parent <trenchcoat2>`, and
the trenchcoat was picked up three turns earlier.

The second half is the message.  `print_formatted ("You already have it.")` is
hard-coded, so it cannot be overridden — and `alreadytaken` is one of four error
names Quest's `SetUpUserDefinedPlayerErrors` switch accepts
(`V4Game.Part2.cs:1610`) that geas has no call site for at all:

| error name | Quest default | geas |
| --- | --- | --- |
| `alreadytaken` | `You already have that.` | hard-coded `You already have it.` |
| `alreadyput` | `It is already there.` | never raised |
| `badput` | `You didn't specify what you wanted to put #quest.error.article# on or in.` | never raised |
| `cantput` | `You can't put that there.` | never raised |

`display_error` itself is fine — it scans the game block before falling back to
the table (`geas-runner.cc:1293-1316`), so the names would work the moment
something asked for them.  In the corpus, TARDIS Escape sets all four bar
`badput` and Briny Blue sets `alreadytaken`; the fix is to route the take
message through `display_error ("alreadytaken", object)` and to give `is_held`
the same `room_of_parent` walk `got` already has (2 diff lines).

### 62. Below ASL 280, `look`/`examine` resolve the noun by a completely different rule

geas has one object-name resolver.  Every `#@object#` binding goes through
`dereference_vars` → `get_obj_name` (`geas-runner.cc:3165-3181`), which searches
inventory and the room, matches the block name *or* any alias/alt name, folds
case, trims surrounding whitespace and forgives a leading article.  That is a
good model of Quest's `Disambiguate` — but Quest only uses `Disambiguate` for
`look`/`examine` from ASL 280 on.  Below that, `ExecLook` takes a wholly
separate path (`V4Game.Part2.cs:4924-4936`):

```csharp
if (BeginsWith(item, "the ")) { item = GetEverythingAfter(item, "the "); }
lookLine = RetrLine("object", item, "look", ctx);
```

and `RetrLine` resolves the noun with `GetThingBlock`, which is a raw scan for a
definition whose name matches exactly (`V4Game.cs:6441-6448`):

```csharp
if (((Strings.LCase(_objs[i].ObjectName) ?? "") == (Strings.LCase(name) ?? "")) &
    ((Strings.LCase(_objs[i].ContainerRoom) ?? "") == (Strings.LCase(room) ?? "")))
```

No alias, no alt names, no trimming, no article tolerance beyond that one
up-front `the ` strip — and the room is the *only* container searched, since
`RetrLine` passes `_currentRoom` and nothing else.  `examine` and `x` are routed
into the same code, because below 280 they are rewritten to `look ` rather than
sent to `ExecExamine` (`V4Game.Part2.cs:4414-4433`).

A probe with `asl-version <217>`, three objects `<Boiler >`, `<The Abyss>` and
`<Rock> alias <stone>`, all in the starting room (geas left, qv4 right):

| command | geas | qv4 |
| --- | --- | --- |
| `look at boiler` | `BOILER LOOK` | `I can't see that here.` |
| `look at abyss` | `ABYSS LOOK` | `I can't see that here.` |
| `look at the abyss` | `ABYSS LOOK` | `I can't see that here.` |
| `look at rock` | `ROCK LOOK` | `ROCK LOOK` |
| `look at stone` | `ROCK LOOK` | `I can't see that here.` |
| `x stone` | `ROCK LOOK` | `I can't see that here.` |

Raising the same file to `asl-version <410>` flips two rows and fixes the rest:
`boiler` and `the abyss` both resolve, `stone` resolves, and `rock` — the real
name behind an alias — stops resolving, which is finding 14's rule.  So the
pre-280 path is not merely stricter, it is the *inverse* of the modern one on
aliases, and it is the only place in Quest where an object whose name begins
with `The ` is unreachable: the typed `the ` is stripped before the compare, so
`the abyss` and `abyss` both fail against the stored `The Abyss`.

Blackforest hits two of the three at once (4 diff lines).  Its boiler is
`define object <Boiler >` — a trailing space — and its `look at boiler` prints
the description in geas and `I can't see that here.` in qv4; the same room's
`define object <The Abyss>` fails `look at the abyss` in qv4 for the other
reason.  Both objects are listed in the room description on both sides, so this
really is the lookup and not scope.

The whitespace half is a deliberate, documented deviation already: geas
registers every block under a trimmed name and says so, citing "Something 'Bout
A Hex", whose journal is `define object <Journal >` while the knapsack hands it
over as `give <Journal >` and a later test spells it `if got <Journal>`
(`geasfile.cc:53-66`).  Folding the two spellings together is what makes that
game work, and it costs nothing above ASL 280 because Quest's own
`Disambiguate` trims too.  The alias and `The `-prefix halves are not covered by
that argument, and they only bite below 280.

13 of the 111 corpus games are pre-280: Koww (200); Devil's Bargain, Easy Money,
Fade To White, The Hobbit, Lovesong, Romantic Music, Space, Uranus (210);
Blackforest, A Certain Oscar, Dream Weaver, MagicSword Part 1 (217).  A fix
would gate the alias/alt-name arm of `get_obj_name` — and the inventory half of
the search list — on `asl_version_ >= 280` for the look/examine handlers only,
the way `st_use` already gates its game-block fallback at
`geas-runner.cc:3932`.

While proving this, the same probe turned up a second pre-280 split that the
corpus does not currently exercise: `take` below 280 does not move anything.
`ExecTake`'s else arm scans the object block for a line starting with `take` and
runs the remainder as a *script*, with no default message and no `PlayerItem`
call (`V4Game.Part2.cs:5242-5261`), so a bare `take` tag is a no-op and the
object stays in the room, where geas prints `You pick it up.` and moves it to
the inventory.  Below 280 an author had to write the `give` themselves.  Worth
folding into the same version gate if 62 is fixed.

### 63. `place locked <...>` is not parsed, so the exit vanishes entirely

Quest strips the `locked ` keyword *after* the direction word, and `place` is
one of the direction words `AddExitFromTag` accepts (`RoomExits.cs:120-146`):

```csharp
else if (_game.BeginsWith(tag, "place ")) { tag = …; thisDir = Direction.None; }
…
if (_game.BeginsWith(tag, "locked ")) { await roomExit.SetIsLocked(true);
                                        tag = _game.GetEverythingAfter(tag, "locked "); }
if (Strings.Left(Strings.Trim(tag), 1) == "<") { @params = Split(GetParameter(tag), ";"); … }
```

so `place locked <Dest; message>` is a place exit to `Dest` that starts locked
and refuses with `message`.  geas's `get_places` reads the token after `place`
and gives up if it is not a parameter (`geas-runner.cc:1415-1422`):

```c++
if (tok == "place")
  {
    tok = next_token (line, c1, c2);
    if (!is_param(tok))
      {
	gi->debug_print ("Expected parameter after 'place' in " + line);
	continue;
      }
```

`locked` is not a parameter, so the whole line is discarded.  The exit is not
listed, `go to <dest>` falls off the end of the `current_places` scan and prints
`badplace`, and the author's lock message is never reachable.
`exit_object_names` drops it the same way (`geas-runner.cc:1629-1642`), so the
place also has no exit object for hyperlinks or `lock`/`unlock`.

Probe, one plain place and two locked ones — with and without a message (geas
left, qv4 right):

| line | geas | qv4 |
| --- | --- | --- |
| exits | `You can go to Shop.` | `You can go to Shop, to Vault or to Cellar.` |
| `go to vault` | `You can't go there.` | `The vault is sealed.` |
| `go to cellar` | `You can't go there.` | `The exit is locked.` |
| `go to shop` | works | works |

So a locked place *is* advertised in the exits line — locking gates traversal
only, exactly as it does for the directional exits geas already handles at
`geas-runner.cc:1816-1841`.  The second field is the lock message, the same
`<dest; lockmessage>` shape `split_exit_dest` documents for 4.10, not the
`prefix; dest` shape a plain `place` uses below 3.53.

One corpus game, one line: Skate Ur Ass Off's Skate Lobby declares
`place locked <Half Pipe Central; You have yet to unlock this Skate Zone.>`, so
geas's lobby lists three exits where Quest lists four and `go to half pipe
central` answers `You can't go there.` instead of the author's refusal (2 diff
lines, plus the exits line already counted under the 4.10 single-line bucket).
It is a demo build with no unlock anywhere in it, so nothing further is gated
behind the missing exit.

The fix is to accept an optional `locked` token after `place`, hand the second
`;` field to the lock machinery instead of treating it as a prefix, and key
`exit_locked` on the destination name for directionless exits.

### 64. `|w` waits without ending the line

Quest's `Print` walks the string a character at a time and treats `|w` as a
flush point:

```csharp
if (Strings.Mid(txt, i, 2) == "|w")
{
    DoPrint(printString);
    printString = "";
    printThis = false;
    i = i + 1;
    await DoWaitAsync();
}
```

(`V4Game.Part2.cs:6745-6753`.)  Everything before the code is printed as one
paragraph, the game waits for a key, and whatever follows the code starts the
next paragraph.  The bare `|c` immediately below it does the same thing with a
screen clear instead of a wait.

geas handles `|w` inside `print_formatted`, and only waits:

```c++
            case 'w':
              wait_keypress("");
              changed = false;
              break;
```

(`geas-runner.cc:7866-7869`.)  Nothing is flushed, so the text on either side
of the code runs together on one line.  `GeasGlkInterface::wait_keypress`
prints its message and blocks (`geasglk.cc:1135-1141`) but emits no newline of
its own, so this is what the player sees under Glk too, not a harness artefact.

`msg <Alpha|w Beta>` at `asl-version <410>`:

```
geas  Alpha Beta
qv4   Alpha
      _Beta
```

(the leading space survives into the second paragraph, as it does in Quest).

Nine corpus games use `|w`, and in seven of them every occurrence is followed
by `|c`, `|n`, or the end of the string, where the line ends anyway and the two
engines agree.  Magic Sword is the one that writes prose straight after the
code:

```
	msg <The leader laughs at you, ... their forms starts changing.
	     |cg|iPress any key to continue|cb|xi|w They grow taller, and wings
	     sprout from their backs. ...>
```

(`MagicSword.asl:1352`; the author uses the code four times this way).  The
replay reaches one of them, and geas prints the author's own "Press any key to
continue" with the next sentence stuck to the end of it (2 diff lines).

The fix is to end the line at `|w` — and at the clearing `|c`, which has the
same flush in Quest — and to suppress `print_formatted`'s closing newline when
the code was the last thing in the string, so a trailing `|w` does not leave a
blank line behind.

### 65. Below ASL 2.80 the inventory is the item table, in declaration order

Fell out of finding 6: once both engines print the same sentence, what is left
is that they do not agree on what goes in it.  From 2.80 up Quest walks `_objs`
and takes everything whose `ContainerRoom` is literally `"inventory"`
(`V4Game.Part2.cs:4500-4522`); below it, it walks `_items` instead and takes
every entry whose `Got` is set (ibid. 4526-4532):

```csharp
for (int j = 1, loopTo2 = _numberItems; j <= loopTo2; j++)
    if (_items[j].Got)
        invList = invList + _items[j].Name + ", ";
```

`_items` is the game block's `items <a; b; c>` line, so the order is the
*declaration* order, whatever order the player picked things up in — and an
object that happens to be in the inventory is not listed at all.

geas has no item table: `get_inventory` (`geas-runner.cc:7250-7275`) returns
the inventory's objects in definition order and then appends `state.items` in
the order `give` reached them.  Both halves show:

```
> i                                    (Dream Weaver, 217)
-Lighter, Hooka, Key, Locket, …        +Hooka, Lighter, Key, Locket, …

> inv                                  (The Devil's Bargain, 210)
-Map, police pass, monkey drawing …    +Map, police pass, photos …
```

— in each case the two engines carry the same things, and Quest's order is the
one the `items <…>` line gives.  The fixture is `itemhide217`, whose
`items <lamp; coin>` and `startitems <coin>` make geas answer `Coin and lamp.`
where qv4 answers `Lamp and coin.`

10 lines across 4 games (Dream Weaver, Devil's Bargain, Venus Flytrap Romantic
Music and What Do You Do), all of them pre-2.80.  Fixing it means parsing the
`items <…>` line — which geas currently ignores, reading only `startitems`
(`geas-runner.cc:2491-2493`) — and using it to order, and to scope, the
pre-2.80 answer.

### 66. A blank `indescription` swallows the room line

**Fixed.**  `look` reads the tag into `tmp` and keeps the answer in
`has_in_desc` (`geas-runner.cc:2063-2078`); from ASL 3.50 it trims the value
first, matching the loader's own trim, and it takes the `indescription` branch
only when what is left is non-empty.  An empty or whitespace-only value now
falls through to the default line, so TARDIS Escape's Console Room announces
itself.

Quest falls back to `You are in <room>.` only when a room has no
`indescription` at all, and the test it uses is
`!string.IsNullOrEmpty(inDescription)` (`V4Game.Part2.cs:3746-3765`).  geas
tests whether the property is *present* (`geas-runner.cc:2063-2081`), so a room
that carries an empty one prints no room line whatsoever.

Two ways to write that empty value, and Quest treats both as absent:

* `indescription <>` is empty on the face of it.
* `indescription < >` is a single space, which survives `GetParameter` — but
  from ASL 3.50 on the loader copies the value into the object-property table
  as well, and `AddToObjectProperties` splits on `=` and `Trim`s what follows
  (`V4Game.cs:4025-4026`), so the space is gone by the time the property write
  reaches back through `case "indescription"` (`V4Game.cs:4111-4118`) and
  overwrites `InDescription` with `""`.

TARDIS Escape's Console Room is the corpus case, and it is the second kind:

```
 > down
+You are in the Console Room.
 There is a coat rack, the Wardrobe Staircase and the Floor Grating here.
```

3 lines in the one game, plus the game's opening.  The fixture is
`indescspace`, which pins all three shapes — a space, an empty value, and a
value ending in a colon, which takes the room name and a full stop.

### 67. `##` is eaten in a room alias and an object `look`

**Fixed.**  `static_eval` grew the empty-name case in both branches — `#` at
`geasfile.cc:930-941`, `%` at `:991-995` — so a conversion character that
closes on the very next character appends itself and moves on instead of
looking up a variable with no name.  RiddleRun's transcript is now identical to
Quest's, and the fixture's room lines read `You are in Room #1.` and `You are
in Room ##2.` on both engines.  The one line of `hashesc` that still differs is
the object `look`, where geas prints `look: A # B and 100% sure` and Quest
`<ERROR>`: that is the known load-time `<ERROR>` limitation described below,
not this finding.

Two conversion characters in a row stand for a literal one.  Quest's
`ConvertParameter` has the case spelt out, and runs the same body for each of
`#`, `%`, `~` and `$`:

```csharp
                if (string.IsNullOrEmpty(varName))
                {
                    result = result + convertChar;
                }
```

(`V4Game.cs:6704-6708` — the same lines finding 51 quotes.)  geas has it in
`eval_string`, which resolves runtime parameters (`geas-runner.cc:7895-7896`,
`:7952-7953`, `:7970-7971`), and did *not* have it in `static_eval`
(`geasfile.cc:894-988`), the load-time twin that resolves definition text.
There `##` is read as a string variable with no name and expands to nothing,
and `%%` as a numeric variable with no name, which reads as `-32768`.

So the identical escape survives a `msg` and is eaten in a room alias.
RiddleRun aliases its five rooms `Room ##1` … `Room ##5`:

```
-You are in Room 1.
+You are in Room #1.
```

10 lines in the one game.  The fixture is `hashesc`, whose second room is
aliased `Room ####2` to pin how many passes have run — one leaves `##`, two
leave nothing — and whose object `look` shows the `%%` case:

```
 > look at sign
-look: A  B and 100-32768 sure
+<ERROR>
```

An object's `look` is converted twice on both sides, which is why Quest
answers that one with `<ERROR>`: its first pass leaves a lone `#`, and an
unpaired conversion character replaces the whole parameter (the behaviour
`static_eval`'s own comment describes).  A room alias gets one pass either
way, so it is the clean comparison of the two.

### 68. `ExpressionHandler` crashes on `2+3*4`, and geas does not

Quest's expression evaluator collapses one operator at a time, and `opNum` — the
index of the operator it picked — is declared *outside* the loop and never reset
(`V4Game.cs:3204-3290`):

```csharp
        var opNum = 0;
        do
        {
            for (int i = 1, loopTo3 = numOperators; i <= loopTo3; i++)
                if ((operators[i] == "/") | (operators[i] == "*")) { opNum = i; break; }

            if (opNum == 0)          // only reached while no * or / has ever been picked
                for (int i = 1, loopTo4 = numOperators; i <= loopTo4; i++)
                    if ((operators[i] == "+") | (operators[i] == "-")) { opNum = i; break; }
```

So once a `*` or `/` has been collapsed, a later pass that finds no `*` or `/`
does not fall through to the `+`/`-` search: `opNum` still holds the *old*
index, which now points past the shortened arrays, and `elements[opNum + 1]`
throws.  `2*3+4` survives (the multiplication is operator 1, and 1 is still a
valid index afterwards); `2+3*4` does not — Quest answers
`[An internal error occurred]` where geas answers 14.

Not reproduced, and not likely to be: an engine that answers a wrong number is
one thing, an engine that reproduces a null-reference crash is another.  No
corpus game's replay reaches it — the three `[An internal error occurred]`
lines in Pyramid of Terror have other causes, and are triaged as QuestViva
defects.  `fixtures/setnum410.asl` records the omission in its header rather
than freezing geas's answer in a golden.

### 69. An operand in exponent notation is read as far as the `E`

`ObscureNumericExps` (`V4Game.cs:3434-3460`) rewrites the `+` or `-` that
follows an `E` before the expression is split, so `1E+18` stays one element:
`2.345E+20` becomes `2.345EX20` for the split and is read back whole.  geas's
number reader stops at the `E`, so `1E+18+0` is 1.

Left alone for the same reason as finding 68 — no corpus game writes an
exponent in a `set numeric`, and the fixture says so instead of pinning the
divergence.

### 70. `not (` and `while (` were missing from the friendly-if conversion

**Fixed.**  `ConvertFriendlyIfs` (`V4Game.cs:353-380`) looks for six pairs —
`if (`, `until (`, `while (`, `not (`, `and (` and `or (` — and geas's
`preprocess()` knew only four (`readfile.cc:1224-1260`).  The absence is not a
silent no-op: the raw `<` that survives the unconverted condition opens a
`<parameter>` as far as the tokeniser is concerned, the search for `then` runs
off the end of the line, and the whole `if` is dropped — neither branch runs.

Kingdom is the regression test, and lost a third of its simulation to it:
`if not ( %V1pop% <= 0 ) then do <V1riot>` meant Village 1 never rioted, never
sent its leader with the tribute and never appeared in the season report, and
`repeat while ( %p% < 3 )` did not loop once.  The loop that recognises the
words also had to become a `while` rather than an `if`, because the word after
one of them may be another (`if not (`, `repeat while (`) and Quest, matching
plain substrings, sees the inner one either way.

### 71. `|sNN` takes two characters, not two digits

**Fixed.**  Quest reads the two characters after `|s` with `int.TryParse`, whose
default `NumberStyles.Integer` skips whitespace at either end, so `"0 "` and
`" 8"` are sizes.  `parse_two_char_int` (`geas-runner.cc:8386-8404`) does the
same.  ChristmaKwanzakkah's title banner ends `|cr! |s0 |cb |xb`, and a stricter
reader printed a literal `|s0` in the middle of the game's first line.
`fixtures/barcodes.asl` has both the tolerant and the malformed forms.

### 72. `action <drop>` is run, where Quest honours only a `drop` tag

`ExecDrop` (`V4Game.cs:5570-5583`) scans the object's own definition lines for
one beginning `drop `, and that raw tag is the only thing it will run.  An
`action <drop>` block is registered like any other action and never consulted,
so the object takes the default drop:

```
Pyramid Of Terror   define object <yellowy blob>  action <drop> { … }
> drop marzipan
-Aha, you drop the yellowy blob, that you and Sutekh both know to be marzipan …
+You drop it.
```

geas runs the action.  Not fixed: the action is where the game unlocks Sutekh's
Chamber Of Cruelty, so under Quest's rule the walkthrough cannot finish — the
trapdoor never opens and the last six commands answer `I can't see that here.`
That is 20 of Pyramid of Terror's 27 diff lines, and every one of them is
geas playing the game the author meant and QuestViva playing the game Quest
wrote.  Reproducing it would mean deciding that this game is unwinnable, which
wants more evidence than one engine's source.

### 73. A room has no suffix

**Fixed.**  `regen_var_room` (`geas-runner.cc:2869-2892`) built the displayed
room name as prefix + alias + suffix.  Quest has a `Suffix` field on objects and
on characters and none on a room: `ShowRoomInfo` assembles the name from the
alias and the prefix and nothing else (`V4Game.Part2.cs:3724-3742`), so a
`suffix <…>` tag on a room is stored in the property table like any other and
never displayed.  No corpus game writes one; `fixtures/lookprop.asl` pins it.

## Direction uncertain

### `take` out of a container prints an implied removal

geas prints `(first removing <article> from <parent>)` and then the parent's
`remove <…>` message; Quest prints neither, because `ExecTake`'s
`isInContainer` is declared `false` and never assigned
(`V4Game.Part2.cs:5165-5215`).  `ExecDrop` does set the same flag
(`V4Game.cs:5552-5610`), so this looks like a bug that Axe's VB6 had and
QuestViva translated faithfully — which would make geas's behaviour a
deliberate improvement rather than a divergence to fix.

360 lines across 17 games, in matched pairs: the parenthetical is always
followed by `Done.`, geas's `defaultremove` text, and all 174 `Done.` lines in
the corpus are one of these.  TARDIS Escape is the first-divergence case; over
half the total is downstream of Wizard's finding-22 desync rather than fresh
evidence.

### `<ERROR>` is not produced at load time

Quest resolves a room's `look` text once, at load (`SetUpRoomData` calls
`GetParameter`, `V4Game.Part2.cs:1170-1172`), so an unmatched conversion
character there stores the literal `<ERROR>` as the description.  geas's
`static_eval` deliberately returns the input unchanged instead
(`geasfile.cc:899-923`): its results are re-emitted as `properties <name=value>`
lines, and `next_token` (`readfile.cc:56-62`) ends a `<…>` token at the first
`>`, so storing `<ERROR>` would read back as `<ERROR`.  Throwing was worse — it
escaped to `set_game`'s catch and left The Statue of Riddles silent from the
first turn, for one stray `%` in `100% empty`.

2 lines, 2 games: Pure Chaos's `look <It's about $1000000.>` and The Statue of
Riddles' `100% empty` dome.  geas prints the author's text; Quest prints
`<ERROR>`.  Printing the author's text is the better outcome for a player, and
fixing it properly means changing how properties are serialised, so it is
recorded rather than filed as a bug.

### Below ASL 2.80 a `place <prefix;room>` loses the room's first letter

`ShowRoomInfoV2` splits a place tag with a `Right()` that counts back from the
end and assumes the author wrote `"; "` with a space (`V4Game.Part2.cs:1986-1990`):

```csharp
                    placeNoFormat = Strings.Right(place, Strings.Len(place) - (Strings.InStr(place, ";") + 1));
```

`Len - (pos + 1)` is one character short of the text after the semicolon, so with
no space the first letter of the destination is cut off.  The ASL ≥ 2.80 loader
does the same job with `Mid(placeData, scp + 1)` and is correct (`:1216-1231`),
which is why this shows up in exactly one game.  Magic Sword Part 1 (ASL 217)
writes `place <Your Training room;Training Room 1>`:

```
-You can go to Your Training room Training Room 1, The Library, or The Grand Luridii's Study.
+You can go to Your Training room raining Room 1, The Library, or The Grand Luridii's Study.
```

6 lines, one game.  The expression is VB-idiomatic enough to be Axe's original
rather than a translation slip, so this is probably real Quest behaviour — but
it is a bug, and reproducing it would only make geas print a mangled room name,
so it is recorded rather than filed.

## Harness artefacts, not engine bugs

* **`wait <message>` text was missing from the geas side.  Fixed.**  `st_wait`
  hands the message to `wait_keypress`, and `geasglk.cc:1135-1141` prints it —
  but the regression runner used to override `wait_keypress` with a no-op, so
  the text never reached the transcript.  525 lines across 44 goldens, and 30
  games' *first* divergence: the second biggest thing in the diffs, and none of
  it real.  The runner now prints the message the way the Glk frontend does
  (`geas_walkthrough_runner.cc:139-148`) and the 111 goldens were re-blessed:
  594 lines added across 49 of them, nothing removed or changed, and five more
  games came out identical to Quest (6 → 11).  Two things it flushed out that
  the bucket had been swallowing: Magic Sword's joined-up `|w` line, which is
  finding 64 and not a missing prompt, and nine King's Quest V lines that are
  finding 57's cut-scene shift with their twins one hunk away.  A bare `wait`
  is a *different* thing and still diverges — see finding 2; no corpus
  walkthrough reaches one.
* **A menu option with a trailing space was echoed twice.**  `SetUpChoiceForm`
  prints `- <chosen option>` once the menu comes back
  (`V4Game.Part2.cs:2660-2663`); the harness prints `[choice] N` in geas's shape
  instead and swallows that line by comparing it with the option text it
  resolved.  The comparison trimmed the printed line but not the expectation,
  so `choice <Buy a Cup of Coffee >` — Amazing Maze, trailing space and all —
  slipped through as a stray `- Buy a Cup of Coffee`.  Fixed in
  `Program.cs:ResolveMenu`'s caller by trimming both sides.
* **The blank-line and whitespace differences** are normalised away on both
  sides (`compare.py:normalise`); Quest's output is HTML and geas's is a Glk
  text buffer.
* **`|xn`** — a printed string ending in `|xn` continues the same paragraph
  (`TextFormatter.cs:29-33`, 196).  geas honours it; the oracle harness did
  not, and invented a line break after every such string.  Fixed in
  `Program.cs:Emit`, which reads the `nobr` flag off the wrapper element.  The
  Devil's Bargain went from 44 diff lines to 9.
* **A decimal-comma locale silently truncated every fraction.**  `AppleLocale`
  on this machine is `en_SE`, and `SetNumericVariableContents`
  (`V4Game.Part2.cs:521`) stores numbers with a culture-sensitive
  `double.ToString()` while `Conversion.Val` reads them back invariantly — so
  `3.5` was written `"3,5"` and read back as `3`.  That produced a convincing
  "geas arithmetic bug" that did not exist.  `Program.cs` now pins
  `CultureInfo.InvariantCulture`; VB6 was invariant in both directions, so this
  restores Quest's behaviour rather than papering over it.
* **`compare.py` could never report a game as identical.**  `l[:1] in "+-"` is
  true for the empty string, so the blank tail of `split("\n")` counted as one
  changed line.  Six games were reported as `DIFF 1 lines` with an empty diff
  file.  Fixed; they are the six identical games in the summary above.
* **A menu splits the turn on the QuestViva side, so a timer armed inside a
  `selection` starts a turn late.**  Both harnesses approximate Quest's
  real-time timers with one tick per player command (`--tick`), and both engines
  skip the tick of the turn a timer is switched on (`BypassThisTurn`,
  `V4Game.Part2.cs:561-574`; `TimerRecord::bypass`, `geas-runner.cc:6164`).  But
  `SendCommand` ticks as soon as the turn *suspends* (`:78-87`), and a
  `selection` suspends it, so the choice's script — including its `timeron` —
  runs after that tick, in the menu-response iteration, which does not tick at
  all.  geas runs the whole selection inside the one turn and ticks after it.
  Net effect: geas's bypass is spent one turn earlier.  ZombiesAttack arms its
  `Zombie gettin up` timer (interval 12) from the `open door` selection on turn
  5; geas fires it at the end of turn 17, QuestViva at the end of turn 18.  That
  one turn is the whole of the game's 100-line divergence: geas kills the zombie
  before the follow-on timers can hurt the player, keeps `#health#` at
  `Perfect`, wins the fat-zombie fight on the first roll and finishes the
  script, while QuestViva takes the damage, loses the fight, re-prompts, and
  eats the rest of the command file as menu answers.  Real Quest ticks its
  timers off a one-second wall clock that has nothing to do with turns, so
  neither ordering is "right"; it is an artefact of the approximation.
  The Lazst Resort diverges the same way and for the same reason: ordering a
  drink from Jack is a ten-option `selection` whose script arms `drinktimer`
  (interval 30), so geas is served one turn earlier, drinks it, and has the
  empty glass the rest of the walkthrough needs for the ice machine, the hand
  dryer and the cork-and-needle compass; QuestViva is still holding the
  cocktail and fails all three.
* **The Detective's replay begins 33 turns too early.**  Both runners find the
  end of a command script's prose header by taking the first line that *begins*
  with `start:` (`geas_walkthrough_runner.cc:414-421`, `Program.cs:243`), which
  is deliberate — ten scripts write the marker as `Start: Entry Hall.` and name
  the opening room on the same line.  The Detective's header contains the
  sentence "…nothing checks whether you have earned them.  The script below
  earns them in order anyway.", wrapped so that `start:` lands at the head of
  line 50, and the real `Start:` is at line 94.  Everything between is typed at
  the game as commands.  Both engines are fed it, so the comparison is still
  sound — and it is how the committed geas golden was blessed, so the suite
  agrees with itself — but 33 of The Detective's turns are prose, and finding 44
  is only visible because five of them start with "the".  A fix is to require
  the marker to be the whole line or to be followed by a capital letter; either
  way the golden needs re-blessing.
* **One game was silently skipped.**  The play-line parser read the label with
  `(\S+)`, and The Lazst Resort's label is quoted and contains a space, so it
  was read as `"The` and reported `missing files`.  Fixed; it now compares
  (and is one of the 22 exit-order games).

## QuestViva's own defects

Where these appear, geas is right and the oracle is wrong.

* **DromBennacht** — `NullReferenceException` during load.
* **A Certain Oscar** — fails to initialise.
* `[An internal error occurred]` — 16 lines across 8 games.  Run any of them
  with `QVH_LOG=1` and the log names the offending script line; all sixteen
  come from three causes, and two of the three are translation defects.
  (`$str…$` rendering as `[ERROR]` was filed here too and is not a defect: an
  unknown function really does yield `[ERROR]` in Quest — see finding 51 — and
  geas's empty string is the divergence.)

**`_objs` is a class, so index 0 is `null` where VB6 had a zeroed record.**
This is the big one — 12 of the 16 lines.  `ObjectType` was a VB6 `Type`, so
`_objs(0)` was a real, blank element and reading it was harmless; QuestViva
declares it `internal class ObjectType` (`V4Game.Types.cs:214`) and leaves
`_objs[0]` unassigned, so the same read throws.  Two routines do exactly that:

```csharp
        else
        {
            await AddToObjectProperties("not parent", childId, ctx);
            await UpdateVisibilityInContainers(ctx, _objs[parentId].ObjectName);
        }
```

(`ExecAddRemoveScript`, `V4Game.cs:2694-2698`.)  That arm is the one taken when
`remove <obj>` has *no* `;parent` — so `parentId` is still its `default` of 0.
Quest passes the empty name through and completes the removal; QuestViva dies
before it.  Eleven lines, six games: Pyramid of Terror (five `remove`s of
sweets), Brice Ball, Freshman Fantastic, Michael's Game, Shipwrecked, Things.

The twelfth is `GetStringContents`' property shortcut, which does not check
that the object exists:

```csharp
            return GetObjectProperty(propName, GetObjectIdNoAlias(objName));
```

(`V4Game.Part2.cs:2582`; `GetObjectIdNoAlias` returns 0 for "no such object",
`V4Game.cs:6214`, and `GetObjectProperty` opens with `var o = _objs[id];`.)
Freshman Fantastic's Tom says `#name:#` — an object called `name`, no property
— and the whole speech is lost:

```
 > speak to Tom
-"Hey there, that Dimplebottom's something, huh?"
-"Say, do me a favor? This homework is kicking my ass and I left my calculator in classroom 101. …"
+[An internal error occurred]
```

**`clone` copies a reference, so it renames the original.**  Also a value-type
translation slip, and a nastier one because it corrupts the world model rather
than just throwing:

```csharp
        _objs[_numberObjs] = new ObjectType();
        _objs[_numberObjs] = _objs[id];
        _objs[_numberObjs].ContainerRoom = cloneTo;
        _objs[_numberObjs].ObjectName = newName;
```

(`V4Game.cs:4381-4384`; `_rooms` is copied the same way at `:4392-4394`.)  The
freshly built `ObjectType` is thrown away on the next line and both slots end up
pointing at the *same* object, so the two assignments rename and move the
original.  Barbarian's greengrocer sells one vegetable per `clone <X; X%c%;
Market>`, and tracing `id` through the walkthrough shows the damage:

```
### CLONE name={Lettuce} id=294 n=436
### CLONE name={Tomato}  id=296 n=437
### CLONE name={Tomato}  id=0   n=438      <- "Tomato" no longer exists
```

The second sale renames the only Tomato to `Tomato2`; the third cannot find one,
falls into `_objs[0]`, and takes the following `show` and `give` down with it —
three lines, and the player never gets the fruit.

**`pause <>` is a type mismatch in VB6 too.**  Sir Loin's one line is
`Conversions.ToInteger("")` throwing out of `V4Game.Part2.cs:5998`; `CInt("")`
is error 13 in VB6 and lands in the same handler, so real Quest prints
`[An internal error occurred]` here as well.  That one is geas's divergence, not
QuestViva's — geas silently accepts the empty parameter, the way it does for
`outputoff <>` (finding 40).

Two things previously filed here are not QuestViva's fault after all.  The
unconsumed `|s9|` (Uranus) and `|"` (The Lazy Gun Cult) codes are Quest
behaving correctly — see finding 33 — and the two `<ERROR>` markers are geas's
own documented compromise, below.
* Three defects were fixed rather than worked around, in
  `../../quest5/harness/oracle/patch_questviva.py`: the CAS loader's character
  set (section 9), and `Begin()` not honouring the turn-suspension handshake
  (section 10), which used to deadlock eight games.  There are now no timeouts
  in the corpus.

## Verified identical

Things that could plausibly have diverged and do not:

* **The RNG.**  Under `GEAS_SEED=1` / `QVH_SEED=1` both engines draw from the
  same xoshiro128\*\* stream, and the draw counts and their order match
  turn for turn (`r4.asl`, `r5.asl`, `r6.asl`, `rngprobe.asl`, `turnprobe.asl`;
  `QVH_TRACE_RAND=1` prints the oracle's stream).  A random event is therefore
  never an excuse for a diff.
* `$rand()` used directly in an `if` comparison.
* Turn-script ordering.
* The `Start:` header in the command scripts is read the same way by both
  harnesses.
* The leading space VB's `Str()` puts in front of a positive number (`" 4"`) is
  display-only and is normalised away.

## Loose ends

* The Quest 5 oracle harness
  (`../../quest5/harness/oracle/Program.cs`) has **not** been given the
  `CultureInfo.InvariantCulture` pin.  Its goldens were generated under
  `en_SE`, so adding it would move them; it needs checking on its own terms.
* The unclassified pile is **empty**: all 20 348 lines are accounted for.  It
  started at 17443 lines across 68 games, and mining it is where findings 19-73
  came from, so the pile having earned its keep is the reason to distrust an
  empty one — a rule that swallows a line is only as good as the reading behind
  it.  (The last five went with the fixes for 16-33: Freshman Fantastic's `-!`
  was finding 28's sentinel, and the four stray exits- or room-line halves —
  Broken Mirror's, Space's and two of Skate Ur Ass Off's — paired up again once
  the exit lines themselves stopped diverging.)  Each bucket's
  comment in `triage.py` says what that reading was; the ones to re-examine
  first if a golden moves are the two generic ones, "command echo realigned by
  a shifted cut-scene" and the `desync:` anchors, which credit whole runs to a
  single cause.  The `wait` fix is one worked example: re-blessing the goldens
  emptied one bucket and refilled the pile with nine lines that belonged to
  another.
* Findings 1 and 66-67 are the same lesson from the other end — what a *fix*
  does to a bucket.  Finding 1's was two blanket regexes, `^You are in .*\.$`
  on the qv4 side and `^You are in .*[^.]$` on the geas side.  While the stop
  was missing they were exact: every pair of room lines differed by it and
  nothing else.  Once geas printed it they kept matching anyway, and what they
  now matched was every *other* reason two room lines can differ.  Re-blessing
  took the bucket from 4665 lines to 11, and those 11 turned out to be three
  separate bugs — findings 66 and 67, and two more lines of King's Quest V's
  type library — still wearing the name of a fixed one.  The rules are pinned
  to a twin now, and a bucket that outlives its finding is the thing to read
  first.  66 and 67 are themselves fixed; their two buckets emptied on the next
  re-bless and are gone from `triage.py`, and RiddleRun's transcript is now
  identical to Quest's.
