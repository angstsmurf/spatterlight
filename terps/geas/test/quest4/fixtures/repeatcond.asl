! `repeat while` / `repeat until`, and where the condition stops and the body
! starts.
!
! ExecuteRepeat (V4Game.cs:5994-6048, braces elided because readfile.cc
! de-inlines brace blocks before it strips comments) splits the line by scanning
! for the end of the condition, not by looking for a keyword: it finds the first
! ">", and if what follows begins with "and " or "or " it keeps looking, else
! everything after it is the loop body.  There is no "do" in the syntax at all --
!
!     repeat until flag <done> script
!     repeat while is <%n%;lt;3> and is <%m%;gt;0> script
!
! -- and the body is an ordinary script statement, so it can be a braced block, a
! bare statement, a `do <proc>`, or a `choose <menu>`.
!
! geas required a literal `do` between condition and body, so every form that did
! not happen to use one parsed as a condition with an empty body, and the whole
! loop was then silently discarded with no error: a game that put a puzzle inside
! one simply did nothing when you tried to solve it.
!
! The corpus case is Shipwrecked (Wonderjudge, ASL 410), whose stone building
! holds seven colored holes that must be pressed in the order the cave drawings
! give.  USE STONE ROD ON COLORED HOLES is
!
!     repeat until flag <stoneb1> choose <coloredholes>
!
! so before the fix the rod did nothing whatever and the temple never opened.
define game <repeatcond>
    asl-version <410>
    start <Chamber>

    ! A braced body, no "do" anywhere.
    command <countup> {
        set numeric <n; 0>
        repeat until is <%n%;3> {
            inc <n>
            msg <n is now %n%>
        }
        msg <done at %n%>
    }

    ! `repeat while`, and a single unbraced statement as the body.
    command <countdown> {
        set numeric <n; 3>
        repeat while is <%n%;gt;0> dec <n>
        msg <countdown ended at %n%>
    }

    ! A multi-clause condition: the "or" continuation belongs to the condition,
    ! and the body is whatever follows the *last* clause's ">".
    command <both> {
        set numeric <n; 0>
        set numeric <m; 10>
        repeat until is <%n%;3> or is <%m%;8> {
            inc <n>
            dec <m>
            msg <n %n% m %m%>
        }
        msg <stopped at n %n% m %m%>
    }

    ! A flag condition with a procedure body -- the shape Shipwrecked uses.
    command <untilflag> {
        set numeric <n; 0>
        repeat until flag <ready> do <bump>
        msg <untilflag ended at %n%>
    }
end define

define procedure <bump>
    inc <n>
    msg <bumped to %n%>
    if is <%n%;2> then flag on <ready>
end define

define room <Chamber>
    look <A round stone chamber.>
end define
