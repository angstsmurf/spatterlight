! What a description tag does and does not replace.
!
! ShowRoomInfo builds "You are in ...", the object list and the exit lines into
! one roomDisplayText and prints it -- with the exits line after it -- only when
! the room has no description tag; when it has one, the tag runs *instead*
! (V4Game.Part2.cs:3929-3951).  So a room with a description tag gets no object
! list and no "You can go ..." line unless the description prints them itself.
! Tim Hamilton's "Barbarian" and "The Maze" both end their description with
! `msg <You can go #quest.doorways#.>`; geas used to add its own copy on top and
! showed the line twice.
!
! The room's "look" text is the exception, and it is version-gated.  showLookText
! starts true and is only cleared by `descTagExist And ASLVersion >= 310`
! (V4Game.Part2.cs:3887, 3924-3927), so below 3.10 the look text is printed after
! the description tag as well -- which is what this fixture pins, with
! lookafterdesc310.asl on the other side of the gate.
!
! "study" has a description tag, so only the tag and the look text are printed.
! "hall" has none, so it gets the default display: "You are in the hall", the
! object list, the exits and then the look text.
define game <lookafterdesc>
    asl-version <300>
    start <study>
end define

define room <study>
    look <Books to the ceiling.>
    description <A cramped study.>
    east <hall>

    define object <quill>
        look <A quill pen.>
    end define
end define

define room <hall>
    look <A portrait of the founder.>
    west <study>

    define object <umbrella>
        look <A black umbrella.>
    end define
end define
