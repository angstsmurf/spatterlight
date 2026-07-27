! The objects-pane verb menu (get_object_verbs / the "verbs <object>" command)
! must only ever offer a verb the parser would actually accept.
!
! An object's `action <name>` is NOT a verb by itself.  Quest recognises a verb
! only where the *game block* declares it -- ExecVerb scans for lines beginning
! "verb " or "lib verb " and nowhere else (V4Game.cs:2952-2955) -- so
! `action <lift>` with no `verb <lift>` is dead code the player cannot reach.
! geas used to harvest the whole action list into the menu, which advertised
! such a verb and then rejected the command it typed: "Escape from this house"
! offered Lift on its painting, whose safe is reachable only by TAKE.
!
! No shipped game can guard this, because the pane is not part of a transcript:
! all 111 walkthroughs are byte-identical either way.
!
! The three objects below cover the three cases:
!   painting  -- `action <lift>`, undeclared: Lift must NOT be listed, and
!                LIFT PAINTING must be refused.
!   crate     -- `action <raise>` reached by a declared verb with synonyms and a
!                colon-separated property name, so the menu lists the primary
!                typed name ("Hoist") while the handler it checks for is
!                `raise` (V4Game.cs:2958-2976).  HOIST and HEAVE both work;
!                RAISE, the property name, is not a command at all.
!   sack      -- `remove` is engine-dispatched ("remove <object>"), so Remove is
!                listed without any declaration.
define game <verbmenu>
    asl-version <350>
    start <Yard>
    verb <hoist;heave:raise> msg <Nothing budges.>
end define

define room <Yard>
    define object <painting>
        look <A portrait of a man in victorian dress.>
        take msg <You lift it away and find a safe.>
        action <lift> msg <Never reached: no verb <lift> is declared.>
    end define

    define object <crate>
        look <A heavy wooden crate.>
        action <raise> msg <You heave the crate up onto its end.>
    end define

    define object <sack>
        look <A sack, tangled in the netting.>
        action <remove> msg <You work the sack free of the netting.>
    end define
end define
