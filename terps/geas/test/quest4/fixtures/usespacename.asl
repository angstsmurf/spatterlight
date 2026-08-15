' A trailing space in the name a `use on` or `give to` tag points at.
'
' Quest trims neither side.  An object's ObjectName is a raw GetParameter result
' (V4Game.Part2.cs:3229) and so is UseData().UseObject (V4Game.cs:4257-4283; and
' GetParameter itself does no trimming, V4Game.cs:1858-1890), so a declaration
' and a tag that spell the name with the same stray space agree, and the scripted
' use runs.
'
' geas trims the name in a `define` header (readfile.cc), because so much else
' about a name arrives trimmed -- an exit destination, for one -- but the
' rewriter that turns a `use on` line into an `action` did not.  The object was
' registered under the trimmed name while its partner's handler was filed under
' the padded one, get_obj_action missed, and the `defaultuse` error ran instead
' of the script.  Londe Perplex has "define object <Warrior >" in West War and a
' matching "use on <Warrior >" on the Rage; the whole scripted attack was lost.
'
' `give to`, and the reversed `use` and `give` forms just below them in the same
' rewriter, take the same trim and are pinned here too.  The unpadded pair is the
' control: it has to keep working.
define game <usespacename>
    asl-version <410>
    start <Field>
end define

define room <Field>
    look <A field.>

    define object <Warrior >
    end define

    define object <Wizard>
    end define

    define object <Sage>
        use <coin > msg <The sage weighs the coin.>
        give <apple > msg <The sage bites the apple.>
    end define

    define object <rage>
        take
        use on <Warrior > msg <The rage burns the warrior.>
        use on <Wizard> msg <The rage burns the wizard.>
    end define

    define object <coin >
        take
        give to <Warrior > msg <The warrior pockets the coin.>
    end define

    define object <apple >
        take
    end define
end define
