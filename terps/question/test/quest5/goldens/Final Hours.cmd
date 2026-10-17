# Final Hours (v1.5, Ernest Jr): the furthest reachable state (state=Running). The game stops after the opening: living
# room (candle lights it), kitchen (burning toaster, food supply and its bag of nutrients), bathroom (bath, cupboard with
# first aid kit, pistol and hidden blade; bandages/tissues/plaster heal), garden, and `climb over fence` to the Streets,
# which has no description. `walk to` / `walk closer destroyed plane` knocks the player out, and the 20 s timeout wakes
# them in the plane "with a man" -- but the Man is a child of the plane, which is not a container, so he is out of
# scope ("I can't see that.") and the `teamup`/`stab`/`ask` branches are unreachable. No ending exists.
look
light candle
x phone
take phone
x candle
south
x toaster
x food supply
open food supply
x bag of nutrients
eat bag of nutrients
north
x bath
switch off bath
x cupboard
open cupboard
x first aid kit
open first aid kit
x bandages
x pistol
x hidden blade
inventory
north
look
climb over fence
x plane
walk to destroyed plane
walk closer destroyed plane
look
x man
