# The Screen Savers on Planet X — walkthrough (WON, full 142/142)

**Game:** `The Screen Savers On Planet X.taf`. ADRIFT **3.9** (TAF header byte8
`0x94`/byte10 `0x37`, native). A fan game built around TechTV's *The Screen
Savers*: you are a guest on the historic first galaxy-wide broadcast from Planet
X, but **all thirteen cast members are missing** (Leo, Patrick, Martin, Scott,
Megan, Jessica, Joshua, Cat, Morgan, Tom, Roger, Yosh, Darci). You must track
each one down and get them back to the studio in time. **No combat.**
Deterministic.

**Result:** **WIN, full 142/142 (100%)**, verified 3× identical (marker:
*"You've managed to get everyone to the set! Congratulations!"*). Solution:
`harness/screen_savers_solution.txt` (132 commands).

## How the win works

There is exactly **one EndGame action** (task 0, command `*` = *any* input,
all-rooms) whose only restriction is **game-variable #1 == 13**. That variable is
a "cast members gathered" counter, incremented by **thirteen milestone tasks**
(one per cast member). The win therefore fires on the first command issued *after*
the thirteenth milestone completes — which is why the solution ends with a bare
`look`. The stored maximum **142 = thirteen +10 milestones + two +6 puzzle steps**
(`search room`, `drop bulldozer`); completing all thirteen milestones reaches the
win *and* the max simultaneously.

## Phases

The studio sits at the centre. The **Main hall** hub leads W→Netcam, E→Lab Area A
(which holds a **teleporter** to the TechTV office cubes), N→Studio lobby→Outside
studio. The **planet overworld** fans out from Outside studio (E twisty path → Sea
of Dust & Rocket Pad; N crossroads → Library/Dark Room & Hotel/Cafe; W hill → Mr.
Universe). The endgame is a **space section** flown from the Rocket Pad.

**1 · Collect the office items (via the Lab A teleporter).** Grab the **disc** and
**megaphone** in Lab Area A, teleport to the cubes, and collect the **tape**
(Patrick's cube), **video** (Martin's cube) and banana **peel** (Kitchen), then
teleport back. `put tape in megaphone` (doable anywhere).

**2 · Six overworld rescues.**
- **Megan** — `install disc` on her G4 in the Main hall.
- **Jessica** — `drop peel` in the Snack room (the refill servobot slips on it).
- **Martin** — `play video` at the Mr. Universe Contest (the embarrassing speedo
  footage clears the stage).
- **Darci** — `flip switch` in the Hotel game room (she rage-quits the rigged
  arcade game and hurls it into the **Hotel lobby**, scattering a **bazooka**,
  **tokens** and broken **glass** there).
- **Joshua** — back in the Hotel lobby, `shoot bulldozer` with the bazooka
  (shrinks the bulldozer menacing the hotel). **The bazooka is "Hotel property"
  and refuses to leave — `drop bazooka` before exiting**, but take the shrunken
  bulldozer.
- **Tom** — in the Dark Room (through the Library): `search room` (+6, finds a
  book) → `stand in light` → `read book` → `talk to Tom about John Hanson` (the
  answer to his "first president of the U.S. Congress" riddle).
- **Roger** — take **Roger's ID** in the Desert Cafe, `put ID in tube` at the
  Studio lobby's odd device (spits out a **printout**, which appears in the lobby
  itself), then `show printout to Roger`.
- **Patrick** — teleport to his cube and `drop bulldozer` (+6); the mini-bulldozer
  clears the junk pile burying him.

**3 · The space section (Rocket Pad).** `take`/`wear spacesuit`, `enter rocket`.
Travel by `set dial to <code>` + `push green button`; `push blue button` returns
to the pad. EVA (`exit rocket`) needs the spacesuit **worn**. Codes:
**1119** = Galactic Mart, **3071** = space-time rift, **4692** = satellite,
**7438** = flying saucer.
- **Scott** — dial **1119**, enter the Mart (`w`), `buy modulator` with the tokens.
- **Yosh** — install that modulator later (see below).
- **(rift, gathers Cat)** — dial **3071**, `exit rocket`, `enter rift` (allowed
  only *before* it's fixed) into the **Flying Saucer Storage Room**: `take sticker`
  (off the yellow crate), `put sticker on red crate` (reveals a space-time
  **patch**), `take patch`, `south` back to the rift, `fix rift`, then `enter
  rocket` (+10). *The sticker step also unlocks the saucer below.*
- **Morgan** — dial **4692**, `exit rocket`, `kick satellite` (+10), `enter
  rocket`, `push blue button` to the pad, `exit rocket` (the milestone).
- **Leo** — dial **7438** (last!), `exit rocket`, `enter saucer`: the AMD-chip
  aliens flee and Leo frees himself. **This entry teleports you straight back to
  Outside studio**, so the saucer must be the final space trip.

**4 · Yosh / finish.** From Outside studio, return to **Lab Area B** and `install
modulator` — the thirteenth milestone. The next command (`look`) triggers the win.

## Notes

- **Ordering constraints that matter:** the bazooka can't leave the hotel (drop
  it); the rift must be entered *before* fixing it (that's how you reach the
  storage room for the patch); and the **saucer trip must come last** in space
  because `enter saucer` warps you back to the planet surface.
- **Compass labels** in some overworld rooms read rotated versus the internal exit
  table — navigate by each room's "to the north…/east…" prose (the bundled
  solution already encodes the correct moves).
- **Object-move actions are 1-based room refs** in this file (e.g. the bulldozer's
  `v3=18` lands it in internal room 17, the Hotel lobby) while player-move actions
  are direct — a harmless engine-neutral quirk noted only because it explains where
  the scattered items appear.
- The `push green button` travel tasks award **no score** (an earlier read of a
  restriction's `Var1` as a ChangeScore was wrong); 142 is exactly the thirteen
  +10 milestones plus `search room` +6 and `drop bulldozer` +6. Faithful to the
  3.9 Runner; no SCARE change, no combat-assist.
