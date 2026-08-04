# Mangiasaur — walkthrough (**WIN, 63/74, 87 commands**)

- **Game:** *Mangiasaur* by DCBSupafly, ADRIFT Spring Comp 2011. You are a newly
  woken dinosaur alone in the forest, and the entire verb set is **EAT**.
- **Engine:** **ADRIFT 4.00** — the 14-byte header is
  `3c 42 3f c9 6a 87 c2 cf 93 45 3e 61 39 fa`, `V400_SIGNATURE` verbatim
  (`sctaffil.cpp:55`). 8 rooms, 65 objects, **190 tasks**, 33 variables,
  33 events, 3 NPCs.
- **Result:** **WIN** — `eat platter` in the Hall of Humans runs the ending
  chain (TASK 177 → 178..186), which prints the "you taste. . ." roll-call of
  everything you ate on the way, then *"You have eaten and you have grown."*
  and the credits: *"Thanks for playing Mangiasaur! …"*
- **Score: 63 out of a maximum of 74 — and 63 is the ceiling.** Ten of the
  missing eleven belong to a monster that is never placed in the world, and
  the eleventh is a duplicate score action that ADRIFT 4.00 refuses to pay
  twice. See "The 11 points nobody can score" below.
- **Harness row:** `mangiasaur_solution.txt|Mangiasaur.taf|Thanks for playing
  Mangiasaur!|`, PASSing golden. No env assignments — the game has no title
  screen and no wait-for-key before the first prompt, and the one waitkey it
  does have (*"Press any key for credits and scoring"*) is fed by the
  `quit`/`y` the harness appends, so `SCR_SKIP_WAITKEY=1` makes no difference
  to the transcript.
- **Sources:** `downloaded/Mangiasaur_clubfloyd.html` — the ClubFloyd session
  of 2012-02-12 (the page's own URL says `intfic_clubfloyd_20120202`, which
  disagrees with its own title; the URL is what allthingsjacq.com actually
  serves, so `INDEX.md` keeps it). 247 commands, a winning oracle in the sense
  that the group did eventually reach `eat platter`, but it is group flailing
  with `undo` in it and about half the commands are jokes, so it is unusable
  as a replay. It confirmed the shape of the endgame (spear → hole → down →
  platter) and nothing else. The game also ships its own hint list: type
  `walkthru` (TASK 189) for the progression spine.

## The 11 points nobody can score

`score` maxes out at 63/74 no matter what you do, and both missing chunks are
author bugs rather than content the route skipped.

**10 points: `eat NAMGUAGL` (TASK 76) is unreachable because the NAMGUAGL is
never placed in the world.** `SCR_DUMP_OBJLOC` at load says

```
OBJLOC obj=15 pos=-1 room=-1 parent=-1 effroom=-1 static=0 … [NAMGUAGL]
```

i.e. hidden. Grep the whole task dump for obj15 and you get six restrictions
and exactly one action — TASK 76's own `ACT type=0 v1=7 v2=0 v3=0`, which
*hides* it again. Nothing else moves it, and no event moves any object at all:
every `EVENT` line in the dump ends `o2=0->0 o3=0->0`. So the beast the game
spends three tasks warning you about (TASK 77/78 `# NAMGUAGLWarning2/1`, all
gated on "NAMGUAGL visible to player") and the death it can inflict
(TASK 79 `# DEATH BY NAMGUAGL`, plus EVENTs 16/26/27 `FEAR THE NAMGUAGL2/1/0`)
are all dead code. You can stand on the Forest Floor forever and nothing will
come.

**1 point: TASK 123 `eat mutilated carcass` asks for its score twice.** It
carries two `ACT type=4` actions, `v1=5` and `v1=1`. In
`task_run_change_score_action` (`sctasks.cpp:1027+`) the guard is
`increase_score = !gs_task_scored(game, task)` and only `version <=
TAF_VERSION_380` is allowed to re-score, so a 4.00 game pays the first and
drops the second. The route gets the 5.

## Eight counter variables are declared, read, and never written

Grep every `ACT type=3` in the dump and these eight never appear on the left
side: **eatenMoths, eatenBugs, eatenMoss, eatenHoppers, eatenBuzzBirds,
eatenBushes, eatenRoots, hunterHasSpear**. Consequences worth knowing:

