# ADRIFT 4 Battle System — work artifacts

Persistent home for the files behind the SCARE Battle System port
(committed on branch `claudeslop`, commit `bf2b595c`). Moved here out of
`/tmp` so a reboot doesn't wipe them. See also `terps/scare/TODO_battle_system.md`
in the spatterlight repo and the memory notes `scare-battle-system-port`
and `adrift-battle-formulas-re`.

## Layout

- `games/` — 17 ADRIFT 4 `.taf` games whose `BattleSystem` flag is set
  (the regression corpus; pass a path as argv[1] to any harness).
- `decompiled/` — DotFix VB Decompiler output of `run400.exe`. **`Battles.bas`**
  is the combat engine the port was reverse-engineered from; `NewParse.bas`,
  `Express.bas`, `run400.bas` and the forms are the rest of the Runner.
  Original archive: `~/Desktop/Petter.7z`.
- `runner/` — the original ADRIFT 4 Runner/Generator (`run400.exe`,
  `gen400.exe`), the `Manual.pdf` (battle rules pp. 65–71) and its text dump
  `manual.txt`, plus `run400.{ascii,u16}.txt` string dumps. Extracted from
  `~/Downloads/ADRIFT40/ADRIFT.cab`.
- `harness/` — throwaway C test harnesses (build against the SCARE sources):
  - `scbattle_test.c` — drives combat directly (force a kill / enemy attack,
    read output via `pf_get_buffer`).
  - `scprobe.c` / `scscan.c` / `scfind*.c` — dump/scan battle data per game.
  - `scweapon.c` — scan every game's objects for the weapon flag + attack
    *method* (chop/cut/hit/shoot/stab/throw), flagging projectile (shoot/throw)
    weapons, plus enemy/armour counts. One line per game.
  - `scproj_test.c` — arm the player with a game's projectile weapon, co-locate
    an enemy, and resolve a fight through the real `battle_player_attack()`
    pipeline, printing per-round damage.
  - `scproj_regress.sh` — **deterministic projectile-combat regression**: builds
    `scproj_test.c` with `seed.c` (fixed RNG) and diffs five games against
    `scproj_regress.golden`. Covers shoot (method 3, replaces strength) + the
    v3.90 legacy hit model (`deaths`, `Colony`), throw (method 5, adds HitValue)
    + KilledTask (`light_up_4summer_comp`), the v4.0 acc>agi model (`Space Boy's
    First Adventure`), and the faithful v4.0 zero-accuracy stalemate (`cyber2`).
    Run `sh scproj_regress.sh`; re-bless with `--bless`.
- `jasea-0.2t.jar` — jAsea (SCARE's predecessor); confirmed to contain **no**
  combat code, only the `BattleSystem` flag in its TAF readers.

## Build / run

From the spatterlight repo's `terps/scare`:

```sh
# console interpreter (bundled zlib123.zip is absent; link system zlib)
clang -O2 -w sc*.c os_ansi.c -lz -o /tmp/scare_cli
/tmp/scare_cli ~/adrift-battle/games/Sun_Empire_Quest_For_The_Founders.taf

# a harness (links the SCARE objects + test stubs)
clang -O1 -w -I. sc*.c sxstubs.c sxglob.c sxutils.c \
    ~/adrift-battle/harness/scbattle_test.c -lz -o /tmp/scbattle_test
/tmp/scbattle_test ~/adrift-battle/games/Sun_Empire_Quest_For_The_Founders.taf
```
