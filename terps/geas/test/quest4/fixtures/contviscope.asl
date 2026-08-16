! Quest's container sweep is per-container, not global.  Every runtime call to
! UpdateVisibilityInContainers passes OnlyParent -- DoOpenClose (V4Game.cs:2255),
! DoLook (2147) and both DoAddRemove paths (2133, 2698) all name the one
! container whose state just changed -- and the argument-less form runs exactly
! once, at the end of loading (V4Game.Part2.cs:3611).  So an event on one
! container can never re-derive the hidden flags of anything held by another --
! with one exception, a `remove <X>` written without a parent, which sweeps
! everything because it hands the sweep an unassigned object number
! (removenoparent).
!
! That is what keeps a scripted "show <X>" alive.  As containervis pins down, a
! reveal into a never-opened container sticks until *that* container has an event
! of its own; looking into, opening or filling some other container leaves it
! alone.  "Pyramid Of Terror" depends on it: the goth is parented in a
! sarcophagus that is never opened, revealed by a bare "show <goth>", and the
! player then wanders a map full of containers before coming back to talk to her.
!
! geas swept every container on every one of those events, so the goth was
! dragged back out of scope by the first unrelated LOOK AT and SPEAK TO GOTH
! answered "You don't see any goth."  The last two turns below show the one sweep
! that must still fire: an event on the goth's own container re-derives her flag
! and, the sarcophagus being shut, hides her again -- Quest's behaviour too.
define game <ContVisScope>
 asl-version <410>
 start <Room>

 command <reveal goth> show <goth>
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
  ! Bare "remove", so the coin can be taken back out at all -- a take out of a
  ! container runs the container's remove handling first (takefromcontainer).
  remove
 end define

 define object <coin>
  look <A coin.>
  parent <chest>
  take
 end define
end define
