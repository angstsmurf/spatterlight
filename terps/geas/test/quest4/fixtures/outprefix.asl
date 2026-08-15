' "out <the; Yard>" -- the prefix half of an out exit, which Quest puts back in
' front of the destination only under conditions.  UpdateDoorways splits the
' parameter at the semicolon and immediately rejoins it as "prefix name"
' (V4Game.Part2.cs:7218-7227); that joined string is what the player sees when
' the destination has no alias.  When it has one, the alias replaces the whole
' thing, and the prefix is put back only from ASL 3.60 on
' (ibid. 7294-7305):
'
'     no alias      -> "the Shed"    at every version
'     alias, >= 360 -> "the back yard"
'     alias, <  360 -> "back yard"   -- the prefix is gone
'
' geas reattached the prefix in all three cases.  King's Quest V (350) is the
' corpus game: it goes `out <the; small country inn -outdoors>' to an aliased
' room, and Quest's line has no "the" in it.  See outprefix350.asl for the same
' rooms below the gate.
define game <outprefix>
 asl-version <400>
 start <Hall>
end define

define room <Hall>
 look <A hall.>
 out <the; Yard>
end define

define room <Yard>
 alias <back yard>
 look <A yard.>
 out <the; Shed>
end define

define room <Shed>
 look <A shed.>
end define
