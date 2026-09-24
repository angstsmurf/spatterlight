# The Night That The Moon Shone Grey (thenightmoon.taf)

Vampire-hunter quest, ~400 points. **Best-reachable, unwinnable verdict** —
walkthrough ends on the death screen at the Skeleton guard fight in the
Prison, having scored 120/400 (30%).

## Route to the blocking point

1. `attack rat with longsword`, `take rat` — kill the Giant rat, take the
   corpse.
2. Navigate to the smithy and `give dead giant rat to smith` — this is the
   ONLY working phrasing for the reward task (obtains the silver stake).
   Confirmed via `SCR_TRACE_TASKS=1`: the task's ALTCMD requires the
   literal two-adjective phrase "dead giant rat" together with "smith", not
   "adrian" and not "rat" alone — every other combination silently falls
   through to the generic library fallback ("Adrian doesn't seem
   interested...") without the task ever being attempted.
3. Navigate to the hermit's hut, `ask hermit about secret entrance`
   (teleports to the Fields), `open secret door` (reaches the Guard room).
4. In the Library: `search bookcase` spawns a Dark elf; `attack elf with
   longsword` x2 kills it (scripted 2-hit kill). `move rug` opens the
   Prison trapdoor.
5. Detour via Landing/Upper hallway/Flight of stairs to the Tower room:
   `take healing powder` fails ("Take what?") until `examine table` is
   issued first — the object is not in scope until the table is examined.
6. Continue to the Coffin room: `open box`, then `look in box` (reveals "A
   dead man is inside the brown box"), then `examine man` (reveals "A
   small key is inside the dead man"), then `take key`. Each reveal step
   is required in sequence before the next noun becomes resolvable.
7. Return to the Library: `take potion` / `drink healing potion`. The
   potion the Dark elf dropped is NOT takeable immediately after the kill
   ("Take what?") — it only becomes scope-visible after a substantial
   number of intervening turns (a delayed EVENT reveal, not an immediate
   scope change), which is why the route detours to the Tower/Coffin rooms
   before doubling back for it.
8. Descend to the Prison — the Skeleton guard attacks unprovoked on the
   very first turn after entry.

## The Skeleton guard fight — unwinnable

- The bare alias `attack skeleton with longsword` resolves to the wrong,
  absent NPC (a different "Skeleton" elsewhere in the game) and fails
  ("You are not in the same place as them!"). The fuller name `attack
  skeleton guard with longsword` is required just to target the right NPC.
- Even with that fix, the fight cannot be won by any means tried:
  - Full stamina via the healing potion beforehand, plus a second full
    heal via `eat healing powder` mid-fight — survived to 22 attacks, died
    on the 23rd (confirmed reproducibly via bisection under fixed
    `SCR_RNG=xoshiro`).
  - `SCR_ASSUME_COMBAT=1` combat assist — no improvement.
  - Taking the village-gate armour/shield first — both are actually
    unobtainable despite being listed as "Dynamic" objects in the `rooms
    *` debugger dump (`take armour`/`take shield` both fail with "Take
    what?"); they appear to belong to/be held by the Guard NPC, never
    freely lying in the room.
  - Bypassing the fight (typing `take keys` without attacking) — the NPC
    is hostile regardless and kills the player within ~2 turns.
- Root cause (via `SCR_TRACE_TASKS=1`): `attack %character% with
  %object%` (TASK 5) is one single generic library task shared by every
  enemy in the game, and only ever performs `ACT type=7 v1=0 v2=0 v3=2`
  ("changing battle attribute 0 of NPC by/to 2") per hit. TASK 11
  ("skeleton's death", a silent trigger with the crumble-to-dust ending
  text) never once appears in the trace log, regardless of attack count
  (tested up to 30 attacks) — its trigger condition is never satisfied by
  this generic attack task, so the Skeleton guard has no reachable death
  condition via ordinary combat.
- Diagnostic note: issuing `status` mid-fight (SCARE's actual battle-status
  command; `stats`/`statusline` are not recognized) appears to consume a
  "free" turn for the hostile NPC — removing all `status` calls from the
  script measurably improved survival, so the final golden script contains
  none.

## Unreached content

Everything beyond the Skeleton guard (the vampire fight — Tasks 31–34,
"who are you" / "vampire dying" / "stake count valdimir" — and the win
condition) was mapped from the static TASK dump but never reached or
tested live, since the Skeleton guard blocks all further progress.

SCR_RNG=xoshiro required for determinism.
