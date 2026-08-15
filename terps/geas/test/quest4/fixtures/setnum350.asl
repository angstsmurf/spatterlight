! Below ASL 3.91 a `set numeric` right-hand side is not an expression at all.
! ExecSetVar only reaches ExpressionHandler from 3.91 (V4Game.cs:7252-7350); at
! earlier versions it looks for one operator -- the first "+" anywhere, else the
! first "*", else the first "/", else a "-" at the second character or later --
! and performs that single operation, reading each side with VB's Val(), which
! takes the number off the front of the text and stops.  There is no precedence
! and no second operation: "10*3/4" multiplies ten by Val("3/4"), which is three.
!
! The search runs over a copy in which the character after every "E" has been
! blanked (ObscureNumericExps, V4Game.cs:3434-3455) so that the sign of an
! exponent cannot be taken for an operator, but the operands are cut out of the
! original -- hence "1E+18+0" splitting at the second plus.
!
! A division by zero is refused rather than answered: Quest logs it and leaves
! the variable at zero.
!
! Numbers are displayed with VB's Str(), which prints the shortest text that
! reads back as the same double and drops the leading zero of a fraction.
define game <Setnum350>
 asl-version <350>
 start <Room>

 define variable <x>
  type numeric
 end define

 command <run> {
  set numeric <x; 10*3/4>
  msg <10*3/4 [%x%]>
  set numeric <x; 2+3*4>
  msg <2+3*4 [%x%]>
  set numeric <x; 100-40-30>
  msg <100-40-30 [%x%]>
  set numeric <x; -8-3>
  msg <-8-3 [%x%]>
  set numeric <x; 1/3>
  msg <1/3 [%x%]>
  set numeric <x; 2.5*4>
  msg <2.5*4 [%x%]>
  set numeric <x; 7/0>
  msg <7/0 [%x%]>
  set numeric <x; 3 apples+4>
  msg <3 apples+4 [%x%]>
  set numeric <x; 1E+18+0>
  msg <1E+18+0 [%x%]>
  set numeric <x; 41>
  msg <41 [%x%]>
 }
end define

define room <Room>
 look <A plain room.>
end define
