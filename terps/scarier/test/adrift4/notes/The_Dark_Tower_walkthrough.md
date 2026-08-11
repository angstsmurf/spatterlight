# The Dark Tower — walkthrough

- **Engine:** ADRIFT 3.9 (`DarkTower.taf`, 39,485 bytes). You fell asleep
  working late at A-Z Graphics and woke on the lobby floor with the power out
  and nothing in your pockets. **62 rooms**, 40 objects, 30 tasks, **no NPCs,
  no events, no variables**.
- **Result:** ★ **WON.** There is **no score at all** — zero `ACT type=4` in
  the file, and `score` answers *"0 out of a maximum of 0"* — so the finish
  line is T8 `turn on power` (`ACT type=6 v1=0`).
- **Solution:** `goldens/darktower_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `restored power to the building.`
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS` +
  `SCR_DUMP_OBJLOC`.

## Route in outline

The whole game is one sweep: get the gold card off the lobby floor, go down to
the garage for the flashlight, get the red card from the Gate House, then
climb the stairs floor by floor, using each floor's card once and emptying its
four suites, and finally come back down to Maintenance with twelve pieces of
hardware.

```
Lobby        take gold key card / take memo / x memo / use gold key card
Garage       look under van                       (the flashlight)
Gate House   take red key card
1st floor    use red key card
             Suite 100 -> power panel key, blue key card
             Security  -> paper (the key-card memo)
             Suite 101 -> breaker switch, elevator key
2nd floor    use blue key card
             Suite 200 -> power cable
             Suite 201 -> green key card
             Suite 203 -> transformer, garage key
3rd floor    use green key card
             Suite 300 -> patch kit
             Suite 301 -> yellow key card
             Suite 303 -> wire cutters
             Suite 302 -> gate key
4th floor    use yellow key card
             Suite 401 -> black key card
             Suite 403 -> diamond screwdriver
5th floor    Suite 500 -> repair manual        (no card needed)
             use black key card
             Suite 501 -> power cord
             Suite 503 -> wrench
             Suite 502 -> phillips screwdriver
Maintenance  take letter / x letter
             fix power panel / fix generator / pull lever / pull switch
             turn on power                     (EndGame win)
```

The five repair steps have to go in that order: T9 `pull lever` is restricted
on T6, T10 `pull switch` on T7, and T8 on both plus the two switches being
open.

## The two repairs

| Task | Needs held, all at once |
|---|---|
| 6 `fix power panel` | power panel key, breaker switch, power cable, patch kit, diamond screwdriver, phillips screwdriver, repair manual, wire cutters |
| 7 `fix generator` | transformer, wrench, repair manual, power cord |

Eight simultaneous held-object restrictions is the largest single task in the
v4 corpus so far. There is no inventory limit to fight, but there is also no
partial credit and no hint: `fix power panel` with seven of the eight in hand
simply falls through to the default refusal.

The repair manual is needed by **both** and is destroyed by neither. T6 does
scatter the tools — the screwdrivers, the panel key and the wire cutters all
get moved to room 54 (an elevator shaft), which is Gary the repairman's whole
characterisation.

## One card unlocks a whole floor

The key card tasks are `where=2` lists of hallway rooms, and every suite door
is `gateTask=<n> wantDone=1`. So a card is used **once** and every door of
that colour opens for good. The paper in Security spells it out:

> Also, for some reason when you unlock one office on a floor, all offices on
> that floor are unlocked as well. Same goes for the gold card. You use that
> in one location, all doors locked with a gold card are unlocked.

The cards chain strictly, each one locked behind the last:

| Card | Found in | Opens |
|---|---|---|
| gold | Lobby floor, turn one | Maintenance, Gate House, Security, Roof |
| red | Gate House (past the parking garage) | 1st floor |
| blue | Suite 100 | 2nd floor |
| green | Suite 201 | 3rd floor |
| yellow | Suite 301 | 4th floor |
| black | Suite 401 | 5th floor |

## Two things that stop a first attempt cold

- **Everything is dark.** Outside the lobby and the parking garage — which
  the introduction tells you have working emergency lights — every room
  answers *"It's too dark to see anything. You might be eaten by a grue. You
  need a flashlight."* The flashlight is under the black van at the north end
  of the garage: `look under van` (T27). Nothing points you there.
- **`read` doesn't work on the documents.** `read memo` gets *"You can't read
  the memo!"*, and the same for the paper and the letter. They are plain
  object descriptions: `x memo`. The three of them are the game's only
  characterisation — Tom writing to Bob about firing Gary, the key-card
  policy, and Tom writing to Gary about the missing manual.

## Dead content

The game ends the moment you restore power, and **a large part of the file
sits on the far side of that**:

- **The elevators.** T0–T5 (`press l`, `press 2` … `press p`) are all
  restricted on T8, and the six elevator lobbies are behind `gateTask=8`
  doors. The elevator key (Suite 101) and T29 `use elevator key` exist and can
  never be used.
- **The six elevator-shaft rooms** (54–59), and with them the blueprint and
  mysterious parts 1, 2 and 3.
- **The Entrance** (room 0). The player starts in the Lobby; room 0 is only
  reachable through `EXIT room=32 W -> 0 gateTask=17`, and T17 `open security
  gate` is itself restricted on T8. Mysterious part 6 is in there.
- **The whole alternative ending.** T22 `build box` wants all ten mysterious
  parts held at once; four of them are in the rooms above. The box would give
  the cube key, which gives T24 `push button` → the car key, which gives T26
  `use key` at the black van — a second `ACT type=6 v1=0`. Six of the ten
  parts are reachable; the puzzle is not.

None of that is a bug in SCARE: it is an unfinished game, and the WINTEXT
says so — **"To be continued................"**.

## Notes

- **T20 `kick van` is an instant loss** (`ACT type=6 v1=2`) and is the only
  losing ending in the file. `knock on van` and `look at van` are safe.
- **The garage key and the gate key are collectable and useless.** T23 `open
  gate` and T28 `use garage key` at the Gate House score nothing and open
  nothing — `EXIT room=52 E -> 60` is ungated, so the Garage Exit is already
  open.
- **No `<waitkey>` anywhere** (`SCR_MARK_WAITKEY=1`).
