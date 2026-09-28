# Incomplete and incompletable ADRIFT 4 games

Every game in the ADRIFT 4 corpus (`test/adrift4/`) that cannot be won, cannot
reach its declared maximum, or has no ending at all.  Merged 2026-09-28 from
`UNWINNABLE.md` and `UNREACHABLE.md` (last refreshed 2026-09-27); where the two
disagreed, the newer measurement and the per-game walkthrough note win.
Completed the same day against every golden's closing tally: each row that ends
below its declared maximum is listed below with the verdict from its manifest
comment in `harness/run_v4_walkthroughs.sh`.  Unscored games (no tally in the
golden) are covered only by section 6.

Out of scope: the ~85 `.taf` files with no walkthrough row because they were
declined on content grounds (14 of them permanently) — see *Content policy* in
`WALKTHROUGH_TODO.md`.

Scores are what the committed walkthrough rows score in the seeded headless
harness (suite: 672/672 PASS), cross-checked against each row's manifest
comment, which is the authority when a walkthrough note disagrees.  "Faithful"
means every assist off and no patches.  The per-game note
(`<Game>_walkthrough.md`) carries the current verdict; a remembered summary
doesn't.

## The two repairs

They are not interchangeable.

- **GLK-layer assists** — `glk combatassist / moveassist / repeatassist /
  roomassist / capacity on` (harness: `SCR_ASSUME_COMBAT`, `_MOVES`,
  `_REPEATS`, `_ROOMS`, `_CAPACITY`).  They change ENGINE BEHAVIOUR for one
  session and are deliberate divergences from the Runner.  In the glk build
  eleven known games get the ones they need turned on automatically
  (`GSC_GAME_ASSIST_TABLE` in `os_glk.cpp`, matched on GameName+GameAuthor),
  with a one-line startup notice; `glk <assist> off` restores faithful
  behaviour.  The headless harness does NOT apply that table.
- **Patch table** — `PATCH_TABLE` in `sctafpar.cpp`, 36 games, 230 edits.  It
  changes GAME DATA, content-verified edit by edit, and repairs the author's
  slip itself.  ON BY DEFAULT in the glk build (`glk patches off`, then reload,
  to play a game exactly as its author left it), with a one-line notice; opt-in
  in the harness with `SCR_ASSUME_PATCHES=1`.  Each patched game keeps its
  faithful row and gains a patched one.

So a Spatterlight player sees the "best" column below, not the faithful one.

What the patch table will not do: place objects the author never placed,
rewrite battle statistics, schedule encounters the author never scheduled, or
"fix" a game that is merely faithful to its own engine version.  Where the
accident is one of the five the engine has an assist for, the assist is the
right repair.

## Summary

