# The Encyclopedia of Elementals (Adam Holbrook, 2013, ASL 540) -- best-reachable script
#
# RESULT: state=Finished, errors=0 -- but NOT the intended win. This reaches the game's
# "Game Over" castle-collapse death (the "GoBoom" sequence in the Main Hall), which is
# PROVEN to be the only finish() reachable by *any* command script under this oracle --
# see the derivation note below. Two runs (seed 1234) are byte-identical.
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
# WHY THE REST OF THE GAME IS UNREACHABLE (verified by direct experiment, not just
# code reading -- see below): with GoBoom set, "sinister to secret" and "main hall to
# sinister" get LockExit'd, forcing the only route back through Main Hall. Main Hall's
# <description> script (game.aslx:1284-1310), on ANY entry with GoBoom true, prints
# the collapse scene and then does:
#     wait { SetTimeout(50){ msg(...); SetTimeout(13){ msg(...); SetTimeout(7){
#       msg(...); finish } } } }
# -- an author-intended 70-REAL-SECOND window in which an interactive player can type
# "help man"/"use pole on beams" (SetObjectFlagOff GoBoom, rescues Dave, 150 gold) or
# "run away" (escapes, sacrificing Dave) before the collapse. The harness's DrainTimers
# (harness/oracle/Program.cs, see its "Real-time timers" README section) is a `while`
# loop that drains every pending self-destructing `timeout*` SetTimeout to completion,
# synchronously, right after AutoAdvance() for the SAME command that triggered them --
# BEFORE the next script line is even read. Because each callback in this chain creates
# the next SetTimeout itself (no intervening real "wait" the harness would pause on),
# the whole 50+13+7 cascade -- all three warnings AND the final `finish` -- fires within
# the single "go main hall" command's settle. This was verified directly: a script
# ending in "go main hall" / "run away" / "take metal pole" / "use pole on beams" (in
# that order) shows via [diag] that finish already fired mid-"go main hall", with
# scriptExhausted=False -- none of the three follow-up lines ever executed, because
# world.State was already Finished before they could be read. This is the load-bearing
# mirror image of the corpus's documented Escape From the Mechanical Bathhouse case
# (an AUTHORED, non-self-destructing real timer the oracle leaves dormant, granting
# UNLIMITED time) and of I Contain Multitudes (an ending SetTimeout the harness never
# even reaches): here a chain of SELF-destructing SetTimeouts inside a danger sequence
# instead resolves ATOMICALLY and INSTANTLY, granting ZERO time -- there is no point in
# any script, before or after "go main hall", where a rescue or escape command could be
# inserted. Since the "main hall to castle fields" exit is the only way out of the
# Castle chapter, and entering Main Hall with GoBoom true is mandatory to reach it, and
# that entry unconditionally and immediately ends the game, Sections 2-5 of the game
# (Drensburg, the Ruined Castle, the Druid/Fort infiltration, and the finale against the
# crystal necromancer) are PROVABLY unreachable from this oracle by any command script,
# regardless of player choices -- not a derivation gap, a proven engine/harness timing
# incompatibility specific to this game's real-time puzzle design.
#
# This script therefore plays the entirety of the Castle chapter's reachable content
# (both hidden-passage puzzles, the trigger-book pickup) and ends on the forced "Game
# Over" -- the only finish() any script can reach. errors=0.
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
