! Quest keeps two separate answers to "where is this object", and they are
! allowed to disagree.
!
!   ContainerRoom  the room (or "inventory"), written by MoveThing and read by
!                  `here`, `got`, $locationof$, the room listing and
!                  `for each object in <room>`.
!   parent         a *property*, the container, written by DoAddRemove and read
!                  by ListContents, UpdateVisibilityInContainers and
!                  PlayerCanAccessObject.
!
! `add` writes the property and copies the container's room across
! (V4Game.cs:2117-2119).  MoveThing then carries the contents along whenever the
! container moves (V4Game.cs:6643-6656).  But `lose` moves the *object* and
! leaves the property alone (PlayerItem, V4Game.Part2.cs:6660), so a label taken
! out of a carried bottle by `lose` is standing in the room and is still listed
! as the bottle's contents -- which is how the Wizard's Tavern comes to have a
! Label in it.  `give` is the one that does clear the property first
! (V4Game.Part2.cs:6646-6652), so what is carried does not follow its old
! container back out of the inventory.
!
! geas conflated the two in one field, which lost the label entirely.
define game <ContainerRoom>
 asl-version <410>
 start <Tavern>

 command <where> msg <bottle in $locationof(bottle)$, label in $locationof(label)$>
 command <stash> {
  show <label>
  add <label; bottle>
 }
 command <takebottle> give <bottle>
 command <takelabel> give <label>
 command <dropbottle> lose <bottle>
 command <droplabel> lose <label>
end define

define room <Tavern>
 look <A small country tavern.>

 define object <bottle>
  look <A bottle of sludge.>
  container
  transparent
  take
 end define

 define object <label>
  look <A curling label.>
  hidden
  take
 end define
end define

define room <Cellar>
 look <A cold cellar.>
end define
