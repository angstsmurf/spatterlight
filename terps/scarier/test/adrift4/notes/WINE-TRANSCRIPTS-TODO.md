# TODO: Runner-transcript verification of the pre-4.0 walkthroughs

The *Professor Von Witt* exercise, generalised. That game's author walkthrough
was replayed command-for-command in the real Windows Runner under Wine, the
Runner's own transcript was diffed against Scarier's, and three engine bugs
fell out of the diff (bare `pick` take-synonyms, room-alt `Var2` being a
1-based **global** object number, and the 1-2 vs 3+ surface-listing split) —
plus one non-bug that cost a session (see *Verbose* below). The write-up is
the comment block above the `professor_solution.txt` row in
`harness/run_v4_walkthroughs.sh`; the fixes are commit `6b61f2ab`.

Nothing in this file is done yet. It is the candidate list and the recipe.

**Scope: 3.90, 3.80 and 3.70 games only.** The 4.00 pool (124 further
seed-invariant rows) is deliberately not listed here — it is being worked
through separately.

## Why these games and not others

A walkthrough can only be diffed turn-for-turn against the Runner if the game
is *deterministic along that path*. The test, run 2026-08-23 over the whole
v4 harness:

    export SCR_SEED=97      && harness/run_v4_walkthroughs.sh
    export SCR_SEED=424242  && harness/run_v4_walkthroughs.sh

A row that still PASSes its seed-1 golden under **both** alternate seeds has
no visible randomness anywhere on its walkthrough path. One alternate seed is
not enough: the second seed killed nine rows that had survived the first by
luck. Result:

| | rows |
|---|---:|
| wired v4 walkthrough rows | 303 |
| carrying their own `SCR_SEED` / `SCR_ASSUME_COMBAT` (excluded) | 16 |
| seed-invariant under both seeds | 190 |
| — of those, 4.00 (out of scope here) | 124 |
| — **of those, pre-4.0 (this file)** | **66** |

The 16 excluded rows are excluded because `$ROW_ENV` is applied *after* `env`
in `transcript()`, so a row that pins its own seed overrides the ambient
export and its PASS proves nothing.

Two caveats on the test itself:

- Seed-invariance is measured **in Scarier**. A randomness feature Scarier
  does not implement at all would be invisible to it. The Runner is the
  oracle, not the harness.
- It measures the *walkthrough path*, not the game. A game can be full of
  random events that the walkthrough happens never to trigger.

## Which Runner to launch

Match the Runner to the file's version — 3.9 and 4.0 differ in real
semantics (event task dispatch, take wording, division rounding). The version
is in the .taf header, bytes 8-10, the XOR-obfuscated version characters:

| bytes 8-10 | version | Runner |
|---|---|---|
| `93 45 3E` | 4.00 | `run400.exe` |
| `94 45 37` | 3.90 | `run390.exe` |
| `94 45 36` | 3.80 | `run380.exe` |
| `94 45 39` | 3.70 | `run370.exe` |

Do **not** use "is there a zlib `78 9c` at offset 22" as the test — that is
true only of 4.00 and silently classifies every older file as unknown/4.0
depending on which way the check is written. All four Runners live in
`~/adrift-battle/runner/wine/pfx/drive_c/adrift/`.

## Capturing a transcript

The Adventure menu splits at 3.90/3.80, and this was measured live, not
assumed:

**run390 (and run400) — "Start Transcript".** Live streaming to
`C:\adrift\Adrift_<N>.txt`, plain text, opened *before* play. Use the
existing helper unchanged, passing the Runner as `$3`:

    cd ~/adrift-battle/runner/wine
    sh runner_transcript.sh <game.taf> <cmdfile> run390.exe

**run380 and run370 — "Save Transcript".** No live transcript at all: the
menu item dumps the whole scrollback *at the moment you click it* to
`C:\adrift\Adven_<N>.rtf` and pops a "Transcript saved" MsgBox (no Save-As
prompt; dismissed with key code 36). So the order is reversed — play first,
save last:

    sh runner_savetranscript.sh <game.taf> <cmdfile> run380.exe
    textutil -convert txt -stdout pfx/drive_c/adrift/Adven_1.rtf

The output is RTF with a colour table, which is a bonus: it preserves the
bold and the colours, so room headings and author styling survive.
`runner_savetranscript.sh` also carries run370's different click point — that
Runner's window is a fixed 559x498 and its entry field sits at window-relative
280,452, where the maximised 3.80/4.0 layout puts it at screen 400,825.

⚠️ `runner_transcript.sh` ends with `ls -t pfx/drive_c/adrift/Adrift_*.txt |
head -1`. Point it at a 3.80/3.70 game and it will happily print the path of
somebody else's hours-old transcript instead of failing — that is how the
"run370 can't save transcripts" wrong conclusion got made. Use
`runner_savetranscript.sh` for those two.

