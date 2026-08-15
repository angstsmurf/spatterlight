! Quest's container *verb* -- PUT X IN Y / PUT X ON Y -- and the "add" hook the
! target object gets to intercept it with.
!
! DoAddRemove, at V4Game.cs:2578-2636, looks the target up twice, in this order.
! Braces are elided below because readfile.cc de-inlines brace blocks before it
! strips comments, so a lone brace in a comment would be read as a block start.
!
!     if ObjectActionExists(parentId, "add") then
!         SetStringContents("quest.add.object.name", childName, ctx)
!         ExecuteScript(_objs[parentId].Actions[...].Script, ctx, parentId)
!     else
!         var add = GetObjectProperty("add", parentId, false, false)
!         if add is not empty then
!             PlayerErrorMessage / Print(add)
!             AddToObjectProperties("parent=" + parentName, childId, ctx)
!             ...
!         else
!             PlayerError = PlayerError.CantPut
!
! So an "add" *action* is a script that is handed the object's name in
! quest.add.object.name and is responsible for the containment itself -- with the
! `add <child; parent>` statement, or by refusing.  An "add" *property* is just a
! message, after which Quest performs the containment for you.  Both were
! unimplemented: geas went straight to its own permissive "You put X in Y."
!
! Two things had to change.  In readfile.cc, "add" was in neither the object
! property list nor the object action list, so `add { ... }` was emitted as a raw
! line nothing ever read and `add <text>` was not a property -- every scripted
! container in every game was silently inert.  It now sits in both lists, exactly
! as "remove" does, and for the same reason.  In geas-runner.cc the PUT chain
! consults the action, then the property, before falling through.
!
! geas keeps its permissive fallback -- a plain container or surface with no "add"
! at all still accepts anything -- instead of Quest's CantPut, because the corpus
! walkthroughs lean on it.
!
! The corpus case is Shipwrecked (Wonderjudge, ASL 410), which needs both halves.
! Its small basket takes only the *lit* lantern, and its `add` script is what
! lowers the lantern into the tomb and raises the flag the archway checks -- the
! game is unwinnable without it.  Its jar of honey takes only the box of matches,
! which is the one way to keep them dry through the swim to the third island.
define game <containeradd>
    asl-version <410>
    start <Hold>
    ! Proof that the action really was handed quest.add.object.name.
    command <report> msg <cargo = [#cargo#]>
end define

define room <Hold>
    look <The hold of a boat.>

    ! An "add" action: a script, told what it was handed, that does the moving
    ! itself.  This one is picky, like Shipwrecked's basket.
    define object <basket>
        look msg <A small basket.>
        container
        opened
        add {
            if ($lcase(#quest.add.object.name#)$ = lantern) then {
                msg <The lantern settles into the basket.>
                add <lantern; basket>
                set string <cargo; #quest.add.object.name#>
            }
            else msg <The basket is only big enough for the lantern.>
        }
    end define

    ! An "add" property: a message, after which the engine does the containment.
    define object <sack>
        look msg <A canvas sack.>
        container
        opened
        add <You stuff it into the sack.>
    end define

    ! No "add" of either kind -- geas's own default containment answers.
    define object <crate>
        look msg <A wooden crate.>
        container
        opened
    end define

    define object <lantern>
        look msg <A lantern.>
        take
    end define

    define object <rope>
        look msg <A coil of rope.>
        take
    end define

    define object <pebble>
        look msg <A pebble.>
        take
    end define
end define
