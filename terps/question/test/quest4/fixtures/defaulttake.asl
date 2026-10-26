' QDK writes the take-success message as "error <defaulttake; ...>", with two
' t's, but Quest's error-name switch only knows "defaultake"
' (SetUpUserDefinedPlayerErrors, V4Game.Part2.cs:1485).  In Quest the
' two-t line is an unknown name, so it overwrites badcommand: in The Mansion
' every command the parser cannot place answers "You pick it up."  Question
' deliberately honours the QDK spelling as the take-success message instead
' (FINDINGS.md, finding 30), so the game's own badcommand line survives.
'
' The lines are in the order QDK writes them, badcommand first.

define game <Defaulttake>
 asl-version <311>
 start <Hall>
 error <badcommand; That doesn't work.>
 error <defaulttake; You pocket #quest.error.article#.>
end define

define room <Hall>
 look <The hall.>
 define object <rock>
  prefix <a>
  take
 end define
end define
