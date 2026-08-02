# Villains and Kings — Walkthrough

A short ADRIFT **3.9** game with the Battle System enabled. Derived under SCARE
(deterministic seed) and cross-checked against the game's internal task, event,
exit and Battle-System data, and against the original ADRIFT Runner
(`run400.exe` / `jasea-0.2t.jar`, source in `decompiled/Battles.bas`).

**Result: 31 / 37 points, and there is no winning ending.** No task anywhere in
the file carries an `ACT type=6` (EndGame), so the game simply stops when you
stop typing; 31/37 is the terminal state. The remaining 6 points are dead in
the file itself: one scoring task can never run in any room, and one is a
duplicate of a task that consumes the same object. Full analysis at the end.
Recorded as `harness/villains_and_kings_solution.txt`.

> **Correction (2026-08-02).** This file previously claimed **13 / 37** as the
> faithful maximum, on the grounds that every character's Accuracy and Agility
> are 0 so the `accuracy > agility` hit gate can never pass, and offered a
> **30 / 37** figure reachable only with the non-faithful `SCR_ASSUME_COMBAT`
> assist. That analysis applied the **4.0** combat rules to a **3.9** file.
> `Villains_And_Kings.taf` carries the V390 signature
> (`… c2 cf 94 45 37 61 …`), so `battle_is_legacy_version()` puts it on the
> `battle_legacy` path, which **skips the accuracy test entirely** — every blow
> lands, and the assassin dies to a single sword stroke with no aid at all.
> The old verdict predates the 3.9 battle-legacy port. The assisted corpus row
> and its two files have been retired; the same mistake had been made for
> [*The Search for Mr. Smith*](The_Search_For_Mr_Smith_walkthrough.md), also
> V390. `SCR_ASSUME_COMBAT` still has a legitimate row in the suite for
> *To Hell and Beyond*, which really is a 4.0 zero-accuracy game.
>
> Two further claims in the old text were wrong for reasons unrelated to
> combat: `open window` **is** scoreable (just not after `push tile`), and the
> scored `take soap` fails because of its `Where` field, not a verb race.

You are "Assface the Third", a not-really-a-detective summoned by King Harry.

---

## Town map

```
  Royal Shower Room (3) -E-> Kings throne room (0) [Harry, Guido, doughnuts]
                                 |  S
                              Hall (1)  [broken window w/ soap; jackass spawns here]
                                 |  S
                            Waiting room (2) [start; Small man has the note]
                                 |  S
                            Courtyard (4) -E- Armory (5) -in- Forge (6)
                                 |  W
                            Giant fountain (7) -N- Outside a BIG house (9) -W- Sculpto's House (8) [radio, dead "guy"]
```

---

## Combat data (all shipped values)

| | Stamina | Strength | Defence | Accuracy | Agility | Speed |
|---|---:|---:|---:|---:|---:|---|
| **Player** | 5 (max 5) | 2 | 1 | 0 | 0 | — |
| **Jackass Trying to Kill You** (NPC 5) | 3 | 1 | 1 | 0 | 0 | 1 (most turns) |

Weapons / armour: Kinda Sharp **Sword** hit 2, Method 1 (`cut`); Highly
Explosive **Grenade** hit 10, Method 5 (`throw`); **Shield** protection 1;
Guido's **Apple** hit 2, Method 5 (held by an NPC, never yours). Sword,
grenade and shield all sit on the Weapon Rack in the Armory (room 5).

Under `battle_legacy` there is no hit roll: **damage = strength + weapon
HitValue − target defence**, every turn, on both sides.

* Bare hands: `2 − 1 = 1` ⇒ three blows to drain the assassin's 3 stamina.
* Sword: `2 + 2 − 1 = 3` ⇒ **one blow kills**.
* Grenade: `2 + 10 − 1 = 11` ⇒ also one blow, thrown
  (`throw grenade at guy`), and the grenade lands on the floor afterwards.
* The assassin back: `1 − 1 = 0` ⇒ "hits you, but it doesn't seem to do any
  damage." The fight is unloseable as well as unlosable-to.

The route below takes the sword. Bare hands work too, at two extra turns.

---

## The full route (31 / 37)

Answer the two setup prompts (name, `male`), press return past the intro, then:

