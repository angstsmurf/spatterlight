! The other side of the ASL 4.10 gate that destroyexit.asl pins.  Same game, one
! version lower, and both exit behaviours invert:
!
! * "destroy exit <Hedge; north>" does nothing at all, because before 4.10
!   DestroyExit only ever searches the room's *places* list and a direction name
!   is not a place (V4Game.cs:3608-3654).
!
! * "create exit north <Hedge; Hedge>" wipes the declared script, because before
!   4.10 there are no exit objects: it assigns the room's North field and stamps
!   its type back to Text (V4Game.cs:5429-5450).  "Bear Campsite" (ASL 400) needs
!   this -- it declares `south { msg <a Grizzly Bear blocks your path> ... }` and
!   opens the way out with `create exit south <Campsite; Freedom>`.
!
! The game-block description fallback is not version-gated, so it still applies.
define game <destroyexit400>
    asl-version <400>
    start <Hedge>
    startscript set numeric <xpos; 0>
    description {
        msg <You are at square %xpos%.>
        ! As in destroyexit.asl, the exits are printed here because a description
        ! tag replaces the default room display: without this there is nothing to
        ! read a destroyed exit off.  #quest.doorways# is 4.10 only, so this
        ! pre-4.10 game uses the older per-kind variable.
        msg <You can go #quest.doorways.dirs#.>
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
