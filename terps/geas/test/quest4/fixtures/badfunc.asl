' An unknown function name is a visible error, not a hole in the text.
'
' DoFunction looks for a `define function' block, falls back to the built-in
' list, and when neither has the name logs "No such function" and returns the
' literal "[ERROR]" (V4Game.cs:6763-6771).  geas returned the empty string, so
' the sentence closed up over the gap and a typo in a game read as if the
' author had meant it.
'
' The `d' command below is the plain case.  `e' is the one worth reading twice:
' "cost is $5 and $6" is a well-formed conversion as far as either engine is
' concerned, so both call a function named "5 and " and both miss -- what is
' being tested is only what a miss prints.  `f' is the escape that is *not* a
' call at all, "$$" for a literal dollar (Pure Chaos prices its safe that way).
'
' The other commands hold the four built-ins geas was missing, since each of
' them would now print [ERROR] instead: `objectname' and `thisobject' are the
' calling object's real name and `thisobjectname' its alias, `timerinterval'
' is the interval with VB's leading space on it (the sibling `ubound' trims
' its number, this one does not), and `removeformatting' strips Quest's own
' bar-codes.  `findexit' is a 4.10 exit-object function geas has no model for;
' it answers with Quest's own not-found value rather than the marker.
define game <badfunc>
    asl-version <410>
    start <Lab>

    define variable <n>
        type numeric
        value <0>
    end define

    command <d> msg <one $nosuch(x)$ two>
    command <e> msg <cost is $5 and $6>
    command <f> msg <price is $$1000000>
    command <g> msg <interval is [$timerinterval(clock)$] and [$timerinterval(nosuchtimer)$]>
    command <h> msg <plain: $removeformatting(|bbold|xb and |cr|s12red|cb)$>
    command <i> msg <exit is [$findexit(Lab; north)$]>
    command <j> {
        doaction <beaker; rub>
        doaction <flask; rub>
    }
end define

define room <Lab>
    look <A white room.>

    ' In an action, not a `look' tag: a tag's parameter is resolved once at
    ' load time, when there is no calling object to name, and Quest walks into
    ' a null reference there rather than printing anything.
    define object <beaker>
        alias <glass beaker>
        action <rub> msg <It is $objectname$, alias $thisobjectname$, this $thisobject$.>
    end define

    define object <flask>
        action <rub> msg <It is $objectname$, alias [$thisobjectname$].>
    end define
end define

define timer <clock>
    interval <7>
    action <set numeric <n; 1>>
end define
