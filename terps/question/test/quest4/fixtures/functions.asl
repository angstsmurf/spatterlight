! Built-in function edge cases.
!
!   $mid$      is 1-based in Quest, and has a 2-argument (to-end-of-string) form
!   $rand$     takes signed bounds, and must survive a reversed/equal pair
!   $f()$      an explicit empty argument list is not an argument
!   $ubound$   highest defined index of an array
!
! The $rand$ checks assert a *range*, not a value: the value depends on the C
! library's rand(), so a golden transcript can't name it.
define game <Functions>
 asl-version <311>
 start <Room>

 define variable <r>
  type numeric
  value <0>
 end define

 command <mid> {
  msg <mid3: $mid(abcdef;2;3)$>
  msg <mid2: $mid(abcdef;4)$>
  msg <mid1: $mid(abcdef;1;1)$>
 }

 command <zeroarg> msg <zeroarg: $noargs()$>

 command <randsame> msg <randsame: $rand(-5;-5)$>

 command <randneg> {
  set numeric <r; $rand(-9;-1)$>
  if (%r% >= -9) and (%r% <= -1) then msg <randneg: in range> else msg <randneg: BAD (%r%)>
 }

 command <randrev> {
  set numeric <r; $rand(9;1)$>
  if (%r% >= 1) and (%r% <= 9) then msg <randrev: in range> else msg <randrev: BAD (%r%)>
 }

 command <randzero> {
  set numeric <r; $rand(0;0)$>
  if (%r% = 0) then msg <randzero: 0> else msg <randzero: BAD (%r%)>
 }

 command <ubound> {
  set numeric <arr[3]; 7>
  msg <ubound: $ubound(arr)$>
 }
end define

define room <Room>
 look <A room.>
end define

define function <noargs>
 return <called-ok>
end define
