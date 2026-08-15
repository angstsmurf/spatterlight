! The companion to setnum350: the same right-hand sides at an ASL version that
! does reach ExpressionHandler, which is every version from 3.91 up.  Here the
! arithmetic is the arithmetic anyone would expect -- as many operations as
! there are operators, multiplication and division before addition and
! subtraction -- so "10*3/4" is seven and a half rather than thirty.
!
! An inline {...} expression is stringified by .NET's ToString() rather than
! VB's Str(), so it keeps the leading zero of a fraction that "%var%" drops.
!
! Two shapes are deliberately missing here, because geas and Quest still part
! company over them and the fixture would only freeze geas's answer: an
! expression whose first multiplication or division is followed by an addition
! or subtraction ("2+3*4"), which ExpressionHandler crashes on, and an operand
! written in exponent notation ("1E+18+0"), which geas reads only as far as the
! "E".  See findings 68 and 69.
define game <Setnum410>
 asl-version <410>
 start <Room>

 define variable <x>
  type numeric
 end define

 command <run> {
  set numeric <x; 10*3/4>
  msg <10*3/4 [%x%]>
  set numeric <x; 2*3+4>
  msg <2*3+4 [%x%]>
  set numeric <x; 100-40-30>
  msg <100-40-30 [%x%]>
  set numeric <x; -8-3>
  msg <-8-3 [%x%]>
  set numeric <x; 1/3>
  msg <1/3 [%x%]>
  set numeric <x; 2.5*4>
  msg <2.5*4 [%x%]>
  set numeric <x; 41>
  msg <41 [%x%]>
  msg <inline [{1/3}]>
  msg <inline [{10*3/4}]>
 }
end define

define room <Room>
 look <A plain room.>
end define
