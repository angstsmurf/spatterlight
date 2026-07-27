! A container's listing lines: `list`, `list empty` and `list closed`.
!
! ProcessListInfo (V4Game.cs:3457-3528, reached from the container branch of the
! object loader, V4Game.Part2.cs:3542-3546) files each of the three under its own
! name, and the `<text>` form and the scripted form go to different places:
!
!     list empty <text>       property   "list empty=text"
!     list empty script       action     "<list empty> script"
!     list empty off          nothing -- that is the default already
!     list off                property   "not list"
!
! and identically for `list closed` and for plain `list`.
!
! geas had "list" in neither its property nor its action word list, so every one
! of these lines fell through the loader's last resort and became a *type*
! property named after the whole line.  That lost the text, and -- much worse --
! it meant `property <X; list empty>` was always false, because Quest's
! `property` condition only asks whether the property exists (ExecuteIfProperty,
! V4Game.cs:5991).  Games use exactly that to ask "is this container empty?",
! since a container that has something in it lists its contents instead of its
! empty text.
!
! The corpus case is Shipwrecked (Wonderjudge, ASL 410), where the last piece of
! the raft you escape the first island on is
!
!     use <crate2> {
!         if property <crate2; list empty> then ... hide <crate2> ... show <raft5>
!         else msg <You will need to empty the crate first.>
!     }
!
! so the game could not be finished at all: the crate was empty and the check
! still said it was not.  Worth being clear about what that check really tests --
! the property exists from the moment the object is loaded and is never removed,
! so it stays true once the crate has something in it too, and the author's `else
! msg <You will need to empty the crate first.>` is unreachable.  The last two
! commands in the script pin that: it is faithful, not a leftover of the bug.
!
! `list off`'s "not list" property is loader-side only for now: Quest uses it to
! mean "never list this container's contents" (ObjectContents returns an empty
! string for a non-empty container without a positive "list", V4Game.cs:3349-3421)
! and geas's LOOK IN does not consult it yet.  The loader emits it correctly,
! which is what the corpus needs.
!
! Note the second half of the script.  LOOK IN on an empty container now answers
! with its `list empty` action or property before falling back to geas's own "It
! is empty.", which is what ObjectContents does (V4Game.cs:3425-3431).
define game <listempty>
    asl-version <410>
    start <Beach>
    command <check crate> if property <crate; list empty> then msg <crate reads empty> else msg <crate reads full>
    command <check chest> if property <chest; list empty> then msg <chest reads empty> else msg <chest reads full>
end define

define room <Beach>
    look <A sandy beach.>

    ! The <text> forms: both become properties of those names.
    define object <crate>
        look msg <A wooden crate.>
        container
        opened
        list empty <There is nothing in the crate.>
    end define

    define object <chest>
        look msg <A sea chest.>
        container
        closed
        list empty <The chest is empty.>
        list closed <The chest is shut.>
    end define

    ! The scripted form: an action, not a property -- so `property <barrel; list
    ! empty>` is false here even though the barrel is empty, exactly as in Quest.
    define object <barrel>
        look msg <A barrel.>
        container
        opened
        list empty msg <You peer into the barrel and see your own reflection.>
    end define

    define object <pebble>
        look msg <A pebble.>
        take
    end define
end define
