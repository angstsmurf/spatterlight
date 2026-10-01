! Types that include each other in a cycle (and one that includes itself).
! Only flatten_type had a depth bound; the property/action lookups, the key
! listing and type_of_type followed the "type <...>" lines until the stack
! overflowed, so examining the rock crashed.
define game <TypeCycle>
 asl-version <410>
 start <Lab>
 command <check> if type <rock; c> then msg <is c> else msg <not c>
 command <poke> doaction <rock; poke>
end define
define type <a>
 type <b>
 weight=2
end define
define type <b>
 type <a>
 action <poke> msg <Poked.>
end define
define type <self>
 type <self>
end define
define room <Lab>
 look <A lab.>
 define object <rock>
  type <a>
  type <self>
 end define
end define
