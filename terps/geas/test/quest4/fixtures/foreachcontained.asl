! "for each object in <room>" walks ContainerRoom, so it sees what is inside the
! containers standing in that room.
!
! Quest's loop compares each object's ContainerRoom to the name it was given
! (ExecForEach, V4Game.cs:5029-5040) and knows nothing about the "parent"
! property, and
! DoAddRemove files a child under its container's room -- so a bead dropped into
! a basket is, as far as the loop is concerned, in the church.  The Wizard's
! DETECT MAGIC is the game that notices:
!
!     for each object in <#quest.currentroom#> {
!       if property <#quest.thing#; magical> and exists <#quest.thing#> then ...
!     }
!
! and it glows the Yellow Bead sitting in the church's basket.  geas filed the
! bead under the basket instead of the room, so the loop skipped it.
!
! The loop is not a scope test: it reaches into a *shut* container, as the third
! object below shows.  `exists` is the only filter, and that is the inverse of
! "hidden" -- which the container sweep does derive from the container's state,
! so an unseen container's contents are skipped after all, but by a different
! route and only once the sweep has run.
define game <ForEachContained>
 asl-version <410>
 start <Church>

 command <detect> {
  for each object in <#quest.currentroom#> {
   if property <#quest.thing#; magical> then msg <The #@quest.thing# glows briefly.>
  }
 }
 command <detect existing> {
  for each object in <#quest.currentroom#> {
   if property <#quest.thing#; magical> and exists <#quest.thing#> then msg <The #@quest.thing# glows briefly.>
  }
 }
end define

define room <Church>
 look <A quiet church.>

 define object <basket>
  look <A wicker basket.>
  container
  surface
 end define

 define object <yellow bead>
  look <A yellow bead.>
  parent <basket>
  properties <magical>
  take
 end define

 define object <chest>
  look <A shut chest.>
  container
  open
  close
 end define

 ! In a container that is neither open nor seen: the loop still walks it, but
 ! the sweep has already hidden it, so only the "exists" form skips it.
 define object <ruby>
  look <A red ruby.>
  parent <chest>
  properties <magical>
  take
 end define

 define object <candle>
  look <A plain candle.>
 end define
end define
