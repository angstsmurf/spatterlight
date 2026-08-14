! "give" and "lose" mean two different things depending on the ASL version.
! Below 2.80 they set a flag on a named *item* -- Quest 2's parallel inventory,
! which has nothing to do with objects.  From 2.80 on, PlayerItem ignores the
! item table completely and just moves the *object* of that name into
! "inventory" (got=true) or into the current room (got=false), running its
! gain/lose script (V4Game.Part2.cs:6594-6647).  "if got <x>" follows suit: for
! 2.80 and up it tests nothing but that object's room and Exists flag, and logs
! "No object 'x' defined." if there is no such object (V4Game.cs:7370-7382).
!
! geas keeps a 2.x item list of its own, and used to add to it on every "give"
! regardless of version.  That list outlived the object: a 3.x game that gave
! the player a coin and later took it away with "move <coin; vault>" -- which is
! how World's End confiscates your weapons -- still listed the coin in the
! inventory, still passed "if got <coin>", and still resolved "use coin on slot"
! against the item rather than saying it was not there.  The list is now only
! populated below 2.80, so this game (3.10) is object-only.
define game <Items280>
 asl-version <310>
 start <Room>

 command <arm> give <coin>
 command <confiscate> move <coin; vault>
 command <disarm> lose <coin>
 command <check> if got <coin> then msg <got coin: yes> else msg <got coin: no>
 command <phantom> give <ghost>
end define

define room <Room>
 look <A plain room.>

 define object <slot>
  look <A coin slot.>
  use <coin> msg <The coin drops into the slot.>
 end define
end define

define room <vault>
 look <A strongroom.>

 define object <coin>
  look <A dull brass coin.>
 end define
end define
