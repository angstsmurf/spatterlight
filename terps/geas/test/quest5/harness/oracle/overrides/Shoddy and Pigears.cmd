# Shoddy and Pigears (ASL v580); no published walkthrough exists -- derived entirely
# from source (game.aslx, 10544 lines). Comic Toytown adventure: Shoddy the teddy
# bear must rescue his friend Pigears from pirates who have snatched him away to
# Arkville aboard a train, after a burglary-themed prologue back home. Sole win:
# `light fuse` on the cannon's fuse in The stern chaser once the jacket has been
# set alight (`Got(Jacket burning)`) -- fires the cannon into Pigears' cell and
# calls `do_end` / `finish`, "THE END".
#
# `hidechildren` footgun (hit three times): the Jacket in the Wardrobe needs
# `look at jacket` before its pocket Coin is takeable (taking the coin also
# auto-adds the jacket itself to inventory via the coin's own `<ontake>`); the
# stern chaser's Cannon needs `look at cannon` before its Fuse resolves by name;
# the Ark's Model boat needs `open model boat` (its firsttime `<take>` script only
# reveals/unhides the nested Glue and adds an "Open" verb -- it does NOT actually
# put the boat itself in inventory) before `take glue` works.
#
# Visibility-gate footgun: the Commanders office's Wallpaper1 object is
# `<visible>false</visible>` by default and is only flipped visible as a script
# side-effect of `take calendar` in the same room -- not discoverable by reading
# Wallpaper1 in isolation, only by reading Calendar's `<take>` script.
#
# Parser footgun: the built-in `unlock` verb's pattern is `<pattern>unlock
# #object#</pattern>` -- single-object only, no "with X" clause. "unlock
# magazine door with bronze key" fails with the generic DefaultMultiObjectVerb
# "That doesn't work."; the correct form is the bare "unlock magazine door" (the
# engine auto-applies the door's declared `<key>` from inventory). Unlocking the
# magazine door also auto-walks the player through it via the door's own
# `onunlock` script (`UnlockExit` + `MoveObject(player, ...)`), so no separate
# movement command follows it.
#
# Exit-visibility footgun (Sewer pipe, in the Arkville sewers): its `up` exit to
# East harbour is tagged `<visible>false</visible>` and -- unlike some other
# invisible-but-usable exits in this game -- is genuinely blocked ("You can't go
# there.") when typed directly. The working return route from the sewers to East
# harbour is `east` (to Below the harbour wall) then `climb ladder` (the ladder's
# `<climb>` script relocates both itself and the player to East harbour).
#
# Puzzle chain (traced from source): pinch the neighbour's ladder from the alley
# shed, prop it against the house and climb to Pigears' bedroom for the Jacket +
# Coin; buy a chocolate bar at the sweet shop; retrieve a False beard from a
# coffin under the trapdoor behind the police station and wear it (required or
# the pawnbroker Gusty recognises and ejects you); give the chocolate bar to the
# toy-fort sentry to get in; in the Commanders office, take the calendar (reveals
# the wallpaper) then take the Bronze key hidden behind it, and take the Medals
# off the table; unlock the magazine door with the bronze key (auto-walks you
# through); pawn the medals to Gusty for cash; buy a ticket by speaking to Hector
# at the turnstile while carrying the ten-pound note (auto-boards you); ride the
# train (last/middle/front carriage, then the compartment) to Arkville, where
# pickpockets take most of your inventory. At the Ark, open the Model boat in
# the Workshop for Glue, then burn the (now-empty) Jacket in the Ark cabin's
# fireplace for "Jacket burning"; use the glue on the Keyhole in the Back
# passage, which triggers a riot back at Arkville Harbour, unlocking the pub's
# Bucket; collect it, fill it with manure in Ark muck, carry it into Under the
# gangway and drop it there -- the stench routs the pirate guard, unlocking the
# main deck. Cross the pirate ship to The stern chaser, look at the cannon to
# reveal its fuse, and light the fuse (with the burning jacket in hand) for the
# win.
out
down
south
east
take ladder
west
north
north
drop ladder
climb ladder
west
look at jacket
take coin
out
down
east
south
south
west
west
south
buy chocolate bar
north
west
north
down
open coffin
look at mac the spoon
take false beard
wear false beard
up
south
east
north
give chocolate bar to sentry
north
west
take calendar
look at wallpaper
take bronze key
east
unlock magazine door
look at table
take medals
out
south
south
east
east
east
north
give medals to gusty
south
west
west
north
speak to hector
north
west
west
north
south
north
north
north
west
west
take model boat
open model boat
take glue
east
burn jacket
east
south
west
west
west
use glue on keyhole
east
east
east
west
take bucket
east
north
down
west
take manure
east
up
south
east
down
north
drop bucket of manure
south
east
climb ladder
north
east
down
east
look at cannon
light fuse
