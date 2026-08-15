' Quest prints the default "You are in <room>." line only when a room has no
' indescription, and the test is IsNullOrEmpty (V4Game.Part2.cs:3746-3765).
' geas used to test whether the property is present, so a room carrying an
' empty one printed no room line at all (geas-runner.cc:2063-2078).
'
' Both ways of writing an empty value count.  `indescription <>` is empty as
' read; `indescription < >` survives GetParameter as a single space, but from
' ASL 3.50 the loader also copies the value into the object-property table,
' where AddToObjectProperties trims what follows the "=" (V4Game.cs:4025) and
' writes the trimmed value back over InDescription (V4Game.cs:4111-4118).
' TARDIS Escape's Console Room is the second kind.
'
' The third room is the case that does work: a value ending in a colon takes
' the room name and a full stop.

define game <Indescspace>
 asl-version <410>
 start <Console Room>
end define

define room <Console Room>
 alias <Console Room>
 prefix <the>
 indescription < >
 east <Empty Room>
end define

define room <Empty Room>
 alias <Empty Room>
 prefix <the>
 indescription <>
 west <Console Room>
 east <Colon Room>
end define

define room <Colon Room>
 alias <Colon Room>
 prefix <the>
 indescription <You are standing in:>
 west <Empty Room>
end define
