# To Hell & Beyond — Analysis (UNWINNABLE, faithful max ≈ 23/373)

A large ADRIFT 4.0 adventure (190 rooms, 41 NPCs, 92 tasks, a `greet`/`ask`
conversation system, and the Battle System enabled). Analysed under a headless,
deterministic SCARE build against the game's internal task/exit/event/battle
data, and **ground-truth-verified against the original ADRIFT Runner
(`run400.exe`) on Windows.**

**Result: the game is UNWINNABLE in any faithful interpreter.** It is broken in
*two* independent ways — its combat data and its progression moves — and the
maximum reachable score is **≈ 23 / 373** (you are trapped well before any
ending). This is the author's data, faithful to run400.exe; it is **not** a SCARE
defect.

You play an adventurer who falls into "beyond" (the town of Oran) and is meant to
defeat the tyrant **Xozim** and either **go home** or **claim the throne**.

---

## Breakage #1 — combat can never be won (known)

ADRIFT resolves a hit with **`accuracy > agility`** (strict). The shipped battle
data leaves **every character's Accuracy and Agility at 0**, and no weapon adds
accuracy, so the test is always `0 > 0` = false: no blow by anyone ever lands.
Nothing in the 92 tasks ever raises the player's accuracy, and the only way to
register a kill is in combat (the `^^…dead^^` tasks are NPC *KilledTasks*, and
there are zero "execute task" actions). Both endings require Xozim dead, so even
ignoring everything else, no ending is reachable.

> SCARE's opt-in combat assist (`SCR_ASSUME_COMBAT=1`) can force hits to study the
> intended strength-vs-defence combat, but **it does not make the game winnable**
> — Breakage #2 traps you first.

## Breakage #2 — the progression moves are dead (newly found, run400-verified)

The towns are *not* connected by walkable exits. Every transition between areas
(Oran→Tinev, mansion→ship, ship→shore, shore→…→the finale) is a **task action of
type 1 (move player/NPC)**. Crucially, the mid-game transitions were authored
with the move's **"To:" combo left at VB's default `ListIndex = -1`**, the
destination room sitting unused in `Var3`:

| Task | Intended move | Var2 | Var3 |
|------|---------------|-----:|-----:|
| `jump down` (escape the mansion) | player → Your cabin (ship) | −1 | 42 |
| `out` (ship cabin) | player → hull ledge | −1 | 44 |
| `tie you up again` | player → ship hallway | −1 | 48 |
| `get in` (life-boat) | player → the shore | −1 | 63 |
| `^^discussion^^` (Sulfan) | player → the finale | −1 | 178 |

**The Runner silently ignores a move whose "To" type is unset** (its `Select
Case` has no matching/`Case Else` branch), so these moves do nothing. The author
*also* authored some moves the normal way (`Var2 = 0`, e.g. *ask Ilsar about
Tinev → 14*), which is why the opening works but the mid-game does not.

**Consequence:** after the mansion captures you, `jump down` (and `out`, the only
other exit) are no-ops and the front doors are locked — **you can never leave the
mansion.** Every later area, NPC and scoring task is therefore unreachable.

### Ground truth (run400.exe, Windows)
Driven through the opening to the balcony, `jump down` prints

> *"You jump down to the ground. It hurt a little in the fall but you'll be all
> right."*

…and **leaves the player on the Balconies** — no move to the ship. SCARE
reproduces this byte-for-byte. (Both other reimplementations agree it's
non-standard: SCARE historically `sc_fatal`'d on `Var2=-1`; jAsea throws
*"Unknown MoveNPCAction"*. The faithful behaviour is the run400 no-op, which SCARE
does by default — commit `00daaa24`.)

**Not a 3.9-conversion parse bug.** A scan of the whole corpus shows the
`Var2=-1` move pattern occurs **only in native-4.0 games** (To Hell & Beyond ×10,
X-Files ×2, HYPER Battle System ×1) and in **zero** of the 3.9 games (whose
moves all carry a proper `Var2`). So SCARE is not mis-converting 3.9 data; the `-1` is baked into
these games' shipped 4.0 TAFs (most plausibly from the author upgrading a 3.9
game in ADRIFT's own Generator, which can leave the new "To:" field unset). In
X-Files and HYPER Battle System the unset moves are redundant, so those games
still win; only To Hell & Beyond put them on the critical path.

