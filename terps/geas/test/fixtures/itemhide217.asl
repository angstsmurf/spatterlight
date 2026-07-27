' "give <x>" followed by "hideobject <x>", the Quest 2.x way of handing over an
' item that is also standing in the room as scenery.
'
' Below ASL 2.80 items and objects are two separate tables.  PlayerItem sets the
' Got flag on the *item* of that name and never looks at an object at all
' (V4Game.Part2.cs:6658-6669), and hideobject clears the *object's* Exists flag
' and never looks at an item (SetAvailability, ibid. 2994 and 3056 onwards).  So
' the pair above leaves the item carried and the scenery gone, which is exactly
' what the author wants: one name, two things.
'
' From 2.80 on there is no such split -- give moves the object into "inventory"
' and runs its gain script (ibid. 6604-6647), and "if got" is
' (ContainerRoom = "inventory") AND Exists (V4Game.cs:7370-7378).  The same two
' lines therefore hand you the object and then delete it.  itemhide280.asl is this
' same game at 2.80, and its golden says "got lamp: no" where this one says yes.
'
' geas used to move the object into the inventory on give at every ASL version,
' which turned the 2.x idiom into the 2.80 behaviour: the corpus case is The Dream
' Weaver (Bad Omen, ASL 217), where
'
'     define procedure <~Internal Procedure 2>
'       msg <You take the hooka and put it in your coat pocket>
'       give <Hooka>
'       hideobject <Hooka>
'
' left the player with nothing, and the hooka is the key to every remaining
' puzzle in the game.
'
' "ditch" pins the mirror-image rule for lose: below 2.80 it clears the item flag
' and leaves the object where it is (still hidden), so the lamp does not reappear.
'
' The helper is called "~Internal grab" for a reason: below ASL 3.20 a parameter
' beginning with "~Internal" is the one thing GetParameter hands back unconverted
' (V4Game.cs:1884-1893), because otherwise the leading ~ would open a collectable
' reference, find no closing ~, and turn the whole name into "<ERROR>".  QDK names
' every procedure it generates for a .cas file that way; a hand-written "~grab"
' really would be unreachable here.

define game <Itemhide217>
 asl-version <217>
 start <Cellar>
 items <lamp; coin>
 startitems <coin>

 command <check> if got <lamp> then msg <got lamp: yes> else msg <got lamp: no>
 command <ditch> lose <lamp>
end define

define room <Cellar>
 look <The cellar.>

 define object <lamp>
  look msg <A brass lamp.>
  take do <~Internal grab>
 end define
end define

define procedure <~Internal grab>
 msg <You pocket the lamp.>
 give <lamp>
 hideobject <lamp>
end define
