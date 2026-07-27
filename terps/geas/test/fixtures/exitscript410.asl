! A directional exit that carries a *script* rather than a destination, and the
! one-statement-per-line rule that governs what that script can do.
!
! Parsing (RoomExits.AddExitFromTag, RoomExits.cs:141-186): after the direction
! and an optional "locked", if what follows starts with "<" then the first
! <...> is taken as the exit's parameter and *everything after the closing ">"*
! becomes the exit's script.  For a directional exit that parameter is the lock
! message, never a destination -- so `west locked <a> <more text>` is a locked
! script exit, not an exit to a room called "a".
!
! Running (RoomExit.Go, RoomExit.cs:240-259): the lock is tested first and
! refuses the exit whatever it carries; only an unlocked script exit runs its
! script.
!
! Execution (V4Game.ExecuteScript, V4Game.Part2.cs:5686-5722): a script string
! is split on CRLF *only*, and each resulting line is dispatched by matching a
! single leading keyword with BeginsWith.  GetParameter then reads just the
! first <...>.  So one line is one statement: anything after the first
! statement's parameter is silently dropped, and a line whose leading keyword
! is not recognised logs "Unrecognized keyword" and does nothing at all.
!
! WEST below is the exact shape "Michael's Game" gives its final door -- three
! `locked` clauses and the winning `msg` all on one line.  The first <...>
! becomes the lock message and the rest becomes a script starting with the
! keyword "locked", which is not a script statement, so the win text is
! unreachable and the game cannot be finished.  geas must print nothing there.
define game <ExitScript410>
 asl-version <410>
 start <Start>
 command <openup> {
  unlock <Start; west>
  unlock <Start; north>
  unlock <Start; east>
  unlock <Start; south>
  msg <Every exit is unlocked now.>
 }
end define

define room <Start>
 look <The starting room.>
 up goto <Town>
 west locked <The guard will not let you past.> locked <second> locked <third> msg <You have beaten the game>
 north locked <Not yet.> msg <The north script ran.>
 east locked <Not yet either.> msg <first statement> msg <second statement>
 south locked <Nor this one.> msg <About to leave.> goto <Town>
end define

define room <Town>
 look <A small town.>
 down <Start>
end define
