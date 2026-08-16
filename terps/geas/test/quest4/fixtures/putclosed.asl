' The player's PUT refuses a closed container before any of its add machinery
' is consulted: Quest checks `surface || opened` ahead of both the `add`
' action and the `add <text>` property lookups (ExecAddRemove,
' V4Game.cs:2563-2578), so a closed container answers only "You can't put
' that there."  A surface is exempt -- it has no open state at all.
' Confirmed against the real Quest 4.1.5 runner (putprobe.asl, 2026-08-16):
' only an opened sack runs its add script.
'
' The corpus case is Barbarian, whose Sack takes the pile of bones through an
' `add` action but must be closed again before USE SACK ON PEDESTAL -- the
' intended dance is OPEN SACK / PUT BONES IN SACK / CLOSE SACK, and geas used
' to let the put through while the sack was still closed, skipping the whole
' puzzle.

define game <Putclosed>
 asl-version <410>
 start <Lab>
end define

define room <Lab>
 look <The lab.>

 define object <bone>
  take
 end define

 define object <coin>
  take
 end define

 define object <pin>
  take
 end define

 define object <sack>
  take
  container
  open {
   msg <You open the sack.>
   open <sack>
  }
  close {
   msg <You close the sack.>
   close <sack>
  }
  add {
   msg <The sack takes the #quest.add.object.name#.>
   add <bone; sack>
  }
 end define

 define object <box>
  container
  add <It goes in the box.>
 end define

 define object <tray>
  properties <container; surface>
  add <It rests on the tray.>
 end define
end define
