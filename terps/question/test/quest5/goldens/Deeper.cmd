# Deeper (v2.4, "The Pixie", first published 2016). A procedurally-generated
# dungeon-crawler roguelike; no published walkthrough exists anywhere.
#
# CHARACTER CREATION IS A RAW HTML/JS DIALOG, NOT A ShowMenu: the game's
# <gamestart> block injects a jQuery-UI dialog (name field, sex radios, four
# point-buy attribute counters against a 10-point pool, five starting-bonus
# radios) whose "Done" button runs client-side JS (setValues()) that packs
# the choices into "name|sex|str|agi|int|sta|bonus" and fires
# ASLEvent("HandleDialogue", answer). This has no text-typeable fallback --
# unlike the game's later level-up menus (line 16096 etc.), which ARE plain
# ShowMenu and drivable normally -- so it cannot be answered by typed input
# at all. It IS drivable directly via the oracle's own `event:NAME;PARAM`
# script directive (Program.cs: `event: -> world.SendEvent`), which calls
# QuestViva's SendEvent(eventName, param) exactly as ASLEvent would: this
# invokes <function name="HandleDialogue" parameters="answer"> with `answer`
# set verbatim to the PARAM half of the directive, still split on "|" by the
# function itself as the JS would have done. HandleDialogue does zero
# validation of the point-buy total (see game.pov.pointsleft, computed but
# never enforced) or of the bonus key, so any "name|Male|s|a|i|t|bonusN" with
# s+a+i+t<=10 and bonusN in bonus1..bonus5 is accepted. This script uses
# `event:HandleDialogue;Skybird|Male|3|3|2|2|bonus1` (default name, all 10
# points spent, bonus1 = two healing potions) -- the same JS-callback-bypass
# technique documented for Moquette's ASLEvent-driven Act transitions.
#
# BEST-REACHABLE, NOT FINISHED -- STRUCTURALLY DISPROPORTIONATE, NOT GATED:
# unlike Pixie's Quest (genuinely unwinnable) this game IS winnable in
# principle -- the sole win (source line ~8216/8229) is reaching 14/14
# artefacts, then walking north or south out of the road. But the dungeon is
# a true procedural roguelike: GenerateLevel(room) builds each of (at least)
# 14 increasingly dangerous floors on first visit from a growing random
# encounter table (game.specials, one semicolon-separated monster group per
# floor, e.g. floor 14 = "lich;ghost-12;zombie-7;zombie-7;zombie-7;zombie-7"),
# and artefacts are not at fixed locations -- the in-game hint text is
# explicit: "SEARCH bodies to find things" after killing a monster, i.e. loot
# is chance-based per kill, not scripted per room. This is architecturally
# unlike The Legend of the Secret of the Smelly, Stinky Fish (also a combat
# dungeon crawl, but with a FIXED ~93-room map and deterministic single-alias
# spawn functions, fully derivable to a hard `finish` in 1021 steps): Deeper
# has no fixed map to read from source and no way to know how many kills a
# given artefact requires without exhaustively playing every branch. Treating
# 14/14 as in-scope for a hand-derived script would mean playing an unbounded
# amount of procedurally-generated content sight-unseen, not transcribing a
# knowable path. This script instead demonstrates the actually-interesting,
# fully deterministic parts -- character creation via direct event injection,
# equipping a weapon, reaching the dungeon entrance, collecting the edifice
# key, descending to floor 1, and one leg of corridor exploration -- and
# stops at a clean, reproducible non-combat checkpoint rather than starting
# (and freezing the exact outcome of) an open-ended fight.
#
# ENGINE GAP FOUND (non-fatal, game keeps running): `south` off the level-1
# chamber lands in a passage-corner room whose <description> script calls
# ProcessExitList -> FormatStringList(a), and FormatStringList does not
# exist anywhere in QuestViva's engine or standard library (grep confirms
# zero definitions) -- it's a legacy ASL library helper (list->prose join,
# "a, b and c") that was apparently never ported. This only fires for a room
# whose exit list has an entry with 3+ pipe-separated descriptors (rare);
# QuestViva degrades gracefully (two caught script errors, room still prints
# its exits from the fallback %exits% substitution), so the game is left
# fully `Running`, not soft-locked. errors=2, state=Running, deterministic
# (seed=1234).
event:HandleDialogue;Skybird|Male|3|3|2|2|bonus1
equip dagger
east
north
take key
down
south
