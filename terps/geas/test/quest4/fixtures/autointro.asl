! The default half of nointro.asl: a startscript that does not say "nointro"
! leaves _autoIntro set, so Quest displays <intro> once the startscript has
! finished -- after the startscript's own output, not before it
! (V4Game.Part2.cs:7946 and 7987).
define game <autointro>
    asl-version <410>
    start <r>
    startscript msg <Startscript ran first.>
end define

define text <intro>
Here is the intro, exactly once.
end define

define room <r>
    look <A room.>
end define
