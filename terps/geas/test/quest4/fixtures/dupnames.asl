' Two objects may share a name.  Quest keeps one record per "define object", so
' each has its own room, but almost every lookup in the engine is by name and
' answers for whichever record was defined first (GetObjectID, V4Game.cs).  This
' fixture pins down which of those lookups must be per-record and which must
' stay per-name.
'
' The corpus case is The Hobbit (Nathan Lindsell, ASL 210), which defines a
' "ring" in three consecutive rooms, a "map" in three, a "food" in three and a
' "Pipe" in two -- QDK 1.0 duplicated the scenery instead of moving it.  Listing
' a room's contents by name put all three rings in the first of those rooms and
' none in the other two, and the inventory reported one ring per definition after
' taking any of them.  regen_var_objects and get_room_contents therefore start
' their container walk from each record's own parent (room_of_parent) rather than
' from its name (room_of).
'
' What must NOT change, because Quest is name-keyed here too:
'
'   * "move"/"give"/"lose" and the built-in TAKE name an object and act on the
'     first record with that name.  So TAKE RING in the east room prints the
'     east ring's own take message -- the parser resolved the noun in scope --
'     and then moves the *west* ring into the inventory: the east room still
'     lists its ring afterwards and the west room no longer lists its own.  That
'     is what Quest does, and the last four lines of the .cmd pin it.
'
'   * A property lookup by name is scope-aware enough to answer for the object
'     in front of the player: LOOK AT RING gives "A plain ring." in the west
'     room and "A jewelled ring." in the east one.
'
' The lantern pair covers the container case: one lantern is nested in a chest in
' the west room and the other stands loose in the east room.  room_of_parent has
' to walk up through the chest to reach West, and must not credit the east
' lantern to West on the way.  The chest is `opened` but not yet `seen`, so its
' contents stay hidden until the first LOOK AT CHEST -- which is why the opening
' room description lists no lantern and the next one does.

define game <Dupnames>
 asl-version <210>
 start <West>
end define

define room <West>
 look <The west room.>
 east <East>

 define object <ring>
  prefix <plain>
  look <A plain ring.>
  take <You take the plain ring.>
 end define

 define object <chest>
  prefix <wooden>
  container
  opened
 end define

 define object <lantern>
  prefix <dented>
  parent <chest>
 end define
end define

define room <East>
 look <The east room.>
 west <West>

 define object <ring>
  prefix <jewelled>
  look <A jewelled ring.>
  take <You take the jewelled ring.>
 end define

 define object <lantern>
  prefix <polished>
 end define
end define
