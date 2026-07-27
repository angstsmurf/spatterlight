' TAKE out of a container is REMOVE followed by TAKE, and the container gets a
' veto.
'
' From ASL 3.91 "remove" is the container half of "put": ExecAddRemove
' (V4Game.cs:2369-2624) resolves the container -- the one named after "from", or
' whatever the item's own `parent` property says -- and then hands the decision
' to *it*, never to the item.  A `remove` action is trusted to do the removal
' itself, and is told which item it was asked for through
' quest.remove.object.name; a `remove` text property is printed and the engine
' then clears the item's parent property for it (DoAddRemove, V4Game.cs:2113);
' and a container that declares neither answers "You can't remove that."
' (PlayerError.CantRemove, V4Game.Part2.cs:449).  A bare `remove` line is a
' property with no text, which prints "Done." -- and that is the line QDK writes
' on every container it builds whose contents are meant to come out again.
'
' TAKE leans on all of that.  For a *text or default* take of an object that has
' a parent, ExecTake prints "(first removing <article> from <container>)", runs
' the REMOVE command on the player's behalf, and abandons the take if the object
' is still parented afterwards (V4Game.Part2.cs:5178-5204).  A *scripted* take is
' exempt: the script is on its own, and the object stays parented (which is how
' Barbarian's sack-on-the-pedestal trap survives being taken apart -- see
' containerparent).
'
' QuestViva never reaches that block.  Its ExecTake reads the parent into
' parentID and then leaves isInContainer false forever, so nothing in the port
' ever removes anything on the way to taking it; the missing line is the one its
' own ExecDrop still has,
'
'     if IsYes(GetObjectProperty("parent", id, true, false)) then isInContainer = true
'
' (V4Game.cs:5560-5570), and Quest itself plainly did the removal: QDK's bare
' `remove` line would otherwise be dead weight, and Tim Hamilton's "The Things
' That Go Bump In The Night" (ASL 410) cannot be finished without it.  Its water
' pump wants four dead fuses out of the fuse box and a good one in, and the box's
' `add` script counts what is still parented to it:
'
'     for each object in game if is <#(quest.thing):parent#;Fuse Box> then inc <objectcheck>
'     if is <%objectcheck%;gt=;4> then msg <There's no room. The box is full of fuses.>
'
' geas took the object and left the property, so the box stayed full of fuses it
' no longer held.
'
' The four containers below cover the four answers a container can give, and the
' scripted take covers the exemption.  (The jar's `is` test on the item name is
' the shape that turned up the operator-splitting bug pinned by iscompare: the
' "lt;" inside "bolt;" used to be read as a less-than.)  Note that the *item* is what a refusal is
' reported about (its article), and that a refused take leaves the item where it
' was -- "check" reads the properties back out afterwards.

define game <TakeFromContainer>
 asl-version <410>
 start <Pantry>

 command <check> {
  msg <coin:[#coin:parent#] pin:[#pin:parent#] nut:[#nut:parent#] bolt:[#bolt:parent#] key:[#key:parent#]>
 }
end define

define room <Pantry>
 look <A pantry lined with shelves.>

 ' A bare "remove": permitted, and the default "Done." is printed.
 define object <tin>
  look <A biscuit tin.>
  container
  opened
  remove
 end define

 define object <coin>
  look <A coin.>
  take
  parent <tin>
 end define

 ' A "remove <text>" property: the text is printed and the engine clears the
 ' parent property itself.
 define object <box>
  look <A cardboard box.>
  container
  opened
  remove <You fish it out of the box.>
 end define

 define object <pin>
  look <A pin.>
  take
  parent <box>
 end define

 ' A "remove" *action*: it is handed quest.remove.object.name and does the
 ' removal itself.  This one refuses one of its two contents, which stops the
 ' take of that object dead while the other still comes out.
 define object <jar>
  look <A screw jar.>
  container
  opened
  remove {
   if is <#quest.remove.object.name#; bolt> then msg <The bolt is rusted into place.>
   else {
    msg <You pick the #quest.remove.object.name# out of the jar.>
    remove <#quest.remove.object.name#>
   }
  }
 end define

 define object <nut>
  look <A nut.>
  take
  parent <jar>
 end define

 define object <bolt>
  look <A bolt.>
  take
  parent <jar>
 end define

 ' No remove of any kind: nothing comes out of this one.
 define object <urn>
  look <A sealed urn.>
  container
  opened
 end define

 define object <key>
  look <A small key.>
  take
  parent <urn>
 end define

 ' A scripted take is exempt from the implied removal, so this one leaves the
 ' urn with its parent property untouched -- and the urn's veto never applies.
 define object <ring>
  look <A brass ring.>
  take {
   msg <You slip the ring off the urn's neck.>
   give <ring>
  }
  parent <urn>
 end define
end define
