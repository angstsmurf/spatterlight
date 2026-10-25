' Probe game for run_autosave_tests.py (Quest 4 half): the colour mode across
' a relaunch.  The dark `define game' palette turns it on from the first
' window; `paint' changes the background in play, which only the autosave
' remembers (the relaunch boots the game afresh).  COLOURS OFF and ON are the
' player's, and must survive a relaunch too.  The new background is bare hex:
' QuestViva hands any name but its six to the browser as `#name', which it
' ignores, so `navy' would change nothing.
'
' Not a fixture, like probe4.asl.
define game <Autosave Colours Probe>
 asl-version <410>
 start <Hall>
 background <black>
 foreground <white>
 startscript msg <White on black, with |cyyellow|cb words.>
 command <paint> {
  background <000080>
  msg <The walls turn navy.>
 }
end define

define room <Hall>
 look <A dark hall, with |cyyellow|cb words in it.>
end define
