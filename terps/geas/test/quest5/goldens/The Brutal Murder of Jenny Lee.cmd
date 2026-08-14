# The Brutal Murder of Jenny Lee (Christina Zhang) -- WINNING SCRIPT
#
# RESULT: state=Finished, errors=0 -- genuine win: Ending 1 "escape". After solving the
# murder (Mrs. Vanderbilt, the librarian, killed Jenny for the Mother-of-Pearl saxophone
# keys), the detective AI follows the previous detective's trail: west x10 in Limbo, then
# the fog exit north of the Last Storage Room cabinet ("You take a deep breath and step
# forward." -> finish).
#
# SOURCE: the game's official PDF walkthrough (bundled with the game), extracted to text.
# It is a bullet list of near-literal commands organised by Day; all commands below are
# taken from it verbatim except as noted. Deviations / derivation choices (all verified
# against game.aslx, seed 1234):
#   * The two endings are mutually exclusive; this script takes Ending 1 (escape), which is
#     a deterministic `finish` on the revealed fog exit. Ending 2 (museum) would instead
#     idle 15 turns in Limbo 5/6 (SetTurnTimeout(15) + nested SetTimeout monologue chain).
#   * "Go in any direction N times" bullets are realised as explicit `go north` repeats:
#     Day 1 limbo = 7 moves (journal's SetTurnTimeout(7) then returns you to the Bedroom);
#     Day 2 limbo = 7 moves (the piece of paper is moved into Limbo 2 by a SetTurnTimeout(5);
#     an odd move count leaves the player in Limbo 2 with it), then 2 more moves after
#     reading it (SetTurnTimeout(3) -> Learning Center).
#   * Day 3 limbo uses the sticky note's literal directions N W N E E E S (the exit scripts
#     count per-direction moves: needs e>=3, n>=2, s>=1, w>=1; the 7th move lands on
#     Rainy Street).
#   * "1953", "0903", "4489", "2471" are answers to `get input` prompts (keypad / library
#     computer), sent as plain lines per the harness format.
#   * The walkthrough's "Unlock file cabinet" auto-opens the cabinet (the green key found in
#     the Day 3 pencil basket is its key object), so no extra `open` is needed -- Jenny's
#     father's letters and the silk bag of pearl keys are revealed directly.
#   * Ending 1's "Go west x 10" works because only the WEST exits of Limbo 5/6 increment the
#     limbo_w2 counter; the 10th west teleports to the Last Band Office, before the museum
#     ending's 15-turn timeout can fire.
#   * No verb remaps were needed: every walkthrough command parses as written (look at /
#     look under / shake / flip / open / read / unlock / use are all authored verbs).
# Day 1 -- tutorial
take key
open door
take door
go south
open key
# Day 1 -- Band Room
look at binder
look at bulletin board
look at bulletin notice
look at chalkboard
go east
look at cabinet
use keypad
1953
open backpack
read journal
# Limbo: 7 turns until SetTurnTimeout(7) fires
go north
go north
go north
go north
go north
go north
go north
# Day 2 -- Bedroom -> Library
look at desk
look at information desk
open box
look at pink card
look at green card
use computer station
0903
use computer station
4489
# Limbo: note arrives in Limbo 2 after 5 turns; walkthrough says 7 moves
go north
go north
go north
go north
go north
go north
go north
look at piece of paper
go north
go north
# Day 3 -- Learning Center -> Office
go east
look at blue workspace
shake pencil basket
flip pencil basket
open key
read sticky note
# Limbo3: exit combination from the sticky note
go north
go west
go north
go east
go east
go east
go south
# Rainy Street
read newspaper clipping
# Day 4 -- Last Bedroom
look at desk
look at journal
look under desk
read bundle of letters
go north
go west
go north
look at jacket
look at business card
go east
go east
go east
go south
use computer station
2471
unlock file cabinet
look at silk bag
look at stack of letters
# Limbo 5: Ending 1 (escape) -- west x10
go west
go west
go west
go west
go west
go west
go west
go west
go west
go west
# Last Band Office -> Last Storage Room
go west
look at cabinet
use keypad
2471
read parchment
go north