**Optional repair (opt-in, off by default).** A sibling of the combat assist,
`sc_set_move_assist` (harness env `SCR_ASSUME_MOVES`), honours an unset (`-1`) move
whose `Var3` names a real room as "to room". It is **off by default** (so SCARE
stays faithful to run400.exe) and, when on, lets conversion-broken games be
completed: To Hell & Beyond then wins **248/373**, while X-Files (299/299) and
HYPER Battle System (100/100) are unchanged.

> **Update 2026-07-14:** "X-Files/HYPER Battle System unchanged" was true only
> of their walkthrough transcripts.  Both games carry an unset **NPC** move
> whose `Var3` is a *dangling* index (out of range for the room list — the
> implied offset even differs from this game's, so no single re-basing decodes
> them all): X-Files' diner buzzer should summon Dean, HYPER Battle System's
> `attack` should bring the flare rat to the Attack Menu.  Move assist now
> repairs a dangling (`Var3 > 0`, non-room) unset NPC move by summoning the
> NPC to the player's room — correct in every corpus instance — and both
> X-Files and HYPER Battle System are on the Glk port's auto-assist list
> (os_glk.cpp `GSC_GAME_ASSIST_TABLE`).  To Hell & Beyond's ten unset moves
> all carry in-range `Var3`s and are unaffected by the fallback; all 75
> harness rows still pass.

---

## Maximum reachable score ≈ 23 / 373

Only the Oran/Tinev scoring tasks *before* the mansion trap are obtainable:

| Task | Points | Where |
|------|-------:|-------|
| `use rope with cliff` | +3 | Oran cliff (combine the hook + rope, place on cliff) |
| `give meat package to nimf` | +10 | Tinev tavern (ask Kiren → package → Nimf; +75 gold) |
| `give meat to dog` | +5 | Tinev docks (buy meat 50g, feed the dog) |
| `use crowbar with manhole` | +5 | Tinev docks (crowbar from the mansion forecourt) |

**Total: 23.** Everything beyond — `^^aquired armor^^` +20, `greet Trace` +25,
the three combat kills, `^^xozimisdead^^` +50, `go home` +80, `claim the throne`
+150 — sits past the dead `jump down`/`get in` moves and is unreachable. (The
prior estimate of ~68 assumed the map was "fully traversable"; it is not — the
areas are joined only by the broken move-tasks, not by exits.)

### Route to bank the 23 (deterministic, verified)
```
greet zifan / open door / s            (-> Oran)
... fetch hook (Ilsar's house) + rope (cliff), use hook with rope,
    use rope with cliff                (+3)
... go to the Inn, ask Ilsar about Tinev   (the one working teleport -> Tinev)
ask Kiren about delivery               (meat package)
give meat package to nimf              (+10, +75 gold)   [tavern open 10:00-23:00]
buy meat / give meat to dog            (+5; opens the docks)
take crowbar (mansion forecourt) / use crowbar with manhole  (+5)
```
Do **not** enter the mansion kitchen — that captures you with no escape (faithful
default). The full intended route on through the ship / Mika / Sulfan to a
**248/373 "claim the throne" win** is in
`goldens/to_hell_and_beyond_assisted_solution.txt` (224 commands, deterministic);
it requires **both** assists, `SCR_ASSUME_COMBAT=1 SCR_ASSUME_MOVES=1`.

### 2026-07-14 — assisted route verified and wired as a regression row

The assisted solution **wins, deterministically** (verified 3× byte-identical
on the current scarier binary): 23 faithful points, then +5 reaching the shore,
+10 the scorpion bounty, +10 approaching the ship, +50 `^^xozimisdead^^`, +150
`claim the throne` = **248/373**, ending *"You are now ruler of Beyond.......
Congratulations!"*. It is now a golden-diffed row in
`harness/run_v4_walkthroughs.sh` (env `SCR_ASSUME_COMBAT=1 SCR_ASSUME_MOVES=1`,
marker `You are now ruler of Beyond`), taking the v4 suite to **75/75 PASS**.

