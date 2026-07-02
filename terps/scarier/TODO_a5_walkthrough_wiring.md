# TODO: wire remaining walkthroughs into the a5 regression harness

`test/run_a5_walkthroughs.sh` only exercises a game once it has (1) a game file
in `test/adrift5-games/`, (2) a turn-by-turn command script
`test/<Name>_walkthrough.txt`, and (3) a `name|game|vbudget|xbudget` line in the
script's `MAP`. `test/adrift5-games/walkthroughs/` already has raw
walkthrough/hint material for several games whose game files are staged but
that never got a command script or a MAP line. This is the backlog.

## Ready to wire (script + game file already staged — just needs a MAP line)

- ~~**PathwayToDestruction**~~ ✅ **WIRED (2026-07-02).** Full MATCH in both RNG
  modes (`0|0` MAP line, golden `test/PathwayToDestruction_expected.txt`). Surfaced
  and fixed the `<cls>` commit-boundary bug — see the top entry in
  `TODO_a5_walkthrough_bugs.md` ("Pathway to Destruction: `<cls>` … DONE").

## Have a full walkthrough, need conversion to a command script

- **CallOfTheShaman** → `TheCallOfTheShaman.blorb` — source:
  `walkthroughs/CallOfTheShaman_walkthrough.txt`
- **LostCoastlines** → `Lost_Coastlines.taf` — source:
  `walkthroughs/LostCoastlines_walkthrough.pdf` (8.6 MB; will need PDF text
  extraction, same as Anno1700/GrandpasRanch/TreasureHuntInTheAmazon were)
- **Skybreak** → `Skybreak.taf` — source: `walkthroughs/Skybreak_walkthrough.pdf`

## Have hints only (not a full walkthrough) — treat with the StoneOfWisdom caution

StoneOfWisdom's first script was a reconstruction of its bundled hint sheet
with no real navigation, and it tested nothing (the avatar never left the
first room). It was only useful once replaced with an actual winning
137-turn playthrough. Assume the same is true here: these need someone to
actually play the game to a win using the hints as a guide, not a literal
transcription of the hints file into commands.

- **BugHuntOnMenelaus** → `Bug Hunt On Menelaus.blorb` — hints:
  `walkthroughs/BugHuntOnMenelaus_hints.txt`
- **FinnsBigAdventure** → `FBA v.3c.blorb` — hints:
  `walkthroughs/FinnsBigAdventure_hints.txt`
- **MagorInvestigates** → `MI_v.1.blorb` — hints:
  `walkthroughs/MagorInvestigates_hints.txt` (very short file — may only
  cover part of the game; verify before trusting it as complete)
- **ThingsThatGoBumpInTheNight** → `TBN v.2.blorb` — hints:
  `walkthroughs/ThingsThatGoBumpInTheNight_hints.txt`
- **XanixXixonResurgence** → `XXR v.4.blorb` — hints:
  `walkthroughs/XanixXixonResurgence_hints.txt`

## Not in scope here: no walkthrough material at all yet

These game files are staged in `test/adrift5-games/` but there's no
walkthrough or hints for them anywhere in the corpus — finding/writing one is
a separate task from "wiring up," so they're not listed above:
DwarfOfDirewoodForest, Halloween, MuseumHeist, October31st,
TheEuripidesEnigma, TheFortressOfFear, TheLostLabyrinthOfLazaitch, Tingalan,
Tribute.

## Wiring checklist per game

1. Derive/convert a full winning (or intentionally-losing, if that's the
   only path) command script into `test/<Name>_walkthrough.txt`.
2. `test/a5_groundtruth.sh test/adrift5-games/<file> test/<Name>_walkthrough.txt`
   in both vanilla and `FD_RNG=xoshiro` modes; count the diff hunks.
3. Add a `name|game|vbudget|xbudget` line to the `MAP` heredoc in
   `run_a5_walkthroughs.sh`, using the observed hunk counts as the initial
   budget (0|0 if it's a clean MATCH).
4. If the vanilla transcript is stable, save it as
   `test/<Name>_expected.txt` so the harness can use the fast golden-diff
   path instead of shelling out to dotnet/FrankenDrift every run.
5. If it diverges at baseline, log the root cause in
   `A5_WALKTHROUGH_FINDINGS.md` / `TODO_a5_walkthrough_bugs.md` rather than
   just carrying a budget number with no explanation.
