' At ASL 4.10 the exits are one folded list and one variable.  UpdateDoorways
' hands the whole job to RoomExits.GetAvailableDirectionsDescription and sets
' quest.doorways alone (V4Game.Part2.cs:7207-7211); the older
' quest.doorways.dirs, .places, .out and .out.display are written only in the
' branch below 4.10, and ShowRoomInfo's places half likewise (ibid. 3838-3874).
' A 4.10 game that prints one of the old ones therefore gets nothing at all.
' Nearco's "Puedes ir al: |b#quest.doorways.dirs#|xb" is a bare label in Quest,
' and geas used to fill it with the full compass.
'
' The folded list is not the old lists concatenated, either.  A place in it
' reads "to " + the destination room's own prefix + the bold name
' (GetDirectionNameDisplay, RoomExits.cs:522-528, with the prefix copied from
' _rooms[GetRoomId()].Prefix by RoomExit.cs:225-236) -- the destination's
' prefix, not the one written on the place tag -- 4.10 has no prefix field on
' the tag at all, its second parameter being the lock message.  The truck has a
' room prefix and the shed has none, which is the contrast.  "out" is in the
' list too, as a bare word, in the order the room's tags created it.
'
' doorways400 is the same game one version down.
define game <Doorways410>
 asl-version <410>
 start <Hall>

 command <dump> {
  msg <folded [#quest.doorways#]>
  msg <dirs [#quest.doorways.dirs#]>
  msg <places [#quest.doorways.places#]>
  msg <out [#quest.doorways.out#] display [#quest.doorways.out.display#]>
 }
end define

define room <Hall>
 look <A hall.>
 north <Kitchen>
 out <Yard>
 place <Work Truck>
 place <Shed>
 up <Loft>
end define

define room <Work Truck>
 prefix <A>
 look <A truck.>
end define

define room <Shed>
 look <A shed.>
end define

define room <Kitchen>
 look <A kitchen.>
end define

define room <Yard>
 look <A yard.>
end define

define room <Loft>
 look <A loft.>
end define
