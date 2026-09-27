' Probe game for run_autosave_tests.py (Quest 4 half).  Small on purpose: two
' rooms, two portable objects, and one command per piece of state the
' Spatterlight autosave has to carry across a relaunch --
'
'   roll    the RNG position ($rand$ under determinism)
'   count   a numeric variable
'   pulse   a real-time timer (three ticks per firing; the Glk frontend ticks
'           it once per timer event, and only with the sa_delays setting on)
'   question / menu
'           a yes/no question and a selection menu, the two host prompts a
'           game can be closed on (neither autosaves: the relaunch resumes at
'           the turn prompt before them).  Not `q`/`m`: `q` is the
'           frontend's quit metaverb.
'
' The lamp in the pane is the hyperlink case: clicking its name unfolds its
' verb menu (a re-save, no turn), clicking "Take" runs the command.  The
' engine's own UNDO covers the undo history.
'
' Not a fixture: quest4/harness/run_fixtures.sh globs quest4/fixtures/*.asl
' and would want a .cmd and .expected beside this file.
define game <Autosave Probe>
 asl-version <410>
 start <Hall>
 command <roll> msg <You roll $rand(1;1000)$.>
 command <count> {
  set numeric <count; %count% + 1>
  msg <Count is %count%.>
 }
 command <question> {
  if ask <Ready?> then msg <[yes]> else msg <[no]>
 }
 command <menu> choose <pick>
end define

define variable <count>
 type numeric
 value <0>
end define

define timer <pulse>
 interval <3>
 action msg <[pulse]>
 enabled
end define

define room <Hall>
 look <A hall. The study is north.>
 north <Study>
 define object <lamp>
  look <A brass lamp.>
  take
 end define
 define object <book>
  look <A dusty book.>
  take
 end define
end define

define room <Study>
 look <A study. The hall is south.>
 south <Hall>
 define object <desk>
  look <A desk with nothing on it.>
 end define
end define

define selection <pick>
 info <Which one?>
 choice <One> msg <[one]>
 choice <Two> msg <[two]>
end define