```
take note                  (sneak the note from the Small man's back pocket)
n                          (-> Hall)
open window                (+1; must be done BEFORE `push tile` -- see traps)
n                          (-> Kings throne room)
give note to king          (+1; he tosses you 100 Gold)
s  s  s                    (Hall -> Waiting room -> Courtyard)
e                          (-> Armory)
take sword
wield sword
w                          (-> Courtyard)
n  n  n                    (-> Waiting room -> Hall -> throne room)
w                          (-> Royal Shower Room)
push tile                  (+2; a loose tile - "click" opens the Hall window AND
                            makes the assassin appear in the Hall as an Enemy)
e  s                       (-> throne room -> Hall)
attack guy                 (+2, `jackassdies`; one sword stroke is enough)
search guy                 (+10; yields the GOLDEN SOAP from the corpse)
take golden soap
n                          (-> throne room)
give the golden soap to king   (+5; "I HAVE THE GOLDEN SOAP!")
s                          (-> Hall)
take soap on a rope        (from the now-open broken window)
x window                   (bind the parser's "referenced object" to the broken
                            window, so the next line targets it and not the
                            decorative stained-glass "windows")
close window               (+1)
n                          (-> throne room)
give soap on a rope to king   (+5; the king runs off to shower)
take doughnut              (+3; only possible WHILE the king is in the shower)
s  s  s                    (Hall -> Waiting room -> Courtyard)
w                          (-> Giant fountain)
n                          (-> Outside a BIG house)
w                          (-> Sculpto's House)
use radio                  (+1)
```

Score map: note **+1**, open window **+1**, push tile **+2**, `jackassdies`
**+2**, search guy **+10**, golden soap **+5**, close window **+1**, rope soap
**+5**, doughnut **+3**, radio **+1** = **31 / 37**.

### Traps on this route

* **`open window` must come before `push tile`.** The task requires the broken
  window in state CLOSED(6), which is its starting state. `push tile` moves it
  to OPEN(5), and `close window` moves it to LOCKED(7) — it never returns to
  CLOSED, so once the tile is pushed the +1 is gone forever. (The old writeup
  concluded the task was unreachable because it only ever tried it late.)
* **`x window` before `close window`.** "window" is ambiguous between the
  broken window and the decorative stained-glass "windows"; examining it first
  points the parser at the right one. Typing `close window` with *no* object
  ever referenced used to crash SCARE — see the last section.
* **Golden soap before rope soap.** While you are holding the rope soap,
  `give … golden soap …` matches the *ordinary*-soap task first. Hand over the
  golden soap while it is the only soap you carry.
* **The doughnut is a one-moment window.** `take doughnut` scores +3 only while
  the king is showering; before that, "the king is watching you hover around
  his doughnuts".
* **`attack jackass` does not work.** "jackass" is not one of the NPC's
  handles — they are the full name and the alias "guy". `attack guy` or
  `attack jackass trying to kill you`. This matches the Runner.

---

## The 6 points that are genuinely unreachable

### `take soap` (+1) — `Where = NO_ROOMS`

Task 5 is a scored `take soap`, and its room list is `ROOMLIST_NO_ROOMS` (0):
the task is enabled in **zero** rooms, so `task_can_run_task_directional` can
never let it fire, whatever you type and wherever you stand. You still get the
soap — the library verb handles it — just never the point. (The old writeup
guessed a verb race; the cause is structural.)

### `yes` (+5) — a mutually-exclusive duplicate

There are two **+5** tasks for handing over the rope soap: `give soap to king`
(task 2) and `yes` (task 17, answering the king's "is this for me?" prompt).
Both transfer the single `soap on a rope`, so once one has fired you no longer
hold the soap and the other cannot. Verified live: after giving the soap,
`yes` only prints nag text. Only **+5** of the nominal **+10** is obtainable.

### Tally

31 reachable + 1 (`take soap`, dead by `Where`) + 5 (duplicate soap) = **37**.

---

## A genuine SCARE bug found and fixed: `close window` crash

While deriving this, `close window` typed before any object had been referenced
**crashed SCARE** with `prop_find_child: integer key cannot be negative` and
aborted the interpreter. Cause: an object-state restriction that refers to "the
referenced object" was evaluated when no object was bound (`referenced_object`
is its initial **-1**), and that -1 was passed straight to `prop_get_integer`,
which fatals on a negative key.

The original Runner does **not** crash here, so this was a SCARE robustness
defect. Fixed in `terps/scarier/screstrs.cpp` (`restr_pass_task_object_state`,
the `if (object < 0) return FALSE;` guard) — the restriction simply can't be
satisfied — matching the `referenced_object == -1` guards already used
elsewhere in the codebase. After the fix, `close window` gives a graceful
response, and the intended `x window` → `close window` path scores its +1.