| Game | File | Faithful | Best (Spatterlight) | Status |
|---|---|---|---|---|
| Illegal Socks | illegalsocks.taf | 745/2155, no win | same | **Unwinnable** — patched, still lost |
| Inverness Castle | inverness.taf | 75/205, no ending | same (repeat assist) | **Unwinnable** — no ending in the file |
| Les Feux de l'enfer | Les Feux de l'enfer.taf | 75/115, no win | same | **Unwinnable** — demo, only deaths |
| Quest I | QuestI.taf | 10/10, no win | same | **Unwinnable** — only deaths, no WinText |
| Villains & Kings | Villains_And_Kings.taf | 30/37, no ending | 31/37 patched | **Unwinnable** — no winning ending |
| The Long Journey Home | Journ2.taf | 5/90, no ending | 30/90 (repeat assist + patch) | **Unwinnable** — no ending reachable |
| Topaz (two post-comp releases) | — | — | — | **Unwinnable** (the comp release in the corpus wins) |
| To Hell & Beyond | To_Hell_And_Beyond.taf | 0/373 | 248/373 WIN (265 max row) | Won by assist (combat + move) |
| Welcome to Wonderland | wonderland.taf | no win | 215 WIN | Won by assist (capacity + combat) |
| The tunnels of Athylon | — | no win | 25/25 WIN | Won by assist (combat) |
| Space Run | — | no win | 290/290 WIN | Won by assist (room) |
| Enigma Creature | — | endless dodges | fights resolve | Assist (combat) |
| The X-Files: A New Beginning | — | Dean never appears | appears | Assist (move); 299/299 faithful |
| HYPER Battle System | — | 100/100 | 100/100 | Assist (move), cosmetic |
| The Spirit's Flight | The_Spirits_Flight.taf | 50/95 | 95/95 WIN | Won by patch |
| Del Sol MADNESS | Del Sol.taf | 26/46 | 45/46 WIN | Won by patch; 46 unreachable by design |
| The Hangover | hangover.taf | 5/7 | 7/7 WIN | Room assist 6/7, patch 7/7 |
| Crazy Old Bag Lady (COBL) | COBL.taf | 160/230 | 230/230 WIN | Won by patch |
| Mesto prestupleniya 2 / Crime Scene 2 | CS2.taf | 23/70, no win | 70/70 WIN | Won by patch |
| The Quest For More Hair | liqid.taf | 60/100, soft-lock | 95/100 WIN | Won by patch |
| Sandy's Lost Doll | Sandy.taf | 0/0, no win | win | Won by patch |
| Tenebrae Semper | — | stranded | THE END [4 of 5] | Won by patch |
| Blood Relatives | Blood_Relatives.taf | 11/13, no win | 13/13, vault still shut | Points patched; win **still blocked** |
| Ebony's World | ebonysworld.taf | no win | 1450 WIN | Won by patch |
| Mystery House | MysteryHouse.taf | no win | win | Won by patch |
| Bedlam | bedlam.taf | no win | win | Won by patch |
| Filthy Bill (AIF) | filthybill.taf | 5 of 6, no win | 1000/1000 WIN | Won by patch |
| Fun Town (AIF) | fun town.taf | 200/200, no win | 200/200 WIN | Won by patch |
| The Annihilation of Think.com 3 | TAOT3.taf | 0/1 | 1/1 WIN | Won by patch |
| The Vampire With A Conscience | Vampire.taf | 70/100 | 100/100 | Won by patch |
| The Merry Murders | Merry_Murders.taf | 120/135 | 135/135 | Won by patch |
| Jason Vs. Salm | — | 0/1000 (wins) | same | Short — not patched |
| Greek School Adventure | — | 185/275 (wins) | same | Short — not patched |
| Marooned | — | 80/140 | same | Short — not patched |
| The Search For Mr Smith | — | 90/100 (wins) | same | Short — not patched |
| TheADRIFTProject | — | 90/100 | same | Short — not patched |
| Mangiasaur | — | 63/74 | same | Short — not patched |
| Aquarius Part 1 | — | 85/95 | same | Short — not patched |
| British Fox and the Celebrity Abductions | — | 46/50 | same | Short — not patched |
| The Night The Moon Shone Grey | thenightmoon | 360/400 (wins) | 380/400 | Short — partly patched |
| Space Boy Volume I | — | 1009/1374 | 1039/1374 | Short — partly patched |
| Melbourne Beach | — | 38/41 | 39/41 | Short — partly patched |
| Sommeril | — | 85/100 | 95/100 | Short — patched to its pool |
| The Fugitive | — | 656/666 | 646/666 | Short — patch trades 20 for 10 |
| Studio (AIF) | — | 96/100 | 100/100 | Short points fixed by patch |
| The Twilight | — | 485/500 | 500/500 | Short points fixed by patch |
| House Of Horror | — | 145/155 | 155/155 | Short points fixed by patch |
| Sun Empire: Quest for the Founders (Part I) | — | 140/145 | 145/145 | Short points fixed by patch |
| Terrified | — | 60/65 | 65/65 | Short points fixed by patch |
| Sentor | — | 12/13 | 13/13 | Short points fixed by patch |
| Professor Von Witt | — | 154/229 | 229/229 | Short points fixed by patch |
| FunHouse | — | 310/410 | 410/410 | Short points fixed by patch |
| Locked Out | — | 90/110 | 110/110 | Short points fixed by patch |
| The Crime Scene | — | 78/80 | 80/80 | Short points fixed by patch |
| Goldilocks - Breaking & Entering | — | 32/35 | 35/35 | Short points fixed by patch |
| ALEXIS, Cowboy Blues, A Day In The Life Of A Super Hero, The Alchemist, Full Circle, Provenance | — | — | — | Short by route choice (section 4) |
| The Warlord, The Princess & The Bulldog | warlord.taf | 99/100 | same | Short — Runner-faithful take answer |
| The Prostitute, Loving Family, fantasyworld (AIF) | — | — | — | Short by content policy (section 4) |
| Insidejob, JimPond, Grumble, Great Escape, ARGH's Great Escape, Cursed, Crime Adventure, VGM1_3 and 21 more | — | — | — | Phantom maximum (section 5) |
| ~33 sandboxes, intros and demos | — | — | — | No ending by design (see below) |

---

## 1. Unwinnable, even with every repair

**Illegal Socks** (illegalsocks.taf) — faithful 745/2155.  Both endings need
the boss "Dr. Myanus Hurts" dead, and his authored Name holds a literal ". ":
4.0's own input splitter cuts "attack dr. myanus hurts" in two before any
handler sees it, "attack dr.myanus hurts" fails the literal whole-Name test,
and every alias phrasing fails the same re-check.  Player Accuracy is 0 too,
so the Battle Dome is a stalemate.  An authoring bug, not a Scarier one.
*Patched 2026-09-26* (`PATCH_ILLEGAL_SOCKS`): the period is dropped from both
copies of the Name, so the Doctor is a legal target at last.  Still not
winnable — his 40 stamina / 35 strength / 20 defence against the player's
10/8/8 decide it with or without the combat assist, because the problem was
never the hit roll.  Needs rebalancing, which a field patch cannot do.

