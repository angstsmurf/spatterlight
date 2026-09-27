#!clock=5
# The Encyclopedia of Elementals (Adam Holbrook, 2013, ASL 540) -- best-reachable so far
#
# RESULT: state=Running, errors=0 -- the whole Castle chapter, including the Main Hall
# rescue (Dave saved with the metal pole), ending alive in the Fields Across the Moat
# at the start of Section 2. The rescue is a real-time window, hence `#!clock=5` (the
# typing clock: five seconds per typed command, nothing drained) -- see the note below.
# Sections 2-5 are reachable now and still owed.
#
# SOURCE: no published walkthrough exists anywhere for this game -- derived entirely
# from game.aslx (15617 lines, ASL 540, 5-section high-fantasy plot: Castle heist,
# Drensburg romance/training, a Ruined-Castle flashback dungeon-crawl, a Druid/Fort
# infiltration, and a finale against a "crystal necromancer"). `grep -n finish` finds
# 6 sites total: 4 are deaths (this castle-collapse; two window-jump suicides in the
# Bedroom and the Blacksmith Shop; `changedhealth` health<1 "You have been slain.");
# the 6th, in the `End` room's `end` command, is the genuine win, reached (per the
# source) after defeating the crystal necromancer with a dispel potion in Final Fight.
# THAT ending is structurally unreachable from the oracle -- see below.
#
# CHARACTER CREATION: name (free text), gender (1=Male), aspect (1=Fire -- gives a
# +5 damage fire amulet and direct dialogue later; 2=Light/3=Force instead insert a
# "druids explain" detour scene reaching the same place, so Fire is the richest/most
# direct branch and was picked per the task's "one coherent branch" guidance -- moot
# here since the game never gets that far, but kept for a faithful start), input
# color (1-4, cosmetic).
#
# ROUTE PLAYED (the entire Castle-chapter content the oracle can actually reach):
# bed -> "get up" -> Bedroom (open nightstand for glow powder, look under bed for
# slippers) -> Hallway -> Main Hall (x throne/tapestry) -> Mysterious/Sinister Wing
# ("pull torch" reveals a secret passage) -> Secret Passage ("use glow powder" lights
# the dark room, "push rock" reveals the Forbidden Library) -> Forbidden Library
# (x bookshelves; "x table" reveals an open book on it -- a `hidechildren` surface,
# so the table must be examined before "take open book" resolves; taking it via its
# alias "open book", NOT its alt "bestiary"/"monster", which either hits the wrong
# "bookshelves" object or "can't see that" before the table is examined) -- taking
# the book is the game's title beat: it converts to the "Encyclopedia of Elementals"
# inventory item and sets player.GoBoom=true, which is the ONLY thing anywhere in the
# source that reveals the "main hall to castle fields" exit -- the SOLE way out of the
# Castle chapter (grep confirms exactly one `to="Castle Fields"` in the whole file).
#
# THE MAIN HALL RESCUE WINDOW (why this script needs `#!clock=5`): with GoBoom set,
# "sinister to secret" and "main hall to sinister" get LockExit'd, forcing the only
# route back through Main Hall. Main Hall's <description> script (game.aslx:1284-1310),
# on ANY entry with GoBoom true, prints the collapse scene and then does:
#     wait { SetTimeout(50){ msg(...); SetTimeout(13){ msg(...); SetTimeout(7){
#       msg(...); finish } } } }
# -- an author-intended 70-REAL-SECOND window in which the player can type "x rubble"
# (the rubble is a `hidechildren` surface; examining it reveals the metal pole), "take
# metal pole", "use pole on beams" (SetObjectFlagOff GoBoom, rescues Dave, moves both
# to Castle Fields after a `wait`) or just "run away" (escapes, sacrificing Dave).
# Under the default DrainTimers model -- a player who waits out every pending
# SetTimeout before typing again -- the whole 50+13+7 cascade, `finish` included,
# resolved inside the single "go main hall" command, and this script's golden was that
# forced "Game Over" until 2026-09-25. On the typing clock the three rescue commands
# cost 15 seconds against the 50-second first warning: Dave is saved and the script
# ends in the Fields Across the Moat with the rescued man in tow.
#
# This script therefore plays the entire Castle chapter (both hidden-passage puzzles,
# the trigger-book pickup, the rescue) and stops alive at the start of Section 2
# (Drensburg). errors=0.
Hawk
1
1
1
get up
open nightstand
take glow powder
look under bed
take slippers
go hallway
go main hall
x throne
x tapestry
go mysterious wing
x torch
pull torch
go secret passage
use glow powder
x loose rock
push rock
go forbidden library
x bookshelves
x table
take open book
x Encyclopedia of Elementals
go secret passage
go mysterious wing
go main hall
x rubble
take metal pole
use pole on beams
