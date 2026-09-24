# Dr. Science Mouth (Farley Sweet, 2016, ASL v550) — WINNING walkthrough,
#   override-only (no published walkthrough exists). Halloween-night
#   horror/abduction game set in the Sampton Warehouse, with a huge branching
#   map full of "shadow object" duplicates (Nurse Namanda1-4, Dr Science
#   Mouth1-6, wonder ball/wonder ball1, locker/locker1, etc.) that get
#   swapped in via RemoveObject/MoveObject depending on earlier choices.
# Ending: escape the warehouse via the Surgery Room's broken window, then
#   outrun Dr. Science Mouth to Street Corner2 where a police officer shoots
#   him dead — "you have survived a truly frightening Halloween." Oracle
#   reports state=Finished errors=0, steps=32, no parser errors anywhere in
#   the transcript (deterministic, no RNG). Many side branches call a bare
#   `finish` on instant/near-instant death (the `tackle` verb always fails;
#   hiding under/in most Dental Room, Surgery Room and Bedroom furniture
#   leads to the shared "Dental Dungeon" bad-ending room; the Smoky Room
#   maze's northeast branches and the machine's "flip switch" both lead to
#   death; the Oversized Nursery's giant crib is an instant-kill trap the
#   red knight in Fancy Hall explicitly warns about) — this script avoids
#   all of them by taking the shortest verified-safe route instead of
#   exploring the deeper Nursery/Office/Bunkroom/Dental-Dungeon2 wing.
# Route: `north` to the Field; `ignore cat` (the black cat's safest of 3
#   options — "pet"/"kick" both swap in extra puzzle-object duplicates
#   elsewhere, "kick" specifically swaps in lethal trap variants of the
#   wonder ball/locker/bench seen later). `northwest` to the Parking Lot;
#   `pull rusted door` (unlocks the "in" exit, no need to investigate the
#   bushes/hole side route); `in` to Cluttered Hall; `north` twice to reach
#   the Waiting Room then Fancy Hall; `hug large man` reveals Dr. Science
#   Mouth's identity and unlocks the west exit. `west` to the Dental Room;
#   `sit on dentist chair` triggers a trap-then-rescue by Nurse Namanda,
#   elevating you to the Elevated Dental Room (the alternate `hide in
#   cabinet` route leads to the same room but is otherwise equivalent — sit
#   was chosen arbitrarily as the simpler single action). `east` twice
#   through Laboratory to the X-Ray Room, `south` to the Construction Room
#   where Dr. Science Mouth ambushes you — `throw wire` electrocutes him
#   (the equally-safe alternative is `push step ladder`; `tackle` is always
#   lethal) and unlocks both exits. `southeast` to the Narrow Hallway,
#   `southwest` to the Museum, `west` to the Bedroom; `investigate desk`
#   grants the key (do not touch the wonder ball — grabbing it is safe in
#   this branch but pointless). `east`/`northeast` back to the Narrow
#   Hallway, `unlock door` with the key, `south` to the Library Balcony (do
#   NOT read the green book — it's a floor-trap leading straight to the
#   Dental Dungeon), `down` to the Library, `east` to the Dining Room, `east`
#   into the Smoky Room maze. From the entrance: `east` to the room with the
#   machine (do NOT flip its switch — that's a death trap), `southeast` to
#   the exit room, `east` into the Surgery Room (do NOT hide under/in any of
#   its furniture — all lead to the Dental Dungeon). `jump from broken
#   window` escapes to the Parking Lot with a sprained ankle; `southeast`
#   to the Field, `south` to Street Corner2 where the officer kills Dr.
#   Science Mouth; `enter vehicle` seals the winning ending.
north
ignore cat
northwest
pull rusted door
in
north
north
hug large man
west
sit on dentist chair
east
east
south
throw wire
southeast
southwest
west
investigate desk
east
northeast
unlock door
south
down
east
east
east
southeast
east
jump from broken window
southeast
south
enter vehicle