**Inverness Castle** (inverness.taf) — 75/205.  No ending exists in the file
at all: not one EndGame action across its 57 tasks.  The box poses one riddle
of fourteen, so 130 of the 205 are for riddles you are never asked.  The repeat
assist is on automatically (a spent catch-all otherwise answers every later
command, even `quit`, with "You have already done that."), but there is still
nothing to win.

**Les Feux de l'enfer** — 75/115.  Five EndGame actions, all v1=2 (death);
zero v1=0.  Killing Anarazel just teleports you back to the entrance.  A demo
build: the clef bleue is never created, so rooms 19-30 are sealed.

**Quest I** (QuestI.taf) — 10/10.  Full score is reachable, but the only two
EndGame actions are both var1=2 (death) and there is no WinText.

**Villains & Kings** (Villains_And_Kings.taf) — faithful 30/37, patched 31/37.
No task anywhere carries a winning ending.  Down one point since 2026-09-14:
`close window` now goes to the library close, as in run390.  *Patched
2026-09-26*: T5 `take * soap * * *` opened from Where=NO_ROOMS to all rooms, so
fishing the soap on a rope out of the broken window scores the point it was
written for.  31/37 is the ceiling on paper.  Its assisted row was retired: a
3.9-signature game whose combat the engine now repairs unconditionally with the
legacy hit model.

**The Long Journey Home** (Journ2.taf) — faithful 5/90, no ending.  Three
independent walls (30/90 is the ceiling on paper), and the real Runner bricks
in the Lair first: a spent task with a bare `*` pattern claims every later
command there.  Ported 2026-09-13, so Scarier bricks at 5/90 too.  The repeat
assist (automatic in glk) gets it back to 30/90.  The 90 is two careers,
chosen by `male` or `female` on move one, and every scoring act exists twice.
*Patched 2026-09-27* (`PATCH_JOURN2`), new FEMALE row 20/90 → 30/90: Rage's
valve female twin T24 is Where=NO_ROOMS and its only command is the author's
own label, so nothing could run or type it, while T22 — the unrestricted debris
refusal three slots above — repeats all four valve phrasings and claimed the
line for BOTH genders.  The patch gives T24 room 9 and the four phrasings and
hangs T22's missing restriction on it ("T18 remove debris must NOT be done",
empty FailMessage, mirroring the author's own T18/T19 and T20/T21 pairs).  The
row still needs the repeat assist: the spent T3 catch-all in the Lair is not
data-fixable.  30 is the female ceiling — Rage's exit gate names the MALE twin,
and an exit can name only one task.  Not derived: the same T22 repair unblocks
the male T25, worth 40 with the walk out intact.  The card game has no starter,
and the ending is sealed regardless.

**Topaz** — three releases; the comp release in the corpus is winnable, the two
post-comp releases are not.

## 2. Unwinnable as authored, won with a GLK-layer assist

Automatic in the glk build.  Every row measured with and without on
2026-09-26/27.

**To Hell & Beyond** (To_Hell_And_Beyond.taf) — faithful 0/373 (the player
never even leaves the mansion; ~23/373 on paper).  Every character's Accuracy
and Agility are 0, so accuracy > agility never passes — no blow ever lands, and
both endings need Xozim dead.  Mid-game progression moves have an unset "To:"
(Var2=-1); T72 never runs (Theeve KilledTask -1); an NPC walk moves the player
away from T83.  Only with BOTH combat + move assists does it reach "You are now
ruler of Beyond": 248/373 honest, 265/373 via an exploit on the max row.  True
ceiling 293 (T86/T87 are exclusive).  run400-confirmed.  Not patched — a
whole-game stat repair.

**Welcome to Wonderland** (wonderland.taf).  The ethereal knife is
phantom-weighed by the NPC-held rod/Staff chain (94 > MaxWt 90) and can never
be taken.  Unwinnable in run400 as well, so the wall is the Runner's.  Capacity
+ combat assist get the knife and land the blows: 215, a win; the harness row
runs the same pair.

**The tunnels of Athylon** — combat assist: no win → WIN, 25/25.

**The Town of Azra** — combat assist (4.00 release).  Lets the fights land;
the game still has no ending (section 6), and the 3.90 release's combat works
as authored with no assist.

**Space Run** — room assist (a task left set to run in no room): no win → WIN,
290/290.  (Combat assist measured, no difference.)

**Enigma Creature** — combat assist: fights were an endless exchange of dodges.

**The X-Files: A New Beginning** — move assist: Dean and his diner
conversation appear.  (The score itself is 299/299 faithful; see Resolved.)

**HYPER Battle System** — move assist, cosmetic only (100/100 either way).

The Hangover's room assist is covered with its patch below.  Vampire and Merry
Murders were dropped from the assist table on 2026-09-27: each is one bad
field, so the patch table repairs them instead.  Measured and deliberately NOT
in the table: g7056, Ghoster, Noximion (and combat in Space Run), where the
assist made no difference, and Goldilocks, whose route the capacity switch
breaks.

