' "goto" to a room that does not exist.
'
' PlayGame looks the name up with GetRoomID and gives up if it finds nothing
' (V4Game.Part2.cs:6674-6684):
'
'     var id = GetRoomID(room, ctx)
'     if id = 0 then
'         LogASLError("No such room '" & room & "'", WarningError)
'         return
'     _currentRoom = room
'
' so the player does not move, nothing is printed to the transcript (the error
' goes to the log), and -- because ExecuteScript just awaits PlayGame and carries
' on (ibid. 5786-5789) -- the rest of the script still runs.
'
' GetRoomID compares against RoomName only (V4Game.cs:6348-6365).  A room's
' "alias" is what the *player* sees and what "go to" matches; it is never a goto
' target.  Nor is an object's name, however much it looks like a place.
'
' geas moved regardless, which invented a room out of the name it was handed.
' Rooms and objects share one property table, so when the name belonged to an
' object the phantom room was described with that object's prefix and look text:
' "pure chaos" (game.asl, ASL 400) ends with `goto <bed>` followed by `goto
' <bedtied>`, where "bed" is an object in the bedroom and the room actually meant
' carries `alias <bed>`, and the win printed "You are in a bed / It's tiny!"
' before finding its way to the real room by the second goto.
'
' The one way a room can exist without being declared is "create room", which
' appends an empty entry to the same _rooms array GetRoomID scans (ExecuteCreate,
' V4Game.cs:5244-5276) -- no description, no exits, nothing but a name that goto
' now accepts.  That is what the "attic" command tests.

define game <Gotonoroom>
 asl-version <400>
 start <Hall>
end define

define room <Hall>
 look <The hall.>

 command <object> {
   goto <bed>
   msg <Still here.>
 }
 command <alias> goto <bunk>
 command <real> goto <Bedroom>
 command <attic> {
   create room <Attic>
   goto <Attic>
 }

 define object <bed>
  look <It's tiny!>
  prefix <a>
 end define
end define

define room <Bedroom>
 alias <bunk>
 look <The bedroom.>
 command <back> goto <Hall>
end define
