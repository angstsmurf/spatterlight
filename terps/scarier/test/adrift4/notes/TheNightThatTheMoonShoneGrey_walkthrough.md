# The Night That The Moon Shone Grey (thenightmoon.taf)

Vampire-hunter quest (ADRIFT 3.90), 400 points: thirteen +20 tasks,
"vampire dying" +40 and "stake count valdimir" +100. **WON, 360/400**, which
is also the Runner's ceiling. Wired as
`thenightmoon_solution.txt|thenightmoon.taf|You scored 360 out of the maximum 400!|SCR_RNG=xoshiro`;
every SCR_SEED 1-20 wins at 360.

## The two dead +20 awards

- **Task 12 "wolf's remains":** the Wolf (NPC 8) starts hidden and only
  task 4 "moving wolf to random room" places it, but nothing runs task 4
  (the "Baying at the moon" event has TaskAffected 0; no walk, KilledTask or
  execute-task action names it), so the wolf never appears.
- **Task 17 `behead drow`:** it wants the REFERENCED object held AND the
  dark elf's body in the Library. `behead drow` references nothing (run390x
  "Who?"); `behead dark elf` references the body, which cannot be held and
  lie in the Library at once (run390x "You do not have dead dark elf.";
  Scarier gives the same since 2026-09-26, when 3.9's rule that any
  `%object%` command's name walk binds the turn's reference was ported).
  Scarier used to award task 17: a "referenced object is held" restriction
  with no referenced object fell into the any-object loop. Fixed 2026-09-26
  in `screstrs.cpp` -- it now fails below TAF 4.00, silently at 3.90, per
  run390 passrest (451CDD).

## Route

1. `attack rat with longsword`, `take rat`, then at the smithy `give dead
   giant rat to smith` -- the ONLY phrasing task 23's ALTCMD accepts ("dead
   giant rat" plus "smith"; "adrian" or bare "rat" fall through to the
   library's "Adrian doesn't seem interested...").
2. Detour to the Village wall and `fight skeleton with longsword` (+20).
   attack/kill/hit/stab all match task 5's `%character%` patterns, which
   only change attitude; `fight` goes to the battle system and kills the
   skeleton in one hit.
3. Hermit's hut: `ask hermit about secret entrance`, `open secret door`.
   Library: `search bookcase` spawns the dark elf, `attack elf with
   longsword` x2, `move rug` opens the Prison trapdoor.
4. Tower room: `examine table` before `take healing powder`. Coffin room:
   `open box`, `look in box`, `examine man`, `take key` -- each reveal is
   needed before the next noun resolves.
5. Back in the Library, `take potion`, `drink healing potion`, then `d` to
   the Prison.
6. The skeleton guard: the bare alias "skeleton" resolves to the other,
   absent Skeleton, so it takes `attack skeleton guard with longsword` (two
   hits). The guard used to be unkillable; that was two Scarier bugs, both
   ported from the run390 decompile:
   - run390 checktask's `%character%` arm (44AD48-44ADC2) walks every NPC
     with no break: the last hit is stored as the reference, but the
     command is re-spelled on the FIRST hit only (the 3.9 twin of the
     pre-4.0 `%object%` substitution veto). "Guard", the guard's first
     Name, is what the line gets spelled with, so TASK 11's "attack guard"
     matches and the fight counts.
   - run390 killchar (42D344-42D40C) overwrites the command line with the
     KilledTask's Command(0) and runs tasks(1); dobattle's no-break target
     loop (44CC1C-44D1D5) then tests every LATER NPC against that line, so
     on the second `attack elf` the dark elf's KilledTask "drow giving in"
     makes the loop strike the Drow too.
7. `look` before `take keys` (dropped objects are unseen until listed),
   `open cell` (+20, frees Astrania), take the bastard sword (HitValue 25
   against the longsword's 15).
8. `astrania follow me` so task 29's "Astrania not alone" holds, `unlock
   door with key`, `in`, `who are you` (+20, fight moves to the Rooftop
   ledge), four `attack vampire with bastard sword` (the fourth runs
   "vampire dying", +40), then `stake count valdimir` (+100) before
   "Vampire recovering" (2-3 turns) finishes.

The exact commands are in `goldens/thenightmoon_solution.txt`.

## Runner check

run390x (`runner_transcripts/thenightmoon.txt`) wins at 360. The differences
are all deliberate or cosmetic:

- "a giant rat is here" is lowercase in the Runner; Scarier capitalises it.
- One whitespace-only turn.
- **Deliberate deviation:** on the four `attack vampire with bastard sword`
  turns run390's profanity arm (45F8E4, "bastard" anywhere in the line)
  prints "I really don't think there's any need for language like that!"
  ahead of the hit. Scarier doesn't port it; the hits land identically.

SCR_RNG=xoshiro required for determinism.
