! The three "open"/"close" declaration forms are not all the same thing to
! Quest: a bare "open" line, and one whose whole argument is a plain <message>,
! become object *properties*, while a scripted "open msg <...>" becomes an
! action (V4Game.Part2.cs:3470-3503).  ExecOpenClose needs that property both to
! allow the open at all and for the text to print, falling back to its
! "defaultopen" message when the property carries no value (V4Game.cs:2814-2848).
!
! geas used to file all three forms as actions, so a bare "open" opened the
! container in silence and an "open <message>" line was recognised as neither
! property nor action and dropped -- which mattered once compiled .cas games
! started decompiling their "container"/"parent" lines at all, because a closed
! container hides its contents and the items inside became unreachable.
define game <OpenClose>
 asl-version <400>
 start <Room>
end define

define room <Room>
 look <A plain room.>

 ! Bare open/close: openable, no message of its own.  The bare "remove" is what
 ! lets the coin be taken out again -- a take out of a container runs the
 ! container's remove handling first (see takefromcontainer).
 define object <crate>
  container
  open
  close
  remove
 end define

 ! open <message>: a property whose value is the text to print.
 define object <safe>
  container
  open <The safe swings open.>
 end define

 ! open followed by script: an action, which runs instead of any message.
 define object <chest>
  container
  open msg <You lever the chest open.>
 end define

 define object <coin>
  parent <crate>
  take
 end define
end define
