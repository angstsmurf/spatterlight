! A "place" exit is listed, and answers to GO TO, under the destination room's
! alias -- but only if the place has no script of its own.  Both the listing
! (GetGoToExits, V4Game.Part2.cs:7790-7810) and the name-matching (PlaceExist,
! ibid. 6568-6578) gate the alias lookup on `string.IsNullOrEmpty(Places[i].
! Script)`, so a scripted place shows its raw room name and only answers to
! that.  geas matched the raw name already but *listed* the alias, so the room
! description advertised an exit that GO TO then refused -- World's End hides
! your confiscated weapons behind exactly such an exit.
!
! Note the plain place is the mirror image: aliased in the listing and reachable
! only by the alias, "plainroom" itself being no answer.
define game <PlaceAlias>
 asl-version <400>
 start <Clearing>
end define

define room <Clearing>
 look <A clearing with two lifts.>
 place <plainroom>
 place <scriptedroom> {
  msg <You squeeze into the scripted lift.>
  goto <scriptedroom>
 }
end define

define room <plainroom>
 alias <plain lift>
 look <The plain lift.>
 out <Clearing>
end define

define room <scriptedroom>
 alias <scripted lift>
 look <The scripted lift.>
 out <Clearing>
end define
