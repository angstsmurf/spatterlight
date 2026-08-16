' The `out' tag is cut in two at its first `>' -- finding 74.
'
' Every other direction is read by GetTextOrScript, which decides from the line
' whether the room field holds a destination or a script.  Before ASL 4.10 `out'
' is not read that way: SetUpRoomData keeps both halves of the line, Out.Text =
' GetParameter (the contents of the first <...>) and Out.Script = everything
' after that same `>' (V4Game.Part2.cs:1018-1030).
'
' So the parameter is always a destination, whatever keyword stands in front of
' it.  GoDirection runs Out.Script when there is one and otherwise hands Out.Text
' to PlayGame as a room name (ibid. 6324-6356), and PlayGame refuses a room that
' does not exist without a word (ibid. 6674-6684).  So the Bedroom's tag below
' does *nothing at all*: its message is a room name, not a message.  Fade to
' White (210) and A Bargain (210) both ship an out like that.
'
' UpdateDoorways prints Out.Text either way (ibid. 7218-7233), so the room still
' advertises the would-be message, semicolon-split into prefix and name as any
' destination would be.  An `out { ... }' block that readfile de-inlines into
' `out do <!intprocNNN>' -- exactly as Quest's own ConvertMultiLineSections does
' (V4Game.cs:588-590) -- advertises the procedure's name.
'
' A script after the parameter still wins, and so does the destination a `create
' exit out <src; dest>' assigns, because that assigns Out.Text alone and leaves
' Out.Script standing (V4Game.cs:5482-5485).
'
' From 4.10 on AddExitFromTag parses `out' with all the rest and a leading
' keyword really does make the tag a script; roominfov2exits covers the other end
' of the range, where the display comes from ShowRoomInfoV2 instead.
define game <outparam>
    asl-version <350>
    start <Bedroom>
    command <unbar> create exit out <Bedroom; Hall>
end define

define procedure <ring>
    msg <The bell rings.>
end define

define room <Bedroom>
    look <A bedroom.>
    out msg <You need to get up first.>
    east <Hall>
end define

define room <Hall>
    look <A hall.>
    out do <ring>
    east <Porch>
    west <Bedroom>
end define

define room <Porch>
    look <A porch.>
    out <Bedroom> msg <You think better of it.>
    east <Gate>
    west <Hall>
end define

define room <Gate>
    look <A gate.>
    out <the; Bedroom>
    east <Field>
    west <Porch>
end define

define room <Field>
    look <A field.>
    west <Gate>
end define
