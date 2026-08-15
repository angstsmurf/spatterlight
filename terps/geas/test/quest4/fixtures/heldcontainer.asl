! Pick up a bag and you have picked up what is in it.  Quest keeps a contained
! object's ContainerRoom in step with its container -- DoAddRemove copies it on
! `add` (V4Game.cs:2118) and MoveThing has recursed into every child since ASL
! 3.91 (V4Game.cs:6643-6653) -- so the key below is in the inventory scope the
! moment the bag is, and a take of it misses the room, finds the inventory and
! answers alreadytaken (ExecTake, V4Game.Part2.cs:5143-5153).
!
! geas models containment the other way round, with the content's location
! being the container itself, so the key looked unheld and the take ran: implied
! remove, gain script and all, on something the player was already carrying.
! TARDIS Escape is the corpus case -- a pocket parented to a worn trenchcoat --
! and it overrides alreadytaken, as this does, so the whole answer was wrong
! rather than merely differently worded.
!
! Taking the bag again is the control: an object directly in the inventory
! reaches the same refusal by the route geas always had.
!
! The `x bag` line still lacks Quest's "It contains key." (finding 12); the rest
! of the transcript is word for word.
!
! Not covered: `i` is left out because Quest lists the key there and geas does
! not, which is a different piece of the same ContainerRoom model; and there is
! no sub-3.91 companion, because below that version the two engines disagree
! about container visibility long before a take is reached.
define game <HeldContainer>
 asl-version <410>
 start <Cellar>
 error <alreadytaken; Look in your inventory, you greedy pig!>
end define

define room <Cellar>
 look <A damp cellar.>

 define object <bag>
  look <A canvas bag.>
  container
  properties <opened>
  take
  remove
 end define

 define object <key>
  look <A brass key.>
  parent <bag>
  take
 end define
end define
