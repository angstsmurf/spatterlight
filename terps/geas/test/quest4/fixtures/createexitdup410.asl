' Creating a "go to" exit that already exists.
'
' Quest never lists the same place twice, but the two ASL generations get there
' by different routes:
'
'  * Below 4.10, ExecuteCreateExit walks the room's Places array looking for the
'    destination (LCase'd on both sides) and, if it finds it, logs
'    "Exit from 'X' to 'Y' already exists" and returns without doing anything
'    (V4Game.cs:5385-5409).  The existing place is kept, in its existing spot.
'
'  * From 4.10 on the places are a Dictionary keyed by destination room name, so
'    AddPlaceExit removes any entry already under that key and then inserts the
'    new one (RoomExits.cs:46-56).  The exit is *replaced*, and because the
'    dictionary is walked in insertion order it moves to the end of the listing.
'    The key is compared case-sensitively, so a differently-cased destination is
'    a different key and really does get its own second entry.
'
' The 4.10 exists-check in ExecuteCreateExit cannot fire, incidentally: destRoom
' is only filled in on the `ASLVersion < 410` path above it, so the 4.10 branch
' asks the dictionary for "" and always gets false.  AddPlaceExit is the whole of
' the 4.10 rule.
'
' Both versions count *statically declared* places too -- they are added by the
' same AddExitFromTag/Places code the create script uses -- so `place <Cellar>`
' in the room definition is enough to make a later `create exit <Hall; Cellar>`
' a duplicate.  And `destroy exit` genuinely removes the place, so re-creating
' it afterwards is allowed again.
'
' The corpus case is DJay32's "Metroid" Lite (ASL 400): Boardroom East's
' `place <Hallway>` script runs `create exit <Hallway; Hidden Secrets>` every
' single time you walk into the hallway carrying the ice weapon, and geas -- which
' just appended -- answered "You can go to Piperoom, Boardroom, Storage Room, or
' Storage Room."
'
' createexitdup.asl is this same game at ASL 400.

define game <Createexitdup410>
 asl-version <410>
 start <Hall>
end define

define room <Hall>
 look <The hall.>
 place <Cellar>

 command <mkattic> create exit <Hall; Attic>
 command <mkloft> create exit <Hall; Loft>
 command <mkcellar> create exit <Hall; Cellar>
 command <mklowercellar> create exit <Hall; cellar>
 command <rmattic> destroy exit <Hall; Attic>
end define

define room <Cellar>
 look <The cellar.>
 place <Hall>
end define

define room <Attic>
 look <The attic.>
 place <Hall>
end define

define room <Loft>
 look <The loft.>
 place <Hall>
end define