Unverified: whether the scrollback dump is capped for a long session. `cave`
(216 commands) is the one to check that on before trusting a 3.80 diff.

## Before measuring anything

- **Turn Verbose ON** (Options → Verbose, Ctrl+V). It resets to OFF on every
  launch and never persists. With it OFF, re-entering a visited room prints
  only `RoomName.` and NPC walker lines are *absent entirely*. Scarier models
  the Verbose-ON Runner, and author transcripts are Verbose-ON sessions.
  Measured in run400; assumed but **not yet verified** for run390/380/370 —
  check the Options menu on the first game of each version.
- **Tick the Appearance checkboxes.** All five default OFF and never persist.
  "Room names in descriptions" off means no room headings at all; "References
  in brackets" governs the `g` echo.
- **Fresh process per measurement.** Adventure → Restart game does not
  reliably reset NPC walk state.
- Feed with `drive_ckpt.sh`, which echo-verifies each line. Wine mangles
  input; read the echo, never assume the line landed.
- Rows marked **waitkey** below contain a "press any key" pause that swallows
  one fed keystroke. They are usable but need the feeder to account for it —
  prefer a non-waitkey game first.
- Real-time pauses (the Professor pie) only manifest live. They are not a
  divergence.
- Kill Wine afterwards, properly:
  `pkill -9 -f wine; pkill -f wineserver; pkill -9 -f 'C:\\'; pkill -9 -f 'start\.exe /exec'`
  then verify with `ps aux | grep -iE 'wine|\.exe'`. `pkill -f wine` alone
  matches nothing — Wine's Windows processes carry Windows command lines.

## Candidates

Sorted by NPC **walk** count first, then by length. Walks are the payload:
every Professor-class divergence found so far lived in walk phase, walk
arrival announcements, or walker presence lines. `walks`/`NPCs`/`events` come
from `SCR_DUMP_TASKS=1 harness/scare <game>`. `cmds` is the walkthrough
length. Solution files are `goldens/<solution>_solution.txt`.

### 3.90 — 54 games

