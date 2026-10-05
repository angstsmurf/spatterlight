! q3ext.qlb is bundled, as Quest bundles it: with no copy next to the game the
! include still brings the library in (V4Game.cs:1397-1415).  Three things it
! leans on, each of which Question had differently:
!
!  - its startscript moves the player into its own limbo room to force a
!    refresh, and Quest puts the player in the start room once the startscript
!    is over (V4Game.Part2.cs:8026-8040);
!  - its clothing type says properties <alias=$thisobject$>, which at load
!    time is the null context's object and so nothing at all: the alias is
!    empty, the room lists the name, and $displayname$ answers the empty alias
!    (V4Game.cs:7177-7184, 6871);
!  - its take command matches the noun against $displayname$ of everything in
!    the room, and a hidden object answers "!" (V4Game.cs:6862-6871), so the
!    hidden `pillow 2' does not steal "take pillow".
!include <q3ext.qlb>
define game <q3extlib>
 asl-version <350>
 start <Bedroom>
 startscript do <q3ext.qlb.setup>
 command <names> msg <[$displayname(Shoes)$] [$displayname(pillow)$] [$displayname(pillow 2)$]>
end define

define room <Bedroom>
 look <A small bedroom.>
 define object <Shoes>
  look <Black shoes.>
  type <shoes>
 end define
 define object <pillow>
  alias <decorative pillow>
  alt <pillow>
  look <A pillow.>
  type <object>
 end define
 define object <pillow 2>
  alias <decorative pillow>
  alt <pillow>
  hidden
  type <object>
 end define
end define
