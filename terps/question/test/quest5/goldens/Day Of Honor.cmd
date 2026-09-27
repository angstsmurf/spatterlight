# Day Of Honor (Evan Williams, a Triangle Games production, 2013, ASL v550) —
#   WINNING walkthrough, override-only (no published walkthrough exists).
# Ending reached: "THE HERO RETURNS", Ending 3 of 4 — the best of the game's
#   4 endings (the other 3: killing the elder in the "blowgun" puzzle leads to
#   an immediate LONELY TRAITOR/PRIMORIGEN CHAMPION branch; telling the tribe
#   the truth at the end leads to THE ELDER'S THEORY, being stoned and exiled).
#   finish is called (bare `finish` statement, not `finish(...)`); oracle
#   reports state=Finished errors=0. Deterministic, no RNG in this route.
# Route: enter temple, search the rubble (finds blowgun/"smoker", bullet,
#   crystal shard/"shard", axe — take smoker+shard only), go north to storage,
#   `explore shelves` reveals a green foamthycik blocking the far exit, load
#   the blowgun with the crystal shard and `blow smoker` (always opens a
#   target menu regardless of an "at X" suffix — "blow smoker at X" is parsed
#   as a two-object verb call and just prints "That doesn't do anything.");
#   pick the foamthycik (menu index varies with scope — verify before reuse).
#   Killing it caves in the exit but reveals a skeleton holding leather
#   gloves/"hand skins" and an ID card/"odd stone" — take+wear the gloves
#   (sets the "gloveson" flag; climbing the elevator-shaft ropes ungloved is
#   instant death). Climb the ropes down into the (dark) computer center,
#   `look at pulsating lights` to reveal the hidden "red circle" power button
#   (hidechildren objects need an explicit look before they resolve), touch
#   it to restore power/light, `look at smoothstone blocks` to spawn the
#   "working block"/throne/helmet, sit + put on the helmet, answer "Initialize?"
#   with 1 (Yes) — this sets the "educated" flag and renames most objects'
#   aliases (blowgun->still "blowgun" internally but "smoker" was its alias
#   throughout; "odd stone"->"ID card", "pulsating lights"->"primary station",
#   "statues"->"broken charge stations", etc). Go back up/out to the temple
#   entrance, `look at broken charge stations` to reveal "charge station",
#   use the ID card on it to charge it, return in/down, use the ID card on
#   the "primary station" to grant the card library access, `unlock door`
#   (the north door, now that access=true), north into the admin offices
#   hall, `use elevator` — triggers the final "wisdom" menu ("What will you
#   say?"); pick option 1 ("I killed a green foamthycik and found a room
#   with magical ancient writings") for Ending 3 of 4 (option 2, the truthful
#   answer, is Ending 4 of 4 — a worse, exile ending).
look
in
search rubble
take smoker
take shard
north
explore shelves
load smoker with shard
blow smoker
2
look at skeleton
take hand skins
take odd stone
wear hand skins
south
look at hole
climb ropes
2
look at pulsating lights
touch red circle
look at smoothstone blocks
sit on throne
put on helmet
1
up
out
look at broken charge stations
use ID card on charge station
in
down
use ID card on primary station
unlock door
north
use elevator
1
