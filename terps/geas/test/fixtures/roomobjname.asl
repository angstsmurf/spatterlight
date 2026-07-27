! A room and an object that share a name -- and the deprecated "gettag", which
! is how a game reads the room half back.
!
! Quest keeps rooms in _rooms and objects in _objs, so the two namespaces never
! collide there.  Games rely on that: the standard way to build a container in
! Quest 3/4 is to declare a *room* whose contents the player is silently walked
! into (outputoff / goto <box> / read #quest.formatobjects# / goto back), and
! that room is named after the object the player can see.  "The Devil's Bargain"
! has five such pairs -- box, desk, wallet, briefcase and panel 579.
!
! geas keeps rooms and objects in one name -> blocktype map and one object
! record vector, and the room used to win both, so for such a pair:
!
!   - "x desk" printed the room's look text ("object") instead of the object's;
!   - the room's listing prefix was pasted onto the object, giving room
!     descriptions like "There is Inside the cedar desk are desk here.";
!   - "here <desk>", "got <desk>" and "locationof(desk)" answered for the room
!     placeholder, which sits nowhere, so every one of them said no.
!
! An unqualified lookup now answers for the object (register_block, and objects
! before rooms in GeasState's name index), which is what Quest does: the room
! half is only ever named explicitly, by goto or by gettag.
!
! gettag itself is room-scoped even when the name is also an object's, because
! it predates the object namespace:
!
!     return await FindStatement(await DefineBlockParam("room", parameters[1]),
!                                parameters[2]);          ' V4Game.cs:6899
!
! FindStatement takes the first line of the block that *begins with* the tag,
! skipping nested define blocks, and returns its <...> parameter with the usual
! substitutions applied (V4Game.Part2.cs:6164-6189).  A name with no room block
! gives "", not an error.
define game <roomobjname>
    asl-version <410>
    start <r1>
    command <probe> do <probe>
end define

define room <r1>
    look <Room one.>

    define object <desk>
        prefix <a cedar>
        look msg <A cedar desk.>
    end define
end define

! The container room.  Its own "look" line comes after a nested define block, so
! it also pins that gettag skips sub-blocks rather than answering with the
! nested object's "look msg <...>".
define room <desk>
    prefix <Inside the cedar desk are>
    ! "suffix" is not one of the tags readfile.cc recognises on a room, so this
    ! line survives verbatim and gettag has to fall back to scanning the block.
    suffix <, gathering dust>

    define object <photos>
        look msg <Incriminating photos.>
    end define

    look <object>
end define

define procedure <probe>
    if here <desk> then msg <here desk: yes> else msg <here desk: no>
    msg <locationof desk = [$locationof(desk)$]>
    msg <locationof photos = [$locationof(photos)$]>
    msg <gettag desk prefix = [$gettag(desk;prefix)$]>
    msg <gettag desk look = [$gettag(desk;look)$]>
    msg <gettag r1 look = [$gettag(r1;look)$]>
    msg <gettag photos look = [$gettag(photos;look)$]>
    msg <gettag desk suffix = [$gettag(desk;suffix)$]>
end define