**The earlier "it DESYNCS" claim was a replay error, not a route error:** run
with only `SCR_ASSUME_COMBAT=1`, the dead `jump down` move leaves you trapped
in the mansion, the rest of the script whiffs, and the closing `claim the
throne` gets *"I don't understand what you mean!"* — the exact reported
symptom. This game needs **both** assists (reproduced both ways 2026-07-14).

### 2026-08-02 — re-checked against the V390 sweep and the hidden-weapon-accuracy trap; verdict CONFIRMED

Re-audited after two neighbouring games had their combat verdicts overturned
([*Mr. Smith*](The_Search_For_Mr_Smith_walkthrough.md) and
[*Villains & Kings*](Villains_And_Kings_walkthrough.md) turned out to be **V390**
files on the `battle_legacy` path, which skips the accuracy gate; and
[*Jason Vs. Salm*](Jason_Vs_Salm_walkthrough.md) turned out to have a weapon
Accuracy bonus that the `status` screen hides). Neither escape hatch exists
here, so Breakage #1 stands:

- **`To_Hell_And_Beyond.taf` is V400** (`c2 cf 93 45 3e 61`). `battle_legacy`
  does not apply; the 4.0 `accuracy > agility` gate is the right model. (It is
  still an *upgraded* 3.9 game by the fingerprint — all 41 NPCs and the player
  have every attribute range degenerate, `Lo == Hi`, with no exception.)
- **Every one of the 44 objects has an Accuracy bonus of 0** — checked with the
  new `acc=` field in `SCR_DUMP_OBJLOC` (added 2026-08-02 precisely because this
  value is invisible on the `status` screen and is what made the Jason Vs. Salm
  reading wrong). Best weapons are the Megasword (hit 50), claymore (16),
  bladed whip (15), long sword (12); none of them helps you connect.
- **No type-7 action anywhere touches Accuracy or Agility.** All 12 of them are
  `v1=1` stamina heals (four NPC +1000 resets and a player +15), `v1=2`
  max-stamina +3/+3/+5, `v1=4` **Max** Strength +3/+3, and `v1=8` **Max**
  Defence +2/+5. Player base: Stamina 20, Strength 5-5, Accuracy **0-0**,
  Defence 3-3, Agility **0-0**.
- Xozim is **NPC 38**, startRoom 189 (the Large Cave): Stamina 120, Strength 9,
  Defence 8, Accuracy 0, Agility 0, `KilledTask = 85` (`^^xozimisdead^^`, +50).
  Task 85 is the *sole* restriction on both endings — task 86 `go home` (+80)
  and task 87 `claim the throne` (+150), the only two `ACT type=6` actions in
  the file, each gated on `RESTR type=2 v1=86 v2=0` ("task 85 done").

**New finding: the game's own combat "upgrades" are no-ops.** The `v1=4` /
`v1=8` actions above change `battle->max[]`, and `battle->max[]` is read by
exactly one function — `battle_attribute_max()`, which feeds the *Max* column of
the `status` display. Combat rolls come from `lo[]`/`hi[]`
(`battle_eff_strength` / `_accuracy` / `_defence` / `_agility`), which those
actions never touch. So the +3/+3 Strength and +2/+5 Defence rewards move a
number on the status screen and nothing else: the author wanted attribute
indices 3 and 7 (the ranges) and used 4 and 8 (the caps) — the same shape of
slip as Jason Vs. Salm's difficulty tasks buffing the wrong character. Whether
run400 also ignores the caps in combat is unprobed, but it cannot change the
verdict here: no hit lands at any strength.

### 2026-08-02 — the assisted ceiling is 265, not 248 (and 373 is not the maximum)

Asked whether the assists could beat 248, I took the scoring tasks apart. They
can: **265**, wired as a second regression row
(`goldens/to_hell_and_beyond_assisted_max_solution.txt`, same env
`SCR_ASSUME_COMBAT=1 SCR_ASSUME_MOVES=1`, same win marker). But it is an
**exploit row, not an honest maximum** — keep the 248 row as the honest result.
The exact accounting:

