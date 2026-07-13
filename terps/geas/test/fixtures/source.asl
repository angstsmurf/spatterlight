! Source-file lexing: "_" at the end of a line continues it onto the next.  A
! line that is *only* the continuation marker leaves an empty accumulator, which
! must not be indexed at size() - 1.
define game <Source>
 asl-version <311>
 start <Room>

 command <joined> msg <joined: _
   one two three>

 command <bare> msg <bare: ok>
end define

_

define room <Room>
 look <A room.>
end define
