! ExecVerb reads a game-scope verb's name list with GetParameter, so #string#,
! %numeric% and $function$ references inside the name are substituted before any
! matching is attempted (V4Game.cs:2946).
!
! Two consequences, both pinned below.  A name built from a *defined* variable
! matches under its substituted spelling: "verb <#polish#>" with polish = "buff"
! answers BUFF, not "#polish#".  And a name built from an *undefined* one
! collapses to nothing: "verb <drop #@object#>" -- what QDK writes when an author
! types the command form into the verb box -- becomes "drop " and can never match,
! because ExecVerb tests BeginsWith (cmd, name + " ") and that leaves two spaces.
! The verb is dead and DROP falls through to Quest's own handling, which runs the
! object's drop script.
!
! geas used to take the name list unevaluated, so "drop #@object#" matched as a
! two-noun pattern and swallowed every DROP in the game.  "Pyramid Of Terror"
! carries exactly that verb, and its only route into the second half of the map
! is the gingerbread man's drop script.
define game <verbeval>
    asl-version <410>
    start <r>
    define variable <polish>
        type string
        value <buff>
    end define
    verb <drop #@object#> msg <the dead verb fired>
    verb <#polish#> msg <nothing to buff>
end define

define room <r>
    define object <lamp>
        take
        action <drop> msg <lamp put down, drop script ran>
        action <buff> msg <lamp gleams>
    end define

    define object <rock>
        take
        action <drop> msg <rock put down, drop script ran>
    end define
end define
