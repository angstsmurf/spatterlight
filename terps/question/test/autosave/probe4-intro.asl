' Probe game for run_autosave_tests.py (Quest 4 half): the questions a game
' asks before its first turn.  The startscript rolls a number, asks for a
' name (`enter`) and then a gender (a selection menu); both questions
' autosave, as a record of the answers given so far, and the relaunch boots
' again with those answers up to the question it was closed on.  `who` shows
' that the answers and the number arrived.
'
' Not a fixture, like probe4.asl.
define game <Autosave Intro Probe>
 asl-version <410>
 start <Hall>
 startscript {
  set numeric <lucky; $rand(1;1000)$>
  msg <What is your name?>
  enter <name>
  msg <Hello, #name#.>
  choose <gender>
 }
 command <who> msg <You are #name#, #gender#, lucky number %lucky%.>
end define

define variable <lucky>
 type numeric
 value <0>
end define

define variable <name>
 type string
 value <nobody>
end define

define variable <gender>
 type string
 value <unknown>
end define

define room <Hall>
 look <A hall.>
end define

define selection <gender>
 info <Are you male or female?>
 choice <Male> set string <gender; male>
 choice <Female> set string <gender; female>
end define
