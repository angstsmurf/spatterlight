! Quest re-derives the hidden flags of everything parented inside a container
! only when the container itself has an event -- UpdateVisibilityInContainers
! (V4Game.Part2.cs:7650) is called from DoOpenClose, DoLook, DoAddRemove and
! ExecAddRemoveScript, plus once at the end of loading (Part2:3611), and from
! nowhere else.  It is not a per-turn sweep.  The rule it applies is
! "surface || ((open || transparent) && seen)".
!
! So a script's "show <X>" on an object that lives inside a shut container
! sticks, and the object stays referrable: Disambiguate scopes a typed noun on
! the object's room and its hidden flag alone, never on the container chain
! (DisambObjHere, V4Game.cs:4303-4350, with Exists being what "hidden" sets,
! V4Game.cs:4165-4178).  Two games in the corpus reveal a parented object exactly
! that way and never open the container afterwards: Christmas Day's Knife and
! Darkness's Hammer.
!
! geas used to run the sweep from regen_var_objects, i.e. after every turn, so
! those reveals were undone on the next redraw and the object could not be taken.
! Moving it to Quest's four call sites is what this fixture pins down -- along
! with the consequence at the end: LOOK AT the still-closed container re-derives
! the flags and hides the revealed object again, which is Quest's behaviour too.
!
! The hammer and the nail are taken while the case is still shut, and both are
! written with a *scripted* take, exactly as the two corpus games write theirs
! (`take give <Hammer>`).  That is not decoration: a text or default take of a
! parented object runs the container's REMOVE on the way, and a shut container
! refuses it (see takefromcontainer).  The knife, taken with the case open, is
! the bare-take half of that.
define game <ContainerVis>
 asl-version <400>
 start <Room>

 command <reveal hammer> show <hammer>
 command <reveal nail> show <nail>
end define

define room <Room>
 look <A plain room.>

 define object <case>
  look <A wooden case.>
  container
  open
  close
  ! Bare "remove", the line QDK writes on a container whose contents may be
  ! taken out: from 3.91 a take of a parented object goes through the
  ! container's own remove handling, and one with none refuses the take
  ! outright (see takefromcontainer).
  remove
 end define

 define object <hammer>
  look <A hammer.>
  parent <case>
  take give <hammer>
 end define

 define object <knife>
  look <A knife.>
  parent <case>
  take
 end define

 define object <nail>
  look <A bent nail.>
  parent <case>
  take give <nail>
 end define
end define
