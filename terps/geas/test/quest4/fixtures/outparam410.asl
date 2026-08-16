' The 4.10 half of finding 74: `out' is an ordinary exit tag again.
'
' From 4.10 on rooms carry exit objects and every tag, `out' included, goes
' through AddExitFromTag, which reads it the way GetTextOrScript reads the eight
' compass directions -- a leading keyword makes the whole line a script
' (RoomExits.cs:143-191).  So the two tags that do nothing at 350 (see
' outparam.asl) both fire here, and the directions line replaces the pre-4.10
' "You can go out to ..." line, which is why nothing is advertised in its place.
'
' The refusal changes with it.  PlayerError.DefaultOut, "There's nowhere you can
' go out to around here.", belongs to GoDirection, which 4.10 no longer uses for
' this; ExecuteGo fails to find an exit called "out" and answers with BadPlace,
' "You can't go there." (RoomExits.cs:291-296), exactly as a missing `north'
' does.  The Field below has no out at all and shows both wordings when read
' beside outparam.asl.
define game <outparam410>
    asl-version <410>
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
