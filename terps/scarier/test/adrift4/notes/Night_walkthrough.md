# The Night That Dripped Blood — walkthrough (**WIN**)

- **Engine:** ADRIFT (SCARE-compatible .taf). A horror/mystery vignette:
  after a one-night stand, Petter is sent on an errand chain by his
  cafe-owner boss, Monk, then chases down a "seek out the Blue Boar" note
  through the town's Rare & Lost Books shop, gets trapped in the fairground
  Ghost Train by its owner Glenn, escapes, phones Glenn's house, is ambushed
  in the alley behind it, and gets the full backstory (a WWII U-boat
  massacre turned 1950s pulp novel turned a recurring town curse) from
  Glenn's partner Chloe before finally meeting Rachel on the beach.
- **Result:** genuine win, **100/100** — not just a best-reachable ending.
  Final text: *"You scored 100 out of the maximum 100! That is 100% of the
  game! Well done - you scored maximum points!"* Wired as
  `night_solution.txt|The_Night_That_Dripped_Blood.taf||`, no env, win
  marker on the final lines.
- **The TV remote chain:** `look at couch` in the Lounge (the very first
  room) digs a TV remote control out of the back of the couch; carrying it
  lets `look at remote control` be done from anywhere later to find
  batteries — needed to `play radio` in the Cafe Kitchen once the Monk
  errand chain reaches that point. Neither the remote nor its batteries are
  placed in the world at game start (`SCR_DUMP_OBJLOC` shows both start at
  `room=-1`) — only this task ever places them.
- **Monk's wages are a multi-step chore, not a simple handoff:** `give
  timesheet to monk` alone fails (unsigned). The actual unlock is `ask monk
  about errand`, which starts a repeatable `cook burgers` chore in the Cafe
  Kitchen that stalls forever until the player also `give magazine to monk`
  (the X-rated magazine found via `get post`/`open envelope`), then `look at
  desk` and `look at chair` reveal his office chair is held together by
  screws. `loosen screws` needs **both** the screwdriver (from `search
  washing machine` → `get screwdriver` in the Utility Room) **and** the
  radio already playing as noise cover, or Monk catches the player in the
  act. Done correctly, Monk falls, spills his coffee, and flees, dropping
  his wage packet on the desk — `get packet` then `open packet` gets the
  cash needed for everything downstream.
- **The Blue Boar subplot is order-sensitive:** `read note` (from the
  envelope) must happen *before* `read newspaper` — the newspaper's advert
  text (revealing the bookshop's address) only appears once the note has
  already been read; reading them in the wrong order silently loses the
  advert. At the shop, `ring bell` summons Miss Bent; `ask miss bent about
  blue boar` then `buy book` (needs the Monk cash) gets a 1950s pulp novel
  about undead Nazi U-boat crewmen haunting the town — `look at book` reads
  it, dropping a flyer, and `look at flyer` reveals a note that finally
  makes the fairground's Ghost Train owner, Glenn, appear and start selling
  tickets. He is provably absent before this (`buy ticket` fails with "The
  Ghost Train is closed for repairs," and waiting 15 turns at the ride
  produces no change) — this is a task-completion gate on his NPC presence,
  not a walk schedule or a timer, confirmed by reading `screstrs.cpp`'s
  restriction dispatcher (a type-3 "player/NPC" check testing whether Glenn
  is physically in the room) and then empirically by completing the whole
  bookshop subplot first.
- **The Ghost Train is a scripted trap with its own escape puzzle:** `buy
  ticket` locks the player inside a rigged car. `look at tunnel` finds a
  lever; `pull lever` opens an escape route through two more tunnels; `look
  at floor` at the dead end finds a trapdoor, and `d` slides down a chute
  back outside. The ticket, examined afterward (`look at ticket`), now shows
  a phone number on its back.
- **The phone call leads to a scripted alley confrontation:** back at the
  house Kitchen, `call number` reaches Chloe and then Glenn, who gives his
  bungalow's address. At the bungalow's Alleyway, `look at window` triggers
  Glenn — intending to kill the player to "end the curse" — cornering them
  with a sword; `attack` resolves it (Chloe secretly drugs Glenn), and both
  are carried into the bungalow's Sitting Room. `ask chloe about curse` (any
  topic works once she's willing to talk) delivers the full backstory: a
  1945 U-boat, U53, sunk while attempting to surrender; 1950s writer Daniel
  Daniels novelized it as "The Blue Boar" and, per Chloe, unwittingly
  invoked real dark magic, cursing the town with recurring undead-crew
  massacres tied to the book resurfacing.
- **The beach/Rachel ending only opens after Chloe's exposition** — `swim`
  at the Beach (Clifftop → Top Of Path → Path → down) introduces Rachel;
  `ask woman about herself` then `ask rachel about herself` (she confirms
  she's twenty-one) leads to shelter from sudden rain in her car, where
  `kiss rachel` delivers the game's win text and full score.
- **Content note:** the endgame scene (Rachel, in her car) and the Alleyway
  window scene (Glenn and Chloe) both include explicit adult sexual/nudity
  content. The game states Rachel's age directly ("I'm twenty one years of
  age") and independently describes Glenn as "in his mid twenties" when
  first met at the Ghost Train — no minors appear anywhere in the text.

## The walkthrough

```
Petter
male
e
in
w
d
look at couch
e
s
search washing machine
get screwdriver
n
w
get post
open envelope
read note
open door
out
e
n
n
w
give timesheet to monk
ask monk about errand
n
cook burgers
look at remote control
play radio
s
give magazine to monk
look at desk
look at chair
loosen screws
get packet
open packet
read newspaper
e
s
s
s
in
look at counter
ring bell
ask miss bent about blue boar
buy book
look at book
look at flyer
out
n
e
e
e
n
buy ticket
look at tunnel
pull lever
e
n
look at floor
d
look at ticket
w
s
s
w
w
w
w
in
e
call number
w
out
e
n
w
w
look at window
attack
ask chloe about curse
s
w
w
d
d
swim
ask woman about herself
ask rachel about herself
kiss rachel
```
