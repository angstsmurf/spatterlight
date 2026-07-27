' "for each exit in <room>", "for each room in game", and the ASL 4.10
' #quest.doorways# variable.
'
' An exit in Quest 4.10 is an object like any other: RoomExit's constructor
' appends a record to _objs with IsExit set (RoomExit.cs:16-27), and
' UpdateObjectName (RoomExit.cs:172-232) names it "<room>.<direction>" for a
' compass exit or "<room>.exitN" for a directionless "place", N counted per room
' from the room object's own quest.lastexitid.  The objects are made as the
' room's exit tags are read, so "for each exit in" walks them in source order.
'
' Three consequences are pinned here.
'
'  1. A destroyed exit is still walked.  DestroyExit hands the exit to
'     RoomExits.RemoveExit, which only clears the object's Exists flag
'     (RoomExits.cs:575-591), while ExecForEach filters on IsRoom/IsExit alone
'     and never looks at Exists (V4Game.cs:5029-5040).  So "cellar" below still
'     counts two exits after both have been destroyed -- and, because the count
'     comes out non-zero, falls into the branch that prints the (now empty)
'     #quest.doorways#, ending up with a bare "You can go .".  Tim Hamilton's
'     "The Things That Go Bump In The Night" hits precisely that trap and works
'     around it: its game-block description counts the exits with a "for each
'     exit" loop, and the one room it wants to say "You can go nowhere." about
'     gets the count back to zero with a "dec" *inside* the same loop -- which
'     only cancels out if the exits it destroyed in the startscript are still
'     being counted.
'
'  2. Re-creating a compass exit does not add a second object: SetDirection
'     reuses the RoomExit of a direction the room has already had
'     (RoomExits.cs:20-33).  "study" ends with two exits, not three, after its
'     north exit is destroyed and made again.
'
'  3. "for each object in" never sees a room or an exit, and "for each room in"
'     sees only rooms -- all three forms share one _objs walk and pick their
'     records by the IsRoom/IsExit flags (V4Game.cs:4969-5042).  geas used to
'     implement only the object form, and that one walked the rooms too.
'
' #quest.doorways# is the 4.10 folded list of everywhere you can go: compass
' exits first, in the order the tags created them, then the places, joined with
' ", " and a final " or " (RoomExits.GetAvailableDirectionsDescription,
' RoomExits.cs:303-353).  Note that the "hall" tags are deliberately out of
' compass order -- east before north -- because Quest lists them in tag order,
' not in the fixed north/south/east/west order of the pre-4.10 line.
'
' geas prints its own "You can go ..." line after a custom description as well,
' which real Quest does not (see display_room in geas-runner.cc), so each room
' here is followed by a second, engine-generated copy of the exits.

define game <ForEachExit>
 asl-version <410>
 start <hall>
 startscript {
  destroy exit <cellar; north>
  destroy exit <cellar; study>
 }
 description {
  msg <-- #quest.currentroom# -->
  set numeric <n; 0>
  for each exit in <#quest.currentroom#> {
   inc <n>
   msg <exit: #quest.thing#>
  }
  msg <exit count: %n%>
  if is <%n%;gt;0> then msg <You can go #quest.doorways#.> else msg <You can go nowhere.>
 }
 command <rooms> {
  set numeric <r; 0>
  for each room in game inc <r>
  set numeric <o; 0>
  for each object in game inc <o>
  msg <rooms: %r%  objects (game object included): %o%>
 }
 command <allexits> {
  set numeric <n; 0>
  for each exit in game inc <n>
  msg <exit objects in the whole game: %n%>
 }
 command <remake> {
  destroy exit <study; north>
  create exit north <study; hall>
  msg <north remade>
 }
end define

' Tags deliberately in a non-compass order, and one "place" among them, to show
' that the exit objects -- and #quest.doorways# -- follow the source order.
define room <hall>
 look <The hall.>
 east <study>
 place <cellar>
 north <attic>
end define

define room <study>
 look <The study.>
 north <hall>
 out <hall>
end define

' Both of this room's exits are destroyed by the startscript, so it leads
' nowhere -- but the two exit objects are still there to be counted.
define room <cellar>
 look <The cellar.>
 north <hall>
 place <study>
end define

define room <attic>
 look <The attic.>
 south <hall>
end define

define object <lamp>
 look <A lamp.>
 startin <hall>
end define

define object <coin>
 look <A coin.>
 startin <study>
end define
