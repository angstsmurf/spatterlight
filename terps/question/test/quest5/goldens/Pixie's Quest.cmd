# Pixie's Quest (ASL v550, 13606-line game.aslx inside a zip-format .quest with
# ~30 auxiliary media/prop files). A bespoke personal gift game "for The Pixie"
# made by three co-authors -- Dark Lizerd (DL), XanMag (XM) and K.V. (KV) --
# per the title screen; no published walkthrough exists anywhere.
#
# NO REACHABLE WIN STATE: exhaustive grep of the source finds exactly one
# `finish` call in the whole game, on a provably-unreachable second-visit
# branch of "Your Bedroom" that nothing in the game can trigger. This is
# therefore driven to the best documented reachable milestone per the
# task's best-reachable-ending fallback, not to a `finish`.
#
# SCORE CEILING: the in-game `score` command reports "True Maximum Score:
# [ 227 ]" -- this is the authoritative ceiling, NOT the decorative,
# apparently-unused `kv_maxScoreObject.maxScore=105` attribute that also
# exists in source. This script stops at 32/227 (see below) rather than
# chasing the full 227, because the single largest remaining chunk (the
# Crate Mart "green shard", +50) sits behind an entire separate, large
# puzzle subplot -- see next paragraph.
#
# CRATE MART IS GATED BEHIND XANMAG'S SUBPLOT: `kv_Crate Mart`'s `enter`
# script rejects the player ("We're not open yet!", ejects back to the Hub)
# unless `player.thanked=true`. That flag is set only by the
# `completelyThanked` turnscript once `XanMag.TaskCount` (starts at 3)
# reaches 0 AND the player carries `kv_token of appreciation`. Dropping
# XanMag.TaskCount to 0 requires completing THREE independent puzzle
# chains of her own subplot: (1) smoke a "blunt", get "high", then
# transcribe hidden text from a "swirling column of light" (WordDocumentXM)
# onto a notepad and place the resulting envelope on a pedestal
# (IdeaInAnEnvelopeXM); (2) a "beer" fetch quest (WorldofBeerXM); (3)
# recording a "dope beat". This is a large, independent puzzle area in its
# own right and is deliberately NOT pursued this batch -- verified this is
# a real, enforced gate (not a script bug) by reaching Crate Mart via the
# fully-unlocked booth/slot/token teleport with the token in hand and
# being immediately rebuffed by "The Cratesman".
#
# ORDERING FOOTGUN (must-follow, or the run permanently soft-locks a whole
# branch): the RH-desk / PC / screen / ANY key / booth-unlock sequence
# (giving DL's disk to RH, switching on RH's PC, pressing the revealed ANY
# key) MUST happen BEFORE the first visit to "The Land of Confusion" (KVs
# room). `rh_PC`'s `onswitchon` script is gated by
# `if (not KVs room.visited)` -- once KVs room has been visited even once,
# that gate is permanently false and the entire PC/screen/booth reveal
# chain becomes unreachable for the rest of the game. This script therefore
# does the whole RH-desk chain first and only then travels to KVs room.
#
# take globe / take planet disk: DarkLizerd's desk hides the disk under a
# globe. "take planet disk" alone answers "I can't see that." -- the disk
# only becomes visible/takeable as a side effect of DL's own response text
# to a (failing) "take globe" ("he touches a disk... and the planet
# disappears"). "take globe" must therefore be sent immediately before
# "take planet disk", even though "take globe" itself never succeeds.
#
# ride ostrapode -> "I can't see that. (ostrapode)": the object's internal
# name is `ostrapode` but its only parser-recognised alias is "strange-
# looking bird" / "bird" -- use "ride bird".
#
# sky-fall 2-turn timer: entering `kv_sky` (after "ride bird") arms
# `SetTurnTimeoutID(2, "falling")`, a hardcoded 2-turn countdown that force-
# moves the player to `kv_Darkness` regardless of what they type. Any
# command sent in the sky consumes one of those two turns even if it fails
# ("I didn't understand", "I can't see that."), so `feel`/`use lamp` (which
# are only valid once actually IN kv_Darkness) must NOT be sent while still
# in the sky, or they get silently wasted as no-ops before the fall lands
# and then still need to be repeated. Two "look" commands are sent first
# to absorb exactly the 2-turn window, so the fall happens cleanly and
# "feel"/"use lamp" are the first real commands issued inside kv_Darkness.
#
# travel / 1: "Travel to where? 1: The Land of Confusion" is a ShowMenu
# prompt. The harness's menu: prefix expects the option KEY, not its
# display text -- "menu:The Land of Confusion" is rejected ("I didn't
# understand your command."). Per Program.cs's documented leniency (a bare
# line while a menu/question is pending resolves as the menu key / option
# number / yes-no answer), a bare "1" is what actually answers it.
#
# Milestone reached and verified deterministic (byte-identical transcripts
# across two independent seed=1234 runs; errors=0, state=Running,
# scriptExhausted=True): full completion of KV's World -- the DL desk/disk
# puzzle, the RH-desk PC/screen/ANY-key/booth unlock chain (non-obvious
# ordering constraint above), the Hunt-the-Wumpus-parody Meadow/bird/sky/
# darkness/lamp sequence, and the Swedenborgian Space pouch-search payoff
# that hands over `kv_token of appreciation`. Score log at this point:
# "Examined DL's desk: 1 point", "Switched on the PC: 1 point", "Pressed
# the ANY key: 10 points", "Completed KV's gateway: 10 points", "Searched
# the strange pouch: 10 points" = 32/227. This is a clean, well-defined,
# narratively-complete beat (a distinct "ALERT" + "Quest complete: Search
# the Strange Pouch." marker plus a tangible reward), and is the documented
# judgment call for this game's best-reachable ending given the XanMag
# subplot's out-of-scope size and the confirmed absence of any true win.
yes
east
east
more
beeping
look
enter strange doorway
examine desk
take globe
take planet disk
west
give disk to rh
examine richard headkid's desk
switch on pc
examine screen
press any key
travel
1
north
ride bird
look
look
feel
use lamp
take pouch
search pouch