```
  373    sum of every ChangeScore in the file
 − 80    task 86 `go home` and task 87 `claim the throne` BOTH carry an ACT
         type=6 (EndGame).  Only one of the two can ever be banked, and the
         throne is worth +150 to `go home`'s +80, so 80 is unbankable.
 ─────
  293    true ceiling
 − 25    task 83 `greet Trace` — structurally unreachable (below)
 −  3    the ^^Days^^ timer, unavoidable cost of the detour (below)
 ─────
  265
```

**The +20: task 72 `^^aquired armor^^`.** This is Theeve's death reward, and
**nothing in the game executes it.** To Hell & Beyond is an upgraded 3.9 file,
and 3.9 has no execute-task action at all (`sctafpar.cpp`'s
`|V390_TASK_ACTION:Type>4?#Type++|` fixup *splices in* 4.0's type-5 slot on
upgrade, so a converted file can never contain one) — every chain in the game
therefore runs through events, NPC walks, or battle `KilledTask`. Theeve is
**NPC 28**, a fully configured hostile (Stamina 50-50, Strength 7-7, Defence
4-4, attitude 2) and the only one in the file left with `killedTask = -1`;
every other hostile has one. So killing him awards nothing, and the task can
only be fired by walking to **room 128** (Theeve's hiding place) and typing the
author's internal task name verbatim:

```
                    (from the Mika entrance, room 112)
e / e / e / s / s / w / s / s / e / s
^^aquired armor^^                        (+20)
n / w / n / n / e / n / n / w / w / in   (back to the Mika entrance)
```

In the wired row this 21-command detour is spliced in after line 144 of the
base assisted solution (the `in` that leaves the player at the Mika entrance).
**run400 has not been probed** for whether it, too, accepts a bare task name as
input — this is a SCARE-side observation only.

**Knock-on: the Mika armor shop is permanently locked.** Tasks 73 `buy studded
leather` and 74 `buy studded pants` each carry `RESTR type=2 v1=73 v2=0`
("task 72 done"). Since nothing executes task 72, the shop can never open —
a *third* independent breakage alongside the zero-accuracy combat and the
`Var2 = -1` moves.

**The −3: the `^^Days^^` timer.** The 20-move round trip costs one decrease of
the day counter. This is not avoidable by trimming elsewhere: cutting the
route's trailing `z` waits from 18 to 4 saves fourteen turns and still wins, but
the penalty fires anyway, and a control of 21 idle `z` at the same point costs
**two** decreases. One −3 is the price of the detour.

**The unreachable +25: task 83 `greet Trace`.** Trace is in **room 166** (the
Tinev tavern back area), but entering that room *is* the progression trigger: an
NPC walk with `charTask = 90` fires task 89 `^^discussion^^`, which teleports
the player straight out on the very turn they arrive. There is no turn in which
the player is standing in room 166 able to type `greet Trace`. (My first attempt
at this route inserted the greeting into the tavern sequence and desynced
everything — the teleport landed the player in Mika before the weapons shop.)

---

## Bottom line

To Hell & Beyond is a substantial, well-built-looking world that its author left
doubly broken: every character's Battle-System Accuracy/Agility is 0 (combat can
never be won) **and** the mid-game progression moves were authored with the
"To-room" combo unset (`Var2 = -1`), which the Runner ignores (verified in
run400.exe). The player is permanently trapped in the old mansion, so **no
ending is reachable and the score tops out at ≈ 23 / 373.** SCARE is faithful to
this by default on both counts. Two opt-in aids (off by default) let you study /
complete the author's intended game: `SCR_ASSUME_COMBAT=1` (forces hits in the
all-zero-accuracy combat) and `SCR_ASSUME_MOVES=1` (honours the unset `Var2=-1`
moves); with both, the game completes at **248 / 373** — or **265** if you also
type the never-executed task name `^^aquired armor^^` in room 128, which is an
exploit rather than play. 373 is not the ceiling: 293 is, because the two
endings that would have to coexist to reach it are mutually exclusive.