## 3. Unwinnable as authored, won with the patch table

**The Spirit's Flight** (The_Spirits_Flight.taf) — 50/95 → 95/95.  The Ice
Totem is never un-hidden by any task/event (seals invoke elementals), and the
file's sole ACT type=6 is where=0 with nothing able to dispatch it.  The patch
gives Crynasalda's task the drop action her sister guardians carry, gives the
chant task room 0, the Stone Circle its own restrictions name, and a second
command — the four-line verse can never reach a task, since every Runner from
3.9 up cuts input at "," and ". " before matching, so "say the chant", the
words the room's own message asks for, is added — and cuts Carnifern's Defense
of 250 (siblings: 20 and 27) to 25, which the golden axe can get through.

**Del Sol MADNESS** (Del Sol.taf) — 26/46 → 45/46.  Its one EndGame (T26) is
carried as a KilledTask by an NPC who never leaves room 3, and the nightmare
MoReLaND you do meet has no KilledTask at all, so shooting her down dispatches
nothing.  The patch clears kissing_jarvin's starting 1, has T4 exec the
orphaned T27 so Hina comes along, and gives the nightmare MoReLaND the
KilledTask T26 was written for.  46 is unreachable by design: `make out with
jarvin` (+1) sets kissing_jarvin and nothing clears it before Physics, so it
excludes T14's `no` (+10) — MaxScore counts both branches.

**The Hangover** (hangover.taf) — 5/7 → 6/7 (room assist) → 7/7 (patch).
Both endgame tasks are Where/Type = 0 (ROOMLIST_NO_ROOMS); run390 answers
"You can't do that here!" to both and tops out at the same 5/7.  The room
assist runs both but reaches only 6/7: T10 never gives you the second approval
form.  The patch gives both tasks their own rooms (T10 the Cafeteria, T14 the
Form Process Office), adds the missing open action to `open the filing
cabinet`, and makes T10 hand over the form its own text promises.

**Crazy Old Bag Lady** (COBL.taf) — 160/230 → 230/230.  The win (T49) needs
"People helping"==4, but the counter the hobos' newspapers sit on is a STATIC
with an empty room list.  The Corner Shop's description has always put it
there, so the patch gives it the Corner Shop; the whistle then gathers all
four helpers at the riverbank.

**Mesto prestupleniya 2 / Crime Scene 2** (CS2.taf) — 23/70, no win → 70/70.
The SYNONYM table rewrites взять/открыть/разбить and the compass words before
task matching, and the game ships no ALTCMDs, so every task whose command
starts with one of them is dead: the five evidence pickups, and T71 `юг`, the
only move into room 25, where the win `написать отчет` has to be typed.
run390-measured under Wine, word for word.

**The Quest For More Hair** (liqid.taf) — 60/100, soft-lock → 95/100 WIN.  The
old harness marker ("I surrender, you win!") is Jenkins' surrender, not an
ending.  `rope cliff` hides the rope, but the task and its reverse both need
the rope held, so it can never be undone, and the Airport's S/W exits need it
undone.  Hamish's KilledTask is 0 and nothing runs ^^smugglerdiesevent^^, so
the hair shop, `buy hair` and the talk-king win are out too.  The patch makes
T36 non-Repeatable so its ReverseCommand can untie the rope, and gives Hamish
the KilledTask (T43) that opens the hair shop.  The patched route buys the Gun
and two lots of armour rather than the blaster (850 denmarkarians don't
stretch further) and heals at the kitchen cupboard between bouts.  The missing
5 is `buy hair`'s second +5 action, dropped by ADRIFT's score-a-task-once
rule, which MaxScore counts anyway.  (The ferry man was once miscategorised as
unwinnable and is beatable.)

**Sandy's Lost Doll** (Sandy.taf) — 0/0.  The win task's RESTR type=4 Var1=0
tests the command's referenced number, which `look in toilet` never supplies.
run400-confirmed.  The patch points it at variable 0 ("mom"), the one its two
predecessor tasks test; the third look in the toilet ends the game.

**Tenebrae Semper**.  The night rooms (6/7) are entered only by tasks that
themselves live in rooms 6/7 (run400-confirmed).  Two tasks narrate a walk and
move nobody — T16, the stairwell door, and T35, the walk back across campus —
both one-shot, stranding you in the Science Center Hallway.  The patch gives
each the move-player action its text describes (rooms 6 and 5); the patched
row runs to THE END, [Ending 4 of 5].

**Blood Relatives** (Blood_Relatives.taf) — 11/13 → 13/13, **win still
blocked**.  The win (T345 drop treasure) sits behind the vault door T344,
whose RESTR type=4 Var1=0 tests the referenced number — Sandy's bug again; no
%number% pattern exists.  The bronze key's T385 `exam desk` is dead too (the
game's own synonym rewrites exam → x).  The patch rewrites T385 to `x desk`;
the vault ending is deliberately left alone — nothing in the data says which
variable T344 meant.

