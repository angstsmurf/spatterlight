! The other side of the ASL 3.10 gate that lookafterdesc.asl pins.  Same game,
! one version up, and the described room loses its look text: showLookText is
! cleared by `descTagExist And ASLVersion >= 310` (V4Game.Part2.cs:3924-3927),
! because from 3.10 on a description is expected to print the look text itself
! when and where it wants it.
!
! The undescribed room is unaffected -- it still gets the default display and the
! look text after it.
define game <lookafterdesc310>
    asl-version <310>
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
