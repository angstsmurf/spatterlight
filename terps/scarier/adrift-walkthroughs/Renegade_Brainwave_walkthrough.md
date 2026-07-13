# Renegade Brainwave — J. J. Guest (Ectocomp 2010) — ADRIFT 4

**Status: ★ WON — wired into `run_v4_walkthroughs.sh` (PASS).**

* Game: `Adrift games/Ectocomp_2010/Renegade_Brainwave.taf` (symlinked as
  `games/Renegade_Brainwave.taf`)
* Solution: `harness/renegade_brainwave_solution.txt` (23 commands, no waitkeys)
* Golden: `renegade_brainwave_solution.expected.txt`
* Win marker: `planet Earth has been averted!`

Third of the "port-based" games from `TODO_plover_walkthroughs.md` §5.

## The Key & Compass walkthrough is for a *different, bigger game*

Guest's Inform 7 release says so outright in its ABOUT text:

> "Renegade Brainwave was first written in three hours for ECTOCOMP 2010. **This
> Glulx version is an expansion of the original idea.**"

So the K&C solution is not a lossy transcription of the ADRIFT game — it solves
content that does not exist here. The ADRIFT original is a 3-hour comp entry:
**9 rooms, 25 tasks, 11 objects**. Almost none of the Inform puzzle chain
(alligator + angora sweater, LOBO, Mandoo's turban, the doughnut economy,
Donald burning the swamp gas, the pie, the mausoleum/sarcophagus pry, `squeeze
bone`, `shoot dog`) is in it. Even the villain is renamed: the ADRIFT puppy is
the **Dread Domingo**, a chihuahua; in Inform she is the Dread Devora, a husky
mix.

Deriving from the `.taf` structure (`SCR_DUMP_TASKS=1`) rather than the
walkthrough was much faster than patching the port script.

## The real chain (ADRIFT)

The whole game is: get the gun, trade it for a cigar, trade the cigar for the
vault, take the bone, and throw the bone through the interdimensional curtain.
The chihuahua chases it and is flattened into the second dimension.

1. **`open handbag` → `drop handbag` → `take gun`.** The gun is in the bag, but
   so is a seventeen-foot boa constrictor. `EVENT 0` kills you **exactly one
   turn** after the bag is opened, so `drop handbag` must be the very next
   command — the snake leaves with the bag, and the gun stays behind in it on
   the ground, free to take.
2. **`west` → Yew tree.** Arriving here spawns the gorilla (who wanders off to
   the Cemetary) and reveals the **crowbar**. Take it.
3. **Cemetary (NW corner) with the gun in hand.** No need to `shoot gorilla` —
   TASK 23 fires on *arrival while carrying the gun*: the gorilla throws up his
   arms and offers a cigar. `take cigar`.
4. **`give cigar to dummy`** at the Vault of Mandoo — the ventriloquist's dummy
   smokes it and is destroyed (nothing left but his cuban-heeled shoes),
   unlocking the vault. `in`, `take bone` (the sarcophagus is already open —
   `open sarcophagus` fails with "You can't open the stone sarcophagus!").
5. **`open chapel`** (needs the crowbar) → `in` → `down` → `in` (space module).
6. **`throw bone at curtain`** → win.

## Traps that are *not* on the win path

* **The astronaut's helmet.** `take helmet` (TASK 19) starts `EVENT 1
  [Domingo Kills Player]`, which kills you 3 turns later; and TASK 20 makes
  `out` fatal once the helmet is off. The helmet is pure bait — the solution
  never touches it. (In the Inform port the helmet is *required*; here it only
  kills you.)
* `smoke cigar` (TASK 6), `east`/`enter curtain` in the module (TASK 17), and
  `take snake` (TASK 11) are all instant deaths.
