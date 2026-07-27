! A definition-level "look <...>" is *not* stored as written: Quest runs it
! through GetParameter when it reads the object (V4Game.Part2.cs:3332-3346), so
! any #variable# or #object:property# in it is resolved once, at load time, in
! file order -- a later SET does not change the description, and a property
! defined further down the same block is not visible to it yet.
!
! geas resolves it at load too (GeasFile::static_eval), but that lookup used to
! reach get_obj_property in the middle of reading the block, which built the
! block's parsed property cache from the two or three lines read so far and
! marked it done.  Every property line below the look -- and the look itself --
! was then silently invisible: the object still appeared in the room, but
! "look at" fell through to "Nothing out of the ordinary." and its properties
! read as unset.  GeasBlock::cached_lines now notices the block grew and rebuilds.
!
! What each object pins down:
!
!  * bottle -- the property is defined above the look line, so it substitutes
!    (30), and the alias/weight lines *below* the look line still take effect.
!  * crate -- the same property defined below the look line is not there yet, so
!    the substitution yields "!", which is what Quest's GetObjectProperty
!    returns for a missing property when it logs the error (V4Game.cs:6248-6251).
!    The property is still readable afterwards, from the room's own commands.
!  * flask -- the scripted form ("look msg <...>") is an action, evaluated on
!    every LOOK AT, so it tracks the property as it changes.  This is the form
!    real games use for a shrinking bottle (YOU ARE A TIGER's whiskey).
define game <LoadTimeProp>
 asl-version <400>
 start <Room>

 command <probe> {
  msg <bottle: weight #bottle:weight#, alias #bottle:alias#>
  msg <crate: count #crate:count#>
 }
 command <pour> property <flask; volume = 10>
end define

define room <Room>
 look <A plain room.>

 define object <bottle>
  properties <volume = 30>
  look <There is #bottle:volume# percent left.>
  properties <weight = 2>
  alias <bottle of gin>
 end define

 define object <crate>
  look <It holds #crate:count# jars.>
  properties <count = 7>
 end define

 define object <flask>
  properties <volume = 30>
  look msg <The flask is #flask:volume# percent full.>
 end define
end define
