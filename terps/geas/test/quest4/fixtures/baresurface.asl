! A bare "surface" line declares a container as well.
!
! Quest's object loader answers the line with two AddToObjectProperties calls,
! "container" and then "surface" (V4Game.Part2.cs:3472-3477).  That second,
! silent property is what makes the surface list anything at all: ListContents
! refuses outright anything without "container" (V4Game.cs:3306-3309), so a
! table that was only ever tagged "surface" would swallow whatever was put on
! it.  Barbarian's Mantle did exactly that to the Vase standing on it, and the
! Wizard's bar-top to the bottle of sludge.
!
! Only the bare line does it.  "properties <surface>" adds the one property it
! names, and so does a surface inherited from a type, whose lines Quest folds
! straight into a property string (GetPropertiesInType, V4Game.cs:6335-6342).
! The three objects below are the three spellings, side by side.
!
! And only from ASL 3.91: below that the branch's body is skipped and the bare
! line records neither property.  See baresurface350.
define game <BareSurface>
 asl-version <410>
 start <Room>
end define

define type <shelfish>
 surface
end define

define room <Room>
 look <A room with three things to stand things on.>

 ! The bare line: container + surface.
 define object <mantle>
  look <A stone mantle.>
  surface
 end define

 define object <vase>
  look <A tall vase.>
  parent <mantle>
 end define

 ! A properties line: surface alone, so ListContents says nothing.
 define object <ledge>
  look <A narrow ledge.>
  properties <surface>
 end define

 define object <bowl>
  look <A clay bowl.>
  parent <ledge>
 end define

 ! Inherited from a type: also surface alone.
 define object <plinth>
  look <A marble plinth.>
  type <shelfish>
 end define

 define object <bust>
  look <A chipped bust.>
  parent <plinth>
 end define
end define
