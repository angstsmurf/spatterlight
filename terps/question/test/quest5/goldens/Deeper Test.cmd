# Deeper Test: bounded tour. The dungeon is randomly generated and endless (only a death ends it), so there is no win.
# Character creation is a JS dialog, answered via event:HandleDialogue (name|sex|str|agi|int|sta|bonus). Equip the sabre, take the key, descend one level.
event:HandleDialogue;Boris|Male|3|3|2|2|bonus2
look
i
equip sabre
east
in
take silver key
keys
d
look
help