**Ebony's World** (ebonysworld.taf) — 1450.  The win (T26 `flip switch`) needs
dial1/dial2/lever/valve = 3/2/2/2, but no ACT writes variable index 3 (valve):
both lever and valve tasks write index 2.  The patch repoints both valve tasks
(restriction and action) to index 3; Bardo gets his thank-you at the same 1450.

**Mystery House** (MysteryHouse.taf).  The only win (T0, `drop treasure Chest`
with the chest open) tests an Openable state nothing flips: T3 "open chest"
carries no ACT.  The patch gives T3 its change-status action.

**Bedlam** (bedlam.taf).  A quarter-finished preview with one real win.  T37
("ask barbara about keys") narrates handing over the car keys but has zero ACT
lines, so object 30 stays Hidden and T38 `start car` can never pass.  The
patch gives T37 its move-object action (`start car` twice — T31's no-keys
message matches first and is not repeatable).

**Filthy Bill** (filthybill.taf, AIF) — 5 of 6 → 1000/1000.  The sixth
conquest needs the french tickler worn, sealed inside the passed-out Bum's
coat; the engine refuses taking a held/worn object off a living NPC, and no
task relocates the coat.  The patch gives `give whiskey to bum` a second action
that drops the coat in the street.

**Fun Town** (fun town.taf, AIF) — 200/200, no win → WIN.  The one WIN action
("open treasure chest") sits behind a 20-entry gate chain including a
death-only task.  T117's gate named task 105 (the no-condom death) instead of
its twin 106 (+10, "You would have died otherwise"); the patch moves the
restriction to the scoring twin.

**The Annihilation of Think.com 3** (TAOT3.taf) — 0/1 → 1/1 WIN.  The single
point and the only winning EndGame are in T22, where=1 room=18, which nothing
leads into.  T20 (choice B, "he turns and runs to your left") is the one link
in the chase chain that forgets to move the player; the patch gives it the move
to room 18.  Its prequel wins, 35/35.

**The Vampire With A Conscience** (Vampire.taf) — 70/100 → 100/100.  T61 (east
out of the Bozo backyard, the only exit) is spent by the first exit and claims
the second under the pre-4.0 spent-task rule (run390-measured, ported
2026-09-13).  The patch ticks T61 Repeatable: its CompleteText says "You enter
the nightclub again." and its RepeatText is a single space.  With `glk patches
off`, `glk repeatassist on` is still the way past.

**The Merry Murders** (Merry_Murders.taf) — 120/135 → 135/135.  The second
archives `n` is claimed by spent T46 ("I have already done that.";
run390-measured, ported).  The patch renames T46's redundant first command
slot from `n` to `unlock the archive door`.

## 4. Winnable, but points are blocked

### Not patched

**Jason Vs. Salm** — 0/1000, WIN reached.  The four scored victory options are
honour-gated by difficulty; tasks 10-12 give their bonuses to Jason instead of
the player, so the top two are arithmetically unreachable and the third is a
lottery.  Combat assist doesn't apply (it only fires when NO accuracy/agility
is configured, and Salm has 34/50).  A battle-stat rewrite, not a data slip.

**Greek School Adventure** — 185/275, the true ceiling (runner_transcripts/
greekschool.txt).  Wins, but only through a task-repeat exploit.  Four pools
totalling exactly 90 points are structurally dead — what the game's own "90
points short" message reports: an NPC keeps the key; the ladle is never placed;
holiness caps at 90 while the +50 ending needs 100; one pool needs dead
"crusader" content.  Rooms+repeats assists: no change.  Mostly missing content.

**Marooned** — 80/140, the ceiling (re-measured 2026-09-27).  T24 is not a
lost point: T14 already pays the same +10 for the same act, so T24's 10 are
phantom maximum.  The real blocks are T35 (needs a flare gun that loading
destroys), T27 and the T9/T28 pair.  T35 also requires T34, the dented can's
copy of the shark throw (T33 is the scratched can's), which has no room set —
an authoring slip, but no patch: throwing the dented can forfeits the Rescue
event, and T35 is blocked by the flare gun anyway.  Route physics are the measured real 3.80
pooled-burden model.  Capacity assist: no change.

**The Search For Mr Smith** — 90/100, a win.  Task 22 (`###bear dead`, +10)
is only reachable as the bear's KilledTask, and the fight is one hit short: 70
stamina against your best arithmetic.  No assist touches HP arithmetic;
candidate only if a stat edit is acceptable.

**TheADRIFTProject** — 90/100 ("You finished 10 points short").  T69 `#Put
Transmitter on Darwin` has Where=NO_ROOMS, but the transmitter and receiver
are hidden objects nothing ever places, and the author's own #NOTES task lists
"Make some sort of transmitter" as a to-do.  Room assist measured: T69 runs
everywhere, score stays 90.  Deliberately not patched.

