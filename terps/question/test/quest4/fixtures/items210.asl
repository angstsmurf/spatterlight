! The other side of the gate in items280.asl.  Below ASL 2.80 "give"/"lose" set
! the Got flag on a named *item* from the game block's "items <...>" list
! (V4Game.Part2.cs:6660 onwards) and "if got <x>" reads that flag, with no
! object of that name needing to exist at all (V4Game.cs:7384-7392).  So the
! lamp here is carried, and listed, purely as an item.
define game <Items210>
 asl-version <210>
 start <Room>
 items <lamp>

 command <arm> give <lamp>
 command <disarm> lose <lamp>
 command <check> if got <lamp> then msg <got lamp: yes> else msg <got lamp: no>
end define

define room <Room>
 look <A plain room.>
end define
