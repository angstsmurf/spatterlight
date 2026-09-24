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
- **Result:** **WIN**, 67/120 (55%) — and the game's own ending text
  explicitly calls out that this is a shortcut: "you did all of this WITHOUT
  even having to go through the hideous and painful task of having to play
  through the many extra levels too!". Wired as
  `dickynoodle_solution.txt|DickyNoodle.TAF|You untie your loving Uncle
  Noodle`, no env.

## The route

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
`drive east` triggers a scripted car-crash detour that dumps the player,
uninjured, in a hospital discharge room. From there it's a straight shot —
north to the hallway, in and down to the underground parking garage, one
more north to find a trail of cheese — and `follow trail` jumps directly to
the Victory room, where `untie uncle` ends the game.
