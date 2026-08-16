' The pre-2.80 room display builds its `out' and `place' lines by hand --
' finding 54.
'
' Below ASL 2.80 ShowRoomInfo hands the whole display to ShowRoomInfoV2
' (V4Game.Part2.cs:3692), which reads the room block's own source lines rather
' than the exit objects 2.80 introduced.  Two of the lines it writes come out
' differently as a result.
'
' `out'.  V2 takes GetParameter of the last line beginning "out" -- the first
' parameter on it, whatever the tag was written for -- and prints it after "You
' can go out to " (ibid. 1939-1942, 2004-2025).  A scripted out therefore
' advertises its script's parameter: the Cell here says "You can go out to The
' door is locked..", which is exactly what Fade to White (210) does with its
' `out msg <You need to get up first.>'.  From 2.80 UpdateDoorways reads a
' destination instead and a scripted out prints no line at all.  The parameter
' is looked up as a room name to find an alias, which is the Corridor's case,
' and printed plain -- there is no prefix split and no bolding before 2.80.
'
' Going that way prints nothing at all, which is finding 74 rather than this
' one: before 4.10 the loader keeps Out.Text and Out.Script as two halves of the
' same line, split at its first `>', so an out with no script after the
' parameter has a destination and only a destination -- and "The door is
' locked." is not the name of a room (V4Game.Part2.cs:1018-1030, 6324-6356).
' outparam.asl covers that end of it.
'
' `place'.  V2 cuts the printed form out of the parameter itself:
'
'     prefix  =  Trim(Left(place, InStr(place, ";") - 1))
'     name    =  Right(place, Len(place) - (InStr(place, ";") + 1))
'
' (ibid. 1988-1999).  The two are not each other's inverse: the name drops one
' character too many, because that +1 assumes a space after the semicolon.  The
' usual spelling has one and survives -- "The; Library" gives "The Library" --
' but "Your Training room;Training Room 1" gives "Your Training room raining
' Room 1", and Magic Sword (217) really is played that way.  A parameter with
' no semicolon in it is printed whole and unTrimmed (ibid. 2000-2003).
'
' The names the player may type are untouched by any of this: PlaceExist splits
' the parameter properly (ibid. 6568-6594), so `go to training room 1' still
' works, and this fixture goes that way to prove it.  The trailing "or" keeps
' its comma before 2.80 (ibid. 2073-2085), which geas already had.
'
' `look'.  V2 reads it off the source lines too, taking GetParameter of the
' first line beginning "look" outside a nested define (ibid. 2157-2183).  So
' the Corridor's `look msg <...>' -- an action anywhere else, with no text to
' it at all -- reads here as the message it would have printed.  And V2 sets no
' quest.lookdesc: only the 2.80 display writes that variable (ibid. 3897), so
' the `lookdesc' command below prints an empty one in both rooms.
define game <roominfov2exits>
    asl-version <210>
    start <Cell>
    command <lookdesc> msg <lookdesc=[#quest.lookdesc#]>
end define

define room <Cell>
    alias <the cell>
    indescription <You are in the cell.>
    look <A bare cell.>
    out msg <The door is locked.>
    place <Your Training room;Training Room 1>
    place <The; Library>
    place <Yard>
    north <Corridor>
end define

define room <Corridor>
    indescription <You are in the corridor.>
    look msg <A dim corridor.>
    out <Cell>
end define

define room <Training Room 1>
    indescription <You are in the training room.>
end define

define room <Library>
    alias <The Library>
    indescription <You are in the library.>
end define

define room <Yard>
    indescription <You are in the yard.>
end define
