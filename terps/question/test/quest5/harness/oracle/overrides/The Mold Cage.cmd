# The Mold Cage (Michael Fairhurst, 2013): the win (state=Finished). The start script ends in an empty
# `CloneObjectAndMove ( , )`, so the opening logs one script error in the oracle (and in ours). Route: the custom verb
# `challenge for title of moldest wizard <object>` → thrown into the Mold Cage Entrance; `get tired of how much mold there
# is even more mold` creates the exit to the Maple Syrup Factory; looking at the workers (a `wait`) reveals the Self
# Surgery Room; the Surgery Machine ("You die. Therefore you are out of the Mold Cage!! You Win!!") calls `finish`.
# Side branches walked: the throne room speak, the factory workers' speak (reveals the Dinosaur), the Rec Center (Neo and Trinity stay invisible: the exit's `look` is a script, which `look east` does not run), the Rec Center with Neo and Trinity.
look
x moldest wizard
speak to moldest wizard
challenge for title of moldest wizard moldest wizard
look
get tired of how much mold there is even more mold
go to an opening into the mold cage maple syrup factory
look
speak to factory workers
answer:no
look east
east
look
west
look at factory workers
go to the mold cage self surgery room
look
use surgery machine