| game | solution | cmds | walks | NPCs | events | waitkey | notes |
|---|---|---:|---:|---:|---:|---|---|
| `Merry_Murders.taf` | `merry_murders` | 181 | 8 | 8 | 2 | yes | [Merry_Murders_walkthrough](Merry_Murders_walkthrough.md) |
| `Vampire.taf` | `vampire` | 205 | 7 | 11 | 11 | yes | [LairOfTheVampire_walkthrough](LairOfTheVampire_walkthrough.md) |
| `gamma.taf` | `gamma` | 315 | 4 | 10 | 0 | -- | -- |
| `S_Tar_Dus.taf` | `stardust` | 199 | 4 | 6 | 0 | -- | [S_Tar_Dus_T_walkthrough](S_Tar_Dus_T_walkthrough.md) |
| `wingman1.taf` | `wingman1` | 33 | 3 | 3 | 0 | -- | -- |
| `tcom.taf` | `tcom` | 13 | 3 | 1 | 0 | -- | [tcom_walkthrough](tcom_walkthrough.md) |
| `windy2.taf` | `windy2` | 200 | 2 | 8 | 1 | -- | -- |
| `Richard.taf` | `richard` | 189 | 2 | 5 | 13 | yes | [WhereIsRichard_walkthrough](WhereIsRichard_walkthrough.md) |
| `cleft.taf` | `cleft` | 115 | 1 | 2 | 1 | -- | [The_Cleft_in_the_Rock_walkthrough](The_Cleft_in_the_Rock_walkthrough.md) |
| `ECOD3.taf` | `ecod3` | 26 | 1 | 1 | 0 | -- | [ECOD3_walkthrough](ECOD3_walkthrough.md) |
| `BobBobsly.taf` | `bob_bobsly` | 25 | 1 | 2 | 0 | -- | [Bob_Bobsly_walkthrough](Bob_Bobsly_walkthrough.md) |
| `largo-winch.taf` | `largo_winch` | 323 | 0 | 42 | 22 | -- | [Largo_Winch_walkthrough](Largo_Winch_walkthrough.md) |
| `mudergreatfalls.taf` | `murder_great_falls` | 255 | 0 | 0 | 0 | yes | [Murder_in_Great_Falls_walkthrough](Murder_in_Great_Falls_walkthrough.md) |
| `report.taf` | `report` | 254 | 0 | 0 | 0 | -- | [Report_Espionage_walkthrough](Report_Espionage_walkthrough.md) |
| `Archie's Birthday V 1-2.taf` | `archie` | 240 | 0 | 8 | 0 | yes | [Archies_Birthday_walkthrough](Archies_Birthday_walkthrough.md) |
| `croft.taf` | `croft` | 193 | 0 | 4 | 1 | -- | -- |
| `DarkTower.taf` | `darktower` | 174 | 0 | 0 | 0 | -- | [The_Dark_Tower_walkthrough](The_Dark_Tower_walkthrough.md) |
| `FarFromHome.taf` | `farfromhome` | 167 | 0 | 0 | 0 | yes | [Far_From_Home_walkthrough](Far_From_Home_walkthrough.md) |
| `EnqueteAHautsRisques.taf` | `enquete_a_hauts_risques` | 145 | 0 | 13 | 7 | -- | -- |
| `Captive.taf` | `captive` | 141 | 0 | 2 | 19 | -- | [Captive_Universe_walkthrough](Captive_Universe_walkthrough.md) |
| `The Screen Savers On Planet X.taf` | `screen_savers` | 133 | 0 | 10 | 19 | -- | [The_Screen_Savers_On_Planet_X_walkthrough](The_Screen_Savers_On_Planet_X_walkthrough.md) |
| `thewoods.taf` | `thewoods` | 133 | 0 | 0 | 0 | yes | [The_Woods_Are_Dark_walkthrough](The_Woods_Are_Dark_walkthrough.md) |
| `Chosen.taf` | `chosen` | 123 | 0 | 0 | 0 | yes | [Chosen_walkthrough](Chosen_walkthrough.md) |
| `Renuntio.taf` | `renuntio` | 118 | 0 | 0 | 3 | yes | [Renuntio_walkthrough](Renuntio_walkthrough.md) |
| `as.taf` | `asylum` | 102 | 0 | 1 | 0 | yes | [Asylum_walkthrough](Asylum_walkthrough.md) |
| `A_Morning_with_a_Headache.taf` | `morning_headache` | 88 | 0 | 3 | 8 | -- | [A_Morning_with_a_Headache_walkthrough](A_Morning_with_a_Headache_walkthrough.md) |
| `sleaze.taf` | `sleaze` | 86 | 0 | 0 | 0 | -- | [Sleaze_City_walkthrough](Sleaze_City_walkthrough.md) |
| `Wheel105.taf` | `wheels_must_turn` | 77 | 0 | 4 | 15 | yes | [The_Wheels_Must_Turn_walkthrough](The_Wheels_Must_Turn_walkthrough.md) |
| `tq3.taf` | `tq3` | 76 | 0 | 2 | 4 | -- | [The_Quest_Moody_walkthrough](The_Quest_Moody_walkthrough.md) |
| `mhpquest.taf` | `mhpquest` | 68 | 0 | 2 | 0 | -- | [MHP_Quest_walkthrough](MHP_Quest_walkthrough.md) |
| `everything.taf` | `everything` | 68 | 0 | 0 | 0 | yes | [Everything_Emanuelle_walkthrough](Everything_Emanuelle_walkthrough.md) |
| `ECOD2.taf` | `ecod2` | 61 | 0 | 0 | 0 | yes | [ECOD2_walkthrough](ECOD2_walkthrough.md) |
| `chicago.taf` | `chicago` | 60 | 0 | 3 | 0 | -- | [Chicago_walkthrough](Chicago_walkthrough.md) |
| `hangover.taf` | `the_hangover` | 56 | 0 | 16 | 0 | -- | -- |
| `veteran.taf` | `veteran` | 47 | 0 | 3 | 0 | -- | [Veteran_Experience_walkthrough](Veteran_Experience_walkthrough.md) |
| `lostsouls.taf` | `lost_souls` | 47 | 0 | 0 | 0 | -- | [Lost_Souls_walkthrough](Lost_Souls_walkthrough.md) |
| `CRM.taf` | `crm` | 46 | 0 | 0 | 0 | -- | [That_Crazy_Radioactive_Monkey_walkthrough](That_Crazy_Radioactive_Monkey_walkthrough.md) |
| `Villains_And_Kings.taf` | `villains_and_kings` | 44 | 0 | 0 | 0 | -- | [Villains_And_Kings_walkthrough](Villains_And_Kings_walkthrough.md) |
| `DFU.taf` | `dfu` | 44 | 0 | 1 | 0 | -- | [Dance_Fever_USA_walkthrough](Dance_Fever_USA_walkthrough.md) |
| `impulso.taf` | `impulso` | 43 | 0 | 0 | 0 | -- | [Impulso_walkthrough](Impulso_walkthrough.md) |
| `Colony.taf` | `colony` | 40 | 0 | 3 | 3 | -- | [Colony_walkthrough](Colony_walkthrough.md) |
| `LOST.TAF` | `lost` | 38 | 0 | 3 | 11 | yes | [Albert_is_Lost_walkthrough](Albert_is_Lost_walkthrough.md) |
| `LOST.TAF` | `lost_down` | 38 | 0 | 3 | 11 | yes | -- |
| `amonkeytoomany.taf` | `amonkeytoomany` | 34 | 0 | 1 | 0 | -- | [A_Monkey_Too_Many_walkthrough](A_Monkey_Too_Many_walkthrough.md) |
| `Phoenix_Destiny.taf` | `phoenix_destiny` | 33 | 0 | 0 | 0 | -- | [Phoenix_Destiny_walkthrough](Phoenix_Destiny_walkthrough.md) |
| `CAH.taf` | `cruel` | 30 | 0 | 0 | 0 | -- | [Cruel_and_Hilarious_Punishment_walkthrough](Cruel_and_Hilarious_Punishment_walkthrough.md) |
| `forest.taf` | `forest_on_the_norm` | 27 | 0 | 4 | 0 | -- | [Forest_On_The_Norm_walkthrough](Forest_On_The_Norm_walkthrough.md) |
| `Locked_door_with_water_trap.taf` | `locked_door` | 21 | 0 | 0 | 0 | yes | -- |
| `Theannihilationofthink2.taf` | `think2` | 19 | 0 | 0 | 0 | -- | [Theannihilationofthink2_walkthrough](Theannihilationofthink2_walkthrough.md) |
| `lifesimulation.taf` | `lifesimulation` | 19 | 0 | 0 | 0 | -- | [lifesimulation_walkthrough](lifesimulation_walkthrough.md) |
| `Insane.taf` | `escape_from_insanity` | 16 | 0 | 0 | 0 | -- | [Escape_from_Insanity_walkthrough](Escape_from_Insanity_walkthrough.md) |
| `Toxically_Earth.taf` | `toxically_earth` | 11 | 0 | 17 | 0 | -- | [Toxically_Earth_walkthrough](Toxically_Earth_walkthrough.md) |
| `Dreams.taf` | `dreamland` | 10 | 0 | 0 | 1 | -- | [Dreamland_walkthrough](Dreamland_walkthrough.md) |
| `Matt's House.taf` | `matts_house` | 8 | 0 | 5 | 0 | -- | [Matts_House_walkthrough](Matts_House_walkthrough.md) |