* The escalating bat attack (TASK 49, "you have eaten 5/10/15 bugs and the
  colony has noticed") can never fire.
* Moss, bugs, slither hoppers and bristle bushes are effectively infinite —
  their "you have eaten them all" guards test counters stuck at 0.
* **Two of the eight ending paragraphs can never print.** TASK 179 (moss) and
  TASK 185 (roots) are gated on `eatenMoss`/`eatenRoots`, so the winning
  transcript shows six "you taste. . ." lines (caterpillars, rodent liver,
  scale crawlers, herd beast, beetles, shrimp) out of eight, no matter how much
  moss and how many roots you ate. This route eats both, repeatedly, and still
  gets six.

## Four timing facts that cost derivation time

### 1. The air sac is a **one-shot** fuse, and that is why the route eats five in a row

`eat air sac` (TASK 86) sets `carcassEdible = 1`. That one variable does two
unrelated jobs:

* it suppresses the drowning timer — `down` (TASK 100) executes TASK 101
  `# Down, without air sacs`, whose only restriction is `carcassEdible == 0`,
  and TASK 101 is what starts EVENT 28, a flat 5-turn countdown to TASK 89
  `# Drown`;
* it is the gate on TASK 123 `eat mutilated carcass` up on the Mesa Top.

The catch is **EVENT 17 `# no breathing`, `starter=3 startTask=87 restart=0`**.
The *first* air sac you ever eat starts a 10–20 turn countdown that runs
**exactly once** and finishes by executing TASK 88, the breathe hub, which sets
`carcassEdible` back to 0 (and, if you are still in the ocean, runs TASK 89 and
kills you). Because `restart=0`, eating more air sacs does not restart it — but
neither does it protect you: an early sac's fuse will happily expire *after* a
later sac and undo it.

Two dead ends this produced before the route settled:

* Eat a sac in the valley, do the hut, then dive → the fuse expires mid-ocean:
  *"You inhale sharply after the last bit of air sac air leaves your nostrils.
  You take a deep breath. / Your lungs fill up with water… I'm afraid you are
  dead!"*
* Eat a sac on the way back from the ocean and rocket straight to the mesa →
  the *old* fuse expires one turn after landing, and the carcass answers
  *"The stench of it is too much. Maybe if you held your breath…"* (`hold
  breath`, TASK 125, is flavour — it has no actions).

The fix is to make the one-shot fire while you are standing somewhere safe,
and then eat one more sac: eat sac #1, dive immediately, clear the ocean in
six turns, surface, and then eat sacs in the valley until the fuse pops. After
it has popped, `carcassEdible` stays 1 for the rest of the game. Five sacs
(commands 71–75) is exactly enough under the harness seed — the fuse fires on
command 74 and the sac on 75 sticks.

### 2. `burp on sap` is not a door, it is the ride

TASK 162 `burp * sap` has one restriction (`canBurp == 1`) and an
`ACT type=1 v1=0 v2=0 v3=6` — it *moves the player to the Mesa Top*. This is
Foam's rocket tree from the credits: the burp ignites the sap, the tree
uproots itself and launches you at the plateau. `canBurp` comes from eating the
hut's **still-lit** torch (TASK 94), so the mesa is gated behind the hut, not
behind the valley.

### 3. The cavern's bat and the canopy's moth are pure turn-counting

* The bat (EVENT 11, 3–20 turns) arrives on the **8th** `eat moss`, and TASK 52
  needs it present. Seven fillers is one turn early and you get *"The hunger
  makes you crazy. Try again."*; eight is exact under the harness seed.
* `eat moth` (TASK 39) needs `timeSinceChirp <= 1`, and `chirp` (TASK 33) sets
  it to 0. The ticker that raises it is EVENT 10, whose **pause task is TASK 38
  `# Chirp Empty Response`** — the version of `chirp` that runs in every room
  *except* the cavern. So the moment you chirp anywhere else (command 44, back
  up in the canopy), the ticker is paused for good and `timeSinceChirp` is
  pinned at 0. Every later "how long since you chirped" gate is therefore
  permanently open.
* Each rodent needs its own fresh chirp: TASK 64/65 unset TASK 63
  `# Set Rodent Edible`, and TASK 61 requires TASK 63 to be *done*.

### 4. The ocean is a light budget, and the shark only shows up in the dark

`oceanLight` starts at 6 and every tentacle net eaten costs 1 (TASK 108/109).

| what | needs |
| --- | --- |
| `eat shrimp` (+3) | `oceanLight > 3` — and TASK 112 makes the shrimp vanish **permanently** the moment light drops below 4, so eat them first |
| `eat wispy stalk plant` (+2) | `oceanLight > 2` |
| the baby shark appears (TASK 115) | `oceanLight < 4` **and** `> 1` |
| `eat * shark` (+5) | shark present, `size >= 30`; it then teleports you back up to the hut, so it is necessarily the last thing you do down there |

Three nets take the light from 6 to 3, which is inside the shark's window, and
EVENT 29 puts the shark in the water one turn later. That makes the whole ocean
six turns — which matters, because of the air-sac fuse above. Never `burp`
underwater: TASK 173 `# Burp (Ocean Drown)` starts EVENT 21, a 1-turn fuse to
TASK 89.

## Things not to type

* **`eat human`** in the Hall of Humans (TASK 187) is a second, worse ending —
  `ACT type=6 v1=0` gated on `size > 60`. This route ends at size 84, so it is
  wide open and would cut the game short.
* **`eat hunter`** on the mesa (TASK 148) needs `size >= 80` and scores
  nothing. `eat spear` gets the spear off him without eating him.
* **`eat hut dweller`** (TASK 97, `size >= 45`) also scores nothing, and the
  dweller is the one who hands you the food that does.

## The route

87 commands. `harness/mangiasaur_solution.txt`. Score in brackets is the
running total.

| # | Command(s) | What it does |
| --- | --- | --- |
| 1–24 | `eat pollen`, `eat fly` ×5, `eat bud` ×10, `eat caterpillar` ×8 | Forest Canopy. Only the first of each scores (+1 each); the rest are size and, mostly, turns — the cocoon (EVENT 8) needs 5–24 of them before it appears, and it shows up on turn 20 here **[4]** |
| 25 | `eat cocoon` | **+3**, and TASK 24 sets `canFly`, which is the gate on the canopy↔floor exits (TASK 26) **[7]** |
| 26–27 | `look`, `eat bird` | one filler turn; the small green bird (EVENT 4) arrives on it. `eat bird` is **+2** and +6 size **[9]** |
| 28–30 | `d`, `s`, `eat bug` | down to the Forest Floor, south into the Cavern, **+1** **[10]** |
| 31–38 | `eat moss` ×8 | **+1** on the first. Eight is exact: the bat lands on the 8th **[11]** |
| 39 | `eat bat` | **+4**, +6 size, and TASK 54 sets `canChirp` **[15]** |
| 40–41 | `chirp`, `eat rodent` | the chirp reveals a rodent; eating it is **+3** and sets `canDig` **[18]** |
| 42–45 | `n`, `u`, `chirp`, `eat moth` | back up to the canopy. This chirp is what freezes the chirp ticker (see above); the moth is **+5**, the biggest single award in the game **[23]** |
| 46–48 | `d`, `eat slug`, `eat scale crawler` | Forest Floor: **+1**, **+2** and +16 size **[26]** |
| 49–54 | `e`, `eat grass`, `eat beetles`, `eat bark`, `eat sap`, `eat herd beast` | Valley Pass. The grass reveals the air sacs and the beetles; the beetles are **+1** and set `sharpTeeth`, which is the gate on bark; the bark is **+1** and leaks the sap; the sap is **+1**; the herd beast is **+1** and +20 size (it needs `size >= 30`) **[31]** |
| 55–59 | `in`, `eat torch`, `eat food`, `eat rug`, `dig` | Small Adobe Hut. The torch is **+3** and sets `canBurp`; eating it is also what makes the dweller offer food (EVENT 31 fires on the same turn), **+1**; the rug is **+1** and uncovers the hole; `dig` needs `canDig` and opens the way down **[36]** |
| 60–63 | `out`, `eat air sac`, `in`, `d` | **+2**, and this is sac #1 — the fuse starts here and the dive has to be immediate **[38]** |
| 64–65 | `eat shrimp`, `eat wispy stalk plant` | **+3**, **+2**, both while the water is still bright **[43]** |
| 66–68 | `eat tentacle net` ×3 | **+2** on the first; the three of them take `oceanLight` 6→3 and put the baby shark in the water **[45]** |
| 69 | `eat shark` | **+5**, +size, and it chases you straight back up into the hut **[50]** |
| 70–75 | `out`, `eat air sac` ×5 | Valley Pass. The one-shot breathe fuse fires on command 74; the sac on 75 is the one that sticks, and `carcassEdible` is 1 from here to the end |
| 76 | `burp on sap` | the rocket tree — lands you on the Mesa Top |
| 77–82 | `eat slither hopper`, `eat buzz bird`, `eat carcass`, `eat vomit`, `eat bristle bush`, `eat roots` | **+1**, **+1** (and the buzz birds abandon the carcass they were guarding), **+5**, **+3**, **+1**, **+2** **[63]** |
| 83–84 | `eat spear`, `vomit spear into hole` | the hunter holds the spear out rather than fight; swallowing it and coughing it back up wedges it in the small hole, and the flat rock slides open (TASK 140) |
| 85–87 | `d`, `score`, `eat platter` | Hall of Humans. EVENT 30 fires on the way down and TASK 176 puts the platter in front of you; `score` records 63/74 in the golden; `eat platter` → **WIN** |

## Reproducing

```sh
cd terps/scarier/adrift-walkthroughs
sh harness/run_v4_walkthroughs.sh mangiasaur   # PASS against the committed golden
```
