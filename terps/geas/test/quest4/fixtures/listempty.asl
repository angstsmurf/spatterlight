! A container's listing lines: `list`, `list empty` and `list closed`.
!
! ProcessListInfo, at V4Game.cs:3457-3528 and reached from the container branch
! of the object loader at V4Game.Part2.cs:3542-3546, files each of the three
! under its own name, and the `<text>` form and the scripted form differ:
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
! `property` condition only asks whether the property exists -- ExecuteIfProperty,
! V4Game.cs:5991.  Games use exactly that to ask "is this container empty?",
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
! `list off` is the opt-out, and it works by omission: from ASL 3.91 the loader
! gives every object a "list" property, `list off` records "not list" instead,
! and ListContents returns an empty string for a non-empty container that has no
! positive "list" -- V4Game.cs:3349-3421.
!
! The chest is declared `closed` and never given an `open` line, so OPEN CHEST is
! refused: ExecOpenClose wants an `open` action or an `open` property and says
! "You can't open that." when the object has neither -- V4Game.cs:2812-2827.  It
! stays shut, and so keeps answering with its `list closed` text.
!
! A container's listing is the tail of its description rather than an answer to
! any verb of its own, so every line below prints the object's `look` text first
! and its listing after.  An empty container answers with its `list empty` action
! or property, and one that has neither says nothing -- V4Game.cs:3425-3431.
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

    ! The scripted form: an action, not a property -- so asking after the barrel's
    ! "list empty" property is false here even though the barrel is empty, and
    ! ListContents has to look for the action too, exactly as Quest does.
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