### 3.80 — 10 games

| game | solution | cmds | walks | NPCs | events | waitkey | notes |
|---|---|---:|---:|---:|---:|---|---|
| `haunt.taf` | `haunt` | 88 | 5 | 4 | 3 | -- | -- |
| `superliam.taf` | `super_liam` | 86 | 5 | 11 | 0 | -- | -- |
| `cave.taf` | `cave` | 216 | 2 | 5 | 12 | -- | -- |
| `akron.taf` | `akron` | 44 | 2 | 4 | 0 | -- | -- |
| `jb2000.taf` | `james_bond` | 20 | 1 | 1 | 0 | -- | -- |
| `haunted.taf` | `haunted_house` | 116 | 0 | 0 | 2 | -- | -- |
| `Crime_Adventure.taf` | `crime_adventure` | 90 | 0 | 2 | 3 | -- | [Crime_Adventure_walkthrough](Crime_Adventure_walkthrough.md) |
| `first.taf` | `fistandantalus` | 18 | 0 | 1 | 0 | -- | -- |
| `duck.taf` | `duck_mccloud` | 13 | 0 | 0 | 0 | -- | -- |
| `microwaveman.taf` | `microwave_man` | 9 | 0 | 1 | 1 | -- | -- |

### 3.70 — 2 games

| game | solution | cmds | walks | NPCs | events | waitkey | notes |
|---|---|---:|---:|---:|---:|---|---|
| `arlo.taf` | `alices_restaurant` | 85 | 11 | 9 | 9 | -- | [ADRIFT_370](ADRIFT_370.md) |
| `castle.taf` | `castle_quest` | 17 | 0 | 1 | 0 | -- | [ADRIFT_370](ADRIFT_370.md) |

`arlo.taf` is the single best target in this whole file: 11 walks in 85
commands, and 3.70 is the least-exercised parse schema in the engine.

## What to do with a diff

Same discipline as Professor:

1. Rule out Verbose and the Appearance checkboxes first. Two of the three
   apparent Professor divergences were display settings.
2. Rule out the feed. Scroll the Runner to the session top and confirm the
   commands actually landed in order before believing an output difference.
3. Only then treat it as an engine bug — fix the engine, never the
   walkthrough, re-bless the golden, and record the measurement that
   justifies any deliberate deviation in the row's comment block in
   `harness/run_v4_walkthroughs.sh`.
