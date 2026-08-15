' `lose <obj>` is "put this object in the room I am standing in", not "drop it
' if I am holding it".  Quest's PlayerItem (V4Game.Part2.cs:6610-6660) looks the
' name up in the object table and calls MoveThing (..., _currentRoom, ...)
' followed by the object's lose script, with no test of where the object was --
' the mirror image of `give`, which has never had one either.
'
' Geas used to guard both on the object's parent being "inventory", so a `lose`
' aimed at something lying in another room did nothing at all.  Sutekh depends
' on the Quest behaviour: kissing the statuette does `lose <brass lantern>` to
' yank the lantern in from a room two away, which is how the player is meant to
' find it.
'
' So: spade starts in the shed and must arrive in the hall with its lose script
' run, fork must arrive silently, and lamp -- carried -- must still work.

define game <Loseanywhere>
 asl-version <410>
 start <Hall>
 command <one> lose <spade>
 command <two> lose <fork>
 command <three> lose <lamp>
 ' The lamp is handed over at the start rather than declared with a bare
 ' `parent <inventory>` and no room of its own: QuestViva crashes on such an
 ' object before the game starts, and then the oracle cannot check this
 ' fixture at all.
 startscript give <lamp>
end define

define room <Hall>
 look <The hall.>
 define object <rock>
  prefix <a>
 end define
 define object <lamp>
  prefix <a>
  lose msg <You put the lamp down.>
 end define
end define

define room <Shed>
 look <The shed.>
 define object <spade>
  prefix <a>
  lose msg <The spade clatters down.>
 end define
 define object <fork>
  prefix <a>
 end define
end define