**Mangiasaur** — 63/74, the ceiling (run400x agrees turn for turn).  T76
belongs to a monster the file never places; T123 is a duplicate score action
ADRIFT 4.00 refuses to pay twice.

**Aquarius Part 1** — 85/95, the ceiling.  The EndGame into Part 2 closes the
game first.  Part 2 reaches 200/200.

**British Fox and the Celebrity Abductions** — 46/50.  The Controller's Office
ends the game before the rest can be banked (21/50 when first wired, 43 after
rerouting, 46 on 2026-09-27 via `ring bell` banking T333 and playing the Welsh
Fox opening out to bank T52; later damage rolls are RNG-stream sensitive, safe
draw offsets 0,+1..+4,+7,+8).  The last 4 are branch facts: T460 and T189 are
excluded once she is captured, Grace's arrest is the other, exclusive branch,
and T564 would need the whole RNG-sensitive gauntlet re-derived.

### Partly patched

**The Night The Moon Shone Grey** — 360/400 → 380/400.  The two dead +20
tasks: the wolf never appears (nothing runs task 4), and "behead drow" wants
the dark elf's body both held and in the Library (run390x agrees); the patch
recovers one of them.

**Space Boy Volume I** — 1009/1374 → 1039/1374.

**Melbourne Beach** — 38/41 → 39/41.  39 is the ceiling; the other 2 are
genuine design contradictions.

**Sommeril** — 85/100 → 95/100, the whole pool its sixteen scoring tasks sum
to.  T6 `take wet page` (+10) was restricted on the page being held by the
FISH, which never holds it (the creating task drops it inside the FOUNTAIN);
the patch asks the fountain instead.

**The Fugitive** — 656/666 → 646/666.  The patched row takes the car to prove
the mirror's +10; the taxi route it gives up is worth 20, so the patch makes
the car the author's lesser choice rather than a silent one.

### Fully repaired by the patch table

**Studio (AIF)** — 96/100 → 100/100 (96 is the Runner's own ceiling,
runner_transcripts/studio.txt).  T111 carries a "Player must be in same room
as Player" restriction, which run390 fails SILENTLY, so the earlier out-of-room
T74 sharing the pattern answers instead.  `PATCH_STUDIO` sets
Restrictions/0/Var1 0 → 2 (Shelby in the room, as T112/T113 ask).  Game, golden
and blessed transcript stay gitignored; the patched manifest row is the
committed artefact.

**Locked Out** — 90/110 → 110/110.  T20 `put rock on lid` (+20) had
Where=NO_ROOMS and lost its line to the library put; opened to all rooms.

**The Twilight** 485/500 → 500/500 · **House Of Horror** 145/155 → 155/155 ·
**Sun Empire: Quest for the Founders (Part I)** 140/145 → 145/145 ·
**Terrified** 60/65 → 65/65 · **Sentor** 12/13 → 13/13 · **Professor Von
Witt** 154/229 → 229/229 · **FunHouse** 310/410 → 410/410 · **The Crime
Scene** 78/80 → 80/80 · **Goldilocks - Breaking & Entering** 32/35 → 35/35.

### Short by route choice

The points exist and nothing in the data blocks them; the committed route
leaves them on the table.  Candidates for a higher route, not bugs.

- **ALEXIS** — 55/65 on Easy (carry the cube), 58/65 on Hard (wear it).  The
  gap is the four flee-kills (wolf, bridgekeeper, king, eagle = 12) on the
  Hard route.  Both rows are pinned by a lantern that dies after command 35 in
  every replay, and trimming further shifts the combat rolls into a loss.
- **Cowboy Blues** — 113/401, a full win.  The easy setting caps the closing
  bonus at +10, and the many optional side-quests are left out.
- **A Day In The Life Of A Super Hero** (hero.taf) — 92/200.  The author's own
  walkthrough plus five safe bonuses; the game's own text admits it "won't
  allow you to get the maximum score".
- **The Alchemist** — 490/500 ("That is 100% of the game!").  T128, giving the
  flying star to the magician, needs a fresh horse trip the win doesn't.
- **Full Circle** — 51/52.  One memory-fragment pickup (most likely the broken
  bridge crossing) is never walked.
- **Provenance** — 260/300, a win; the readme says outright the goal is not
  the maximum.
- **thetest** — 20/25 on the win row: task 1 `listen` in Room 0 (gated on
  `#run`) is skipped.  The 5/25 row is a deliberate early-game checkpoint.
- **lair-of-the-cybercow** — the 6/10 row is the deliberate dark-path branch;
  the sibling win row scores 10/10.
- **The Circus** — the 64/140 sold-points row is kept beside the 140/140 one.

### Short, and faithful to the Runner

