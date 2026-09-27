# Bear's Epic Quest — Hospes, 2011 (ASL 500, Quest 5.0.4233 — oldest ASL version in the corpus)
# No published walkthrough exists — derived from game source (game.aslx).
# Ending: the game's only ending — "Sleep on" the Comfy Stone in the Dark Cave
# calls `finish` directly (the sole `finish` in the source).
# Route: you play a bear. "x bear" (the player object's look script) sets the LAS
# flag and unlocks both exits out of the Great Outdoors. Defecating on the leaves
# sets the Crapped flag, which gates eating the deer carcass; eating the carcass
# creates the "Dark Cave" exit back in the Great Outdoors.
# Quirks: the bushes exit's alias overrides its "in" direction, so it can only be
# entered as "go and enter some bushes". Custom author verbs: Shit on / Maul /
# Sleep on / Piss on. "piss on god" (a global command) spawns the dead astronaut
# DEADSAUBER — optional flavour, included for content. No score, no RNG, no timers.
x bear
go and enter some bushes
x leaves
shit on leaves
taste leaves
out
s
x deer
eat deer
kill deer
x carcass
eat carcass
n
piss on god
x astronaut
piss on astronaut
go dark cave
x stone
sleep on stone
