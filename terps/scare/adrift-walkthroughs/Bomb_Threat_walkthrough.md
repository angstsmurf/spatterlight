# Bomb Threat — walkthrough (WON, 0/0)

*Bomb Threat* — an ADRIFT comp game in which you are **Jack Wayne**, an FBI
agent. A phone call in a street booth informs you that a bomb has been planted
in the Imperial Agency skyscraper next door, and the anonymous caller (the
bomber, **Edgar Henderson**) sets you a "little game": follow his floor-by-floor
clues, find and defuse the bomb, then survive the final confrontation.

- **Result:** WIN, deterministic. There is **no score** (the game has zero
  ChangeScore actions — `0/0`), so the win *is* the maximum result.
- Solution: `harness/bomb_threat_solution.txt`. Verified 3× identical
  ("Congratulations!").

## Full command list

```
out
w
in
up
out
s
open desk
take key
unlock cabinet
open portfolio
look at piece of paper
n
n
open folder
take card
s
read magazine
in
press 3rd floor
out
n
take pliers
take crowbar
s
in
down
out
e
down
open package
z
cut red wire
shoot edgar
```

## Phase by phase

**1. Out of the booth and into the building.** From the Telephone Booth go
`out` to the Footpath, then `w` into the Lobby and `in` to the elevator. Ride
`up` to the 28th floor (`up` reaches the 28th directly; the caller's first
instruction) and step `out` into the corridor.

**2. The office — key, cabinet, security card (28th floor).** Go `s` into the
Office Room. `open desk` (a key is inside), `take key`, `unlock cabinet` (the
key opens it, revealing a portfolio), `open portfolio`, `look at piece of paper`
(the portfolio's clue). Then `n` back to the corridor and `n` into the
Conference Room; `open folder` and `take card` — the **security card** is needed
to enter the floor-3 Tool Room. Return `s` to the corridor.

**3. The magazine clue → floor 3.** `read magazine` prints the bomber's final
clue (a BMX magazine, page 72): *"to defuse the bomb you'll need one of these
[pliers]… I live on floor 3 and I am quite light…"*. Step `in` to the elevator
and `press 3rd floor` — the lift descends to floor 3. Go `out` to the corridor.

**4. The Tool Room — pliers and crowbar.** The north door demands security
clearance, which your card grants automatically. Go `n` into the Tool Room and
`take pliers` (to cut the bomb wires) and `take crowbar` (to open the manhole).
Return `s`, `in` to the elevator, `down` to the ground floor, `out` to the
lobby, and `e` to the Footpath.

**5. Into the sewers.** Holding the crowbar, the manhole on the Footpath opens
(an event watches for the crowbar each turn), exposing a `down` exit. Go `down`
into the Sewers, where a package sits. `open package` reveals the bomb. Pause
one turn (`z`) — see the timing note below — then `cut red wire`: the red wire
defuses the bomb (the **blue wire detonates it — instant death**), and Edgar
appears from down the sewer with a uzi.

**6. Beat Edgar to the trigger.** `shoot edgar` — Jack draws and fires; the
shot lands "right between Edgar's eyes," Edgar falls dead, and *"You have just
won Bomb Threat!"*

## Notes on mechanics / quirks (reverse-engineered)

- **No score, single win.** The game has no ChangeScore actions; the only
  victory is killing Edgar after defusing the bomb. There are several death
  endings: cutting the **blue** wire, the two "psycho blows the building"
  countdown timers (≈50 turns each, paused as you progress — never a threat on
  this route), and losing the Edgar shoot-out.

- **The "piece of paper" and "magazine" are command-only clues.** Neither is a
  real object — tasks `look at piece of paper` and `read * magazine *` carry no
  object/location restrictions, so they complete wherever you type them and
  simply print their clue text. The 43rd-floor Projects Room (reachable via
  `look at piece of paper` → `press 43rd floor`) is therefore an **optional
  detour**; this route skips it and reads the magazine on the 28th floor. The
  security card, by contrast, is genuinely required (the floor-3 door checks
  that you hold it).

- **The Edgar shoot-out is a random roll, and the `z` matters.** `shoot edgar`
  executes `hit += random(1,3)`: a roll of **1 or 2 wins** (a clean kill, tasks
  `#hitedgar1`/`#hitedgar2`), but **3 is death** (`#hitedgar3`). Cutting the red
  wire starts a 2-turn countdown (`#die2` = Edgar guns you down), so you can
  only fire on the single turn *after* cutting — there is no room to re-roll
  afterwards. The street "traffic" event draws two randoms every turn, so the
  outcome depends on the RNG stream position. Under the harness's fixed seed
  (1234), inserting exactly **one `z` before `cut red wire`** lands the roll on
  1 (a headshot) and makes the win deterministic. (A human playing live just
  retries the shot; the data is faithful — the roll exists in the original
  Runner too.)

## Tooling note

Surfaced and fixed a robustness bug in the committed dump module
`terps/scare/scdump.c`: the event dump read every `Events[e]` field with
`prop_get_integer`, which `sc_fatal`s on a missing property, so it **aborted on
any game whose events omit a field** (Bomb Threat's events lack `Obj2`/`Obj3`).
Converted those reads to the tolerant `prop_get` pattern used elsewhere in the
file and added `Time1`/`Time2`/`PauseTask` to the output — which is exactly what
made the Edgar/death-timer interplay legible here. The Spatterlight build never
compiles `scdump.c` (it is gated behind `-DSCARE_DUMP_TOOLS`, harness-only).
