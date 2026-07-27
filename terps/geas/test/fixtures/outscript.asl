! A room whose "out" exit is a script rather than a destination.
!
! Quest's out exit takes either form: `out <Room>` goes there, and `out { ... }`
! runs a script that decides for itself (typically ending in a `goto`).  What it
! must never do is leak the script into the room description.  Quest builds that
! line from the exit *destinations* only -- an `out` whose value is a script
! contributes nothing to it, because there is no place name to print.
!
! geas printed the de-inlined script text verbatim.  readfile.cc hoists a brace
! block out to a generated procedure and rewrites the line as `out do
! <!intprocNNN>`, and the room-description code then took that whole string as
! the destination, so the Sail boat in Shipwrecked greeted the player with
!
!     You can go out to do <!intproc242>.
!
! It now suppresses the line when the destination still contains a '<', which is
! the tell-tale of a script rather than a room name.
!
! The corpus case is Shipwrecked (Wonderjudge, ASL 410), whose sail boat is
! moored at whichever island you last sailed to, so its `out` has to branch:
!
!     out {
!         msg <You clamber out of the boat and go ashore.>
!         if flag <sailboat1> then goto <Rocky beach> else goto <East shore1>
!     }
!
! Note the deliberate divergence this fixture also pins, in the two-room
! comparison below: geas prints the pre-410 two-line "You can go north." / "You
! can go out to the harbour." form rather than ASL 410's single folded
! GetAvailableDirectionsDescription line.  That is a known, separate gap.
define game <outscript>
    asl-version <410>
    start <Boat>
end define

! The scripted form: nothing about "out" may appear in the description, but the
! exit still works, and it is the script that chooses where you land.
define room <Boat>
    look <The deck of a small boat.>
    indescription <You are on a:>
    north <Cabin>
    out {
        msg <You clamber out of the boat and go ashore.>
        if flag <moored east> then goto <East shore> else goto <West shore>
    }
end define

define room <Cabin>
    look <A cramped cabin.>
    south <Boat>
    command <moor east> flag on <moored east>
end define

! The plain form, for contrast: a destination does get announced.
define room <West shore>
    look <A stretch of shore.>
    out <Boat>
    north <Cabin>
end define

define room <East shore>
    look <A different stretch of shore.>
    out <Boat>
end define
