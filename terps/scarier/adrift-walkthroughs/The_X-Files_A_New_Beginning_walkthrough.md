# The X-Files: A New Beginning (Prologue) — Walkthrough

ADRIFT story game ("the first in a series"). You are **William Mulder**, son of
Fox Mulder and Dana Scully, finally granted access to the X-Files on Christmas
Eve 2027. A linear, conversation-driven investigation (no combat) across three
acts: FBI HQ / Washington D.C., a side trip to Arlington, and the town of
**Bellefleur, Oregon**. Derived under SCARE (deterministic seed).

**Status: WON, full game verified end-to-end at 299 / 299 (100%), deterministic.**
The win is the scripted `The End` ending in the Guest Bedroom (no combat), and
every scoring task is reachable for a full **299 / 299**.

**Talking to characters:** use `ask <name> about <N>` with topic *numbers*
(1, 2, 3 …); the topics advance in sequence and the payoff often comes a couple
of topics in. Several milestones score when you *take/examine* the thing the NPC
offers, not when they offer it.


---

## Full verified walkthrough (299 / 299, win)

The complete command list is `harness/xfiles_solution.txt`. Annotated by act:

### Act 1 — Washington, D.C. / Arlington (FBI HQ + your apartment)

```
(blank)                     clear the long intro; you start in Your Office
open desk
take all from desk          mug, gun holster, cell phone, the gift, badge, memo
clean desk                  +1   (the "Clean *" gag task — "clean <anything>")
burn memo                   +1   (the "Burn" gag — the memo isn't needed later)
strip                       +1   (the "Strip Tease" gag — you go naked)
wear holster                redress the holster (only the holster matters)
drink coffee                +1
d                           -> Lobby
e                           -> Parking Garage A
d                           -> Parking Garage B (Cancerman, the "P4" meeting)
ask cancerman about 1/2/3   who you are / your parents / are they dead?
take case file              +25  (Case File 10193, holds a Warehouse Key)
take warehouse key
examine warehouse key       +25  (labelled "Davis Storage, Baltimore, MD / 5")
u, out, w                   -> In Front Of FBI HQ
n                           -> Davis Storage Warehouse, Baltimore
use key                     +25  (garage door groans open)
take knife                  (Small Pocket Knife, in Garage 5)
out, s                      -> In Front Of FBI HQ
examine nameplate           +25  (it clicks off; you pocket Mulder's nameplate)
donate to the poor          +10  (the charity Volunteer/bell-ringer is here)
```

Arlington side-trip (your home cluster, reachable south of FBI HQ — collect two
small points, then come straight back):

```
s                           -> Outside the Red Bull Tavern, Arlington
in                          -> Red Bull Tavern (Nick the barkeep)
play jukebox                +1   (pops a quarter in — "One" by Filter)
u                           -> Your Apartment (fish tank, coffee table)
take fish food container
open fish food container
take pellet
open fish tank              you must open the tank first
feed fish                   +1
d, out, n                   back to In Front Of FBI HQ
```

Then the Falls Church / Lone-Gunmen run:

```
(blank)
sw                          -> Outside John Doggett's House (nameplate opened this)
knock                       +25  ("Uncle John" pulls you in; needs badge + nameplate)
out, ne                     -> In Front Of FBI HQ
w                           -> Outside The Lair Of The LGM (a camera, a VW van)
look at camera              +25  (you grin into the lens; the locks click open)
call ruth                   +10  (check in with your partner)
```

### Act 2 — the van west to Bellefleur, Oregon

The gun you need for the climax is **hidden inside the wrapped gift** — open it
before boarding, or the van refuses you ("take your gun along, just in case"):

```
open gift                   reveals Your Gun (and an Alien Keychain)
take gun
out                         -> Outside The Lair Of The LGM
get in the van              +25  (Get In The Van; needs case file, memo, GUN held)
                            -> a 3,000-mile cut-scene; you arrive in Bellefleur
```

### Act 3 — Bellefleur, Oregon

Hub town around a Town Square. Pick up the optional points, get directions, then
the van takes you to the forest for the finale:

```
e, n                        -> Outside Alan Dale Motel
in                          -> Alan Dale Motel
u                           -> the Gunmen's rented room (Langly's laptop, phone book)
touch labtop                +1   (triggers a Byers scuffle; you wake with ice)
sleep                       +1   (the night passes)
take phone book             (R0 of the look-up: you must be holding it)
open phone book             (R1: it must be open)
look up byers               +1   (any character name; "…must be unlisted")
d, out                      -> Outside Alan Dale Motel
n                           -> Town Square
se                          -> Outside Dean's Diner
in                          -> Dean's Diner
buzzer                      +10  (you can't resist pushing the buzzer)
out, nw                     -> Town Square
ne                          -> Outside Arroway Hardware
in                          -> Arroway Hardware (Mr. Arroway)
take directions             +25  (the route to the site; the Gunmen gather here)
out                         -> Outside Arroway Hardware (the VW van; a tailing car)
get in the van              +25 *Van* and +10 Arrive-Outside-Forest
                            -> the van dies at the orange "X"; you draw your gun
look                        let the arrival settle one turn
enter the forest            +25  -> Forest Clearing (a hidden Soldier)
ko                          knock the soldier out -> the cabin / Guest Bedroom
the end                     THE WIN — your parents (Mulder & Scully) reveal all,
                            "Welcome to the Resistance."
```

Final score **299 / 299 (100%)**, deterministic (identical on three runs).

## Pitfalls (do NOT do these)

The author left several joke "bad end" tasks that fire instantly:
- **`*End Game`** (at In Front Of FBI HQ) = a **−500** lose ending.
- **`Kill Cancerman`**, **`Kill *Car`**, **`*Suicide`** = death endings.
- **`Kill Langly`** = **−10** (and a battle). The other Gunmen are friends.

## The last point — `Look up *%character%*` (+1) — and a corrected note

The Bellefleur phone-book task (`look up byers`, in the Gunmen's room) is the
hardest +1 to find, and I initially **mis-diagnosed it as a SCARE parser bug** —
it is not. SCARE's parser matches `Look up *%character%*` against `look up byers`
just fine (the `%character%` binds); the task simply has two **restrictions** that
must both hold:

- **R0 — you must be *holding* the phone book** (object location: held by player).
- **R1 — the phone book must be *open*** (object state: open).

My early attempts only `open`ed the book without picking it up, so R0 failed and
the command fell through to the library "look up" verb ("Have X-Ray vision, do
you?"). The fix is just: **`take phone book` → `open phone book` → `look up
byers`** (done in the room-21 block of the solution). Then the +1 scores ("…it
must be unlisted. Doesn't anyone in this damn community have a telephone?").

So **there is no SCARE bug here, and no engine obstacle anywhere in the game** —
every scoring task is reachable. The verified result is a full **299 / 299**.
(Aside, confirmed while chasing this: ADRIFT's `*` wildcard is zero-or-more, and
both the 3.9 and 4.0 Runners — `run390.txt`/`run400.txt` — match this pattern;
SCARE matches it too. The earlier "wildcard quirk" theory was wrong.)
