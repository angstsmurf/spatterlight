# Medievalist's Quest (Cein Chavez, 2017, ASL 550) — derived entirely from the
# game source: NO published walkthrough exists. A Quest GAMEBOOK
# (_editorstyle=gamebook, inlined GameBookCore): every "command" is a page
# NAME (the options-dictionary key), not the link text — HandleCommand does
# GetObject(command) and moves the player there. Linear CYOA about Jonathan,
# who just wants popped corn: find the basement key in the piggy log, flee
# the Cinch Scoundrel, lose chicken Newtford to a soleclops, then beat the
# Scoundrel (drink half the cupped strength residue -> CRITICAL) and
# incinerate the soleclops, reaching the authored terminal page "Part 1 end"
# ("End of part 1" — the game is explicitly only part 1/3).
# Notes:
#  - The source contains NO finish() call anywhere; the ending page has no
#    options, so state=Running at "Part 1 end" IS the genuine full ending
#    (precedent: games whose ending parks without finish).
#  - Author bug: in the soleclops fight, page "Bout1"'s Fight/Item options
#    point at the FIRST battle's Fight/Item pages (which loop back to the
#    first "Bout"), not "Fight2"/"Item2" — so the residue "Strength" page is
#    unreachable; the Spunk Spell -> Punch -> incinerate route is the
#    intended/working kill.
#  - Tour included: all three key-hunt searches (bed, wood pile, vases) and
#    the safe battle flavour options (Fight/Item hesitation, blocked Run)
#    that loop back to their Bout pages. No RNG on this path.
Living Quarters
Mmm
Cup
Cooking Pit
Open the basement door
Look under bed
Open the basement door
Fire wood pile
Open the basement door
Look at nicely made vases
Piggy log
Grab your piggy log again
Go down
Grab the lamp on the crafting bench
Bout
Fight
Bout
Item
Bout
Run
Close the basement door
Tiresome
Fire
Lamp
Outside
Coop
Enter coop
Greetings
Letter
Goldlisum
Acorn
Plan
Bout II
Run1
Bout II
Item1
Item use bout II
Bout II CSR used
Fight power
Victory
Equip 1
Soleclops
Bout1
Spunk Spell
Punch
Spunking it up
Victory1
Continue
Part 1 end
