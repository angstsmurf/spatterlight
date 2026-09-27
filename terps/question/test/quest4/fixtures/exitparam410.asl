! The ASL 4.10 half of exitparam.asl: from 4.10 on the first field of a two-field
! directional exit is the destination and the second is a lock message that only
! bites when the tag also says "locked" (RoomExits.cs:186-191).  So NORTH here
! reaches Town, and the "you would need a key" text is never printed at all.
define game <ExitParam410>
 asl-version <410>
 start <Start>
end define

define room <Start>
 look <The starting room.>
 north <Town; you would need a key>
 south locked <Vault; The vault is locked.>
end define

define room <Town>
 look <A small town.>
 south <Start>
end define

define room <Vault>
 look <The vault.>
end define
