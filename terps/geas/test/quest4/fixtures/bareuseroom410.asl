! "use <item>" with no target, when both the room and the item define a handler.
!
! Quest looks at the room's own list of bare-use handlers first, so a room can
! give a carried item a special meaning while the player is standing there -- but
! only below ASL 410.  ExecUse, V4Game.Part2.cs:5348-5368, reads (braces elided,
! because readfile.cc de-inlines brace blocks before it strips comments, so a
! lone brace in a comment would be read as the start of a block):
!
!     if (string.IsNullOrEmpty(useOn))
!         if (ASLVersion < 410)
!             var r = _rooms[roomId];
!             for (int i = 1, loopTo1 = r.NumberUse; i <= loopTo1; i++)
!                 if (LCase(_objs[id].ObjectName) == LCase(r.Use[i].Text))
!                     foundUseScript = true; useScript = r.Use[i].Script; break;
!         if (!foundUseScript)
!             useScript = _objs[id].Use;
!             ...
!
! From 410 on the room's list is simply never consulted for a bare use, so the
! object's own "use" action always wins.  geas checked the room first regardless
! of version.
!
! "Mario Is Missing 2: Ultimate Quest" (ASL 350) is unfinishable without the
! room-first half: its vacuum cleaner has a generic "use" that only comments on
! the noise, and the two rooms that matter -- the dusty side room that hides the
! Bob-omb doll, and the third-floor landing that hides the door to Lady Bow --
! each override it with a room-level "use <Vaccuum Cleaner>".
!
! This fixture is the ASL 410 half of bareuseroom.asl -- the same source with the
! version bumped, so every bare use here must reach the object's own handler.
define game <bareuseroom410>
    asl-version <410>
    start <r1>
end define

define room <r1>
    look <Room one.>
    west <r2>
    ! The room's override for a bare "use torch".
    use <torch> msg <The room handler ran.>

    define object <torch>
        look msg <A torch.>
        take
        ! The item's own handler, which wins outright at 410.
        use msg <The object handler ran.>
    end define

    define object <lamp>
        look msg <A lamp.>
        take
        use msg <The lamp's own handler ran.>
    end define
end define

! A second room with no bare-use list at all, to pin that the object's handler
! still runs there.
define room <r2>
    look <Room two.>
    east <r1>
end define
