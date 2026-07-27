' Block names are case-insensitive in Quest: every lookup goes through an
' LCase-to-LCase comparison (GetRoomID, GetObjectIDNoAlias, GetThingNumber and
' friends), so a game may spell a name one way in `define` and another way when
' it refers to it.  geas kept its name -> block-type map keyed by the exact
' spelling, so a mismatch found the block (find_by_name folds case) but reported
' it as having no properties, no actions and no type at all.
'
' The Birthday Assignment does exactly this: `define room <EDock>` reached by
' `goto <Edock>`.  The room was entered and described, but its script -- which
' prints the ending and calls playerwin -- never ran, so the demo just stopped
' in an empty room.
'
' Each of the checks below names something with a spelling that differs from its
' definition, covering the geasfile.cc entry points in turn: get_obj_property,
' obj_of_type and get_obj_action (the room script).  Typing the object name in
' the wrong case at the prompt already worked, because the verb matcher resolves
' the word to the defined spelling before looking the action up -- it is the
' scripted names that went astray.  Without the fix the room's own `look` text
' vanishes too, since that is a property lookup like any other.

define game <Namecase>
 asl-version <410>
 start <Lobby>
end define

' Inside a type block a property is a bare line, not a `properties <...>` list.
define type <Brass>
 shiny
end define

define room <Lobby>
 look <The lobby.>
 ' "EDock", defined below, spelled three different ways
 command <hop> goto <edock>
 command <probe> {
  if property <LAMPpost; polished> then msg <[property: polished]> else msg <[property: NOT polished]>
  if property <lamppost; shiny> then msg <[property: shiny, inherited]> else msg <[property: NOT shiny]>
  if type <LAMPPOST; brass> then msg <[type: brass]> else msg <[type: NOT brass]>
 }

 define object <LampPost>
  type <Brass>
  look <A brass lamp post.>
  properties <polished>
  action <rub> msg <[action: rubbed]>
 end define
end define

define room <EDock>
 look <The dock.>
 script msg <[room script ran]>
end define
