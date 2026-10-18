# Rise of Death solver

Tools that derived `overrides/Rise of Death.cmd` (the full win, 4360 commands).

Two generations:

- Commands 1-2778 (stages 1-3 and the cave bosses, up to `KillCaveBoss`): `qlive.py`
  (qvh live session), `bot.py`, `solve3.py`, `boss2.py`, `probe.py`.
- Commands 2779-4360 (the elemental zones, soul tiers 7-9, Al'Akir, the story rooms to the
  Finale): `rod.py` over the shared live driver `../goldenegg/live.py` (`GE_GAME` picks
  the game; `GE_DRIVER=native` plays on `aslx_replay`, about 6x faster). The result was
  confirmed on qvh before it was frozen.
  - `rod.py`: zone map, fight move choice (Haunt healers to armor 4 before lashing),
    zone farming that alternates elements, upgrade buying (heals with `d` before
    leaving: MaxHealth upgrades don't raise Health).
  - `rserve.py`: keep one Bot alive and run snippets against it:
    `GE_DRIVER=native python3 rserve.py 47320 prefix.cmd`, then
    `python3 ../goldenegg/ask.py 47320 'b.status()'` (or `-f al.py`).
  - `al.py`: walk to Al'Akir, bind R to Extinguish Life, and win the fight.

The route and the game mechanics it relies on are in the Rise of Death row of
`overrides/README.md`. To resume from a prefix, take the first N lines of the override
(after the `#` header) as the rserve.py script.
