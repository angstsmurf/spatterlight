! A cyclic !include chain (this file -> lib -> this file) must be broken, not
! followed until the stack overflows.  The game still loads and both files'
! definitions are spliced in exactly once.
define game <IncludeCycle>
 asl-version <311>
 start <Room>

 command <lib> do <fromlib>
end define

!include <include_cycle_lib.asl>

define room <Room>
 look <A room.>
end define
