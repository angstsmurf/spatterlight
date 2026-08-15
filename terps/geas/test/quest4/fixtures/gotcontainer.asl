! `got <X>` when X is inside a container you are carrying.
!
! Quest tracks two things per object: the "parent" property, which is the
! container it sits in, and a separate ContainerRoom field, which is the room --
! or "inventory" -- that the whole nest is in.  DoAddRemove copies the
! container's ContainerRoom onto whatever is put inside it, V4Game.cs:2117-2121.
! Braces are elided because readfile.cc de-inlines them before stripping
! comments, so a lone brace in a comment would be read as a block start:
!
!     AddToObjectProperties("parent=" + parentName, childId, ctx)
!     _objs[childId].ContainerRoom = _objs[parentId].ContainerRoom
!
! and `got` tests exactly that field, so an object inside a carried container is
! still "got".  geas only looked at the object's immediate parent, which is the
! container, not "inventory", so `got` went false the moment you stowed
! something -- even though the like-named `here` condition already walked the
! chain with room_of_parent.  Both now do.
!
! The two conditions stay complementary through the whole nest, which is what the
! second half of the script checks: while the jar is carried, the matches inside
! it are `got` and not `here`; drop the jar on the shore and they become `here`
! and not `got`, without ever being touched themselves.
!
! The corpus case is Shipwrecked (Wonderjudge, ASL 410).  Its box of matches only
! survives the swim to the third island sealed inside the jar of honey, and the
! barrel of gunpowder that clears the rubble on the far shore is lit by
!
!     if not got <Box of matches> then msg <You don't have anything to light it with.>
!
! so the game was unwinnable one puzzle from the end.
define game <gotcontainer>
    asl-version <410>
    start <Shore>
    command <check> {
        if got <matches> then msg <got matches> else msg <no matches>
        if here <matches> then msg <here matches> else msg <not here matches>
        if got <jar> then msg <got jar> else msg <no jar>
    }
end define

define room <Shore>
    look <A stretch of shore.>
    north <Water>

    define object <jar>
        look msg <A jar of honey.>
        take
        container
        opened
        ! Bare "remove", so the matches can come back out -- a take out of a
        ! container runs the container's remove handling first
        ! (takefromcontainer).
        remove
    end define

    define object <matches>
        look msg <A box of matches.>
        take
    end define
end define

define room <Water>
    look <Waist-deep water.>
    south <Shore>
end define
