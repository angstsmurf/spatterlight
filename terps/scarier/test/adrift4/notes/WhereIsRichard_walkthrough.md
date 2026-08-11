# Where Is Richard? — walkthrough

- **Engine:** ADRIFT 3.90 (`Richard.taf`, 55,039 bytes), by Rich Dersheimer,
  April 2001. **53 rooms, 5 NPCs, 57 tasks, 13 events, 12 variables.**
- **Result:** ★ **WON, 1000/1000** in 68 commands with zero parser failures.
  Ten `ACT type=4` in the file; nine of them are the route and the tenth is
  a −50 penalty you simply do not take.
- **Solution:** `goldens/richard_solution.txt`. Golden blessed, in
  `run_v4_walkthroughs.sh` with `SCR_SKIP_WAITKEY=1`. Win marker:
  `Rich smiles as you hand him the recall beacon`
- **Provenance:** no published walkthrough. The IF Archive package ships
  `Map.jpg` (a hand-drawn sketch of the swamp) and seven `.wav` files, but no
  solution. Derived from `SCR_DUMP_TASKS`, `SCR_DUMP_OBJLOC` and play.

## What it is

Your friend Richard has not answered his phone in a week. You let yourself
into his house, the door locks behind you, and the computer in his study turns
out to be the console for a matter transmitter in the basement. Richard is on
the other end of it with a busted knee, and he has lost the beacon that gets
you both home.

It is a small, well-made, entirely fair game: every obstacle has three or four
solutions, and the only thing it will not forgive is eating the cupcake.

## The score

| Task | Command | Where | Points |
|---|---|---|---|
| T0 | `turn on computer` | computer room 0 | +100 |
| T15 | `type qwertyuiop` | 0 | +100 |
| T16 | `type 001 001 002` | 0 | +100 |
| T40 | `sleep` | bedroom 2 or 7 | +50 |
| T23 | `water plants` | yoga room 6 | +50 |
| T26 | `# fire gone` | tunnels 18 | +100 |
| T37 | `# spider is dead` | corridor 31 | +100 |
| T53 | `# Goo is dead` | swamp 50 | +100 |
| T41 | `press button` holding the beacon | plain 34 | **+300**, and the win |
| T5 | `shoot window` | — | **−50** |

None of the nine repeats: T0/T15/T16 each push `VAR 0 ComputerStatus` past
their own restriction, and T23 and T40 are `rep=0`. T41 carries the file's
only `ACT type=6`.

## The one timer: the capacitor

`ComputerStatus` walks 0 → 1 → 2 → 3 → 4. You drive 0→1 with
`turn on computer` and 1→2 with the password `qwertyuiop`. **2→3 is not a
command at all** — it is EVENT 0 `[Capacitor charging]`, `starter=3
startTask=16` (1-based → T15, the password) with `time1=time2=8`, which runs
T4 `# cap charged` eight turns later.

Only at 3 does T16 accept `type 001 001 002`, and T16 is what sets
`VAR 6 LightStatus = 1` — the restriction on T18 `press button` in the phone
booth, i.e. the transfer to the mining tunnels. So: type the password first
and spend the eight turns looting the house. The route spends sixteen and
never has to wait.

The password and the coordinates are both readable in-game (`x computer`
after each stage), and both have `HINT1`/`HINT2` spelling them out.

## The one-cupcake problem

There is exactly one cupcake and it solves **both** monsters:

| Obstacle | Solutions |
|---|---|
| spider (corridor 31) | T7/T8 shoot it *twice*, T10 hit it with the pick, T12 give it the cupcake |
| goo (swamp 50) | T47 feed it the berries, T48 give it the cupcake, T52 pour the pail of water on it |

Spend the cupcake on the spider and the berries on the goo, as the route does,
and **nothing else in the house is needed**: the small key, the pistol, the box
of ammo, the knife and the pick can all stay where they are.

Feed the cupcake to the goo instead and the spider has to be shot, which means
the key (kitchen cabinet) → `unlock pistol` (guest-bedroom dresser) → the ammo
(yoga-room closet) → `load pistol`, and then two shots, because
`VAR 9 SpiderHealth` takes T7 then T8.

## Do not eat the cupcake

T24 `eat cupcake` sets `VAR 5 Health = 1` and starts EVENT 10 `[Poisoned]`, a
`restart=1 time1=1` event that keeps re-running T56 `# poisoned` (`Health +=
1`) until T57 pauses it. A non-zero Health blocks T19 `climb washer` (the
attic), T20 `get rope`, T21 `fill pail`, T28/T29 the spike and T33 `dig` — most
of the game's verbs, in other words. The bottle of Papto Dismal in the master
bathroom (T14) is the antidote, and a route that never gets poisoned never
needs it.

