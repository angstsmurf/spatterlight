! A parentless `remove <X>` re-derives the hidden flags of *every* container in
! the game -- the one exception to the per-container sweep contviscope pins.
!
! ExecAddRemoveScript takes a separate branch when the parameter has no ";" in
! it (V4Game.cs:2695-2699):
!
!     AddToObjectProperties("not parent", childId)
!     UpdateVisibilityInContainers(ctx, _objs[parentId].ObjectName)
!
! parentId is never assigned on that branch, so it is 0, so the name handed over
! is the empty string -- and an empty OnlyParent is the whole-game sweep Quest
! otherwise runs only at the end of loading (V4Game.Part2.cs:7676-7690).
!
! "Pyramid Of Terror" is the game that hangs on it.  Its goth is revealed inside
! a sarcophagus that is never opened, and the oven two rooms away ends its open
! script with `remove <gingerbread man>`.  That one line puts her back out of
! scope for the rest of the game, taking the Jane Eyre book, the coffee-shop and
! one of the letters of MARZIPAN with it.  It is not a bug we get to fix: the
! walkthrough routes around her because Quest does too.
define game <RemoveNoParent>
 asl-version <410>
 start <Room>

 command <reveal goth> show <goth>
 command <bare remove> remove <coin>
 command <parented remove> remove <coin; chest>
end define

define room <Room>
 look <A dusty chamber.>

 define object <sarcophagus>
  look <A stone sarcophagus.>
  container
  open
  close
 end define

 define object <goth>
  look <A bored goth.>
  parent <sarcophagus>
  take
 end define

 define object <chest>
  look <A tin chest.>
  container
  open
  close
 end define

 define object <coin>
  look <A coin.>
  parent <chest>
  take
 end define
end define
