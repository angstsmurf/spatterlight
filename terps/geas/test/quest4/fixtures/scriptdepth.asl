! Runaway script recursion is reported and abandoned, not allowed to smash the
! interpreter's stack.
!
! Quest fires a variable's `onchange` script on *every* assignment, not only on
! ones that change the value (SetNumericVariableContents, V4Game.Part2.cs:526),
! so a pair of clamps that disagree about what the value should be will assign
! to each other forever.  That is exactly what Kingdom.asl (QDK 3.03) does: each
! village stat has
!
!     onchange {
!       if ( %stat% > %max% ) then set numeric <stat; %max%>
!       if ( %stat% < 1 )     then set numeric <stat; 1>
!     }
!
! and when the game's own arithmetic leaves `max` at 0, "too big, shrink to 0"
! and "too small, grow to 1" hand the value back and forth. geas used to follow
! that all the way down and die of SIGSEGV before printing a single line; Quest
! only bounds `exec` (ExecExec, V4Game.Part2.cs:2197) and would blow its own
! stack here too, so there is no authentic behaviour to match -- the point is
! merely that a buggy game file must not take the process with it.
!
! `oscillate` below is the Kingdom shape reduced to one variable.  `deep` is the
! benign neighbour case: recursion that terminates on its own well inside the cap
! must still run to completion and print its result.
define game <ScriptDepth>
 asl-version <410>
 start <Room>

 define variable <cap>
  type numeric
  value <0>
 end define

 ! Two clamps that disagree: >0 pulls it to 0, <1 pushes it to 1.
 define variable <oscillate>
  type numeric
  value <0>
  onchange {
   if ( %oscillate% > %cap% ) then set numeric <oscillate; %cap%>
   if ( %oscillate% < 1 ) then set numeric <oscillate; 1>
  }
 end define

 define variable <countdown>
  type numeric
  value <0>
 end define

 command <oscillate> {
  msg <before>
  set numeric <oscillate; 5>
  msg <after: oscillate is %oscillate%>
 }
 command <descend> {
  set numeric <countdown; 40>
  do <descend>
  msg <descended, countdown is %countdown%>
 }
end define

! Recurses `countdown` times, then stops.  40 deep is far below the cap.
define procedure <descend>
 if ( %countdown% > 0 ) then {
  set numeric <countdown; %countdown% - 1>
  do <descend>
 }
end define

define room <Room>
 look <A room.>

 define object <clamp>
  look <Assigning to it starts the oscillation.>
  take msg <It is a variable, not a thing.>
 end define
end define