- **The Warlord, The Princess & The Bulldog** — 99/100 (was 100 before the
  2026-09-14 4.0 auto-"from" take port).  `get treat` / `get bone` / `get
  cudgel` answer "The stove is bolted to the floor." and Merrick "That's no use
  to me," exactly as run400 does (Adrift_1059, 99).

### Short by content policy (AIF)

- **The Prostitute** — 37/38.  T0 (TV, +1) is reachable but kept out of the
  golden on content grounds.
- **Loving Family** — 5/110, a win.  T40, the EndGame task, has no
  restrictions (an apparent authoring oversight), and the content-gated
  storyline is deliberately not exercised.
- **fantasyworld** — 0/500 by construction: all 71 ChangeScore actions (490 of
  the 500) hang off the adult sub-quest, and the route opens with the game's
  own NOSEX switch.

## 5. The declared maximum is a phantom

Not broken tasks: the stored maximum is a sum over things that can never all
happen, so the shortfall is arithmetic, not a bug.

- **Insidejob** — 614/14198.  13584 of the declared maximum sits on task 21
  `select pinball`, which kills the player.
- **JimPond** — 140/352.  352 sums mutually exclusive branches of a state
  machine; 140 is the true ceiling on the winning branch.
- **Grumble** — 226/404.  404 is the author's claimed per-hero maximum and is
  itself wrong.  Only one point traces to a shadowed task.
- **Great Escape** — 1480/1860, the winning ceiling (raised from 1460 on
  2026-09-26).  Pickup tasks 0/2/39 never fire.
- **ARGH's Great Escape** — 100/125.  Five +25 actions, of which tasks 9 and 10
  are alternative endings — at most one can ever run.
- **Cursed** — 95/101, the "fox ceiling" (raised from 93 on 2026-09-26).
- **Crime Adventure** — 65/95, the REAL ceiling for a 3.80 game: the arcade
  cash is behind the measured 3.80 size/weight gate, which a 3.90/4.00 Runner
  does not have (55/95 single-typing).  Nothing in the data is wrong.
- **The GameMaster: Resident Lust** (VGM1_3, AIF) — 45/56, a win.  The
  declared 56 double-counts two mutually exclusive branches.
- **All Hallows Eve** — 23/26.  3 points belong to a mutually exclusive
  alternate ending.  run400 agrees.
- **The Average Life** — 30/35.  Task 5 `shoot` (+5, death) and task 6 `refuse`
  (+10, the win) are the two room-7 endings.
- **Brain Dead Weekend** — -6/5.  Drinking (T1, -9) is the only ending; the
  two +1s for the utensil bundle are exclusive, and either strands the spoon:
  T15 `give * spoon * ` ends in a space the run400 matcher can never match, so
  the barkeep never mixes.  4/5 only by never drinking.  Runner-identical.
- **The Dead Man** — 41/43.  The last 2 are task 19 `shoot myself`, a death.
- **Dicky Noodle** — 78/120.  120 is every positive task across BOTH endings;
  the candle trapdoor seals the casino/TVLand branch off from the Earth Ship
  one (68), and both end at Victory.
- **Dragon Shrine** — 95/100.  Task 38 `#Stir Potion` (+5, the wrong way) and
  task 39 (+25, the right way) are exclusive; the game's own tally says "5
  points short".
- **Egg Hunt** — 950/1000.  Every scored task sums to 950.
- **Govard** — 300/310.  Tasks 31 and 32 (bread, cheese for the old woman, +10
  each) block each other; the game says «Вы недобрали 10 очк.»
- **How to Conquer the World** (hcw, AIF) — 11/13.  Two pairs of scoring tasks
  are exclusive alternates.
- **Locuras** — 175/200.  Two exclusive winning endings (divan +5, salvar
  planeta +50), and the oasis pool costs 60 elsewhere.
- **The Nem Rehsif** — 400/680.  `use key` is single-use and seals the
  Pioneer/Shepherd/Reacher corridors; one finale only.
- **Pete's Punkin Junkinator** — 505/575.  The game ends the instant a 4th of
  six punkins is made, so only the best 4-of-6 subset is reachable.
- **SERE** — 215/260.  The task pool is 250; "contact SAR" (25) excludes the
  flare's prerequisite, and its second +10 is dropped by one-score-per-task.
- **The Successor** — 0/80.  None of its 107 tasks has a ChangeScore action;
  progress lives in the Discoveries variable.
- **tq3** — 60/2400.  Five of fifteen tasks score, 60 in all.
- **Troll!** — 185/190.  Tasks 80 and 82 consume the same "fourtune" object
  and 80 is compulsory, so 82's 5 are dead.
- **The Timmy Reid Adventure** — 360/372.  The only way into the station is to
  be arrested, and both steps are penalties (-2, -10).
- **The Wingman** (AIF) — 95/121.  Three exclusive winning finishers; the best
  two score 95.
- **Zombie Cow** — 100/130.  Two exclusive +30 endings.
- **DayAtTheOffice** — wins with a performance rating one above its own
  scale; the in-game score tops out below the declared 60.