## The closets do not open

`open closet` answers *"You can't open the … closet!"* in all four rooms that
have one; the closets are already open and `x closet` lists the contents.

| Container | Holds |
|---|---|
| hall closet (4) | the watering pail |
| master bedroom closet (7) | the backpack → **the cupcake** |
| yoga room closet (6) | the box of ammo |
| guest bedroom closet (2) | the rope (`get rope`, T20) |
| kitchen cabinet (1) | the small key |
| guest bedroom dresser (2) | the pistol |
| master bedroom nightstand (7) | the journal |
| kitchen cutting board (1) | the knife (on it, not in it) |

**The cupcake never leaves the backpack.** T12's restriction is `RESTR type=0
v1=7 v2=1` — "held by the player" — and the Adrift 4 runner answers TRUE for
an object one level down inside a container the player carries. So the route
picks up the backpack and never opens it, and `give cupcake to spider` fires
anyway. This is the same rule that `probe p39held` pinned in the real
run390.exe and that *Cursed* depends on; *Where Is Richard?* is the cleanest
corpus witness for it.

## The fire

Room 18's north exit is `gateTask=26 wantDone=1`, and T26 `# fire gone` is a
bookkeeping task reached by three zero-delay `starter=3` events:

- EVENT 1 ← T27 `put mat on fire` — the yoga mat, lying loose on the floor of
  room 6, needing nothing else. **This is the cheapest and the route's pick.**
- EVENT 2 ← T30 `tie rope to spike` — the rope from the guest bedroom closet
  plus the spike from tunnels 28, hammered into a beam with the pick.
- EVENT 3 ← T34 `throw dirt on fire` — dirt dug with the pick (T33).

T31 pours the pail of water on the flames, which also works. Note the pail can
be **refilled inside the tunnels**: T21's `WHERE_ROOMS` is `[1 5 8 23]` and
room 23 is mining tunnels<11>. The route spends its one pail of water on the
plants instead, for the +50.

## The map

House:

```
 0 computer room: E->3 living room   S->2 guest bedroom  W->1 kitchen
 1 kitchen:       E->0  S->6 yoga    W->9 laundry
 2 guest bedroom: N->0  W->4 hallway
 4 hallway:       E->2  S->5 bathroom  W->6
 6 yoga:          N->1  E->4  W->7 master bedroom
 7 master bed:    N->8 master bathroom  E->6
 9 laundry:       E->1  U->10 attic *T19 (climb washer)  D->11 basement
11 basement:      U->9  IN->12 phone booth
```

The attic — a big transformer and a heavy-duty capacitor — is pure scenery;
nothing up there is needed, and `climb washer` is only on the route in games
that go looking.

Mining tunnels, entered at room 13 by the phone-booth button:

```
13 -W- 14 -N- 15 -N- 18 (FIRE) -N- 21 -W- 22 -W- 25 -U- 29
29 -S- 30 -S- 31 (SPIDER) -S- 32 *T37     32: pull handle -> 33 staging area
```

Swamp, from room 34 (`33 OUT-> 34`, `34 N-> 38`):

```
        43 - 44 - 45 - 46 - 47 - 48 - 49 - 50   GOO / beacon
         |    |                    |
40 - 41 - 42                      51
 |    |    |                       |
37 - 38 - 39                      52   the bush: berries
 |    |
35   34   surreal plain
```

Out: `n n e n n n w w s s` (34→38→41→42→44→45→46→47→48→51→52).
Back to the goo: `n n w w` (52→51→48→49→50).
Home: `e e e e s s s w s s` (50→49→48→47→46→45→44→42→41→38→34).

T42 `press button` on the plain **without** the beacon just returns you to the
staging area, so pressing it early is harmless.

## Notes

- WINTEXT is non-empty — Rich's homecoming speech — so `Congratulations!` is
  suppressed and the transcript ends on the game's own text.
- Rich (NPC 3) sits in the staging area from the start and delivers the
  premise, the map object and the warning about the spiders in one long block
  when you pull the handle in room 32.
- `sleep` is worth +50 and is gated on `VAR 11 DreamState == 0`, so it pays
  once. The dream is the game telling you what your goal is.
- The README promises a sequel — *"What is that strange structure?"* — which is
  the tower visible to the west of swamp room 50 and unreachable here.
- **This game is why `SCR_DUMP_TASKS` had to be fixed.** One of its objects
  has no `Openable` property, and the LOCKKEY / OPENABLE loops in
  `scdump.cpp` called the *fatal* `prop_get_integer()` on it, so the dump
  aborted right after the OBJNAME block — before ROOM, TASK, EVENT or NPC were
  ever printed. The game itself always loaded and played fine; it was purely
  the dev-only instrumentation.
