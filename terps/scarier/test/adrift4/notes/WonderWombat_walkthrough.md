# Adventures of Thumper — Wonder Wombat — walkthrough

**File** `wonderwombat.taf`, 107,200 bytes, ADRIFT **3.90**.
**Author** Chris Tyson (the ending text is signed *"©Copyright 2001-2002, Chris
Tyson, Thumper is my ©opyrighted creation!"*).
**Source** `https://www.adrift.co/files/games/wonderwombat14.zip`.

**Result: WON in 217 commands.** The ending prints

```
THUMPER KICKS ASS!!!

Stay tuned, for the next game in the Thumper series:
Adventures of Thumper 2: Saving General Von Slutt
You scored 0 out of the maximum 0!
That is 100% of the game!
Well done - you scored maximum points!
```

**Artefacts**

| What | Where |
|---|---|
| Route | `goldens/wonderwombat_solution.txt` |
| Transcript | `goldens/wonderwombat_solution.expected.txt` (1,732 lines) |
| Suite row | `harness/run_v4_walkthroughs.sh`, `wonderwombat_solution.txt\|wonderwombat.taf\|THUMPER KICKS ASS!!!\|SCR_SKIP_WAITKEY=1` |

No published walkthrough exists (checked Key & Compass, IF Archive, CASA, IFDB),
the zip ships no solution and the author built no hint menu, so `SCR_DUMP_TASKS`
prints not one `HINT2=`. The route was derived entirely from the structural
dumps.

## The game has no score

There is **not a single `ACT type=4` anywhere in the file**. The declared
MaxScore is 0, so the end-of-game summary reads *"You scored 0 out of the
maximum 0! That is 100% of the game!"* for **any** ending — a loss, a death or
the win. A `You scored …` marker would therefore pass on a corpse, which is why
the row matches the winning cutscene's closing line instead.

That also means "near-maximum score" has no meaning here; the goal is simply to
reach TASK 127.

## Structure

51 rooms, 131 tasks, 76 objects, 39 NPCs, 39 events, 15 variables,
**441 `<waitkey>` tags** (hence `SCR_SKIP_WAITKEY=1` — the standing rule).

The map is one north–south street, Mull Road, with everything hanging off it:
Bobs Boozhouse and the Bath House to the west, Thumper's burnt-out house,
Swear 'O' Rama Stadium and Dark Baron Enterprises to the east, Gunjaville Park
and the graveyard to the north, the alleyway and the Pussycat nightclub to the
south. Two places sit outside it: the **Woop Woop tip**, reachable only by
riding a rubbish dumpster, and **Woop Woop** itself, reachable only by bus once
the weather machine works.

### The four survival meters

`EVENT 0` fires `TASK 1 #statsdown` every five turns:

| Variable | Step | Death |
|---|---|---|
| `bladder` | +1 | TASK 51 at 100 |
| `hygiene` | −1 | TASK 50 `#stink` at 0 **with the gas mask off** |
| `smoke` | +1 | TASK 8 `#craving` at 100 |
| `alcohol` | −1 | — (alcohol ≥ 100 is *Fantasy Land*, not death) |

At one step per five turns every meter death is ~500 turns away, so a normal
route never has to think about smoke at all — which is what makes the game's
cigar/lighter economy (T26 auto-smoke, T49 `light cigar`) dead weight. The
route deliberately never picks up the lighter or the half-smoked cigarette, so
T26 — which fires *automatically* out of the task sweep whenever both are held
and the mask is off, destroying them — never triggers.

Two places do force the issue:

* **The gas mask.** `TASK 38 #stink` sets `hygiene` to **0** outright in the
  dumpster (20), the truck (21) and the tip (22). `TASK 50` then kills you on
  the next sweep unless the mask is worn. Taking the mask out of Thumper's
  house is the first command of the route for a reason.
* **The bladder.** Twenty-plus beers are +5 bladder each, so the binge has to
  be broken in the middle by a trip to the urinal or the bladder death lands
  first.

The spa in the Bath House (`in` / `out`) resets `hygiene`; the route uses it
once for free and once for real, right after the tip.

## Money

| In | | Out | |
|---|---|---|---|
| start | $50 | driving lesson | $100 |
| Wendy's purse | $100 | AK47 ammo | $200 |
| Swear 'O' Rama prize | $5,000 | beer | $5 / mug |
| Steve, for the "cocaine" | $5,000 | bus ticket | $2 |
| Cameron, for the mirror | $26 | | |

Comfortably solvent; nothing in the route is money-gated in practice.

## The five things that are actually hard

**1. The swear-off has to be lost first.** `KARNISHNAR` is written under the
doormat outside the shack in the Woop Woop graveyard — but typing it in round
one gets *"What the hell does Karnishnar mean?"*, because the arena task is not
live yet. Losing a round (any weak insult; the route uses `you idiot`) is what
sends Percy the Possum off to the bar, and the arena only re-opens with him
gone. Round two, `karnishnar`, pays $5,000.

**2. Two different string matchers, one object.** The jar is *"a jar of chronic
fooluffultitus syndrom pills"*.

* `take fooluffultitus pills` → **"Take what?"**, but `take syndrom pills`
  works. The built-in take/drop parser matches the object's own name plus its
  *prefix adjective* words, and `fooluffultitus` is not one of them.
* `give syndrom pills to fry` → **"Give … to who?"**, but
  `give fooluffultitus pills to fry` works. Task command matching runs against
  the **raw input string** (`*fooluffultitus*fry*`), which never consults the
  noun parser at all.

So the same object needs one name to pick up and the other to hand over. Worth
remembering for other games: a refused `take` says nothing about whether the
task command will match.

**3. Fantasy Land is mandatory, and it is turn-parity sensitive.** The **titus
component**, the last part of Matthew's weather machine, exists in exactly one
room: **room 30, Fantasy Land**, which you enter by getting blind drunk
(`TASK 57 #pissed`, alcohol ≥ 100) and leave when the hangover lands
(`TASK 58`, alcohol ≤ 99). Each $5 mug is five `drink beer`s at +5 alcohol
each, and alcohol decays by 1 every five turns, so **which** `drink beer` tips
you over depends on the total turn count of everything before it. Adding or
deleting a single turn anywhere earlier in the route moves the crossing point
and either leaves you sober at `take component` or strands you in Fantasy Land
for a different number of turns. The committed route needs 22 drinks and 8
turns of hangover; both numbers were measured, not reasoned.

This is the one place the walkthrough is genuinely brittle. Anything edited
above `buy beer` must be re-measured below it.

**4. The maze cannot be mapped.** Rooms 32–41 are all called *Maze*, and
`TASK 63` catches `north`/`south`/`east`/`west` (plus `n`/`s`/`e`/`w`) in every
one of them with

```
TASK 63 where=2 room=-1 ... cmd=[north]
    WHERE_ROOMS=[32 33 34 35 36 37 38 39 40 41 ]
    ACT type=1 v1=0 v2=1 v3=0        <- move player to a RANDOM room
```

so a direction is a dice roll, not an edge. The only real exits in the whole
block are `EXIT room=32 S -> dest=31` (back to the pond) and
`EXIT room=41 N -> dest=42` (the Bus Stop). Under the seeded harness the wander
is reproducible, so the route's twelve `n`s are simply the measured number —
they carry no meaning and would be wrong under any other seed.

**5. The endgame is one command.** `kick door` on the Front Porch (room 49)
fires `TASK 123`, and from there the whole showdown — Dazza, Daniel, the UZI,
Womby's handgun, the monster truck ride home — runs as one cutscene that
deposits Thumper back in room 0. `read note` there is `TASK 127`, the win.
`TASK 122` (`open door` / `north`) is the decoy: it has no actions at all.

## Red herrings

Three objects the route picks up do nothing:

| Object | Task | Actions |
|---|---|---|
| mace | T45 `*spray*mace*` | none |
| letter | — | only referenced by a Cameron trade that returns nothing |
| spade | T116 `*dig*` | none |

They are left in the route deliberately: removing them removes three turns, and
three turns is enough to reshuffle the beer block (see #3). Cameron the Cat has
**34** `trade * with cameron` tasks and only three of them hand anything back —
mirror → $26, porno magazine → undead horn, electronic device → annoying fife.
The other 31 are pure flavour.

## Author cheats

Nine unrestricted debug warps survive in the shipped file: `win` (T46, an
immediate `ACT type=6 v1=0` from anywhere), `chunka`, `oozle`, `thoof`, `skam`,
`joxxx`, `glenn`, `rottencop`, `joepoe`. The route uses none of them; they are
noted here only so nobody mistakes one for a puzzle answer while reading the
dump.

## Content

Crude Australian gross-out comedy throughout — swearing, farting, urinating,
drug dealing, a strip-club disguise sequence and a fair amount of casual
misogyny. It is not AIF: there is no sexual content to speak of, and it is
wired like any other corpus game.