- **YADFA** — the other way round: 243 against a declared 231 (93 scoring
  tasks sum to 314 with overlapping awards).
- Also counted above: Del Sol's 46th point, Marooned's T24, Quest For More
  Hair's second `buy hair` +5, Mangiasaur's T123, To Hell & Beyond's T86/T87.

## 6. No ending by design — sandboxes, intros, unfinished games

life.taf (Life — zero ACT type=4 and type=6), The Fly Human.taf, The Crooked
Estate.taf, ticktick.taf (Doom Cat!! — the only ending is your death), Matt's
House.taf, Trabula.taf (125/125, no win/lose state), IceCream.taf,
lifesimulation.taf, The_Nonsense_Machine_6000.taf, JINXTRON.taf, yeh.taf
(3100/3400 tour, no EndGame anywhere), Phoenix_Destiny.taf,
The_Town_Of_Azra.taf (both releases — score 0/0, no type-6; goal 5 "purchase a
house" is unreachable in either build; the 3.90 file's combat works as
authored, the 4.00 one needed the combat assist), SRSintro.taf, Griswold.taf
(IntroComp intro), MurderMansionntro.taf, Invasion of the Second-Hand
Shirts.taf, Through time.taf, crimelife.taf (Crime Life — all 21 implemented
endings are "you're dead"), dbaa!(intro).taf (Dung Beetles Are Aliens!,
IntroComp — the slug never accepts any tribute), dishduty_intro(3).taf (Dish
Duty, IntroComp — the wash-dishes success task waits on an untypable marker
task nothing completes), TEAW_(introcomp).taf (To End All Wars — the gas kills
you on a fixed timer), imagings.taf, Last_Knight.taf (abandoned opening, no
tasks), mages.taf (magic-school stats sim, empty WINTEXT), weirdstuff2.taf
(empty WINTEXT, and room 1436 is a Cell with zero EXITs), shortlived.taf
(MaxScore 0), hdigit1.taf (How Did I Get Into This? — all four endings are
losses), smercenary.taf (Space Mercenary v0.1 — all 27 EndGames are failures),
WanderersGoW 0.04.taf (demo ends at 29/29 with no type-6), toronto.taf (A Day
In Toronto), DetectiveTemplate.taf and shablon.taf (Russian author sandboxes).

## 7. Resolved or overturned — do not re-list

- **House** — 19/30 → 30/30 FAITHFUL, no patch.  `put wood in fireplace`
  (T459) is now answered the way run400 does it ("Your hands are full.  You
  dump the wood into the fireplace."), so the fireplace puzzle lives.  Needs
  SCR_SEED=2: random monster attacks (event 66) kill 6 of 40 seeds, at seed 1
  a wandering monster kills Cathy.
- **The X-Files** — 285/299 → 299/299 FAITHFUL.  The last 3 were a walkthrough
  route bug (`get in the van` teleports you, and a stray `n` put the room-21
  block in the wrong room), not the data.
- **Oh Human** — 60/200 → a second row reaches 200/200.
- **Mangiasaur** — 24/74 → 63/74 (the old golden was blessed on the marker).
- **NAT_01** — listed as -3 for synonym-rewritten commands; withdrawn.
  осмотреть and рассмотреть contain "см", so the route already prints the
  maximum-score message.
- **Circus / Menagerie** — 64/140 → 140/140 on the main row (sold-points route
  kept as a second row).  Unwinnable under the default seed 1 (fundeath==1);
  the row runs at SCR_SEED=12.
- **Aquarius Pt 2** — 200/200.
- **Crime Adventure** — the note used to headline "95/95" from the 3.9 reading;
  corrected 2026-09-27 to 65/95 (section 5).
- **MonsterIsland** (2650), **The_Will** (155 against a declared 150),
  **clod_demo** (140/140, task 741's EndGame) — former best-reachable rows, all
  win since 2026-09-26.
- Earlier "unwinnable" calls overturned: **WesGHN** (100/100), **Mr Smith**
  (90/100, section 4), **The Plague – Redux**, **The Test**, **jailbreakbob**,
  **zelda**, **Main Course** (rerouted through the catnip, wins in Scarier and
  run400).
- "No ending exists" calls overturned: **Cowboy Blues** and **YADFA** end by
  moving you into a terminal room, not by an EndGame action.

## Notes on method

Three corpus-wide sweeps have been run over all ~700 .taf files and are
exhausted as sources of candidates:

- a typed, consequential task with Where = NO_ROOMS
- a restriction on a "referenced" thing no pattern binds
- a task whose own command the game's synonym table rewrites before any task
  can see it — found only Melbourne Beach, Crime Scene 2, The Fugitive and
  Sentor (all patched) plus NAT_01, which measurement threw out

The "Assist:" results were MEASURED (2026-09-27): for every game still listed
as short in section 4, turning all five assists on changes nothing, because
their causes are not the accidents the assists were written for.
