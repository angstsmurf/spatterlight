# I Contain Multitudes (Wonaglot, IF Comp 2021) -- BEST-EFFORT SCRIPT
#
# RESULT: state=Running (genuine [state=Finished] is UNREACHABLE under the headless oracle).
#
# WHY FINISHED IS UNREACHABLE (the author literally warns of this in the walkthrough:
# "because ... some events are time based, the walkthrough function of quest does not work"):
#
#   The headless oracle drives the game with SendCommand(cmd) => elapsedTime = 0 and never
#   subscribes to RequestNextTimerTick, so game.timeelapsed NEVER advances (verified: it is
#   still 0 after 200 turns, and the pre-existing "tomyris move" timer, interval 120, never
#   fires -- Tomyris never appears in the Dark Room no matter how many turns pass).
#
#   Every route to a genuine win therefore breaks:
#     * Tomyris' entire questline (dark room -> film -> Strange Photographs) is gated by the
#       clock timer at game.aslx:12835 (interval 120). It never fires.  Because of this, the
#       normal endgame flag player.knowsp (set only in Tomyris' dialogue, game.aslx:4637/4653)
#       is never set, so `go down` the hatch is refused ("There is no way to go down.").
#     * The ONLY `finish` that is a real win is inside the Ending room's <enter> script
#       (game.aslx:6643).  You reach Ending exclusively via MoveObject(player, Ending), and
#       EVERY such call is nested inside a SetTimeout(...) block created at the Heart
#       (game.aslx:2066/2080/2096/2127/2141/2169/... ).  SetTimeout creates a clock timer
#       (Core CoreTimers.aslx:19-47); with game.timeelapsed frozen it can only fire via the
#       engine's `on ready` flush.  Empirically it DOES cross the player into the Ending room,
#       but the Ending <enter> script sits behind OnEnterRoom's nested `on ready` chain
#       (CoreDescriptions.aslx:153-199); in the deep timer/wait/menu continuation the pending-
#       callback count never returns to 0, so the enter script (hence `finish`) never runs.
#       (Verified: after entering Ending, 20 further turns never emit the ending montage.)
#
# HOW FAR THIS SCRIPT GETS (all reachable content, zero script errors, no menu desync):
#   - Full Prologue.
#   - Arania's quest COMPLETED (libretto repaired + returned, Suite Three singing scene).
#   - Charlotte / Hartleford / photo quests STARTED (their continuations are timer-gated).
#   - Collects: whisky, torn page, fillet knife, libretto, film canister, Tongue of Man, lantern.
#   - Bypasses the timer-gated Tomyris hatch route: takes the Metal Key straight from the
#     (walkable) Captain's Cabin desk, unlocks the footlocker for the Spanner, and uses the
#     author's own spanner path (`open hatch` -> Descend, game.aslx:1561-1571) to reach the
#     final room UNKNOWN without player.knowsp.
#   - Faces the Heart and plays the genuine climactic choice: option 3 "Speak to her"
#     (enabled by the Tongue of Man), then "Are you Anna Alexia?" -> the self-revelation
#     "I am Chandra Fitz...".  This is the last real story beat before the (unreachable)
#     SetTimeout(7) -> Ending montage -> finish.
#
# DEVIATIONS FROM THE PROSE/COMMAND WALKTHROUGH:
#   - Skips the entire Tomyris/Dark-Room/photograph arc (impossible headless -- see above).
#   - Gets the Metal Key by walking to the Captain's Cabin instead of receiving it from the
#     Captain after giving him the (unobtainable) Strange Photographs.
#   - Descends via `open hatch` (spanner) instead of `go down` (which needs player.knowsp).
#
# --- Prologue (Captain's Cabin): No / How long / Where do I begin ---
2
2
2
# --- Crew Quarters: take the four masks ---
take devil
take cherub
take widow
take bauta
e
up
# --- Charlotte's quest start (Mid Deck) ---
talk charlotte
1
1
# --- Whisky for Charlotte (Saloon) ---
up
ne
order a drink
# --- Arania's quest start ---
talk arania
1
# --- Hartleford's quest start (Dining Room); his library arrival is a real-time timer ---
sw
in
talk hartleford
1
# --- Kitchen: fillet knife + torn page ---
w
take knife
n
search cabinets
s
e
out
d
# --- Game Parlor: give Charlotte the whisky ---
se
give bottle to charlotte
# --- Library: take + repair the libretto ---
nw
w
take libretto
put torn page in libretto
# --- Gymnasium: film canister (needs the Blithe Cherub worn) ---
e
e
wear cherub
look under horse
remove cherub
# --- Return the repaired libretto to Arania -> unlocks Suite Three ---
w
up
ne
give libretto to arania
# --- Suite Three: Arania's singing scene (best-result lines 3 & 3), then the follow-up ---
sw
s
sw
talk arania
1
3
3
talk arania
1
ne
n
# --- Endgame collection (Tomyris/photo route is timer-gated; take the Metal Key from the
#     walkable Captain's Cabin instead) ---
up
up
se
open walnut desk
take metal key
nw
d
d
d
d
w
unlock footlocker
take spanner
# --- Brig: cut out the Bishop's tongue (Hungry Devil worn + fillet knife) ---
e
n
wear devil
remove bishop's tongue
# --- Engine Room: get the lantern from Howard ---
s
s
ask howard about lantern
# --- Storage: open the sealed hatch with the spanner and descend (author's spanner path) ---
n
e
look down
open hatch
1
# --- UNKNOWN (final room): the Heart. Genuine climactic choice ---
look at the heart
3
1
# The next beat -- SetTimeout(7) -> MoveObject(player, Ending) -> Ending <enter> -> finish --
# cannot fire headless (see header). State ends Running at the self-revelation.
