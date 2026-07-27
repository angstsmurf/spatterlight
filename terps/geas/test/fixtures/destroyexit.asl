! Two things Tim Hamilton's "The Maze" needs, neither of which geas used to do.
!
! 1. "destroy exit <room; north>" must take a *directional* exit away, not only a
!    "go to" place.  DestroyExit hands the exit to RoomExits.RemoveExit, which
!    clears the exit object's Exists flag whichever kind it is
!    (RoomExits.cs:575-591).
!
! 2. A direction's exit is one long-lived object whose script is set once and
!    never cleared, so destroying it and re-creating it with a plain destination
!    brings the original script back: SetDirection reuses the existing object
!    (RoomExits.cs:20-33), only calls SetScript when the new tag carries a script
!    of its own, and Go tests for a script before the destination
!    (RoomExit.cs:240-259).
!
! 3. A room with no description tag of its own falls back to a "description" line
!    in the game block (V4Game.Part2.cs:3899-3921).  A parameter is text; anything
!    else is a script.
define game <destroyexit>
    asl-version <410>
    start <Hedge>
    startscript set numeric <xpos; 0>
    description {
        msg <You are at square %xpos%.>
    }
end define

define room <Hedge>
    north {
        inc <xpos; 1>
        msg <north script, now %xpos%>
    }
    south msg <south script>
    command <wall> {
        destroy exit <Hedge; north>
        msg <hedge grows over the north gap>
    }
    command <gap> {
        create exit north <Hedge; Hedge>
        msg <hedge opens again>
    }
end define
