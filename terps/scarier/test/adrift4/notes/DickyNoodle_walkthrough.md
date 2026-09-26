# Dicky Noodle 2002 — walkthrough (**WIN, intentional speedrun**)

- **Source:** `DickyNoodle.TAF`, a large ADRIFT amateur comedy adventure. P.I.
  Dicky Noodle sets out to find his kidnapped uncle, the Mayor of
  Noodleville, starting at a "haunted mansion"-style opening (Clicker's
  Candy Store, Agatha's crooked house) before an underground casino/TVLand
  gauntlet.
- **Content review:** raw profanity and crude/gross-out humor throughout —
  a corpse tied to a chair in a kitchen, an elderly NPC (Agatha, stated age
  68) described nude in a "shudder"-comedy beat, dialogue referencing sex
  shops and lubricant, and a closing tease of "plenty of sex" in an unmade
  sequel. None of this is depicted sexual content, and there are no minors
  anywhere in the game. Clean to wire.
- **Result:** **WIN**, 78/120 (65%) -- re-routed 2026-09-26 from 67/120;
  78 is the reachable ceiling (see "Score ceiling" below) — and the game's own ending text
  explicitly calls out that this is a shortcut: "you did all of this WITHOUT
  even having to go through the hideous and painful task of having to play
  through the many extra levels too!". Wired as
  `dickynoodle_solution.txt|DickyNoodle.TAF|You untie your loving Uncle
  Noodle`, no env.

## The route

Start with `knock on door` at the castle field (+1) and `read note` (+1).
Take the skeleton key from the guardhouse, then loop through "Clicker's"
Candy & Cookie Store's basement maze for a gun (kitchen) and a crucifix
(bathroom) before breaking the storage room's window to escape outside.
Circle around to Agatha's crooked house — the key silently satisfies its
locked front door — and upstairs, a bare `kill agatha` (no "with X" phrasing
needed) drops her and leaves a map behind.

Backtrack to the old man's shack near the mine road; taking its candle
triggers a scripted trapdoor fall into "Secret Area #1", the entrance to a
hidden underground casino town. In the Moolah Room, take 5 of the 7
harmless-looking cash piles (ones, fives, fifties, hundreds, five
hundreds) — **the pile of twenties is always instantly lethal**, a fixed
(non-random) trap object, so it and the untested pile of thousands (dropped
for inventory weight) are skipped. Dropping the now-spent key/map/candle
first keeps the carry weight under the limit.

Back at the Front Desk, a bare `buy room` spends one pile automatically and
warps the player straight to the casino's Penthouse. Taking its remote
turns on the TV and one-shots the player into "TVLand" at Mel's Diner (an
"Alice" sitcom parody). Sit, order and eat for free, then head two rooms
north through the kitchen to KITT's front seat (a Knight Rider parody),
where movement must be phrased as `drive <direction>`: `drive north` then
`drive south` (+3, not `drive east`, which costs -4) lands in Three's
Company; `d` to the Ropers' apartment, `call cab` (+3) to Taxi, `n n` to
Cheers' pool room and, WITHOUT the chainsaw, `leave bar` (-8) drops the
player in the same hospital discharge room the east crash uses. North to the
hallway (`take syringe`), in and down to the underground parking, `give
junkie syringe` (+7), one more north to the trail of cheese, and `follow
trail` (+7) jumps to the Victory room, where `untie uncle` (+10) ends the game.

## Score ceiling (78/120)

Score comes only from tasks. Two mutually exclusive terminal branches after
the common opening (knock 1, note 1, key 1, gun 3, window 5, Agatha 10,
map 3 = 24):

- Casino/TVLand (candle 6, room 9, remote 10, buy food 4, eat food 3,
  untie 10 = 42) then at North St.: south+cab+leave bar+junkie+trail
  = 3+3-8+7+7 = +12 (best); east+junkie+trail = -4+7+7 = +10; south+cab+
  chainsaw = +6. Total 24+42+12 = 78.
- Mineshaft/Earth Ship (crucifix; kill ghost 5, green cow 3, alien 5,
  razor 1, gargle 1, Gorgo 10, start ship 9, untie 10 = 44): 68. The candle
  trapdoor seals the player in the casino, and the Earth Ship lands in the
  bank basement next to Victory, so the two branches can't be combined (the
  yellow cow's return to the mineshaft costs -6 against the ghost's +5).
- The declared 120 is exactly every positive-scoring task once (repeatables
  buy food/eat food/kill ghost/gargle counted once) across BOTH branches.
  The 42 missing points = the Earth Ship branch's own 34 (ghost 5, green
  cow 3, alien 5, razor 1, gargle 1, Gorgo 10, start ship 9) + the -8
  `leave bar` penalty the best TVLand route pays. Unreachable in one game.
