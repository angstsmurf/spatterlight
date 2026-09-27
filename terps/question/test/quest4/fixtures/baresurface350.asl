! The pre-3.91 half of baresurface: a bare "surface" line records nothing at all.
!
! Quest's loader matches the line and then does nothing with it below ASL 3.91 --
! the branch's whole body is inside "if (ASLVersion >= 391)"
! (V4Game.Part2.cs:3472-3479) -- so the object is neither a container nor a
! surface, and the thing parented to it stays out of scope the way anything
! parented to a plain object does.  "properties <surface>" is unaffected: it
! records the property it names at any version, which is what the ledge is here
! to show.
define game <BareSurface350>
 asl-version <350>
 start <Room>
end define

define room <Room>
 look <A room with two things to stand things on.>

 ! The bare line: nothing recorded, so no contents listing.
 define object <mantle>
  look <A stone mantle.>
  surface
 end define

 define object <vase>
  look <A tall vase.>
  parent <mantle>
 end define

 ! A properties line: the property is recorded, but ListContents wants
 ! "container" as well, so this lists nothing either.
 define object <ledge>
  look <A narrow ledge.>
  properties <surface>
 end define

 define object <bowl>
  look <A clay bowl.>
  parent <ledge>
 end define
end define
