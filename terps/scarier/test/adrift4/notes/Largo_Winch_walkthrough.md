# Largo Winch — walkthrough (**WIN, 97/97 — maximum**)

- **Game:** *Largo Winch* — a French fan adaptation of the Van Hamme/Francq
  comic, in which Largo hunts the hacker who broke into the W Group's network.
- **Engine:** **ADRIFT 3.90** (`xxd -l 16 games/largo-winch.taf` →
  `… c2 cf 94 45 37 61 …`). 309 tasks, 22 `ACT type=6` (EndGame) actions —
  most of them the fights' instant-death branches.
- **Result:** **WIN**, ending on `Congratulations!`, verified in seeded Scarier
  (`goldens/largo_winch_solution.txt`, PASSing golden, win marker
  `Congratulations!`, **no env**).
- **Score: 97 out of 97 — the maximum.** `SCR_DUMP_TASKS` finds **96 `ACT
  type=4` actions summing to exactly 97** (one of them, the finishing punch of
  the first fight, awards 2), and this route fires **all 96**. The score reads
  *"Votre score est 96 sur un maximum de 97"* one command before the end, and
  the final `salle du bigboard` is the 97th point.
- **Source:** `downloaded/LargoWinch_solution.txt`, the author's own published
  command list — 251 lines, expanded and repaired into **323 commands**.
- **The solution file is CP1252, not UTF-8.** Same rule as *Qui a tué Dana?*
  and *Enquête à hauts risques*: `prendre la clé` only parses when the accented
  `é` arrives as a single 0xE9 byte.

## The two conventions in the published list

The author's list is not directly runnable. It uses two shorthands:

* **`commande (N)`** — repeat the command N times. `est (9)` is nine `est`.
* **`commande (prose)`** — a parenthetical stage direction. Every one of these
  is a fight: `combattre (terrasser l'ennemi)`, `combattre (terrasser les deux
  ennemis)`, `combattre (éliminer les trois ennemis)`, `combattre (combat
  final!)`. `combattre` only *opens* the fight; each blow is its own turn, and
  the list never says which blows.

The expander that turns the file into `goldens/largo_winch_solution.txt` lives
in the scratchpad (`largo_gen.py`); its `REPAIRS` list is reproduced in the
harness row's comment block.

## The five fights

Every fight is built the same way: a room per state, one task per (enemy,
blow) pair, and **each enemy answers to exactly one of `coup de poing` /
`coup de pied`**. The wrong blow is usually not a miss but an `ACT type=6` —
instant death. None of the fights award points beyond the single point for
`combattre` itself (and the +2 finisher in fight 1), so the only thing at stake
is survival.

| # | Where | Enemies | Winning line |
| --- | --- | --- | --- |
| 1 | Jack Place's flat | one thug | `coup de poing` ×4 (the 4th scores 2) |
| 2 | Jack Place's courtyard | Boris (baseball bat) + André | `coup de pied boris` ×3 |
| 3 | warehouse roof | three gunmen | `coup de pied ennemi 1` ×2, `… 2` ×2, `… 3` ×1 |
| 4 | hotel room 108 | three knifemen | `coup de pied ennemi 1` ×2, `coup de poing ennemi 2` ×3, `coup de pied ennemi 3` ×3 |
| 5 | Sharon's flat | Helena Dekovar | `coup de pied` ×2 |

Notes on the ones with real choices:

* **Fight 2 cannot be won as the list describes it.** "Terrasser les deux
  ennemis" is impossible: whichever of Boris and André you fell, *the other
  always flees*. Kicking Boris three times is the only line that lands every
  blow and takes no damage in return; punching André also works in three but
  costs a bat hit, and `coup de poing boris` / `coup de pied andré` are both
  fatal. Mixing (two punches at André, then three kicks at Boris) still ends
  with André fleeing, and the score is unchanged either way.
* **Fight 3's enemy 3** carries a club and cannot be punched at all — the first
  punch is fatal. A *single* kick at him cues Simon to grab a chain off the
  floor and finish him, so he costs one turn, not three.
* **Fight 4 offers a one-shot fire extinguisher** (`extincteur`) that blinds
  all three and buys a free double attack. The route leaves it on the wall: the
  three enemies can be killed outright in room 134 (kick 1 ×2 → task 221,
  punch 2 ×3 → task 224, kick 3 ×3 → task 272), and the extinguisher only saves
  the two knife grazes. Its state room 135 is where the author spent most of
  the fight's 45 tasks — six "first blow" flag tasks, each gating six "second
  blow" tasks.

## Four route repairs (published list is wrong or stale)

1. **`ouest` → `nord`** leaving the ground-floor corridor after confronting
   Samantha. The corridor's only exit is north; Jenny is two rooms north, on
   the terrace.
2. **`est` → `nord`** into Sharon's salon (the room with the cushion the CD-ROM
   is under). East is the kitchen.
3. **The Omega basement, three separate errors in five lines.** Giving Olga the
   `devis` already walks Largo down the stairs, so the list's extra `nord` is a
   blocked no-op — the landing has *no exits at all* until the bearded man
   moves. And once he does move, the way back up is **not** `sud` from where he
   sat: the stairs are in a different room, `En bas des escaliers`, reached by
   stepping north into the corridor and back south again. From there `sud`
   climbs to the hall, and the coffee machine is usable **from the hall
   itself** — the `est` its description advertises is not a real exit.
