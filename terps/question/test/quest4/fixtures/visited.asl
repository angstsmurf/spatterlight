' PlayGame stamps every room it enters with a "visited" property, which games read
' back with `property <room; visited>` (V4Game.Part2.cs:6688-6708).  For ASL 391
' up to (but not including) 410 the stamp goes on *before* the room is described
' and before its own entry script runs, so a room script sees itself as already
' visited the first time it fires -- that is what this fixture pins down.  See
' visited410.asl for the other half of the version split.
'
' The Pilgrims Progress (ASL 400) is the game that needs this: its nine-room
' field maze only produces Evangelist once every other field carries `visited`,
' and the test lives in the script of the field you walk back into.  Without the
' stamp the check never passes and the game cannot be finished.

define game <Visited>
 asl-version <400>
 start <hall>
 verb <light> msg <nothing to light>
end define

define room <hall>
 look <The hall.>
 north <study>
 script {
  ' 391..409 marks the room on the way in, so this is true on the very first turn
  if property <hall; visited> then msg <[hall script: hall visited]> else msg <[hall script: hall NOT visited]>
  if property <study; visited> then msg <[hall script: study visited]> else msg <[hall script: study NOT visited]>
 }
 command <checkhall> {
  if property <hall; visited> then msg <[check: hall visited]> else msg <[check: hall NOT visited]>
  if property <study; visited> then msg <[check: study visited]> else msg <[check: study NOT visited]>
  if property <cellar; visited> then msg <[check: cellar visited]> else msg <[check: cellar NOT visited]>
 }
end define

define room <study>
 look <The study.>
 south <hall>
 script if property <study; visited> then msg <[study script: study visited]> else msg <[study script: study NOT visited]>
end define

' Never entered, so it must never acquire the property.
define room <cellar>
 look <The cellar.>
end define
