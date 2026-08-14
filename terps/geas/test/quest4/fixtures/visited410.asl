' visited.asl one ASL version up.  PlayGame stamps the room with "visited" on the
' other side of the entry script from 4.10 onwards (V4Game.Part2.cs:6688-6708), so
' a 4.10 room script still sees its own room as unvisited the first time it runs
' and only finds the property on a return visit.  Rooms *other* than the one being
' entered read the same in both versions, which is what most games actually test.
'
' The game, the rooms and the .cmd are identical to visited's apart from the
' asl-version line, so the two .expected files diff to exactly that difference.

define game <Visited410>
 asl-version <410>
 start <hall>
 verb <light> msg <nothing to light>
end define

define room <hall>
 look <The hall.>
 north <study>
 script {
  ' 410 and later marks the room only after this script, so the first firing says NOT
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
