! A brace block whose body reads
!
!     if got <shades> then
!     msg <...>
!
! is chopped into two independent statements by readfile's de-inlining pass,
! exactly as Quest's ConvertMultiLineSections chops it (V4Game.cs:531-676): the
! block becomes an internal procedure whose lines are then executed one at a
! time.  Quest's ExecuteIf finds nothing after the "then", runs an empty
! then-script, and the msg on the following line runs unconditionally
! (V4Game.Part2.cs:5627-5684) -- so the condition has no effect at all.
!
! geas used to start the then-branch one character past the "then" token, which
! only worked because a space normally follows it.  With "then" ending the
! statement the substr ran off the end of the string and killed the process with
! an uncaught std::out_of_range.  "Pyramid Of Terror" aborted on USE CANE, whose
! script nests such a dangling "then" inside a braced then-branch.
define game <danglthen>
    asl-version <410>
    start <r>
    verb <wave> msg <You can't wave that.>
end define

define room <r>
    define object <shades>
        take
    end define

    define object <cane>
        take
        action <use> {
            if got <shades> then {
                if got <shades> then
                msg <strut like Ray Charles> } else msg <try it with the shades on>
        }
        action <wave> {
            if got <shades> then
            msg <waved, shades or not>
        }
    end define
end define
