# Defeating The Monster: The Ranking Of The Class (Social Dragon 368, 2019, v3.2,
# ASL 580) -- WINNING SCRIPT
#
# RESULT: state=Finished, errors=0, 115 steps -- the game's only win, the King EX
# in the Super Throne Room EX ("You defeated the King! ... THE END."). The other
# `finish` in the game (failing the Throne Jail dice) is the LOSS ending.
#
# SOURCE: no walkthrough exists for this game anywhere; derived entirely from
# game.aslx. Every fight is a RandomChance roll, so this is an RNG alignment
# rather than a puzzle solution: under the harness's deterministic seed the draw
# sequence is fixed, and the script steers it by choosing WHICH rolls to spend.
#
# THE SHAPE OF THE WIN (all of it rests on author bugs -- the intended game is
# effectively unwinnable):
#
# * MONEY IS NEVER CHECKED. No <take> script tests game.pov.money before
#   DecreaseMoney, so every shop item is free. You start with 50 pounds and buy
#   the 11,000-pound Perma Shield and the 6,000-pound Perma Boiled Item outright.
# * SECRET ALLEY SKIPS THE GAME. `look at secret alley` in the opening Briefing
#   Room teleports you straight to the Battle Challenge Lobby, one move from the
#   Throne Room EX -- past the entire main quest (Alanks -> Petris -> Triffids ->
#   Nochos -> Orderer -> The King -> take throne). It also means the 500-turn
#   clock never starts: that timer is armed by `look at time switch`, which this
#   route never touches, so the run is untimed.
# * THE ARENA IS A SAFE RNG PUMP. `look at dummy` costs at most 25 health (75% of
#   the time it costs nothing) and burns exactly two draws, versus 75-125 per
#   swing at a real enemy. It is the only cheap way to advance the stream, and
#   the 21 dummy swings here exist purely to line up the Grand Triffid's 2% kill.
# * DYING IN THE ARENA IS FREE. The dummy's 20% "challenge over" branch moves you
#   to the Lobby AFTER onhealthzero moved you to Throne Jail, overwriting it --
#   so the death at step 25 costs nothing and skips the 70%-fatal dice roll.
# * PERMA SHIELD IS AN INFINITE HEAL. Its <use> is +75 health on a one-turn
#   SetTurnTimeout and never removes the object, so it can be re-used every turn.
#   It doesn't block anything despite its description. This is what pays for the
#   Button Lock's 50% health toll (which sends you to Throne Jail if used at or
#   below 50%).
# * PERMA BOILED ITEM IS A ZERO-RISK PUMP. Its <use> is a bare RandomChance(33)
#   with no downside branch and no consumption -- one draw per turn, forever.
#   The 76 uses at the end walk the stream to the offset where the King EX's 1%
#   lands, from the Pro Item Store one move away.
#
# The two `1` lines answer disambiguation menus: `take boiled item` runs
# CloneObject(Boiled Item) AFTER DoTake has already moved the original into the
# inventory, so the clone lands in the inventory too. One take yields two boiled
# items, no further take is possible (the room is empty), and every later
# `eat boiled item` has to pick between the two.
#
# The three "You attacked the king" lines after THE END are authentic: `finish`
# does not abort the calling script, so the King EX's remaining damage rolls run
# and kill you on the same blow that wins the game. Replaying that tail
# byte-identically needed an engine fix -- drain_on_ready() flushed the on-ready
# queue after `finish`, printing the Throne Jail room description that QuestViva
# abandons (Core does its room description from inside nested `on ready`).
take boiled item
look at secret alley
east
look at dummy
look at dummy
look at dummy
look at dummy
look at dummy
look at dummy
look at dummy
eat boiled item
1
look at dummy
look at dummy
look at dummy
eat boiled item
look at dummy
look at dummy
east
look at dummy
east
look at dummy
east
look at dummy
northwest
look at grand triffid
west
take perma shield
use perma shield
use perma shield
north
use button lock
use perma shield
use perma shield
northwest
northwest
take perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
use perma boiled item
northwest
look at king ex
