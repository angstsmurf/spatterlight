' `|w' and the clearing `|c' end the line as well as pausing -- finding 64.
'
' Quest does not hand a string to the formatter in one piece.  Print walks it a
' character at a time and treats those two codes as flush points: what has
' accumulated so far goes to DoPrint, the accumulator is emptied, and only then
' does the game wait (or clear).  Since every DoPrint ends its line, the text on
' either side of the code is never on the same line
' (V4Game.Part2.cs:6745-6784).  geas used to wait in place and print straight
' on, so `Alpha|w Beta' came out as one line.
'
' The closing DoPrint is the only guarded one -- `If printString <> ""' (ibid.
' 6794-6797) -- so a string that *ends* with either code has had its last line
' ended already and gets no extra blank one after it.  The flush at the code is
' not guarded, which is why a string that *begins* with one begins with an empty
' line.
'
' The five colour codes are not flush points; only a `|c' that is none of |cb,
' |cr, |cl, |cy or |cg clears, and that one prints first and wipes after.
define game <waitflush>
    asl-version <410>
    start <Room>
    command <one> msg <Alpha|w Beta>
    command <two> msg <Gamma|w>
    command <three> msg <|wDelta>
    command <four> msg <Epsilon|cZeta>
    command <five> msg <|cgGreen|cb still one line>
    command <six> msg <Eta|w|wTheta>
end define

define room <Room>
    indescription <You are in the room.>
end define