4. **`insérer la bague métallique dans l'armoire électrique` needs the word
   `plate`.** Hammering the ring renames the object: the cabinet accepts only
   `la bague métallique plate`.

## `ouvrir la porte avec le badge` can never work — the game's own synonym eats it

The published list opens both hotel rooms with `ouvrir la porte avec le badge`,
which is one of the alt-commands the author actually wrote on tasks 213 / 214 /
216. It answers **"Open what?"** — SCARE's library fallback, in English, to a
French command.

The cause is the game's own **input synonym `ouvrir` → `open`**, applied to the
typed line *before* task matching. `SCR_TRACE_FLAGS=512` shows it directly:

```
Printfilter: synonym "open la porte avec le badge"
```

So the string that reaches the task matcher starts with `open`, and it matches
neither `ouvrir la porte avec le badge` (the alt-command) nor anything else —
`open *` in the standard library table is the first thing it *does* match,
hence "Open what?".

This is a **game bug, not an engine one**, and the file proves the author knew
about the synonym elsewhere: the window task 15 carries **both** spellings
(`ouvrir la fenêtre avec le badge` *and* `open la fenêtre avec le badge`), and
the wardrobe task carries `ouvrir l'armoire` *and* `open l'armoire` — which is
why `ouvrir l'armoire` works at command 131. Tasks 213/214/216 got only the
`ouvrir …` half, so their whole "open the door" phrasing family is dead.

**`utiliser le badge`** is the same task's primary command, contains no
rewritten word, and is what the route uses (twice).

## The route, by chapter

323 commands. Ranges are indexes into `goldens/largo_winch_solution.txt`.

| # | Chapter | Beats |
| --- | --- | --- |
| 0–33 | The W building party | key under the sofa → open the V.I.P. door; the press badge and the champagne flute; give Samantha the key; the flowerpot trowel, climb over the terrace rail, badge the window open; the slab under the yellow painting; Sharon needs the champagne |
| 34–39 | **Fight 1** | `combattre` then four punches; `entrer` |
| 40–43 | Sharon's flat | CD-ROM under the cushion → into her computer |
| 44–56 | The Maroto labs, ground floor | magazine for the receptionist; ice-breaker → smash the aquarium glass → extinguisher → gas the snakes |
| 57–83 | Labs 1 and 2 | fetch the mixer from lab 2, give it to the blond scientist, back up to Salma Paz; `ouvrir la porte` |
| 84–98 | Jack Place's flat | paper behind the armchair, tape in the wastebasket, test tube in the fridge; combine paper + tube to reveal the password; `taper agylap` |
| 99–102 | **Fight 2** | `combattre`, `coup de pied boris` ×3 |
| 103–113 | Back at the W building | sell the company, talk to John, send Joy to the hospital, Kerensky's CD into the reader |
| 114–147 | Kostenko's building | bluff the neighbour with `kgb`; clean sheet + dirty sheet → rope; tie it to the 3B balcony; pull fuse 3A downstairs |
| 148–172 | Vladimir's flat, with Simon | small key, matchbook, audio tape; mailbox 3A → the envelope with Omega's address; matchbook to Simon |
| 173–188 | Omega | `devis` to Olga; talk the bearded man out of the doorway, take the copper token; the token into the coffee machine draws Olga away; screwdriver from the toolbox; force Vladimir's desk drawer; Kerensky's hacking CD into his computer |
| 189–208 | The Gagarine Paradise | `ulla ulla` to occupy the barman; sleeping pills from the store room; the ladder round the east wall; phone `9002472832`, `frère` / `tousser` / `da` to draw Tania off; empty the tube into her vodka |
| 209–224 | The warehouse | lighter and pistol; burn the dollars; pistol to Simon; shovel → barrel → alcohol bottle + rag → Molotov; throw it at Vladimir |
| 225–231 | **Fight 3** | `combattre`, kicks, `continuer` |
| 232–251 | Sarjevane | invest with John; the painting behind Nerio's photo; hammer + metal ring, flattened; pry the electrical cabinet with the metal arrow; the flat ring into the missing fuse's slot opens the gate; four rooms north to the secret room; `dalle inscription` |
| 252–283 | The W Hospitality hotel | badge from the receptionist; right-hand lift, 2nd floor, nine east to room 208 (Cardiniac's body); 1st floor, nine east to room 108 |
| 284–293 | **Fight 4** | `combattre`, kick/punch/kick, `continuer` — frees Simon and Joy |
| 294–312 | Back to Sharon's | out of the hotel, e-mail, big-board room; `combattre` at Sharon's door |
| 313–314 | **Fight 5** | `coup de pied` ×2 — Helena Dekovar is arrested |
| 315–322 | Epilogue | John, then Joy at the computer centre, then the big-board room → `Congratulations!` |

## Notes for re-running

* **No env is needed.** The transcript is byte-identical with and without
  `SCR_SKIP_WAITKEY=1`.
* **The game ends *on* the last command**, so a trailing `score` is never
  answered. Check the score one command earlier (96/97) and count the final
  `(Your score has increased by 1)`.
* Write the solution file as **CP1252**:

  ```python
  open(dest, 'w', encoding='cp1252').write('\n'.join(cmds) + '\n')
  ```

  and read transcripts back the same way — `grep` treats them as binary and
  silently reports no matches.
