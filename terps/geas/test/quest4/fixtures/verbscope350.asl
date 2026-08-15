' verbscope.asl one version gate down: the scopes are the same at every
' version, the refusals are not.
'
' Below 4.10 ExecTake has no second lookup to fall back on, so a noun that is
' already in hand is simply not here -- BadThing, the same answer an invented
' noun gets (V4Game.Part2.cs:5143-5167).  And below 3.91 a DROP miss is BadDrop
' rather than NoItem (V4Game.cs:5544-5556).  GIVE's NoItem is not gated and
' reads the same at both versions.
define game <verbscope350>
    asl-version <350>
    start <Kitchen>

    startscript {
        give <red apple>
        give <spare key>
        give <cook doll>
    }
end define

define room <Kitchen>
    look <A tidy kitchen.>

    define object <red apple>
        alt <apple>
        prefix <a>
    end define

    define object <green apple>
        alt <apple>
        prefix <a>
        take <You take the green apple off the table.>
    end define

    define object <spare key>
        alt <key>
        prefix <a>
    end define

    define object <brass key>
        alt <key>
        prefix <a>
        take <You take the brass key off the table.>
    end define

    define object <cook>
        prefix <the>
        gender <he>
        article <him>
        give <spare key> msg <"That's mine," says the cook, pocketing it.>
    end define

    define object <cook doll>
        alt <cook>
        prefix <a>
    end define
end define
