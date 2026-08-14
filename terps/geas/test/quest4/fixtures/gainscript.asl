! gainscript -- an object's `gain` script must run when the object is TAKEn.
!
! Quest's ExecTake, for a take tag that is either bare (TextActionType.Default)
! or a plain message (TextActionType.Text), prints the message and then calls
! PlayerItem(item, true) (V4Game.Part2.cs:5205-5215).  PlayerItem moves the
! object to "inventory" and, for ASL >= 280, runs its GainScript (ibid.
! 6631-6634).  Only a *scripted* take (TextActionType.Script) skips all of that,
! because the script is then responsible for the move itself.
!
! geas ran the gain script through the two-argument run_script overload, whose
! signature is (script, &return_value) -- so it executed the object's *name* as
! a script line and dropped the gain script into the return value.  The gain
! script never ran for a bare or message take.  A scripted take that hands the
! object over with `give <self>` still worked, because the give statement
! handler has its own, correct, PlayerItem equivalent; that is why the bug could
! sit unnoticed.  Enterprising in space takes the Regulations Manual with
! `take give <Regulations Manual>` (worked) and the sandwich, the chocolate and
! the rope with a bare `take` (all three silently dead, and all three gate the
! rest of the game).
!
! The four objects below cover the four cases:
!   pebble  bare take                -> gain runs
!   shell   message take             -> gain runs
!   twig    scripted take            -> gain must NOT run
!   reed    scripted take + give     -> gain runs, through the give handler

define game <GainScript>
 asl-version <410>
 start <Beach>
end define

define room <Beach>
 look <A flat grey beach.>

 define object <pebble>
  look <A grey pebble.>
  take
  gain {
   msg <The pebble is warm.>
   show <mark>
  }
 end define

 define object <shell>
  look <A cracked shell.>
  take <You prise the shell out of the sand.>
  gain msg <The shell smells of salt.>
 end define

 define object <twig>
  look <A bleached twig.>
  take msg <You leave the twig where it lies.>
  gain msg <The twig is brittle.>
 end define

 define object <reed>
  look <A dry reed.>
  take {
   msg <You snap off the reed.>
   give <reed>
  }
  gain msg <The reed rattles.>
 end define

 define object <mark>
  look <A mark in the sand.>
  hidden
 end define
end define
