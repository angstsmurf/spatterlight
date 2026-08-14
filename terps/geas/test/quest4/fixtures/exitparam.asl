! A two-field directional exit parameter means opposite things either side of ASL
! 4.10, and Quest keeps both readings.  Up to 3.53, GoDirection enters everything
! *after* the semicolon, so "north <the; Town>" is prefix "the" plus destination
! "Town" (V4Game.Part2.cs:6319-6326).  From 4.10 the tag goes through
! AddExitFromTag instead, which reads the first field as the destination and the
! second as a lock message -- inert unless the tag also says "locked"
! (RoomExits.cs:186-191).  See exitparam410.asl for that half.
!
! geas used to apply the pre-4.10 reading to every game, so a 4.10 game's
! "west <TARDIS Door; This door is deadbolted...>" walked the player into a room
! named after the message.
define game <ExitParam>
 asl-version <350>
 start <Start>
end define

define room <Start>
 look <The starting room.>
 north <the; Town>
 south locked <Vault; The vault is locked.>
end define

define room <Town>
 look <A small town.>
 south <Start>
end define

define room <Vault>
 look <The vault.>
end define
